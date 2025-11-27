// Copyright 2025 Fancapital Inc.  All rights reserved.
#include <gtest/gtest.h>
#include "stock_master.h"

namespace co {
void StockMaster::HandlePosition(int64_t timestamp, string code, int64_t long_volume, int64_t long_can_close) {
    if (init_timestamp_ == 0 || init_timestamp_ == timestamp) {
        init_timestamp_ = timestamp;
        InitPosition(code, long_volume, long_can_close);
    } else {
        ComparePosition(timestamp, code, long_volume, long_can_close);
    }
}

void StockMaster::InitPosition(string code,int64_t long_volume, int64_t long_can_close) {
    InnerPositionPtr pos = std::make_shared<InnerPosition>(code);
    pos->long_volume_ = long_volume;
    pos->long_can_close_ = long_can_close;
    positions_.insert(std::make_pair(code, pos));
    LOG_INFO << "初始化持仓";
}

void StockMaster::ComparePosition(int64_t timestamp, string code, int64_t long_volume, int64_t long_can_close) {
    auto it = positions_.find(code);
    if (it == positions_.end()) {
        EXPECT_TRUE(false) << "找不到持仓, " << code;
    }
//    EXPECT_EQ(it->second->long_volume_, long_volume);
//    EXPECT_EQ(it->second->long_can_close_, long_can_close);
    int64_t stamp = timestamp % 1000000000LL;
    if (stamp < 150000000) {
        if (it->second->long_volume_ != long_volume) {
            LOG_WARN << code << ", long_volume, StockMaster持仓: " << it->second->long_volume_ << ", 查询: " << long_volume;
        }
        if (it->second->long_can_close_ != long_can_close) {
            LOG_WARN << code << ", long_can_close, StockMaster持仓: " << it->second->long_can_close_ << ", 查询: " << long_can_close;
        }
    } else {
        if (it->second->long_volume_ != long_volume) {
            LOG_ERROR << code << ", long_volume, StockMaster持仓: " << it->second->long_volume_ << ", 查询: " << long_volume;
        }
        if (it->second->long_can_close_ != long_can_close) {
            LOG_ERROR << code << ", long_can_close, StockMaster持仓: " << it->second->long_can_close_ << ", 查询: " << long_can_close;
        }
    }
}

void StockMaster::HandleOrderRep(const co::fbs::TradeOrderT& order) {
    if (order.code.compare("204001.SH") == 0 || order.code.compare("131800.SZ") == 0) {
        return;
    }
    std::unique_lock<std::mutex> lock(pos_mutex_);
    if (order.order_no.empty()) {
        return;
    } else {
        if (auto it = inner_order_no_.find(order.order_no); it != inner_order_no_.end()) {
            return;
        }
        inner_order_no_.insert(order.order_no);
        InnerPositionPtr pos;
        auto it = positions_.find(order.code);
        if (it != positions_.end()) {
            pos = it->second;
        } else {
            pos = std::make_shared<InnerPosition>(order.code);
            positions_.insert(std::make_pair(order.code, pos));
        }
        if (order.bs_flag == kBsFlagSell) {
            pos->long_can_close_ -= order.volume;
        }
        // 处理成交先到，响应后到的情况
        if (auto iter = knock_first_orders_.find(order.order_no); iter != knock_first_orders_.end()) {
            for (auto item : (*iter->second)) {
                HandleKnock(item);
            }
        }
        // 申赎根据成交回报处理
        pos->ToString();
    }
}

void StockMaster::HandleKnock(co::fbs::TradeKnockT& knock) {
    if (knock.order_no.empty() || knock.match_no.empty() || knock.code.empty()) {
        return;
    }
    if (knock.match_volume <= 0) {
        return;
    }
    if (auto it = inner_order_no_.find(knock.order_no); it == inner_order_no_.end()) {
        auto iter = knock_first_orders_.find(knock.order_no);
        if (iter == knock_first_orders_.end()) {
            std::unique_ptr<std::vector<co::fbs::TradeKnockT>> knocks = std::make_unique<std::vector<co::fbs::TradeKnockT>>();
            knocks->push_back(knock);
            knock_first_orders_.insert(std::make_pair(knock.order_no, std::move(knocks)));
        } else {
            iter->second->push_back(knock);
        }
        LOG_INFO << "knock first, order_no second, code: " << knock.code
                 << ", order_no: " << knock.order_no
                 << ", match_no: " << knock.match_no
                 << ", match_type: " << knock.match_type
                 << ", bs_flag: " << knock.bs_flag
                 << ", match_volume: " << knock.match_volume;
        return;
    }
    if (auto it = inner_match_no_.find(knock.match_no); it != inner_match_no_.end()) {
        return;
    }
    if (knock.code.compare("204001.SH") == 0 || knock.code.compare("131800.SZ") == 0) {
        return;
    }

    std::unique_lock<std::mutex> lock(pos_mutex_);
    inner_match_no_.insert(knock.match_no);
    InnerPositionPtr pos;
    auto it = positions_.find(knock.code);
    if (it != positions_.end()) {
        pos = it->second;
    } else {
        pos = std::make_shared<InnerPosition>(knock.code);
        positions_.insert(std::make_pair(knock.code, pos));
    }
    if (knock.match_type == co::kMatchTypeOK) {
        if (knock.bs_flag == kBsFlagBuy) {
            pos->long_volume_ += knock.match_volume;
            if (IsT0Code(knock.code)) {
                pos->long_can_close_ += knock.match_volume;
            }
        } else if (knock.bs_flag == kBsFlagSell) {
            pos->long_volume_ -= knock.match_volume;
        } else if (knock.bs_flag == kBsFlagCreate) {
            if (knock.code[0] == '1' || knock.code[0] == '5') {
                pos->long_can_close_ += knock.match_volume;
                pos->long_volume_ += knock.match_volume;
            } else {
                int64_t not_can_close = pos->long_volume_ - pos->long_can_close_;
                pos->long_volume_ -= knock.match_volume;
                if (not_can_close < knock.match_volume) {
                    pos->long_can_close_ -= (knock.match_volume - not_can_close);
                }
            }
        } else if (knock.bs_flag == kBsFlagRedeem) {
            if (knock.code[0] == '1' || knock.code[0] == '5') {
                int64_t not_can_close = pos->long_volume_ - pos->long_can_close_;
                pos->long_volume_ -= knock.match_volume;
                if (not_can_close < knock.match_volume) {
                    pos->long_can_close_ -= (knock.match_volume - not_can_close);
                }
            } else {
                pos->long_can_close_ += knock.match_volume;
                pos->long_volume_ += knock.match_volume;
            }
        }
        if (knock.match_price > 0) {
            pos->price_ = knock.match_price;
        }
    } else {
        if (knock.bs_flag == kBsFlagSell) {
            pos->long_can_close_ += knock.match_volume;
        }
    }
    pos->ToString();
}

// 可转债	T+0	123...， 110...	可以当天无限次买卖
// 跨境/商品ETF	T+0	513...， 518...	主要跟踪境外指数或大宗商品
bool StockMaster::IsT0Code(string code) {
    if (x::StartsWith(code, "123")) {
        return true;
    }
    if (x::StartsWith(code, "110")) {
        return true;
    }
    if (x::StartsWith(code, "513")) {
        return true;
    }
    if (x::StartsWith(code, "518")) {
        return true;
    }
    if (auto item = t0_list_.find(code); item != t0_list_.end()) {
        return true;
    }
    return false;
}

void StockMaster::InitT0Code() {
    t0_list_.insert("019789.SH");
    t0_list_.insert("019776.SH");
    t0_list_.insert("019742.SH");
}
}  // namespace co
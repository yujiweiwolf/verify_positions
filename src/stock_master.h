// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include "x/x.h"
#include "coral/coral.h"

namespace co {
// 内部持仓
struct InnerPosition {
    InnerPosition(string code) : code_(code){
    }
    string code_;
    int64_t long_volume_ = 0;
    int64_t long_can_close_ = 0;
    double price_ = 0;                   // 通过成交回报里的match_price或者10分钟查询持仓更新

    void ToString() {
        LOG_INFO << "StockMaster持仓, "  << code_ << ", long_volume: " << long_volume_
                 << ", long_can_close: " << long_can_close_
//                 << ", long_market_value: " << long_volume_ * price_
//                 << ", price: " << price_
                 ;
    }
};
typedef std::shared_ptr<InnerPosition> InnerPositionPtr;

class StockMaster {
 public:
    StockMaster() = default;
    ~StockMaster() = default;
    void InitT0Code();
    void HandlePosition(int64_t timestamp, string code, int64_t long_volume, int64_t long_can_close);
    void HandleOrderRep(const co::fbs::TradeOrderT& order);
    void HandleKnock(co::fbs::TradeKnockT& knock);
    std::map<std::string, InnerPositionPtr>* GetAllPos() {
        std::unique_lock<std::mutex> lock(pos_mutex_);
        return &positions_;
    }

private:
    void InitPosition(string code, int64_t long_volume, int64_t long_can_close);
    void ComparePosition(int64_t timestamp, string code, int64_t long_volume, int64_t long_can_close);
    bool IsT0Code(string code);

 private:
    std::mutex pos_mutex_;
    std::set<string> inner_order_no_;
    std::set<string> inner_match_no_;
    std::map<std::string, InnerPositionPtr> positions_;
    // 先收到成交回报, 后收到报单响应
    std::unordered_map<std::string, std::unique_ptr<std::vector<co::fbs::TradeKnockT>>> knock_first_orders_;
    std::set<string> t0_list_;
    int64_t init_timestamp_ = 0;
};
}  // namespace co

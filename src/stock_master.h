// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include "x/x.h"
#include "coral/coral.h"

namespace co {
/* 内部持仓
 * 1. 每次重启时, 柜台查到的可用持仓默认是昨仓
 * 2. T0的合约，没有buy_volume, 默认是昨仓, 方便处理
 * 3. 今天申购得到的持仓，不可以用于赎回; 今天赎回得到的持仓，不可以用于申购;
 * */
struct InnerPosition {
    InnerPosition(string code) : code_(code){
    }
    string code_;
    int64_t long_volume_ = 0;
    int64_t pre_volume_ = 0;  // 昨仓
    int64_t buy_volume_ = 0;  // 今仓
    int64_t cr_volume_ = 0;   // 申赎得到的持仓

    int64_t CalculateAvailableVolume() {
        return (pre_volume_ + cr_volume_);
    }

    void ToString() {
        LOG_INFO << "StockMaster持仓, "  << code_ << ", long_volume: " << long_volume_
                 << ", long_can_close: " << CalculateAvailableVolume()
                 << ", pre_volume: " << pre_volume_
                 << ", buy_volume: " << buy_volume_
                 << ", cr_volume: " << cr_volume_;
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
    // 记录卖单的volume是哪部分持仓，撤单时复原持仓. first是pre_volume, second是cr_volume
    std::map<std::string, std::pair<int64_t, int64_t>> order_volume_;
    // 先收到成交回报, 后收到报单响应
    std::unordered_map<std::string, std::unique_ptr<std::vector<co::fbs::TradeKnockT>>> knock_first_orders_;
    std::set<string> t0_list_;
    int64_t init_timestamp_ = 0;
    int64_t index_ = 0;
};
}  // namespace co

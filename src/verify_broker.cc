// Copyright 2025 Fancapital Inc.  All rights reserved.
#include <regex>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include "verify_broker.h"

namespace co {
VerifyBroker::VerifyBroker() {
}

VerifyBroker::~VerifyBroker() {
}

void VerifyBroker::Init(const string& file1, const string& file2) {
    inner_stock_master_.InitT0Code();
    ReadFile(file1);
    ReadFile(file2);
    VerifyPosition();
}

void VerifyBroker::ReadFile(const string& file) {
    if (file.empty()) {
        return;
    }
    if (!boost::filesystem::exists(file)) {
        LOG_ERROR << "not exit file: " << file;
        return;
    }
    co::WALReader reader;
    reader.Open(file.c_str());
    while (true) {
        std::string raw;
        int64_t type = reader.Read(&raw);
        if (raw.empty()) {
            break;
        }
//       LOG_INFO << "type: " << type;
        switch (type) {
            case kFuncFBTradePosition: {
                auto pos = flatbuffers::GetRoot<co::fbs::TradePosition>(raw.data());
//                LOG_INFO << ToString(*pos);
                int64_t timestamp = pos->timestamp();
                all_msg_.insert(std::make_pair(timestamp, std::make_pair(type, raw)));
                break;
            }
            case kFuncFBTradePolicy: {
                auto rep = flatbuffers::GetMutableRoot<co::fbs::TradeOrderMessage>((void*)raw.data());
                // LOG_INFO << ToString(*req);
                int64_t timestamp =rep->timestamp();
                all_msg_.insert(std::make_pair(timestamp, std::make_pair(type, raw)));
                break;
            }
            case kFuncFBTradeKnock: {
                auto knock = flatbuffers::GetRoot<co::fbs::TradeKnock>(raw.data());
//                int64_t timestamp =knock->timestamp();
                int64_t timestamp =knock->recv_time();
                all_msg_.insert(std::make_pair(timestamp, std::make_pair(type, raw)));
                break;
            }
            default:
                break;
        }
    }
}

void VerifyBroker::VerifyPosition() {
    for(const auto& [key, value] : all_msg_) {
        int64_t type = value.first;
        const string& raw = value.second;
        switch (type) {
            case kFuncFBTradePosition: {
                auto q = flatbuffers::GetRoot<co::fbs::TradePosition>(raw.data());
//                LOG_INFO << ToString(*q);
                string fund_id = q->fund_id() ? q->fund_id()->str() : "";
                string code = q->code() ? q->code()->str() : "";
                int64_t long_can_close = q->long_can_close();
                int64_t long_volume = q->long_volume();
                LOG_INFO << "查询持仓, "<< code << ", timestamp: " << q->timestamp() << ", long_volume: " << long_volume <<  ", long_can_close: " << long_can_close;
                inner_stock_master_.HandlePosition(q->timestamp(),  code, long_volume, long_can_close);
                break;
            }
            case kFuncFBTradePolicy: {
                auto rep = flatbuffers::GetMutableRoot<co::fbs::TradeOrderMessage>((void*)raw.data());
                LOG_INFO << ToString(*rep);
                string error = rep->error() ? rep->error()->str() : "";
                auto _req = flatbuffers::GetRoot<co::fbs::TradeOrderMessage>(raw.data());
                co::fbs::TradeOrderMessageT req;
                _req->UnPackTo(&req);
                int items_size = (int)req.items.size();
                for (int i = 0; i < items_size; ++i) {
                    auto &order = req.items[i];
                    if (!order->order_no.empty()) {
                        inner_stock_master_.HandleOrderRep(*order);
                    }
                }
                break;
            }
            case kFuncFBTradeKnock: {
                auto k = flatbuffers::GetRoot<co::fbs::TradeKnock>(raw.data());
                co::fbs::TradeKnockT knock;
                k->UnPackTo(&knock);
                // 只打印一次
                if (auto it = inner_match_no_.find(knock.inner_match_no); it == inner_match_no_.end()) {
                    LOG_INFO << ToString(*k);
                    inner_match_no_.insert(knock.inner_match_no);
                }
                inner_stock_master_.HandleKnock(knock);
                break;
            }
        }
    }
}


}  // namespace co

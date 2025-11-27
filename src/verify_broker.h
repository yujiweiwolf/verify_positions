// Copyright 2025 Fancapital Inc.  All rights reserved.
#pragma once
#include "broker/broker.h"
#include "stock_master.h"

namespace co {
class VerifyBroker{
 public:
    VerifyBroker();
    virtual~VerifyBroker();
    void Init(const string& file1, const string& file2);

private:
    void ReadFile(const string& file);
    void VerifyPosition();

 private:
   std::multimap<int64_t, std::pair<int64_t, string>> all_msg_;
   StockMaster inner_stock_master_;
   std::set<string> inner_match_no_;
};
}  // namespace co

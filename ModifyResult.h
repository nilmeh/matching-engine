#pragma once
#include "ExecutionReport.h"
#include <vector>

struct ModifyResult {
    bool success;
    std::vector<ExecutionReport> reports;
};
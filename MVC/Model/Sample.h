#pragma once
#include <string>

struct Sample {
    int    id;
    std::string name;
    double avgProductionTime;  // 평균 생산시간 (시간 단위)
    double yieldRate;          // 수율 (0.0 ~ 1.0)
    int    stock;              // 현재 재고
};

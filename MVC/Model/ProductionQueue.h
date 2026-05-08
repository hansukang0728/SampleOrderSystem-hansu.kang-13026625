#pragma once
#include <queue>

struct ProductionTask {
    int    orderId;
    int    sampleId;
    int    shortage;           // 부족 수량
    int    actualProduction;   // 실 생산량 = ceil(shortage / (yieldRate * 0.9))
    double totalProductionTime; // 총 생산시간 = avgProductionTime * actualProduction
};

using ProductionQueue = std::queue<ProductionTask>;

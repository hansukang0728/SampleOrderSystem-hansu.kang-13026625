#pragma once
#include "../Model/ProductionQueue.h"
#include "SampleController.h"
#include "OrderController.h"

class ProductionController {
public:
    ProductionController(ProductionQueue& queue, SampleController& sampleCtrl, OrderController& orderCtrl);

    bool                  processNext();    // FIFO: 다음 생산 작업 처리 → CONFIRMED
    int                   getQueueSize() const;
    const ProductionTask* peekNext() const;

private:
    ProductionQueue&  queue_;
    SampleController& sampleCtrl_;
    OrderController&  orderCtrl_;
};

#include "ProductionController.h"

ProductionController::ProductionController(ProductionQueue& queue, SampleController& sampleCtrl, OrderController& orderCtrl)
    : queue_(queue), sampleCtrl_(sampleCtrl), orderCtrl_(orderCtrl) {}

bool ProductionController::processNext() {
    if (queue_.empty()) return false;

    ProductionTask task = queue_.front();
    queue_.pop();

    // 생산 완료: 재고 보충 후 주문 상태 CONFIRMED로 전이
    if (Sample* s = sampleCtrl_.findById(task.sampleId))
        s->stock += task.actualProduction;

    if (Order* o = orderCtrl_.findById(task.orderId))
        o->status = OrderStatus::CONFIRMED;

    return true;
}

int ProductionController::getQueueSize() const {
    return (int)queue_.size();
}

const ProductionTask* ProductionController::peekNext() const {
    if (queue_.empty()) return nullptr;
    return &queue_.front();
}

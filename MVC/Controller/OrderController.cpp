#include "OrderController.h"
#include <cmath>
#include <stdexcept>

OrderController::OrderController(SampleController& sampleCtrl, ProductionQueue& prodQueue)
    : sampleCtrl_(sampleCtrl), prodQueue_(prodQueue) {}

int OrderController::createOrder(int sampleId, int quantity, const std::string& customerName) {
    if (!sampleCtrl_.findById(sampleId))
        throw std::runtime_error("존재하지 않는 시료 ID입니다.");

    Order o;
    o.id           = nextId_++;
    o.sampleId     = sampleId;
    o.quantity     = quantity;
    o.customerName = customerName;
    o.status       = OrderStatus::RESERVED;
    orders_.push_back(o);
    return o.id;
}

bool OrderController::approveOrder(int orderId) {
    Order* order = findById(orderId);
    if (!order || order->status != OrderStatus::RESERVED) return false;

    Sample* sample = sampleCtrl_.findById(order->sampleId);
    if (!sample) return false;

    if (sample->stock >= order->quantity) {
        // Case 1: 재고 충분 → 즉시 CONFIRMED
        sample->stock -= order->quantity;
        order->status = OrderStatus::CONFIRMED;
    } else {
        // Case 2: 재고 부족 → 생산 큐 등록 후 PRODUCING
        int shortage         = order->quantity - sample->stock;
        sample->stock        = 0;
        int actualProduction = (int)std::ceil(shortage / (sample->yieldRate * 0.9));
        double totalTime     = sample->avgProductionTime * actualProduction;

        prodQueue_.push({ order->id, sample->id, shortage, actualProduction, totalTime });
        order->status = OrderStatus::PRODUCING;
    }
    return true;
}

bool OrderController::rejectOrder(int orderId) {
    Order* order = findById(orderId);
    if (!order || order->status != OrderStatus::RESERVED) return false;
    order->status = OrderStatus::REJECTED;
    return true;
}

bool OrderController::releaseOrder(int orderId) {
    Order* order = findById(orderId);
    if (!order || order->status != OrderStatus::CONFIRMED) return false;
    order->status = OrderStatus::RELEASE;
    return true;
}

const std::vector<Order>& OrderController::getAllOrders() const {
    return orders_;
}

Order* OrderController::findById(int id) {
    for (auto& o : orders_)
        if (o.id == id) return &o;
    return nullptr;
}

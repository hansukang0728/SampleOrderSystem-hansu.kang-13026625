#pragma once
#include "../Model/Order.h"
#include "../Model/ProductionQueue.h"
#include "SampleController.h"
#include <vector>
#include <string>

class OrderController {
public:
    OrderController(SampleController& sampleCtrl, ProductionQueue& prodQueue);

    int  createOrder(int sampleId, int quantity, const std::string& customerName);
    bool approveOrder(int orderId);
    bool rejectOrder(int orderId);
    bool releaseOrder(int orderId);

    const std::vector<Order>& getAllOrders() const;
    Order* findById(int id);

private:
    std::vector<Order> orders_;
    SampleController&  sampleCtrl_;
    ProductionQueue&   prodQueue_;
    int nextId_ = 1;
};

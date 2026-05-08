#pragma once
#include "../Controller/SampleController.h"
#include "../Controller/OrderController.h"
#include "../Controller/ProductionController.h"

class MainView {
public:
    MainView(SampleController& sampleCtrl, OrderController& orderCtrl, ProductionController& prodCtrl);
    void run();

private:
    void showMainMenu();
    void handleSampleMenu();
    void handleOrderMenu();
    void handleProductionMenu();
    void handleMonitorMenu();

    void printSampleList();
    void printOrderList();
    void printStockStatus(const Sample& sample);

    SampleController&   sampleCtrl_;
    OrderController&    orderCtrl_;
    ProductionController& prodCtrl_;
};

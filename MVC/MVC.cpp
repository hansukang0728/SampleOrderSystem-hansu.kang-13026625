#include "Model/ProductionQueue.h"
#include "Controller/SampleController.h"
#include "Controller/OrderController.h"
#include "Controller/ProductionController.h"
#include "View/MainView.h"
#include <windows.h>

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    ProductionQueue    prodQueue;
    SampleController   sampleCtrl;
    OrderController    orderCtrl(sampleCtrl, prodQueue);
    ProductionController prodCtrl(prodQueue, sampleCtrl, orderCtrl);

    MainView view(sampleCtrl, orderCtrl, prodCtrl);
    view.run();

    return 0;
}

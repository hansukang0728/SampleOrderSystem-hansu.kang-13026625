#include "MainView.h"
#include <iostream>
#include <iomanip>
#include <limits>

MainView::MainView(SampleController& sampleCtrl, OrderController& orderCtrl, ProductionController& prodCtrl)
    : sampleCtrl_(sampleCtrl), orderCtrl_(orderCtrl), prodCtrl_(prodCtrl) {}

void MainView::run() {
    // 초기 샘플 데이터
    sampleCtrl_.addSample("시료A", 2.0, 0.95, 100);
    sampleCtrl_.addSample("시료B", 3.5, 0.85, 50);
    sampleCtrl_.addSample("시료C", 1.5, 0.90, 0);

    int choice = 0;
    while (true) {
        showMainMenu();
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        switch (choice) {
            case 1: handleSampleMenu();     break;
            case 2: handleOrderMenu();      break;
            case 3: handleProductionMenu(); break;
            case 4: handleMonitorMenu();    break;
            case 0: std::cout << "시스템을 종료합니다.\n"; return;
            default: std::cout << "잘못된 입력입니다.\n";
        }
    }
}

void MainView::showMainMenu() {
    std::cout << "\n========================================\n";
    std::cout << "  반도체 시료 생산주문관리 시스템 (POC)\n";
    std::cout << "========================================\n";
    std::cout << "  1. 시료 관리\n";
    std::cout << "  2. 주문 관리\n";
    std::cout << "  3. 생산 라인\n";
    std::cout << "  4. 모니터링\n";
    std::cout << "  0. 종료\n";
    std::cout << "========================================\n";
    std::cout << "선택: ";
}

void MainView::printStockStatus(const Sample& sample) {
    if (sample.stock == 0)       std::cout << "[고갈]";
    else if (sample.stock < 30)  std::cout << "[부족]";
    else                         std::cout << "[여유]";
}

void MainView::printSampleList() {
    std::cout << "\n[시료 목록]\n";
    std::cout << std::left
        << std::setw(5)  << "ID"
        << std::setw(10) << "이름"
        << std::setw(14) << "평균생산시간(h)"
        << std::setw(10) << "수율"
        << std::setw(8)  << "재고"
        << "상태\n";
    std::cout << std::string(55, '-') << "\n";
    for (const auto& s : sampleCtrl_.getAllSamples()) {
        std::cout << std::setw(5)  << s.id
                  << std::setw(10) << s.name
                  << std::setw(14) << s.avgProductionTime
                  << std::setw(10) << s.yieldRate
                  << std::setw(8)  << s.stock;
        printStockStatus(s);
        std::cout << "\n";
    }
}

void MainView::printOrderList() {
    std::cout << "\n[주문 목록]\n";
    std::cout << std::left
        << std::setw(8)  << "주문ID"
        << std::setw(8)  << "시료ID"
        << std::setw(8)  << "수량"
        << std::setw(14) << "고객명"
        << "상태\n";
    std::cout << std::string(50, '-') << "\n";
    for (const auto& o : orderCtrl_.getAllOrders()) {
        std::cout << std::setw(8)  << o.id
                  << std::setw(8)  << o.sampleId
                  << std::setw(8)  << o.quantity
                  << std::setw(14) << o.customerName
                  << orderStatusToString(o.status) << "\n";
    }
}

// ── 시료 관리 ──────────────────────────────────────────────────────────────
void MainView::handleSampleMenu() {
    std::cout << "\n--- 시료 관리 ---\n";
    std::cout << "  1. 시료 목록 조회\n";
    std::cout << "  2. 시료 등록\n";
    std::cout << "  3. 시료 검색\n";
    std::cout << "선택: ";
    int choice; std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (choice == 1) {
        printSampleList();
    } else if (choice == 2) {
        std::string name;
        double avgTime, yieldRate;
        int stock;
        std::cout << "시료명: ";          std::getline(std::cin, name);
        std::cout << "평균 생산시간(h): "; std::cin >> avgTime;
        std::cout << "수율(0.0~1.0): ";   std::cin >> yieldRate;
        std::cout << "초기 재고: ";        std::cin >> stock;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        sampleCtrl_.addSample(name, avgTime, yieldRate, stock);
        std::cout << "시료가 등록되었습니다.\n";
    } else if (choice == 3) {
        std::string keyword;
        std::cout << "검색어: "; std::getline(std::cin, keyword);
        auto results = sampleCtrl_.searchByName(keyword);
        if (results.empty()) { std::cout << "검색 결과가 없습니다.\n"; return; }
        for (auto* s : results)
            std::cout << "[" << s->id << "] " << s->name << " / 재고: " << s->stock << "\n";
    }
}

// ── 주문 관리 ──────────────────────────────────────────────────────────────
void MainView::handleOrderMenu() {
    std::cout << "\n--- 주문 관리 ---\n";
    std::cout << "  1. 주문 목록 조회\n";
    std::cout << "  2. 주문 생성\n";
    std::cout << "  3. 주문 승인\n";
    std::cout << "  4. 주문 거절\n";
    std::cout << "  5. 주문 출고\n";
    std::cout << "선택: ";
    int choice; std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (choice == 1) {
        printOrderList();
    } else if (choice == 2) {
        int sampleId, quantity;
        std::string customerName;
        printSampleList();
        std::cout << "시료 ID: "; std::cin >> sampleId;
        std::cout << "수량: ";    std::cin >> quantity;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "고객명: "; std::getline(std::cin, customerName);
        try {
            int id = orderCtrl_.createOrder(sampleId, quantity, customerName);
            std::cout << "주문 생성 완료. 주문 ID: " << id << " (RESERVED)\n";
        } catch (const std::exception& e) {
            std::cout << "오류: " << e.what() << "\n";
        }
    } else if (choice == 3) {
        printOrderList();
        int orderId; std::cout << "승인할 주문 ID: "; std::cin >> orderId;
        if (orderCtrl_.approveOrder(orderId)) {
            Order* o = orderCtrl_.findById(orderId);
            std::cout << "승인 완료. 현재 상태: " << orderStatusToString(o->status) << "\n";
        } else {
            std::cout << "승인 실패. (RESERVED 상태 주문만 승인 가능)\n";
        }
    } else if (choice == 4) {
        printOrderList();
        int orderId; std::cout << "거절할 주문 ID: "; std::cin >> orderId;
        std::cout << (orderCtrl_.rejectOrder(orderId) ? "주문이 거절되었습니다.\n" : "거절 실패.\n");
    } else if (choice == 5) {
        printOrderList();
        int orderId; std::cout << "출고할 주문 ID: "; std::cin >> orderId;
        std::cout << (orderCtrl_.releaseOrder(orderId)
            ? "출고 완료. 상태: RELEASE\n"
            : "출고 실패. (CONFIRMED 상태 주문만 출고 가능)\n");
    }
}

// ── 생산 라인 ─────────────────────────────────────────────────────────────
void MainView::handleProductionMenu() {
    std::cout << "\n--- 생산 라인 (FIFO) ---\n";
    std::cout << "대기 중인 생산 작업: " << prodCtrl_.getQueueSize() << "건\n";

    const ProductionTask* next = prodCtrl_.peekNext();
    if (!next) { std::cout << "처리할 생산 작업이 없습니다.\n"; return; }

    Sample* s = sampleCtrl_.findById(next->sampleId);
    std::cout << "\n[다음 생산 작업]\n";
    std::cout << "  주문 ID    : " << next->orderId << "\n";
    std::cout << "  시료       : " << (s ? s->name : "?") << "\n";
    std::cout << "  부족분     : " << next->shortage << "\n";
    std::cout << "  실 생산량  : " << next->actualProduction << "\n";
    std::cout << "  총 생산시간: " << next->totalProductionTime << "h\n";

    std::cout << "\n생산 처리하시겠습니까? (1=예 / 0=아니오): ";
    int yn; std::cin >> yn;
    if (yn == 1) {
        prodCtrl_.processNext();
        std::cout << "생산 완료. 주문 상태가 CONFIRMED로 변경되었습니다.\n";
    }
}

// ── 모니터링 ──────────────────────────────────────────────────────────────
void MainView::handleMonitorMenu() {
    std::cout << "\n--- 모니터링 ---\n";
    printSampleList();
    printOrderList();
    std::cout << "\n생산 큐 대기 건수: " << prodCtrl_.getQueueSize() << "\n";
}

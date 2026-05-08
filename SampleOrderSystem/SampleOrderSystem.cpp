#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <numeric>
#include <limits>
#include "Model/app_db.h"
#include "View/ConsoleUI.h"
#include "Service/SampleService.h"
#include "Service/OrderService.h"
#include "Service/MonitoringService.h"
#include "Service/ProductionService.h"
#include "Service/DummyDataService.h"
#include "View/SampleView.h"
#include "View/OrderView.h"
#include "View/OrderManagerView.h"
#include "View/MonitoringView.h"
#include "View/ProductionView.h"
#include "View/ReleaseView.h"
#include "View/DummyDataView.h"

static void InitConsole() {
    if (!SetConsoleOutputCP(CP_UTF8) || !SetConsoleCP(CP_UTF8))
        OutputDebugStringW(L"[WARN] UTF-8 콘솔 설정 실패\n");

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode))
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
int main(int argc, char** argv) {
    InitConsole();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#else

// ── SAMSUNG ASCII 아트 ────────────────────────────────────────
static void printSamsungLogo() {
    std::cout << UI::CYN << UI::BOLD;
    std::cout << "   ***   **  *   *  ***  *   * *  *  ***\n";
    std::cout << "  *     *  * ** ** *     *   * ** * *   \n";
    std::cout << "   **   **** * * *  **   *   * * ** *  *\n";
    std::cout << "     *  *  * *   *    *  *   * *  **    *\n";
    std::cout << "   ***  *  * *   *  ***   ***  *   *  ***\n";
    std::cout << UI::RST << "\n";
}

// ── 생산라인 상태 문자열 ──────────────────────────────────────
static std::string productionStatus(AppDB& db, const char*& color) {
    auto& q = db.queue();  // checkAndComplete() 자동 실행
    int inProgress = 0, waiting = 0;
    double maxPct = 0.0;

    for (const auto& p : q) {
        if      (p.isInProgress()) { ++inProgress; maxPct = std::max(maxPct, p.progressPct()); }
        else if (p.isWaiting())    { ++waiting; }
    }

    if (inProgress > 0 && waiting > 0) {
        color = UI::GRN;
        return "생산중 (" + std::to_string((int)maxPct) + "%) | 대기 " + std::to_string(waiting) + "건";
    }
    if (inProgress > 0) {
        color = UI::GRN;
        return "생산중 (" + std::to_string((int)maxPct) + "%)";
    }
    if (waiting > 0) {
        color = UI::YLW;
        return "대기 (" + std::to_string(waiting) + "건)";
    }
    color = UI::GRY;
    return "유휴 (IDLE)";
}

// ── 시스템 현황 전광판 ────────────────────────────────────────
static void printStatusBoard(AppDB& db) {
    // 통계 계산
    int sampleCnt = (int)db.samples().size();
    int totalStock = 0;
    for (const auto& s : db.samples()) totalStock += s.stock;

    int activeCnt = 0;
    for (const auto& o : db.orders())
        if (o.status != OrderStatus::REJECTED) ++activeCnt;

    const char* prodColor = UI::GRY;
    std::string prodStatus = productionStatus(db, prodColor);

    const std::string HL = UI::rep("─", UI::WIDTH);

    std::cout << "\n"
              << UI::DIM << "  ┌" << HL << "┐\n" << UI::RST
              << UI::GRY  << "  │  📅 " << UI::WHT << nowStr()
              << UI::GRY  << std::string(UI::WIDTH - 6 - 19, ' ') << "│\n"  // 날짜/시간 19자
              << UI::GRY  << "  │  " << UI::RST
              << UI::GRY  << "🧪 등록 시료 : " << UI::CYN  << UI::BOLD
              << std::setw(3) << sampleCnt << "개" << UI::RST << UI::GRY
              << "       📦 총 재고  : " << UI::WHT << UI::BOLD
              << std::setw(5) << totalStock << " ea" << UI::RST << UI::GRY
              << "   │\n"
              << "  │  " << UI::RST
              << UI::GRY  << "📋 주문 건수 : " << UI::YLW2 << UI::BOLD
              << std::setw(3) << activeCnt << "건" << UI::RST << UI::GRY
              << "       🏭 생산라인 : " << prodColor << UI::BOLD
              << prodStatus << UI::RST << UI::GRY << "\n"
              << "  │" << std::string(UI::WIDTH + 2, ' ') << "│\n"
              << UI::DIM  << "  └" << HL << "┘\n" << UI::RST;
}

// ── 메인 메뉴 출력 ────────────────────────────────────────────
static void printMainMenu(AppDB& db) {
    UI::clearScreen();
    printSamsungLogo();
    UI::printHeader("반도체 시료 생산 주문 관리 시스템");
    printStatusBoard(db);

    std::cout << "\n"
              << "  " << UI::CYN << " 1. " << UI::RST << UI::WHT << " 시료 관리        " << UI::GRY << "(등록 / 조회 / 검색)\n"       << UI::RST
              << "  " << UI::CYN << " 2. " << UI::RST << UI::WHT << " 시료 주문        " << UI::GRY << "(주문 생성)\n"                 << UI::RST
              << "  " << UI::CYN << " 3. " << UI::RST << UI::WHT << " 주문 승인 / 거절 " << UI::GRY << "(RESERVED 주문 처리)\n"        << UI::RST
              << "  " << UI::CYN << " 4. " << UI::RST << UI::WHT << " 모니터링         " << UI::GRY << "(재고 현황 / 주문 현황)\n"     << UI::RST
              << "  " << UI::CYN << " 5. " << UI::RST << UI::WHT << " 생산 라인 조회   " << UI::GRY << "(생산 큐 확인 / 처리)\n"      << UI::RST
              << "  " << UI::CYN << " 6. " << UI::RST << UI::WHT << " 출고 처리        " << UI::GRY << "(CONFIRMED 주문 출고)\n"       << UI::RST
              << "  " << UI::CYN << " 7. " << UI::RST << UI::WHT << " 더미 데이터 / 초기화\n"                                          << UI::RST;
    UI::printHLine();
    std::cout << "  " << UI::DIM << " 0. " << UI::RST << UI::GRY << " 종료\n" << UI::RST;
    UI::printHLine();
}

// ── main ──────────────────────────────────────────────────────
int main() {
    InitConsole();
    AppDB            db("data.json");
    SampleService    sampleSvc(db);
    OrderService     orderSvc(db);
    DummyDataService dummySvc(db);
    MonitoringService  monitorSvc(db);
    ProductionService  prodSvc(db);
    SampleView         sampleView(sampleSvc);
    OrderView          orderView(orderSvc, sampleSvc);
    OrderManagerView   orderMgrView(orderSvc, sampleSvc);
    MonitoringView     monitorView(monitorSvc);
    ProductionView     prodView(prodSvc, db);
    ReleaseView        releaseView(orderSvc, sampleSvc);
    DummyDataView      dummyView(dummySvc);

    int choice = -1;
    while (choice != 0) {
        printMainMenu(db);
        choice = UI::readInt("  선택: ");

        switch (choice) {
        case 1: sampleView.run();   UI::goToMain = false; break;
        case 2: orderView.run();    UI::goToMain = false; break;
        case 3: orderMgrView.run(); UI::goToMain = false; break;
        case 4: monitorView.run();  UI::goToMain = false; break;
        case 5: prodView.run();     UI::goToMain = false; break;
        case 6: releaseView.run(); UI::goToMain = false; break;
        case 7: dummyView.run();    UI::goToMain = false; break;
        case 0: std::cout << "\n"; UI::printSuccess("종료합니다.\n");           break;
        default: UI::printError("잘못된 선택입니다."); break;
        }
    }
    return 0;
}
#endif

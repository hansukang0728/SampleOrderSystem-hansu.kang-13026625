#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include "View/ConsoleUI.h"
#include "Service/MonitoringService.h"

class MonitoringView {
public:
    explicit MonitoringView(MonitoringService& svc) : svc_(svc) {}

    void run() {
        while (true) {
            render();
            std::string inp = UI::readLine(
                "  [Enter] 새로고침   [0] 뒤로   [m] 메인: ");
            if (inp == "0") return;
            if (inp == "m" || inp == "M") { UI::goToMain = true; return; }
            // 그 외(Enter 포함) → 재렌더링
        }
    }

private:
    MonitoringService& svc_;

    // ── 섹션 헤더 (타이틀 + 우측 업데이트 시각) ──────────────
    void printSectionHeader(const std::string& title,
                            const std::string& updatedAt) const {
        // "[제목]" 왼쪽 + 시각 오른쪽 정렬
        int titleWidth  = UI::dispWidth(title);
        int timeWidth   = static_cast<int>(updatedAt.size());
        int gap         = UI::WIDTH - titleWidth - timeWidth - 2; // "  " prefix
        if (gap < 1) gap = 1;

        std::cout << "  "
                  << UI::CYN << UI::BOLD << title << UI::RST
                  << std::string(gap, ' ')
                  << UI::GRY << updatedAt << UI::RST << "\n";
    }

    // ── M-02: 주문 현황 ───────────────────────────────────────
    void renderOrderStatus(const DashboardData& d) const {
        std::cout << "\n";
        printSectionHeader("[ 주문 현황 ]  (REJECTED 제외)", d.updatedAt);
        UI::printHLine();

        auto row = [](const char* label, int cnt, const char* col) {
            std::cout << "  "
                      << UI::GRY << UI::padR(label, 14) << ": " << UI::RST
                      << col << UI::BOLD << std::setw(4) << cnt << "건"
                      << UI::RST << "\n";
        };

        row("RESERVED",  d.cntReserved,  UI::YLW2);
        row("PRODUCING", d.cntProducing, UI::YLW);
        row("CONFIRMED", d.cntConfirmed, UI::GRN);
        row("RELEASE",   d.cntRelease,   UI::GRY);
        UI::printHLine();
    }

    // ── M-01: 재고 현황 ───────────────────────────────────────
    void renderStockStatus(const std::string& updatedAt) const {
        const auto& samples = svc_.db().samples();  // collect() 이후 안전

        std::cout << "\n";
        printSectionHeader("[ 재고 현황 ]", updatedAt);
        UI::printHLine();

        if (samples.empty()) {
            UI::printInfo("등록된 시료가 없습니다.");
            UI::printHLine();
            return;
        }

        std::cout << UI::YLW << UI::BOLD
                  << "  " << UI::padR("ID",   7)
                  << UI::padR("이름", 22)
                  << std::right << std::setw(9) << "재고(ea)"
                  << "   상태\n"
                  << UI::RST;
        UI::printHLine();

        for (const auto& s : samples) {
            auto info = svc_.calcStockInfo(s.id);
            std::cout << "  "
                      << UI::CYN << UI::padR(s.id,   7) << UI::RST
                      << UI::WHT << UI::padR(s.name, 22) << UI::RST
                      << std::right << std::setw(7) << s.stock << " ea"
                      << "   "
                      << info.color << UI::BOLD << info.status << UI::RST
                      << "  " << info.color << info.icon << UI::RST
                      << "\n";
        }
        UI::printHLine();
    }

    // ── 전체 화면 렌더링 ──────────────────────────────────────
    void render() {
        UI::clearScreen();

        // ① collect() 1회 — updatedAt 캡처 + checkAndComplete()
        DashboardData d = svc_.collect();

        // 헤더 (업데이트 시각 포함)
        const std::string EQ = UI::rep("═", UI::WIDTH);
        int titleW = UI::dispWidth("모니터링 대시보드");
        int timeW  = static_cast<int>(d.updatedAt.size());
        int gap    = UI::WIDTH - titleW - timeW - 2;
        if (gap < 1) gap = 1;

        std::cout << "\n"
                  << UI::CYN << UI::BOLD
                  << "  ╔" << EQ << "╗\n"
                  << "  ║   " << "모니터링 대시보드"
                  << std::string(gap, ' ')
                  << UI::RST << UI::GRY << d.updatedAt << UI::RST
                  << UI::CYN << UI::BOLD << "\n"
                  << "  ╚" << EQ << "╝\n"
                  << UI::RST;

        // ② M-02 주문 현황
        renderOrderStatus(d);

        // ③ M-01 재고 현황 (동일 updatedAt 사용)
        renderStockStatus(d.updatedAt);

        std::cout << "\n";
        UI::printHLine();
    }
};

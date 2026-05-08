#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "View/ConsoleUI.h"
#include "Service/ProductionService.h"

class ProductionView {
public:
    static const DWORD REFRESH_MS = 3000;  // 3초 자동 새로고침

    ProductionView(ProductionService& svc, AppDB& db)
        : svc_(svc), db_(db) {}

    void run() {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

        while (true) {
            FlushConsoleInputBuffer(hStdin);  // 잔여 입력 제거
            render();

            DWORD ret = WaitForSingleObject(hStdin, REFRESH_MS);

            if (ret == WAIT_OBJECT_0) {
                // 키 이벤트 읽기 (마우스·리사이즈 이벤트 필터링)
                INPUT_RECORD rec;
                DWORD        nRead = 0;
                ReadConsoleInput(hStdin, &rec, 1, &nRead);

                if (nRead > 0 && rec.EventType == KEY_EVENT
                              && rec.Event.KeyEvent.bKeyDown) {
                    char ch = rec.Event.KeyEvent.uChar.AsciiChar;
                    if (ch == '0') return;
                    if (ch == 'm' || ch == 'M') { UI::goToMain = true; return; }
                    // Enter 또는 다른 키 → 재렌더링
                }
            }
            // WAIT_TIMEOUT → 3초 경과, 자동 재렌더링
        }
    }

private:
    ProductionService& svc_;
    AppDB&             db_;

    // ── 전체 화면 렌더링 ──────────────────────────────────────
    void render() {
        UI::clearScreen();

        // ① checkAndComplete() — 부작용 1회
        db_.checkAndComplete();

        // 헤더
        UI::printHeader("생산 라인");
        std::cout << UI::GRY << "  📅 " << UI::WHT << nowStr()
                  << UI::GRY << "  (3초마다 자동 갱신)\n" << UI::RST;

        // ② 생산 현황 (IN_PROGRESS)
        renderInProgress();

        // ③ 대기 주문 (WAITING)
        renderWaiting();

        // 하단 네비게이션
        std::cout << "\n"
                  << UI::DIM  << "  [Enter] 즉시 새로고침   "
                  << UI::RST  << UI::GRY << "[0] 뒤로   [m] 메인\n"
                  << UI::RST;
        UI::printHLine();
    }

    // ── 섹션 헤더 ─────────────────────────────────────────────
    void printSectionHeader(const std::string& title) const {
        std::cout << "\n"
                  << UI::DIM << "  " << UI::rep("─", 10) << UI::RST << " "
                  << UI::MGN << UI::BOLD << title << UI::RST
                  << " " << UI::DIM << UI::rep("─", 10) << UI::RST << "\n";
    }

    // ── 생산 현황 카드 (IN_PROGRESS) ─────────────────────────
    void renderInProgress() const {
        printSectionHeader("[ 생산 현황 ]  IN_PROGRESS");

        const auto* p = svc_.inProgressItem();
        if (!p) {
            UI::printInfo("현재 생산 중인 작업이 없습니다.");
            return;
        }

        // 주문·시료 정보 조회
        const Order*  o = db_.findOrder(p->order_id);
        const Sample* s = db_.findSample(p->sample_id);

        std::string sampleName   = s ? s->name : p->sample_id;
        std::string customerName = o ? o->customer_name : "-";
        std::string orderTime    = o ? o->created_at : "-";

        int    curProd = ProductionService::currentProduction(*p);
        double pct     = p->progressPct();
        std::string estComp = ProductionService::estimatedCompletion(*p);

        // 카드 출력
        std::cout << "\n"
                  << UI::GRY << "  주문번호    : " << UI::CYN  << p->order_id     << "\n"
                  << UI::GRY << "  시료        : " << UI::WHT  << p->sample_id
                                                   << "  " << sampleName          << "\n"
                  << UI::GRY << "  고객명      : " << UI::WHT  << customerName     << "\n"
                  << UI::GRY << "  주문 시각   : " << UI::WHT  << orderTime        << "\n"
                  << UI::GRY << "  생산 시작   : " << UI::WHT  << p->started_at   << "\n"
                  << UI::RST;
        UI::printHLine();
        std::cout << UI::GRY << "  부족분      : " << UI::WHT  << std::setw(4) << p->shortage
                                                   << " ea      실 생산량  : "
                  << UI::YLW << std::setw(4) << p->actual_qty << " ea\n"
                  << UI::GRY << "  총 생산시간 : " << UI::WHT
                             << std::fixed << std::setprecision(1) << p->total_time
                             << "분     현재 생산량: "
                  << UI::GRN << std::setw(4) << curProd << " ea  ("
                  << std::setprecision(1) << pct << "%)\n"
                  << UI::GRY << "  예상 완료   : " << UI::CYN  << estComp          << "\n"
                  << UI::RST;
        UI::printHLine();
    }

    // ── 대기 주문 목록 (WAITING · FIFO) ──────────────────────
    void renderWaiting() const {
        printSectionHeader("[ 대기 주문 ]  WAITING · FIFO");

        auto wq = svc_.waitingQueue();
        if (wq.empty()) {
            UI::printInfo("대기 중인 주문이 없습니다.");
            return;
        }

        UI::printHLine();
        std::cout << UI::YLW << UI::BOLD
                  << "  " << std::setw(2) << "#"
                  << "   " << UI::padR("주문번호",    22)
                  << UI::padR("시료",       10)
                  << UI::padR("고객명",     16)
                  << std::right << std::setw(7) << "수량"
                  << "   대기 시작\n"
                  << UI::RST;
        UI::printHLine();

        int seq = 1;
        for (const auto* p : wq) {
            const Order*  o = db_.findOrder(p->order_id);
            const Sample* s = db_.findSample(p->sample_id);

            std::string sampleLabel  = p->sample_id + (s ? " " + s->name : "");
            std::string customerName = o ? o->customer_name : "-";
            // enqueued_at 앞 16자 (YYYY-MM-DD HH:MM)
            std::string enqTime = p->enqueued_at.size() >= 16
                                  ? p->enqueued_at.substr(0, 16) : p->enqueued_at;

            std::cout << "  " << UI::CYN << std::setw(2) << seq++ << UI::RST << "   "
                      << UI::WHT << UI::padR(p->order_id,   22) << UI::RST
                      << UI::CYN << UI::padR(sampleLabel,   10) << UI::RST
                      << UI::WHT << UI::padR(customerName,  16) << UI::RST
                      << std::right << std::setw(5) << p->shortage << " ea"
                      << "   " << UI::GRY << enqTime << UI::RST << "\n";
        }
        UI::printHLine();
        std::cout << UI::GRY << "  대기: " << UI::WHT << wq.size() << "건\n" << UI::RST;
    }
};

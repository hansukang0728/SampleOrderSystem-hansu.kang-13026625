#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include "View/ConsoleUI.h"
#include "Service/OrderService.h"
#include "Service/SampleService.h"

class OrderManagerView {
public:
    static const int PAGE_SIZE = 5;

    OrderManagerView(OrderService& orderSvc, SampleService& sampleSvc)
        : orderSvc_(orderSvc), sampleSvc_(sampleSvc) {}

    void run() {
        int page = 0;

        while (true) {
            auto reserved = orderSvc_.reservedOrders();

            UI::clearScreen();
            UI::printHeader("주문 승인 / 거절");

            if (reserved.empty()) {
                std::cout << "\n";
                UI::printInfo("처리 대기 중인 주문이 없습니다.");
                UI::waitEnter(); return;
            }

            int totalPages = (static_cast<int>(reserved.size()) + PAGE_SIZE - 1)
                             / PAGE_SIZE;
            if (page >= totalPages) page = totalPages - 1;

            printPage(reserved, page, totalPages);

            // ── 입력 처리 ─────────────────────────────────────
            std::string input = UI::readLine("  선택: ");

            if (input == "0") return;
            if (input == "n" && page < totalPages - 1) { ++page; continue; }
            if (input == "p" && page > 0)              { --page; continue; }

            // 번호 선택 (1 ~ 현재 페이지 항목 수)
            int num = 0;
            try { num = std::stoi(input); } catch (...) { continue; }

            int start = page * PAGE_SIZE;
            int end   = std::min(start + PAGE_SIZE,
                                 static_cast<int>(reserved.size()));
            int idx   = start + num - 1;

            if (num < 1 || idx >= end) continue;

            const Order* selected = reserved[idx];
            if (handleSelected(*selected))
                page = 0;  // 처리 후 첫 페이지로 리셋 (목록 갱신)
        }
    }

private:
    OrderService&  orderSvc_;
    SampleService& sampleSvc_;

    // ── 페이지 출력 ───────────────────────────────────────────

    void printPage(const std::vector<const Order*>& reserved,
                   int page, int totalPages) const {
        int start = page * PAGE_SIZE;
        int end   = std::min(start + PAGE_SIZE,
                             static_cast<int>(reserved.size()));

        std::cout << "\n";
        UI::printHLine();
        std::cout << UI::YLW << UI::BOLD
                  << "  " << std::setw(2) << "#"
                  << "   " << UI::padR("주문번호",    22)
                  << UI::padR("시료",      8)
                  << UI::padR("고객명",    16)
                  << std::right << std::setw(7) << "수량"
                  << "   접수일시\n"
                  << UI::RST;
        UI::printHLine();

        for (int i = start; i < end; ++i) {
            const Order& o = *reserved[i];
            // 접수일시: 앞 16자만 표시 (초 생략)
            std::string dt = o.created_at.size() >= 16
                             ? o.created_at.substr(0, 16) : o.created_at;
            std::cout << "  " << UI::CYN << std::setw(2) << (i - start + 1)
                      << UI::RST << "   "
                      << UI::WHT << UI::padR(o.id,            22) << UI::RST
                      << UI::CYN << UI::padR(o.sample_id,      8) << UI::RST
                      << UI::WHT << UI::padR(o.customer_name, 16) << UI::RST
                      << std::right << std::setw(5) << o.quantity << " ea"
                      << "   " << UI::GRY << dt << UI::RST << "\n";
        }
        UI::printHLine();

        // 페이지 정보
        std::cout << "\n"
                  << UI::GRY << "  총 " << UI::WHT << reserved.size()
                  << UI::GRY << "건 (RESERVED) | "
                  << UI::CYN << (page + 1) << " / " << totalPages
                  << UI::GRY << " 페이지\n\n" << UI::RST;

        // 네비게이션
        std::cout << "  ";
        if (end - start > 0)
            std::cout << UI::YLW << "[1~" << (end - start) << "] 선택   " << UI::RST;
        if (page < totalPages - 1)
            std::cout << UI::YLW << "[n] 다음   " << UI::RST;
        if (page > 0)
            std::cout << UI::YLW << "[p] 이전   " << UI::RST;
        std::cout << UI::DIM << "[0] 뒤로\n" << UI::RST;
        UI::printHLine();
    }

    // ── 주문 선택 후 승인/거절 처리 ──────────────────────────
    // 반환: true = 처리 완료(목록 갱신 필요), false = 취소

    bool handleSelected(const Order& order) {
        Sample* s = sampleSvc_.findById(order.sample_id);
        std::string sampleName = s ? s->name : order.sample_id;

        std::cout << "\n"
                  << UI::GRY << "  선택된 주문: "
                  << UI::CYN  << order.id << UI::RST
                  << UI::GRY  << " | " << UI::WHT << order.customer_name
                  << UI::GRY  << " | " << UI::CYN << order.sample_id
                  << UI::GRY  << " | " << UI::WHT << order.quantity << "ea"
                  << "\n";
        UI::printHLine();
        std::cout << UI::GRN  << "  [a] 승인   "
                  << UI::RED  << "[r] 거절   "
                  << UI::DIM  << "[0] 취소\n" << UI::RST;
        UI::printHLine();

        std::string action = UI::readLine("  선택: ");

        if (action == "a" || action == "A") {
            auto r = orderSvc_.approveOrder(order.id);
            if (!r.success) { UI::printError("승인 처리에 실패했습니다."); return false; }
            showApproveResult(r, order, sampleName, s);
            UI::waitEnter();
            return true;
        }
        if (action == "r" || action == "R") {
            orderSvc_.rejectOrder(order.id);
            std::cout << "\n"
                      << UI::GRY << "  주문 상태 : " << UI::RED << "REJECTED\n" << UI::RST;
            UI::printSuccess("거절 처리 완료.");
            UI::waitEnter();
            return true;
        }
        return false;  // 취소
    }

    // ── 승인 결과 출력 ────────────────────────────────────────

    void showApproveResult(const OrderService::ApproveResult& r,
                           const Order& order,
                           const std::string& sampleName,
                           const Sample* s) const {
        if (r.sufficient) {
            // Case 1: 재고 충분
            std::cout << "\n"
                      << UI::GRY << "  시료: " << UI::CYN << order.sample_id
                      << " " << UI::WHT << sampleName
                      << UI::GRY << " | 재고: " << UI::WHT << r.prevStock << "ea"
                      << UI::GRY << " | 주문수량: " << UI::WHT << order.quantity << "ea\n"
                      << UI::GRY << "  → 재고 충분 → 즉시 CONFIRMED\n\n"
                      << UI::GRY << "  재고 차감 : " << UI::WHT
                                  << r.prevStock << " → " << (r.prevStock - order.quantity) << " ea\n"
                      << UI::GRY << "  주문 상태 : " << UI::GRN << "CONFIRMED\n" << UI::RST;
            UI::printSuccess("승인 완료. 출고 대기 중입니다.");
        } else {
            // Case 2: 재고 부족
            int stockAfter = s ? s->stock : 0;  // 이미 0으로 처리됨
            std::cout << "\n"
                      << UI::GRY << "  시료: " << UI::CYN << order.sample_id
                      << " " << UI::WHT << sampleName
                      << UI::GRY << " | 재고: " << UI::WHT << r.prevStock << "ea"
                      << UI::GRY << " | 주문수량: " << UI::WHT << order.quantity << "ea\n"
                      << UI::GRY << "  → 재고 부족 (부족분: " << UI::YLW << r.shortage
                                  << "ea" << UI::GRY << ") → 생산 라인 등록\n\n"
                      << UI::GRY << "  실 생산량  : " << UI::WHT << r.actualQty << " ea\n"
                      << UI::GRY << "  총 생산시간: " << UI::WHT
                                  << std::fixed << std::setprecision(1)
                                  << r.totalTime << "분\n"
                      << UI::GRY << "  주문 상태  : " << UI::YLW << "PRODUCING\n" << UI::RST;
            UI::printSuccess("승인 완료. 생산 라인에 등록되었습니다.");
        }
    }
};

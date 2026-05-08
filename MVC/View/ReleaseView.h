#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include "View/ConsoleUI.h"
#include "Service/OrderService.h"
#include "Service/SampleService.h"

class ReleaseView {
public:
    static const int PAGE_SIZE = 5;

    ReleaseView(OrderService& orderSvc, SampleService& sampleSvc)
        : orderSvc_(orderSvc), sampleSvc_(sampleSvc) {}

    void run() {
        int page = 0;

        while (true) {
            auto confirmed = orderSvc_.confirmedOrders();

            UI::clearScreen();
            UI::printHeader("출고 처리");

            if (confirmed.empty()) {
                std::cout << "\n";
                UI::printInfo("출고 대기 중인 주문이 없습니다.");
                UI::waitEnter(); return;
            }

            int totalPages = (static_cast<int>(confirmed.size()) + PAGE_SIZE - 1)
                             / PAGE_SIZE;
            if (page >= totalPages) page = totalPages - 1;

            printPage(confirmed, page, totalPages);

            std::string input = UI::readLine("  선택: ");

            if (input == "0") return;
            if (input == "m" || input == "M") { UI::goToMain = true; return; }
            if (input == "n" && page < totalPages - 1) { ++page; continue; }
            if (input == "p" && page > 0)              { --page; continue; }

            int num = 0;
            try { num = std::stoi(input); } catch (...) { continue; }

            int start = page * PAGE_SIZE;
            int end   = std::min(start + PAGE_SIZE,
                                 static_cast<int>(confirmed.size()));
            int idx   = start + num - 1;
            if (num < 1 || idx >= end) continue;

            if (handleSelected(*confirmed[idx]))
                page = 0;  // 처리 후 첫 페이지로
        }
    }

private:
    OrderService&  orderSvc_;
    SampleService& sampleSvc_;

    void printPage(const std::vector<const Order*>& confirmed,
                   int page, int totalPages) const {
        int start = page * PAGE_SIZE;
        int end   = std::min(start + PAGE_SIZE,
                             static_cast<int>(confirmed.size()));

        std::cout << "\n";
        UI::printHLine();
        std::cout << UI::YLW << UI::BOLD
                  << "  " << std::setw(2) << "#"
                  << "   " << UI::padR("주문번호",  22)
                  << UI::padR("시료",       8)
                  << UI::padR("고객명",    16)
                  << std::right << std::setw(7) << "수량"
                  << "\n" << UI::RST;
        UI::printHLine();

        for (int i = start; i < end; ++i) {
            const Order& o = *confirmed[i];
            const Sample* s = sampleSvc_.findById(o.sample_id);
            std::string sampleLabel = o.sample_id + (s ? " " + s->name : "");

            std::cout << "  " << UI::CYN << std::setw(2) << (i - start + 1)
                      << UI::RST << "   "
                      << UI::WHT << UI::padR(o.id,            22) << UI::RST
                      << UI::CYN << UI::padR(o.sample_id,      8) << UI::RST
                      << UI::WHT << UI::padR(o.customer_name, 16) << UI::RST
                      << std::right << std::setw(5) << o.quantity << " ea"
                      << "\n";
        }
        UI::printHLine();

        std::cout << "\n"
                  << UI::GRY << "  총 " << UI::WHT << confirmed.size()
                  << UI::GRY << "건 (CONFIRMED) | "
                  << UI::CYN << (page + 1) << " / " << totalPages
                  << UI::GRY << " 페이지\n\n" << UI::RST;

        if (page > 0)               std::cout << UI::YLW << "  [p] 이전   " << UI::RST;
        if (page < totalPages - 1)  std::cout << UI::YLW << "  [n] 다음   " << UI::RST;
        std::cout << UI::DIM << "  [0] 뒤로   " << UI::RST
                  << UI::GRY << "[m] 메인\n" << UI::RST;
        UI::printHLine();
    }

    bool handleSelected(const Order& order) {
        const Sample* s = sampleSvc_.findById(order.sample_id);
        std::string sampleName = s ? s->name : order.sample_id;

        std::cout << "\n"
                  << UI::GRY << "  선택된 주문: "
                  << UI::CYN  << order.id       << UI::RST
                  << UI::GRY  << " | " << UI::WHT << order.customer_name
                  << UI::GRY  << " | " << UI::CYN << order.sample_id
                  << UI::GRY  << " | " << UI::WHT << order.quantity << "ea\n";
        UI::printHLine();

        if (!orderSvc_.releaseOrder(order.id)) {
            UI::printError("출고 처리에 실패했습니다."); return false;
        }

        std::cout << UI::GRY << "  주문 상태 : " << UI::RST
                  << UI::GRY << "CONFIRMED → " << UI::GRN << UI::BOLD
                  << "RELEASE\n" << UI::RST;
        UI::printSuccess("출고 처리 완료.");
        UI::waitEnter();
        return true;
    }
};

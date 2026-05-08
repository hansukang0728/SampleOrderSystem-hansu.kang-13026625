#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include "View/ConsoleUI.h"
#include "Service/OrderService.h"
#include "Service/SampleService.h"

class OrderView {
public:
    OrderView(OrderService& orderSvc, SampleService& sampleSvc)
        : orderSvc_(orderSvc), sampleSvc_(sampleSvc) {}

    // 주문 생성 흐름 (단일 기능, 서브메뉴 없음)
    // 취소 기능 미제공 — Ctrl+C 전용
    void run() {
        UI::clearScreen();
        UI::printHeader("시료 주문");
        std::cout << "\n";

        // ① 시료 ID 입력 — 재입력 루프
        Sample* sample = nullptr;
        while (true) {
            std::string id = UI::readLine("  시료 ID (예: S-001): ");
            sample = sampleSvc_.findById(id);
            if (sample) break;
            UI::printError("존재하지 않는 시료 ID입니다.");
        }
        std::cout << UI::GRY << "  → [" << UI::CYN << sample->id << UI::GRY << "] "
                  << UI::WHT << sample->name
                  << UI::GRY << " | 재고: " << UI::WHT << sample->stock << "ea"
                  << UI::RST << "\n\n";

        // ② 고객명 입력 — 재입력 루프
        std::string customer;
        while (true) {
            customer = UI::readLine("  고객명: ");
            if (OrderService::validateCustomerName(customer)) break;
            UI::printError("고객명을 입력해주세요.");
        }

        // ③ 주문 수량 입력 — 재입력 루프
        int qty{};
        while (true) {
            qty = UI::readInt("  주문 수량 (ea): ");
            if (OrderService::validateQuantity(qty)) break;
            UI::printError("주문 수량은 1 이상이어야 합니다.");
        }

        // ④ 주문 생성 및 결과 출력
        Order o = orderSvc_.createOrder(sample->id, qty, customer);

        std::cout << "\n";
        UI::printHLine();
        std::cout << UI::GRY << "   주문번호 : " << UI::CYN  << o.id                << "\n"
                  << UI::GRY << "   시료     : " << UI::WHT  << o.sample_id
                                                 << "  " << sample->name          << "\n"
                  << UI::GRY << "   고객명   : " << UI::WHT  << o.customer_name    << "\n"
                  << UI::GRY << "   수량     : " << UI::WHT  << o.quantity << " ea" << "\n"
                  << UI::GRY << "   상태     : " << UI::YLW  << "RESERVED"          << "\n"
                  << UI::RST;
        UI::printHLine();
        std::cout << "\n";
        UI::printSuccess("주문이 접수되었습니다.");
        UI::waitEnter();
    }

private:
    OrderService&  orderSvc_;
    SampleService& sampleSvc_;
};

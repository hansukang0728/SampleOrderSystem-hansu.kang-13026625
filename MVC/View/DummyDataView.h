#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "View/ConsoleUI.h"
#include "Service/DummyDataService.h"

class DummyDataView {
public:
    explicit DummyDataView(DummyDataService& svc) : svc_(svc) {}

    void run() {
        int choice = -1;
        while (choice != 0) {
            UI::clearScreen();
            UI::printHeader("더미 데이터 / 초기화");
            std::cout << "\n"
                      << "  " << UI::CYN << " 1. " << UI::RST
                      << UI::WHT << " 시료 더미 생성    " << UI::RST
                      << UI::GRY << "(D-01)\n"              << UI::RST
                      << "  " << UI::CYN << " 2. " << UI::RST
                      << UI::WHT << " 주문 더미 생성    " << UI::RST
                      << UI::GRY << "(D-02, 등록 시료 기반)\n" << UI::RST
                      << "  " << UI::RED << " 3. " << UI::RST
                      << UI::WHT << " 전체 데이터 초기화" << UI::RST
                      << UI::GRY << "(D-03)\n"              << UI::RST;
            UI::printHLine();
            std::cout << "  " << UI::DIM << " 0. " << UI::RST
                      << UI::GRY << " 뒤로\n" << UI::RST;
            UI::printHLine();

            choice = UI::readInt("  선택: ");
            switch (choice) {
            case 1: handleGenerateSamples(); break;
            case 2: handleGenerateOrders();  break;
            case 3: handleReset();           break;
            case 0:                          break;
            default: UI::printError("잘못된 선택입니다."); break;
            }
        }
    }

private:
    DummyDataService& svc_;

    // ── D-01: 시료 더미 생성 ─────────────────────────────────────
    void handleGenerateSamples() {
        UI::clearScreen();
        UI::printHeader("시료 더미 생성");
        std::cout << "\n"
                  << UI::GRY << "  생성 범위: 1 ~ "
                  << DummyDataService::MAX_SAMPLES << "개  "
                  << "(기본: " << DummyDataService::DEFAULT_SAMPLE_COUNT << "개)\n\n"
                  << UI::RST;

        int count = UI::readInt("  생성할 시료 수: ");
        if (count <= 0 || count > DummyDataService::MAX_SAMPLES) {
            UI::printError("1 ~ " + std::to_string(DummyDataService::MAX_SAMPLES)
                           + " 범위여야 합니다.");
            UI::waitEnter(); return;
        }

        auto samples = svc_.generateSamples(count);
        std::cout << "\n";
        printSampleTable(samples);
        UI::printSuccess(std::to_string(samples.size()) + "개 시료가 생성되었습니다.");
        UI::waitEnter();
    }

    // ── D-02: 주문 더미 생성 ─────────────────────────────────────
    void handleGenerateOrders() {
        UI::clearScreen();
        UI::printHeader("주문 더미 생성");

        // 등록 시료 없으면 안내
        // DummyDataService 내부에서 samples를 참조하므로 직접 체크 필요
        // AppDB 접근은 Service를 통해서만 — 간접적으로 generateOrders(0)로 검증
        auto probe = svc_.generateOrders(0);
        // generateOrders가 empty를 반환하는 경우는 samples 없는 경우
        // 하지만 count=0이면 항상 empty. 대신 D-02 전에 시료 먼저 생성 안내만 출력
        std::cout << "\n"
                  << UI::GRY << "  생성 범위: 1 ~ "
                  << DummyDataService::MAX_ORDERS << "건  "
                  << "(등록된 시료 중 무작위 선택)\n\n"
                  << UI::RST;

        int count = UI::readInt("  생성할 주문 수: ");
        if (count <= 0 || count > DummyDataService::MAX_ORDERS) {
            UI::printError("1 ~ " + std::to_string(DummyDataService::MAX_ORDERS)
                           + " 범위여야 합니다.");
            UI::waitEnter(); return;
        }

        auto orders = svc_.generateOrders(count);
        std::cout << "\n";
        if (orders.empty()) {
            UI::printError("등록된 시료가 없습니다. 시료를 먼저 생성해주세요.");
        } else {
            printOrderTable(orders);
            UI::printSuccess(std::to_string(orders.size()) + "건 주문이 생성되었습니다.");
        }
        UI::waitEnter();
    }

    // ── D-03: 전체 초기화 ────────────────────────────────────────
    void handleReset() {
        UI::clearScreen();
        UI::printHeader("전체 데이터 초기화");
        std::cout << "\n"
                  << UI::RED << UI::BOLD
                  << "  ⚠️  모든 시료·주문·생산 큐 데이터가 삭제됩니다.\n"
                  << "      이 작업은 되돌릴 수 없습니다.\n\n"
                  << UI::RST;

        std::string confirm = UI::readLine("  계속하려면 'YES' 입력: ");
        if (confirm != "YES") {
            UI::printInfo("취소되었습니다.");
            UI::waitEnter(); return;
        }

        svc_.resetAll();
        std::cout << "\n";
        UI::printSuccess("전체 데이터가 초기화되었습니다.");
        UI::waitEnter();
    }

    // ── 출력 헬퍼 ────────────────────────────────────────────────

    void printSampleTable(const std::vector<Sample>& samples) const {
        UI::printHLine();
        std::cout << UI::YLW << UI::BOLD
                  << "  " << UI::padR("ID",   7)
                  << UI::padR("이름", 22)
                  << std::right << std::setw(12) << "생산시간(분)"
                  << std::setw(9)  << "수율(%)"
                  << std::setw(9)  << "재고(ea)"
                  << UI::RST << "\n";
        UI::printHLine();
        for (const auto& s : samples) {
            std::cout << "  "
                      << UI::CYN << UI::padR(s.id,   7) << UI::RST
                      << UI::WHT << UI::padR(s.name, 22) << UI::RST
                      << std::right << std::setw(10)
                                    << std::fixed << std::setprecision(1)
                                    << s.avg_production_time
                      << std::setw(8) << std::fixed << std::setprecision(1)
                                      << (s.yield_rate * 100.0)
                      << std::setw(9) << s.stock
                      << "\n";
        }
        UI::printHLine();
        std::cout << "\n";
    }

    void printOrderTable(const std::vector<Order>& orders) const {
        UI::printHLine();
        std::cout << UI::YLW << UI::BOLD
                  << "  " << UI::padR("주문번호", 22)
                  << UI::padR("시료",   8)
                  << UI::padR("고객명", 12)
                  << std::right << std::setw(8) << "수량(ea)"
                  << "  상태\n"
                  << UI::RST;
        UI::printHLine();
        for (const auto& o : orders) {
            std::cout << "  "
                      << UI::CYN << UI::padR(o.id,            22) << UI::RST
                      << UI::WHT << UI::padR(o.sample_id,      8) << UI::RST
                      << UI::WHT << UI::padR(o.customer_name, 12) << UI::RST
                      << std::right << std::setw(6) << o.quantity
                      << "  " << UI::YLW2 << "RESERVED" << UI::RST
                      << "\n";
        }
        UI::printHLine();
        std::cout << "\n";
    }
};

#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include "View/ConsoleUI.h"
#include "Service/SampleService.h"

class SampleView {
public:
    explicit SampleView(SampleService& svc) : svc_(svc) {}

    void run() {
        int choice = -1;
        while (choice != 0) {
            UI::clearScreen();
            UI::printHeader("시료 관리");
            std::cout << "\n"
                      << "  " << UI::CYN << " 1. " << UI::RST << UI::WHT << " 시료 등록\n"         << UI::RST
                      << "  " << UI::CYN << " 2. " << UI::RST << UI::WHT << " 전체 조회\n"         << UI::RST
                      << "  " << UI::CYN << " 3. " << UI::RST << UI::WHT << " ID 조회\n"           << UI::RST
                      << "  " << UI::CYN << " 4. " << UI::RST << UI::WHT << " 이름 검색\n"         << UI::RST;
            UI::printHLine();
            std::cout << "  " << UI::DIM << " 0. " << UI::RST << UI::GRY << " 뒤로\n" << UI::RST;
            UI::printHLine();

            choice = UI::readInt("  선택: ");
            switch (choice) {
            case 1: handleAdd();      break;
            case 2: handleListAll();  break;
            case 3: handleFindById(); break;
            case 4: handleSearch();   break;
            case 0:                   break;
            default: UI::printError("잘못된 선택입니다."); break;
            }
        }
    }

private:
    SampleService& svc_;

    // ── 공통 출력 ─────────────────────────────────────────────

    void printTable(const std::vector<const Sample*>& samples) const {
        UI::printHLine();
        std::cout << UI::YLW << UI::BOLD
                  << "  " << UI::padR("ID",   7)
                  << UI::padR("이름", 22)
                  << std::right << std::setw(12) << "생산시간(분)"
                  << std::setw(9)  << "수율(%)"
                  << std::setw(9)  << "재고(ea)"
                  << UI::RST << "\n";
        UI::printHLine();
        for (const auto* s : samples) {
            std::cout << "  "
                      << UI::CYN << UI::padR(s->id,   7) << UI::RST
                      << UI::WHT << UI::padR(s->name, 22) << UI::RST
                      << std::right << std::setw(10)
                                    << std::fixed << std::setprecision(1)
                                    << s->avg_production_time
                      << std::setw(8) << std::fixed << std::setprecision(1)
                                      << (s->yield_rate * 100.0)
                      << std::setw(9) << s->stock
                      << "\n";
        }
        UI::printHLine();
    }

    void printSample(const Sample& s) const {
        UI::printHLine();
        std::cout << UI::GRY << "   ID            : " << UI::WHT << s.id             << "\n"
                  << UI::GRY << "   이름          : " << UI::WHT << s.name           << "\n"
                  << UI::GRY << "   평균 생산시간 : " << UI::WHT
                             << std::fixed << std::setprecision(2)
                             << s.avg_production_time << " 분/개\n"
                  << UI::GRY << "   수율          : " << UI::WHT
                             << std::fixed << std::setprecision(1)
                             << (s.yield_rate * 100.0) << " %\n"
                  << UI::GRY << "   재고          : " << UI::WHT << s.stock << " ea\n"
                  << UI::RST;
        UI::printHLine();
    }

    // ── S-01: 시료 등록 ───────────────────────────────────────

    void handleAdd() {
        UI::clearScreen();
        UI::printHeader("시료 등록");
        std::cout << "\n";

        // 각 필드별 재입력 루프 — 유효한 값이 입력될 때까지 반복
        std::string name;
        while (true) {
            name = UI::readLine("  이름: ");
            if (SampleService::validateName(name)) break;
            UI::printError("이름을 입력해주세요.");
        }

        double avgTime{};
        while (true) {
            avgTime = UI::readDouble("  평균 생산시간 (분/개): ");
            if (SampleService::validateAvgTime(avgTime)) break;
            UI::printError("생산시간은 0보다 커야 합니다.");
        }

        double yieldRate{};
        while (true) {
            yieldRate = UI::readDouble("  수율 (0.0~1.0): ");
            if (SampleService::validateYieldRate(yieldRate)) break;
            UI::printError("수율은 0.0 초과 1.0 이하여야 합니다.");
        }

        int stock{};
        while (true) {
            stock = UI::readInt("  초기 재고 (ea): ");
            if (SampleService::validateStock(stock)) break;
            UI::printError("재고는 0 이상이어야 합니다.");
        }

        Sample s = svc_.add(name, avgTime, yieldRate, stock);
        std::cout << "\n";
        UI::printSuccess("등록 완료 — ID: " + s.id + " | " + s.name);
        UI::waitEnter();
    }

    // ── S-02: 전체 조회 (페이지네이션) ───────────────────────

    void handleListAll() {
        const auto& all = svc_.all();
        if (all.empty()) {
            UI::clearScreen();
            UI::printHeader("시료 전체 조회");
            UI::printInfo("등록된 시료가 없습니다.");
            UI::waitEnter(); return;
        }

        int totalPg = svc_.totalPages();
        int page    = 0;

        while (true) {
            UI::clearScreen();
            UI::printHeader("시료 전체 조회");

            int start = page * SampleService::PAGE_SIZE;
            int end   = std::min(start + SampleService::PAGE_SIZE,
                                 static_cast<int>(all.size()));
            std::vector<const Sample*> slice;
            for (int i = start; i < end; ++i) slice.push_back(&all[i]);
            printTable(slice);

            std::cout << "\n"
                      << UI::GRY << "  총 " << UI::WHT << all.size() << "개"
                      << UI::GRY << " | " << UI::CYN << (page + 1) << " / " << totalPg
                      << UI::GRY << " 페이지\n\n" << UI::RST;

            if (page > 0)             std::cout << UI::YLW << "  [p] 이전 페이지   " << UI::RST;
            if (page < totalPg - 1)   std::cout << UI::YLW << "  [n] 다음 페이지   " << UI::RST;
            std::cout << UI::DIM << "  [0] 뒤로\n" << UI::RST;
            UI::printHLine();

            std::string input = UI::readLine("  선택: ");
            if      (input == "n" && page < totalPg - 1) ++page;
            else if (input == "p" && page > 0)           --page;
            else if (input == "0")                       break;
        }
    }

    // ── S-03: ID 조회 ─────────────────────────────────────────

    void handleFindById() {
        UI::clearScreen();
        UI::printHeader("시료 ID 조회");
        std::cout << "\n";

        std::string id = UI::readLine("  시료 ID (예: S-001): ");
        Sample* s = svc_.findById(id);
        if (!s) {
            UI::printError("존재하지 않는 시료 ID입니다."); UI::waitEnter(); return;
        }
        std::cout << "\n";
        printSample(*s);
        UI::waitEnter();
    }

    // ── S-04: 이름 검색 ──────────────────────────────────────

    void handleSearch() {
        UI::clearScreen();
        UI::printHeader("시료 이름 검색");
        std::cout << "\n";

        std::string kw = UI::readLine("  검색어: ");
        auto result = svc_.searchByName(kw);

        std::cout << "\n";
        if (result.empty()) {
            UI::printInfo("검색 결과가 없습니다.");
        } else {
            printTable(result);
            std::cout << UI::GRY << "  " << result.size() << "건 검색됨\n" << UI::RST;
        }
        UI::waitEnter();
    }
};

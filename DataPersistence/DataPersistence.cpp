// DataPersistence.cpp
// 시료 관리 시스템 - JSON 파일 기반 CRUD
// 요구사항: C++17, 프로젝트 속성 -> C/C++ -> 언어 -> C++ 언어 표준: ISO C++17

#include <iostream>
#include <string>
#include <iomanip>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#endif
#include "json_db.h"

// ── 공통 유틸 ────────────────────────────────────────────────
static void clearInputError() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

static void printTable(const std::vector<Sample>& samples) {
    if (samples.empty()) {
        std::cout << "  (등록된 시료 없음)\n";
        return;
    }
    std::cout << "\n"
              << std::left
              << std::setw(5)  << "ID"
              << std::setw(22) << "이름"
              << std::setw(18) << "평균생산시간(분)"
              << std::setw(10) << "수율(%)"
              << "\n"
              << std::string(55, '-') << "\n";
    for (const auto& s : samples) {
        std::cout << std::right << std::setw(4)  << s.id << "  "
                  << std::left  << std::setw(22) << s.name
                  << std::right << std::setw(12) << std::fixed << std::setprecision(1)
                                                 << s.avg_production_time << "      "
                  << std::setw(6) << std::fixed << std::setprecision(1)
                                                << (s.yield_rate * 100.0) << "\n";
    }
}

// ── CRUD 명령 ─────────────────────────────────────────────────
static void cmdCreate(SampleDB& db) {
    std::cout << "\n[시료 등록]\n";

    // 메뉴 선택 후 버퍼에 남은 개행 제거
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string name;
    std::cout << "이름: ";
    std::getline(std::cin, name);
    if (name.empty()) { std::cout << "이름이 비어있습니다.\n"; return; }

    double avgTime;
    std::cout << "평균 생산시간 (분/개): ";
    if (!(std::cin >> avgTime) || avgTime <= 0) {
        clearInputError();
        std::cout << "잘못된 입력입니다. (양수만 허용)\n"; return;
    }

    double yieldRate;
    std::cout << "수율 (0.0 ~ 1.0): ";
    if (!(std::cin >> yieldRate) || yieldRate < 0.0 || yieldRate > 1.0) {
        clearInputError();
        std::cout << "잘못된 입력입니다. (0.0 ~ 1.0 범위만 허용)\n"; return;
    }

    Sample s = db.create(name, avgTime, yieldRate);
    std::cout << "등록 완료! ID: " << s.id << "\n";
}

static void cmdList(SampleDB& db) {
    std::cout << "\n[전체 시료 조회]";
    printTable(db.all());
}

static void cmdFindById(SampleDB& db) {
    std::cout << "\n[ID 조회]\n조회할 ID: ";
    int id;
    if (!(std::cin >> id)) { clearInputError(); std::cout << "잘못된 입력입니다.\n"; return; }

    Sample* s = db.findById(id);
    if (!s) { std::cout << "ID " << id << " 시료를 찾을 수 없습니다.\n"; return; }

    std::cout << "\n"
              << "  ID              : " << s->id << "\n"
              << "  이름            : " << s->name << "\n"
              << "  평균 생산시간   : " << std::fixed << std::setprecision(2)
                                        << s->avg_production_time << " 분/개\n"
              << "  수율            : " << std::fixed << std::setprecision(1)
                                        << (s->yield_rate * 100.0) << " %\n";
}

static void cmdUpdate(SampleDB& db) {
    std::cout << "\n[시료 수정]\n수정할 ID: ";
    int id;
    if (!(std::cin >> id)) { clearInputError(); std::cout << "잘못된 입력입니다.\n"; return; }

    Sample* found = db.findById(id);
    if (!found) { std::cout << "ID " << id << " 시료를 찾을 수 없습니다.\n"; return; }

    Sample s = *found;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string input;
    std::cout << "이름 [" << s.name << "] (변경 없으면 Enter): ";
    std::getline(std::cin, input);
    if (!input.empty()) s.name = input;

    std::cout << "평균 생산시간 [" << s.avg_production_time << "] (변경 없으면 Enter): ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try {
            double v = std::stod(input);
            if (v > 0) s.avg_production_time = v;
            else std::cout << "  양수여야 합니다. 기존 값 유지.\n";
        } catch (...) { std::cout << "  잘못된 형식. 기존 값 유지.\n"; }
    }

    std::cout << "수율 [" << s.yield_rate << "] (변경 없으면 Enter): ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try {
            double v = std::stod(input);
            if (v >= 0.0 && v <= 1.0) s.yield_rate = v;
            else std::cout << "  0.0~1.0 범위여야 합니다. 기존 값 유지.\n";
        } catch (...) { std::cout << "  잘못된 형식. 기존 값 유지.\n"; }
    }

    if (db.update(s)) std::cout << "수정 완료!\n";
    else              std::cout << "수정 실패.\n";
}

static void cmdDelete(SampleDB& db) {
    std::cout << "\n[시료 삭제]\n삭제할 ID: ";
    int id;
    if (!(std::cin >> id)) { clearInputError(); std::cout << "잘못된 입력입니다.\n"; return; }

    Sample* s = db.findById(id);
    if (!s) { std::cout << "ID " << id << " 시료를 찾을 수 없습니다.\n"; return; }

    std::cout << "ID " << s->id << " [" << s->name << "] 를 삭제하시겠습니까? (y/n): ";
    char c;
    std::cin >> c;
    if (c != 'y' && c != 'Y') { std::cout << "취소되었습니다.\n"; return; }

    if (db.remove(id)) std::cout << "삭제 완료!\n";
    else               std::cout << "삭제 실패.\n";
}

// ── main ──────────────────────────────────────────────────────
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // data.json은 실행 파일 위치 기준으로 생성됨
    SampleDB db("data.json");

    int choice = -1;
    while (choice != 0) {
        std::cout << "\n==============================\n"
                  << "    시료(Sample) 관리 시스템\n"
                  << "==============================\n"
                  << " 1. 시료 등록\n"
                  << " 2. 전체 시료 조회\n"
                  << " 3. ID로 시료 조회\n"
                  << " 4. 시료 수정\n"
                  << " 5. 시료 삭제\n"
                  << " 0. 종료\n"
                  << "선택: ";

        if (!(std::cin >> choice)) {
            clearInputError();
            continue;
        }

        switch (choice) {
        case 1: cmdCreate(db);   break;
        case 2: cmdList(db);     break;
        case 3: cmdFindById(db); break;
        case 4: cmdUpdate(db);   break;
        case 5: cmdDelete(db);   break;
        case 0: std::cout << "종료합니다.\n"; break;
        default: std::cout << "잘못된 선택입니다.\n"; break;
        }
    }
    return 0;
}

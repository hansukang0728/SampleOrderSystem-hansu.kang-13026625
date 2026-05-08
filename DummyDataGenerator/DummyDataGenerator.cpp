#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <random>
#include <ctime>
#include <cmath>
#include "json_lite.h"
#include "models.h"

// ── ANSI 색상 ────────────────────────────────────────────────
namespace C {
    const char* RST  = "\033[0m";
    const char* BOLD = "\033[1m";
    const char* DIM  = "\033[2m";
    const char* CYN  = "\033[96m";
    const char* YLW  = "\033[93m";
    const char* GRN  = "\033[92m";
    const char* WHT  = "\033[97m";
    const char* GRY  = "\033[90m";
}

// ── 이름 풀 (그리스 문자 계열) ───────────────────────────────
static const std::string NAME_POOL[] = {
    "알파", "베타", "감마", "델타", "엡실론",
    "제타", "에타", "세타", "이오타", "카파",
    "람다", "뮤",   "뉴",   "크시",  "오미크론",
    "파이", "로",   "시그마","타우", "웁실론"
};
static const int POOL_SIZE = 20;

// ── 유틸 ─────────────────────────────────────────────────────
static std::string rep(const std::string& s, int n) {
    std::string r; for (int i = 0; i < n; ++i) r += s; return r;
}

static int dispWidth(const std::string& s) {
    int w = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = s[i];
        if      (c < 0x80) { w += 1; i += 1; }
        else if (c < 0xE0) { w += 1; i += 2; }
        else if (c < 0xF0) { w += 2; i += 3; }
        else                { w += 2; i += 4; }
    }
    return w;
}

static std::string padR(const std::string& s, int width) {
    std::string r = s;
    int pad = width - dispWidth(s);
    if (pad > 0) r += std::string(pad, ' ');
    return r;
}

// ── 더미 데이터 생성 ──────────────────────────────────────────
static std::vector<Sample> generate(int count, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> timeDist(10.0, 120.0);
    std::uniform_real_distribution<double> yieldDist(0.70, 0.99);

    std::vector<Sample> samples;
    samples.reserve(count);

    for (int i = 0; i < count; ++i) {
        Sample s;
        s.id = i + 1;

        // 이름: 풀 20개 순환, 21번째부터 번호 접미사 추가
        std::string base = NAME_POOL[i % POOL_SIZE];
        if (i >= POOL_SIZE)
            base += "-" + std::to_string(i / POOL_SIZE + 1);
        s.name = base + "-시료";

        s.avg_production_time =
            std::round(timeDist(rng) * 10.0) / 10.0;
        s.yield_rate =
            std::round(yieldDist(rng) * 1000.0) / 1000.0;

        samples.push_back(s);
    }
    return samples;
}

// ── data.json 저장 ────────────────────────────────────────────
static void saveToDb(const std::vector<Sample>& samples,
                     const std::string& path)
{
    auto root = JsonValue::makeObject();
    root["next_id"] = JsonValue((int)samples.size() + 1);
    auto arr = JsonValue::makeArray();
    for (const auto& s : samples)
        arr.push(s.toJson());
    root["samples"] = arr;
    root.saveFile(path);
}

// ── 결과 출력 ─────────────────────────────────────────────────
static void printResult(const std::vector<Sample>& samples,
                        const std::string& path)
{
    const std::string EQ = rep("═", 56);
    const std::string HL = rep("─", 56);

    std::cout << "\n"
              << C::CYN << C::BOLD
              << "  ╔" << EQ << "╗\n"
              << "  ║   DummyDataGenerator  ·  더미 시료 생성 완료"
              << std::string(10, ' ') << "║\n"
              << "  ╚" << EQ << "╝\n"
              << C::RST << "\n"
              << C::GRY << "  ▸ 저장 경로  " << C::RST
              << C::WHT << path << C::RST << "\n"
              << C::GRY << "  ▸ 생성 개수  " << C::RST
              << C::CYN << C::BOLD << samples.size() << "개" << C::RST
              << "\n\n"
              << C::DIM << "  " << HL << C::RST << "\n"
              << C::YLW << C::BOLD
              << "  " << padR("ID",   4)
              << "  " << padR("이름", 22)
              << "  " << std::right << std::setw(14) << "생산시간(분)"
              << "  " << std::right << std::setw(8)  << "수율(%)"
              << C::RST << "\n"
              << C::DIM << "  " << HL << C::RST << "\n";

    for (const auto& s : samples) {
        std::cout << "  "
                  << C::CYN  << std::right << std::setw(2) << s.id << C::RST
                  << "    "
                  << C::WHT  << padR(s.name, 22) << C::RST
                  << "  " << std::right << std::setw(12) << std::fixed
                           << std::setprecision(1) << s.avg_production_time
                  << "  " << std::right << std::setw(7)  << std::fixed
                           << std::setprecision(1) << (s.yield_rate * 100.0)
                  << "\n";
    }

    std::cout << C::DIM << "  " << HL << C::RST << "\n\n"
              << C::GRN << C::BOLD << "  저장 완료!" << C::RST << "\n\n";
}

// ── main ──────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // 인수: [count=10] [output_path=../DataPersistence/data.json]
    int count = 10;
    std::string path = "..\\DataPersistence\\data.json";

    if (argc > 1) {
        count = std::atoi(argv[1]);
        if (count <= 0 || count > 1000) {
            std::cerr << "오류: 개수는 1~1000 사이여야 합니다.\n";
            return 1;
        }
    }
    if (argc > 2) path = argv[2];

    auto samples = generate(count,
                            static_cast<unsigned>(std::time(nullptr)));
    saveToDb(samples, path);
    printResult(samples, path);

    return 0;
}

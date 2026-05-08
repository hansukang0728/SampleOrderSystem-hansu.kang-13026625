#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <ctime>
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
    const char* RED  = "\033[91m";
    const char* WHT  = "\033[97m";
    const char* GRY  = "\033[90m";
    const char* MGN  = "\033[95m";
}

static const int POLL_MS = 2000;

// ── 유틸 ─────────────────────────────────────────────────────
static std::string rep(const std::string& s, int n) {
    std::string r;
    for (int i = 0; i < n; ++i) r += s;
    return r;
}

// UTF-8 문자열의 터미널 표시 폭 (한글/CJK = 2, 그 외 = 1)
static int dispWidth(const std::string& s) {
    int w = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = s[i];
        if      (c < 0x80) { w += 1; i += 1; }
        else if (c < 0xE0) { w += 1; i += 2; }
        else if (c < 0xF0) { w += 2; i += 3; }  // 한글/CJK 포함
        else                { w += 2; i += 4; }
    }
    return w;
}

// 표시 폭 기준으로 오른쪽 패딩
static std::string padR(const std::string& s, int width) {
    std::string r = s;
    int pad = width - dispWidth(s);
    if (pad > 0) r += std::string(pad, ' ');
    return r;
}

// double → 고정소수점 문자열
static std::string fmt(double v, int prec = 1) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(prec) << v;
    return o.str();
}

// ── 파일 감시 ────────────────────────────────────────────────
static FILETIME getFileWriteTime(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &info))
        return {};
    return info.ftLastWriteTime;
}

static bool sameTime(FILETIME a, FILETIME b) {
    return a.dwLowDateTime == b.dwLowDateTime &&
           a.dwHighDateTime == b.dwHighDateTime;
}

// ── 데이터 로드 ───────────────────────────────────────────────
static std::map<int, Sample> loadSamples(const std::string& path) {
    std::map<int, Sample> result;
    try {
        auto root = JsonValue::loadFile(path);
        if (!root.contains("samples")) return result;
        for (const auto& j : root["samples"].arr) {
            Sample s = Sample::fromJson(j);
            result[s.id] = s;
        }
    } catch (...) {}
    return result;
}

// ── 변경 감지 ─────────────────────────────────────────────────
struct Change {
    enum class Kind { Added, Modified, Removed } kind;
    Sample sample;
};

static std::vector<Change> diff(const std::map<int, Sample>& prev,
                                const std::map<int, Sample>& curr)
{
    std::vector<Change> out;
    for (const auto& kv : curr) {
        auto it = prev.find(kv.first);
        if (it == prev.end()) {
            out.push_back({ Change::Kind::Added, kv.second });
        } else {
            const Sample& p = it->second;
            const Sample& c = kv.second;
            if (p.name != c.name ||
                p.avg_production_time != c.avg_production_time ||
                p.yield_rate != c.yield_rate)
                out.push_back({ Change::Kind::Modified, c });
        }
    }
    for (const auto& kv : prev)
        if (curr.find(kv.first) == curr.end())
            out.push_back({ Change::Kind::Removed, kv.second });
    return out;
}

// ── 시간 문자열 ───────────────────────────────────────────────
static std::string nowStr() {
    auto t = std::chrono::system_clock::to_time_t(
                 std::chrono::system_clock::now());
    struct tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// ── 화면 렌더링 ───────────────────────────────────────────────
static void render(const std::map<int, Sample>& samples,
                   const std::vector<Change>& changes,
                   const std::string& path,
                   const std::string& updateTime)
{
    // 커서를 맨 위로 이동 후 화면 지움
    std::cout << "\033[H\033[J";

    const std::string EQ = rep("═", 58);
    const std::string HL = rep("─", 58);

    // ╔ 타이틀 ════════════════════════════════════════════════
    std::cout << C::CYN << C::BOLD
              << "  ╔" << EQ << "╗\n"
              << "  ║   DataMonitor  ·  시료 현황 실시간 감시"
              << std::string(17, ' ') << "║\n"
              << "  ╚" << EQ << "╝\n"
              << C::RST << "\n";

    // ── 시스템 정보 ──────────────────────────────────────────
    std::cout << C::GRY  << "  ▸ 감시 파일  " << C::RST
              << C::WHT  << path            << C::RST << "\n"
              << C::GRY  << "  ▸ 최종 갱신  " << C::RST
              << C::WHT  << updateTime      << C::RST
              << C::GRY  << "    ▸ 주기  "   << C::RST
              << C::WHT  << POLL_MS / 1000.0 << "초" << C::RST
              << C::GRY  << "    ▸ 시료  "   << C::RST
              << C::CYN  << C::BOLD << samples.size() << "개" << C::RST
              << "\n\n";

    // ── 테이블 헤더 ──────────────────────────────────────────
    std::cout << C::DIM << "  " << HL << C::RST << "\n";
    std::cout << C::YLW << C::BOLD
              << "  " << padR("ID",   4)
              << "  " << padR("이름", 20)
              << "  " << std::right << std::setw(14) << "생산시간(분)"
              << "  " << std::right << std::setw(8)  << "수율(%)"
              << C::RST << "\n"
              << C::DIM << "  " << HL << C::RST << "\n";

    // ── 데이터 행 ────────────────────────────────────────────
    if (samples.empty()) {
        std::cout << "\n  " << C::GRY << "(등록된 시료 없음)" << C::RST << "\n\n";
    } else {
        for (const auto& kv : samples) {
            const Sample& s = kv.second;
            std::cout << "  "
                      << C::CYN << std::right << std::setw(2) << s.id << C::RST
                      << "    "
                      << C::WHT << padR(s.name, 20) << C::RST
                      << "  " << std::right << std::setw(12)
                               << fmt(s.avg_production_time)
                      << "  " << std::right << std::setw(7)
                               << fmt(s.yield_rate * 100.0)
                      << "\n";
        }
    }
    std::cout << C::DIM << "  " << HL << C::RST << "\n";

    // ── 변경 감지 ─────────────────────────────────────────────
    std::cout << "\n";
    if (changes.empty()) {
        std::cout << "  " << C::GRY << "변경 없음" << C::RST << "\n";
    } else {
        std::cout << "  " << C::MGN << C::BOLD
                  << "[ 변경 감지 ]" << C::RST << "\n\n";
        for (const auto& c : changes) {
            const char* icon;
            const char* col;
            const char* label;
            switch (c.kind) {
            case Change::Kind::Added:
                icon = "▲"; col = C::GRN; label = "추가"; break;
            case Change::Kind::Modified:
                icon = "◆"; col = C::YLW; label = "수정"; break;
            default:
                icon = "▼"; col = C::RED; label = "삭제"; break;
            }
            std::cout << "  " << col << icon << "  " << C::BOLD << label << C::RST
                      << "  " << C::GRY << "ID:" << C::RST
                      << C::WHT << c.sample.id << C::RST
                      << "   " << C::WHT << c.sample.name << C::RST
                      << "\n";
        }
    }

    // ╔ 푸터 ══════════════════════════════════════════════════
    std::cout << "\n"
              << C::CYN << C::BOLD
              << "  ╔" << EQ << "╗\n"
              << C::RST
              << C::GRY
              << "  Ctrl+C 로 종료\n"
              << C::RST;

    std::cout.flush();
}

// ── main ──────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // ANSI 이스케이프 코드 활성화
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    std::string path = (argc > 1)
        ? argv[1]
        : "..\\DataPersistence\\data.json";

    std::cout << "DataMonitor 시작 중... 감시 파일: " << path << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::map<int, Sample> prev;
    std::vector<Change>   lastChanges;
    FILETIME prevTime{};
    bool firstRun = true;

    while (true) {
        FILETIME currTime = getFileWriteTime(path);

        if (firstRun || !sameTime(currTime, prevTime)) {
            auto curr   = loadSamples(path);
            lastChanges = firstRun ? std::vector<Change>{} : diff(prev, curr);
            prev        = curr;
            prevTime    = currTime;
            firstRun    = false;
            render(prev, lastChanges, path, nowStr());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));
    }

    return 0;
}

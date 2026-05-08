#pragma once
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <limits>

namespace UI {

// ── ANSI 색상 (constexpr → 내부 링키지, ODR 안전) ───────────
constexpr const char* RST  = "\033[0m";
constexpr const char* BOLD = "\033[1m";
constexpr const char* DIM  = "\033[2m";
constexpr const char* CYN  = "\033[96m";   // Bright Cyan  — 헤더
constexpr const char* YLW  = "\033[93m";   // Bright Yellow — 경고·대기
constexpr const char* GRN  = "\033[92m";   // Bright Green  — 성공·생산중
constexpr const char* RED  = "\033[91m";   // Bright Red    — 오류·삭제
constexpr const char* WHT  = "\033[97m";   // Bright White  — 데이터
constexpr const char* GRY  = "\033[90m";   // Dark Gray     — 유휴·레이블
constexpr const char* MGN  = "\033[95m";   // Magenta       — 강조 섹션
constexpr const char* YLW2 = "\033[33m";   // Yellow        — 수정

static const int WIDTH = 58;

// ── 유틸리티 ──────────────────────────────────────────────────

// UTF-8 문자열 터미널 표시 폭 (한글/CJK = 2, 그 외 = 1)
inline int dispWidth(const std::string& s) {
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

// 표시 폭 기준 오른쪽 패딩
inline std::string padR(const std::string& s, int width) {
    std::string r = s;
    int pad = width - dispWidth(s);
    if (pad > 0) r += std::string(pad, ' ');
    return r;
}

// 문자열 반복
inline std::string rep(const std::string& s, int n) {
    std::string r;
    for (int i = 0; i < n; ++i) r += s;
    return r;
}

// 화면 지우기
inline void clearScreen() {
    std::cout << "\033[H\033[J";
}

// ── 출력 헬퍼 ────────────────────────────────────────────────

// 박스 헤더 (Cyan + Bold)
inline void printHeader(const std::string& title) {
    const std::string EQ = rep("═", WIDTH);
    std::cout << "\n"
              << CYN << BOLD
              << "  ╔" << EQ << "╗\n"
              << "  ║   " << title << "\n"
              << "  ╚" << EQ << "╝\n"
              << RST;
}

// 구분선 (DIM, ─ WIDTH 자)
inline void printHLine() {
    std::cout << DIM << "  " << rep("─", WIDTH) << RST << "\n";
}

// 오류 메시지 (Bright Red)
inline void printError(const std::string& msg) {
    std::cout << RED << "  ✖  " << msg << RST << "\n";
}

// 성공 메시지 (Bright Green)
inline void printSuccess(const std::string& msg) {
    std::cout << GRN << "  ✔  " << msg << RST << "\n";
}

// 정보 메시지 (Gray)
inline void printInfo(const std::string& msg) {
    std::cout << GRY << "  ·  " << msg << RST << "\n";
}

// ── 입력 헬퍼 ────────────────────────────────────────────────

// 정수 입력 — 실패 시 -1 반환, EOF 시 0 반환 (종료 처리), 버퍼·개행 자동 정리
inline int readInt(const std::string& prompt) {
    std::cout << WHT << prompt << RST << std::flush;
    int v;
    if (!(std::cin >> v)) {
        if (std::cin.eof()) return 0;  // EOF → 종료로 처리
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return -1;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return v;
}

// 문자열 입력 (getline, 프롬프트 출력 후)
inline std::string readLine(const std::string& prompt) {
    std::cout << WHT << prompt << RST << std::flush;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

// double 입력 — 실패 시 -1.0 반환
inline double readDouble(const std::string& prompt) {
    std::cout << WHT << prompt << RST << std::flush;
    double v;
    if (!(std::cin >> v)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return -1.0;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return v;
}

// 계속하려면 Enter 대기
inline void waitEnter(const std::string& msg = "계속하려면 Enter 키를 누르세요...") {
    std::cout << "\n" << GRY << "  " << msg << RST;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

} // namespace UI

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include "models.h"

static void InitConsole() {
    if (!SetConsoleOutputCP(CP_UTF8) || !SetConsoleCP(CP_UTF8))
        OutputDebugStringW(L"[WARN] UTF-8 콘솔 설정 실패\n");

    // ANSI 이스케이프 코드 활성화 (색상·박스 드로잉용, best-effort)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode))
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#ifdef SOS_TEST_MODE
// ── 테스트 모드: gtest runner ─────────────────────────────────
#include <gtest/gtest.h>
int main(int argc, char** argv) {
    InitConsole();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#else
// ── 앱 모드: 메인 진입점 (Phase 6에서 메뉴 루프로 확장) ──────
int main() {
    InitConsole();
    std::cout << "SampleOrderSystem 초기화 완료\n";
    return 0;
}
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>

static void InitConsole() {
    if (!SetConsoleOutputCP(CP_UTF8) || !SetConsoleCP(CP_UTF8))
        OutputDebugStringW(L"[WARN] UTF-8 콘솔 설정 실패\n");

    // ANSI 이스케이프 코드 활성화 (색상·박스 드로잉용, best-effort)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode))
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

int main() {
    InitConsole();
    std::cout << "SampleOrderSystem 초기화 완료\n";
    return 0;
}

# Phase 4-1 — 메인 메뉴 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md)  
> Feature 문서: [docs/feature/main.md](../feature/main.md)  
> 상위 계획: **Phase 4 / 7** — 서비스·UI 레이어  
> 서브 단계: **4-1** — 메인 메뉴 루프 + ConsoleUI 공통 유틸

---

## 1. 목표

`SampleOrderSystem.cpp`의 `main()`에 메인 메뉴 루프를 구현하고,  
이후 모든 View가 공유할 `ConsoleUI` 공통 유틸리티를 `view/ConsoleUI.h`에 정의한다.

### 완료 기준
- [ ] `view/ConsoleUI.h` — ANSI 색상·테이블·dispWidth 공통 유틸 구현
- [ ] `SampleOrderSystem.cpp` — AppDB 생성 + 메인 메뉴 루프 구현
- [ ] 시스템 현황 전광판: 날짜/시간·시료수·총재고·주문건수·생산라인 상태 표시
- [ ] 메뉴 출력: 헤더(Cyan+Bold) + 7개 항목 + 선택 프롬프트
- [ ] 잘못된 입력 시 `"잘못된 선택입니다."` 출력 후 재표시
- [ ] `0` 입력 시 `"종료합니다."` 출력 후 정상 종료
- [ ] 빌드 경고 0, 오류 0 / 실행 후 메뉴 정상 출력 확인

---

## 2. 구현 대상 파일

| 파일 | 구분 | 설명 |
|---|---|---|
| `view/ConsoleUI.h` | 신규 | ANSI 색상·dispWidth·테이블 출력 공통 유틸 |
| `SampleOrderSystem.cpp` | 수정 | AppDB 생성 + 메인 메뉴 루프 |
| `SampleOrderSystem.vcxproj` | 수정 | `view/ConsoleUI.h` ClInclude 등록 |

---

## 3. ConsoleUI 설계 (`view/ConsoleUI.h`)

DataMonitor 프로젝트의 UI 패턴을 추출·정리한 공통 유틸리티.  
모든 View 헤더가 `#include "ConsoleUI.h"` 하나로 색상·정렬·출력을 공유한다.

### 3.1 ANSI 색상 상수

```cpp
namespace UI {
    // 색상
    const char* RST  = "\033[0m";
    const char* BOLD = "\033[1m";
    const char* DIM  = "\033[2m";
    const char* CYN  = "\033[96m";   // Bright Cyan  — 헤더
    const char* YLW  = "\033[93m";   // Bright Yellow — 테이블 헤더
    const char* GRN  = "\033[92m";   // Bright Green  — 성공·추가
    const char* RED  = "\033[91m";   // Bright Red    — 오류·삭제
    const char* WHT  = "\033[97m";   // Bright White  — 일반 데이터
    const char* GRY  = "\033[90m";   // Dark Gray     — 레이블
    const char* MGN  = "\033[95m";   // Magenta       — 강조 섹션
    const char* YLW2 = "\033[33m";   // Yellow        — 경고·수정
}
```

### 3.2 공통 유틸리티 함수

```cpp
namespace UI {

    // UTF-8 문자열 터미널 표시 폭 (한글/CJK = 2, 그 외 = 1)
    inline int dispWidth(const std::string& s);

    // 표시 폭 기준 오른쪽 패딩
    inline std::string padR(const std::string& s, int width);

    // 문자열 반복 (구분선·박스 드로잉용)
    inline std::string rep(const std::string& s, int n);

    // 화면 지우기 (ANSI \033[H\033[J)
    inline void clearScreen();

    // 박스 헤더 출력 (Cyan + Bold, 너비 58)
    // ╔══════════════════════════════════════════════════════════╗
    // ║   <title>
    // ╚══════════════════════════════════════════════════════════╝
    inline void printHeader(const std::string& title);

    // 구분선 출력 (DIM, ─ 58자)
    inline void printHLine();

    // 오류 메시지 출력 (Bright Red)
    inline void printError(const std::string& msg);

    // 성공 메시지 출력 (Bright Green)
    inline void printSuccess(const std::string& msg);

    // 정수 입력 — 실패 시 -1 반환, 버퍼 자동 정리
    inline int  readInt(const std::string& prompt);

    // 문자열 입력 (getline)
    inline std::string readLine(const std::string& prompt);

} // namespace UI
```

### 3.3 구현 코드

```cpp
#pragma once
#include <string>
#include <iostream>
#include <limits>

namespace UI {

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
const char* YLW2 = "\033[33m";

static const int WIDTH = 58;

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

inline std::string padR(const std::string& s, int width) {
    std::string r = s;
    int pad = width - dispWidth(s);
    if (pad > 0) r += std::string(pad, ' ');
    return r;
}

inline std::string rep(const std::string& s, int n) {
    std::string r;
    for (int i = 0; i < n; ++i) r += s;
    return r;
}

inline void clearScreen() {
    std::cout << "\033[H\033[J";
}

inline void printHeader(const std::string& title) {
    const std::string EQ = rep("═", WIDTH);
    std::cout << CYN << BOLD
              << "  ╔" << EQ << "╗\n"
              << "  ║   " << title << "\n"
              << "  ╚" << EQ << "╝\n"
              << RST;
}

inline void printHLine() {
    std::cout << DIM << "  " << rep("─", WIDTH) << RST << "\n";
}

inline void printError(const std::string& msg) {
    std::cout << RED << "  ✖ " << msg << RST << "\n";
}

inline void printSuccess(const std::string& msg) {
    std::cout << GRN << "  ✔ " << msg << RST << "\n";
}

inline int readInt(const std::string& prompt) {
    std::cout << WHT << prompt << RST;
    int v;
    if (!(std::cin >> v)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return -1;
    }
    return v;
}

inline std::string readLine(const std::string& prompt) {
    std::cout << WHT << prompt << RST;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

} // namespace UI
```

---

## 4. 시스템 현황 전광판 설계

### 4.1 표시 항목 및 데이터 출처

| 항목 | 데이터 출처 | 표시 예 |
|---|---|---|
| 📅 현재 날짜/시간 | `nowStr()` | `2026-05-08 15:30:00` |
| 🧪 등록 시료 수 | `db.samples().size()` | `5개` |
| 📦 총 재고 수량 | `sum(s.stock for s in db.samples())` | `320 ea` |
| 📋 활성 주문 건수 | REJECTED 제외 주문 수 | `3건` |
| 🏭 생산라인 상태 | 미완료 큐 항목 분석 | 아래 참고 |

### 4.2 생산라인 상태 판정 로직

```cpp
std::string productionStatus(AppDB& db) {
    // queue()는 checkAndComplete() 자동 실행
    auto& q = db.queue();
    int inProgress = 0, waiting = 0;
    double maxPct = 0.0;

    for (const auto& p : q) {
        if (p.isInProgress()) { ++inProgress; maxPct = std::max(maxPct, p.progressPct()); }
        else if (p.isWaiting()) ++waiting;
    }

    if (inProgress > 0)
        return "IN_PROGRESS (" + std::to_string((int)maxPct) + "%)";
    if (waiting > 0)
        return "WAITING (" + std::to_string(waiting) + "건)";
    return "IDLE";
}
```

**생산라인 상태 표시 규칙**

| 조건 | 표시 | 색상 |
|---|---|---|
| 미완료 큐 없음 | `유휴 (IDLE)` | `UI::GRY` |
| WAITING만 존재 | `대기 (N건)` | `UI::YLW` |
| IN_PROGRESS 존재, WAITING 없음 | `생산중 (N%)` | `UI::GRN` |
| IN_PROGRESS + WAITING 혼재 | `생산중 (N%) \| 대기 M건` | `UI::GRN` |

### 4.3 전광판 출력 예시

```
  ┌──────────────────────────────────────────────────────────┐
  │  📅 2026-05-08 15:30:00                                  │
  │  🧪 등록 시료  :   5개       📦 총 재고  :   320 ea      │
  │  📋 주문 건수  :   3건       🏭 생산라인 :  IN_PROGRESS(38%)│
  └──────────────────────────────────────────────────────────┘
```

> 전광판은 메인 메뉴가 표시될 때마다 AppDB를 재조회하여 최신 상태 반영.  
> 단, `queue()` 호출 시 `checkAndComplete()`가 자동 실행되므로  
> 생산라인 상태 조회 자체가 자동 완료 처리를 트리거한다.

---

## 5. 메인 메뉴 루프 설계 (`SampleOrderSystem.cpp`)

### 5.1 구조

```cpp
// 전광판 출력 함수
static void printStatusBoard(AppDB& db);

// 메인 메뉴 전체 출력 (전광판 포함)
static void printMainMenu(AppDB& db);

int main() {
    InitConsole();
    AppDB db("data.json");

    int choice = -1;
    while (choice != 0) {
        printMainMenu(db);                  // 전광판 + 메뉴 출력
        choice = UI::readInt("선택: ");
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        switch (choice) {
        case 1: /* 시료 관리      */ break;
        case 2: /* 시료 주문      */ break;
        case 3: /* 주문 승인/거절 */ break;
        case 4: /* 모니터링       */ break;
        case 5: /* 생산 라인      */ break;
        case 6: /* 출고 처리      */ break;
        case 7: /* 더미 데이터    */ break;
        case 0: std::cout << "\n  종료합니다.\n\n"; break;
        default: UI::printError("잘못된 선택입니다."); break;
        }
    }
    return 0;
}
```

### 5.2 printMainMenu() 전체 출력 예시

```
  ╔══════════════════════════════════════════════════════════╗
  ║   SampleOrderSystem  ·  시료 주문 관리 시스템
  ╚══════════════════════════════════════════════════════════╝

  ┌──────────────────────────────────────────────────────────┐
  │  📅 2026-05-08 15:30:00                                  │
  │  🧪 등록 시료  :   5개       📦 총 재고  :   320 ea      │
  │  📋 주문 건수  :   3건       🏭 생산라인 :  IN_PROGRESS(38%)│
  └──────────────────────────────────────────────────────────┘

    1.  시료 관리          (등록 / 조회 / 검색)
    2.  시료 주문          (주문 생성)
    3.  주문 승인 / 거절   (RESERVED 주문 처리)
    4.  모니터링           (재고 현황 / 주문 현황)
    5.  생산 라인 조회     (생산 큐 확인 / 처리)
    6.  출고 처리          (CONFIRMED 주문 출고)
    7.  더미 데이터 / 초기화
  ──────────────────────────────────────────────────────────
    0.  종료
  ──────────────────────────────────────────────────────────
선택:
```

### 5.3 UI 규칙 (feature/main.md 기준)

| 항목 | 규칙 |
|---|---|
| 헤더 | Bright Cyan + Bold, `╔══╗` 박스 |
| 메뉴 번호 | Cyan 강조 |
| 선택 프롬프트 | White |
| 잘못된 입력 | Bright Red `"잘못된 선택입니다."` + 메뉴 재표시 |
| 종료 | `"종료합니다."` 출력 후 프로그램 정상 종료 |
| 각 기능 실행 후 | Enter 키 입력 대기 → 메인 메뉴로 복귀 |

---

## 6. 입력 처리 규칙

| 입력 | 처리 |
|---|---|
| 유효한 번호 (0~7) | 해당 기능으로 분기 |
| 범위 외 번호 | `"잘못된 선택입니다."` 출력 후 재표시 |
| 숫자가 아닌 입력 | `readInt()` 내부에서 버퍼 정리 후 -1 반환 → default 분기 |
| Ctrl+C | OS 기본 처리 (별도 핸들링 없음) |

---

## 7. Phase 4-1 완료 후 파일 구조

```
SampleOrderSystem/
├── json_lite.h
├── models.h
├── app_db.h
├── view/
│   └── ConsoleUI.h          ← NEW: 공통 UI 유틸리티
├── SampleOrderSystem.cpp    ← UPDATE: 메인 메뉴 루프
├── SampleOrderSystem.vcxproj ← UPDATE: view/ConsoleUI.h 등록
└── docs/phase/
    └── phase4_1_mainmenu_design.md ← 이 파일
```

---

## 8. 의존성

```
ConsoleUI.h  ← 독립적 (표준 라이브러리만 사용)
    └── (Phase 4-2 이후 모든 View가 의존)

SampleOrderSystem.cpp
    ├── app_db.h     (Phase 3)
    ├── ConsoleUI.h  (Phase 4-1)
    └── (Phase 4-2 이후 각 View 헤더 추가)
```

---

## 9. 검증 시나리오

```
실행 → 전광판 + 메인 메뉴 출력 확인
  → 전광판: 현재 날짜/시간, 시료 수, 총 재고, 주문 건수, 생산라인 상태 표시
  → 1~7 입력: "준비 중입니다." stub 출력 후 메인 메뉴 복귀
  → 9   입력: "잘못된 선택입니다." 출력 후 메뉴 재표시
  → abc 입력: "잘못된 선택입니다." 출력 후 메뉴 재표시
  → 0   입력: "종료합니다." 출력 후 프로세스 종료
```

> Phase 4-1에서 1~7 메뉴는 stub 처리.  
> 실제 기능은 Phase 4-2(시료관리)부터 순서대로 구현.

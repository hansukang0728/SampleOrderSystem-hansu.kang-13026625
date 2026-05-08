# Phase 1 — 프로젝트 기반 설정 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md)  
> 상위 계획: **Phase 1 / 7** — 프로젝트 설정

---

## 1. 목표

SampleOrderSystem 프로젝트가 올바른 빌드 환경을 갖추고,  
이후 모든 Phase의 코드가 오류 없이 컴파일될 수 있는 기반을 확립한다.

### 완료 기준
- [ ] `SampleOrderSystem.vcxproj` — `/utf-8`, `NOMINMAX`, C++20 설정 완료
- [ ] `json_lite.h` — DataPersistence에서 복사, 프로젝트에 등록
- [ ] 초기 `SampleOrderSystem.cpp` — 콘솔 초기화 + 시작 메시지 출력
- [ ] Debug x64 빌드 경고 0, 오류 0 통과

---

## 2. 작업 상세

### 2.1 vcxproj 컴파일러 플래그 설정

현재 `SampleOrderSystem.vcxproj` 누락 항목:

| 항목 | 현재값 | 목표값 | 이유 |
|---|---|---|---|
| `/utf-8` | 없음 | 추가 | 소스 파일 UTF-8 처리, 한글 리터럴 정상 컴파일 |
| `NOMINMAX` | 없음 | 추가 | `<windows.h>` 의 `min`/`max` 매크로와 `std::numeric_limits` 충돌 방지 |
| `LanguageStandard` | `stdcpp20` | 유지 | C++20 (이미 설정됨) |

적용 대상: 4개 구성 모두 (Debug\|Win32, Release\|Win32, Debug\|x64, Release\|x64)

**변경 내용 (각 `<ClCompile>` 블록)**
```xml
<!-- 추가할 항목 -->
<PreprocessorDefinitions>...;NOMINMAX;%(PreprocessorDefinitions)</PreprocessorDefinitions>
<AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
```

---

### 2.2 json_lite.h 복사 및 등록

**출처**: `DataPersistence/DataPersistence/json_lite.h` (솔루션 루트 기준 정식 원본)  
> 솔루션 내 DataMonitor/, DummyDataGenerator/에도 복사본 존재하나 원본은 DataPersistence/DataPersistence/ 기준으로 한다.

**배치 위치**: `SampleOrderSystem/json_lite.h`

**vcxproj 등록**:
```xml
<ItemGroup>
  <ClInclude Include="json_lite.h" />
</ItemGroup>
```

**json_lite.h 제공 기능 요약**

| 기능 | 설명 |
|---|---|
| `JsonValue::parse(text)` | JSON 문자열 → JsonValue 파싱 |
| `JsonValue::loadFile(path)` | 파일 로드 (없으면 빈 Object 반환) |
| `JsonValue::saveFile(path)` | 파일 저장 |
| `JsonValue::dump(indent)` | JsonValue → 문자열 직렬화 |
| `JsonValue::makeObject()` | 빈 Object 생성 |
| `JsonValue::makeArray()` | 빈 Array 생성 |
| `obj["key"]` / `obj.push(v)` | Object/Array 접근·추가 |
| `v.asInt()` / `asDouble()` / `asString()` | 타입별 값 추출 |

---

### 2.3 초기 SampleOrderSystem.cpp 구성

Phase 1 완료 시점의 `SampleOrderSystem.cpp`:

```cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // ANSI 이스케이프 코드 활성화
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    std::cout << "SampleOrderSystem 초기화 완료\n";
    return 0;
}
```

**포함 이유**
- `SetConsoleOutputCP(CP_UTF8)` : 한글 콘솔 출력 (이후 모든 View에서 사용)
- `ENABLE_VIRTUAL_TERMINAL_PROCESSING` : ANSI 색상·박스 드로잉 활성화 (ConsoleUI에서 사용)
- `WIN32_LEAN_AND_MEAN` : `<windows.h>` 불필요한 헤더 제외

---

## 3. Phase 1 완료 후 파일 구조

```
SampleOrderSystem/
├── CLAUDE.md
├── PRD.md
├── plan.md
├── json_lite.h                  ← NEW: DataPersistence에서 복사
├── packages.config              ← 기존 파일 (변경 없음)
├── SampleOrderSystem.cpp        ← UPDATE: 콘솔 초기화 코드
├── SampleOrderSystem.vcxproj    ← UPDATE: /utf-8, NOMINMAX 추가
├── SampleOrderSystem.vcxproj.filters  ← UPDATE: json_lite.h 등록
└── docs/
    ├── phase/
    │   └── phase1_design.md     ← NEW: 이 파일
    ├── feature/
    │   └── (기존 7개 문서)
    └── solution-projects.md
```

---

## 4. 의존성

```
Phase 1 (기반 설정)
    └── Phase 2 (models.h) 가 의존
            └── Phase 3 (app_db.h) 가 의존
                    └── Phase 4 ~ 7
```

Phase 1이 완료되지 않으면 이후 모든 Phase의 빌드가 불가하다.

---

## 5. 검증 방법

```
MSBuild CRAProject.slnx /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

기대 결과:
```
SampleOrderSystem.cpp
SampleOrderSystem.vcxproj -> ...\x64\Debug\SampleOrderSystem.exe
```
- 경고(warning) 0건
- 오류(error) 0건
- 실행 시 `"SampleOrderSystem 초기화 완료"` 출력

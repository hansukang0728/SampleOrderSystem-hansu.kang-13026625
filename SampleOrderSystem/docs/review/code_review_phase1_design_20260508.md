# Design Review — phase1_design.md

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `docs/phase/phase1_design.md`  
**리뷰어**: design-doc-reviewer agent  
**종합 평가**: ⚠️ Needs Minor Revisions

---

## 🔴 Critical Issues

1. **gmock NuGet 의존성 빌드 차단 가능성**  
   DataPersistence·DataMonitor·DummyDataGenerator의 `.vcxproj`에 `gmock.1.11.0` NuGet 참조가 추가되어 있으며, `..\packages\gmock.1.11.0\build\native\gmock.targets` 파일이 없으면 `<Error>` 태스크가 솔루션 전체 빌드를 강제 실패시킨다. Phase 1 완료 기준 "경고 0, 오류 0"에 영향을 줄 수 있으므로 솔루션 빌드 전 확인이 필요하다. SampleOrderSystem.vcxproj 자체에는 해당 참조가 없다.

## 조치 결과
- [x] SampleOrderSystem.vcxproj에 gmock 참조 없음 확인 — 해당 없음
- [ ] 솔루션 전체 빌드 시 타 프로젝트 gmock 오류 발생 여부 빌드 후 확인

---

## 🟠 Major Issues

1. **json_lite.h 출처 경로 표기 불명확**  
   솔루션 내 `DataPersistence/`, `DataMonitor/`, `DummyDataGenerator/` 3곳에 복사본 존재. 단일 출처(`DataPersistence/DataPersistence/json_lite.h`)를 명시해야 혼란 없음.

2. **C++ 표준 버전 불일치**  
   CLAUDE.md 섹션 6에 "C++17 이상"으로 명시되어 있으나, vcxproj는 `stdcpp20`. phase1_design.md는 `stdcpp20` 유지 방향. CLAUDE.md를 C++20으로 통일 필요.

## 조치 결과
- [x] json_lite.h 출처 경로 phase1_design.md에서 수정
- [x] CLAUDE.md C++17 → C++20 으로 수정

---

## 🟡 Minor Issues

1. 완료 기준이 Debug\|x64만 언급, 4개 구성 전체 적용 명시 부족
2. `SetConsoleMode` 실패 시 처리 방침 미명시 (best-effort로 처리)
3. 파일 구조 트리에 `packages.config` 미포함
4. `json_lite.h` vcxproj 등록 시 `.filters` 파일 업데이트 항목 미언급

## 조치 결과
- [x] phase1_design.md 파일 구조에 packages.config 추가
- [x] .filters 업데이트 항목 작업 목록에 추가

---

## 💡 Suggestions

1. `CRAProject.slnx` 등록 확인 항목을 phase1_design.md에도 "(완료)" 상태로 명시
2. 5절 검증 방법에 `.exe` 실행 및 출력 확인 명령어 추가
3. `WIN32_LEAN_AND_MEAN`을 vcxproj PreprocessorDefinitions에 추가 고려

---

## ✅ Strengths

- 목표가 단일하고 명확 ("빌드 환경 확립")
- 플래그 추가 이유를 표에 명기
- json_lite.h API 요약표 포함
- vcxproj XML 변경 스니펫 직접 제시
- 완료 기준이 측정 가능한 체크박스 형식

---

## ❓ Open Questions

1. 솔루션 전체 빌드 시 타 프로젝트 gmock 오류가 SampleOrderSystem 빌드에 영향을 주는가?
2. `/WX` (경고를 오류로 처리) 플래그 적용 여부?
3. `SetConsoleMode` 실패가 Phase 1 완료 기준에 포함되는가?

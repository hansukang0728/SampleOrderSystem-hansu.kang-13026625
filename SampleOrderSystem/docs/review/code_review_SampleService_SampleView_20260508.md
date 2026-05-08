# Code Review — SampleService.h + SampleView.h (Phase 4-2)

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `MVC/Service/SampleService.h`, `MVC/View/SampleView.h`  
**리뷰어**: clean-code-reviewer agent

---

## 종합 평가
설계 명세와 정합성이 완벽하고 MVC 책임 분리가 명확하게 지켜졌다. 44개 테스트 전체 통과. 전반적으로 완성도가 높다.

---

## Critical 🔴
없음

---

## Major 🟡

1. **`searchByName` 포인터 무효화 위험** — `add()` 후 벡터 재할당 시 반환 포인터 무효화 가능. 현재 즉시 소비하므로 실제 위험 낮음. 주석으로 경고 명시.

2. **`handleAdd()` UX** — 유효성 실패 시 처음부터 재입력. 각 필드별 재입력 루프로 개선.

## 조치 결과
- [x] 각 필드별 `while(true)` 재입력 루프 적용
- [x] `static_cast<int>` 통일 (C-style 캐스트 제거)

---

## Minor 🟢

1. C-style `(int)` 캐스트 → `static_cast<int>` ✅ 적용
2. `totalPg` 루프 내 stale 가능성 — 현재 루프 내 add() 경로 없으므로 무해
3. `showMenu()` 미분리 — 기능 상 무관, 설계 문서와 경미한 불일치
4. `padR("이름", 22)` 매직 넘버

---

## 긍정 사항 ✅
- 설계 명세 100% 정합
- `const` 정확성 완벽 (all/searchByName/totalPages/printTable/printSample)
- 페이지네이션 경계 조건 처리 (p/n 버튼 조건부 표시)
- `SampleView → SampleService → AppDB` 단방향 의존성 준수
- 책임 분리 철저 (Service는 bool, View가 오류 메시지 출력)

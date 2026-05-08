# Code Review — app_db.h + tests/app_db_test.cpp (Phase 3)

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `app_db.h`, `tests/app_db_test.cpp`  
**리뷰어**: clean-code-reviewer agent

---

## 종합 평가
Phase 3 설계 문서를 충실히 이행했고, 인터페이스·ID 생성·영속성·자동 완료 로직이 명세와 일치한다. `samples()`/`orders()` non-const 노출로 인한 캡슐화 문제와 테스트 누락 항목이 있었으나 즉시 반영했다.

---

## Critical 🔴
없음

---

## Major 🟡

1. **`samples()` / `orders()` non-const 참조 노출** — write-through 원칙 우회 가능
2. **TC-DB-01에 `queue().empty()` 검사 누락** — 3개 컬렉션 중 하나 미검증
3. **`checkAndComplete()`에서 완료 항목 재순회** — `isDone()` 명시적 가드 누락

## 조치 결과
- [x] `samples()` / `orders()` → `const` 반환으로 변경
- [x] TC-DB-01에 `EXPECT_TRUE(db.queue().empty())` 추가
- [x] `checkAndComplete()` 루프에 `if (p.isDone() || !p.isTimeElapsed()) continue` 적용
- [x] TC-DB-06에 `yield_rate` 검증 추가

---

## Minor 🟢

1. 완료 항목 재순회 가독성 → `isDone()` 가드로 해결 ✅
2. `nextOrderId()` 크기 가드 — 이미 구현에 반영됨
3. 테스트 파일 경로 하드코딩 — 현재 수준에서 허용
4. TC-DB-06 `yield_rate` 검증 누락 → 추가 ✅

---

## 긍정 사항 ✅
- 설계 문서 준수율 100%
- `checkAndComplete()` nullptr 방어 처리
- `queue()` / `frontWaiting()` 자동 체크 설계
- max 기반 ID 생성 (count 기반 충돌 방지)
- SetUp/TearDown 양방향 파일 격리
- 한국어 주석의 적절한 밀도

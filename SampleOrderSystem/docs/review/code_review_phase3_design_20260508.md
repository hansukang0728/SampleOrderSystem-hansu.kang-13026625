# Design Review — phase3_design.md

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `docs/phase/phase3_design.md`  
**리뷰어**: design-doc-reviewer agent  
**종합 평가**: ⚠️ Minor Revisions

---

## 🔴 Critical Issues
없음

---

## 🟠 Major Issues

1. **`queue()` 자동 `checkAndComplete()` 부작용 미명세** — 반환 참조/포인터 안정성 조건 필요
2. **`nextOrderId()` count 기반 충돌 가능** — 삭제 후 재생성 시 ID 충돌 → `max(seq)+1` 방식으로 변경
3. **WAITING 항목이 `checkAndComplete()` 대상이 아님 미명시** — Phase 4 `processNext()` 의존 관계 누락

## 조치 결과
- [x] sec 5.2 `nextOrderId()` → max(seq)+1 로직으로 수정
- [x] sec 6.2 WAITING 항목 건너뜀 명시, Phase 4 의존 관계 기술
- [x] sec 11 `queue()` 반환 참조 안정성 조건 추가

---

## 🟡 Minor Issues

1. `frontPending()` vs `frontWaiting()` plan.md와 명칭 불일치
2. `nextQueueId_` 멤버 변수 제거 → const 메서드로 교체 (plan.md 불일치)
3. gtest Fixture `SetUp()` 미구현으로 잔여 파일 처리 불완전
4. TC-DB-14 `started_at` 수동 설정 패턴 취약성
5. `updateSample/Order/QueueItem` "전체 교체" 동작 미명시

## 조치 결과
- [x] sec 4.1 주석으로 plan.md 변경 이유 명시
- [x] sec 8.1 `SetUp()` 추가
- [x] sec 11 update 메서드 "전체 교체" 동작 명시

---

## ✅ Strengths
- Write-through 원칙 일관성
- data.json 구조와 도메인 모델 완전 정합
- ID 생성 빈 컬렉션 엣지 케이스 처리
- `checkAndComplete()` nullptr 안전 처리
- gtest 15개 케이스가 핵심 기능 망라
- CLAUDE.md Phase 3 검증 레벨 충족

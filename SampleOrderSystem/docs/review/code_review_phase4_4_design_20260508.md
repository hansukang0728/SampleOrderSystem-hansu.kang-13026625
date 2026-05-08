# Design Review — phase4_4_ordermanager_design.md

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `docs/phase/phase4_4_ordermanager_design.md`  
**리뷰어**: design-doc-reviewer agent  
**종합 평가**: ⚠️ Minor Revisions

---

## 🔴 Critical Issues
없음

---

## 🟠 Major Issues

1. **Case 2에서 기존 재고(stock) 차감 누락** — `stock = 0` 명세 및 테스트 EXPECT 누락
2. **OrderManager.md 총 생산시간 예시 오류** — `45.0분 × 57` → `0.5분 × 57 = 28.5분`

## 조치 결과
- [x] 섹션 3.2 Case 2에 `stock = 0` 명시
- [x] TC-OM-02·TC-OM-03에 `stock == 0` 검증 추가
- [x] OrderManager.md 3.2절 예시 수정

---

## 🟡 Minor Issues

1. `std::ceil()` → `int` 캐스팅에 `static_cast<int>` 미명시
2. TC-OM-02와 TC-OM-10 중복
3. SetUp 시료 `avg_production_time` 단일값(0.5) — total_time 다양성 부족
4. S9 시나리오 구현 전략 미명세

## 조치 결과
- [x] static_cast<int>(std::ceil(...)) 명시
- [x] TC-OM-10을 다른 yield_rate 경계값 테스트로 교체
- [x] 베타-시료 avg_production_time = 1.5로 변경하여 공식 다양성 확보
- [x] S9 시간 시뮬레이션 전략 명시

# Design Review — phase4_5_monitoring_design.md

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `docs/phase/phase4_5_monitoring_design.md`  
**리뷰어**: design-doc-reviewer agent  
**종합 평가**: ⚠️ Minor Revisions

---

## 🔴 Critical Issues

1. **DashboardData 구조체 인터페이스 불일치** — 참조 필드 주석만 있고 실제 미선언

## 조치 결과
- [x] DashboardData에 재고 목록·큐 항목 미포함 의도 명시
- [x] render() 흐름: collect() → renderXxx(d) 로 명확화

---

## 🟠 Major Issues

1. **render() 내부 collect() 1회 원칙과 하위 render 메서드 불일치**
2. **TC-MN-05 pendingQueue() 첫 호출 의도 불명확**
3. **pendingQueue() 단독 호출 시 자동완료 미처리 미방어**

## 조치 결과
- [x] render() 흐름 의사코드로 명세 (collect() → 스냅샷 확보 → render)
- [x] TC-MN-05 첫 pendingQueue() 호출 제거 (불필요)
- [x] pendingQueue() API 전제조건 강화 명시

---

## 🟡 Minor Issues

1. TC-MN-08 검증 불충분 — 실제 시각값 비교 추가
2. PRODUCING 상태 집계 TC 누락
3. stockColor() 테스트 누락
4. Monitoring.md [m] 메인 옵션 미반영

## 조치 결과
- [x] TC-MN-08 EXPECT_EQ("2026-05-08 11:00", result) 추가
- [x] TC-MN-09 PRODUCING 집계 케이스 추가
- [x] Monitoring.md [m] 메인 추가

# Design Review — phase4_6_productionline_design.md

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `docs/phase/phase4_6_productionline_design.md`  
**리뷰어**: design-doc-reviewer agent  
**종합 평가**: ⚠️ Minor Revisions

---

## 🔴 Critical Issues
없음

---

## 🟠 Major Issues

1. **FlushConsoleInputBuffer 위치 누락** — 섹션 4.2 코드에 없고 섹션 9에만 언급
2. **checkAndComplete() 이중 호출** — render() 직접 호출 + inProgressItem() 내부 호출

## 조치 결과
- [x] 섹션 4.2에 FlushConsoleInputBuffer 추가
- [x] 이중 호출 정책 명확화: render()에서만 1회, inProgressItem()은 단순 조회

---

## 🟡 Minor Issues

1. TC-PS-06 "≈ 0%" 단정 표현 — total_time 극소값 케이스 미고려
2. estimatedCompletion 포맷 불일치 (feature 19자 vs design 16자)
3. waitingQueue() 정렬 기준 서비스 명세 미기재
4. processNext() IN_PROGRESS 중복 방지 가드 미정의

## 조치 결과
- [x] TC-PS-06 범위 검사 유지, "≈ 0%" 표현 완화
- [x] ProductionLine.md 예상완료 포맷 16자로 통일
- [x] waitingQueue() 정렬 기준 명시 (enqueued_at 오름차순)
- [x] processNext() 가드 조건 추가 (IN_PROGRESS 존재 시 false 반환)

---

## ✅ Strengths
- progressPct() [0,100] 클램핑으로 currentProduction() 안전성 보장
- total_time × 60 단위 변환 정확
- parseTime 실패 시 "-" 폴백 일관
- WaitForSingleObject 논블로킹 방식 적합
- TC-PS-09 자동완료 시뮬레이션 실용적

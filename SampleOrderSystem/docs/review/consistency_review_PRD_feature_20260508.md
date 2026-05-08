# Cross-Document Consistency Review — PRD ↔ Feature 문서

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: PRD.md ↔ feature 문서 7종 ↔ phase2_design.md  
**리뷰어**: design-doc-reviewer agent  
**종합 평가**: ⚠️ Minor Inconsistencies

---

## 🔴 Critical Issues
없음

---

## 🟠 Major Issues

1. **ProductionLine.md — started_at / 3단계 생산 상태 미반영**  
   phase2_design.md의 WAITING→IN_PROGRESS→DONE 2단계 처리 모델이 반영되지 않음.  
   processNext()가 즉시 완료 처리하는 단일 모델로 기술되어 있어 충돌.

2. **Monitoring.md — started_at / 진행률(%) 미반영**  
   생산 큐 현황 테이블에 상태, 진행률, 예상 완료 시각 컬럼 없음.

3. **CLAUDE.md 섹션 2.3 — started_at 필드 누락**  
   ProductionQueueItem 필드 정의에 started_at(9번째 필드) 미기재.

## 조치 결과
- [x] ProductionLine.md 업데이트 (3단계 흐름, 진행률 컬럼)
- [x] Monitoring.md 업데이트 (진행률·상태·예상완료 컬럼, 자동 완료 명시)
- [x] CLAUDE.md 섹션 2.3 업데이트 (started_at 필드, 3단계 상태 표)

---

## 🟡 Minor Issues

4. **O-05 주문 목록 조회** — feature 문서에 독립 명세 없음 (Monitoring M-02가 부분 대응)
5. **S-03 ID 조회** — sampleManage.md 서브메뉴 및 기능 상세 누락
6. **OrderManager.md §3.1 PRD ID 오류** — "접수된 주문 목록 조회 (O-02)" → O-02는 승인 기능
7. **phase2_design.md 섹션 번호 역전** — 6.5 → 6.4 순서 오류
8. **D-01~D-03 더미 데이터** — feature 문서 및 main.md 메뉴에 없음

## 조치 결과
- [x] sampleManage.md S-03 추가 (메뉴 3번, 기능 상세 3.3절)
- [x] OrderManager.md §3.1 ID 레이블 수정 (O-02 → O-05 부분 대응)
- [x] phase2_design.md 섹션 번호 수정 (6.5→6.4)
- [x] main.md 더미 데이터 메뉴 7번 추가
- [x] docs/feature/dummyData.md 신규 작성 (D-01~D-03)

---

## ✅ PRD Coverage Check

| PRD ID | 기능 | 반영 문서 | 반영 여부 |
|---|---|---|---|
| S-01 | 시료 등록 | sampleManage.md §3.1 | ✅ |
| S-02 | 전체 조회 | sampleManage.md §3.2 | ✅ |
| S-03 | ID 조회 | (없음) | ❌ 누락 |
| S-04 | 이름 검색 | sampleManage.md §3.3 | ✅ |
| S-05 | 재고 상태 표시 | sampleManage.md §3.2, Monitoring.md §3.2 | ✅ |
| O-01 | 주문 생성 | sampleOrder.md | ✅ |
| O-02 | 주문 승인 | OrderManager.md §3.2 | ✅ |
| O-02a | 재고 충분 분기 | OrderManager.md §3.2 Case 1 | ✅ |
| O-02b | 재고 부족 분기 | OrderManager.md §3.2 Case 2 | ✅ |
| O-03 | 주문 거절 | OrderManager.md §3.3 | ✅ |
| O-04 | 주문 출고 | Release.md | ✅ |
| O-05 | 주문 목록 조회 (전체, 필터) | (Monitoring M-02 부분 대응) | ⚠️ 부분 누락 |
| P-01 | 생산 큐 조회 | ProductionLine.md §3.1 | ✅ |
| P-02 | 다음 작업 처리 | ProductionLine.md §3.2 | ⚠️ phase2_design과 불일치 |
| P-03 | 생산량 계산 | ProductionLine.md §4 | ✅ |
| P-04 | 생산시간 계산 | ProductionLine.md §4 | ✅ |
| M-01 | 재고 현황 대시보드 | Monitoring.md §3.2 | ✅ |
| M-02 | 활성 주문 현황 | Monitoring.md §3.1 | ✅ |
| M-03 | 생산 큐 현황 | Monitoring.md §3.3 | ⚠️ 진행률 미반영 |
| D-01 | 시료 더미 생성 | (없음) | ❌ 누락 |
| D-02 | 주문 더미 생성 | (없음) | ❌ 누락 (선택) |
| D-03 | 데이터 초기화 | (없음) | ❌ 누락 (선택) |

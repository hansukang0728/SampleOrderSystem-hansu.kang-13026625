# Phase 7 — 전체 흐름 검증 (Verification Design Document)

> 참조: [plan.md](../../plan.md) · [PRD.md](../../PRD.md)  
> 상위 계획: **Phase 7 / 7** — 전체 흐름 시나리오 검증

---

## 1. 목표

PRD.md의 완료 기준(Definition of Done) 7개 항목을 gtest 시나리오 테스트로 검증한다.  
Phase 2~4-7에 걸쳐 구현된 전체 기능이 통합적으로 정상 동작함을 확인한다.

---

## 2. PRD DoD 체크리스트 → 검증 시나리오 매핑

| DoD 항목 | 담당 시나리오 |
|---|---|
| 시료 CRUD 및 재고 상태 표시 | FS-01, FS-05 |
| 주문 생성 → 승인(충분/부족 분기) → 거절 | FS-02, FS-03, FS-04 |
| 생산 큐 FIFO 처리 및 자동 갱신 | FS-04 |
| 출고(RELEASE) 처리 | FS-02, FS-03 |
| 프로그램 재실행 후 데이터 유지 | FS-06 |
| 더미 데이터 생성 기능 | FS-07 |
| 콘솔 한글 출력 정상 | 빌드 성공 + 앱 실행으로 확인 |

---

## 3. 검증 시나리오 상세

### FS-01: 재고 충분 전체 플로우
```
시료 등록 (stock=100)
  → 주문 생성 → RESERVED
  → 승인 → 재고 충분 → CONFIRMED, stock 차감
  → 출고 → RELEASE
  → 검증: 상태, 재고, 영속성
```

### FS-02: 재고 부족 전체 플로우 (생산 포함)
```
시료 등록 (stock=0)
  → 주문 생성 → RESERVED
  → 승인 → 재고 부족 → PRODUCING, 생산 큐 등록 + 자동 시작
  → 생산 완료 시뮬레이션 → CONFIRMED, stock += actual_qty
  → 출고 → RELEASE
  → 검증: 모든 상태 전이, 재고 변화
```

### FS-03: 주문 거절 플로우
```
시료 등록 (stock=50)
  → 주문 생성 → RESERVED
  → 거절 → REJECTED
  → 검증: 재고 불변, REJECTED 상태
  → REJECTED 주문 출고 시도 → 실패
```

### FS-04: FIFO 생산 큐 전체 플로우
```
시료 등록 (stock=0)
  → 주문 3건 생성 및 승인
  → 큐 상태: 1번 IN_PROGRESS, 2·3번 WAITING
  → 1번 생산 완료 → 2번 자동 IN_PROGRESS
  → 2번 생산 완료 → 3번 자동 IN_PROGRESS
  → 3번 생산 완료 → 전체 CONFIRMED
  → FIFO 순서 검증, 재고 누적 증가 검증
```

### FS-05: 재고 상태 계산 (주문 수요 대비)
```
시료 등록 (stock=5)
  → RESERVED 수요=3   → 여유 (5 >= 3)
  → RESERVED 수요=10  → 부족 (5 < 10)
  → stock=0           → 고갈
  → stock=0, 생산 중=10, 수요=8 → 여유 (0+10 >= 8)
  → stock=0, 생산 중=5, 수요=8  → 부족 (0+5 < 8)
```

### FS-06: 완전 영속성 검증
```
[라운드 1] 시료/주문/생산/출고 전체 실행
  → AppDB 재시작
[라운드 2] 모든 상태 동일하게 유지 확인
  → CONFIRMED 주문 출고 가능
  → RELEASE 상태 주문 재출고 불가
  → 완료된 생산 큐 항목 completed=true 유지
```

### FS-07: 더미 데이터 생성 플로우
```
DummyDataService.generateSamples(10) → 10개 시료, S-001~S-010
DummyDataService.generateOrders(5)   → 5건 주문, RESERVED
resetAll() → 빈 상태
재생성      → S-001부터 다시 시작
```

---

## 4. 검증 파일

| 파일 | 내용 |
|---|---|
| `tests/phase7_full_flow_test.cpp` | FS-01 ~ FS-07 (7개 시나리오, 총 ~30개 EXPECT) |

---

## 5. 완료 기준

모든 FS 시나리오 PASS + PRD.md DoD 7개 항목 `[x]` 처리

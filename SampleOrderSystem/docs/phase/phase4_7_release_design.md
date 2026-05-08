# Phase 4-7 — 출고 처리 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md)  
> Feature 문서: [docs/feature/Release.md](../feature/Release.md)  
> 상위 계획: **Phase 4 / 7** — 서비스·UI 레이어  
> 서브 단계: **4-7** — 출고 처리 (O-04)

---

## 1. 목표

CONFIRMED 상태의 주문 목록을 표시하고, 번호 선택으로 출고(RELEASE) 처리한다.  
`OrderService`에 `releaseOrder()` · `confirmedOrders()`를 추가하고 `ReleaseView`로 UI를 구현한다.

### 완료 기준
- [ ] `OrderService` — `releaseOrder()` · `confirmedOrders()` 추가
- [ ] `ReleaseView.h` — CONFIRMED 목록 (5개/페이지) + 번호 선택 출고
- [ ] CONFIRMED → RELEASE 전환, 재고 변화 없음
- [ ] 메인 메뉴 6번에 `ReleaseView` 연결
- [ ] gtest — `ReleaseTest` 전체 통과
- [ ] 빌드 경고 0, 오류 0

---

## 2. 구현 대상 파일

| 파일 | 위치 | 구분 |
|---|---|---|
| `OrderService.h` | `MVC/Service/` | 수정 (2개 메서드 추가) |
| `ReleaseView.h` | `MVC/View/` | 신규 |
| `SampleOrderSystem.cpp` | `SampleOrderSystem/` | 수정 (6번 연결) |
| `tests/release_test.cpp` | `SampleOrderSystem/tests/` | 신규 |

---

## 3. OrderService 확장

```cpp
// CONFIRMED 주문 목록
std::vector<const Order*> confirmedOrders() const;

// O-04: 출고 처리 CONFIRMED → RELEASE
// 반환: true(성공), false(주문 없음 또는 CONFIRMED 아님)
bool releaseOrder(const std::string& orderId);
```

**releaseOrder() 구현**
```
① findOrder(orderId) → nullptr이면 false
② CONFIRMED 상태 확인 → 아니면 false
③ order.status = RELEASE
④ updateOrder()
⑤ true 반환
```

> 재고 변화 없음 — 승인 시 이미 차감됨

---

## 4. ReleaseView 설계

### 4.1 인터페이스

```cpp
class ReleaseView {
public:
    static const int PAGE_SIZE = 5;

    ReleaseView(OrderService& orderSvc, SampleService& sampleSvc)
        : orderSvc_(orderSvc), sampleSvc_(sampleSvc) {}
    void run();

private:
    void printPage(const std::vector<const Order*>& confirmed,
                   int page, int totalPages) const;
    bool handleSelected(const Order& order);  // true: 처리 완료, false: 취소

    OrderService&  orderSvc_;
    SampleService& sampleSvc_;
};
```

### 4.2 메인 루프 흐름

```
run() 진입
  → confirmedOrders() 로드
  → 비어있으면 "출고 대기 중인 주문이 없습니다." → 복귀
  → 페이지 표시 루프:
       printPage() 출력
       입력:
         "1"~"5"  → 해당 주문 선택 → handleSelected()
                    → 처리 후 목록 갱신
         "n"/"p"  → 페이지 이동
         "0"      → 복귀
         "m"      → 메인
```

### 4.3 화면 출력 예시

```
  ╔══════════════════════════════════════════════════════════╗
  ║   출고 처리
  ╚══════════════════════════════════════════════════════════╝

  ──────────────────────────────────────────────────────────
  #   주문번호              시료     고객명          수량
  ──────────────────────────────────────────────────────────
  1   ORD-20260508-0001   S-001   삼성전자        30 ea
  2   ORD-20260508-0002   S-002   SK하이닉스      50 ea
  ──────────────────────────────────────────────────────────
  총 2건 (CONFIRMED) | 1 / 1 페이지

  [1~2] 선택   [0] 뒤로   [m] 메인
```

선택 후 확인:
```
  선택된 주문: ORD-20260508-0001 | 삼성전자 | S-001 | 30ea

  주문 상태 : CONFIRMED → RELEASE
  ✔  출고 처리 완료.
```

---

## 5. 메인 메뉴 연결

```cpp
ReleaseView releaseView(orderSvc, sampleSvc);
case 6: releaseView.run(); UI::goToMain = false; break;
```

---

## 6. gtest 계획 (`tests/release_test.cpp`)

```cpp
// TC-RL-01: releaseOrder — CONFIRMED → RELEASE
// TC-RL-02: releaseOrder — 존재하지 않는 주문 → false
// TC-RL-03: releaseOrder — CONFIRMED 아닌 주문 → false (RESERVED/REJECTED)
// TC-RL-04: confirmedOrders — CONFIRMED만 필터링
// TC-RL-05: 영속성 — 재로드 후 RELEASE 상태 유지
// TC-RL-06: releaseOrder — 재고 변화 없음
```

**총 6개 테스트 케이스 (ReleaseTest)**

---

## 7. 의존성

```
AppDB → OrderService (Phase 4-4 확장)
              └── ReleaseView → SampleService (시료명 표시)
```

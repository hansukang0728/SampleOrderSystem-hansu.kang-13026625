# Phase 4-4 — 주문 승인 / 거절 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md)  
> Feature 문서: [docs/feature/OrderManager.md](../feature/OrderManager.md)  
> 상위 계획: **Phase 4 / 7** — 서비스·UI 레이어  
> 서브 단계: **4-4** — 주문 승인/거절 (O-02, O-02a, O-02b, O-03)

---

## 1. 목표

RESERVED 주문에 대해 **승인**(재고 분기 자동 처리) 또는 **거절**을 구현한다.  
Phase 4-4에서 `OrderService`에 `approveOrder()` · `rejectOrder()` · `reservedOrders()`를 추가하고,  
`OrderManagerView`로 UI를 분리한다.

### 완료 기준
- [ ] `OrderService` — `approveOrder()` · `rejectOrder()` · `reservedOrders()` 추가
- [ ] `OrderManagerView.h` — 진입 즉시 RESERVED 목록 (5개/페이지), 번호 선택 → a/r/0 처리
- [ ] Case 1 (재고 충분): `stock -= quantity` → CONFIRMED
- [ ] Case 2 (재고 부족): 부족분 계산 → 생산 큐 등록 → PRODUCING
- [ ] 거절: RESERVED → REJECTED, 재고 변화 없음
- [ ] 메인 메뉴 3번에 `OrderManagerView` 연결
- [ ] gtest — `OrderManagerTest` 전체 통과
- [ ] 시나리오 테스트 S8 · S9 · S10 추가
- [ ] 빌드 경고 0, 오류 0

---

## 2. 구현 대상 파일

| 파일 | 위치 | 구분 |
|---|---|---|
| `OrderService.h` | `MVC/Service/` | 수정 (3개 메서드 추가) |
| `OrderManagerView.h` | `MVC/View/` | 신규 |
| `SampleOrderSystem.cpp` | `SampleOrderSystem/` | 수정 (3번 메뉴 연결) |
| `tests/order_manager_test.cpp` | `SampleOrderSystem/tests/` | 신규 |
| `tests/scenario_test.cpp` | `SampleOrderSystem/tests/` | 수정 (S8·S9·S10 추가) |

---

## 3. OrderService 확장

### 3.1 추가 메서드

```cpp
// RESERVED 주문 목록
std::vector<const Order*> reservedOrders() const;

// O-02: 주문 승인 — 재고 상황에 따라 자동 분기
// 반환값: true(성공), false(주문 없음 또는 RESERVED 아님)
struct ApproveResult {
    bool    success    = false;
    bool    sufficient = false;  // true: 재고 충분, false: 재고 부족
    int     shortage   = 0;
    int     actualQty  = 0;
    double  totalTime  = 0.0;
};
ApproveResult approveOrder(const std::string& orderId);

// O-03: 주문 거절
// 반환값: true(성공), false(주문 없음 또는 RESERVED 아님)
bool rejectOrder(const std::string& orderId);
```

### 3.2 approveOrder() 핵심 비즈니스 로직

```
① 주문 조회 → 없으면 false
② RESERVED 상태 확인 → 아니면 false
③ 연결된 시료 조회
④ 재고 비교:

   [Case 1] stock >= quantity
     · stock -= quantity
     · order.status = CONFIRMED
     · result.sufficient = true

   [Case 2] stock < quantity
     · shortage   = quantity - stock
     · stock      = 0          ← 기존 재고 전량 소진 (생산 완료 후 actual_qty 입고)
     · actual_qty = static_cast<int>(std::ceil(shortage / (yield_rate × 0.9)))
     · total_time = avg_production_time × actual_qty
     · db_.enqueue(orderId, sampleId, shortage, actual_qty, total_time)
     · order.status = PRODUCING
     · result.sufficient = false
     · result.shortage / actualQty / totalTime 기록

⑤ db_.updateSample() + db_.updateOrder() + save
⑥ result.success = true 반환
```

**생산 계산 공식 (CLAUDE.md 섹션 3)**
```
actual_qty = ceil(shortage / (yield_rate × 0.9))
total_time = avg_production_time × actual_qty
```

> `<cmath>` 의 `std::ceil()` 사용.  
> `shortage`, `actual_qty` 는 `int`; 나눗셈은 `double`로 수행 후 ceil → int 캐스팅.

### 3.3 reservedOrders() 구현

```cpp
std::vector<const Order*> reservedOrders() const {
    std::vector<const Order*> result;
    for (const auto& o : db_.orders())
        if (o.status == OrderStatus::RESERVED)
            result.push_back(&o);
    return result;
}
```

---

## 4. OrderManagerView 설계 (`MVC/View/OrderManagerView.h`)

### 4.1 인터페이스

```cpp
class OrderManagerView {
public:
    static const int PAGE_SIZE = 5;  // 페이지당 5개

    OrderManagerView(OrderService& orderSvc, SampleService& sampleSvc)
        : orderSvc_(orderSvc), sampleSvc_(sampleSvc) {}

    void run();  // 진입 즉시 RESERVED 목록 표시 + 선택 처리 루프

private:
    void printPage(const std::vector<const Order*>& reserved,
                   int page, int totalPages) const;
    void handleSelected(const Order& order);  // 선택된 주문: a/r/0 처리
    void showApproveResult(const OrderService::ApproveResult& r,
                           const Order& order) const;

    OrderService&  orderSvc_;
    SampleService& sampleSvc_;
};
```

### 4.2 메인 루프 흐름

```
run() 진입
  ↓
RESERVED 목록 로드 (reservedOrders())
  │
  ├─ 비어있으면 → "처리 대기 중인 주문이 없습니다." → 복귀
  │
  └─ 목록 있으면 → 페이지 표시 루프:
       printPage() 출력
       입력 처리:
         "1"~"5"  → 현재 페이지의 해당 주문 선택 → handleSelected()
                    → 처리 후 목록 갱신 (reservedOrders() 재호출)
         "n"      → 다음 페이지 (마지막이면 무시)
         "p"      → 이전 페이지 (첫 페이지면 무시)
         "0"      → 메인 복귀
         그 외    → 무시, 재표시
```

### 4.3 화면 출력 예시

```
  ╔══════════════════════════════════════════════════════════╗
  ║   주문 승인 / 거절
  ╚══════════════════════════════════════════════════════════╝

  ──────────────────────────────────────────────────────────
  #   주문번호              시료     고객명          수량   접수일시
  ──────────────────────────────────────────────────────────
  1   ORD-20260508-0001   S-001   삼성전자        30 ea  2026-05-08 10:00
  2   ORD-20260508-0002   S-002   SK하이닉스      50 ea  2026-05-08 10:15
  3   ORD-20260508-0003   S-001   LG화학          20 ea  2026-05-08 10:30
  4   ORD-20260508-0004   S-003   현대자동차      15 ea  2026-05-08 10:45
  5   ORD-20260508-0005   S-002   포스코          40 ea  2026-05-08 11:00
  ──────────────────────────────────────────────────────────
  총 8건 (RESERVED) | 1 / 2 페이지

  [1~5] 선택   [n] 다음   [p] 이전   [0] 뒤로
```

주문 선택 후 승인/거절:
```
  선택된 주문: ORD-20260508-0002 | SK하이닉스 | S-002 | 50ea
  ──────────────────────────────────────────────────────────
  [a] 승인   [r] 거절   [0] 취소
선택:
```

### 4.4 승인 결과 — Case 1 (재고 충분)

```
  시료: S-001 알파-시료 | 재고: 100ea | 주문수량: 30ea
  → 재고 충분 → 즉시 CONFIRMED

  재고 차감 : 100 → 70 ea
  주문 상태 : CONFIRMED
  ✔  승인 완료. 출고 대기 중입니다.
```

### 4.5 승인 결과 — Case 2 (재고 부족)

```
  시료: S-002 베타-시료 | 재고: 5ea | 주문수량: 50ea
  → 재고 부족 (부족분: 45ea) → 생산 라인 등록

  실 생산량  : ceil(45 / (0.88 × 0.9)) = 57 ea
  총 생산시간: 1.5분 × 57 = 85.5분
  주문 상태  : PRODUCING
  ✔  승인 완료. 생산 라인에 등록되었습니다.
```

### 4.6 거절 결과

```
  선택된 주문: ORD-20260508-0001 | 삼성전자 | S-001 | 30ea

  주문 상태 : REJECTED
  ✔  거절 처리 완료.
```

---

## 5. 메인 메뉴 연결

```cpp
// main() 객체 생성
OrderManagerView orderMgrView(orderSvc, sampleSvc);

// case 3 연결
case 3: orderMgrView.run(); break;
```

---

## 6. gtest 계획 (`tests/order_manager_test.cpp`)

```cpp
class OrderManagerTest : public ::testing::Test {
protected:
    const std::string              path_  = "test_ordermgr.json";
    std::unique_ptr<AppDB>         db_;
    std::unique_ptr<SampleService> sampleSvc_;
    std::unique_ptr<OrderService>  orderSvc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_        = std::make_unique<AppDB>(path_);
        sampleSvc_ = std::make_unique<SampleService>(*db_);
        orderSvc_  = std::make_unique<OrderService>(*db_);
        // 공통 시료 등록
        sampleSvc_->add("알파-시료", 0.5, 0.95, 100); // S-001, 재고 충분
        sampleSvc_->add("베타-시료", 1.5, 0.88,   5); // S-002, 재고 부족 (avgTime 다른 값)
        sampleSvc_->add("감마-시료", 0.5, 0.90,   0); // S-003, 재고 없음
    }
    void TearDown() override {
        orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }
};

// TC-OM-01: 승인 — 재고 충분 → CONFIRMED, stock 감소
TEST_F(OrderManagerTest, Approve_StockSufficient) {
    auto o = orderSvc_->createOrder("S-001", 30, "삼성전자");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_TRUE(r.success);
    EXPECT_TRUE(r.sufficient);
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(70, sampleSvc_->findById("S-001")->stock);  // 100 - 30
}

// TC-OM-02: 승인 — 재고 부족 → PRODUCING, 생산 큐 등록
TEST_F(OrderManagerTest, Approve_StockInsufficient) {
    auto o = orderSvc_->createOrder("S-002", 50, "SK하이닉스");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_TRUE(r.success);
    EXPECT_FALSE(r.sufficient);
    EXPECT_EQ(OrderStatus::PRODUCING, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(45, r.shortage);    // 50 - 5
    // ceil(45 / (0.88 × 0.9)) = ceil(56.82) = 57
    EXPECT_EQ(57, r.actualQty);
    EXPECT_NEAR(1.5 * 57, r.totalTime, 1e-9);  // 1.5 * 57 = 85.5
    EXPECT_EQ(0, sampleSvc_->findById("S-002")->stock); // 기존 재고 전량 소진
    // 생산 큐 등록 확인
    EXPECT_FALSE(db_->queue().empty());
    EXPECT_EQ(o.id, db_->queue()[0].order_id);
}

// TC-OM-03: 승인 — 재고 0 → 전량 생산
TEST_F(OrderManagerTest, Approve_ZeroStock) {
    auto o = orderSvc_->createOrder("S-003", 20, "LG화학");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_TRUE(r.success);
    EXPECT_FALSE(r.sufficient);
    EXPECT_EQ(20, r.shortage);
    // ceil(20 / (0.90 × 0.9)) = ceil(24.69) = 25
    EXPECT_EQ(25, r.actualQty);
    EXPECT_EQ(0, sampleSvc_->findById("S-003")->stock); // 재고 0 유지 (변화 없음)
    EXPECT_EQ(OrderStatus::PRODUCING, orderSvc_->findById(o.id)->status);
}

// TC-OM-04: 승인 — 존재하지 않는 주문
TEST_F(OrderManagerTest, Approve_NotFound) {
    auto r = orderSvc_->approveOrder("ORD-99991231-9999");
    EXPECT_FALSE(r.success);
}

// TC-OM-05: 승인 — RESERVED 아닌 주문 (이미 승인된 주문)
TEST_F(OrderManagerTest, Approve_NotReserved) {
    auto o = orderSvc_->createOrder("S-001", 10, "포스코");
    orderSvc_->approveOrder(o.id);  // CONFIRMED 상태로 변경
    auto r = orderSvc_->approveOrder(o.id);  // 재승인 시도
    EXPECT_FALSE(r.success);
}

// TC-OM-06: 거절 → REJECTED
TEST_F(OrderManagerTest, Reject_Success) {
    auto o = orderSvc_->createOrder("S-001", 30, "현대자동차");
    EXPECT_TRUE(orderSvc_->rejectOrder(o.id));
    EXPECT_EQ(OrderStatus::REJECTED, orderSvc_->findById(o.id)->status);
}

// TC-OM-07: 거절 — 재고 변화 없음
TEST_F(OrderManagerTest, Reject_StockUnchanged) {
    auto o = orderSvc_->createOrder("S-001", 30, "한화솔루션");
    orderSvc_->rejectOrder(o.id);
    EXPECT_EQ(100, sampleSvc_->findById("S-001")->stock);  // 불변
}

// TC-OM-08: 거절 — RESERVED 아닌 주문
TEST_F(OrderManagerTest, Reject_NotReserved) {
    auto o = orderSvc_->createOrder("S-001", 10, "롯데케미칼");
    orderSvc_->approveOrder(o.id);   // CONFIRMED로 변경
    EXPECT_FALSE(orderSvc_->rejectOrder(o.id));
}

// TC-OM-09: reservedOrders() — RESERVED만 필터링
TEST_F(OrderManagerTest, ReservedOrders_Filter) {
    auto o1 = orderSvc_->createOrder("S-001", 10, "고객A");
    auto o2 = orderSvc_->createOrder("S-001", 20, "고객B");
    auto o3 = orderSvc_->createOrder("S-001",  5, "고객C");
    orderSvc_->approveOrder(o1.id);   // CONFIRMED
    orderSvc_->rejectOrder(o2.id);    // REJECTED
    // o3 은 RESERVED 유지

    auto reserved = orderSvc_->reservedOrders();
    ASSERT_EQ(1u, reserved.size());
    EXPECT_EQ(o3.id, reserved[0]->id);
}

// TC-OM-10: ceil 경계값 — TC-OM-02와 다른 시료·shortage로 공식 다양성 확보
// 감마-시료: yield_rate=0.90, stock=0, shortage=9
// ceil(9 / (0.90 × 0.9)) = ceil(9 / 0.81) = ceil(11.11) = 12
TEST_F(OrderManagerTest, ProductionFormula_DifferentYield) {
    auto o = orderSvc_->createOrder("S-003", 9, "OCI Company");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_EQ(12, r.actualQty);
    EXPECT_NEAR(0.5 * 12, r.totalTime, 1e-9);  // 감마: avgTime=0.5
}

// TC-OM-11: 영속성 — 재로드 후 상태 유지
TEST_F(OrderManagerTest, Persistence_ApproveResult) {
    auto o = orderSvc_->createOrder("S-001", 30, "Samsung SDI");
    orderSvc_->approveOrder(o.id);

    orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
    db_        = std::make_unique<AppDB>(path_);
    sampleSvc_ = std::make_unique<SampleService>(*db_);
    orderSvc_  = std::make_unique<OrderService>(*db_);

    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(70, sampleSvc_->findById("S-001")->stock);
}
```

**총 11개 테스트 케이스 (OrderManagerTest)**

---

## 7. 시나리오 테스트 추가 (scenario_test.cpp)

Phase 4-4 완료 후 `tests/scenario_test.cpp`에 추가:

```cpp
// S8: 승인 (재고 충분) → CONFIRMED → 출고 → RELEASE

// S9: 승인 (재고 부족) → PRODUCING → 생산 완료 → CONFIRMED → 출고
// 시간 시뮬레이션 전략: total_time을 0에 가까운 값으로 설정 후
// queue()[0].started_at을 과거 시각("2000-01-01 00:00:00")으로 직접 설정 →
// db.updateQueueItem() → db.checkAndComplete() 호출 시 자동 완료 처리

// S10: 주문 거절 → 재고 불변 확인
```

---

## 8. Phase 4-4 완료 후 파일 구조

```
MVC/
├── Service/
│   └── OrderService.h     ← UPDATE: approveOrder·rejectOrder·reservedOrders 추가
└── View/
    └── OrderManagerView.h ← NEW

SampleOrderSystem/
└── tests/
    ├── order_manager_test.cpp ← NEW (11개)
    └── scenario_test.cpp      ← UPDATE (S8·S9·S10 추가)
```

---

## 9. 의존성

```
AppDB (Phase 3)
    ├── SampleService (Phase 4-2)  ←── OrderManagerView (시료 정보 조회)
    └── OrderService  (Phase 4-4 확장)
            └── OrderManagerView  (승인/거절 UI)
```

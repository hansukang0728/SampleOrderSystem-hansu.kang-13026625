# Phase 4-3 — 시료 주문 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md)  
> Feature 문서: [docs/feature/sampleOrder.md](../feature/sampleOrder.md)  
> 상위 계획: **Phase 4 / 7** — 서비스·UI 레이어  
> 서브 단계: **4-3** — 시료 주문 생성 (O-01)

---

## 1. 목표

시료 ID·고객명·수량을 입력받아 RESERVED 상태의 주문을 생성한다.  
`OrderService`는 Phase 4-4(승인/거절)·4-6(출고)에서 확장되므로,  
이번 Phase에서는 **주문 생성(O-01)과 관련 유효성 검증**만 구현한다.

### 완료 기준
- [ ] `MVC/Service/OrderService.h` — createOrder + 유효성 검증
- [ ] `MVC/View/OrderView.h` — 주문 생성 UI
- [ ] 시료 ID 입력 → 존재 확인 → 재고 표시 (상태 레이블 없음)
- [ ] 고객명·수량 재입력 루프 (유효값 입력까지 반복)
- [ ] 주문번호 `ORD-YYYYMMDD-XXXX` 자동 생성, RESERVED 저장, 결과 출력
- [ ] 메인 메뉴 2번에 OrderView 연결
- [ ] gtest — `OrderServiceTest` 전체 통과
- [ ] 빌드 경고 0, 오류 0

---

## 2. 구현 대상 파일

| 파일 | 위치 | 구분 |
|---|---|---|
| `OrderService.h` | `MVC/Service/` | 신규 |
| `OrderView.h` | `MVC/View/` | 신규 |
| `SampleOrderSystem.cpp` | `SampleOrderSystem/` | 수정 (2번 메뉴 연결) |
| `SampleOrderSystem.vcxproj` | `SampleOrderSystem/` | 수정 |
| `MVC.vcxproj` | `MVC/` | 수정 |

---

## 3. OrderService 설계 (`MVC/Service/OrderService.h`)

### 3.1 인터페이스

```cpp
class OrderService {
public:
    explicit OrderService(AppDB& db) : db_(db) {}

    // O-01: 주문 생성 → RESERVED  (ID·created_at 자동 생성은 AppDB 담당)
    Order createOrder(const std::string& sampleId, int qty,
                      const std::string& customer);

    // 조회 (Phase 4-4 이후 사용 — 현재는 내부 목록만 제공)
    const std::vector<Order>& all() const;
    Order* findById(const std::string& id);

    // 유효성 검증 (bool 반환, 오류 메시지 출력은 View 담당)
    static bool validateCustomerName(const std::string& name); // 트림 후 비어있지 않아야 함
    static bool validateQuantity(int qty);

private:
    AppDB& db_;
};
```

> Phase 4-4에서 `approveOrder()`, `rejectOrder()` 추가  
> Phase 4-6에서 `releaseOrder()` 추가

### 3.2 유효성 검증 규칙

| 메서드 | 조건 | 실패 시 |
|---|---|---|
| `validateCustomerName` | 공백 트림 후 비어있지 않음 (`"   "` 불가) | false |
| `validateQuantity` | >= 1 | false |

> **시료 ID 유효성**: 시료 존재 여부는 `SampleService::findById()`로 확인.  
> `OrderService`는 사이드 이펙트 없이 `createOrder()` 실행 전에 View가 검증 완료했다고 가정한다.

---

## 4. OrderView 설계 (`MVC/View/OrderView.h`)

### 4.1 인터페이스

```cpp
class OrderView {
public:
    OrderView(OrderService& orderSvc, SampleService& sampleSvc)
        : orderSvc_(orderSvc), sampleSvc_(sampleSvc) {}
    void run();  // 주문 생성 흐름 (단일 기능, 서브메뉴 없음)

private:
    OrderService&  orderSvc_;
    SampleService& sampleSvc_;
};
```

> `OrderView`는 서브메뉴 없이 주문 생성 흐름만 수행하고 완료/취소 후 메인으로 복귀.  
> Phase 4-4의 주문 승인/거절은 별도 `OrderManagerView`로 분리한다.

### 4.2 주문 생성 흐름

> **취소 기능 미제공**: 주문 생성 흐름 중 중단은 Ctrl+C 전용.  
> 재입력 루프는 유효한 값이 입력될 때까지 반복하며 별도 탈출 키 없음.

```
[시료 주문]

① 시료 ID 입력
   → 재입력 루프: SampleService::findById(id) 가 null 이면 오류 후 재입력

② 시료 정보 표시 (재고 수량, 상태 레이블 없음)
   → [S-001] 알파-시료 | 재고: 100ea

③ 고객명 입력
   → 재입력 루프: validateCustomerName() 실패 시 오류 후 재입력

④ 주문 수량 입력
   → 재입력 루프: validateQuantity() 실패 시 오류 후 재입력

⑤ OrderService::createOrder() 호출 → 저장

⑥ 주문 요약 출력 + 완료 메시지
```

### 4.3 UI 출력 예시

```
  ╔══════════════════════════════════════════════════════════╗
  ║   시료 주문
  ╚══════════════════════════════════════════════════════════╝

  시료 ID (예: S-001): S-001
  → [S-001] 알파-시료 | 재고: 100ea

  고객명: 홍길동
  주문 수량 (ea): 30

  ──────────────────────────────────────────────────────────
   주문번호 : ORD-20260508-0001
   시료     : S-001  알파-시료
   고객명   : 홍길동
   수량     : 30 ea
   상태     : RESERVED
  ──────────────────────────────────────────────────────────

  ✔  주문이 접수되었습니다.
```

---

## 5. 메인 메뉴 연결

```cpp
// main() 객체 생성
OrderService  orderSvc(db);
OrderView     orderView(orderSvc, sampleSvc);

// case 2 연결
case 2: orderView.run(); break;
```

---

## 6. gtest 계획 (`tests/order_service_test.cpp`)

```cpp
class OrderServiceTest : public ::testing::Test {
protected:
    const std::string              path_  = "test_order.json";
    std::unique_ptr<AppDB>         db_;
    std::unique_ptr<OrderService>  svc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_  = std::make_unique<AppDB>(path_);
        svc_ = std::make_unique<OrderService>(*db_);
    }
    void TearDown() override {
        svc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }
};

// TC-OS-01: 주문 생성 → RESERVED 상태 + created_at 형식 검증
TEST_F(OrderServiceTest, CreateOrder_Reserved) {
    auto o = svc_->createOrder("S-001", 30, "홍길동");
    EXPECT_EQ(OrderStatus::RESERVED, o.status);
    EXPECT_EQ("S-001",  o.sample_id);
    EXPECT_EQ(30,       o.quantity);
    EXPECT_EQ("홍길동", o.customer_name);
    // created_at 형식: "YYYY-MM-DD HH:MM:SS" (19자)
    ASSERT_EQ(19u, o.created_at.size());
    EXPECT_EQ('-', o.created_at[4]);
    EXPECT_EQ('-', o.created_at[7]);
    EXPECT_EQ(' ', o.created_at[10]);
    EXPECT_EQ(':', o.created_at[13]);
    EXPECT_EQ(':', o.created_at[16]);
}

// TC-OS-02: 주문번호 형식 (ORD-YYYYMMDD-XXXX)
TEST_F(OrderServiceTest, CreateOrder_IdFormat) {
    auto o = svc_->createOrder("S-001", 10, "고객A");
    std::string today = todayStr();
    std::string expectedPrefix = "ORD-" + today + "-";
    EXPECT_EQ(0u, o.id.find(expectedPrefix));
    EXPECT_EQ(expectedPrefix + "0001", o.id);
}

// TC-OS-03: 당일 주문 순번 증가
TEST_F(OrderServiceTest, CreateOrder_DailySequence) {
    auto o1 = svc_->createOrder("S-001", 10, "고객A");
    auto o2 = svc_->createOrder("S-002", 20, "고객B");
    auto o3 = svc_->createOrder("S-001",  5, "고객C");
    std::string today = todayStr();
    EXPECT_EQ("ORD-" + today + "-0001", o1.id);
    EXPECT_EQ("ORD-" + today + "-0002", o2.id);
    EXPECT_EQ("ORD-" + today + "-0003", o3.id);
}

// TC-OS-04: 유효성 검증 — 고객명 (공백 트림 포함)
TEST_F(OrderServiceTest, Validation_CustomerName) {
    EXPECT_FALSE(OrderService::validateCustomerName(""));
    EXPECT_FALSE(OrderService::validateCustomerName("   "));  // 공백만 → 실패
    EXPECT_TRUE(OrderService::validateCustomerName("홍길동"));
    EXPECT_TRUE(OrderService::validateCustomerName("A"));
}

// TC-OS-05: 유효성 검증 — 주문 수량
TEST_F(OrderServiceTest, Validation_Quantity) {
    EXPECT_FALSE(OrderService::validateQuantity(0));
    EXPECT_FALSE(OrderService::validateQuantity(-1));
    EXPECT_TRUE(OrderService::validateQuantity(1));   // 경계: 최솟값
    EXPECT_TRUE(OrderService::validateQuantity(100));
}

// TC-OS-06: 영속성 — 재로드 후 주문 유지
TEST_F(OrderServiceTest, Persistence) {
    svc_->createOrder("S-001", 30, "홍길동");
    svc_.reset(); db_.reset();
    db_  = std::make_unique<AppDB>(path_);
    svc_ = std::make_unique<OrderService>(*db_);
    ASSERT_EQ(1u, svc_->all().size());
    EXPECT_EQ("홍길동",           svc_->all()[0].customer_name);
    EXPECT_EQ(OrderStatus::RESERVED, svc_->all()[0].status);
}
```

**총 6개 테스트 케이스 (OrderServiceTest)**

---

## 7. Phase 4-3 완료 후 파일 구조

```
MVC/
├── Service/
│   ├── SampleService.h    (Phase 4-2)
│   └── OrderService.h     ← NEW
└── View/
    ├── ConsoleUI.h        (Phase 4-1)
    ├── SampleView.h       (Phase 4-2)
    └── OrderView.h        ← NEW

SampleOrderSystem/
└── tests/
    └── order_service_test.cpp ← NEW
```

---

## 8. 의존성

```
AppDB (Phase 3)
    ├── OrderService (Phase 4-3)  ←── OrderView
    └── SampleService (Phase 4-2) ←── OrderView  (시료 ID 존재 확인용)

OrderView → OrderService   (주문 생성)
OrderView → SampleService  (시료 ID 검증 + 정보 표시)
```

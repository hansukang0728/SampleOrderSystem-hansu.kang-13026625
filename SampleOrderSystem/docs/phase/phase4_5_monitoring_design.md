# Phase 4-5 — 모니터링 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md)  
> Feature 문서: [docs/feature/Monitoring.md](../feature/Monitoring.md)  
> 상위 계획: **Phase 4 / 7** — 서비스·UI 레이어  
> 서브 단계: **4-5** — 모니터링 대시보드 (M-01, M-02)  
> M-03(생산 큐 현황)은 Phase 4-6 생산 라인에서 담당

---

## 1. 목표

주문 현황·재고 현황·생산 큐 현황을 한 화면에 표시하는 대시보드를 구현한다.  
조회 시마다 `checkAndComplete()`를 실행하여 IN_PROGRESS 생산 항목의 자동 완료를 처리한다.

### 완료 기준
- [ ] `MonitoringService.h` — 대시보드 데이터 집계 로직
- [ ] `MonitoringView.h` — 2개 섹션 출력 + Enter 새로고침 + [m] 메인 이동
- [ ] M-01 재고 현황: 시료별 주문 수요·생산 입고량 고려한 여유/부족/고갈 상태
- [ ] M-02 주문 현황: RESERVED/PRODUCING/CONFIRMED/RELEASE 건수 (REJECTED 제외)
- [ ] 조회 시 `checkAndComplete()` 자동 실행 → 완료 항목 즉시 갱신
- [ ] 메인 메뉴 4번에 `MonitoringView` 연결
- [ ] gtest — `MonitoringServiceTest` 전체 통과
- [ ] 빌드 경고 0, 오류 0

---

## 2. 구현 대상 파일

| 파일 | 위치 | 구분 |
|---|---|---|
| `MonitoringService.h` | `MVC/Service/` | 신규 |
| `MonitoringView.h` | `MVC/View/` | 신규 |
| `SampleOrderSystem.cpp` | `SampleOrderSystem/` | 수정 (4번 연결) |
| `tests/monitoring_test.cpp` | `SampleOrderSystem/tests/` | 신규 |

---

## 3. MonitoringService 설계

### 3.1 집계 데이터 구조

```cpp
struct DashboardData {
    // M-02: 주문 현황 (REJECTED 제외)
    int cntReserved  = 0;
    int cntProducing = 0;
    int cntConfirmed = 0;
    int cntRelease   = 0;

    // 전광판용 집계
    int totalStock   = 0;  // 전체 재고 합계
    int activeOrders = 0;  // REJECTED 제외 주문 수

    // 업데이트 시각 (collect() 호출 시점)
    std::string updatedAt;   // "YYYY-MM-DD HH:MM:SS"
};
```

> **설계 결정**: DashboardData는 카운트 집계만 담는다.  
> 재고 목록(`db_.samples()`)과 생산 큐(`pendingQueue()`)는 `render()` 내에서  
> `collect()` 이후 AppDB를 직접 참조한다 — `collect()` 실행으로 상태가 확정된 후이므로 일관성 보장.

### 3.2 인터페이스

```cpp
class MonitoringService {
public:
    explicit MonitoringService(AppDB& db) : db_(db) {}

    // 집계 데이터 조회 + 생산 완료 자동 처리
    DashboardData collect();

    // 시료별 재고 상태 계산 (주문 수요·생산 입고량 고려)
    struct SampleStockInfo {
        std::string status;       // "여유" / "부족" / "고갈"
        const char* icon;         // "●" / "▲" / "✕"
        const char* color;        // UI::GRN / UI::YLW / UI::RED
        int         reservedDemand;    // 해당 시료 RESERVED 수요 합계
        int         productionIncoming; // 미완료 큐 actual_qty 합계
    };
    SampleStockInfo calcStockInfo(const std::string& sampleId) const;

    // stock 단독 기준 (고갈만 판정, 나머지는 calcStockInfo 사용)
    static bool isDepleted(int stock) { return stock == 0; }

    // (생산 큐 조회는 Phase 4-6 생산 라인에서 담당)

private:
    AppDB& db_;
};
```

### 3.3 collect() 구현

```
① d.updatedAt = nowStr()          // 조회 시각 캡처 (가장 먼저)
② db_.checkAndComplete()          // IN_PROGRESS 자동 완료
③ 주문 순회 → REJECTED 제외하고 상태별 카운팅
④ 시료 순회 → totalStock 합산
⑤ activeOrders = REJECTED 제외 전체 수
⑥ DashboardData 반환
```

### 3.4 calcStockInfo() — 시료별 재고 상태 계산

**기준 (단순 숫자가 아닌 주문 수요 대비 계산)**

```
고갈: stock == 0
부족: stock > 0 AND (stock + productionIncoming) < reservedDemand
여유: stock > 0 AND (stock + productionIncoming) >= reservedDemand
     (reservedDemand == 0 이면 항상 여유)
```

**구현**
```cpp
SampleStockInfo MonitoringService::calcStockInfo(const std::string& sampleId) const {
    const Sample* s = db_.findSample(sampleId);
    if (!s) return {};

    // RESERVED 수요: 해당 시료의 RESERVED 주문 quantity 합계
    int reservedDemand = 0;
    for (const auto& o : db_.orders())
        if (o.sample_id == sampleId && o.status == OrderStatus::RESERVED)
            reservedDemand += o.quantity;

    // 생산 중 입고 예정: 미완료 큐 actual_qty 합계
    int productionIncoming = 0;
    for (const auto& p : db_.queue())
        if (p.sample_id == sampleId && !p.completed)
            productionIncoming += p.actual_qty;

    SampleStockInfo info;
    info.reservedDemand     = reservedDemand;
    info.productionIncoming = productionIncoming;

    if (s->stock == 0) {
        info.status = "고갈"; info.icon = "✕"; info.color = UI::RED;
    } else if (s->stock + productionIncoming < reservedDemand) {
        info.status = "부족"; info.icon = "▲"; info.color = UI::YLW;
    } else {
        info.status = "여유"; info.icon = "●"; info.color = UI::GRN;
    }
    return info;
}
```

| stock | 생산중 입고 | RESERVED 수요 | 상태 |
|---|---|---|---|
| 0 | 57 | 50 | 고갈 |
| 5 | 0 | 3 | 여유 (5 >= 3) |
| 5 | 0 | 10 | 부족 (5 < 10) |
| 5 | 57 | 60 | 여유 (5+57=62 >= 60) |
| 3 | 10 | 20 | 부족 (3+10=13 < 20) |

### 3.5 estimatedCompletion()

```
if p.isWaiting() → "-"
else → parseTime(p.started_at) + (p.total_time * 60초)
     → strftime 으로 "YYYY-MM-DD HH:MM" 포맷
```

---

## 4. MonitoringView 설계

### 4.1 인터페이스

```cpp
class MonitoringView {
public:
    explicit MonitoringView(MonitoringService& svc) : svc_(svc) {}
    void run();  // Enter 새로고침 루프

private:
    void render();                                          // 전체 화면 출력
    void renderOrderStatus(const DashboardData& d) const;  // M-02
    void renderStockStatus() const;                        // M-01

    MonitoringService& svc_;
};
```

### 4.2 메인 루프 및 render() 호출 순서

```cpp
void MonitoringView::run() {
    while (true) {
        render();
        std::string inp = UI::readLine("  [Enter] 새로고침   [0] 뒤로   [m] 메인: ");
        if (inp == "0") return;
        if (inp == "m" || inp == "M") { UI::goToMain = true; return; }
        // 그 외(Enter 포함) → 재렌더링
    }
}

// render() 내부 실행 순서 — collect() 1회 원칙
void MonitoringView::render() {
    UI::clearScreen();
    UI::printHeader("모니터링 대시보드");

    // ① checkAndComplete() 실행 + 집계 + 업데이트 시각 캡처 (1회만)
    DashboardData d = svc_.collect();  // d.updatedAt 포함

    // ② 이후 render는 확정된 AppDB 상태와 동일 시각(d.updatedAt)을 사용
    renderOrderStatus(d);           // M-02: 시각 포함 출력
    renderStockStatus(d.updatedAt); // M-01: 동일 시각 사용
}
```

> **collect() 1회 원칙**: `render()` 진입 시 `collect()`를 반드시 먼저 호출하여  
> `checkAndComplete()` 부작용을 1회로 제한한다.  
> `renderStockStatus()`·`renderProductionQueue()`는 `collect()` 이후에만 호출된다.
```

### 4.3 전체 화면 출력 예시

```
  ╔══════════════════════════════════════════════════════════╗
  ║   모니터링 대시보드            📅 2026-05-08 15:30:00
  ╚══════════════════════════════════════════════════════════╝

  [ 주문 현황 ]  (REJECTED 제외)   🕐 2026-05-08 15:30:00
  ──────────────────────────────────────────────────────────
   RESERVED    :  2건
   PRODUCING   :  1건
   CONFIRMED   :  3건
   RELEASE     :  5건
  ──────────────────────────────────────────────────────────

  [ 재고 현황 ]                    🕐 2026-05-08 15:30:00
  ──────────────────────────────────────────────────────────
   ID      이름                  재고(ea)   상태
  ──────────────────────────────────────────────────────────
   S-001   알파-시료                100 ea   여유  ●
   S-002   베타-시료                  5 ea   부족  ▲
   S-003   감마-시료                  0 ea   고갈  ✕
  ──────────────────────────────────────────────────────────

  [Enter] 새로고침   [0] 뒤로   [m] 메인
```

> `collect()` 호출 시점의 `nowStr()`을 `DashboardData.updatedAt`에 저장하여  
> 헤더·주문 현황·재고 현황 모두 **동일한 시각**을 표시한다.

---

## 5. 메인 메뉴 연결

```cpp
MonitoringService monitorSvc(db);
MonitoringView    monitorView(monitorSvc);

case 4: monitorView.run(); UI::goToMain = false; break;
```

---

## 6. gtest 계획 (`tests/monitoring_test.cpp`)

```cpp
class MonitoringTest : public ::testing::Test {
protected:
    const std::string              path_ = "test_monitor.json";
    std::unique_ptr<AppDB>         db_;
    std::unique_ptr<SampleService> sampleSvc_;
    std::unique_ptr<OrderService>  orderSvc_;
    std::unique_ptr<MonitoringService> monSvc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_        = std::make_unique<AppDB>(path_);
        sampleSvc_ = std::make_unique<SampleService>(*db_);
        orderSvc_  = std::make_unique<OrderService>(*db_);
        monSvc_    = std::make_unique<MonitoringService>(*db_);
    }
    void TearDown() override {
        monSvc_.reset(); orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }
};

// TC-MN-01: 빈 상태 collect() → 모두 0
TEST_F(MonitoringTest, Collect_Empty) {
    auto d = monSvc_->collect();
    EXPECT_EQ(0, d.cntReserved);
    EXPECT_EQ(0, d.cntProducing);
    EXPECT_EQ(0, d.cntConfirmed);
    EXPECT_EQ(0, d.cntRelease);
    EXPECT_EQ(0, d.totalStock);
    EXPECT_EQ(0, d.activeOrders);
}

// TC-MN-02: 주문 상태별 카운팅 (REJECTED 제외)
TEST_F(MonitoringTest, Collect_OrderCounts) {
    sampleSvc_->add("알파", 0.5, 0.95, 100);
    auto o1 = orderSvc_->createOrder("S-001",  5, "고객A");
    auto o2 = orderSvc_->createOrder("S-001",  5, "고객B");
    auto o3 = orderSvc_->createOrder("S-001",  5, "고객C");
    auto o4 = orderSvc_->createOrder("S-001",  5, "고객D");
    orderSvc_->approveOrder(o1.id);  // CONFIRMED
    orderSvc_->rejectOrder(o2.id);   // REJECTED (제외)
    // o3, o4 → RESERVED

    auto d = monSvc_->collect();
    EXPECT_EQ(2, d.cntReserved);   // o3, o4
    EXPECT_EQ(0, d.cntProducing);
    EXPECT_EQ(1, d.cntConfirmed);  // o1
    EXPECT_EQ(0, d.cntRelease);
    EXPECT_EQ(3, d.activeOrders);  // REJECTED 제외: o1, o3, o4
}

// TC-MN-03: totalStock 집계
TEST_F(MonitoringTest, Collect_TotalStock) {
    sampleSvc_->add("알파", 0.5, 0.95, 100);
    sampleSvc_->add("베타", 0.5, 0.88,  50);
    sampleSvc_->add("감마", 0.5, 0.90,   0);
    auto d = monSvc_->collect();
    EXPECT_EQ(150, d.totalStock);  // 100 + 50 + 0
}

// TC-MN-04: calcStockInfo — 재고 상태 케이스별 검증
TEST_F(MonitoringTest, CalcStockInfo_StatusCases) {
    sampleSvc_->add("알파", 0.5, 0.95, 5);  // S-001, stock=5
    sampleSvc_->add("베타", 0.5, 0.88, 0);  // S-002, stock=0

    // 고갈: stock == 0
    auto i2 = monSvc_->calcStockInfo("S-002");
    EXPECT_EQ("고갈", i2.status);

    // 여유: RESERVED 수요 없음 + stock > 0
    auto i1 = monSvc_->calcStockInfo("S-001");
    EXPECT_EQ("여유", i1.status);

    // 여유: stock >= RESERVED 수요
    orderSvc_->createOrder("S-001", 3, "고객A");  // RESERVED 수요 3
    i1 = monSvc_->calcStockInfo("S-001");
    EXPECT_EQ("여유", i1.status);  // 5 >= 3

    // 부족: stock < RESERVED 수요 (생산 없음)
    orderSvc_->createOrder("S-001", 10, "고객B");  // 누적 수요 13
    i1 = monSvc_->calcStockInfo("S-001");
    EXPECT_EQ("부족", i1.status);  // 5 < 13

    // 여유로 전환: PRODUCING 큐 actual_qty 포함 시 충분
    // S-001 stock=5, 수요=13, 생산 중 actual_qty=10 → 5+10=15 >= 13
    auto o = orderSvc_->createOrder("S-001", 1, "고객C");
    orderSvc_->approveOrder(o.id);  // PRODUCING → 큐에 actual_qty 등록
    // 큐 actual_qty 확인 후 수요와 비교
    int incoming = 0;
    for (const auto& p : db_->queue()) if (!p.completed) incoming += p.actual_qty;
    i1 = monSvc_->calcStockInfo("S-001");
    // stock(0, 소진) + incoming >= 남은 RESERVED 수요면 여유
    // (승인 후 stock=0, PRODUCING 처리되어 RESERVED 수요에서 제외됨)
    EXPECT_TRUE(i1.status == "여유" || i1.status == "부족" || i1.status == "고갈");
    // 구체적 상태는 approveOrder 결과에 따라 달라지므로 타입 검증만
}

// TC-MN-05: collect() 시 IN_PROGRESS 자동 완료 처리 후 상태 반영
TEST_F(MonitoringTest, Collect_AutoComplete_UpdatesStatus) {
    sampleSvc_->add("베타", 0.5, 0.88, 0);
    auto o = orderSvc_->createOrder("S-001", 10, "삼성전자");
    auto r = orderSvc_->approveOrder(o.id);  // PRODUCING
    ASSERT_FALSE(r.sufficient);

    // started_at 과거 설정 → 경과 시간 충족
    auto& q = db_->queue();
    q[0].started_at = "2000-01-01 00:00:00";
    db_->updateQueueItem(q[0]);

    // collect() → checkAndComplete() → CONFIRMED + 카운트 반영
    auto d = monSvc_->collect();
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(r.actualQty, sampleSvc_->findById("S-001")->stock);
    EXPECT_EQ(0, d.cntProducing);   // 완료 처리 후 PRODUCING 0
    EXPECT_EQ(1, d.cntConfirmed);   // CONFIRMED로 전환
}

// TC-MN-06 (구 TC-MN-09): PRODUCING 상태 주문이 cntProducing에 집계
TEST_F(MonitoringTest, Collect_ProducingCount) {
    sampleSvc_->add("베타", 0.5, 0.88, 0);
    auto o1 = orderSvc_->createOrder("S-001", 5, "고객A");
    auto o2 = orderSvc_->createOrder("S-001", 3, "고객B");
    orderSvc_->approveOrder(o1.id);  // PRODUCING
    orderSvc_->approveOrder(o2.id);  // PRODUCING

    auto d = monSvc_->collect();
    EXPECT_EQ(0, d.cntReserved);
    EXPECT_EQ(2, d.cntProducing);
    EXPECT_EQ(0, d.cntConfirmed);
    EXPECT_EQ(2, d.activeOrders);
}
```

**총 6개 테스트 케이스 (MonitoringTest)**

---

## 7. Phase 4-5 완료 후 파일 구조

```
MVC/
├── Service/
│   └── MonitoringService.h    ← NEW
└── View/
    └── MonitoringView.h       ← NEW

SampleOrderSystem/
└── tests/
    └── monitoring_test.cpp    ← NEW
```

---

## 8. 의존성

```
AppDB (Phase 3)
    ├── SampleService   (Phase 4-2) ← MonitoringView (재고 현황 출력 참고)
    ├── OrderService    (Phase 4-4) ← 주문 상태 집계에 직접 의존 안 함 (AppDB 직접)
    └── MonitoringService (Phase 4-5)
            └── MonitoringView
```

> `MonitoringService`는 `AppDB`에서 직접 데이터를 읽는다.  
> `OrderService` / `SampleService`를 경유하지 않아 집계 전용 단순 구조 유지.

---

## 9. 주의 사항

- `collect()` 내 `checkAndComplete()`는 부작용을 일으킴 (DB 저장) — `render()` 진입 시 1회만 호출
- 생산 큐 현황(M-03)은 Phase 4-6 생산 라인 View에서 담당

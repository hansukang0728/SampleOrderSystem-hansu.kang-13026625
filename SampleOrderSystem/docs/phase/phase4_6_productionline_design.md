# Phase 4-6 — 생산 라인 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md)  
> Feature 문서: [docs/feature/ProductionLine.md](../feature/ProductionLine.md)  
> 상위 계획: **Phase 4 / 7** — 서비스·UI 레이어  
> 서브 단계: **4-6** — 생산 라인 (P-01, P-02, P-03, P-04)

---

## 1. 목표

생산 라인의 **현재 생산 현황**(IN_PROGRESS)과 **대기 주문 목록**(WAITING)을 조회하고,  
다음 대기 항목의 생산을 시작(processNext)할 수 있는 화면을 구현한다.  
화면은 3초마다 자동 갱신되어 실시간 진행률·현재 시각을 표시한다.

### 완료 기준
- [ ] `ProductionService.h` — processNext(), pendingQueue(), inProgressItem()
- [ ] `ProductionView.h` — 생산 현황 카드 + 대기 목록 + 3초 자동 새로고침
- [ ] 생산 현황: 주문정보·주문시각·현재 생산량(추정)·예상 완료 시각 표시
- [ ] 대기 주문: FIFO 순서, 주문번호·시료·고객명·수량·대기시작 표시
- [ ] [s] 생산 시작: WAITING → IN_PROGRESS 전환
- [ ] 현재 시각 표시 + 3초 자동 갱신 (논블로킹)
- [ ] [m] 메인 즉시 이동, [0] 뒤로
- [ ] gtest — `ProductionServiceTest` 전체 통과
- [ ] 빌드 경고 0, 오류 0

---

## 2. 구현 대상 파일

| 파일 | 위치 | 구분 |
|---|---|---|
| `ProductionService.h` | `MVC/Service/` | 신규 |
| `ProductionView.h` | `MVC/View/` | 신규 |
| `SampleOrderSystem.cpp` | `SampleOrderSystem/` | 수정 (5번 연결) |
| `tests/production_service_test.cpp` | `SampleOrderSystem/tests/` | 신규 |

---

## 3. ProductionService 설계

### 3.1 인터페이스

```cpp
class ProductionService {
public:
    explicit ProductionService(AppDB& db) : db_(db) {}

    // P-02: 다음 WAITING 항목 → IN_PROGRESS
    // 반환: true(성공), false(WAITING 없음)
    bool processNext();

    // IN_PROGRESS 항목 (없으면 nullptr) — 단순 조회 (부작용 없음)
    // checkAndComplete()는 render() 내에서만 1회 실행
    const ProductionQueueItem* inProgressItem() const;

    // WAITING 항목 목록 (FIFO: enqueued_at 오름차순 = 내부 저장 순서)
    std::vector<const ProductionQueueItem*> waitingQueue() const;

    // 추정 현재 생산량 (진행률 × actual_qty, 소수점 버림)
    static int currentProduction(const ProductionQueueItem& p);

    // 예상 완료 시각 "YYYY-MM-DD HH:MM" (WAITING이면 "-")
    static std::string estimatedCompletion(const ProductionQueueItem& p);

private:
    AppDB& db_;
};
```

### 3.2 processNext() 구현

```
① inProgressItem() 조회 → nullptr이 아니면 false 반환  (단일 라인: IN_PROGRESS 중복 방지)
② db_.frontWaiting() 조회
③ nullptr이면 false 반환
④ item.started_at = nowStr()
⑤ db_.updateQueueItem(item)
⑥ true 반환
```

> **단일 생산 라인 제약**: IN_PROGRESS 항목이 이미 존재하면 추가 시작 불가.  
> `processNext()` 는 WAITING → IN_PROGRESS 전환 전 IN_PROGRESS 항목 유무를 먼저 확인한다.

### 3.3 currentProduction()

```cpp
static int currentProduction(const ProductionQueueItem& p) {
    if (!p.isInProgress()) return 0;
    double pct = p.progressPct();           // [0, 100] 클램핑 보장
    return static_cast<int>(pct * p.actual_qty / 100.0);
}
```

### 3.4 estimatedCompletion()

```cpp
static std::string estimatedCompletion(const ProductionQueueItem& p) {
    if (p.started_at.empty()) return "-";
    std::time_t start = parseTime(p.started_at);
    if (start == -1) return "-";
    std::time_t done = start + static_cast<std::time_t>(p.total_time * 60.0);
    struct tm tm{};
    localtime_s(&tm, &done);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}
```

---

## 4. ProductionView 설계

### 4.1 인터페이스

```cpp
class ProductionView {
public:
    ProductionView(ProductionService& svc, AppDB& db)
        : svc_(svc), db_(db) {}
    void run();

private:
    void render();
    void renderInProgress() const;
    void renderWaiting() const;

    ProductionService& svc_;
    AppDB&             db_;
};
```

### 4.2 자동 새로고침 구현 (3초 논블로킹)

Windows `WaitForSingleObject`를 활용한 논블로킹 입력 대기:

```cpp
void ProductionView::run() {
    const DWORD REFRESH_MS = 3000;  // 3초
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

    while (true) {
        FlushConsoleInputBuffer(hStdin);  // 잔여 입력 제거 (자동갱신 오작동 방지)
        render();

        // 최대 3초 대기하며 키 입력 확인
        DWORD ret = WaitForSingleObject(hStdin, REFRESH_MS);

        if (ret == WAIT_OBJECT_0) {
            std::string inp = UI::readLine("");
            if (inp == "0") return;
            if (inp == "m" || inp == "M") { UI::goToMain = true; return; }
            if (inp == "s" || inp == "S") {
                if (!svc_.processNext())
                    UI::printError("시작 가능한 대기 작업이 없습니다.");
            }
            // Enter 또는 그 외 → 재렌더링
        }
        // WAIT_TIMEOUT → 3초 경과, 자동 재렌더링
    }
}
```

### 4.3 render() 구현 흐름

```
① UI::clearScreen()
② db_.checkAndComplete()   — 1회만 실행 (inProgressItem/waitingQueue는 단순 조회)
③ 헤더 + 현재 시각 출력
④ renderInProgress()        → 생산 현황 카드 (inProgressItem() const 조회)
⑤ renderWaiting()           → 대기 목록 테이블 (waitingQueue() const 조회)
⑥ 하단 네비게이션
```

> checkAndComplete()는 render()에서만 1회 호출. inProgressItem()·waitingQueue()는 부작용 없는 const 조회.

### 4.4 생산 현황 카드 출력 예시

```
  ─────────────── [ 생산 현황 ]  IN_PROGRESS ───────────────  📅 10:25:30

  주문번호    : ORD-20260508-0002
  시료        : S-002  베타-시료
  고객명      : SK하이닉스
  주문 시각   : 2026-05-08 10:15:00
  생산 시작   : 2026-05-08 10:20:00
  ──────────────────────────────────────────────────────────
  부족분      :  45 ea      실 생산량  :  57 ea
  총 생산시간 :   0.5분     현재 생산량:  22 ea  (38.2%)
  예상 완료   : 2026-05-08 10:50
  ──────────────────────────────────────────────────────────
```

IN_PROGRESS 없을 경우:
```
  ─────────────── [ 생산 현황 ]  IN_PROGRESS ───────────────
  현재 생산 중인 작업이 없습니다.
```

### 4.5 대기 목록 출력 예시

```
  ────────────── [ 대기 주문 ]  WAITING · FIFO ─────────────
  ──────────────────────────────────────────────────────────
   #   주문번호              시료         고객명        수량   대기 시작
  ──────────────────────────────────────────────────────────
   1   ORD-20260508-0005   S-001 알파    삼성전자      25 ea  2026-05-08 11:00
  ──────────────────────────────────────────────────────────
  대기: 1건
```

WAITING 없을 경우: `"대기 중인 주문이 없습니다."` 출력

---

## 5. 메인 메뉴 연결

```cpp
ProductionService prodSvc(db);
ProductionView    prodView(prodSvc, db);

case 5: prodView.run(); UI::goToMain = false; break;
```

---

## 6. gtest 계획 (`tests/production_service_test.cpp`)

```cpp
class ProductionServiceTest : public ::testing::Test {
protected:
    const std::string              path_ = "test_prod.json";
    std::unique_ptr<AppDB>         db_;
    std::unique_ptr<SampleService> sampleSvc_;
    std::unique_ptr<OrderService>  orderSvc_;
    std::unique_ptr<ProductionService> prodSvc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_        = std::make_unique<AppDB>(path_);
        sampleSvc_ = std::make_unique<SampleService>(*db_);
        orderSvc_  = std::make_unique<OrderService>(*db_);
        prodSvc_   = std::make_unique<ProductionService>(*db_);
        sampleSvc_->add("알파", 0.5, 0.88, 0);  // S-001
    }
    void TearDown() override {
        prodSvc_.reset(); orderSvc_.reset();
        sampleSvc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }

    // 생산 큐에 항목 추가 헬퍼
    void enqueueOrder(const std::string& customer, int qty) {
        auto o = orderSvc_->createOrder("S-001", qty, customer);
        orderSvc_->approveOrder(o.id);  // PRODUCING → 큐 등록
    }
};

// TC-PS-01: processNext — WAITING → IN_PROGRESS
TEST_F(ProductionServiceTest, ProcessNext_WaitingToInProgress) {
    enqueueOrder("삼성전자", 5);
    EXPECT_TRUE(prodSvc_->processNext());

    auto* item = prodSvc_->inProgressItem();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(item->isInProgress());
    EXPECT_FALSE(item->started_at.empty());
}

// TC-PS-02: processNext — WAITING 없을 때 false
TEST_F(ProductionServiceTest, ProcessNext_NoWaiting) {
    EXPECT_FALSE(prodSvc_->processNext());
    EXPECT_EQ(nullptr, prodSvc_->inProgressItem());
}

// TC-PS-03: waitingQueue — FIFO 순서
TEST_F(ProductionServiceTest, WaitingQueue_FIFOOrder) {
    enqueueOrder("고객A", 3);
    enqueueOrder("고객B", 5);
    enqueueOrder("고객C", 2);

    auto wq = prodSvc_->waitingQueue();
    ASSERT_EQ(3u, wq.size());
    // FIFO: enqueue 순서 = queue ID 오름차순
    EXPECT_LT(wq[0]->id, wq[1]->id);
    EXPECT_LT(wq[1]->id, wq[2]->id);
}

// TC-PS-04: processNext 후 waitingQueue 갱신
TEST_F(ProductionServiceTest, ProcessNext_UpdatesWaitingQueue) {
    enqueueOrder("고객A", 3);
    enqueueOrder("고객B", 5);

    prodSvc_->processNext();  // 첫 번째 → IN_PROGRESS

    auto wq = prodSvc_->waitingQueue();
    ASSERT_EQ(1u, wq.size());  // 두 번째만 WAITING
}

// TC-PS-05: currentProduction — WAITING이면 0
TEST_F(ProductionServiceTest, CurrentProduction_WaitingIsZero) {
    enqueueOrder("고객A", 5);
    auto wq = prodSvc_->waitingQueue();
    ASSERT_FALSE(wq.empty());
    EXPECT_EQ(0, ProductionService::currentProduction(*wq[0]));
}

// TC-PS-06: currentProduction — IN_PROGRESS 유효 범위 검증
// (avg_production_time=0.5, qty=5 → total_time≈수분 → 방금 시작 직후 진행률 극소)
TEST_F(ProductionServiceTest, CurrentProduction_ValidRange) {
    enqueueOrder("고객A", 5);
    prodSvc_->processNext();
    auto* item = prodSvc_->inProgressItem();
    ASSERT_NE(nullptr, item);
    int cp = ProductionService::currentProduction(*item);
    // 유효 범위: [0, actual_qty] (극소 total_time 케이스도 포함)
    EXPECT_GE(cp, 0);
    EXPECT_LE(cp, item->actual_qty);
}

// TC-PS-07: estimatedCompletion — WAITING이면 "-"
TEST_F(ProductionServiceTest, EstimatedCompletion_Waiting) {
    enqueueOrder("고객A", 5);
    auto wq = prodSvc_->waitingQueue();
    ASSERT_FALSE(wq.empty());
    EXPECT_EQ("-", ProductionService::estimatedCompletion(*wq[0]));
}

// TC-PS-08: estimatedCompletion — IN_PROGRESS이면 시각 문자열
TEST_F(ProductionServiceTest, EstimatedCompletion_InProgress) {
    enqueueOrder("고객A", 5);
    prodSvc_->processNext();
    auto* item = prodSvc_->inProgressItem();
    ASSERT_NE(nullptr, item);
    std::string est = ProductionService::estimatedCompletion(*item);
    EXPECT_NE("-", est);
    EXPECT_EQ(16u, est.size());  // "YYYY-MM-DD HH:MM" (초 생략, feature 문서와 통일)
}

// TC-PS-10 (추가): processNext — IN_PROGRESS 존재 시 재시작 방지
TEST_F(ProductionServiceTest, ProcessNext_BlocksIfInProgress) {
    enqueueOrder("고객A", 3);
    enqueueOrder("고객B", 5);
    EXPECT_TRUE(prodSvc_->processNext());   // 첫 번째 → IN_PROGRESS
    EXPECT_FALSE(prodSvc_->processNext());  // IN_PROGRESS 중 재시작 불가
    // WAITING 항목은 여전히 1건
    EXPECT_EQ(1u, prodSvc_->waitingQueue().size());
}

// TC-PS-09: inProgressItem — 생산 완료 후 nullptr 반환
TEST_F(ProductionServiceTest, InProgressItem_AfterCompletion) {
    enqueueOrder("고객A", 5);
    prodSvc_->processNext();

    // 완료 시뮬레이션
    auto& q = db_->queue();
    q[0].started_at = "2000-01-01 00:00:00";
    db_->updateQueueItem(q[0]);

    // inProgressItem() 내부에서 checkAndComplete() 실행
    auto* item = prodSvc_->inProgressItem();
    EXPECT_EQ(nullptr, item);  // 완료 처리 후 IN_PROGRESS 없음
}
```

**총 10개 테스트 케이스 (ProductionServiceTest)**

---

## 7. Phase 4-6 완료 후 파일 구조

```
MVC/
├── Service/
│   └── ProductionService.h    ← NEW
└── View/
    └── ProductionView.h       ← NEW

SampleOrderSystem/
└── tests/
    └── production_service_test.cpp ← NEW
```

---

## 8. 의존성

```
AppDB (Phase 3)
    └── ProductionService (Phase 4-6)
            └── ProductionView
                    └── WaitForSingleObject (Windows API, 논블로킹 입력)
```

---

## 9. 구현 주의사항

- `WaitForSingleObject`는 콘솔 핸들(`STD_INPUT_HANDLE`)에 대해 동작.  
  단, 입력 버퍼에 이미 데이터가 있으면 즉시 `WAIT_OBJECT_0` 반환.  
  → `FlushConsoleInputBuffer(hStdin)` 을 render() 직전에 호출하여 잔여 입력 제거.
- `readLine("")` 호출 시 프롬프트 없이 줄 읽기.
- 자동 새로고침 간격: **3초** (`REFRESH_MS = 3000`).
- render()마다 `checkAndComplete()` 1회 실행 — 부작용 중복 없음.
- `currentProduction()`은 추정값(소수점 버림)으로, 실제 물리적 생산량이 아님.

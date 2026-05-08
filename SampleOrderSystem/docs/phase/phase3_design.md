# Phase 3 — DB 레이어 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md) · [phase2_design.md](phase2_design.md)  
> 상위 계획: **Phase 3 / 7** — DB 레이어  
> POC 참조: `DataPersistence/DataPersistence/json_db.h` (SampleDB 구조 참고)

---

## 1. 목표

`data.json` 단일 파일로 3개 컬렉션(samples / orders / production_queue)을 관리하는  
`AppDB` 클래스를 구현한다. Phase 4(Service), Phase 5(UI)가 이 클래스를 통해서만 파일에 접근한다.

### 완료 기준
- [ ] `app_db.h` — `AppDB` 클래스 구현 (헤더 온리)
- [ ] `data.json` — 실행 후 파일 생성 확인
- [ ] 영속성 검증 — 프로그램 재실행 후 데이터 유지 확인
- [ ] toJson → saveFile → loadFile → fromJson 왕복 검증
- [ ] gtest — `AppDBTest` 전체 통과
- [ ] 빌드 경고 0, 오류 0

---

## 2. 구현 대상 파일

| 파일 | 구분 | 설명 |
|---|---|---|
| `app_db.h` | 신규 | AppDB 클래스 (헤더 온리) |
| `SampleOrderSystem.vcxproj` | 수정 | `app_db.h` ClInclude 등록 |
| `tests/app_db_test.cpp` | 신규 | AppDB gtest |
| `SampleOrderSystem.cpp` | 수정 | `#include "app_db.h"` 추가 |

---

## 3. data.json 구조

```json
{
  "samples": [
    {
      "id": "S-001",
      "name": "알파-시료",
      "avg_production_time": 30.5,
      "yield_rate": 0.95,
      "stock": 100
    }
  ],
  "orders": [
    {
      "id": "ORD-20260508-0001",
      "sample_id": "S-001",
      "quantity": 30,
      "customer_name": "홍길동",
      "status": "RESERVED",
      "created_at": "2026-05-08 10:00:00"
    }
  ],
  "production_queue": [
    {
      "id": 1,
      "order_id": "ORD-20260508-0001",
      "sample_id": "S-001",
      "shortage": 45,
      "actual_qty": 57,
      "total_time": 2565.0,
      "completed": false,
      "enqueued_at": "2026-05-08 10:05:00",
      "started_at": ""
    }
  ]
}
```

---

## 4. AppDB 클래스 설계

### 4.1 인터페이스

```cpp
class AppDB {
public:
    explicit AppDB(const std::string& path);

    // ── Sample ──────────────────────────────────────────
    Sample               createSample(const std::string& name, double avgTime,
                                      double yieldRate, int stock);
    std::vector<Sample>& samples();
    Sample*              findSample(const std::string& id);
    bool                 updateSample(const Sample& s);

    // ── Order ────────────────────────────────────────────
    Order               createOrder(const std::string& sampleId, int qty,
                                    const std::string& customer);
    std::vector<Order>& orders();
    Order*              findOrder(const std::string& id);
    bool                updateOrder(const Order& o);

    // ── ProductionQueueItem ──────────────────────────────
    ProductionQueueItem  enqueue(const std::string& orderId,
                                 const std::string& sampleId,
                                 int shortage, int actualQty, double totalTime);
    std::vector<ProductionQueueItem>& queue();
    bool                             updateQueueItem(const ProductionQueueItem& p);
    ProductionQueueItem*             frontWaiting();

    // 조회 시 IN_PROGRESS 항목 자동 완료 점검 및 처리
    void checkAndComplete();

private:
    void        load();
    void        save() const;
    std::string nextSampleId()  const;
    std::string nextOrderId()   const;
    int         nextQueueId()   const;

    std::string                      path_;
    std::vector<Sample>              samples_;
    std::vector<Order>               orders_;
    std::vector<ProductionQueueItem> queue_;
};
```

### 4.2 설계 원칙

| 원칙 | 내용 |
|---|---|
| Write-through | 모든 변경 즉시 `data.json` 저장 |
| In-memory cache | 시작 시 전체 로드, 이후 메모리 우선 |
| 단일 파일 | samples / orders / production_queue 모두 data.json 하나에 저장 |
| 파일 없으면 빈 상태 | `JsonValue::loadFile`이 없으면 빈 Object 반환하므로 자동 처리 |

---

## 5. ID 생성 규칙

### 5.1 Sample ID — `S-NNN`

```cpp
std::string AppDB::nextSampleId() const {
    int maxN = 0;
    for (const auto& s : samples_) {
        int n = 0;
        sscanf_s(s.id.c_str(), "S-%d", &n);
        maxN = std::max(maxN, n);
    }
    return formatSampleId(maxN + 1);
}
```

- 기존 ID의 최대 번호 + 1
- 삭제 후 재생성 시 번호 재사용 없음 (항상 증가)
- 예: 기존 `S-001`, `S-003` 있으면 → `S-004`

### 5.2 Order ID — `ORD-YYYYMMDD-XXXX`

```cpp
std::string AppDB::nextOrderId() const {
    std::string today  = todayStr();              // "YYYYMMDD"
    std::string prefix = "ORD-" + today + "-";
    int maxSeq = 0;
    for (const auto& o : orders_) {
        if (o.id.substr(0, prefix.size()) == prefix) {
            int n = 0;
            sscanf_s(o.id.c_str() + prefix.size(), "%d", &n);
            maxSeq = std::max(maxSeq, n);
        }
    }
    return formatOrderId(today, maxSeq + 1);
}
```

- 당일 주문 중 **가장 큰 순번 + 1** 을 사용 (count 기반이 아닌 max 기반)
  - count 기반은 삭제 후 재생성 시 ID 충돌 위험 → max 기반으로 방지
  - 현재 설계에 삭제 기능은 없으나 방어적으로 max 방식 채택
- 날짜가 바뀌면 순번 초기화 (새 날짜 기준 재카운팅)
- 예: `ORD-20260508-0001`, `ORD-20260508-0002`, ...

### 5.3 Queue ID — 정수 순번

```cpp
int AppDB::nextQueueId() const {
    int maxId = 0;
    for (const auto& p : queue_) maxId = std::max(maxId, p.id);
    return maxId + 1;
}
```

---

## 6. 핵심 구현 상세

### 6.1 load() / save()

```cpp
void AppDB::load() {
    auto root = JsonValue::loadFile(path_);

    if (root.contains("samples"))
        for (const auto& j : root["samples"].arr)
            samples_.push_back(Sample::fromJson(j));

    if (root.contains("orders"))
        for (const auto& j : root["orders"].arr)
            orders_.push_back(Order::fromJson(j));

    if (root.contains("production_queue"))
        for (const auto& j : root["production_queue"].arr)
            queue_.push_back(ProductionQueueItem::fromJson(j));
}

void AppDB::save() const {
    auto root = JsonValue::makeObject();

    auto sarr = JsonValue::makeArray();
    for (const auto& s : samples_) sarr.push(s.toJson());
    root["samples"] = sarr;

    auto oarr = JsonValue::makeArray();
    for (const auto& o : orders_) oarr.push(o.toJson());
    root["orders"] = oarr;

    auto qarr = JsonValue::makeArray();
    for (const auto& p : queue_) qarr.push(p.toJson());
    root["production_queue"] = qarr;

    root.saveFile(path_);
}
```

### 6.2 checkAndComplete() — IN_PROGRESS 자동 완료

```
순서:
  1. queue_ 순회
  2. isTimeElapsed() == true 인 항목 발견   ← IN_PROGRESS 항목만 해당
  3. completed = true
  4. 연결된 Sample의 stock += actual_qty
  5. 연결된 Order의 status = CONFIRMED
  6. save()
```

> **WAITING 항목은 `checkAndComplete()` 대상이 아니다.**  
> `isTimeElapsed()`는 내부적으로 `isInProgress()`를 포함하므로, `started_at == ""`인 WAITING 항목은 아무리 오래되어도 자동 완료되지 않는다.  
> WAITING → IN_PROGRESS 전환(`started_at = nowStr()` 기록)은 **Phase 4의 `ProductionService::processNext()`** 가 담당한다.

이 메서드는 `queue()` / `frontWaiting()` 호출 시 자동 실행되도록 설계한다.

```cpp
void AppDB::checkAndComplete() {
    bool changed = false;
    for (auto& p : queue_) {
        if (!p.isTimeElapsed()) continue;
        p.completed = true;
        if (auto* s = findSample(p.sample_id)) {
            s->stock += p.actual_qty;
        }
        if (auto* o = findOrder(p.order_id)) {
            o->status = OrderStatus::CONFIRMED;
        }
        changed = true;
    }
    if (changed) save();
}
```

### 6.3 frontWaiting() — FIFO 첫 번째 WAITING 항목

```cpp
ProductionQueueItem* AppDB::frontWaiting() {
    checkAndComplete();
    for (auto& p : queue_)
        if (p.isWaiting()) return &p;
    return nullptr;
}
```

### 6.4 enqueue()

```cpp
ProductionQueueItem AppDB::enqueue(const std::string& orderId,
                                   const std::string& sampleId,
                                   int shortage, int actualQty, double totalTime)
{
    ProductionQueueItem p;
    p.id          = nextQueueId();
    p.order_id    = orderId;
    p.sample_id   = sampleId;
    p.shortage    = shortage;
    p.actual_qty  = actualQty;
    p.total_time  = totalTime;
    p.completed   = false;
    p.enqueued_at = nowStr();
    p.started_at  = "";          // WAITING 상태로 시작
    queue_.push_back(p);
    save();
    return p;
}
```

---

## 7. 수동 검증 시나리오

### 7.1 data.json 생성 확인

```
AppDB db("data.json");
db.createSample("알파-시료", 30.5, 0.95, 100);
→ data.json 파일 생성 확인
→ JSON 내용 육안 검증
```

### 7.2 영속성 검증 (재실행)

```
1회 실행: createSample("알파-시료", ...) → 저장
2회 실행: AppDB db("data.json") → samples() 조회
         → "알파-시료" 데이터 유지 확인
```

### 7.3 toJson → saveFile → loadFile → fromJson 왕복

```
Sample s = db.createSample("테스트", 45.0, 0.88, 50);
→ data.json 저장
AppDB db2("data.json");
Sample* s2 = db2.findSample(s.id);
→ s와 s2 모든 필드 일치 확인
```

### 7.4 Order ID 일련번호 확인

```
Order o1 = db.createOrder("S-001", 10, "홍길동");  // ORD-20260508-0001
Order o2 = db.createOrder("S-001", 20, "김철수");  // ORD-20260508-0002
→ ID 형식 및 순번 확인
```

### 7.5 생산 큐 자동 완료 확인

```
ProductionQueueItem p = db.enqueue("ORD-...", "S-001", 45, 57, 0.0001);
// total_time = 0.0001분 (즉시 완료되도록)
std::this_thread::sleep_for(std::chrono::seconds(1));
db.checkAndComplete();
→ p.completed == true
→ 관련 Order.status == CONFIRMED
→ Sample.stock 증가 확인
```

---

## 8. gtest 검증 계획

### 8.1 환경

| 항목 | 내용 |
|---|---|
| 테스트 파일 | `tests/app_db_test.cpp` |
| 임시 DB 경로 | `test_data.json` (테스트 후 삭제) |
| 공통 설정 | `SetUp()`에서 임시 파일 사용, `TearDown()`에서 삭제 |

**Fixture 구조**

```cpp
class AppDBTest : public ::testing::Test {
protected:
    const std::string path_ = "test_data.json";
    // SetUp: 이전 테스트 런의 잔여 파일 제거 (비정상 종료 대비)
    void SetUp()    override { std::remove(path_.c_str()); }
    void TearDown() override { std::remove(path_.c_str()); }
};
```

### 8.2 테스트 케이스

```cpp
// TC-DB-01: 신규 파일 생성 — 빈 컬렉션으로 시작
TEST_F(AppDBTest, NewFile_EmptyCollections) {
    AppDB db(path_);
    EXPECT_TRUE(db.samples().empty());
    EXPECT_TRUE(db.orders().empty());
    EXPECT_TRUE(db.queue().empty());
}

// TC-DB-02: Sample 생성 및 ID 형식 확인
TEST_F(AppDBTest, CreateSample_IdFormat) {
    AppDB db(path_);
    auto s = db.createSample("알파-시료", 30.5, 0.95, 100);
    EXPECT_EQ("S-001", s.id);
    EXPECT_EQ("알파-시료", s.name);
    EXPECT_DOUBLE_EQ(30.5, s.avg_production_time);
    EXPECT_DOUBLE_EQ(0.95, s.yield_rate);
    EXPECT_EQ(100, s.stock);
}

// TC-DB-03: Sample ID 순번 증가
TEST_F(AppDBTest, CreateSample_IdSequence) {
    AppDB db(path_);
    auto s1 = db.createSample("알파", 10.0, 0.9, 0);
    auto s2 = db.createSample("베타", 20.0, 0.8, 0);
    EXPECT_EQ("S-001", s1.id);
    EXPECT_EQ("S-002", s2.id);
}

// TC-DB-04: Sample findById
TEST_F(AppDBTest, FindSample_ExistingId) {
    AppDB db(path_);
    db.createSample("알파-시료", 30.5, 0.95, 100);
    auto* s = db.findSample("S-001");
    ASSERT_NE(nullptr, s);
    EXPECT_EQ("알파-시료", s->name);
}

// TC-DB-05: Sample findById — 없는 ID
TEST_F(AppDBTest, FindSample_NotFound) {
    AppDB db(path_);
    EXPECT_EQ(nullptr, db.findSample("S-999"));
}

// TC-DB-06: Sample 영속성 — 재로드 후 데이터 유지
TEST_F(AppDBTest, Sample_Persistence) {
    {
        AppDB db(path_);
        db.createSample("알파-시료", 30.5, 0.95, 100);
    }
    AppDB db2(path_);
    ASSERT_EQ(1u, db2.samples().size());
    EXPECT_EQ("S-001",     db2.samples()[0].id);
    EXPECT_EQ("알파-시료", db2.samples()[0].name);
    EXPECT_DOUBLE_EQ(30.5, db2.samples()[0].avg_production_time);
}

// TC-DB-07: Sample 업데이트 후 영속성
TEST_F(AppDBTest, UpdateSample_Persistence) {
    AppDB db(path_);
    db.createSample("알파-시료", 30.5, 0.95, 100);
    auto* s = db.findSample("S-001");
    s->stock = 200;
    db.updateSample(*s);

    AppDB db2(path_);
    auto* s2 = db2.findSample("S-001");
    ASSERT_NE(nullptr, s2);
    EXPECT_EQ(200, s2->stock);
}

// TC-DB-08: Order 생성 및 ID 형식 확인
TEST_F(AppDBTest, CreateOrder_IdFormat) {
    AppDB db(path_);
    db.createSample("알파-시료", 30.5, 0.95, 100);
    auto o = db.createOrder("S-001", 30, "홍길동");

    std::string today = todayStr();
    std::string expectedPrefix = "ORD-" + today + "-";
    EXPECT_EQ(0u, o.id.find(expectedPrefix));
    EXPECT_EQ("S-001",   o.sample_id);
    EXPECT_EQ(30,        o.quantity);
    EXPECT_EQ("홍길동",  o.customer_name);
    EXPECT_EQ(OrderStatus::RESERVED, o.status);
}

// TC-DB-09: Order ID 당일 순번
TEST_F(AppDBTest, CreateOrder_DailySequence) {
    AppDB db(path_);
    db.createSample("알파", 10.0, 0.9, 0);
    auto o1 = db.createOrder("S-001", 10, "고객A");
    auto o2 = db.createOrder("S-001", 20, "고객B");

    std::string today = todayStr();
    EXPECT_EQ("ORD-" + today + "-0001", o1.id);
    EXPECT_EQ("ORD-" + today + "-0002", o2.id);
}

// TC-DB-10: Order 영속성
TEST_F(AppDBTest, Order_Persistence) {
    {
        AppDB db(path_);
        db.createSample("알파", 10.0, 0.9, 0);
        db.createOrder("S-001", 10, "홍길동");
    }
    AppDB db2(path_);
    ASSERT_EQ(1u, db2.orders().size());
    EXPECT_EQ("홍길동", db2.orders()[0].customer_name);
    EXPECT_EQ(OrderStatus::RESERVED, db2.orders()[0].status);
}

// TC-DB-11: enqueue — 기본 생성 및 WAITING 상태
TEST_F(AppDBTest, Enqueue_WaitingState) {
    AppDB db(path_);
    auto p = db.enqueue("ORD-001", "S-001", 45, 57, 2565.0);
    EXPECT_EQ(1,       p.id);
    EXPECT_EQ("S-001", p.sample_id);
    EXPECT_EQ(45,      p.shortage);
    EXPECT_EQ(57,      p.actual_qty);
    EXPECT_FALSE(p.completed);
    EXPECT_EQ("",  p.started_at);
    EXPECT_TRUE(p.isWaiting());
}

// TC-DB-12: frontWaiting — FIFO 순서
TEST_F(AppDBTest, FrontWaiting_Fifo) {
    AppDB db(path_);
    db.enqueue("ORD-001", "S-001", 10, 12, 120.0);
    db.enqueue("ORD-002", "S-002", 20, 23, 230.0);

    auto* front = db.frontWaiting();
    ASSERT_NE(nullptr, front);
    EXPECT_EQ(1, front->id);  // 가장 먼저 enqueue된 항목
}

// TC-DB-13: frontWaiting — 없을 때 nullptr
TEST_F(AppDBTest, FrontWaiting_Empty) {
    AppDB db(path_);
    EXPECT_EQ(nullptr, db.frontWaiting());
}

// TC-DB-14: checkAndComplete — total_time 0 → 즉시 완료
TEST_F(AppDBTest, CheckAndComplete_ImmediateCompletion) {
    AppDB db(path_);
    db.createSample("알파", 10.0, 0.9, 5);
    auto o = db.createOrder("S-001", 50, "홍길동");
    // 주문 상태를 PRODUCING으로 변경
    auto* op = db.findOrder(o.id);
    op->status = OrderStatus::PRODUCING;
    db.updateOrder(*op);

    // total_time = 0 → 즉시 경과
    auto p = db.enqueue(o.id, "S-001", 45, 57, 0.0);
    // started_at 수동 설정 (과거)
    auto* pp = db.queue().data();
    pp->started_at = "2000-01-01 00:00:00";
    db.updateQueueItem(*pp);

    db.checkAndComplete();

    EXPECT_TRUE(db.queue()[0].completed);
    EXPECT_EQ(OrderStatus::CONFIRMED, db.findOrder(o.id)->status);
    EXPECT_EQ(5 + 57, db.findSample("S-001")->stock);
}

// TC-DB-15: Queue 영속성
TEST_F(AppDBTest, Queue_Persistence) {
    {
        AppDB db(path_);
        db.enqueue("ORD-001", "S-001", 10, 12, 120.0);
    }
    AppDB db2(path_);
    ASSERT_EQ(1u, db2.queue().size());
    EXPECT_EQ(1,       db2.queue()[0].id);
    EXPECT_EQ("S-001", db2.queue()[0].sample_id);
    EXPECT_TRUE(db2.queue()[0].isWaiting());
}
```

**총 15개 테스트 케이스**

---

## 9. Phase 3 완료 후 파일 구조

```
SampleOrderSystem/
├── json_lite.h          (Phase 1)
├── models.h             (Phase 2)
├── app_db.h             ← NEW: AppDB 클래스
├── SampleOrderSystem.cpp  ← UPDATE: #include "app_db.h"
├── SampleOrderSystem.vcxproj ← UPDATE: app_db.h 등록
├── tests/
│   ├── models_test.cpp  (Phase 2)
│   └── app_db_test.cpp  ← NEW: AppDB gtest
└── docs/
    └── phase/
        ├── phase1_design.md
        ├── phase2_design.md
        └── phase3_design.md ← 이 파일
```

---

## 10. 의존성

```
json_lite.h  (Phase 1)
    └── models.h  (Phase 2)
            └── app_db.h  (Phase 3)
                    └── SampleService.h  (Phase 4)
                    └── OrderService.h   (Phase 4)
                    └── ProductionService.h (Phase 4)
```

Phase 4 Service 레이어는 `AppDB&` 참조를 통해서만 데이터에 접근한다.  
직접 파일 I/O를 수행하는 코드는 `app_db.h`에만 존재한다.

---

## 11. 주의 사항

- **WAITING 항목 자동 완료 없음**: `checkAndComplete()`는 IN_PROGRESS 항목만 처리. WAITING → IN_PROGRESS 전환은 Phase 4 `processNext()` 담당
- **`checkAndComplete()` nullptr 처리**: `findSample` / `findOrder`가 nullptr 반환 시 조용히 건너뜀 (데이터 불일치 방어)
- **update 메서드 = 전체 교체**: `updateSample` / `updateOrder` / `updateQueueItem`은 id 일치 항목을 **전달받은 객체 전체로 교체**. 부분 업데이트 아님. id 일치 항목 없으면 `false` 반환
- **`queue()` 반환 참조 안정성**: `queue()` 호출 시 `checkAndComplete()` 자동 실행. 이후 반환된 참조/포인터는 다음 `checkAndComplete()` 재호출 전까지 안정적. `queue_` 벡터는 현재 in-place 수정만 수행하므로 반복자 무효화 없음
- **삭제 기능 없음**: Sample / Order 삭제는 PRD 범위 외. OrderStatus::REJECTED로 소프트 처리
- **단일 프로세스 전용**: 동시 접근(멀티스레드, 다중 프로세스) 고려 없음
- **`sscanf_s` MSVC 전용**: `nextSampleId()` / `nextOrderId()` 내 사용. 이식 시 교체 필요

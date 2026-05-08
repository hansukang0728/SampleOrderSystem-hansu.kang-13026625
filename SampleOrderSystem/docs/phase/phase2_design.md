# Phase 2 — 모델 레이어 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md) · [phase1_design.md](phase1_design.md)  
> 상위 계획: **Phase 2 / 7** — 모델 레이어

---

## 1. 목표

시스템의 핵심 도메인 모델 3개를 정의하고 JSON 직렬화/역직렬화를 완성한다.  
이 파일(`models.h`)은 Phase 3(DB), Phase 4(Service), Phase 5(UI) 전체가 의존하는 기반이다.

### 완료 기준
- [ ] `Sample` — 5개 필드, `toJson` / `fromJson` / `stockStatus` 구현
- [ ] `Order` — 6개 필드, `toJson` / `fromJson` / `statusToString` / `stringToStatus` 구현
- [ ] `ProductionQueueItem` — 9개 필드, `toJson` / `fromJson` / 상태 메서드 구현
- [ ] 유틸리티 — `nowStr()`, `todayStr()`, `formatSampleId()`, `formatOrderId()`, `elapsedMinutes()` 구현
- [ ] `tests/models_test.cpp` — gtest 단위 테스트 전체 통과 (PASS)
- [ ] `SampleOrderSystem.cpp` 에서 3개 모델 포함 후 빌드 경고 0, 오류 0
- [ ] 실행 검증 — `toJson` → `dump` → `parse` → `fromJson` 왕복 후 값 일치 확인
  > 파일 I/O(`saveFile`/`loadFile`) 왕복 검증은 Phase 3(AppDB) 에서 수행한다.

---

## 2. 구현 대상 파일

| 파일 | 구분 | 설명 |
|---|---|---|
| `models.h` | 신규 | 도메인 모델 3개 + 유틸리티 함수 |
| `SampleOrderSystem.cpp` | 수정 | `#include "models.h"` 추가 및 검증 코드 |
| `SampleOrderSystem.vcxproj` | 수정 | `models.h` ClInclude 등록 |

---

## 3. 모델 상세 설계

### 3.1 Sample (시료)

**필드 정의**

| 필드 | C++ 타입 | JSON 키 | 형식 / 범위 | 설명 |
|---|---|---|---|---|
| id | `std::string` | `"id"` | `"S-001"` | 고유 식별자 (자동 생성) |
| name | `std::string` | `"name"` | — | 시료명 |
| avg_production_time | `double` | `"avg_production_time"` | > 0, 분/개 | 개당 평균 생산시간 |
| yield_rate | `double` | `"yield_rate"` | 0.0 초과 ~ 1.0 | 수율 |
| stock | `int` | `"stock"` | >= 0, ea | 현재 재고 수량 |

**재고 상태 (`stockStatus`)**

| 반환값 | 조건 | 색상 힌트 |
|---|---|---|
| `"고갈"` | stock == 0 | Red |
| `"부족"` | 1 <= stock <= 10 | Yellow |
| `"여유"` | stock >= 11 | Green |

**구현 코드**

```cpp
struct Sample {
    std::string id;
    std::string name;
    double      avg_production_time = 0.0;
    double      yield_rate          = 0.0;
    int         stock               = 0;

    JsonValue toJson() const {
        auto o = JsonValue::makeObject();
        o["id"]                  = JsonValue(id);
        o["name"]                = JsonValue(name);
        o["avg_production_time"] = JsonValue(avg_production_time);
        o["yield_rate"]          = JsonValue(yield_rate);
        o["stock"]               = JsonValue(stock);
        return o;
    }

    static Sample fromJson(const JsonValue& j) {
        Sample s;
        s.id                  = j["id"].asString();
        s.name                = j["name"].asString();
        s.avg_production_time = j["avg_production_time"].asDouble();
        s.yield_rate          = j["yield_rate"].asDouble();
        s.stock               = j["stock"].asInt();
        return s;
    }

    static std::string stockStatus(int qty) {  // 파라미터명: qty (멤버 stock과 구분)
        if (qty == 0)        return "고갈";
        if (qty <= 10)       return "부족";
        return "여유";
    }
};
```

---

### 3.2 Order (주문)

**필드 정의**

| 필드 | C++ 타입 | JSON 키 | 형식 / 범위 | 설명 |
|---|---|---|---|---|
| id | `std::string` | `"id"` | `"ORD-YYYYMMDD-XXXX"` | 주문번호 (자동 생성) |
| sample_id | `std::string` | `"sample_id"` | `"S-NNN"` | 대상 시료 ID |
| quantity | `int` | `"quantity"` | >= 1, ea | 주문 수량 |
| customer_name | `std::string` | `"customer_name"` | — | 고객명 |
| status | `OrderStatus` | `"status"` | 문자열로 직렬화 | 현재 주문 상태 |
| created_at | `std::string` | `"created_at"` | `"YYYY-MM-DD HH:MM:SS"` | 주문 생성 시각 |

**OrderStatus 열거형**

```cpp
enum class OrderStatus {
    RESERVED,   // 접수 대기
    REJECTED,   // 거절
    PRODUCING,  // 생산 중
    CONFIRMED,  // 출고 대기
    RELEASE     // 출고 완료
};
```

**상태 전이 (참조용)**
```
RESERVED → CONFIRMED  (승인, 재고 충분)
RESERVED → PRODUCING  (승인, 재고 부족)
RESERVED → REJECTED   (거절)
PRODUCING → CONFIRMED (생산 완료)
CONFIRMED → RELEASE   (출고)
```

**구현 코드**

```cpp
enum class OrderStatus { RESERVED, REJECTED, PRODUCING, CONFIRMED, RELEASE };

struct Order {
    std::string id;
    std::string sample_id;
    int         quantity      = 0;  // 기본값 0 = 미초기화 상태; AppDB/Service가 유효값(>=1) 보장
    std::string customer_name;
    OrderStatus status        = OrderStatus::RESERVED;
    std::string created_at;

    static std::string statusToString(OrderStatus s) {
        switch (s) {
        case OrderStatus::RESERVED:  return "RESERVED";
        case OrderStatus::REJECTED:  return "REJECTED";
        case OrderStatus::PRODUCING: return "PRODUCING";
        case OrderStatus::CONFIRMED: return "CONFIRMED";
        case OrderStatus::RELEASE:   return "RELEASE";
        }
        return "UNKNOWN";
    }

    // 알 수 없는 값 → RESERVED 폴백 (의도적 설계: data.json 손상 시 안전 복원)
    static OrderStatus stringToStatus(const std::string& s) {
        if (s == "REJECTED")  return OrderStatus::REJECTED;
        if (s == "PRODUCING") return OrderStatus::PRODUCING;
        if (s == "CONFIRMED") return OrderStatus::CONFIRMED;
        if (s == "RELEASE")   return OrderStatus::RELEASE;
        return OrderStatus::RESERVED;
    }

    JsonValue toJson() const {
        auto o = JsonValue::makeObject();
        o["id"]            = JsonValue(id);
        o["sample_id"]     = JsonValue(sample_id);
        o["quantity"]      = JsonValue(quantity);
        o["customer_name"] = JsonValue(customer_name);
        o["status"]        = JsonValue(statusToString(status));
        o["created_at"]    = JsonValue(created_at);
        return o;
    }

    static Order fromJson(const JsonValue& j) {
        Order o;
        o.id            = j["id"].asString();
        o.sample_id     = j["sample_id"].asString();
        o.quantity      = j["quantity"].asInt();
        o.customer_name = j["customer_name"].asString();
        o.status        = stringToStatus(j["status"].asString());
        o.created_at    = j["created_at"].asString();
        return o;
    }
};
```

---

### 3.3 ProductionQueueItem (생산 큐 항목)

#### 생산 상태 흐름

```
enqueued_at 기록           started_at 기록          completed = true
     │                           │                         │
  WAITING ──(processNext())── IN_PROGRESS ──(경과≥total)── DONE
(started_at 비어있음)      (started_at 채워짐)        (재고·주문 자동 갱신)
```

- **WAITING** : `started_at == ""` && `completed == false`
- **IN_PROGRESS** : `started_at != ""` && `completed == false`
- **DONE** : `completed == true`

조회·모니터링 시마다 IN_PROGRESS 항목의 경과 시간을 확인하여  
`경과 시간 >= total_time` 이면 자동으로 완료 처리한다.

#### 필드 정의

| 필드 | C++ 타입 | JSON 키 | 설명 |
|---|---|---|---|
| id | `int` | `"id"` | 순번 (1부터 자동 증가) |
| order_id | `std::string` | `"order_id"` | 연결된 주문 ID |
| sample_id | `std::string` | `"sample_id"` | 생산할 시료 ID |
| shortage | `int` | `"shortage"` | 부족 수량 (ea) |
| actual_qty | `int` | `"actual_qty"` | 실 생산량 = `ceil(shortage / (yield_rate × 0.9))` |
| total_time | `double` | `"total_time"` | 총 생산시간 (분) = `avg_production_time × actual_qty` |
| completed | `bool` | `"completed"` | 생산 완료 여부 |
| enqueued_at | `std::string` | `"enqueued_at"` | 큐 등록 시각 (`YYYY-MM-DD HH:MM:SS`) |
| **started_at** | `std::string` | `"started_at"` | **실제 생산 시작 시각** (비어있으면 WAITING 상태) |

#### 경과 시간 계산 유틸리티

```cpp
// "YYYY-MM-DD HH:MM:SS" 문자열 → time_t 변환
inline std::time_t parseTime(const std::string& s) {
    struct tm tm{};
    sscanf_s(s.c_str(), "%d-%d-%d %d:%d:%d",
             &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
             &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon  -= 1;
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

// 경과 시간(분) 계산: started_at 기준 현재까지
inline double elapsedMinutes(const std::string& started_at) {
    if (started_at.empty()) return 0.0;
    auto start = parseTime(started_at);
    auto now   = std::chrono::system_clock::to_time_t(
                     std::chrono::system_clock::now());
    return std::difftime(now, start) / 60.0;
}
```

#### 구현 코드

```cpp
struct ProductionQueueItem {
    int         id          = 0;   // 기본값 0 = 미초기화; AppDB가 enqueue 시 유효 순번 부여
    std::string order_id;
    std::string sample_id;
    int         shortage    = 0;
    int         actual_qty  = 0;
    double      total_time  = 0.0;
    bool        completed   = false;
    std::string enqueued_at;
    std::string started_at;        // 비어있음 = WAITING, 채워짐 = IN_PROGRESS

    // 현재 생산 상태
    bool isWaiting()    const { return !completed && started_at.empty(); }
    bool isInProgress() const { return !completed && !started_at.empty(); }
    bool isDone()       const { return completed; }

    // 진행률 (%) — IN_PROGRESS 상태에서만 유효
    double progressPct() const {
        if (!isInProgress() || total_time <= 0.0) return 0.0;
        return std::min(elapsedMinutes(started_at) / total_time * 100.0, 100.0);
    }

    // 완료 여부 판정 (경과 시간 기준)
    bool isTimeElapsed() const {
        return isInProgress() && elapsedMinutes(started_at) >= total_time;
    }

    JsonValue toJson() const {
        auto o = JsonValue::makeObject();
        o["id"]          = JsonValue(id);
        o["order_id"]    = JsonValue(order_id);
        o["sample_id"]   = JsonValue(sample_id);
        o["shortage"]    = JsonValue(shortage);
        o["actual_qty"]  = JsonValue(actual_qty);
        o["total_time"]  = JsonValue(total_time);
        o["completed"]   = JsonValue(completed);
        o["enqueued_at"] = JsonValue(enqueued_at);
        o["started_at"]  = JsonValue(started_at);
        return o;
    }

    static ProductionQueueItem fromJson(const JsonValue& j) {
        ProductionQueueItem p;
        p.id          = j["id"].asInt();
        p.order_id    = j["order_id"].asString();
        p.sample_id   = j["sample_id"].asString();
        p.shortage    = j["shortage"].asInt();
        p.actual_qty  = j["actual_qty"].asInt();
        p.total_time  = j["total_time"].asDouble();
        p.completed   = j["completed"].asBool();
        p.enqueued_at = j["enqueued_at"].asString();
        p.started_at  = j.contains("started_at") ? j["started_at"].asString() : "";
        return p;
    }
};
```

---

## 4. 유틸리티 함수

`models.h` 상단에 공통 유틸리티 함수를 정의한다.

```cpp
// 현재 시각 → "YYYY-MM-DD HH:MM:SS"
inline std::string nowStr() {
    auto t = std::chrono::system_clock::to_time_t(
                 std::chrono::system_clock::now());
    struct tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// Sample ID 포맷 생성: 번호 → "S-NNN"  (실제 번호 부여는 AppDB 담당)
// 완료 기준 명칭: formatSampleId (generate- 접두사는 사용 안 함)
inline std::string formatSampleId(int n) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "S-%03d", n);
    return buf;
}

// Order ID 포맷 생성: "ORD-YYYYMMDD-XXXX"  사용 예: formatOrderId(todayStr(), nextSeq)
inline std::string formatOrderId(const std::string& date, int seq) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "ORD-%s-%04d", date.c_str(), seq);
    return buf;
}

// 오늘 날짜 → "YYYYMMDD"
inline std::string todayStr() {
    auto t = std::chrono::system_clock::to_time_t(
                 std::chrono::system_clock::now());
    struct tm tm{};
    localtime_s(&tm, &t);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y%m%d", &tm);
    return buf;
}
```

---

## 5. models.h 헤더 구성

```cpp
#pragma once
#include <string>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cmath>
#include "json_lite.h"

// ── 유틸리티 ──────────────────────────────────────────────
// nowStr(), todayStr(), formatSampleId(), formatOrderId()

// ── Sample ────────────────────────────────────────────────
// struct Sample { ... };

// ── OrderStatus + Order ───────────────────────────────────
// enum class OrderStatus { ... };
// struct Order { ... };

// ── ProductionQueueItem ───────────────────────────────────
// struct ProductionQueueItem { ... };
```

---

## 6. 검증 시나리오

`SampleOrderSystem.cpp` 의 `main()` 에 임시 검증 코드를 추가하여 왕복 직렬화를 확인한다.  
검증 완료 후 해당 코드는 제거하고 Phase 3으로 진행한다.

### 6.1 Sample 왕복 검증
```
Sample 생성 (id="S-001", name="알파-시료", avg=30.5, yield=0.95, stock=100)
  → toJson()  → dump() 출력
  → parse()   → fromJson()
  → 모든 필드 값 일치 확인
  → stockStatus(100) == "여유" 확인
  → stockStatus(5)   == "부족" 확인
  → stockStatus(0)   == "고갈" 확인
```

### 6.2 Order 왕복 검증
```
Order 생성 (id="ORD-20260508-0001", sample_id="S-001", qty=30,
            customer="홍길동", status=RESERVED, created_at=nowStr())
  → toJson()  → dump() 출력
  → parse()   → fromJson()
  → 모든 필드 값 일치 확인
  → statusToString(PRODUCING) == "PRODUCING" 확인
  → stringToStatus("CONFIRMED") == OrderStatus::CONFIRMED 확인
```

### 6.3 ProductionQueueItem 왕복 검증
```
// 수치 근거: yield_rate=0.88, shortage=45
// actual_qty = ceil(45 / (0.88 × 0.9)) = ceil(45 / 0.792) = ceil(56.82) = 57 ✓
// total_time = avg_production_time(45.0) × actual_qty(57) = 2565.0 ✓

ProductionQueueItem 생성 (id=1, order_id="ORD-20260508-0001", sample_id="S-002",
                          shortage=45, actual_qty=57, total_time=2565.0,
                          completed=false, enqueued_at=nowStr(), started_at="")
  → toJson()  → dump() 출력
  → parse()   → fromJson()
  → 모든 필드 값 일치 확인
  → completed == false, started_at == "" 확인 (WAITING 상태)
  → isWaiting() == true 확인

// IN_PROGRESS 상태 검증
started_at = nowStr() 로 변경
  → isInProgress() == true
  → progressPct() > 0.0
  → isTimeElapsed() == false (방금 시작했으므로)
```

### 6.4 경과 시간 유틸리티 검증
```
elapsedMinutes("") == 0.0  (WAITING 상태 안전 처리)
parseTime(nowStr()) ≈ 현재 time_t (오차 < 2초)
```

### 6.4 유틸리티 검증
```
nowStr()          → "YYYY-MM-DD HH:MM:SS" 형식 출력 확인
todayStr()        → "YYYYMMDD" 형식 출력 확인
formatSampleId(3) → "S-003" 확인
formatOrderId("20260508", 1) → "ORD-20260508-0001" 확인
```

---

## 7. Phase 2 완료 후 파일 구조

```
SampleOrderSystem/
├── json_lite.h              (Phase 1, 변경 없음)
├── models.h                 ← NEW: Sample + Order + ProductionQueueItem
├── SampleOrderSystem.cpp    ← UPDATE: #include "models.h" + 검증→제거
├── SampleOrderSystem.vcxproj ← UPDATE: models.h ClInclude 등록
└── docs/
    └── phase/
        ├── phase1_design.md
        └── phase2_design.md ← 이 파일
```

---

## 8. 의존성

```
json_lite.h  (Phase 1)
    └── models.h  (Phase 2)
            └── app_db.h  (Phase 3)
                    └── ProductionService.h  (Phase 4)
                    └── MonitorView.h        (Phase 5)
```

Phase 3은 `models.h` 없이 구현 불가하다.

---

## 10. gtest 검증 계획

---

### 10.1 환경

| 항목 | 내용 |
|---|---|
| 프레임워크 | Google Test + Google Mock 1.11.0 (NuGet: `gmock.1.11.0`) |
| 헤더 | `<gtest/gtest.h>`, `<gmock/gmock.h>` (자동 포함) |
| 테스트 파일 | `SampleOrderSystem/tests/models_test.cpp` |
| 실행 방식 | `SOS_TEST_MODE` 전처리 매크로로 앱/테스트 모드 전환 |

**SampleOrderSystem.cpp 테스트 모드 진입점**

```cpp
#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
int main(int argc, char** argv) {
    InitConsole();
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
#else
int main() { /* 기존 앱 로직 */ }
#endif
```

**vcxproj 테스트 빌드 설정**
- Debug|x64 구성에 `SOS_TEST_MODE` 전처리 매크로 추가
- `tests/models_test.cpp` ClCompile 등록

---

### 10.2 테스트 파일 구조

```
SampleOrderSystem/
└── tests/
    └── models_test.cpp
        ├── SampleTest       (4개 테스트)
        ├── OrderTest        (5개 테스트)
        ├── ProductionQueueItemTest  (7개 테스트)
        └── UtilsTest        (5개 테스트)
```

---

### 10.3 Sample 테스트 (`SampleTest`)

```cpp
#include <gtest/gtest.h>
#include "../models.h"

// TC-S-01: toJson/fromJson 왕복 직렬화
TEST(SampleTest, ToFromJsonRoundTrip) {
    Sample s;
    s.id = "S-001";  s.name = "알파-시료";
    s.avg_production_time = 30.5;
    s.yield_rate = 0.95;  s.stock = 100;

    Sample s2 = Sample::fromJson(s.toJson());

    EXPECT_EQ(s.id,                  s2.id);
    EXPECT_EQ(s.name,                s2.name);
    EXPECT_DOUBLE_EQ(s.avg_production_time, s2.avg_production_time);
    EXPECT_DOUBLE_EQ(s.yield_rate,   s2.yield_rate);
    EXPECT_EQ(s.stock,               s2.stock);
}

// TC-S-02: stockStatus 경계값
TEST(SampleTest, StockStatus_Boundary) {
    EXPECT_EQ("고갈", Sample::stockStatus(0));   // 경계: 고갈
    EXPECT_EQ("부족", Sample::stockStatus(1));   // 경계: 부족 하한
    EXPECT_EQ("부족", Sample::stockStatus(10));  // 경계: 부족 상한
    EXPECT_EQ("여유", Sample::stockStatus(11));  // 경계: 여유 하한
    EXPECT_EQ("여유", Sample::stockStatus(200)); // 일반값
}

// TC-S-03: 기본값 확인
TEST(SampleTest, DefaultValues) {
    Sample s;
    EXPECT_EQ(0.0, s.avg_production_time);
    EXPECT_EQ(0.0, s.yield_rate);
    EXPECT_EQ(0,   s.stock);
}

// TC-S-04: JSON 키 이름 검증
TEST(SampleTest, JsonKeyNames) {
    Sample s;
    s.id = "S-001";  s.name = "테스트";
    s.avg_production_time = 1.0;  s.yield_rate = 0.9;  s.stock = 5;

    auto j = s.toJson();
    EXPECT_TRUE(j.contains("id"));
    EXPECT_TRUE(j.contains("name"));
    EXPECT_TRUE(j.contains("avg_production_time"));
    EXPECT_TRUE(j.contains("yield_rate"));
    EXPECT_TRUE(j.contains("stock"));
}
```

---

### 10.4 Order 테스트 (`OrderTest`)

```cpp
// TC-O-01: toJson/fromJson 왕복
TEST(OrderTest, ToFromJsonRoundTrip) {
    Order o;
    o.id = "ORD-20260508-0001";  o.sample_id = "S-001";
    o.quantity = 30;  o.customer_name = "홍길동";
    o.status = OrderStatus::RESERVED;
    o.created_at = "2026-05-08 10:00:00";

    Order o2 = Order::fromJson(o.toJson());

    EXPECT_EQ(o.id,            o2.id);
    EXPECT_EQ(o.sample_id,     o2.sample_id);
    EXPECT_EQ(o.quantity,      o2.quantity);
    EXPECT_EQ(o.customer_name, o2.customer_name);
    EXPECT_EQ(o.status,        o2.status);
    EXPECT_EQ(o.created_at,    o2.created_at);
}

// TC-O-02: statusToString 전체 상태
TEST(OrderTest, StatusToString_AllValues) {
    EXPECT_EQ("RESERVED",  Order::statusToString(OrderStatus::RESERVED));
    EXPECT_EQ("REJECTED",  Order::statusToString(OrderStatus::REJECTED));
    EXPECT_EQ("PRODUCING", Order::statusToString(OrderStatus::PRODUCING));
    EXPECT_EQ("CONFIRMED", Order::statusToString(OrderStatus::CONFIRMED));
    EXPECT_EQ("RELEASE",   Order::statusToString(OrderStatus::RELEASE));
}

// TC-O-03: stringToStatus 전체 상태
TEST(OrderTest, StringToStatus_AllValues) {
    EXPECT_EQ(OrderStatus::RESERVED,  Order::stringToStatus("RESERVED"));
    EXPECT_EQ(OrderStatus::REJECTED,  Order::stringToStatus("REJECTED"));
    EXPECT_EQ(OrderStatus::PRODUCING, Order::stringToStatus("PRODUCING"));
    EXPECT_EQ(OrderStatus::CONFIRMED, Order::stringToStatus("CONFIRMED"));
    EXPECT_EQ(OrderStatus::RELEASE,   Order::stringToStatus("RELEASE"));
}

// TC-O-04: stringToStatus 알 수 없는 값 → RESERVED 폴백
TEST(OrderTest, StringToStatus_UnknownFallback) {
    EXPECT_EQ(OrderStatus::RESERVED, Order::stringToStatus("INVALID"));
    EXPECT_EQ(OrderStatus::RESERVED, Order::stringToStatus(""));
    EXPECT_EQ(OrderStatus::RESERVED, Order::stringToStatus("reserved")); // 소문자
}

// TC-O-05: statusToString → stringToStatus 왕복
TEST(OrderTest, StatusRoundTrip) {
    auto statuses = { OrderStatus::RESERVED, OrderStatus::REJECTED,
                      OrderStatus::PRODUCING, OrderStatus::CONFIRMED,
                      OrderStatus::RELEASE };
    for (auto s : statuses)
        EXPECT_EQ(s, Order::stringToStatus(Order::statusToString(s)));
}
```

---

### 10.5 ProductionQueueItem 테스트 (`ProductionQueueItemTest`)

```cpp
// TC-P-01: toJson/fromJson 왕복
TEST(ProductionQueueItemTest, ToFromJsonRoundTrip) {
    ProductionQueueItem p;
    p.id = 1;  p.order_id = "ORD-20260508-0001";  p.sample_id = "S-002";
    p.shortage = 45;  p.actual_qty = 57;  p.total_time = 2565.0;
    p.completed = false;
    p.enqueued_at = "2026-05-08 10:20:00";
    p.started_at  = "";

    ProductionQueueItem p2 = ProductionQueueItem::fromJson(p.toJson());

    EXPECT_EQ(p.id,          p2.id);
    EXPECT_EQ(p.order_id,    p2.order_id);
    EXPECT_EQ(p.shortage,    p2.shortage);
    EXPECT_EQ(p.actual_qty,  p2.actual_qty);
    EXPECT_DOUBLE_EQ(p.total_time, p2.total_time);
    EXPECT_EQ(p.completed,   p2.completed);
    EXPECT_EQ(p.started_at,  p2.started_at);
}

// TC-P-02: 생산 상태 3단계 — WAITING
TEST(ProductionQueueItemTest, State_Waiting) {
    ProductionQueueItem p;
    p.started_at = "";  p.completed = false;

    EXPECT_TRUE(p.isWaiting());
    EXPECT_FALSE(p.isInProgress());
    EXPECT_FALSE(p.isDone());
    EXPECT_DOUBLE_EQ(0.0, p.progressPct());
}

// TC-P-03: 생산 상태 3단계 — IN_PROGRESS
TEST(ProductionQueueItemTest, State_InProgress) {
    ProductionQueueItem p;
    p.started_at = "2026-05-08 10:00:00";  p.completed = false;
    p.total_time = 9999.0;  // 아직 완료되지 않을 충분한 시간

    EXPECT_FALSE(p.isWaiting());
    EXPECT_TRUE(p.isInProgress());
    EXPECT_FALSE(p.isDone());
}

// TC-P-04: 생산 상태 3단계 — DONE
TEST(ProductionQueueItemTest, State_Done) {
    ProductionQueueItem p;
    p.completed = true;

    EXPECT_FALSE(p.isWaiting());
    EXPECT_FALSE(p.isInProgress());
    EXPECT_TRUE(p.isDone());
    EXPECT_DOUBLE_EQ(0.0, p.progressPct());
}

// TC-P-05: isTimeElapsed — 과거 시각으로 완료 판정
TEST(ProductionQueueItemTest, IsTimeElapsed_PastStart) {
    ProductionQueueItem p;
    p.started_at = "2000-01-01 00:00:00";  // 충분히 오래된 과거
    p.total_time = 1.0;                    // 1분 (이미 경과)
    p.completed  = false;

    EXPECT_TRUE(p.isTimeElapsed());
}

// TC-P-06: isTimeElapsed — 방금 시작했으면 false
TEST(ProductionQueueItemTest, IsTimeElapsed_JustStarted) {
    ProductionQueueItem p;
    p.started_at = nowStr();      // 현재 시각
    p.total_time = 999999.0;     // 매우 긴 생산 시간
    p.completed  = false;

    EXPECT_FALSE(p.isTimeElapsed());
}

// TC-P-07: fromJson — started_at 키 없는 경우 빈 문자열로 역직렬화 (하위 호환)
TEST(ProductionQueueItemTest, FromJson_MissingStartedAt) {
    auto j = JsonValue::makeObject();
    j["id"]          = JsonValue(1);
    j["order_id"]    = JsonValue(std::string("ORD-20260508-0001"));
    j["sample_id"]   = JsonValue(std::string("S-001"));
    j["shortage"]    = JsonValue(10);
    j["actual_qty"]  = JsonValue(13);
    j["total_time"]  = JsonValue(390.0);
    j["completed"]   = JsonValue(false);
    j["enqueued_at"] = JsonValue(std::string("2026-05-08 10:00:00"));
    // started_at 키 의도적 누락

    ProductionQueueItem p = ProductionQueueItem::fromJson(j);
    EXPECT_EQ("", p.started_at);
    EXPECT_TRUE(p.isWaiting());
}
```

---

### 10.6 유틸리티 테스트 (`UtilsTest`)

```cpp
// TC-U-01: formatSampleId 형식 검증
TEST(UtilsTest, FormatSampleId) {
    EXPECT_EQ("S-001", formatSampleId(1));
    EXPECT_EQ("S-010", formatSampleId(10));
    EXPECT_EQ("S-100", formatSampleId(100));
    EXPECT_EQ("S-999", formatSampleId(999));
}

// TC-U-02: formatOrderId 형식 검증
TEST(UtilsTest, FormatOrderId) {
    EXPECT_EQ("ORD-20260508-0001", formatOrderId("20260508", 1));
    EXPECT_EQ("ORD-20260508-0010", formatOrderId("20260508", 10));
    EXPECT_EQ("ORD-20260508-9999", formatOrderId("20260508", 9999));
}

// TC-U-03: nowStr 형식 검증 (YYYY-MM-DD HH:MM:SS = 19자)
TEST(UtilsTest, NowStr_Format) {
    std::string s = nowStr();
    ASSERT_EQ(19u, s.size());
    EXPECT_EQ('-', s[4]);   // YYYY-
    EXPECT_EQ('-', s[7]);   // MM-
    EXPECT_EQ(' ', s[10]);  // DD 공백
    EXPECT_EQ(':', s[13]);  // HH:
    EXPECT_EQ(':', s[16]);  // MM:
}

// TC-U-04: todayStr 형식 검증 (YYYYMMDD = 8자)
TEST(UtilsTest, TodayStr_Format) {
    std::string s = todayStr();
    ASSERT_EQ(8u, s.size());
    // 모든 문자가 숫자인지 확인
    for (char c : s)
        EXPECT_TRUE(std::isdigit((unsigned char)c));
}

// TC-U-05: elapsedMinutes — 빈 문자열 입력 시 0.0 반환
TEST(UtilsTest, ElapsedMinutes_Empty) {
    EXPECT_DOUBLE_EQ(0.0, elapsedMinutes(""));
}
```

---

### 10.7 테스트 실행 및 예상 결과

**빌드 방법** (Visual Studio → 프로젝트 속성 → C/C++ → 전처리기에 임시 추가)
```
SOS_TEST_MODE
```

**예상 출력**
```
[==========] Running 21 tests from 4 test suites.
[----------] 4 tests from SampleTest
[ RUN      ] SampleTest.ToFromJsonRoundTrip      ... [ OK ]
[ RUN      ] SampleTest.StockStatus_Boundary     ... [ OK ]
[ RUN      ] SampleTest.DefaultValues            ... [ OK ]
[ RUN      ] SampleTest.JsonKeyNames             ... [ OK ]
[----------] 5 tests from OrderTest
[ RUN      ] OrderTest.ToFromJsonRoundTrip        ... [ OK ]
[ RUN      ] OrderTest.StatusToString_AllValues   ... [ OK ]
[ RUN      ] OrderTest.StringToStatus_AllValues   ... [ OK ]
[ RUN      ] OrderTest.StringToStatus_UnknownFallback ... [ OK ]
[ RUN      ] OrderTest.StatusRoundTrip            ... [ OK ]
[----------] 7 tests from ProductionQueueItemTest
[ RUN      ] ProductionQueueItemTest.ToFromJsonRoundTrip    ... [ OK ]
[ RUN      ] ProductionQueueItemTest.State_Waiting          ... [ OK ]
[ RUN      ] ProductionQueueItemTest.State_InProgress       ... [ OK ]
[ RUN      ] ProductionQueueItemTest.State_Done             ... [ OK ]
[ RUN      ] ProductionQueueItemTest.IsTimeElapsed_PastStart  ... [ OK ]
[ RUN      ] ProductionQueueItemTest.IsTimeElapsed_JustStarted ... [ OK ]
[ RUN      ] ProductionQueueItemTest.FromJson_MissingStartedAt ... [ OK ]
[----------] 5 tests from UtilsTest
[ RUN      ] UtilsTest.FormatSampleId    ... [ OK ]
[ RUN      ] UtilsTest.FormatOrderId     ... [ OK ]
[ RUN      ] UtilsTest.NowStr_Format     ... [ OK ]
[ RUN      ] UtilsTest.TodayStr_Format   ... [ OK ]
[ RUN      ] UtilsTest.ElapsedMinutes_Empty ... [ OK ]
[==========] 21 tests from 4 test suites ran.
[  PASSED  ] 21 tests.
```

---

## 9. 하위 Phase 영향도 (`started_at` 추가에 따른 설계 반영 필요)

| Phase | 영향 항목 | 설계 반영 내용 |
|---|---|---|
| **Phase 3** (AppDB) | `enqueue()` | `started_at = ""` 로 초기화하여 저장 |
| **Phase 4** (ProductionService) | `processNext()` | WAITING → IN_PROGRESS: `started_at = nowStr()` 기록 후 저장 |
| **Phase 4** (ProductionService) | `tick()` 또는 조회 시 | IN_PROGRESS 항목 순회 → `isTimeElapsed()` 확인 → 완료 처리 (`completed=true`, `stock+=actual_qty`, 주문→CONFIRMED`) |
| **Phase 5** (ProductionView) | 생산 큐 출력 | 상태(WAITING/IN_PROGRESS/DONE), 진행률(`progressPct()%`), 잔여 시간 표시 |
| **Phase 5** (MonitorView) | 생산 큐 현황 | IN_PROGRESS 항목의 진행률 및 예상 완료 시각 표시 |

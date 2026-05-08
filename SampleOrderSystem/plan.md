# SampleOrderSystem — 구현 계획 (Implementation Plan)

> 참조: [CLAUDE.md](CLAUDE.md) | [PRD.md](PRD.md) | [docs/solution-projects.md](docs/solution-projects.md)

---

## 1. 아키텍처 결정

### 레이어 구조

```
┌──────────────────────────────────────┐
│           View Layer                 │  콘솔 메뉴 / 화면 출력
│  MainView, SampleView, OrderView,    │
│  MonitorView, ProductionView,        │
│  ReleaseView, ConsoleUI              │
├──────────────────────────────────────┤
│          Service Layer               │  비즈니스 로직
│  SampleService, OrderService,        │
│  ProductionService                   │
├──────────────────────────────────────┤
│            DB Layer                  │  JSON 파일 영속성
│  AppDB (samples + orders + queue)    │
├──────────────────────────────────────┤
│          Model Layer                 │  도메인 모델
│  Sample, Order, ProductionQueueItem  │
├──────────────────────────────────────┤
│        Infrastructure                │  외부 의존성 없음
│  json_lite.h                         │
└──────────────────────────────────────┘
```

### 설계 원칙
- **단일 데이터 파일**: `data.json` 하나에 samples / orders / queue 모두 저장
- **Write-through**: 변경 즉시 파일 저장
- **헤더 온리 최대화**: 소규모 프로젝트이므로 Service·DB를 헤더에 구현
- **POC 재사용**: `json_lite.h` 그대로, UI 패턴은 DataMonitor에서 참고

---

## 2. 파일 구조

```
SampleOrderSystem/
├── CLAUDE.md
├── PRD.md
├── plan.md                       ← 이 파일
├── docs/
│   ├── feature/
│   │   ├── main.md
│   │   ├── sampleManage.md
│   │   ├── sampleOrder.md
│   │   ├── OrderManager.md
│   │   ├── Monitoring.md
│   │   ├── ProductionLine.md
│   │   └── Release.md
│   └── solution-projects.md
│
├── json_lite.h                   ← DataPersistence에서 복사 (변경 없음)
├── models.h                      ← Sample(+stock) / Order / ProductionQueueItem
├── app_db.h                      ← AppDB: 통합 JSON DB (3개 컬렉션)
│
├── service/
│   ├── SampleService.h           ← Sample CRUD + 재고 상태
│   ├── OrderService.h            ← 주문 생성·승인·거절·출고 + 생산큐 등록
│   └── ProductionService.h       ← FIFO 처리 + 생산량/시간 계산
│
├── view/
│   ├── ConsoleUI.h               ← ANSI 색상·테이블·dispWidth 공통 유틸
│   ├── SampleView.h              ← 시료 관리 메뉴
│   ├── OrderView.h               ← 주문 생성 메뉴
│   ├── OrderManagerView.h        ← 주문 승인/거절 메뉴
│   ├── MonitorView.h             ← 모니터링 대시보드
│   ├── ProductionView.h          ← 생산 라인 메뉴
│   └── ReleaseView.h             ← 출고 처리 메뉴
│
└── SampleOrderSystem.cpp         ← main: 객체 조립 + 메인 메뉴 루프
```

---

## 3. 구현 단계

---

### Phase 1 — 프로젝트 기반 설정
**목표**: 빌드 환경 완성, 빈 main 실행 확인

- [ ] `SampleOrderSystem.vcxproj` — `/utf-8`, `NOMINMAX`, C++20 설정
- [ ] `DataPersistence.slnx` — SampleOrderSystem 프로젝트 등록 확인
- [ ] `json_lite.h` — DataPersistence에서 복사
- [ ] 빈 `main()` 빌드 통과 확인

---

### Phase 2 — 모델 레이어 (`models.h`)
**목표**: 3개 도메인 모델 + JSON 직렬화 완성

#### Sample
```cpp
struct Sample {
    std::string id;               // "S-001"
    std::string name;
    double      avg_production_time;  // 분/개
    double      yield_rate;           // 0.0 ~ 1.0
    int         stock;                // ea
    JsonValue toJson() const;
    static Sample fromJson(const JsonValue&);
    static std::string stockStatus(int stock); // "여유"/"부족"/"고갈"
};
```

#### Order
```cpp
enum class OrderStatus { RESERVED, REJECTED, PRODUCING, CONFIRMED, RELEASE };

struct Order {
    std::string id;           // "ORD-YYYYMMDD-XXXX"
    std::string sample_id;
    int         quantity;
    std::string customer_name;
    OrderStatus status;
    std::string created_at;   // "YYYY-MM-DD HH:MM:SS"
    JsonValue toJson() const;
    static Order fromJson(const JsonValue&);
    static std::string statusToString(OrderStatus);
    static OrderStatus stringToStatus(const std::string&);
};
```

#### ProductionQueueItem
```cpp
struct ProductionQueueItem {
    int         id;
    std::string order_id;
    std::string sample_id;
    int         shortage;
    int         actual_qty;
    double      total_time;
    bool        completed;
    std::string enqueued_at;
    JsonValue toJson() const;
    static ProductionQueueItem fromJson(const JsonValue&);
};
```

---

### Phase 3 — DB 레이어 (`app_db.h`)
**목표**: data.json 단일 파일로 3개 컬렉션 관리

```cpp
class AppDB {
public:
    explicit AppDB(const std::string& path);

    // Sample
    Sample              createSample(const std::string& name, double avgTime,
                                     double yieldRate, int stock);
    std::vector<Sample>& samples();
    Sample*             findSample(const std::string& id);
    bool                updateSample(const Sample&);

    // Order
    Order               createOrder(const std::string& sampleId, int qty,
                                    const std::string& customer);
    std::vector<Order>& orders();
    Order*              findOrder(const std::string& id);
    bool                updateOrder(const Order&);

    // ProductionQueueItem
    ProductionQueueItem enqueue(const std::string& orderId,
                                const std::string& sampleId,
                                int shortage, int actualQty, double totalTime);
    std::vector<ProductionQueueItem>& queue();
    bool                             updateQueueItem(const ProductionQueueItem&);
    ProductionQueueItem*             frontPending();  // 미완료 중 가장 오래된 항목

private:
    void load();
    void save() const;

    std::string                      path_;
    std::vector<Sample>              samples_;
    std::vector<Order>               orders_;
    std::vector<ProductionQueueItem> queue_;
    int                              nextQueueId_ = 1;
};
```

**data.json 구조**
```json
{
  "samples": [ ... ],
  "orders":  [ ... ],
  "production_queue": [ ... ]
}
```

**ID 생성 규칙**
| 타입 | 형식 | 예시 |
|---|---|---|
| Sample ID | `S-NNN` (3자리) | `S-001` |
| Order ID | `ORD-YYYYMMDD-XXXX` (당일 4자리 순번) | `ORD-20260508-0001` |
| Queue ID | 정수 순번 | `1`, `2`, ... |

---

### Phase 4 — 서비스 레이어

#### SampleService (`service/SampleService.h`)
| 메서드 | PRD | 설명 |
|---|---|---|
| `add(name, avgTime, yieldRate, stock)` | S-01 | 시료 등록 |
| `all()` | S-02 | 전체 조회 |
| `findById(id)` | S-03 | ID 조회 |
| `searchByName(keyword)` | S-04 | 이름 검색 |
| `stockStatus(stock)` | S-05 | 여유/부족/고갈 반환 |

#### OrderService (`service/OrderService.h`)
| 메서드 | PRD | 설명 |
|---|---|---|
| `createOrder(sampleId, qty, customer)` | O-01 | 주문 생성 → RESERVED |
| `approveOrder(orderId)` | O-02 | 재고 분기 → CONFIRMED or PRODUCING |
| `rejectOrder(orderId)` | O-03 | RESERVED → REJECTED |
| `releaseOrder(orderId)` | O-04 | CONFIRMED → RELEASE |
| `activeOrders()` | O-05 | REJECTED 제외 목록 |
| `reservedOrders()` | O-02 | RESERVED 목록 |
| `confirmedOrders()` | O-04 | CONFIRMED 목록 |

**approveOrder 핵심 로직**
```
재고 >= 수량  →  stock -= qty, status = CONFIRMED
재고 <  수량  →  shortage = qty - stock
                actual_qty = ceil(shortage / (yieldRate × 0.9))
                total_time = avgTime × actual_qty
                db.enqueue(...)
                status = PRODUCING
```

#### ProductionService (`service/ProductionService.h`)
| 메서드 | PRD | 설명 |
|---|---|---|
| `pendingQueue()` | P-01 | 미완료 큐 항목 FIFO 순 반환 |
| `processNext()` | P-02 | 첫 항목 처리: stock+=actualQty, order→CONFIRMED |
| `queueSize()` | P-01 | 대기 건수 |

---

### Phase 5 — UI 레이어

#### ConsoleUI (`view/ConsoleUI.h`)
DataMonitor에서 추출한 공통 유틸:
- ANSI 색상 상수 (`C::CYN`, `C::GRN`, ...)
- `dispWidth(str)` — 한글 표시 폭 계산
- `padR(str, width)` — 표시 폭 기준 오른쪽 패딩
- `rep(str, n)` — 문자열 반복 (구분선용)
- `nowStr()` — 현재시각 문자열
- `printHeader(title)` — 박스 헤더 출력
- `printHLine()` — 구분선 출력
- `clearScreen()` — 화면 지우기

#### 각 View 구현 순서
1. `SampleView.h` — 시료 등록·전체조회·검색 (S-01~S-05)
2. `OrderView.h` — 주문 생성 (O-01)
3. `OrderManagerView.h` — 승인·거절 (O-02, O-03)
4. `ProductionView.h` — 생산 큐 조회·처리 (P-01, P-02)
5. `ReleaseView.h` — 출고 처리 (O-04)
6. `MonitorView.h` — 대시보드 (M-01~M-03)

---

### Phase 6 — 더미 데이터 & 메인 조립

#### 더미 데이터 (`SampleOrderSystem.cpp` 내장)
- DummyDataGenerator 로직 참고
- 메인 메뉴 옵션 또는 실행인수로 트리거
- 시료 10개 기본 생성 (PRD D-01)

#### 메인 루프 (`SampleOrderSystem.cpp`)
```cpp
int main() {
    // 콘솔 초기화 (UTF-8, ANSI)
    AppDB db("data.json");
    SampleService    sampleSvc(db);
    OrderService     orderSvc(db);
    ProductionService prodSvc(db);

    // 메인 메뉴 루프
    while (true) {
        // 번호 입력 → View 호출
    }
}
```

---

### Phase 7 — 검증

| 검증 항목 | 방법 |
|---|---|
| 시료 CRUD | 등록 후 재실행 → 데이터 유지 확인 |
| 주문 승인 (재고 충분) | 재고 충분 시료 주문 → 즉시 CONFIRMED |
| 주문 승인 (재고 부족) | 재고 0 시료 주문 → PRODUCING + 큐 등록 |
| 생산 처리 | processNext → stock 증가 + CONFIRMED 전환 |
| 출고 처리 | CONFIRMED → RELEASE |
| 영속성 | 프로그램 재실행 후 상태 유지 |
| 한글 출력 | 콘솔 한글 정상 표시 |

---

## 4. 구현 순서 요약

```
Phase 1  프로젝트 설정          vcxproj, 빌드 확인
Phase 2  모델 레이어            models.h (Sample, Order, ProductionQueueItem)
Phase 3  DB 레이어              app_db.h (AppDB, data.json 통합)
Phase 4  서비스 레이어          SampleService, OrderService, ProductionService
Phase 5  UI 레이어              ConsoleUI + 6개 View
Phase 6  메인 조립 + 더미데이터  SampleOrderSystem.cpp
Phase 7  검증                   전체 흐름 시나리오 테스트
```

---

## 5. 의존성 그래프

```
json_lite.h
    └── models.h
            └── app_db.h
                    ├── SampleService.h
                    ├── OrderService.h ──── ProductionService.h
                    │
                    ├── ConsoleUI.h
                    ├── SampleView.h
                    ├── OrderView.h
                    ├── OrderManagerView.h
                    ├── ProductionView.h
                    ├── ReleaseView.h
                    └── MonitorView.h
                              │
                    SampleOrderSystem.cpp (main)
```

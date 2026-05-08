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
> 상세 설계: **[docs/phase/phase1_design.md](docs/phase/phase1_design.md)**

**목표**: 빌드 환경 완성, 콘솔 초기화 main 실행 확인

- [x] `SampleOrderSystem.vcxproj` — `/utf-8`, `NOMINMAX`, C++20 설정
- [x] `CRAProject.slnx` — SampleOrderSystem 프로젝트 등록 확인
- [x] `json_lite.h` — DataPersistence에서 복사, vcxproj 등록
- [x] `SampleOrderSystem.cpp` — `InitConsole()` 분리, UTF-8·ANSI 초기화
- [x] Debug x64 빌드 경고 0, 오류 0 통과
- [x] 실행 확인 — `"SampleOrderSystem 초기화 완료"` 출력 정상

---

### Phase 2 — 모델 레이어 (`models.h`)  ✅ 완료
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

### Phase 3 — DB 레이어 (`app_db.h`)  ✅ 완료
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
    ProductionQueueItem*             frontWaiting();  // WAITING 상태 중 가장 오래된 항목

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

### Phase 4 — 서비스·UI 레이어

#### Phase 4-1 — 메인 메뉴 ✅ 완료
#### Phase 4-2 — 시료 관리 ✅ 완료
#### Phase 4-3 — 시료 주문 ✅ 완료
#### 더미 데이터 + 시나리오 테스트 ✅ 완료
#### Phase 4-4 — 주문 승인/거절 ✅ 완료
#### Phase 4-5 — 모니터링 ✅ 완료
#### Phase 4-6 — 생산 라인 ✅ 완료
#### Phase 4-7 — 출고 처리 ✅ 완료

### Phase 5 — UI 레이어  ✅ 완료 (Phase 4와 병행 진행)
> ConsoleUI.h + 각 View(SampleView, OrderView, OrderManagerView, MonitoringView, ProductionView, ReleaseView, DummyDataView)는 Phase 4 서브 단계에서 MVC/View/에 구현됨

### Phase 6 — 메인 조립 + 더미 데이터  ✅ 완료 (Phase 4와 병행 진행)
> SampleOrderSystem.cpp main() 루프, 전광판, 서비스 객체 조립은 Phase 4-1~4-7 진행 중 동시 구현됨  
> DummyDataService / DummyDataView는 Phase 4-3 이후 구현됨

### Phase 7 — 전체 흐름 검증  ✅ 완료
> 설계 문서: [docs/phase/phase4_1_mainmenu_design.md](docs/phase/phase4_1_mainmenu_design.md)

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

| Phase | 내용 | 설계 문서 | 검증 레벨 |
|---|---|---|---|
| Phase 1 | 프로젝트 설정 (vcxproj, json_lite.h, 콘솔 초기화) | [phase1_design.md](docs/phase/phase1_design.md) | 빌드 + 실행 출력 확인 |
| Phase 2 | 모델 레이어 (Sample, Order, ProductionQueueItem) | phase2_design.md | 빌드 + toJson/fromJson 왕복 확인 |
| Phase 3 | DB 레이어 (AppDB, data.json 통합) | phase3_design.md | 빌드 + data.json 생성·재실행 유지 확인 |
| Phase 4 | 서비스 레이어 (SampleService, OrderService, ProductionService) | phase4_design.md | 빌드 + 재고 분기·생산 계산 공식 시나리오 |
| Phase 5 | UI 레이어 (ConsoleUI + 6개 View) | phase5_design.md | 빌드 + 메뉴 출력·입력 흐름 + 기능별 시나리오 |
| Phase 6 | 메인 조립 + 더미 데이터 | phase6_design.md | 빌드 + PRD DoD 전체 시나리오 |
| Phase 7 | 검증 (전체 흐름 시나리오 테스트) | phase7_design.md | 빌드 + 재실행 데이터 유지 + 엣지 케이스 |

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

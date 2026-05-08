# 솔루션 구성 프로젝트 개요

> 이 문서는 `DataPersistence.slnx` 솔루션에 포함된 POC 프로젝트들의 역할과,  
> 각 프로젝트가 **SampleOrderSystem** 구현에 어떻게 기여하는지 정리한다.

---

## 1. 솔루션 구조

```
DataPersistence.slnx
├── DataPersistence/          ← JSON 파일 DB POC
├── DataMonitor/              ← 실시간 파일 감시 POC
├── DummyDataGenerator/       ← 더미 데이터 생성 POC
├── MVC/                      ← MVC 아키텍처 POC
└── SampleOrderSystem/        ← 본 프로젝트 (통합 구현)
```

---

## 2. 프로젝트별 상세

---

### 2.1 DataPersistence

**목적**: JSON 파일 기반 데이터 영속성 레이어 POC

**핵심 파일**

| 파일 | 역할 |
|---|---|
| `json_lite.h` | 외부 라이브러리 없는 경량 JSON 파서 / 직렬화기 (헤더 온리) |
| `models.h` | `Sample` 도메인 모델 + `toJson()` / `fromJson()` |
| `json_db.h` | `SampleDB` 클래스 — 파일 기반 CRUD (create / all / findById / update / remove) |
| `DataPersistence.cpp` | 콘솔 메뉴 기반 CRUD 시연 앱 |

**주요 인터페이스**

```cpp
// json_lite.h
struct JsonValue          // parse(), dump(), loadFile(), saveFile()

// models.h
struct Sample             // id, name, avg_production_time, yield_rate
                          // toJson() / fromJson()

// json_db.h
class SampleDB            // create(), all(), findById(), update(), remove()
```

**data.json 포맷**
```json
{
  "next_id": 3,
  "samples": [
    { "id": 1, "name": "알파-시료", "avg_production_time": 30.5, "yield_rate": 0.95 }
  ]
}
```

**SampleOrderSystem 기여**
- `json_lite.h` → 전체 JSON 직렬화/역직렬화 엔진으로 그대로 채택
- `json_db.h` 설계 → Order, ProductionQueueItem DB 레이어 설계 기준
- `data.json` 포맷 → SampleOrderSystem의 통합 데이터 파일 포맷으로 확장

---

### 2.2 DataMonitor

**목적**: `data.json` 실시간 감시 및 변경 감지 POC

**핵심 파일**

| 파일 | 역할 |
|---|---|
| `DataMonitor.cpp` | Windows `FILETIME` 폴링으로 파일 변경 감지 + ANSI 컬러 대시보드 |
| `json_lite.h` | DataPersistence에서 복사한 JSON 파서 |
| `models.h` | DataPersistence에서 복사한 Sample 모델 |

**주요 구현 패턴**

```cpp
// 파일 변경 감지 (2초 폴링)
FILETIME getFileWriteTime(path)
bool sameTime(FILETIME a, FILETIME b)

// 변경 분류
struct Change { enum class Kind { Added, Modified, Removed }; Sample sample; };
std::vector<Change> diff(prev, curr)
```

**SampleOrderSystem 기여**
- **모니터링 UI 패턴** → ANSI 컬러, 박스 드로잉, `dispWidth()` (한글 표시 폭 계산) 재활용
- **변경 감지 로직** → 모니터링 화면의 실시간 갱신 패턴 참고
- **콘솔 초기화** → `SetConsoleOutputCP(CP_UTF8)` + `ENABLE_VIRTUAL_TERMINAL_PROCESSING` 패턴

---

### 2.3 DummyDataGenerator

**목적**: 테스트용 더미 Sample 데이터 대량 생성 POC

**핵심 파일**

| 파일 | 역할 |
|---|---|
| `DummyDataGenerator.cpp` | `std::mt19937` 난수로 시료 N개 생성 → `data.json` 저장 |
| `json_lite.h` / `models.h` | 자체 포함 복사본 |

**사용법**
```
DummyDataGenerator.exe [count=10] [output_path=..\DataPersistence\data.json]
```

**생성 스펙**
| 필드 | 범위 |
|---|---|
| name | 그리스 문자 풀(20개) + `-시료` 접미사 |
| avg_production_time | 10.0 ~ 120.0분 (균등분포) |
| yield_rate | 0.700 ~ 0.990 (균등분포) |

**SampleOrderSystem 기여**
- **테스트 데이터 공급** → SampleOrderSystem 개발·테스트 시 초기 시료 데이터 생성
- **더미 데이터 기능** → PRD D-01의 `시료 더미 생성` 기능 구현 참고
- **난수 생성 패턴** → `std::mt19937` + `uniform_real_distribution` 패턴 재활용

---

### 2.4 MVC

**목적**: 시료·주문·생산 흐름을 MVC 아키텍처로 구현한 인메모리 POC

**디렉터리 구조**
```
MVC/
├── Model/
│   ├── Sample.h           ← id, name, avgProductionTime, yieldRate, stock
│   ├── Order.h            ← id, sampleId, quantity, customerName, OrderStatus
│   └── ProductionQueue.h  ← ProductionTask, using ProductionQueue = std::queue<...>
├── Controller/
│   ├── SampleController.h/.cpp   ← addSample, getAllSamples, findById, searchByName
│   ├── OrderController.h/.cpp    ← createOrder, approveOrder, rejectOrder, releaseOrder
│   └── ProductionController.h/.cpp ← processNext, getQueueSize, peekNext
├── View/
│   └── MainView.h/.cpp    ← 콘솔 메뉴 UI, 각 Controller 호출
└── MVC.cpp                ← main (Controller 조립, View 실행)
```

**도메인 모델**

```cpp
// Model/Order.h
enum class OrderStatus { RESERVED, REJECTED, PRODUCING, CONFIRMED, RELEASE };

struct Order {
    int id;  int sampleId;  int quantity;
    std::string customerName;  OrderStatus status;
};

// Model/ProductionQueue.h
struct ProductionTask {
    int orderId;  int sampleId;  int shortage;
    int actualProduction;  double totalProductionTime;
};
```

**비즈니스 로직 구현 위치**

| 로직 | 구현 위치 |
|---|---|
| 주문 생성 (RESERVED) | `OrderController::createOrder()` |
| 주문 승인 — 재고 분기 | `OrderController::approveOrder()` |
| 주문 거절 | `OrderController::rejectOrder()` |
| 출고 처리 | `OrderController::releaseOrder()` |
| 생산 큐 FIFO 처리 | `ProductionController::processNext()` |
| 실생산량 / 총생산시간 계산 | `ProductionController::processNext()` 내부 |

**SampleOrderSystem 기여**
- **도메인 모델 기준** → `Order`, `ProductionTask` 구조를 JSON 영속성 포함하도록 확장
- **Controller 비즈니스 로직** → `approveOrder` 재고 분기, `processNext` 생산 계산 공식 직접 참고
- **MVC 구조** → `Controller` 계층 분리 패턴을 SampleOrderSystem 아키텍처에 적용
- **OrderStatus 흐름** → RESERVED → CONFIRMED/PRODUCING → RELEASE 상태 전이 정의

---

## 3. SampleOrderSystem 통합 관계

```
DataPersistence          MVC
   │                      │
   │ json_lite.h          │ Order, ProductionTask 모델
   │ json_db.h 설계       │ Controller 비즈니스 로직
   │ data.json 포맷       │ MVC 구조
   └──────────┬───────────┘
              ▼
      SampleOrderSystem
      (JSON 영속성 + MVC 로직 통합)
              │
              │ 테스트 데이터
   DummyDataGenerator
              │
              │ 모니터링 UI 패턴
         DataMonitor
```

| POC 프로젝트 | SampleOrderSystem에서 채택하는 요소 |
|---|---|
| DataPersistence | `json_lite.h` (그대로), DB 레이어 설계, 파일 포맷 |
| MVC | Order/ProductionTask 모델, Controller 비즈니스 로직, 상태 흐름 |
| DataMonitor | 콘솔 UI 패턴 (ANSI 컬러, 한글 폭 계산, 박스 드로잉) |
| DummyDataGenerator | 더미 데이터 생성 기능, 난수 패턴 |

---

## 4. 공유 컴포넌트

SampleOrderSystem이 직접 재사용하는 파일:

| 파일 | 출처 | 용도 |
|---|---|---|
| `json_lite.h` | DataPersistence | JSON 파싱·저장 전체 엔진 |
| `models.h` (확장) | DataPersistence → 확장 | Sample + stock 필드 추가, Order / ProductionQueueItem 신규 추가 |

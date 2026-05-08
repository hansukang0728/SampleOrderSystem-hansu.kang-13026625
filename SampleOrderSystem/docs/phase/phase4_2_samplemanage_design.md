# Phase 4-2 — 시료 관리 (Design Document)

> 참조: [plan.md](../../plan.md) · [CLAUDE.md](../../CLAUDE.md)  
> Feature 문서: [docs/feature/sampleManage.md](../feature/sampleManage.md)  
> 상위 계획: **Phase 4 / 7** — 서비스·UI 레이어  
> 서브 단계: **4-2** — 시료 관리 (S-01 ~ S-04)

---

## 1. 목표

시료의 등록·전체 조회·ID 조회·이름 검색 기능을 구현한다.  
비즈니스 로직은 `SampleService`, UI는 `SampleView`로 분리하여 MVC 구조를 준수한다.

### 완료 기준
- [ ] `MVC/Service/SampleService.h` — Sample CRUD 비즈니스 로직
- [ ] `MVC/View/SampleView.h` — 시료 관리 콘솔 UI
- [ ] S-01 시료 등록: 입력 → 유효성 검증 → DB 저장 → 결과 출력
- [ ] S-02 전체 조회: 페이지네이션 (10개/페이지, n/p/0 입력으로 이동, 재고 상태 표시 없음)
- [ ] S-03 ID 조회: 단건 상세 출력
- [ ] S-04 이름 검색: 부분 일치 검색 결과 출력
- [ ] 메인 메뉴 1번에서 SampleView 진입 연결
- [ ] gtest — `SampleServiceTest` 전체 통과
- [ ] 빌드 경고 0, 오류 0

---

## 2. 구현 대상 파일

| 파일 | 위치 | 구분 |
|---|---|---|
| `SampleService.h` | `MVC/Service/` | 신규 |
| `SampleView.h` | `MVC/View/` | 신규 |
| `SampleOrderSystem.cpp` | `SampleOrderSystem/` | 수정 (1번 메뉴 연결) |
| `SampleOrderSystem.vcxproj` | `SampleOrderSystem/` | 수정 (헤더 등록) |
| `MVC.vcxproj` | `MVC/` | 수정 (헤더 등록) |

---

## 3. SampleService 설계 (`MVC/Service/SampleService.h`)

### 3.1 인터페이스

```cpp
class SampleService {
public:
    explicit SampleService(AppDB& db) : db_(db) {}

    // S-01: 시료 등록 (유효성 검증은 호출 전 완료 가정)
    Sample add(const std::string& name, double avgTime,
               double yieldRate, int stock);

    // S-02: 전체 조회
    const std::vector<Sample>& all() const;

    // S-03: ID 조회
    Sample* findById(const std::string& id);

    // S-04: 이름 검색 (부분 일치, 대소문자 구분 없음)
    std::vector<const Sample*> searchByName(const std::string& keyword) const;

    // 페이지네이션 — 총 페이지 수 (PAGE_SIZE = 10)
    int totalPages() const;

    static const int PAGE_SIZE = 10;

    // 유효성 검증 (정적 메서드, View에서 호출)
    static bool validateName(const std::string& name);
    static bool validateAvgTime(double t);
    static bool validateYieldRate(double y);
    static bool validateStock(int s);

private:
    AppDB& db_;
};
```

### 3.2 유효성 검증 규칙

| 메서드 | 조건 | 실패 시 |
|---|---|---|
| `validateName` | 비어있지 않음 | false |
| `validateAvgTime` | > 0 | false |
| `validateYieldRate` | 0.0 초과 ~ 1.0 이하 | false |
| `validateStock` | >= 0 | false |

> **책임 분리**: `SampleService`의 `validateXxx()`는 `bool`만 반환한다.  
> 오류 메시지 출력(`"이름을 입력해주세요."` 등)은 **SampleView**가 담당한다.  
> ID 자동 생성(`S-NNN` 최댓값+1)은 **AppDB::createSample()** 레이어에서 처리하며, `SampleService::add()`는 AppDB에 위임만 한다.

### 3.3 searchByName 구현

```cpp
std::vector<const Sample*> SampleService::searchByName(const std::string& keyword) const {
    std::vector<const Sample*> result;
    std::string kw = keyword;
    std::transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
    for (const auto& s : db_.samples()) {
        std::string name = s.name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find(kw) != std::string::npos)
            result.push_back(&s);
    }
    return result;
}
```

> `const Sample*` 반환으로 const_cast 없이 안전하게 처리.  
> 검색 결과는 **출력 전용**이며, `add()` 호출 후 기존 검색 결과 포인터 재사용 금지  
> (벡터 재할당 시 포인터 무효화 가능성).

---

## 4. SampleView 설계 (`MVC/View/SampleView.h`)

### 4.1 인터페이스

```cpp
class SampleView {
public:
    explicit SampleView(SampleService& svc) : svc_(svc) {}
    void run();  // 시료 관리 서브메뉴 루프

private:
    void showMenu();
    void handleAdd();       // S-01
    void handleListAll();   // S-02
    void handleFindById();  // S-03
    void handleSearch();    // S-04
    void printTable(const std::vector<const Sample*>& samples) const;
    void printSample(const Sample& s) const;

    SampleService& svc_;
};
```

### 4.2 서브메뉴 루프

```cpp
void SampleView::run() {
    int choice = -1;
    while (choice != 0) {
        UI::clearScreen();
        UI::printHeader("시료 관리");
        std::cout << "\n"
                  << "  " << UI::CYN << " 1. " << UI::RST << UI::WHT << " 시료 등록\n"
                  << "  " << UI::CYN << " 2. " << UI::RST << UI::WHT << " 전체 조회\n"
                  << "  " << UI::CYN << " 3. " << UI::RST << UI::WHT << " ID 조회\n"
                  << "  " << UI::CYN << " 4. " << UI::RST << UI::WHT << " 이름 검색\n";
        UI::printHLine();
        std::cout << "  " << UI::DIM << " 0. " << UI::RST << UI::GRY << " 뒤로\n";
        UI::printHLine();

        choice = UI::readInt("  선택: ");
        switch (choice) {
        case 1: handleAdd();      break;
        case 2: handleListAll();  break;
        case 3: handleFindById(); break;
        case 4: handleSearch();   break;
        case 0: break;
        default: UI::printError("잘못된 선택입니다."); break;
        }
    }
}
```

### 4.3 handleAdd() — S-01

```
[시료 등록]
이름: [입력]
  → validateName 실패 시 오류 메시지 출력 후 return
평균 생산시간 (분/개): [입력]
  → validateAvgTime 실패 시 오류 메시지 출력 후 return
수율 (0.0~1.0): [입력]
  → validateYieldRate 실패 시 오류 메시지 출력 후 return
초기 재고 (ea): [입력]
  → validateStock 실패 시 오류 메시지 출력 후 return

svc_.add(name, avgTime, yieldRate, stock) → 저장
✔ 등록 완료 — ID: S-001 | 알파-시료
```

**출력 예시**
```
  ╔══════════════════════════════════════════════════════════╗
  ║   시료 등록
  ╚══════════════════════════════════════════════════════════╝

  이름: 알파-시료
  평균 생산시간 (분/개): 30.5
  수율 (0.0~1.0): 0.95
  초기 재고 (ea): 100

  ✔  등록 완료 — ID: S-001 | 알파-시료
```

### 4.4 handleListAll() — S-02 (페이지네이션)

#### 페이지네이션 규칙

| 항목 | 값 |
|---|---|
| 페이지당 표시 수 | 10개 |
| 입력 `n` | 다음 페이지 (마지막 페이지면 무시) |
| 입력 `p` | 이전 페이지 (첫 페이지면 무시) |
| 입력 `0` | 메인 메뉴로 복귀 |
| 그 외 입력 | 무시, 현재 페이지 재표시 |

#### 페이지네이션 구현

```cpp
void SampleView::handleListAll() {
    static const int PAGE_SIZE = 10;
    const auto& all = svc_.all();

    if (all.empty()) {
        UI::printInfo("등록된 시료가 없습니다.");
        UI::waitEnter(); return;
    }

    int totalPages = ((int)all.size() + PAGE_SIZE - 1) / PAGE_SIZE;
    int page = 0;  // 0-based

    while (true) {
        UI::clearScreen();
        UI::printHeader("시료 전체 조회");

        // 현재 페이지 슬라이스 출력
        int start = page * PAGE_SIZE;
        int end   = std::min(start + PAGE_SIZE, (int)all.size());
        std::vector<const Sample*> slice;
        for (int i = start; i < end; ++i) slice.push_back(&all[i]);
        printTable(slice);

        // 페이지 정보 및 네비게이션
        std::cout << "\n"
                  << UI::GRY << "  총 " << UI::WHT << all.size() << "개"
                  << UI::GRY << " | " << UI::CYN << (page + 1) << " / " << totalPages
                  << UI::GRY << " 페이지\n\n" << UI::RST;

        // 가능한 네비게이션 표시
        if (page > 0)               std::cout << UI::YLW << "  [p] 이전 페이지   " << UI::RST;
        if (page < totalPages - 1)  std::cout << UI::YLW << "  [n] 다음 페이지   " << UI::RST;
        std::cout << UI::DIM  << "  [0] 뒤로\n" << UI::RST;
        UI::printHLine();

        std::string input = UI::readLine("  선택: ");
        if      (input == "n" && page < totalPages - 1) ++page;
        else if (input == "p" && page > 0)              --page;
        else if (input == "0")                          break;
        // 그 외 입력은 현재 페이지 재표시
    }
}
```

**출력 예시 (1페이지 / 전체 15개)**
```
  ╔══════════════════════════════════════════════════════════╗
  ║   시료 전체 조회
  ╚══════════════════════════════════════════════════════════╝
  ──────────────────────────────────────────────────────────
   ID      이름                  생산시간(분)   수율(%)  재고(ea)
  ──────────────────────────────────────────────────────────
   S-001   알파-시료                    30.5    95.0       100
   ...
   S-010   이오타-시료                  18.0    92.0        55
  ──────────────────────────────────────────────────────────

  총 15개 | 1 / 2 페이지

  [n] 다음 페이지   [0] 뒤로
```

등록 시료 없을 경우: `"등록된 시료가 없습니다."` 출력 후 Enter 대기

### 4.5 handleFindById() — S-03

```
  시료 ID: S-001
  ──────────────────────────────────────────────────────────
   ID            : S-001
   이름          : 알파-시료
   평균 생산시간 : 30.5 분/개
   수율          : 95.0 %
   재고          : 100 ea
  ──────────────────────────────────────────────────────────
```

존재하지 않는 ID: `"존재하지 않는 시료 ID입니다."` 출력

### 4.6 handleSearch() — S-04

```
  검색어: 알파
  ──────────────────────────────────────────────────────────
   S-001   알파-시료   30.5분   95.0%   재고: 100ea
  ──────────────────────────────────────────────────────────
  1건 검색됨
```

결과 없음: `"검색 결과가 없습니다."` 출력

---

## 5. 메인 메뉴 연결

`SampleOrderSystem.cpp` case 1 수정.  
`SampleService`와 `SampleView`는 `main()`에서 생성하여 재사용한다. (매 진입 시 재생성 불필요)

```cpp
// Before
case 1: UI::printInfo("시료 관리 — 준비 중"); UI::waitEnter(); break;

// After
case 1: sampleView.run(); break;
```

```cpp
int main() {
    InitConsole();
    AppDB         db("data.json");
    SampleService sampleSvc(db);
    SampleView    sampleView(sampleSvc);
    // OrderService, ProductionService ... (Phase 4-3 이후)

    int choice = -1;
    while (choice != 0) {
        printMainMenu(db);
        choice = UI::readInt("  선택: ");
        switch (choice) {
        case 1: sampleView.run(); break;
        // ...
        }
    }
}
```

---

## 6. gtest 계획 (`tests/sample_service_test.cpp`)

```cpp
class SampleServiceTest : public ::testing::Test {
protected:
    const std::string            path_ = "test_sample.json";
    std::unique_ptr<AppDB>       db_;
    std::unique_ptr<SampleService> svc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_  = std::make_unique<AppDB>(path_);
        svc_ = std::make_unique<SampleService>(*db_);
    }
    void TearDown() override {
        svc_.reset();
        db_.reset();
        std::remove(path_.c_str());
    }
};

// TC-SS-01: 시료 등록 후 전체 조회
TEST_F(SampleServiceTest, AddAndListAll) {
    svc_->add("알파-시료", 30.5, 0.95, 100);
    ASSERT_EQ(1u, svc_->all().size());
    EXPECT_EQ("S-001",     svc_->all()[0].id);
    EXPECT_EQ("알파-시료", svc_->all()[0].name);
}

// TC-SS-02: ID 조회
TEST_F(SampleServiceTest, FindById_Exists) {
    svc_->add("알파", 10.0, 0.9, 50);
    auto* s = svc_->findById("S-001");
    ASSERT_NE(nullptr, s);
    EXPECT_EQ("알파", s->name);
}

// TC-SS-03: ID 조회 — 없는 ID
TEST_F(SampleServiceTest, FindById_NotFound) {
    EXPECT_EQ(nullptr, svc_->findById("S-999"));
}

// TC-SS-04: 이름 검색 — 부분 일치
TEST_F(SampleServiceTest, SearchByName_PartialMatch) {
    svc_->add("알파-시료", 10.0, 0.9, 0);
    svc_->add("베타-시료", 20.0, 0.8, 0);
    auto result = svc_->searchByName("알파");
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ("S-001", result[0]->id);
}

// TC-SS-05: 이름 검색 — 결과 없음
TEST_F(SampleServiceTest, SearchByName_NoResult) {
    svc_->add("알파-시료", 10.0, 0.9, 0);
    EXPECT_TRUE(svc_->searchByName("감마").empty());
}

// TC-SS-06: 유효성 검증 (경계값 포함)
TEST_F(SampleServiceTest, Validation) {
    EXPECT_FALSE(SampleService::validateName(""));
    EXPECT_TRUE(SampleService::validateName("테스트"));
    EXPECT_FALSE(SampleService::validateAvgTime(0.0));
    EXPECT_TRUE(SampleService::validateAvgTime(30.5));
    EXPECT_FALSE(SampleService::validateYieldRate(0.0));   // 경계: 초과 조건 실패
    EXPECT_TRUE(SampleService::validateYieldRate(1.0));    // 경계: 이하 조건 통과
    EXPECT_FALSE(SampleService::validateYieldRate(1.1));
    EXPECT_TRUE(SampleService::validateYieldRate(0.95));
    EXPECT_FALSE(SampleService::validateStock(-1));
    EXPECT_TRUE(SampleService::validateStock(0));          // 경계: 0 허용
}

// TC-SS-07: 영속성 — 재로드 후 데이터 유지
// unique_ptr reset() 패턴으로 double-free 방지
TEST_F(SampleServiceTest, Persistence) {
    svc_->add("알파-시료", 30.5, 0.95, 100);
    svc_.reset();
    db_.reset();                                            // 파일에 write-through 완료
    db_  = std::make_unique<AppDB>(path_);
    svc_ = std::make_unique<SampleService>(*db_);
    ASSERT_EQ(1u, svc_->all().size());
    EXPECT_EQ("알파-시료", svc_->all()[0].name);
}
```

// TC-SS-08: 페이지네이션 — totalPages 계산
TEST_F(SampleServiceTest, Pagination_TotalPages) {
    EXPECT_EQ(0, svc_->totalPages());          // 빈 상태
    for (int i = 0; i < 10; ++i)
        svc_->add("시료" + std::to_string(i), 10.0, 0.9, 0);
    EXPECT_EQ(1, svc_->totalPages());          // 딱 10개 = 1페이지
    svc_->add("시료10", 10.0, 0.9, 0);
    EXPECT_EQ(2, svc_->totalPages());          // 11개 = 2페이지
}

**총 8개 테스트 케이스 (SampleServiceTest)**

---

## 7. Phase 4-2 완료 후 파일 구조

```
MVC/
├── Model/
│   ├── json_lite.h
│   ├── models.h
│   └── app_db.h
├── View/
│   ├── ConsoleUI.h        (Phase 4-1)
│   └── SampleView.h       ← NEW
└── Service/
    └── SampleService.h    ← NEW

SampleOrderSystem/
├── SampleOrderSystem.cpp  ← UPDATE: sampleView.run() 연결
└── tests/
    └── sample_service_test.cpp ← NEW
```

---

## 8. 의존성

```
AppDB (Phase 3)
    └── SampleService (Phase 4-2)
            └── SampleView (Phase 4-2)
                    └── SampleOrderSystem.cpp case 1
```

#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "Model/app_db.h"
#include "Service/SampleService.h"
#include "Service/OrderService.h"
#include "Service/DummyDataService.h"

// ════════════════════════════════════════════════════
//  Fixture
// ════════════════════════════════════════════════════

class ScenarioTest : public ::testing::Test {
protected:
    const std::string     path_ = "test_scenario.json";
    std::unique_ptr<AppDB> db_;

    void SetUp()    override { std::remove(path_.c_str()); db_ = std::make_unique<AppDB>(path_); }
    void TearDown() override { db_.reset(); std::remove(path_.c_str()); }

    // AppDB 재시작 (영속성 시나리오용)
    void reloadDb() { db_.reset(); db_ = std::make_unique<AppDB>(path_); }
};

// ════════════════════════════════════════════════════
//  시나리오 1: 시료 등록 → 주문 생성 기본 플로우
// ════════════════════════════════════════════════════

TEST_F(ScenarioTest, S1_SampleRegister_Then_OrderCreate) {
    SampleService sampleSvc(*db_);
    OrderService  orderSvc(*db_);

    // 시료 2개 등록
    auto s1 = sampleSvc.add("알파-시료", 30.5, 0.95, 100);
    auto s2 = sampleSvc.add("베타-시료", 45.0, 0.88, 50);
    ASSERT_EQ(2u, sampleSvc.all().size());

    // 주문 생성
    auto o1 = orderSvc.createOrder(s1.id, 30, "홍길동");
    auto o2 = orderSvc.createOrder(s2.id, 20, "김철수");
    ASSERT_EQ(2u, orderSvc.all().size());

    // 주문 상태 및 재고 변화 없음 확인
    EXPECT_EQ(OrderStatus::RESERVED, o1.status);
    EXPECT_EQ(OrderStatus::RESERVED, o2.status);
    EXPECT_EQ(100, sampleSvc.findById(s1.id)->stock);  // 승인 전 재고 불변
    EXPECT_EQ(50,  sampleSvc.findById(s2.id)->stock);

    // 주문번호 형식 확인
    std::string today = todayStr();
    EXPECT_EQ("ORD-" + today + "-0001", o1.id);
    EXPECT_EQ("ORD-" + today + "-0002", o2.id);
}

// ════════════════════════════════════════════════════
//  시나리오 2: 더미 데이터 생성 → 주문 생성 플로우
// ════════════════════════════════════════════════════

TEST_F(ScenarioTest, S2_DummyData_GenerateSamples_Then_Orders) {
    DummyDataService dummySvc(*db_);
    SampleService    sampleSvc(*db_);
    OrderService     orderSvc(*db_);

    // 시료 5개 더미 생성
    auto samples = dummySvc.generateSamples(5, 42);
    ASSERT_EQ(5u, sampleSvc.all().size());

    // ID 형식 확인
    EXPECT_EQ("S-001", sampleSvc.all()[0].id);
    EXPECT_EQ("S-005", sampleSvc.all()[4].id);

    // 이름 형식 확인 ("XXX-시료")
    for (const auto& s : sampleSvc.all())
        EXPECT_NE(std::string::npos, s.name.find("-시료"));

    // 주문 3건 더미 생성
    auto orders = dummySvc.generateOrders(3, 42);
    ASSERT_EQ(3u, orderSvc.all().size());

    // 주문 상태 모두 RESERVED
    for (const auto& o : orderSvc.all())
        EXPECT_EQ(OrderStatus::RESERVED, o.status);

    // 주문의 시료 ID가 실제 존재하는 시료인지 확인
    for (const auto& o : orderSvc.all())
        EXPECT_NE(nullptr, sampleSvc.findById(o.sample_id));
}

// ════════════════════════════════════════════════════
//  시나리오 3: 영속성 — AppDB 재시작 후 데이터 유지
// ════════════════════════════════════════════════════

TEST_F(ScenarioTest, S3_Persistence_SurvivesRestart) {
    // 데이터 생성
    {
        SampleService sampleSvc(*db_);
        OrderService  orderSvc(*db_);
        sampleSvc.add("알파-시료", 30.5, 0.95, 100);
        sampleSvc.add("베타-시료", 45.0, 0.88, 50);
        orderSvc.createOrder("S-001", 30, "홍길동");
        orderSvc.createOrder("S-002", 10, "김철수");
    }

    // AppDB 재시작
    reloadDb();

    // 재시작 후 데이터 완전히 유지
    SampleService sampleSvc2(*db_);
    OrderService  orderSvc2(*db_);

    ASSERT_EQ(2u, sampleSvc2.all().size());
    EXPECT_EQ("알파-시료", sampleSvc2.all()[0].name);
    EXPECT_EQ(100,          sampleSvc2.all()[0].stock);

    ASSERT_EQ(2u, orderSvc2.all().size());
    EXPECT_EQ("홍길동",           orderSvc2.all()[0].customer_name);
    EXPECT_EQ(OrderStatus::RESERVED, orderSvc2.all()[0].status);
}

// ════════════════════════════════════════════════════
//  시나리오 4: 전체 초기화 후 재생성
// ════════════════════════════════════════════════════

TEST_F(ScenarioTest, S4_ResetAll_Then_Regenerate) {
    DummyDataService dummySvc(*db_);
    SampleService    sampleSvc(*db_);
    OrderService     orderSvc(*db_);

    // 초기 데이터 생성
    dummySvc.generateSamples(10, 1);
    dummySvc.generateOrders(5,  1);
    ASSERT_EQ(10u, sampleSvc.all().size());
    ASSERT_EQ(5u,  orderSvc.all().size());

    // 전체 초기화
    dummySvc.resetAll();
    EXPECT_TRUE(sampleSvc.all().empty());
    EXPECT_TRUE(orderSvc.all().empty());

    // 초기화 후 영속성 확인
    reloadDb();
    SampleService sampleSvc2(*db_);
    EXPECT_TRUE(sampleSvc2.all().empty());

    // 재생성 — ID가 S-001부터 다시 시작
    DummyDataService dummySvc2(*db_);
    dummySvc2.generateSamples(3, 99);
    SampleService sampleSvc3(*db_);
    ASSERT_EQ(3u, sampleSvc3.all().size());
    EXPECT_EQ("S-001", sampleSvc3.all()[0].id);
}

// ════════════════════════════════════════════════════
//  시나리오 5: 검색 시나리오 (다량 데이터 중 특정 검색)
// ════════════════════════════════════════════════════

TEST_F(ScenarioTest, S5_Search_InLargeDataset) {
    DummyDataService dummySvc(*db_);
    SampleService    sampleSvc(*db_);

    // 20개 더미 시료 생성 (그리스 문자 전체 풀)
    dummySvc.generateSamples(20, 7);

    // "알파"로 검색 — "알파-시료" 1건만 일치
    auto r1 = sampleSvc.searchByName("알파");
    EXPECT_EQ(1u, r1.size());
    EXPECT_NE(std::string::npos, r1[0]->name.find("알파"));

    // "시료"로 검색 — 전체 20건 일치
    auto r2 = sampleSvc.searchByName("시료");
    EXPECT_EQ(20u, r2.size());

    // 없는 이름 검색
    auto r3 = sampleSvc.searchByName("ZZZZZZ");
    EXPECT_TRUE(r3.empty());
}

// ════════════════════════════════════════════════════
//  시나리오 6: 페이지네이션 (11개 시료 → 2페이지)
// ════════════════════════════════════════════════════

TEST_F(ScenarioTest, S6_Pagination_MultiPage) {
    DummyDataService dummySvc(*db_);
    SampleService    sampleSvc(*db_);

    dummySvc.generateSamples(11, 3);
    EXPECT_EQ(11u, sampleSvc.all().size());
    EXPECT_EQ(2,   sampleSvc.totalPages());  // 10개/페이지 → 2페이지

    dummySvc.generateSamples(9, 4);  // 추가 9개 → 총 20개
    EXPECT_EQ(20u, sampleSvc.all().size());
    EXPECT_EQ(2,   sampleSvc.totalPages());  // 20개 → 정확히 2페이지

    dummySvc.generateSamples(1, 5);  // 1개 더 → 21개
    EXPECT_EQ(21u, sampleSvc.all().size());
    EXPECT_EQ(3,   sampleSvc.totalPages());  // 21개 → 3페이지
}

// ════════════════════════════════════════════════════
//  시나리오 7: 다수 주문 순번 연속성 확인
// ════════════════════════════════════════════════════

TEST_F(ScenarioTest, S7_OrderSequence_Continuity) {
    SampleService sampleSvc(*db_);
    OrderService  orderSvc(*db_);

    sampleSvc.add("알파", 10.0, 0.9, 100);

    // 주문 5건 생성 후 재시작 → 이어서 5건 더 생성
    for (int i = 0; i < 5; ++i)
        orderSvc.createOrder("S-001", i + 1, "고객" + std::to_string(i));
    ASSERT_EQ(5u, orderSvc.all().size());

    reloadDb();
    OrderService orderSvc2(*db_);
    auto o = orderSvc2.createOrder("S-001", 10, "신규고객");
    std::string today = todayStr();
    EXPECT_EQ("ORD-" + today + "-0006", o.id);  // 순번 이어서 6번
}

// NOTE: Phase 4-4 완료 후 추가 예정
// Scenario 8: 승인 (재고 충분) → CONFIRMED → 출고 → RELEASE
// Scenario 9: 승인 (재고 부족) → PRODUCING → 생산 완료 → CONFIRMED → 출고
// Scenario 10: 주문 거절 → 재고 불변

#endif  // SOS_TEST_MODE

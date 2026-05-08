#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include <string>
#include "Model/app_db.h"
#include "Service/SampleService.h"
#include "Service/OrderService.h"
#include "Service/DummyDataService.h"

// ════════════════════════════════════════════════════
//  Fixture
// ════════════════════════════════════════════════════

class ExtScenarioTest : public ::testing::Test {
protected:
    const std::string      path_ = "test_ext.json";
    std::unique_ptr<AppDB> db_;

    void SetUp()    override { std::remove(path_.c_str()); db_ = std::make_unique<AppDB>(path_); }
    void TearDown() override { db_.reset(); std::remove(path_.c_str()); }

    void reloadDb() { db_.reset(); db_ = std::make_unique<AppDB>(path_); }

    // 헬퍼: 생산 완료 시뮬레이션 (started_at을 과거로 설정 → checkAndComplete)
    void simulateProductionComplete(int queueIdx = 0) {
        auto& q = db_->queue();
        ASSERT_GT(static_cast<int>(q.size()), queueIdx);
        q[queueIdx].started_at = "2000-01-01 00:00:00";
        db_->updateQueueItem(q[queueIdx]);
        db_->checkAndComplete();
    }
};

// ════════════════════════════════════════════════════
//  그룹 1: 경계값 (Boundary Value)
// ════════════════════════════════════════════════════

// BC-01: stock == quantity 정확히 일치 → CONFIRMED, stock = 0
TEST_F(ExtScenarioTest, BC01_StockExactlyEqualsQuantity) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("알파", 0.5, 0.95, 30);
    auto o = oSvc.createOrder("S-001", 30, "삼성전자");
    auto r = oSvc.approveOrder(o.id);
    EXPECT_TRUE(r.success);
    EXPECT_TRUE(r.sufficient);
    EXPECT_EQ(0,  svc.findById("S-001")->stock);  // 정확히 소진
    EXPECT_EQ(OrderStatus::CONFIRMED, oSvc.findById(o.id)->status);
}

// BC-02: stock == quantity - 1 (경계: 1ea 부족) → PRODUCING
TEST_F(ExtScenarioTest, BC02_StockOneLessThanQuantity) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("베타", 0.5, 0.90, 29);
    auto o = oSvc.createOrder("S-001", 30, "SK하이닉스");
    auto r = oSvc.approveOrder(o.id);
    EXPECT_FALSE(r.sufficient);
    EXPECT_EQ(1, r.shortage);  // 30 - 29 = 1
    // ceil(1 / (0.90 × 0.9)) = ceil(1 / 0.81) = ceil(1.234) = 2
    EXPECT_EQ(2, r.actualQty);
    EXPECT_EQ(0, svc.findById("S-001")->stock);  // 기존 재고 소진
}

// BC-03: quantity = 1 (최소 주문) + 재고 충분 → CONFIRMED
TEST_F(ExtScenarioTest, BC03_MinimumQuantity) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("감마", 0.5, 0.95, 100);
    auto o = oSvc.createOrder("S-001", 1, "LG화학");
    auto r = oSvc.approveOrder(o.id);
    EXPECT_TRUE(r.sufficient);
    EXPECT_EQ(99, svc.findById("S-001")->stock);
}

// BC-04: ceil 경계값 — 나눗셈이 정수인 케이스 (오차 없음)
// shortage=9, yield=0.90 → 9/(0.9×0.9) = 9/0.81 = 11.111 → ceil = 12
TEST_F(ExtScenarioTest, BC04_CeilBoundary_NonInteger) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("델타", 1.0, 0.90, 0);
    auto o = oSvc.createOrder("S-001", 9, "현대자동차");
    auto r = oSvc.approveOrder(o.id);
    EXPECT_EQ(12, r.actualQty);
    EXPECT_NEAR(1.0 * 12, r.totalTime, 1e-9);
}

// BC-05: yield_rate 최솟값(0.60) 적용 시 ceil 계산
// shortage=10, yield=0.60 → 10/(0.60×0.9) = 10/0.54 = 18.518 → ceil = 19
TEST_F(ExtScenarioTest, BC05_LowYieldRate_LargeActualQty) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("엡실론", 0.5, 0.60, 0);
    auto o = oSvc.createOrder("S-001", 10, "포스코");
    auto r = oSvc.approveOrder(o.id);
    EXPECT_EQ(19, r.actualQty);
}

// BC-06: 재고 1ea, 주문 1ea → CONFIRMED, stock = 0
TEST_F(ExtScenarioTest, BC06_SingleStock_SingleOrder) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("제타", 0.3, 0.95, 1);
    auto o = oSvc.createOrder("S-001", 1, "한화솔루션");
    auto r = oSvc.approveOrder(o.id);
    EXPECT_TRUE(r.sufficient);
    EXPECT_EQ(0, svc.findById("S-001")->stock);
}

// ════════════════════════════════════════════════════
//  그룹 2: 재고 연속 감소 (Stock Depletion)
// ════════════════════════════════════════════════════

// SD-01: 동일 시료 연속 주문 → 재고 점점 소진 → 마지막 주문은 PRODUCING
TEST_F(ExtScenarioTest, SD01_SequentialOrders_StockDepletes) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("에타", 0.5, 0.95, 25);

    auto o1 = oSvc.createOrder("S-001", 10, "고객A");
    auto o2 = oSvc.createOrder("S-001", 10, "고객B");
    auto o3 = oSvc.createOrder("S-001", 10, "고객C");  // 재고 부족 예상

    oSvc.approveOrder(o1.id);
    EXPECT_EQ(15, svc.findById("S-001")->stock);
    EXPECT_EQ(OrderStatus::CONFIRMED, oSvc.findById(o1.id)->status);

    oSvc.approveOrder(o2.id);
    EXPECT_EQ(5,  svc.findById("S-001")->stock);
    EXPECT_EQ(OrderStatus::CONFIRMED, oSvc.findById(o2.id)->status);

    auto r3 = oSvc.approveOrder(o3.id);
    EXPECT_FALSE(r3.sufficient);
    EXPECT_EQ(5,  r3.shortage);
    EXPECT_EQ(0,  svc.findById("S-001")->stock);
    EXPECT_EQ(OrderStatus::PRODUCING, oSvc.findById(o3.id)->status);
}

// SD-02: CONFIRMED + PRODUCING 혼재 확인
TEST_F(ExtScenarioTest, SD02_MixedConfirmedAndProducing) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("세타",  0.5, 0.95, 20);
    svc.add("이오타", 0.5, 0.88,  0);

    auto o1 = oSvc.createOrder("S-001",  5, "고객A");
    auto o2 = oSvc.createOrder("S-002", 10, "고객B");

    oSvc.approveOrder(o1.id);
    oSvc.approveOrder(o2.id);

    EXPECT_EQ(OrderStatus::CONFIRMED,  oSvc.findById(o1.id)->status);
    EXPECT_EQ(OrderStatus::PRODUCING,  oSvc.findById(o2.id)->status);
    EXPECT_EQ(1u, db_->queue().size());  // 생산 큐 1건
}

// ════════════════════════════════════════════════════
//  그룹 3: 생산 큐 다중 처리 (Production Queue)
// ════════════════════════════════════════════════════

// PQ-01: 다중 PRODUCING 주문 → 큐 FIFO 순서 확인
TEST_F(ExtScenarioTest, PQ01_MultipleProducing_FIFOOrder) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("카파", 0.5, 0.90, 0);

    auto o1 = oSvc.createOrder("S-001", 5, "고객A");
    auto o2 = oSvc.createOrder("S-001", 8, "고객B");
    auto o3 = oSvc.createOrder("S-001", 3, "고객C");

    oSvc.approveOrder(o1.id);
    oSvc.approveOrder(o2.id);
    oSvc.approveOrder(o3.id);

    auto& q = db_->queue();
    ASSERT_EQ(3u, q.size());
    // FIFO: o1이 가장 앞
    EXPECT_EQ(o1.id, q[0].order_id);
    EXPECT_EQ(o2.id, q[1].order_id);
    EXPECT_EQ(o3.id, q[2].order_id);
}

// PQ-02: 생산 완료 후 재고 증가 → 다음 주문 재고 충분
TEST_F(ExtScenarioTest, PQ02_AfterProduction_NextOrderSufficient) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("람다", 0.5, 0.90, 0);

    // 첫 주문: 전량 생산 필요
    auto o1 = oSvc.createOrder("S-001", 5, "고객A");
    auto r1 = oSvc.approveOrder(o1.id);
    EXPECT_FALSE(r1.sufficient);
    int actualQty1 = r1.actualQty;

    // 생산 완료 시뮬레이션
    simulateProductionComplete(0);
    // 재고 = actualQty1
    EXPECT_EQ(actualQty1, svc.findById("S-001")->stock);

    // 두 번째 주문: 이제 재고 충분
    if (actualQty1 >= 3) {
        auto o2 = oSvc.createOrder("S-001", 3, "고객B");
        auto r2 = oSvc.approveOrder(o2.id);
        EXPECT_TRUE(r2.sufficient);
        EXPECT_EQ(actualQty1 - 3, svc.findById("S-001")->stock);
    }
}

// PQ-03: 첫 번째 완료 → 두 번째 자동 IN_PROGRESS (FIFO 자동 시작)
// 자동 시작 로직으로 두 번째 항목은 WAITING이 아닌 IN_PROGRESS가 됨
TEST_F(ExtScenarioTest, PQ03_CompletedItem_SecondAutoStarts) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("뮤", 0.5, 0.90, 0);

    auto o1 = oSvc.createOrder("S-001", 3, "고객A");
    auto o2 = oSvc.createOrder("S-001", 4, "고객B");
    oSvc.approveOrder(o1.id);
    oSvc.approveOrder(o2.id);

    ASSERT_EQ(2u, db_->queue().size());
    // 첫 번째: IN_PROGRESS (자동 시작), 두 번째: WAITING
    EXPECT_TRUE(db_->queue()[0].isInProgress());
    EXPECT_TRUE(db_->queue()[1].isWaiting());

    // 첫 번째 완료 → checkAndComplete가 두 번째 자동 시작
    simulateProductionComplete(0);

    // 두 번째 항목이 IN_PROGRESS로 자동 전환
    EXPECT_TRUE(db_->queue()[1].isInProgress());
    // frontWaiting: 이제 WAITING이 없음
    EXPECT_EQ(nullptr, db_->frontWaiting());
}

// ════════════════════════════════════════════════════
//  그룹 4: Burst 입력
// ════════════════════════════════════════════════════

// BU-01: 주문 20건 순번 연속성
TEST_F(ExtScenarioTest, BU01_Burst_OrderSequence_20Orders) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("뉴", 0.5, 0.95, 1000);

    for (int i = 0; i < 20; ++i)
        oSvc.createOrder("S-001", 1, "고객" + std::to_string(i));

    ASSERT_EQ(20u, oSvc.all().size());
    // 순번 1~20 연속성
    std::string today = todayStr();
    EXPECT_EQ("ORD-" + today + "-0001", oSvc.all()[0].id);
    EXPECT_EQ("ORD-" + today + "-0020", oSvc.all()[19].id);
}

// BU-02: 주문 10건 연속 승인 → 재고 정확 차감
TEST_F(ExtScenarioTest, BU02_Burst_Approve_10Orders_StockCorrect) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("크시", 0.5, 0.95, 100);

    std::vector<std::string> ids;
    for (int i = 0; i < 10; ++i) {
        auto o = oSvc.createOrder("S-001", 5, "고객" + std::to_string(i));
        ids.push_back(o.id);
    }
    for (const auto& id : ids)
        oSvc.approveOrder(id);

    EXPECT_EQ(50, svc.findById("S-001")->stock);  // 100 - 10*5 = 50
    // 모두 CONFIRMED
    for (const auto& id : ids)
        EXPECT_EQ(OrderStatus::CONFIRMED, oSvc.findById(id)->status);
}

// BU-03: Burst 더미 데이터(50시료 + 30주문) → 영속성
TEST_F(ExtScenarioTest, BU03_Burst_DummyData_Persistence) {
    DummyDataService dummySvc(*db_);
    SampleService    svc(*db_);
    OrderService     oSvc(*db_);

    dummySvc.generateSamples(50, 777);
    dummySvc.generateOrders(30,  777);

    EXPECT_EQ(50u, svc.all().size());
    EXPECT_EQ(30u, oSvc.all().size());

    reloadDb();
    SampleService svc2(*db_);
    OrderService  oSvc2(*db_);
    EXPECT_EQ(50u, svc2.all().size());
    EXPECT_EQ(30u, oSvc2.all().size());
    // 모두 RESERVED 상태
    for (const auto& o : oSvc2.all())
        EXPECT_EQ(OrderStatus::RESERVED, o.status);
}

// BU-04: 연속 초기화 반복 후 ID 리셋 확인
TEST_F(ExtScenarioTest, BU04_Burst_ResetAndRegenerate_3Times) {
    DummyDataService dummySvc(*db_);
    SampleService    svc(*db_);

    for (int round = 0; round < 3; ++round) {
        dummySvc.resetAll();
        dummySvc.generateSamples(5, static_cast<unsigned>(round + 1));
        EXPECT_EQ(5u, svc.all().size());
        EXPECT_EQ("S-001", svc.all()[0].id);
        EXPECT_EQ("S-005", svc.all()[4].id);
    }
}

// ════════════════════════════════════════════════════
//  그룹 5: 방어 케이스 (Defensive)
// ════════════════════════════════════════════════════

// DEF-01: 이중 승인 방어 — 두 번째 approveOrder는 실패
TEST_F(ExtScenarioTest, DEF01_DoubleApprove_SecondFails) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("오미크론", 0.5, 0.95, 100);
    auto o = oSvc.createOrder("S-001", 10, "삼성전자");

    auto r1 = oSvc.approveOrder(o.id);
    EXPECT_TRUE(r1.success);

    auto r2 = oSvc.approveOrder(o.id);  // 재승인 시도
    EXPECT_FALSE(r2.success);
    EXPECT_EQ(90, svc.findById("S-001")->stock);  // 재고 두 번 차감되지 않음
}

// DEF-02: CONFIRMED 주문 거절 시도 → 실패
TEST_F(ExtScenarioTest, DEF02_RejectConfirmed_Fails) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("파이", 0.5, 0.95, 100);
    auto o = oSvc.createOrder("S-001", 10, "SK하이닉스");
    oSvc.approveOrder(o.id);  // CONFIRMED

    EXPECT_FALSE(oSvc.rejectOrder(o.id));  // 거절 시도 실패
    EXPECT_EQ(OrderStatus::CONFIRMED, oSvc.findById(o.id)->status);
}

// DEF-03: REJECTED 주문 승인 시도 → 실패
TEST_F(ExtScenarioTest, DEF03_ApproveRejected_Fails) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("로", 0.5, 0.95, 100);
    auto o = oSvc.createOrder("S-001", 10, "LG화학");
    oSvc.rejectOrder(o.id);  // REJECTED

    auto r = oSvc.approveOrder(o.id);  // 승인 시도 실패
    EXPECT_FALSE(r.success);
    EXPECT_EQ(100, svc.findById("S-001")->stock);  // 재고 불변
}

// DEF-04: 이중 거절 방어
TEST_F(ExtScenarioTest, DEF04_DoubleReject_SecondFails) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("시그마", 0.5, 0.95, 100);
    auto o = oSvc.createOrder("S-001", 10, "현대자동차");

    EXPECT_TRUE(oSvc.rejectOrder(o.id));
    EXPECT_FALSE(oSvc.rejectOrder(o.id));  // 이중 거절 실패
}

// DEF-05: 존재하지 않는 주문번호 처리
TEST_F(ExtScenarioTest, DEF05_NonExistentOrder) {
    OrderService oSvc(*db_);
    auto r = oSvc.approveOrder("ORD-00000101-0000");
    EXPECT_FALSE(r.success);
    EXPECT_FALSE(oSvc.rejectOrder("ORD-00000101-0000"));
}

// DEF-06: PRODUCING 주문 이중 승인 방어 (생산 큐 중복 등록 방지)
TEST_F(ExtScenarioTest, DEF06_ApproveProducing_Fails) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("타우", 0.5, 0.88, 0);
    auto o = oSvc.createOrder("S-001", 10, "포스코");
    oSvc.approveOrder(o.id);  // PRODUCING
    EXPECT_EQ(1u, db_->queue().size());

    auto r2 = oSvc.approveOrder(o.id);  // 재승인 시도
    EXPECT_FALSE(r2.success);
    EXPECT_EQ(1u, db_->queue().size());  // 큐 중복 등록 없음
}

// ════════════════════════════════════════════════════
//  그룹 6: 복합 시나리오 (Compound)
// ════════════════════════════════════════════════════

// COMP-01: 주문 혼재 처리 → 상태별 필터 확인
TEST_F(ExtScenarioTest, COMP01_MixedOrderStates_FilterCheck) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("웁실론", 0.5, 0.95, 20);

    auto o1 = oSvc.createOrder("S-001", 5,  "고객A");  // → CONFIRMED
    auto o2 = oSvc.createOrder("S-001", 5,  "고객B");  // → CONFIRMED
    auto o3 = oSvc.createOrder("S-001", 20, "고객C");  // → PRODUCING (재고 부족)
    auto o4 = oSvc.createOrder("S-001", 5,  "고객D");  // RESERVED 유지
    auto o5 = oSvc.createOrder("S-001", 5,  "고객E");  // → REJECTED

    oSvc.approveOrder(o1.id);
    oSvc.approveOrder(o2.id);
    oSvc.approveOrder(o3.id);
    oSvc.rejectOrder(o5.id);

    // RESERVED: o4 1건만
    auto reserved = oSvc.reservedOrders();
    ASSERT_EQ(1u, reserved.size());
    EXPECT_EQ(o4.id, reserved[0]->id);

    // 상태 확인
    EXPECT_EQ(OrderStatus::CONFIRMED,  oSvc.findById(o1.id)->status);
    EXPECT_EQ(OrderStatus::CONFIRMED,  oSvc.findById(o2.id)->status);
    EXPECT_EQ(OrderStatus::PRODUCING,  oSvc.findById(o3.id)->status);
    EXPECT_EQ(OrderStatus::RESERVED,   oSvc.findById(o4.id)->status);
    EXPECT_EQ(OrderStatus::REJECTED,   oSvc.findById(o5.id)->status);
}

// COMP-02: 생산 완료 → 재고 충분 → 이후 주문 즉시 CONFIRMED
TEST_F(ExtScenarioTest, COMP02_Production_Complete_Enables_NextConfirm) {
    SampleService svc(*db_);
    OrderService  oSvc(*db_);
    svc.add("알파-2", 0.5, 0.90, 0);

    // 주문 1: 재고 없음 → PRODUCING
    auto o1 = oSvc.createOrder("S-001", 5, "고객A");
    auto r1 = oSvc.approveOrder(o1.id);
    EXPECT_FALSE(r1.sufficient);

    // 생산 완료 → actual_qty 입고
    simulateProductionComplete(0);
    int stock = svc.findById("S-001")->stock;
    EXPECT_GT(stock, 0);

    // 주문 2: 이제 재고 충분
    int qty2 = std::min(stock, 3);  // 재고보다 적게 주문
    auto o2 = oSvc.createOrder("S-001", qty2, "고객B");
    auto r2 = oSvc.approveOrder(o2.id);
    EXPECT_TRUE(r2.sufficient);
    EXPECT_EQ(stock - qty2, svc.findById("S-001")->stock);
}

// COMP-03: 대량 더미 시료 + 연속 주문 + 일부 승인 후 영속성 확인
TEST_F(ExtScenarioTest, COMP03_Burst_DummyPlus_Approve_Persistence) {
    DummyDataService dummySvc(*db_);
    SampleService    svc(*db_);
    OrderService     oSvc(*db_);

    // 시료 10개, 주문 10건 생성
    dummySvc.generateSamples(10, 42);
    dummySvc.generateOrders(10, 42);
    ASSERT_EQ(10u, svc.all().size());
    ASSERT_EQ(10u, oSvc.all().size());

    // 앞 5건 승인 (어떤 결과든 상관없음)
    auto reserved = oSvc.reservedOrders();
    int approved = 0;
    for (int i = 0; i < 5 && i < static_cast<int>(reserved.size()); ++i) {
        oSvc.approveOrder(reserved[i]->id);
        ++approved;
    }

    // 영속성 확인
    reloadDb();
    SampleService svc2(*db_);
    OrderService  oSvc2(*db_);
    EXPECT_EQ(10u, svc2.all().size());
    EXPECT_EQ(10u, oSvc2.all().size());

    // 승인된 주문은 RESERVED 아님
    auto reserved2 = oSvc2.reservedOrders();
    EXPECT_EQ(static_cast<size_t>(10 - approved), reserved2.size());
}

// COMP-04: 초기화 후 전체 플로우 재시작
TEST_F(ExtScenarioTest, COMP04_ResetAll_Then_FullFlow) {
    DummyDataService dummySvc(*db_);
    SampleService    svc(*db_);
    OrderService     oSvc(*db_);

    // 1단계: 더미 데이터 생성 후 초기화
    dummySvc.generateSamples(5, 1);
    dummySvc.generateOrders(3, 1);
    dummySvc.resetAll();
    EXPECT_TRUE(svc.all().empty());
    EXPECT_TRUE(oSvc.all().empty());

    // 2단계: 새로운 시료·주문 등록
    svc.add("신규-알파", 0.5, 0.95, 50);
    auto o = oSvc.createOrder("S-001", 20, "삼성SDI");
    EXPECT_EQ("S-001", svc.all()[0].id);   // 초기화 후 S-001부터 재시작
    EXPECT_EQ("ORD-" + todayStr() + "-0001", o.id);

    // 3단계: 승인 → CONFIRMED
    auto r = oSvc.approveOrder(o.id);
    EXPECT_TRUE(r.sufficient);
    EXPECT_EQ(30, svc.findById("S-001")->stock);

    // 4단계: 영속성
    reloadDb();
    SampleService svc2(*db_);
    OrderService  oSvc2(*db_);
    EXPECT_EQ(OrderStatus::CONFIRMED, oSvc2.findById(o.id)->status);
    EXPECT_EQ(30, svc2.findById("S-001")->stock);
}

#endif  // SOS_TEST_MODE

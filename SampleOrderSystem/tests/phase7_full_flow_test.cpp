#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "Model/app_db.h"
#include "Service/SampleService.h"
#include "Service/OrderService.h"
#include "Service/MonitoringService.h"
#include "Service/DummyDataService.h"

// ════════════════════════════════════════════════════
//  Fixture
// ════════════════════════════════════════════════════

class FullFlowTest : public ::testing::Test {
protected:
    const std::string path_ = "test_fullflow.json";
    std::unique_ptr<AppDB>             db_;
    std::unique_ptr<SampleService>     sampleSvc_;
    std::unique_ptr<OrderService>      orderSvc_;
    std::unique_ptr<MonitoringService> monitorSvc_;
    std::unique_ptr<DummyDataService>  dummySvc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_         = std::make_unique<AppDB>(path_);
        sampleSvc_  = std::make_unique<SampleService>(*db_);
        orderSvc_   = std::make_unique<OrderService>(*db_);
        monitorSvc_ = std::make_unique<MonitoringService>(*db_);
        dummySvc_   = std::make_unique<DummyDataService>(*db_);
    }
    void TearDown() override {
        dummySvc_.reset(); monitorSvc_.reset();
        orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }

    void reloadAll() {
        dummySvc_.reset(); monitorSvc_.reset();
        orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
        db_         = std::make_unique<AppDB>(path_);
        sampleSvc_  = std::make_unique<SampleService>(*db_);
        orderSvc_   = std::make_unique<OrderService>(*db_);
        monitorSvc_ = std::make_unique<MonitoringService>(*db_);
        dummySvc_   = std::make_unique<DummyDataService>(*db_);
    }

    void simulateComplete(int queueIdx = 0) {
        auto& q = db_->queue();
        ASSERT_GT(static_cast<int>(q.size()), queueIdx);
        q[queueIdx].started_at = "2000-01-01 00:00:00";
        db_->updateQueueItem(q[queueIdx]);
        db_->checkAndComplete();
    }
};

// ════════════════════════════════════════════════════
//  FS-01: 재고 충분 전체 플로우
//  DoD: 시료 CRUD · 주문 생성/승인 · 출고
// ════════════════════════════════════════════════════

TEST_F(FullFlowTest, FS01_FullFlow_StockSufficient) {
    // 시료 등록
    auto s = sampleSvc_->add("알파-시료", 0.5, 0.95, 100);
    ASSERT_EQ("S-001", s.id);
    EXPECT_EQ(100, sampleSvc_->findById("S-001")->stock);

    // 주문 생성 → RESERVED
    auto o = orderSvc_->createOrder("S-001", 30, "삼성전자");
    EXPECT_EQ(OrderStatus::RESERVED, o.status);
    EXPECT_EQ(100, sampleSvc_->findById("S-001")->stock);  // 재고 불변

    // 승인 → 재고 충분 → CONFIRMED
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_TRUE(r.success && r.sufficient);
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(70, sampleSvc_->findById("S-001")->stock);  // 100 - 30

    // 출고 → RELEASE
    EXPECT_TRUE(orderSvc_->releaseOrder(o.id));
    EXPECT_EQ(OrderStatus::RELEASE, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(70, sampleSvc_->findById("S-001")->stock);  // 출고 후 재고 불변

    // 영속성
    reloadAll();
    EXPECT_EQ(OrderStatus::RELEASE, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(70, sampleSvc_->findById("S-001")->stock);
}

// ════════════════════════════════════════════════════
//  FS-02: 재고 부족 전체 플로우 (생산 포함)
//  DoD: 주문 승인(부족 분기) · 생산 큐 · 자동 갱신 · 출고
// ════════════════════════════════════════════════════

TEST_F(FullFlowTest, FS02_FullFlow_StockInsufficient_WithProduction) {
    sampleSvc_->add("베타-시료", 0.5, 0.88, 0);  // stock=0

    // 주문 생성 → 승인 → 재고 부족 → PRODUCING
    auto o = orderSvc_->createOrder("S-001", 10, "SK하이닉스");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_TRUE(r.success && !r.sufficient);
    EXPECT_EQ(OrderStatus::PRODUCING, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(0, sampleSvc_->findById("S-001")->stock);

    // 생산 큐 자동 시작 확인
    ASSERT_FALSE(db_->queue().empty());
    EXPECT_TRUE(db_->queue()[0].isInProgress());

    // 생산 완료 시뮬레이션 → CONFIRMED, stock 증가
    simulateComplete(0);
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o.id)->status);
    EXPECT_GT(sampleSvc_->findById("S-001")->stock, 0);

    // 출고 → RELEASE
    EXPECT_TRUE(orderSvc_->releaseOrder(o.id));
    EXPECT_EQ(OrderStatus::RELEASE, orderSvc_->findById(o.id)->status);

    // 영속성
    reloadAll();
    EXPECT_EQ(OrderStatus::RELEASE, orderSvc_->findById(o.id)->status);
    EXPECT_TRUE(db_->queue()[0].completed);
}

// ════════════════════════════════════════════════════
//  FS-03: 주문 거절 플로우
//  DoD: 거절 처리 · 재고 불변
// ════════════════════════════════════════════════════

TEST_F(FullFlowTest, FS03_FullFlow_Rejection) {
    sampleSvc_->add("감마-시료", 0.5, 0.90, 50);
    int stockBefore = 50;

    // 주문 생성 → 거절
    auto o = orderSvc_->createOrder("S-001", 20, "LG화학");
    EXPECT_TRUE(orderSvc_->rejectOrder(o.id));
    EXPECT_EQ(OrderStatus::REJECTED, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(stockBefore, sampleSvc_->findById("S-001")->stock);  // 재고 불변

    // REJECTED 주문 출고 시도 → 실패
    EXPECT_FALSE(orderSvc_->releaseOrder(o.id));

    // REJECTED는 모니터링 제외 확인
    auto d = monitorSvc_->collect();
    EXPECT_EQ(0, d.cntReserved);
    EXPECT_EQ(0, d.cntConfirmed);
    EXPECT_EQ(0, d.activeOrders);  // REJECTED 제외
}

// ════════════════════════════════════════════════════
//  FS-04: FIFO 생산 큐 전체 플로우
//  DoD: 생산 큐 FIFO · 자동 완료 · 연속 자동 시작
// ════════════════════════════════════════════════════

TEST_F(FullFlowTest, FS04_FullFlow_FIFOProduction) {
    sampleSvc_->add("델타-시료", 0.5, 0.90, 0);

    // 주문 3건 생성 및 승인 → 3건 PRODUCING
    auto o1 = orderSvc_->createOrder("S-001", 3, "고객A");
    auto o2 = orderSvc_->createOrder("S-001", 4, "고객B");
    auto o3 = orderSvc_->createOrder("S-001", 2, "고객C");
    orderSvc_->approveOrder(o1.id);
    orderSvc_->approveOrder(o2.id);
    orderSvc_->approveOrder(o3.id);

    ASSERT_EQ(3u, db_->queue().size());
    // FIFO: 첫 번째만 IN_PROGRESS, 나머지 WAITING
    EXPECT_TRUE(db_->queue()[0].isInProgress());
    EXPECT_TRUE(db_->queue()[1].isWaiting());
    EXPECT_TRUE(db_->queue()[2].isWaiting());
    // FIFO 순서 (id 오름차순)
    EXPECT_LT(db_->queue()[0].id, db_->queue()[1].id);
    EXPECT_LT(db_->queue()[1].id, db_->queue()[2].id);

    int totalIncoming = 0;

    // 1번 완료 → 2번 자동 시작
    simulateComplete(0);
    totalIncoming += db_->queue()[0].actual_qty;
    EXPECT_TRUE(db_->queue()[0].completed);
    EXPECT_TRUE(db_->queue()[1].isInProgress());
    EXPECT_TRUE(db_->queue()[2].isWaiting());

    // 2번 완료 → 3번 자동 시작
    simulateComplete(1);
    totalIncoming += db_->queue()[1].actual_qty;
    EXPECT_TRUE(db_->queue()[1].completed);
    EXPECT_TRUE(db_->queue()[2].isInProgress());

    // 3번 완료
    simulateComplete(2);
    totalIncoming += db_->queue()[2].actual_qty;
    EXPECT_TRUE(db_->queue()[2].completed);

    // 전체 CONFIRMED
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o1.id)->status);
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o2.id)->status);
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o3.id)->status);

    // 재고 = 생산된 전체 합
    EXPECT_EQ(totalIncoming, sampleSvc_->findById("S-001")->stock);
}

// ════════════════════════════════════════════════════
//  FS-05: 재고 상태 계산 (주문 수요 대비)
//  DoD: 시료 재고 상태 표시
// ════════════════════════════════════════════════════

TEST_F(FullFlowTest, FS05_StockStatusCalculation) {
    sampleSvc_->add("엡실론-시료", 0.5, 0.90, 5);  // S-001, stock=5

    // 수요 없음 → 여유
    EXPECT_EQ("여유", monitorSvc_->calcStockInfo("S-001").status);

    // RESERVED 수요=3, stock=5 → 여유 (5 >= 3)
    auto o1 = orderSvc_->createOrder("S-001", 3, "고객A");
    EXPECT_EQ("여유", monitorSvc_->calcStockInfo("S-001").status);

    // RESERVED 수요=10 (누적 13), stock=5 → 부족 (5 < 13)
    orderSvc_->createOrder("S-001", 10, "고객B");
    EXPECT_EQ("부족", monitorSvc_->calcStockInfo("S-001").status);

    // 첫 번째 주문 거절 → 수요 감소
    orderSvc_->rejectOrder(o1.id);
    // 수요=10, stock=5 → 부족 (5 < 10)
    EXPECT_EQ("부족", monitorSvc_->calcStockInfo("S-001").status);

    // stock=0 → 고갈
    sampleSvc_->add("제타-시료", 0.5, 0.90, 0);  // S-002
    EXPECT_EQ("고갈", monitorSvc_->calcStockInfo("S-002").status);

    // stock=0 → 고갈 (생산 중이어도 stock=0이면 고갈)
    auto o3 = orderSvc_->createOrder("S-002", 8, "고객C");
    orderSvc_->approveOrder(o3.id);  // PRODUCING → stock=0 유지
    EXPECT_EQ("고갈", monitorSvc_->calcStockInfo("S-002").status);

    // stock>0 + 생산 중 포함 시 여유/부족 구분
    sampleSvc_->add("에타-시료", 0.5, 0.90, 3);  // S-003, stock=3
    orderSvc_->createOrder("S-003", 8, "고객D");  // RESERVED 수요=8
    // stock=3 + productionIncoming=0, 수요=8 → 부족 (3 < 8)
    EXPECT_EQ("부족", monitorSvc_->calcStockInfo("S-003").status);
}

// ════════════════════════════════════════════════════
//  FS-06: 완전 영속성 검증
//  DoD: 프로그램 재실행 후 데이터 유지
// ════════════════════════════════════════════════════

TEST_F(FullFlowTest, FS06_CompletePersistence) {
    // 다양한 상태 데이터 생성
    sampleSvc_->add("알파", 0.5, 0.95, 50);
    sampleSvc_->add("베타", 0.5, 0.88, 0);

    auto o1 = orderSvc_->createOrder("S-001", 10, "고객A");
    orderSvc_->approveOrder(o1.id);  // CONFIRMED
    orderSvc_->releaseOrder(o1.id);  // RELEASE

    auto o2 = orderSvc_->createOrder("S-002", 5, "고객B");
    orderSvc_->approveOrder(o2.id);  // PRODUCING → 큐 등록
    simulateComplete(0);             // CONFIRMED

    auto o3 = orderSvc_->createOrder("S-001", 3, "고객C");
    orderSvc_->rejectOrder(o3.id);  // REJECTED

    // 재시작
    reloadAll();

    // 모든 상태 유지
    EXPECT_EQ(OrderStatus::RELEASE,   orderSvc_->findById(o1.id)->status);
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o2.id)->status);
    EXPECT_EQ(OrderStatus::REJECTED,  orderSvc_->findById(o3.id)->status);
    EXPECT_EQ(40, sampleSvc_->findById("S-001")->stock);  // 50 - 10
    EXPECT_TRUE(db_->queue()[0].completed);

    // RELEASE 재출고 불가
    EXPECT_FALSE(orderSvc_->releaseOrder(o1.id));

    // 2번 주문 출고 가능
    EXPECT_TRUE(orderSvc_->releaseOrder(o2.id));
    EXPECT_EQ(OrderStatus::RELEASE, orderSvc_->findById(o2.id)->status);
}

// ════════════════════════════════════════════════════
//  FS-07: 더미 데이터 생성 플로우
//  DoD: 더미 데이터 생성 기능 동작
// ════════════════════════════════════════════════════

TEST_F(FullFlowTest, FS07_DummyDataFlow) {
    // 시료 10개 생성
    dummySvc_->generateSamples(10, 42);
    ASSERT_EQ(10u, sampleSvc_->all().size());
    EXPECT_EQ("S-001", sampleSvc_->all()[0].id);
    EXPECT_EQ("S-010", sampleSvc_->all()[9].id);
    for (const auto& s : sampleSvc_->all())
        EXPECT_NE(std::string::npos, s.name.find("-시료"));

    // 주문 5건 생성 (등록된 시료 기반)
    dummySvc_->generateOrders(5, 42);
    ASSERT_EQ(5u, orderSvc_->all().size());
    for (const auto& o : orderSvc_->all()) {
        EXPECT_EQ(OrderStatus::RESERVED, o.status);
        EXPECT_NE(nullptr, sampleSvc_->findById(o.sample_id));
    }

    // 영속성
    reloadAll();
    EXPECT_EQ(10u, sampleSvc_->all().size());
    EXPECT_EQ(5u,  orderSvc_->all().size());

    // 초기화 후 재생성 → S-001부터 다시
    dummySvc_->resetAll();
    EXPECT_TRUE(sampleSvc_->all().empty());
    EXPECT_TRUE(orderSvc_->all().empty());

    dummySvc_->generateSamples(3, 99);
    EXPECT_EQ(3u, sampleSvc_->all().size());
    EXPECT_EQ("S-001", sampleSvc_->all()[0].id);  // 초기화 후 재시작
}

#endif  // SOS_TEST_MODE

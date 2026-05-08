#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "Model/app_db.h"
#include "Service/SampleService.h"
#include "Service/OrderService.h"
#include "Service/MonitoringService.h"

class MonitoringTest : public ::testing::Test {
protected:
    const std::string              path_ = "test_monitor.json";
    std::unique_ptr<AppDB>         db_;
    std::unique_ptr<SampleService> sampleSvc_;
    std::unique_ptr<OrderService>  orderSvc_;
    std::unique_ptr<MonitoringService> monSvc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_        = std::make_unique<AppDB>(path_);
        sampleSvc_ = std::make_unique<SampleService>(*db_);
        orderSvc_  = std::make_unique<OrderService>(*db_);
        monSvc_    = std::make_unique<MonitoringService>(*db_);
    }
    void TearDown() override {
        monSvc_.reset(); orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }
};

// TC-MN-01: 빈 상태 collect() → 모두 0, updatedAt 형식 확인
TEST_F(MonitoringTest, Collect_Empty) {
    auto d = monSvc_->collect();
    EXPECT_EQ(0, d.cntReserved);
    EXPECT_EQ(0, d.cntProducing);
    EXPECT_EQ(0, d.cntConfirmed);
    EXPECT_EQ(0, d.cntRelease);
    EXPECT_EQ(0, d.totalStock);
    EXPECT_EQ(0, d.activeOrders);
    // updatedAt 형식: "YYYY-MM-DD HH:MM:SS" (19자)
    ASSERT_EQ(19u, d.updatedAt.size());
    EXPECT_EQ('-', d.updatedAt[4]);
    EXPECT_EQ(' ', d.updatedAt[10]);
}

// TC-MN-02: 주문 상태별 카운팅 (REJECTED 제외)
TEST_F(MonitoringTest, Collect_OrderCounts) {
    sampleSvc_->add("알파", 0.5, 0.95, 100);
    auto o1 = orderSvc_->createOrder("S-001",  5, "고객A");
    auto o2 = orderSvc_->createOrder("S-001",  5, "고객B");
    auto o3 = orderSvc_->createOrder("S-001",  5, "고객C");
    auto o4 = orderSvc_->createOrder("S-001",  5, "고객D");
    orderSvc_->approveOrder(o1.id);  // CONFIRMED
    orderSvc_->rejectOrder(o2.id);   // REJECTED → 제외
    // o3, o4 → RESERVED

    auto d = monSvc_->collect();
    EXPECT_EQ(2, d.cntReserved);   // o3, o4
    EXPECT_EQ(0, d.cntProducing);
    EXPECT_EQ(1, d.cntConfirmed);  // o1
    EXPECT_EQ(0, d.cntRelease);
    EXPECT_EQ(3, d.activeOrders);  // o1+o3+o4 (o2 REJECTED 제외)
}

// TC-MN-03: totalStock 집계
TEST_F(MonitoringTest, Collect_TotalStock) {
    sampleSvc_->add("알파", 0.5, 0.95, 100);
    sampleSvc_->add("베타", 0.5, 0.88,  50);
    sampleSvc_->add("감마", 0.5, 0.90,   0);
    auto d = monSvc_->collect();
    EXPECT_EQ(150, d.totalStock);  // 100 + 50 + 0
}

// TC-MN-04: calcStockInfo — 재고 상태 케이스별 검증
TEST_F(MonitoringTest, CalcStockInfo_StatusCases) {
    sampleSvc_->add("알파", 0.5, 0.95,  5); // S-001
    sampleSvc_->add("베타", 0.5, 0.88,  0); // S-002

    // 고갈: stock == 0
    EXPECT_EQ("고갈", monSvc_->calcStockInfo("S-002").status);

    // 여유: RESERVED 수요 없음 + stock > 0
    EXPECT_EQ("여유", monSvc_->calcStockInfo("S-001").status);

    // 여유: stock >= RESERVED 수요
    orderSvc_->createOrder("S-001", 3, "고객A");
    EXPECT_EQ("여유", monSvc_->calcStockInfo("S-001").status);  // 5 >= 3

    // 부족: stock < RESERVED 수요
    orderSvc_->createOrder("S-001", 10, "고객B");  // 누적 수요 13
    EXPECT_EQ("부족", monSvc_->calcStockInfo("S-001").status);  // 5 < 13
}

// TC-MN-05: collect() 시 IN_PROGRESS 자동 완료 + 카운트 반영
TEST_F(MonitoringTest, Collect_AutoComplete_UpdatesStatus) {
    sampleSvc_->add("베타", 0.5, 0.88, 0);
    auto o = orderSvc_->createOrder("S-001", 10, "삼성전자");
    auto r = orderSvc_->approveOrder(o.id);  // PRODUCING
    ASSERT_FALSE(r.sufficient);

    // started_at 과거 → 경과 시간 충족
    auto& q = db_->queue();
    q[0].started_at = "2000-01-01 00:00:00";
    db_->updateQueueItem(q[0]);

    auto d = monSvc_->collect();  // checkAndComplete() 실행
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(r.actualQty, sampleSvc_->findById("S-001")->stock);
    EXPECT_EQ(0, d.cntProducing);  // 완료 처리 후 0
    EXPECT_EQ(1, d.cntConfirmed);  // CONFIRMED로 전환
}

// TC-MN-06: PRODUCING 상태 주문이 cntProducing에 집계
TEST_F(MonitoringTest, Collect_ProducingCount) {
    sampleSvc_->add("베타", 0.5, 0.88, 0);
    auto o1 = orderSvc_->createOrder("S-001", 5, "고객A");
    auto o2 = orderSvc_->createOrder("S-001", 3, "고객B");
    orderSvc_->approveOrder(o1.id);  // PRODUCING
    orderSvc_->approveOrder(o2.id);  // PRODUCING

    auto d = monSvc_->collect();
    EXPECT_EQ(0, d.cntReserved);
    EXPECT_EQ(2, d.cntProducing);
    EXPECT_EQ(0, d.cntConfirmed);
    EXPECT_EQ(2, d.activeOrders);
}

#endif  // SOS_TEST_MODE

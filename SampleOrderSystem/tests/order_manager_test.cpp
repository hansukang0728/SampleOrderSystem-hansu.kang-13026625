#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "Model/app_db.h"
#include "Service/SampleService.h"
#include "Service/OrderService.h"

class OrderManagerTest : public ::testing::Test {
protected:
    const std::string              path_      = "test_ordermgr.json";
    std::unique_ptr<AppDB>         db_;
    std::unique_ptr<SampleService> sampleSvc_;
    std::unique_ptr<OrderService>  orderSvc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_        = std::make_unique<AppDB>(path_);
        sampleSvc_ = std::make_unique<SampleService>(*db_);
        orderSvc_  = std::make_unique<OrderService>(*db_);
        sampleSvc_->add("알파-시료", 0.5, 0.95, 100); // S-001, 재고 충분
        sampleSvc_->add("베타-시료", 1.5, 0.88,   5); // S-002, 재고 부족
        sampleSvc_->add("감마-시료", 0.5, 0.90,   0); // S-003, 재고 없음
    }
    void TearDown() override {
        orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }
};

// TC-OM-01: 승인 — 재고 충분 → CONFIRMED, stock 감소
TEST_F(OrderManagerTest, Approve_StockSufficient) {
    auto o = orderSvc_->createOrder("S-001", 30, "삼성전자");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_TRUE(r.success);
    EXPECT_TRUE(r.sufficient);
    EXPECT_EQ(100, r.prevStock);
    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(70, sampleSvc_->findById("S-001")->stock);  // 100 - 30
}

// TC-OM-02: 승인 — 재고 부족 → PRODUCING, 생산 큐 등록, stock=0
TEST_F(OrderManagerTest, Approve_StockInsufficient) {
    auto o = orderSvc_->createOrder("S-002", 50, "SK하이닉스");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_TRUE(r.success);
    EXPECT_FALSE(r.sufficient);
    EXPECT_EQ(5, r.prevStock);
    EXPECT_EQ(45, r.shortage);           // 50 - 5
    // ceil(45 / (0.88 × 0.9)) = ceil(56.818) = 57
    EXPECT_EQ(57, r.actualQty);
    EXPECT_NEAR(1.5 * 57, r.totalTime, 1e-9);  // 85.5분
    EXPECT_EQ(0, sampleSvc_->findById("S-002")->stock);  // 기존 재고 전량 소진
    EXPECT_EQ(OrderStatus::PRODUCING, orderSvc_->findById(o.id)->status);
    // 생산 큐 등록 확인
    auto& q = db_->queue();
    ASSERT_FALSE(q.empty());
    EXPECT_EQ(o.id, q[0].order_id);
    EXPECT_EQ(45,   q[0].shortage);
    EXPECT_EQ(57,   q[0].actual_qty);
}

// TC-OM-03: 승인 — 재고 0 → 전량 생산
TEST_F(OrderManagerTest, Approve_ZeroStock) {
    auto o = orderSvc_->createOrder("S-003", 9, "LG화학");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_TRUE(r.success);
    EXPECT_FALSE(r.sufficient);
    EXPECT_EQ(9, r.shortage);
    // ceil(9 / (0.90 × 0.9)) = ceil(11.11) = 12
    EXPECT_EQ(12, r.actualQty);
    EXPECT_EQ(0, sampleSvc_->findById("S-003")->stock);
    EXPECT_EQ(OrderStatus::PRODUCING, orderSvc_->findById(o.id)->status);
}

// TC-OM-04: 승인 — 존재하지 않는 주문
TEST_F(OrderManagerTest, Approve_NotFound) {
    auto r = orderSvc_->approveOrder("ORD-99991231-9999");
    EXPECT_FALSE(r.success);
}

// TC-OM-05: 승인 — 이미 CONFIRMED 주문 재승인 방어
TEST_F(OrderManagerTest, Approve_NotReserved) {
    auto o = orderSvc_->createOrder("S-001", 10, "포스코");
    orderSvc_->approveOrder(o.id);  // CONFIRMED로 변경
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_FALSE(r.success);
}

// TC-OM-06: 거절 → REJECTED
TEST_F(OrderManagerTest, Reject_Success) {
    auto o = orderSvc_->createOrder("S-001", 30, "현대자동차");
    EXPECT_TRUE(orderSvc_->rejectOrder(o.id));
    EXPECT_EQ(OrderStatus::REJECTED, orderSvc_->findById(o.id)->status);
}

// TC-OM-07: 거절 — 재고 변화 없음
TEST_F(OrderManagerTest, Reject_StockUnchanged) {
    auto o = orderSvc_->createOrder("S-001", 30, "한화솔루션");
    orderSvc_->rejectOrder(o.id);
    EXPECT_EQ(100, sampleSvc_->findById("S-001")->stock);
}

// TC-OM-08: 거절 — RESERVED 아닌 주문
TEST_F(OrderManagerTest, Reject_NotReserved) {
    auto o = orderSvc_->createOrder("S-001", 10, "롯데케미칼");
    orderSvc_->approveOrder(o.id);  // CONFIRMED로 변경
    EXPECT_FALSE(orderSvc_->rejectOrder(o.id));
}

// TC-OM-09: reservedOrders() — RESERVED만 필터링
TEST_F(OrderManagerTest, ReservedOrders_Filter) {
    auto o1 = orderSvc_->createOrder("S-001", 10, "고객A");
    auto o2 = orderSvc_->createOrder("S-001", 20, "고객B");
    auto o3 = orderSvc_->createOrder("S-001",  5, "고객C");
    orderSvc_->approveOrder(o1.id);  // CONFIRMED
    orderSvc_->rejectOrder(o2.id);   // REJECTED
    // o3 RESERVED 유지

    auto reserved = orderSvc_->reservedOrders();
    ASSERT_EQ(1u, reserved.size());
    EXPECT_EQ(o3.id, reserved[0]->id);
}

// TC-OM-10: ceil 경계값 — 다른 yield_rate로 공식 다양성 확보
// 감마-시료: yield_rate=0.90, stock=0, shortage=9
// ceil(9 / (0.90 × 0.9)) = ceil(9 / 0.81) = ceil(11.111) = 12
TEST_F(OrderManagerTest, ProductionFormula_DifferentYield) {
    auto o = orderSvc_->createOrder("S-003", 9, "OCI Company");
    auto r = orderSvc_->approveOrder(o.id);
    EXPECT_EQ(12, r.actualQty);
    EXPECT_NEAR(0.5 * 12, r.totalTime, 1e-9);
}

// TC-OM-11: 영속성 — 재로드 후 승인 상태 유지
TEST_F(OrderManagerTest, Persistence_ApproveResult) {
    auto o = orderSvc_->createOrder("S-001", 30, "Samsung SDI");
    orderSvc_->approveOrder(o.id);

    orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
    db_        = std::make_unique<AppDB>(path_);
    sampleSvc_ = std::make_unique<SampleService>(*db_);
    orderSvc_  = std::make_unique<OrderService>(*db_);

    EXPECT_EQ(OrderStatus::CONFIRMED, orderSvc_->findById(o.id)->status);
    EXPECT_EQ(70, sampleSvc_->findById("S-001")->stock);
}

#endif  // SOS_TEST_MODE

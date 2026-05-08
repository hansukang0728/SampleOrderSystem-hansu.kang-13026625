#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "Model/app_db.h"
#include "Service/SampleService.h"
#include "Service/OrderService.h"

class ReleaseTest : public ::testing::Test {
protected:
    const std::string             path_ = "test_release.json";
    std::unique_ptr<AppDB>        db_;
    std::unique_ptr<SampleService> sampleSvc_;
    std::unique_ptr<OrderService>  orderSvc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_        = std::make_unique<AppDB>(path_);
        sampleSvc_ = std::make_unique<SampleService>(*db_);
        orderSvc_  = std::make_unique<OrderService>(*db_);
        sampleSvc_->add("알파-시료", 0.5, 0.95, 100);  // S-001
    }
    void TearDown() override {
        orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }

    // 주문 생성 → 승인(재고 충분) → CONFIRMED
    std::string makeConfirmedOrder(const std::string& customer, int qty) {
        auto o = orderSvc_->createOrder("S-001", qty, customer);
        orderSvc_->approveOrder(o.id);
        return o.id;
    }
};

// TC-RL-01: releaseOrder — CONFIRMED → RELEASE
TEST_F(ReleaseTest, ReleaseOrder_Success) {
    auto id = makeConfirmedOrder("삼성전자", 10);
    EXPECT_TRUE(orderSvc_->releaseOrder(id));
    EXPECT_EQ(OrderStatus::RELEASE, orderSvc_->findById(id)->status);
}

// TC-RL-02: releaseOrder — 존재하지 않는 주문 → false
TEST_F(ReleaseTest, ReleaseOrder_NotFound) {
    EXPECT_FALSE(orderSvc_->releaseOrder("ORD-99991231-9999"));
}

// TC-RL-03: releaseOrder — CONFIRMED 아닌 주문 → false
TEST_F(ReleaseTest, ReleaseOrder_NotConfirmed) {
    // RESERVED 주문 출고 시도
    auto o = orderSvc_->createOrder("S-001", 5, "고객A");
    EXPECT_FALSE(orderSvc_->releaseOrder(o.id));

    // REJECTED 주문 출고 시도
    orderSvc_->rejectOrder(o.id);
    EXPECT_FALSE(orderSvc_->releaseOrder(o.id));
}

// TC-RL-04: confirmedOrders — CONFIRMED만 필터링
TEST_F(ReleaseTest, ConfirmedOrders_Filter) {
    auto id1 = makeConfirmedOrder("고객A", 5);   // CONFIRMED
    auto o2  = orderSvc_->createOrder("S-001", 3, "고객B");  // RESERVED
    orderSvc_->rejectOrder(o2.id);               // REJECTED

    auto confirmed = orderSvc_->confirmedOrders();
    ASSERT_EQ(1u, confirmed.size());
    EXPECT_EQ(id1, confirmed[0]->id);
}

// TC-RL-05: 영속성 — 재로드 후 RELEASE 상태 유지
TEST_F(ReleaseTest, Persistence_ReleaseStatus) {
    auto id = makeConfirmedOrder("삼성전자", 10);
    orderSvc_->releaseOrder(id);

    orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
    db_        = std::make_unique<AppDB>(path_);
    sampleSvc_ = std::make_unique<SampleService>(*db_);
    orderSvc_  = std::make_unique<OrderService>(*db_);

    EXPECT_EQ(OrderStatus::RELEASE, orderSvc_->findById(id)->status);
}

// TC-RL-06: releaseOrder — 재고 변화 없음 (승인 시 이미 차감)
TEST_F(ReleaseTest, ReleaseOrder_StockUnchanged) {
    int stockBefore = sampleSvc_->findById("S-001")->stock;
    auto id = makeConfirmedOrder("삼성전자", 10);
    int stockAfterApprove = sampleSvc_->findById("S-001")->stock;

    orderSvc_->releaseOrder(id);
    int stockAfterRelease = sampleSvc_->findById("S-001")->stock;

    EXPECT_EQ(stockBefore - 10, stockAfterApprove);  // 승인 시 차감
    EXPECT_EQ(stockAfterApprove, stockAfterRelease);  // 출고 시 변화 없음
}

#endif  // SOS_TEST_MODE

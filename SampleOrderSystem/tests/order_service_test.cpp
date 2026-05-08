#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "Model/app_db.h"
#include "Service/OrderService.h"

class OrderServiceTest : public ::testing::Test {
protected:
    const std::string             path_ = "test_order.json";
    std::unique_ptr<AppDB>        db_;
    std::unique_ptr<OrderService> svc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_  = std::make_unique<AppDB>(path_);
        svc_ = std::make_unique<OrderService>(*db_);
    }
    void TearDown() override {
        svc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }
};

// TC-OS-01: 주문 생성 → RESERVED 상태 + created_at 형식
TEST_F(OrderServiceTest, CreateOrder_Reserved) {
    auto o = svc_->createOrder("S-001", 30, "홍길동");
    EXPECT_EQ(OrderStatus::RESERVED, o.status);
    EXPECT_EQ("S-001",  o.sample_id);
    EXPECT_EQ(30,       o.quantity);
    EXPECT_EQ("홍길동", o.customer_name);
    // created_at 형식: "YYYY-MM-DD HH:MM:SS" (19자)
    ASSERT_EQ(19u, o.created_at.size());
    EXPECT_EQ('-', o.created_at[4]);
    EXPECT_EQ('-', o.created_at[7]);
    EXPECT_EQ(' ', o.created_at[10]);
    EXPECT_EQ(':', o.created_at[13]);
    EXPECT_EQ(':', o.created_at[16]);
}

// TC-OS-02: 주문번호 형식 (ORD-YYYYMMDD-0001)
TEST_F(OrderServiceTest, CreateOrder_IdFormat) {
    auto o = svc_->createOrder("S-001", 10, "고객A");
    std::string today          = todayStr();
    std::string expectedId     = "ORD-" + today + "-0001";
    EXPECT_EQ(expectedId, o.id);
}

// TC-OS-03: 당일 주문 순번 증가
TEST_F(OrderServiceTest, CreateOrder_DailySequence) {
    auto o1 = svc_->createOrder("S-001", 10, "고객A");
    auto o2 = svc_->createOrder("S-002", 20, "고객B");
    auto o3 = svc_->createOrder("S-001",  5, "고객C");
    std::string today = todayStr();
    EXPECT_EQ("ORD-" + today + "-0001", o1.id);
    EXPECT_EQ("ORD-" + today + "-0002", o2.id);
    EXPECT_EQ("ORD-" + today + "-0003", o3.id);
}

// TC-OS-04: 유효성 검증 — 고객명 (공백 트림 포함)
TEST_F(OrderServiceTest, Validation_CustomerName) {
    EXPECT_FALSE(OrderService::validateCustomerName(""));
    EXPECT_FALSE(OrderService::validateCustomerName("   "));  // 공백만 → 실패
    EXPECT_TRUE(OrderService::validateCustomerName("홍길동"));
    EXPECT_TRUE(OrderService::validateCustomerName("A"));
}

// TC-OS-05: 유효성 검증 — 주문 수량 (경계값)
TEST_F(OrderServiceTest, Validation_Quantity) {
    EXPECT_FALSE(OrderService::validateQuantity(0));
    EXPECT_FALSE(OrderService::validateQuantity(-1));
    EXPECT_TRUE(OrderService::validateQuantity(1));    // 경계: 최솟값
    EXPECT_TRUE(OrderService::validateQuantity(100));
}

// TC-OS-06: 영속성 — 재로드 후 주문 유지
TEST_F(OrderServiceTest, Persistence) {
    svc_->createOrder("S-001", 30, "홍길동");
    svc_.reset(); db_.reset();
    db_  = std::make_unique<AppDB>(path_);
    svc_ = std::make_unique<OrderService>(*db_);
    ASSERT_EQ(1u, svc_->all().size());
    EXPECT_EQ("홍길동",              svc_->all()[0].customer_name);
    EXPECT_EQ(OrderStatus::RESERVED, svc_->all()[0].status);
    EXPECT_EQ(30,                    svc_->all()[0].quantity);
}

#endif  // SOS_TEST_MODE

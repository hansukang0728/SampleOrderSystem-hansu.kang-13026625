#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "Model/app_db.h"
#include "Service/SampleService.h"
#include "Service/OrderService.h"
#include "Service/ProductionService.h"

class ProductionServiceTest : public ::testing::Test {
protected:
    const std::string               path_ = "test_prod.json";
    std::unique_ptr<AppDB>          db_;
    std::unique_ptr<SampleService>  sampleSvc_;
    std::unique_ptr<OrderService>   orderSvc_;
    std::unique_ptr<ProductionService> prodSvc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_        = std::make_unique<AppDB>(path_);
        sampleSvc_ = std::make_unique<SampleService>(*db_);
        orderSvc_  = std::make_unique<OrderService>(*db_);
        prodSvc_   = std::make_unique<ProductionService>(*db_);
        sampleSvc_->add("알파", 0.5, 0.88, 0);  // S-001
    }
    void TearDown() override {
        prodSvc_.reset(); orderSvc_.reset();
        sampleSvc_.reset(); db_.reset();
        std::remove(path_.c_str());
    }

    // 주문 생성·승인 → 큐 등록
    std::string enqueueOrder(const std::string& customer, int qty) {
        auto o = orderSvc_->createOrder("S-001", qty, customer);
        orderSvc_->approveOrder(o.id);
        return o.id;
    }
};

// TC-PS-01: 첫 번째 enqueue → 즉시 IN_PROGRESS (FIFO 자동 시작)
TEST_F(ProductionServiceTest, Enqueue_FirstItem_AutoStartsInProgress) {
    enqueueOrder("삼성전자", 5);
    const auto* item = prodSvc_->inProgressItem();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(item->isInProgress());
    EXPECT_FALSE(item->started_at.empty());
}

// TC-PS-02: 두 번째 enqueue → 첫 번째 IN_PROGRESS, 두 번째 WAITING
TEST_F(ProductionServiceTest, Enqueue_SecondItem_RemainsWaiting) {
    enqueueOrder("삼성전자", 5);
    enqueueOrder("SK하이닉스", 3);

    EXPECT_NE(nullptr, prodSvc_->inProgressItem());
    auto wq = prodSvc_->waitingQueue();
    ASSERT_EQ(1u, wq.size());  // 두 번째만 WAITING
    EXPECT_TRUE(wq[0]->isWaiting());
}

// TC-PS-03: waitingQueue — FIFO 순서 (id 오름차순)
TEST_F(ProductionServiceTest, WaitingQueue_FIFOOrder) {
    enqueueOrder("고객A", 3);
    enqueueOrder("고객B", 5);
    enqueueOrder("고객C", 2);

    auto wq = prodSvc_->waitingQueue();
    ASSERT_EQ(2u, wq.size());  // 첫 번째는 IN_PROGRESS
    EXPECT_LT(wq[0]->id, wq[1]->id);
}

// TC-PS-04: 생산 완료 후 다음 WAITING 자동 시작
TEST_F(ProductionServiceTest, AfterCompletion_NextWaitingAutoStarts) {
    enqueueOrder("고객A", 3);
    enqueueOrder("고객B", 5);

    // 첫 번째 완료 시뮬레이션
    auto& q = db_->queue();
    q[0].started_at = "2000-01-01 00:00:00";
    db_->updateQueueItem(q[0]);
    db_->checkAndComplete();  // 완료 처리 + 두 번째 자동 시작

    // 두 번째 항목이 자동으로 IN_PROGRESS
    const auto* item = prodSvc_->inProgressItem();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(item->isInProgress());
    EXPECT_EQ(0u, prodSvc_->waitingQueue().size());  // WAITING 없음
}

// TC-PS-05: currentProduction — WAITING이면 0
TEST_F(ProductionServiceTest, CurrentProduction_WaitingIsZero) {
    enqueueOrder("고객A", 5);
    enqueueOrder("고객B", 3);  // WAITING
    auto wq = prodSvc_->waitingQueue();
    ASSERT_FALSE(wq.empty());
    EXPECT_EQ(0, ProductionService::currentProduction(*wq[0]));
}

// TC-PS-06: currentProduction — IN_PROGRESS 유효 범위
TEST_F(ProductionServiceTest, CurrentProduction_ValidRange) {
    enqueueOrder("고객A", 5);
    const auto* item = prodSvc_->inProgressItem();
    ASSERT_NE(nullptr, item);
    int cp = ProductionService::currentProduction(*item);
    EXPECT_GE(cp, 0);
    EXPECT_LE(cp, item->actual_qty);
}

// TC-PS-07: estimatedCompletion — WAITING이면 "-"
TEST_F(ProductionServiceTest, EstimatedCompletion_Waiting) {
    enqueueOrder("고객A", 5);
    enqueueOrder("고객B", 3);
    auto wq = prodSvc_->waitingQueue();
    ASSERT_FALSE(wq.empty());
    EXPECT_EQ("-", ProductionService::estimatedCompletion(*wq[0]));
}

// TC-PS-08: estimatedCompletion — IN_PROGRESS이면 "YYYY-MM-DD HH:MM" (16자)
TEST_F(ProductionServiceTest, EstimatedCompletion_InProgress) {
    enqueueOrder("고객A", 5);
    const auto* item = prodSvc_->inProgressItem();
    ASSERT_NE(nullptr, item);
    std::string est = ProductionService::estimatedCompletion(*item);
    EXPECT_NE("-", est);
    EXPECT_EQ(16u, est.size());
}

// TC-PS-09: inProgressItem — 생산 완료 후 nullptr (다음 항목 없으면)
TEST_F(ProductionServiceTest, InProgressItem_AfterCompletion_NoNext) {
    enqueueOrder("고객A", 5);

    auto& q = db_->queue();
    q[0].started_at = "2000-01-01 00:00:00";
    db_->updateQueueItem(q[0]);
    db_->checkAndComplete();

    EXPECT_EQ(nullptr, prodSvc_->inProgressItem());
    EXPECT_TRUE(prodSvc_->waitingQueue().empty());
}

// TC-PS-10: enqueue 큐 없을 때 첫 항목 즉시 IN_PROGRESS 확인 (영속성)
TEST_F(ProductionServiceTest, Enqueue_AutoStart_Persistent) {
    enqueueOrder("고객A", 5);

    // 재시작 후에도 IN_PROGRESS 유지
    prodSvc_.reset(); orderSvc_.reset(); sampleSvc_.reset(); db_.reset();
    db_        = std::make_unique<AppDB>(path_);
    sampleSvc_ = std::make_unique<SampleService>(*db_);
    orderSvc_  = std::make_unique<OrderService>(*db_);
    prodSvc_   = std::make_unique<ProductionService>(*db_);

    const auto* item = prodSvc_->inProgressItem();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(item->isInProgress());
}

#endif  // SOS_TEST_MODE

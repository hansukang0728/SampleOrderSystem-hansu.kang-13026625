#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <cstdio>
#include "Model/app_db.h"

// ════════════════════════════════════════════════════
//  Fixture — 각 테스트마다 임시 파일 격리
// ════════════════════════════════════════════════════

class AppDBTest : public ::testing::Test {
protected:
    const std::string path_ = "test_data.json";
    void SetUp()    override { std::remove(path_.c_str()); }
    void TearDown() override { std::remove(path_.c_str()); }
};

// ════════════════════════════════════════════════════
//  Sample 테스트  (TC-DB-01 ~ TC-DB-07)
// ════════════════════════════════════════════════════

// TC-DB-01: 신규 파일 — 빈 컬렉션으로 시작
TEST_F(AppDBTest, NewFile_EmptyCollections) {
    AppDB db(path_);
    EXPECT_TRUE(db.samples().empty());
    EXPECT_TRUE(db.orders().empty());
    EXPECT_TRUE(db.queue().empty());   // 3개 컬렉션 모두 검증
}

// TC-DB-02: Sample 생성 및 ID 형식
TEST_F(AppDBTest, CreateSample_IdFormat) {
    AppDB db(path_);
    auto s = db.createSample("알파-시료", 30.5, 0.95, 100);
    EXPECT_EQ("S-001",     s.id);
    EXPECT_EQ("알파-시료", s.name);
    EXPECT_DOUBLE_EQ(30.5, s.avg_production_time);
    EXPECT_DOUBLE_EQ(0.95, s.yield_rate);
    EXPECT_EQ(100,         s.stock);
}

// TC-DB-03: Sample ID 순번 증가
TEST_F(AppDBTest, CreateSample_IdSequence) {
    AppDB db(path_);
    auto s1 = db.createSample("알파", 10.0, 0.9, 0);
    auto s2 = db.createSample("베타", 20.0, 0.8, 0);
    auto s3 = db.createSample("감마", 30.0, 0.7, 0);
    EXPECT_EQ("S-001", s1.id);
    EXPECT_EQ("S-002", s2.id);
    EXPECT_EQ("S-003", s3.id);
}

// TC-DB-04: findSample — 존재하는 ID
TEST_F(AppDBTest, FindSample_ExistingId) {
    AppDB db(path_);
    db.createSample("알파-시료", 30.5, 0.95, 100);
    auto* s = db.findSample("S-001");
    ASSERT_NE(nullptr, s);
    EXPECT_EQ("알파-시료", s->name);
}

// TC-DB-05: findSample — 없는 ID
TEST_F(AppDBTest, FindSample_NotFound) {
    AppDB db(path_);
    EXPECT_EQ(nullptr, db.findSample("S-999"));
}

// TC-DB-06: Sample 영속성 — 재로드 후 데이터 유지
TEST_F(AppDBTest, Sample_Persistence) {
    {
        AppDB db(path_);
        db.createSample("알파-시료", 30.5, 0.95, 100);
    }
    AppDB db2(path_);
    ASSERT_EQ(1u, db2.samples().size());
    EXPECT_EQ("S-001",     db2.samples()[0].id);
    EXPECT_EQ("알파-시료", db2.samples()[0].name);
    EXPECT_DOUBLE_EQ(30.5, db2.samples()[0].avg_production_time);
    EXPECT_DOUBLE_EQ(0.95, db2.samples()[0].yield_rate);
    EXPECT_EQ(100,         db2.samples()[0].stock);
}

// TC-DB-07: Sample 업데이트 후 영속성
TEST_F(AppDBTest, UpdateSample_Persistence) {
    AppDB db(path_);
    db.createSample("알파-시료", 30.5, 0.95, 100);
    auto* s = db.findSample("S-001");
    ASSERT_NE(nullptr, s);
    s->stock = 200;
    db.updateSample(*s);

    AppDB db2(path_);
    auto* s2 = db2.findSample("S-001");
    ASSERT_NE(nullptr, s2);
    EXPECT_EQ(200, s2->stock);
}

// ════════════════════════════════════════════════════
//  Order 테스트  (TC-DB-08 ~ TC-DB-10)
// ════════════════════════════════════════════════════

// TC-DB-08: Order 생성 및 ID 형식
TEST_F(AppDBTest, CreateOrder_IdFormat) {
    AppDB db(path_);
    db.createSample("알파-시료", 30.5, 0.95, 100);
    auto o = db.createOrder("S-001", 30, "홍길동");

    std::string today          = todayStr();
    std::string expectedPrefix = "ORD-" + today + "-";
    EXPECT_EQ(0u, o.id.find(expectedPrefix));
    EXPECT_EQ("S-001",            o.sample_id);
    EXPECT_EQ(30,                 o.quantity);
    EXPECT_EQ("홍길동",           o.customer_name);
    EXPECT_EQ(OrderStatus::RESERVED, o.status);
    EXPECT_FALSE(o.created_at.empty());
}

// TC-DB-09: Order ID 당일 순번
TEST_F(AppDBTest, CreateOrder_DailySequence) {
    AppDB db(path_);
    db.createSample("알파", 10.0, 0.9, 0);
    auto o1 = db.createOrder("S-001", 10, "고객A");
    auto o2 = db.createOrder("S-001", 20, "고객B");

    std::string today = todayStr();
    EXPECT_EQ("ORD-" + today + "-0001", o1.id);
    EXPECT_EQ("ORD-" + today + "-0002", o2.id);
}

// TC-DB-10: Order 영속성
TEST_F(AppDBTest, Order_Persistence) {
    {
        AppDB db(path_);
        db.createSample("알파", 10.0, 0.9, 0);
        db.createOrder("S-001", 10, "홍길동");
    }
    AppDB db2(path_);
    ASSERT_EQ(1u, db2.orders().size());
    EXPECT_EQ("홍길동",           db2.orders()[0].customer_name);
    EXPECT_EQ(10,                 db2.orders()[0].quantity);
    EXPECT_EQ(OrderStatus::RESERVED, db2.orders()[0].status);
}

// ════════════════════════════════════════════════════
//  ProductionQueueItem 테스트  (TC-DB-11 ~ TC-DB-15)
// ════════════════════════════════════════════════════

// TC-DB-11: enqueue — WAITING 상태로 생성
TEST_F(AppDBTest, Enqueue_WaitingState) {
    AppDB db(path_);
    auto p = db.enqueue("ORD-001", "S-001", 45, 57, 2565.0);
    EXPECT_EQ(1,       p.id);
    EXPECT_EQ("S-001", p.sample_id);
    EXPECT_EQ(45,      p.shortage);
    EXPECT_EQ(57,      p.actual_qty);
    EXPECT_DOUBLE_EQ(2565.0, p.total_time);
    EXPECT_FALSE(p.completed);
    EXPECT_EQ("",  p.started_at);
    EXPECT_TRUE(p.isWaiting());
}

// TC-DB-12: frontWaiting — FIFO 순서
TEST_F(AppDBTest, FrontWaiting_Fifo) {
    AppDB db(path_);
    db.enqueue("ORD-001", "S-001", 10, 12, 120.0);
    db.enqueue("ORD-002", "S-002", 20, 23, 230.0);

    auto* front = db.frontWaiting();
    ASSERT_NE(nullptr, front);
    EXPECT_EQ(1, front->id);  // 가장 먼저 enqueue된 항목
}

// TC-DB-13: frontWaiting — 비어있을 때 nullptr
TEST_F(AppDBTest, FrontWaiting_Empty) {
    AppDB db(path_);
    EXPECT_EQ(nullptr, db.frontWaiting());
}

// TC-DB-14: checkAndComplete — 과거 시각으로 자동 완료
TEST_F(AppDBTest, CheckAndComplete_AutoCompletion) {
    AppDB db(path_);
    db.createSample("알파", 10.0, 0.9, 5);
    auto o = db.createOrder("S-001", 50, "홍길동");

    // 주문을 PRODUCING 상태로 변경
    auto* op = db.findOrder(o.id);
    ASSERT_NE(nullptr, op);
    op->status = OrderStatus::PRODUCING;
    db.updateOrder(*op);

    // enqueue
    db.enqueue(o.id, "S-001", 45, 57, 1.0);

    // queue() 호출 → checkAndComplete 실행 (WAITING이므로 변화 없음)
    // started_at을 과거로 설정하여 IN_PROGRESS 상태로 전환
    auto& items = db.queue();
    ASSERT_FALSE(items.empty());
    items[0].started_at = "2000-01-01 00:00:00";  // 충분히 오래된 과거
    db.updateQueueItem(items[0]);

    // 자동 완료 실행
    db.checkAndComplete();

    // 검증
    EXPECT_TRUE(db.queue()[0].completed);
    EXPECT_EQ(OrderStatus::CONFIRMED, db.findOrder(o.id)->status);
    EXPECT_EQ(5 + 57, db.findSample("S-001")->stock);  // 5 + 57 = 62
}

// TC-DB-15: Queue 영속성
TEST_F(AppDBTest, Queue_Persistence) {
    {
        AppDB db(path_);
        db.enqueue("ORD-001", "S-001", 10, 12, 120.0);
    }
    AppDB db2(path_);
    auto& q = db2.queue();  // checkAndComplete 실행 (WAITING이므로 변화 없음)
    ASSERT_EQ(1u, q.size());
    EXPECT_EQ(1,       q[0].id);
    EXPECT_EQ("S-001", q[0].sample_id);
    EXPECT_EQ(10,      q[0].shortage);
    EXPECT_TRUE(q[0].isWaiting());
}

#endif  // SOS_TEST_MODE

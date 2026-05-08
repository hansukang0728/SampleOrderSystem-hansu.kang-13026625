#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include "../models.h"

// ════════════════════════════════════════════════════
//  SampleTest  (TC-S-01 ~ TC-S-04)
// ════════════════════════════════════════════════════

TEST(SampleTest, ToFromJsonRoundTrip) {
    Sample s;
    s.id = "S-001";  s.name = "알파-시료";
    s.avg_production_time = 30.5;
    s.yield_rate = 0.95;  s.stock = 100;

    Sample s2 = Sample::fromJson(s.toJson());

    EXPECT_EQ(s.id,    s2.id);
    EXPECT_EQ(s.name,  s2.name);
    EXPECT_DOUBLE_EQ(s.avg_production_time, s2.avg_production_time);
    EXPECT_DOUBLE_EQ(s.yield_rate,          s2.yield_rate);
    EXPECT_EQ(s.stock, s2.stock);
}

TEST(SampleTest, StockStatus_Boundary) {
    EXPECT_EQ("고갈", Sample::stockStatus(0));   // 경계: 고갈
    EXPECT_EQ("부족", Sample::stockStatus(1));   // 경계: 부족 하한
    EXPECT_EQ("부족", Sample::stockStatus(10));  // 경계: 부족 상한
    EXPECT_EQ("여유", Sample::stockStatus(11));  // 경계: 여유 하한
    EXPECT_EQ("여유", Sample::stockStatus(200)); // 일반값
}

TEST(SampleTest, DefaultValues) {
    Sample s;
    EXPECT_EQ(0.0, s.avg_production_time);
    EXPECT_EQ(0.0, s.yield_rate);
    EXPECT_EQ(0,   s.stock);
}

TEST(SampleTest, JsonKeyNames) {
    Sample s;
    s.id = "S-001";  s.name = "테스트";
    s.avg_production_time = 1.0;
    s.yield_rate = 0.9;  s.stock = 5;

    auto j = s.toJson();
    EXPECT_TRUE(j.contains("id"));
    EXPECT_TRUE(j.contains("name"));
    EXPECT_TRUE(j.contains("avg_production_time"));
    EXPECT_TRUE(j.contains("yield_rate"));
    EXPECT_TRUE(j.contains("stock"));
}

// ════════════════════════════════════════════════════
//  OrderTest  (TC-O-01 ~ TC-O-05)
// ════════════════════════════════════════════════════

TEST(OrderTest, ToFromJsonRoundTrip) {
    Order o;
    o.id = "ORD-20260508-0001";  o.sample_id = "S-001";
    o.quantity = 30;  o.customer_name = "홍길동";
    o.status = OrderStatus::RESERVED;
    o.created_at = "2026-05-08 10:00:00";

    Order o2 = Order::fromJson(o.toJson());

    EXPECT_EQ(o.id,            o2.id);
    EXPECT_EQ(o.sample_id,     o2.sample_id);
    EXPECT_EQ(o.quantity,      o2.quantity);
    EXPECT_EQ(o.customer_name, o2.customer_name);
    EXPECT_EQ(o.status,        o2.status);
    EXPECT_EQ(o.created_at,    o2.created_at);
}

TEST(OrderTest, StatusToString_AllValues) {
    EXPECT_EQ("RESERVED",  Order::statusToString(OrderStatus::RESERVED));
    EXPECT_EQ("REJECTED",  Order::statusToString(OrderStatus::REJECTED));
    EXPECT_EQ("PRODUCING", Order::statusToString(OrderStatus::PRODUCING));
    EXPECT_EQ("CONFIRMED", Order::statusToString(OrderStatus::CONFIRMED));
    EXPECT_EQ("RELEASE",   Order::statusToString(OrderStatus::RELEASE));
}

TEST(OrderTest, StringToStatus_AllValues) {
    EXPECT_EQ(OrderStatus::RESERVED,  Order::stringToStatus("RESERVED"));
    EXPECT_EQ(OrderStatus::REJECTED,  Order::stringToStatus("REJECTED"));
    EXPECT_EQ(OrderStatus::PRODUCING, Order::stringToStatus("PRODUCING"));
    EXPECT_EQ(OrderStatus::CONFIRMED, Order::stringToStatus("CONFIRMED"));
    EXPECT_EQ(OrderStatus::RELEASE,   Order::stringToStatus("RELEASE"));
}

TEST(OrderTest, StringToStatus_UnknownFallback) {
    EXPECT_EQ(OrderStatus::RESERVED, Order::stringToStatus("INVALID"));
    EXPECT_EQ(OrderStatus::RESERVED, Order::stringToStatus(""));
    EXPECT_EQ(OrderStatus::RESERVED, Order::stringToStatus("reserved")); // 소문자
}

TEST(OrderTest, StatusRoundTrip) {
    auto statuses = { OrderStatus::RESERVED, OrderStatus::REJECTED,
                      OrderStatus::PRODUCING, OrderStatus::CONFIRMED,
                      OrderStatus::RELEASE };
    for (auto s : statuses)
        EXPECT_EQ(s, Order::stringToStatus(Order::statusToString(s)));
}

// ════════════════════════════════════════════════════
//  ProductionQueueItemTest  (TC-P-01 ~ TC-P-07)
// ════════════════════════════════════════════════════

TEST(ProductionQueueItemTest, ToFromJsonRoundTrip) {
    ProductionQueueItem p;
    p.id = 1;  p.order_id = "ORD-20260508-0001";  p.sample_id = "S-002";
    p.shortage = 45;  p.actual_qty = 57;  p.total_time = 2565.0;
    p.completed = false;
    p.enqueued_at = "2026-05-08 10:20:00";
    p.started_at  = "";

    ProductionQueueItem p2 = ProductionQueueItem::fromJson(p.toJson());

    EXPECT_EQ(p.id,          p2.id);
    EXPECT_EQ(p.order_id,    p2.order_id);
    EXPECT_EQ(p.sample_id,   p2.sample_id);
    EXPECT_EQ(p.shortage,    p2.shortage);
    EXPECT_EQ(p.actual_qty,  p2.actual_qty);
    EXPECT_DOUBLE_EQ(p.total_time,  p2.total_time);
    EXPECT_EQ(p.completed,   p2.completed);
    EXPECT_EQ(p.enqueued_at, p2.enqueued_at);
    EXPECT_EQ(p.started_at,  p2.started_at);
}

TEST(ProductionQueueItemTest, State_Waiting) {
    ProductionQueueItem p;
    p.started_at = "";  p.completed = false;

    EXPECT_TRUE(p.isWaiting());
    EXPECT_FALSE(p.isInProgress());
    EXPECT_FALSE(p.isDone());
    EXPECT_DOUBLE_EQ(0.0, p.progressPct());
}

TEST(ProductionQueueItemTest, State_InProgress) {
    ProductionQueueItem p;
    p.started_at = "2026-05-08 10:00:00";
    p.completed  = false;
    p.total_time = 999999.0;  // 아직 완료되지 않을 충분한 시간

    EXPECT_FALSE(p.isWaiting());
    EXPECT_TRUE(p.isInProgress());
    EXPECT_FALSE(p.isDone());
}

TEST(ProductionQueueItemTest, State_Done) {
    ProductionQueueItem p;
    p.completed  = true;
    p.started_at = "2026-05-08 10:00:00";

    EXPECT_FALSE(p.isWaiting());
    EXPECT_FALSE(p.isInProgress());
    EXPECT_TRUE(p.isDone());
    EXPECT_DOUBLE_EQ(0.0, p.progressPct());
}

TEST(ProductionQueueItemTest, IsTimeElapsed_PastStart) {
    ProductionQueueItem p;
    p.started_at = "2000-01-01 00:00:00";  // 충분히 오래된 과거
    p.total_time = 1.0;                    // 1분 (이미 경과)
    p.completed  = false;

    EXPECT_TRUE(p.isTimeElapsed());
}

TEST(ProductionQueueItemTest, IsTimeElapsed_JustStarted) {
    ProductionQueueItem p;
    p.started_at = nowStr();       // 현재 시각
    p.total_time = 999999.0;      // 매우 긴 생산 시간
    p.completed  = false;

    EXPECT_FALSE(p.isTimeElapsed());
}

TEST(ProductionQueueItemTest, FromJson_MissingStartedAt) {
    auto j = JsonValue::makeObject();
    j["id"]          = JsonValue(1);
    j["order_id"]    = JsonValue(std::string("ORD-20260508-0001"));
    j["sample_id"]   = JsonValue(std::string("S-001"));
    j["shortage"]    = JsonValue(10);
    j["actual_qty"]  = JsonValue(13);
    j["total_time"]  = JsonValue(390.0);
    j["completed"]   = JsonValue(false);
    j["enqueued_at"] = JsonValue(std::string("2026-05-08 10:00:00"));
    // started_at 키 의도적 누락 → 하위 호환 확인

    ProductionQueueItem p = ProductionQueueItem::fromJson(j);
    EXPECT_EQ("", p.started_at);
    EXPECT_TRUE(p.isWaiting());
}

// ════════════════════════════════════════════════════
//  UtilsTest  (TC-U-01 ~ TC-U-05)
// ════════════════════════════════════════════════════

TEST(UtilsTest, FormatSampleId) {
    EXPECT_EQ("S-001", formatSampleId(1));
    EXPECT_EQ("S-010", formatSampleId(10));
    EXPECT_EQ("S-100", formatSampleId(100));
    EXPECT_EQ("S-999", formatSampleId(999));
}

TEST(UtilsTest, FormatOrderId) {
    EXPECT_EQ("ORD-20260508-0001", formatOrderId("20260508", 1));
    EXPECT_EQ("ORD-20260508-0010", formatOrderId("20260508", 10));
    EXPECT_EQ("ORD-20260508-9999", formatOrderId("20260508", 9999));
}

TEST(UtilsTest, NowStr_Format) {
    std::string s = nowStr();
    ASSERT_EQ(19u, s.size());
    EXPECT_EQ('-', s[4]);   // YYYY-
    EXPECT_EQ('-', s[7]);   // MM-
    EXPECT_EQ(' ', s[10]);  // DD 공백
    EXPECT_EQ(':', s[13]);  // HH:
    EXPECT_EQ(':', s[16]);  // MM:
}

TEST(UtilsTest, TodayStr_Format) {
    std::string s = todayStr();
    ASSERT_EQ(8u, s.size());
    for (char c : s)
        EXPECT_TRUE(std::isdigit((unsigned char)c));
}

TEST(UtilsTest, ElapsedMinutes_Empty) {
    EXPECT_DOUBLE_EQ(0.0, elapsedMinutes(""));
}

#endif  // SOS_TEST_MODE

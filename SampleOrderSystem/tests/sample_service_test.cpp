#ifdef SOS_TEST_MODE
#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "Model/app_db.h"
#include "Service/SampleService.h"

// ════════════════════════════════════════════════════
//  Fixture — unique_ptr 기반, double-free 방지
// ════════════════════════════════════════════════════

class SampleServiceTest : public ::testing::Test {
protected:
    const std::string             path_ = "test_sample.json";
    std::unique_ptr<AppDB>        db_;
    std::unique_ptr<SampleService> svc_;

    void SetUp() override {
        std::remove(path_.c_str());
        db_  = std::make_unique<AppDB>(path_);
        svc_ = std::make_unique<SampleService>(*db_);
    }
    void TearDown() override {
        svc_.reset();
        db_.reset();
        std::remove(path_.c_str());
    }
};

// ════════════════════════════════════════════════════
//  TC-SS-01 ~ TC-SS-08
// ════════════════════════════════════════════════════

// TC-SS-01: 시료 등록 후 전체 조회
TEST_F(SampleServiceTest, AddAndListAll) {
    svc_->add("알파-시료", 30.5, 0.95, 100);
    ASSERT_EQ(1u, svc_->all().size());
    EXPECT_EQ("S-001",     svc_->all()[0].id);
    EXPECT_EQ("알파-시료", svc_->all()[0].name);
    EXPECT_DOUBLE_EQ(30.5, svc_->all()[0].avg_production_time);
    EXPECT_DOUBLE_EQ(0.95, svc_->all()[0].yield_rate);
    EXPECT_EQ(100,         svc_->all()[0].stock);
}

// TC-SS-02: ID 조회 — 존재하는 ID
TEST_F(SampleServiceTest, FindById_Exists) {
    svc_->add("알파", 10.0, 0.9, 50);
    auto* s = svc_->findById("S-001");
    ASSERT_NE(nullptr, s);
    EXPECT_EQ("알파", s->name);
    EXPECT_EQ(50, s->stock);
}

// TC-SS-03: ID 조회 — 없는 ID
TEST_F(SampleServiceTest, FindById_NotFound) {
    EXPECT_EQ(nullptr, svc_->findById("S-999"));
}

// TC-SS-04: 이름 검색 — 부분 일치
TEST_F(SampleServiceTest, SearchByName_PartialMatch) {
    svc_->add("알파-시료", 10.0, 0.9, 0);
    svc_->add("베타-시료", 20.0, 0.8, 0);
    svc_->add("알파베타",  30.0, 0.7, 0);
    auto result = svc_->searchByName("알파");
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ("S-001", result[0]->id);
    EXPECT_EQ("S-003", result[1]->id);
}

// TC-SS-05: 이름 검색 — 결과 없음
TEST_F(SampleServiceTest, SearchByName_NoResult) {
    svc_->add("알파-시료", 10.0, 0.9, 0);
    EXPECT_TRUE(svc_->searchByName("감마").empty());
}

// TC-SS-06: 유효성 검증 (경계값 포함)
TEST_F(SampleServiceTest, Validation_Boundaries) {
    // 이름
    EXPECT_FALSE(SampleService::validateName(""));
    EXPECT_TRUE(SampleService::validateName("a"));
    EXPECT_TRUE(SampleService::validateName("알파-시료"));

    // 평균 생산시간
    EXPECT_FALSE(SampleService::validateAvgTime(0.0));
    EXPECT_FALSE(SampleService::validateAvgTime(-1.0));
    EXPECT_TRUE(SampleService::validateAvgTime(0.001));
    EXPECT_TRUE(SampleService::validateAvgTime(30.5));

    // 수율 (0.0 초과 ~ 1.0 이하)
    EXPECT_FALSE(SampleService::validateYieldRate(0.0));   // 경계: 0.0 제외
    EXPECT_TRUE(SampleService::validateYieldRate(0.001));
    EXPECT_TRUE(SampleService::validateYieldRate(0.95));
    EXPECT_TRUE(SampleService::validateYieldRate(1.0));    // 경계: 1.0 포함
    EXPECT_FALSE(SampleService::validateYieldRate(1.001));

    // 재고 (0 이상)
    EXPECT_FALSE(SampleService::validateStock(-1));
    EXPECT_TRUE(SampleService::validateStock(0));          // 경계: 0 허용
    EXPECT_TRUE(SampleService::validateStock(100));
}

// TC-SS-07: 영속성 — 재로드 후 데이터 유지
TEST_F(SampleServiceTest, Persistence) {
    svc_->add("알파-시료", 30.5, 0.95, 100);
    svc_.reset();
    db_.reset();

    db_  = std::make_unique<AppDB>(path_);
    svc_ = std::make_unique<SampleService>(*db_);
    ASSERT_EQ(1u, svc_->all().size());
    EXPECT_EQ("S-001",     svc_->all()[0].id);
    EXPECT_EQ("알파-시료", svc_->all()[0].name);
    EXPECT_DOUBLE_EQ(30.5, svc_->all()[0].avg_production_time);
    EXPECT_EQ(100,         svc_->all()[0].stock);
}

// TC-SS-08: 페이지네이션 — totalPages 경계값
TEST_F(SampleServiceTest, Pagination_TotalPages) {
    EXPECT_EQ(0, svc_->totalPages());          // 0개 → 0페이지

    for (int i = 0; i < 10; ++i)
        svc_->add("시료" + std::to_string(i), 10.0, 0.9, 0);
    EXPECT_EQ(1, svc_->totalPages());          // 10개 → 1페이지

    svc_->add("시료10", 10.0, 0.9, 0);
    EXPECT_EQ(2, svc_->totalPages());          // 11개 → 2페이지

    for (int i = 11; i < 20; ++i)
        svc_->add("시료" + std::to_string(i), 10.0, 0.9, 0);
    EXPECT_EQ(2, svc_->totalPages());          // 20개 → 2페이지

    svc_->add("시료20", 10.0, 0.9, 0);
    EXPECT_EQ(3, svc_->totalPages());          // 21개 → 3페이지
}

#endif  // SOS_TEST_MODE

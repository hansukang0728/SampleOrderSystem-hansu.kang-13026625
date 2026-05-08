#pragma once
#include <string>
#include <vector>
#include <random>
#include <ctime>
#include <cmath>
#include <algorithm>
#include "Model/app_db.h"

// DummyDataGenerator 프로젝트의 이름 풀·난수 로직을 AppDB 기반으로 이식
class DummyDataService {
public:
    static const int DEFAULT_SAMPLE_COUNT = 10;
    static const int DEFAULT_ORDER_COUNT  =  5;
    static const int MAX_SAMPLES          = 100;
    static const int MAX_ORDERS           = 50;

    explicit DummyDataService(AppDB& db) : db_(db) {}

    // D-01: 시료 더미 생성 — 기존 시료에 추가 (AppDB.createSample 위임)
    std::vector<Sample> generateSamples(int count,
                                        unsigned seed = 0) {
        if (count <= 0) return {};
        if (seed == 0) seed = static_cast<unsigned>(std::time(nullptr));

        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> timeDist(10.0, 120.0);
        std::uniform_real_distribution<double> yieldDist(0.70, 0.99);
        std::uniform_int_distribution<int>     stockDist(0, 200);

        int base = static_cast<int>(db_.samples().size()); // 기존 수 기준 이름 결정
        std::vector<Sample> created;
        created.reserve(count);

        for (int i = 0; i < count; ++i) {
            int idx = base + i;
            std::string name = makeName(idx) + "-시료";

            double avgTime   = std::round(timeDist(rng) * 10.0) / 10.0;
            double yieldRate = std::round(yieldDist(rng) * 1000.0) / 1000.0;
            int    stock     = stockDist(rng);

            created.push_back(db_.createSample(name, avgTime, yieldRate, stock));
        }
        return created;
    }

    // D-02: 주문 더미 생성 — 등록된 시료 기반 무작위 주문 생성
    std::vector<Order> generateOrders(int count,
                                      unsigned seed = 0) {
        const auto& samples = db_.samples();
        if (samples.empty() || count <= 0) return {};
        if (seed == 0) seed = static_cast<unsigned>(std::time(nullptr));

        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> sIdx(
            0, static_cast<int>(samples.size()) - 1);
        std::uniform_int_distribution<int> qtyDist(1, 50);
        std::uniform_int_distribution<int> cIdx(0, CUSTOMER_POOL_SIZE - 1);

        std::vector<Order> created;
        created.reserve(count);
        for (int i = 0; i < count; ++i) {
            const Sample& s = samples[sIdx(rng)];
            created.push_back(
                db_.createOrder(s.id, qtyDist(rng), CUSTOMER_POOL[cIdx(rng)]));
        }
        return created;
    }

    // D-03: 전체 데이터 초기화
    void resetAll() { db_.resetAll(); }

private:
    AppDB& db_;

    // ── 이름 풀 (DummyDataGenerator 프로젝트와 동일) ─────────────
    static inline const std::string NAME_POOL[] = {
        "알파", "베타", "감마", "델타", "엡실론",
        "제타", "에타", "세타", "이오타", "카파",
        "람다", "뮤",   "뉴",   "크시",  "오미크론",
        "파이", "로",   "시그마", "타우", "웁실론"
    };
    static constexpr int POOL_SIZE = 20;

    static inline const std::string CUSTOMER_POOL[] = {
        "홍길동", "김철수", "이영희", "박민준", "최지원",
        "정수현", "강민서", "윤서연", "임태양", "한지은"
    };
    static constexpr int CUSTOMER_POOL_SIZE = 10;

    // 인덱스 → 이름 (풀 순환, 21번째부터 번호 접미사)
    static std::string makeName(int idx) {
        std::string base = NAME_POOL[idx % POOL_SIZE];
        if (idx >= POOL_SIZE)
            base += "-" + std::to_string(idx / POOL_SIZE + 1);
        return base;
    }
};

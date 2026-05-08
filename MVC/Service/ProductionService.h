#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <ctime>
#include "Model/app_db.h"

class ProductionService {
public:
    explicit ProductionService(AppDB& db) : db_(db) {}

    // 현재 IN_PROGRESS 항목 — 단순 const 조회 (부작용 없음)
    // FIFO 자동 시작은 AppDB::checkAndComplete() / enqueue()에서 처리
    // checkAndComplete()는 render() 내에서만 1회 실행
    const ProductionQueueItem* inProgressItem() const {
        for (const auto& p : db_.queue())
            if (p.isInProgress()) return &p;
        return nullptr;
    }

    // WAITING 항목 목록 (FIFO: enqueued_at 오름차순 = 내부 저장 순서)
    std::vector<const ProductionQueueItem*> waitingQueue() const {
        std::vector<const ProductionQueueItem*> result;
        for (const auto& p : db_.queue())
            if (p.isWaiting()) result.push_back(&p);
        return result;
    }

    // 추정 현재 생산량 (진행률 × actual_qty / 100, 소수점 버림)
    static int currentProduction(const ProductionQueueItem& p) {
        if (!p.isInProgress()) return 0;
        double pct = p.progressPct();  // [0, 100] 클램핑 보장
        return static_cast<int>(pct * p.actual_qty / 100.0);
    }

    // 예상 완료 시각 "YYYY-MM-DD HH:MM" — WAITING이면 "-"
    static std::string estimatedCompletion(const ProductionQueueItem& p) {
        if (p.started_at.empty()) return "-";
        std::time_t start = parseTime(p.started_at);
        if (start == -1) return "-";
        std::time_t done = start + static_cast<std::time_t>(p.total_time * 60.0);
        struct tm tm{};
        localtime_s(&tm, &done);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
        return buf;
    }

    // AppDB 접근 (View에서 주문·시료 정보 조회용)
    AppDB& db() { return db_; }

private:
    AppDB& db_;
};

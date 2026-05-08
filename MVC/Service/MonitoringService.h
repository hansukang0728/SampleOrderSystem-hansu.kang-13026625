#pragma once
#include <string>
#include <vector>
#include "Model/app_db.h"
#include "View/ConsoleUI.h"

struct DashboardData {
    // M-02: 주문 현황 (REJECTED 제외)
    int cntReserved  = 0;
    int cntProducing = 0;
    int cntConfirmed = 0;
    int cntRelease   = 0;
    int activeOrders = 0;  // REJECTED 제외 전체 합

    // M-01: 전광판용 집계
    int totalStock   = 0;

    // 업데이트 시각 (collect() 호출 시점)
    std::string updatedAt;
};

struct SampleStockInfo {
    std::string status;          // "여유" / "부족" / "고갈"
    const char* icon  = "●";
    const char* color = UI::GRN;
    int reservedDemand     = 0;  // 해당 시료 RESERVED 수요 합계
    int productionIncoming = 0;  // 미완료 큐 actual_qty 합계
};

class MonitoringService {
public:
    explicit MonitoringService(AppDB& db) : db_(db) {}

    // 집계 + 생산 완료 자동 처리
    // ① updatedAt 캡처 → ② checkAndComplete() → ③ 카운팅·합산
    DashboardData collect() {
        DashboardData d;
        d.updatedAt = nowStr();     // 시각 먼저 캡처

        db_.checkAndComplete();     // IN_PROGRESS 자동 완료

        for (const auto& o : db_.orders()) {
            switch (o.status) {
            case OrderStatus::RESERVED:  ++d.cntReserved;  ++d.activeOrders; break;
            case OrderStatus::PRODUCING: ++d.cntProducing; ++d.activeOrders; break;
            case OrderStatus::CONFIRMED: ++d.cntConfirmed; ++d.activeOrders; break;
            case OrderStatus::RELEASE:   ++d.cntRelease;   ++d.activeOrders; break;
            case OrderStatus::REJECTED:  break;  // 제외
            }
        }
        for (const auto& s : db_.samples())
            d.totalStock += s.stock;

        return d;
    }

    // 시료별 재고 상태 계산 (주문 수요·생산 입고량 고려)
    SampleStockInfo calcStockInfo(const std::string& sampleId) const {
        SampleStockInfo info;
        const Sample* s = db_.findSample(sampleId);
        if (!s) return info;

        // RESERVED 수요: 해당 시료의 RESERVED 주문 quantity 합계
        int reservedDemand = 0;
        for (const auto& o : db_.orders())
            if (o.sample_id == sampleId && o.status == OrderStatus::RESERVED)
                reservedDemand += o.quantity;

        // 생산 중 입고 예정: 미완료 큐 actual_qty 합계
        int productionIncoming = 0;
        for (const auto& p : db_.queue())
            if (p.sample_id == sampleId && !p.completed)
                productionIncoming += p.actual_qty;

        info.reservedDemand     = reservedDemand;
        info.productionIncoming = productionIncoming;

        if (s->stock == 0) {
            info.status = "고갈"; info.icon = "✕"; info.color = UI::RED;
        } else if (s->stock + productionIncoming < reservedDemand) {
            info.status = "부족"; info.icon = "▲"; info.color = UI::YLW;
        } else {
            info.status = "여유"; info.icon = "●"; info.color = UI::GRN;
        }
        return info;
    }

    // View에서 samples() 직접 참조용 (collect() 이후에만 사용)
    AppDB& db() { return db_; }

private:
    AppDB& db_;
};

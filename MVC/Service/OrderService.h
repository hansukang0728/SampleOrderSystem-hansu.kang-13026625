#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>
#include "Model/app_db.h"

class OrderService {
public:
    // approveOrder() 결과 — View가 Case 분기 출력에 사용
    struct ApproveResult {
        bool   success    = false;
        bool   sufficient = false;  // true: 재고 충분, false: 재고 부족
        int    prevStock  = 0;      // 승인 전 재고 (Case 1 출력용)
        int    shortage   = 0;
        int    actualQty  = 0;
        double totalTime  = 0.0;
    };

    explicit OrderService(AppDB& db) : db_(db) {}

    // O-01: 주문 생성 → RESERVED
    Order createOrder(const std::string& sampleId, int qty,
                      const std::string& customer) {
        return db_.createOrder(sampleId, qty, customer);
    }

    // 전체 조회
    const std::vector<Order>& all() const { return db_.orders(); }

    Order* findById(const std::string& id) { return db_.findOrder(id); }

    // RESERVED 주문만 필터링 (enqueued_at 오름차순 유지 — 저장 순서 = FIFO)
    std::vector<const Order*> reservedOrders() const {
        std::vector<const Order*> result;
        for (const auto& o : db_.orders())
            if (o.status == OrderStatus::RESERVED)
                result.push_back(&o);
        return result;
    }

    // O-02: 주문 승인 — 재고 상황에 따라 자동 분기
    ApproveResult approveOrder(const std::string& orderId) {
        ApproveResult r;
        Order* o = db_.findOrder(orderId);
        if (!o || o->status != OrderStatus::RESERVED) return r;

        Sample* s = db_.findSample(o->sample_id);
        if (!s) return r;

        r.prevStock = s->stock;

        if (s->stock >= o->quantity) {
            // Case 1: 재고 충분 → 즉시 CONFIRMED
            s->stock -= o->quantity;
            o->status  = OrderStatus::CONFIRMED;
            r.sufficient = true;
        } else {
            // Case 2: 재고 부족 → 생산 큐 등록
            r.shortage  = o->quantity - s->stock;
            r.actualQty = static_cast<int>(
                std::ceil(r.shortage / (s->yield_rate * 0.9)));
            r.totalTime = s->avg_production_time * r.actualQty;

            s->stock   = 0;          // 기존 재고 전량 소진
            o->status  = OrderStatus::PRODUCING;

            db_.enqueue(o->id, s->id, r.shortage, r.actualQty, r.totalTime);
            r.sufficient = false;
        }

        db_.updateSample(*s);
        db_.updateOrder(*o);
        r.success = true;
        return r;
    }

    // O-03: 주문 거절 → REJECTED (재고 변화 없음)
    bool rejectOrder(const std::string& orderId) {
        Order* o = db_.findOrder(orderId);
        if (!o || o->status != OrderStatus::RESERVED) return false;
        o->status = OrderStatus::REJECTED;
        db_.updateOrder(*o);
        return true;
    }

    // ── 유효성 검증 ──────────────────────────────────────────

    // 공백 트림 후 비어있지 않아야 함
    static bool validateCustomerName(const std::string& name) {
        return std::any_of(name.begin(), name.end(),
                           [](unsigned char c){ return !std::isspace(c); });
    }

    // 1 이상
    static bool validateQuantity(int qty) { return qty >= 1; }

private:
    AppDB& db_;
};

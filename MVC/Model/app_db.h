#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "models.h"

class AppDB {
public:
    explicit AppDB(const std::string& path) : path_(path) { load(); }

    // ── Sample ──────────────────────────────────────────────

    Sample createSample(const std::string& name, double avgTime,
                        double yieldRate, int stock) {
        Sample s;
        s.id                  = nextSampleId();
        s.name                = name;
        s.avg_production_time = avgTime;
        s.yield_rate          = yieldRate;
        s.stock               = stock;
        samples_.push_back(s);
        save();
        return s;
    }

    const std::vector<Sample>& samples() const { return samples_; }

    Sample* findSample(const std::string& id) {
        for (auto& s : samples_)
            if (s.id == id) return &s;
        return nullptr;
    }

    // id 일치 항목 전체 교체 (부분 업데이트 아님)
    bool updateSample(const Sample& updated) {
        for (auto& s : samples_) {
            if (s.id == updated.id) { s = updated; save(); return true; }
        }
        return false;
    }

    // ── Order ────────────────────────────────────────────────

    Order createOrder(const std::string& sampleId, int qty,
                      const std::string& customer) {
        Order o;
        o.id            = nextOrderId();
        o.sample_id     = sampleId;
        o.quantity      = qty;
        o.customer_name = customer;
        o.status        = OrderStatus::RESERVED;
        o.created_at    = nowStr();
        orders_.push_back(o);
        save();
        return o;
    }

    const std::vector<Order>& orders() const { return orders_; }

    Order* findOrder(const std::string& id) {
        for (auto& o : orders_)
            if (o.id == id) return &o;
        return nullptr;
    }

    // id 일치 항목 전체 교체 (부분 업데이트 아님)
    bool updateOrder(const Order& updated) {
        for (auto& o : orders_) {
            if (o.id == updated.id) { o = updated; save(); return true; }
        }
        return false;
    }

    // ── ProductionQueueItem ──────────────────────────────────

    ProductionQueueItem enqueue(const std::string& orderId,
                                const std::string& sampleId,
                                int shortage, int actualQty, double totalTime) {
        ProductionQueueItem p;
        p.id          = nextQueueId();
        p.order_id    = orderId;
        p.sample_id   = sampleId;
        p.shortage    = shortage;
        p.actual_qty  = actualQty;
        p.total_time  = totalTime;
        p.completed   = false;
        p.enqueued_at = nowStr();
        p.started_at  = "";

        // FIFO 자동 시작: IN_PROGRESS도 WAITING도 없으면 이 항목이 즉시 시작
        bool hasActive = false;
        for (const auto& q : queue_)
            if (q.isInProgress() || q.isWaiting()) { hasActive = true; break; }
        if (!hasActive) p.started_at = nowStr();

        queue_.push_back(p);
        save();
        return p;
    }

    // 반환 전 checkAndComplete() 자동 실행 — 항상 최신 완료 상태 보장
    std::vector<ProductionQueueItem>& queue() {
        checkAndComplete();
        return queue_;
    }

    // id 일치 항목 전체 교체 (부분 업데이트 아님)
    bool updateQueueItem(const ProductionQueueItem& updated) {
        for (auto& p : queue_) {
            if (p.id == updated.id) { p = updated; save(); return true; }
        }
        return false;
    }

    // FIFO: 첫 번째 WAITING 항목 반환 (없으면 nullptr)
    // 반환 전 checkAndComplete() 자동 실행
    ProductionQueueItem* frontWaiting() {
        checkAndComplete();
        for (auto& p : queue_)
            if (p.isWaiting()) return &p;
        return nullptr;
    }

    // D-03: 전체 초기화 — 모든 컬렉션 비우고 저장
    void resetAll() {
        samples_.clear();
        orders_.clear();
        queue_.clear();
        save();
    }

    // IN_PROGRESS 항목 완료 처리 + 다음 WAITING 자동 시작 (FIFO)
    void checkAndComplete() {
        bool changed = false;

        // ① 완료 처리
        for (auto& p : queue_) {
            if (p.isDone() || !p.isTimeElapsed()) continue;
            p.completed = true;
            if (auto* s = findSample(p.sample_id)) s->stock += p.actual_qty;
            if (auto* o = findOrder(p.order_id))   o->status = OrderStatus::CONFIRMED;
            changed = true;
        }

        // ② FIFO 자동 시작: IN_PROGRESS 없고 WAITING 존재하면 첫 번째 항목 시작
        bool hasInProgress = false;
        for (const auto& p : queue_)
            if (p.isInProgress()) { hasInProgress = true; break; }

        if (!hasInProgress) {
            for (auto& p : queue_) {
                if (p.isWaiting()) {
                    p.started_at = nowStr();
                    changed = true;
                    break;  // FIFO: 첫 번째 WAITING만 시작
                }
            }
        }

        if (changed) save();
    }

private:
    void load() {
        auto root = JsonValue::loadFile(path_);

        if (root.contains("samples"))
            for (const auto& j : root["samples"].arr)
                samples_.push_back(Sample::fromJson(j));

        if (root.contains("orders"))
            for (const auto& j : root["orders"].arr)
                orders_.push_back(Order::fromJson(j));

        if (root.contains("production_queue"))
            for (const auto& j : root["production_queue"].arr)
                queue_.push_back(ProductionQueueItem::fromJson(j));
    }

    void save() const {
        auto root = JsonValue::makeObject();

        auto sarr = JsonValue::makeArray();
        for (const auto& s : samples_) sarr.push(s.toJson());
        root["samples"] = sarr;

        auto oarr = JsonValue::makeArray();
        for (const auto& o : orders_) oarr.push(o.toJson());
        root["orders"] = oarr;

        auto qarr = JsonValue::makeArray();
        for (const auto& p : queue_) qarr.push(p.toJson());
        root["production_queue"] = qarr;

        root.saveFile(path_);
    }

    // 기존 최대 번호 + 1 → "S-NNN" (삭제 후 재생성 시 번호 재사용 없음)
    std::string nextSampleId() const {
        int maxN = 0;
        for (const auto& s : samples_) {
            int n = 0;
            sscanf_s(s.id.c_str(), "S-%d", &n);
            maxN = std::max(maxN, n);
        }
        return formatSampleId(maxN + 1);
    }

    // 당일 최대 순번 + 1 → "ORD-YYYYMMDD-XXXX"
    std::string nextOrderId() const {
        std::string today  = todayStr();
        std::string prefix = "ORD-" + today + "-";
        int maxSeq = 0;
        for (const auto& o : orders_) {
            if (o.id.size() >= prefix.size() &&
                o.id.substr(0, prefix.size()) == prefix) {
                int n = 0;
                sscanf_s(o.id.c_str() + prefix.size(), "%d", &n);
                maxSeq = std::max(maxSeq, n);
            }
        }
        return formatOrderId(today, maxSeq + 1);
    }

    // 기존 최대 id + 1
    int nextQueueId() const {
        int maxId = 0;
        for (const auto& p : queue_) maxId = std::max(maxId, p.id);
        return maxId + 1;
    }

    std::string                      path_;
    std::vector<Sample>              samples_;
    std::vector<Order>               orders_;
    std::vector<ProductionQueueItem> queue_;
};

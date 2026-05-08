#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include "Model/app_db.h"

class OrderService {
public:
    explicit OrderService(AppDB& db) : db_(db) {}

    // O-01: 주문 생성 → RESERVED (ID·created_at 자동 생성은 AppDB 담당)
    Order createOrder(const std::string& sampleId, int qty,
                      const std::string& customer) {
        return db_.createOrder(sampleId, qty, customer);
    }

    // 조회 (Phase 4-4 이후 활용)
    const std::vector<Order>& all() const { return db_.orders(); }

    Order* findById(const std::string& id) { return db_.findOrder(id); }

    // ── 유효성 검증 (bool 반환, 오류 메시지 출력은 View 담당) ──

    // 공백 트림 후 비어있지 않아야 함 ("   " 불가)
    static bool validateCustomerName(const std::string& name) {
        return std::any_of(name.begin(), name.end(),
                           [](unsigned char c){ return !std::isspace(c); });
    }

    // 1 이상
    static bool validateQuantity(int qty) {
        return qty >= 1;
    }

private:
    AppDB& db_;
};

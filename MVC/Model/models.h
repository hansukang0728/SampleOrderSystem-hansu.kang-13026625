#pragma once
#include <string>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "json_lite.h"

// ── 네임스페이스: 전역 오염 방지 ─────────────────────────────
namespace sos {

// ── 유틸리티 ──────────────────────────────────────────────────

// 현재 로컬 시각 → struct tm (내부 헬퍼)
inline struct tm localNow() {
    auto t = std::chrono::system_clock::to_time_t(
                 std::chrono::system_clock::now());
    struct tm tm{};
    localtime_s(&tm, &t);
    return tm;
}

// 현재 시각 → "YYYY-MM-DD HH:MM:SS"
inline std::string nowStr() {
    char buf[32];
    auto tm = localNow();
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// 오늘 날짜 → "YYYYMMDD"
inline std::string todayStr() {
    char buf[16];
    auto tm = localNow();
    std::strftime(buf, sizeof(buf), "%Y%m%d", &tm);
    return buf;
}

// 번호 → "S-NNN"
inline std::string formatSampleId(int n) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "S-%03d", n);
    return buf;
}

// "ORD-YYYYMMDD-XXXX" 형식 생성   사용 예: formatOrderId(todayStr(), nextSeq)
inline std::string formatOrderId(const std::string& date, int seq) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "ORD-%s-%04d", date.c_str(), seq);
    return buf;
}

// "YYYY-MM-DD HH:MM:SS" → time_t, 파싱 실패 시 -1 반환
inline std::time_t parseTime(const std::string& s) {
    struct tm tm{};
    int y, mo, d, h, mi, sec;
    if (sscanf_s(s.c_str(), "%d-%d-%d %d:%d:%d",
                 &y, &mo, &d, &h, &mi, &sec) != 6) return -1;
    tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
    tm.tm_hour = h;        tm.tm_min = mi;      tm.tm_sec  = sec;
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

// started_at 기준 경과 시간(분), 빈 문자열이거나 파싱 실패 시 0.0
inline double elapsedMinutes(const std::string& started_at) {
    if (started_at.empty()) return 0.0;
    auto start = parseTime(started_at);
    if (start == -1) return 0.0;  // 파싱 실패 방어
    auto now = std::chrono::system_clock::to_time_t(
                   std::chrono::system_clock::now());
    return std::difftime(now, start) / 60.0;
}

} // namespace sos

// 편의를 위해 using 선언 (models.h 포함하는 코드에서 sos:: 없이 사용 가능)
using sos::nowStr;
using sos::todayStr;
using sos::formatSampleId;
using sos::formatOrderId;
using sos::parseTime;
using sos::elapsedMinutes;

// ── Sample (시료) ─────────────────────────────────────────────

struct Sample {
    std::string id;
    std::string name;
    double      avg_production_time = 0.0;  // 분/개
    double      yield_rate          = 0.0;  // 0.0 ~ 1.0
    int         stock               = 0;    // ea

    JsonValue toJson() const {
        auto o = JsonValue::makeObject();
        o["id"]                  = JsonValue(id);
        o["name"]                = JsonValue(name);
        o["avg_production_time"] = JsonValue(avg_production_time);
        o["yield_rate"]          = JsonValue(yield_rate);
        o["stock"]               = JsonValue(stock);
        return o;
    }

    static Sample fromJson(const JsonValue& j) {
        Sample s;
        s.id                  = j["id"].asString();
        s.name                = j["name"].asString();
        s.avg_production_time = j["avg_production_time"].asDouble();
        s.yield_rate          = j["yield_rate"].asDouble();
        s.stock               = j["stock"].asInt();
        return s;
    }

    // qty 파라미터는 멤버 stock과 구분하기 위해 별도 명칭 사용
    static std::string stockStatus(int qty) {
        if (qty == 0)   return "고갈";
        if (qty <= 10)  return "부족";
        return "여유";
    }
};

// ── Order (주문) ──────────────────────────────────────────────

enum class OrderStatus { RESERVED, REJECTED, PRODUCING, CONFIRMED, RELEASE };

struct Order {
    std::string id;
    std::string sample_id;
    int         quantity      = 0;  // 기본값 0 = 미초기화; Service가 유효값(>=1) 보장
    std::string customer_name;
    OrderStatus status        = OrderStatus::RESERVED;
    std::string created_at;

    static std::string statusToString(OrderStatus s) {
        switch (s) {
        case OrderStatus::RESERVED:  return "RESERVED";
        case OrderStatus::REJECTED:  return "REJECTED";
        case OrderStatus::PRODUCING: return "PRODUCING";
        case OrderStatus::CONFIRMED: return "CONFIRMED";
        case OrderStatus::RELEASE:   return "RELEASE";
        default:                     return "UNKNOWN";
        }
    }

    // 알 수 없는 값 → RESERVED 폴백 (의도적 설계: data.json 손상 시 안전 복원)
    static OrderStatus stringToStatus(const std::string& s) {
        if (s == "REJECTED")  return OrderStatus::REJECTED;
        if (s == "PRODUCING") return OrderStatus::PRODUCING;
        if (s == "CONFIRMED") return OrderStatus::CONFIRMED;
        if (s == "RELEASE")   return OrderStatus::RELEASE;
        return OrderStatus::RESERVED;
    }

    JsonValue toJson() const {
        auto o = JsonValue::makeObject();
        o["id"]            = JsonValue(id);
        o["sample_id"]     = JsonValue(sample_id);
        o["quantity"]      = JsonValue(quantity);
        o["customer_name"] = JsonValue(customer_name);
        o["status"]        = JsonValue(statusToString(status));
        o["created_at"]    = JsonValue(created_at);
        return o;
    }

    static Order fromJson(const JsonValue& j) {
        Order o;
        o.id            = j["id"].asString();
        o.sample_id     = j["sample_id"].asString();
        o.quantity      = j["quantity"].asInt();
        o.customer_name = j["customer_name"].asString();
        o.status        = stringToStatus(j["status"].asString());
        o.created_at    = j["created_at"].asString();
        return o;
    }
};

// ── ProductionQueueItem (생산 큐 항목) ────────────────────────

struct ProductionQueueItem {
    int         id          = 0;   // AppDB가 enqueue 시 유효 순번 부여
    std::string order_id;
    std::string sample_id;
    int         shortage    = 0;
    int         actual_qty  = 0;
    double      total_time  = 0.0;
    bool        completed   = false;
    std::string enqueued_at;
    std::string started_at;        // 비어있음 = WAITING, 채워짐 = IN_PROGRESS

    bool isWaiting()    const { return !completed && started_at.empty(); }
    bool isInProgress() const { return !completed && !started_at.empty(); }
    bool isDone()       const { return completed; }

    double progressPct() const {
        if (!isInProgress() || total_time <= 0.0) return 0.0;
        double pct = elapsedMinutes(started_at) / total_time * 100.0;
        return std::min(std::max(pct, 0.0), 100.0);  // [0, 100] 범위 보장
    }

    bool isTimeElapsed() const {
        return isInProgress() && elapsedMinutes(started_at) >= total_time;
    }

    JsonValue toJson() const {
        auto o = JsonValue::makeObject();
        o["id"]          = JsonValue(id);
        o["order_id"]    = JsonValue(order_id);
        o["sample_id"]   = JsonValue(sample_id);
        o["shortage"]    = JsonValue(shortage);
        o["actual_qty"]  = JsonValue(actual_qty);
        o["total_time"]  = JsonValue(total_time);
        o["completed"]   = JsonValue(completed);
        o["enqueued_at"] = JsonValue(enqueued_at);
        o["started_at"]  = JsonValue(started_at);
        return o;
    }

    static ProductionQueueItem fromJson(const JsonValue& j) {
        ProductionQueueItem p;
        p.id          = j["id"].asInt();
        p.order_id    = j["order_id"].asString();
        p.sample_id   = j["sample_id"].asString();
        p.shortage    = j["shortage"].asInt();
        p.actual_qty  = j["actual_qty"].asInt();
        p.total_time  = j["total_time"].asDouble();
        p.completed   = j["completed"].asBool();
        p.enqueued_at = j["enqueued_at"].asString();
        p.started_at  = j.contains("started_at")
                        ? j["started_at"].asString() : "";
        return p;
    }
};

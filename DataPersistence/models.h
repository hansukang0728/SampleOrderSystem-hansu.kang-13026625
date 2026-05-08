#pragma once
#include <string>
#include "json_lite.h"

struct Sample {
    int         id                  = 0;
    std::string name;
    double      avg_production_time = 0.0;  // 분/개
    double      yield_rate          = 0.0;  // 0.0 ~ 1.0

    JsonValue toJson() const {
        auto o = JsonValue::makeObject();
        o["id"]                  = JsonValue(id);
        o["name"]                = JsonValue(name);
        o["avg_production_time"] = JsonValue(avg_production_time);
        o["yield_rate"]          = JsonValue(yield_rate);
        return o;
    }

    static Sample fromJson(const JsonValue& j) {
        Sample s;
        s.id                  = j["id"].asInt();
        s.name                = j["name"].asString();
        s.avg_production_time = j["avg_production_time"].asDouble();
        s.yield_rate          = j["yield_rate"].asDouble();
        return s;
    }
};

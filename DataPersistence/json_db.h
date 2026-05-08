#pragma once
#include <vector>
#include <algorithm>
#include "models.h"

class SampleDB {
    std::string         path_;
    std::vector<Sample> cache_;
    int                 next_id_ = 1;

    void load() {
        auto root = JsonValue::loadFile(path_);
        if (root.contains("next_id"))
            next_id_ = root["next_id"].asInt();
        if (root.contains("samples")) {
            for (const auto& j : root["samples"].arr)
                cache_.push_back(Sample::fromJson(j));
        }
    }

    void save() const {
        auto root = JsonValue::makeObject();
        root["next_id"] = JsonValue(next_id_);
        auto arr = JsonValue::makeArray();
        for (const auto& s : cache_)
            arr.push(s.toJson());
        root["samples"] = arr;
        root.saveFile(path_);
    }

public:
    explicit SampleDB(const std::string& path) : path_(path) {
        load();
    }

    // CREATE
    Sample create(const std::string& name, double avg_time, double yield) {
        Sample s;
        s.id                  = next_id_++;
        s.name                = name;
        s.avg_production_time = avg_time;
        s.yield_rate          = yield;
        cache_.push_back(s);
        save();
        return s;
    }

    // READ ALL
    const std::vector<Sample>& all() const { return cache_; }

    // READ ONE
    Sample* findById(int id) {
        for (auto& s : cache_)
            if (s.id == id) return &s;
        return nullptr;
    }

    // UPDATE
    bool update(const Sample& updated) {
        for (auto& s : cache_) {
            if (s.id == updated.id) {
                s = updated;
                save();
                return true;
            }
        }
        return false;
    }

    // DELETE
    bool remove(int id) {
        auto it = std::remove_if(cache_.begin(), cache_.end(),
                                 [id](const Sample& s) { return s.id == id; });
        if (it == cache_.end()) return false;
        cache_.erase(it, cache_.end());
        save();
        return true;
    }
};

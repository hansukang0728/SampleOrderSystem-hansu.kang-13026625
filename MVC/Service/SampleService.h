#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include "Model/app_db.h"

class SampleService {
public:
    static const int PAGE_SIZE = 10;

    explicit SampleService(AppDB& db) : db_(db) {}

    // S-01: 시료 등록 (ID 자동 생성은 AppDB 담당)
    Sample add(const std::string& name, double avgTime,
               double yieldRate, int stock) {
        return db_.createSample(name, avgTime, yieldRate, stock);
    }

    // S-02: 전체 조회
    const std::vector<Sample>& all() const { return db_.samples(); }

    // S-03: ID 조회
    Sample* findById(const std::string& id) { return db_.findSample(id); }

    // S-04: 이름 검색 (부분 일치, 대소문자 구분 없음)
    // 반환값은 출력 전용 — add() 호출 후 포인터 재사용 금지
    std::vector<const Sample*> searchByName(const std::string& keyword) const {
        std::vector<const Sample*> result;
        std::string kw = keyword;
        std::transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
        for (const auto& s : db_.samples()) {
            std::string name = s.name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name.find(kw) != std::string::npos)
                result.push_back(&s);
        }
        return result;
    }

    // 페이지네이션 — 총 페이지 수 (0개면 0)
    int totalPages() const {
        int cnt = static_cast<int>(db_.samples().size());
        if (cnt == 0) return 0;
        return (cnt + PAGE_SIZE - 1) / PAGE_SIZE;
    }

    // ── 유효성 검증 (bool 반환, 오류 메시지 출력은 View 담당) ──

    static bool validateName(const std::string& name) {
        return !name.empty();
    }

    static bool validateAvgTime(double t) {
        return t > 0.0;
    }

    // 0.0 초과 ~ 1.0 이하
    static bool validateYieldRate(double y) {
        return y > 0.0 && y <= 1.0;
    }

    // 0 이상
    static bool validateStock(int s) {
        return s >= 0;
    }

private:
    AppDB& db_;
};

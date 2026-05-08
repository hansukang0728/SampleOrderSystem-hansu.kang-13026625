#include "SampleController.h"

void SampleController::addSample(const std::string& name, double avgProdTime, double yieldRate, int stock) {
    samples_.push_back({ nextId_++, name, avgProdTime, yieldRate, stock });
}

const std::vector<Sample>& SampleController::getAllSamples() const {
    return samples_;
}

Sample* SampleController::findById(int id) {
    for (auto& s : samples_)
        if (s.id == id) return &s;
    return nullptr;
}

std::vector<Sample*> SampleController::searchByName(const std::string& keyword) {
    std::vector<Sample*> result;
    for (auto& s : samples_)
        if (s.name.find(keyword) != std::string::npos)
            result.push_back(&s);
    return result;
}

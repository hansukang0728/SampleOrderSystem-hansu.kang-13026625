#pragma once
#include "../Model/Sample.h"
#include <vector>
#include <string>

class SampleController {
public:
    void addSample(const std::string& name, double avgProdTime, double yieldRate, int stock);
    const std::vector<Sample>& getAllSamples() const;
    Sample* findById(int id);
    std::vector<Sample*> searchByName(const std::string& keyword);

private:
    std::vector<Sample> samples_;
    int nextId_ = 1;
};

#pragma once
#include "simulation.hpp"
#include "api_budget.hpp"

namespace apo {
class OpenAIClient final : public IDecider, public ICycleReporter {
 public:
  OpenAIClient(std::string key, std::string model, std::string base_url, ApiCallBudget budget);
  Decision decide(const Perception&) override;
  json report_cycle(int cycle, const Agent& agent) override;
 private: std::string key_, model_, base_url_; ApiCallBudget budget_;
};
}

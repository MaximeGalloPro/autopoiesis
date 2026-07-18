#pragma once
#include "simulation.hpp"
#include "api_budget.hpp"

namespace apo {
std::string period_report_instructions();
std::string evolution_request_instructions();

class OpenAIClient final : public IDecider, public ICycleReporter {
 public:
  OpenAIClient(std::string key, std::string model, std::string base_url, ApiCallBudget budget);
  Decision decide(const Perception&) override;
  json report_period(int simulation_cycle, int day, const Agent& agent,
                     const std::vector<std::string>& history) override;
  json request_evolution(int simulation_cycle, int day, const Agent& agent,
                         const std::vector<std::string>& history,
                         const json& report) override;
 private:
  std::string key_, model_, base_url_;
  ApiCallBudget budget_;
  int proposal_window_cycle_{-1};
  std::vector<json> proposals_in_window_;
};
}

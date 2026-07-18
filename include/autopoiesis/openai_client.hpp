#pragma once
#include "simulation.hpp"
#include "api_budget.hpp"

namespace apo {
std::string period_report_instructions();
std::string evolution_request_instructions();
std::string api_http_diagnostic(long status, const std::string& response_body,
                                long duration_ms);

class OpenAIClient final : public IDecider, public ICycleReporter {
 public:
  using DiagnosticSink = std::function<void(const std::string&)>;
  OpenAIClient(std::string key, std::string model, std::string base_url,
               ApiCallBudget budget, DiagnosticSink diagnostic_sink = {});
  Decision decide(const Perception&) override;
  json report_period(int simulation_cycle, int day, const Agent& agent,
                     const std::vector<std::string>& history) override;
  json request_evolution(int simulation_cycle, int day, const Agent& agent,
                         const std::vector<std::string>& history,
                         const json& report) override;
  std::string last_error() const override { return last_error_; }
 private:
  std::string key_, model_, base_url_;
  ApiCallBudget budget_;
  int proposal_window_cycle_{-1};
  std::vector<json> proposals_in_window_;
  DiagnosticSink diagnostic_sink_;
  std::string last_error_;
  void record_result(const std::string& operation, const std::string& agent_name,
                     const std::string& error);
};
}

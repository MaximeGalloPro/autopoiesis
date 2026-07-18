#pragma once
#include "types.hpp"
#include "decision.hpp"
#include <fstream>
#include <set>

namespace apo {
class Logger {
 public:
  explicit Logger(const std::string& directory = "/data");
  void event(int simulation_cycle, int day, const Agent& before, const Decision& decision,
             const std::string& result, const Agent& after);
  void feature_request(int simulation_cycle, int day, const Agent& agent, const Decision& decision);
  void ai_report(int simulation_cycle, int day, const Agent& agent, const json& report);
  void ai_feature_request(int simulation_cycle, int day, const Agent& agent,
                          const json& report, const json& request);
  std::string devil_constraint(int simulation_cycle, int day, const json& request);
  std::set<std::string> known_evolution_keys() const;
  void message(const std::string& line);
  const std::vector<std::string>& recent() const { return recent_; }
 private:
  std::ofstream readable_, structured_; std::vector<std::string> recent_; std::string directory_, request_prefix_; unsigned long request_counter_{0};
  std::set<std::string> evolution_keys_;
  int evolution_window_cycle_{-1};
};
}

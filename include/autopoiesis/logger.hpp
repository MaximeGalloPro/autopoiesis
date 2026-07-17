#pragma once
#include "types.hpp"
#include "decision.hpp"
#include <fstream>

namespace apo {
class Logger {
 public:
  explicit Logger(const std::string& directory = "/data");
  void event(int cycle, const Agent& before, const Decision& decision, const std::string& result, const Agent& after);
  void feature_request(int cycle, const Agent& agent, const Decision& decision);
  void ai_report(int cycle, const Agent& agent, const json& report);
  void ai_feature_request(int cycle, const Agent& agent, const json& report, const json& request);
  void message(const std::string& line);
  const std::vector<std::string>& recent() const { return recent_; }
 private:
  std::ofstream readable_, structured_; std::vector<std::string> recent_; std::string directory_; unsigned long request_counter_{0};
};
}

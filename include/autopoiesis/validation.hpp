#pragma once

#include "types.hpp"
#include <iosfwd>
#include <set>
#include <string>
#include <vector>

namespace apo {
enum class ValidationStage { Empty, Choose, Confirm, Complete };

struct ValidationPrompt {
  ValidationStage stage{ValidationStage::Empty};
  int day{};
  int simulation_cycle{};
  std::vector<json> requests;
  std::size_t selected_index{};
};

enum class EvolutionProgressStage {
  Queued, Preparing, Implementing, Reporting, Verifying, Correcting, Activating,
  Complete, Failed, TimedOut
};

struct EvolutionProgress {
  EvolutionProgressStage stage{EvolutionProgressStage::Queued};
  std::string request_id;
  std::string message;
  std::string detail;
  long long elapsed_seconds{};
  bool successful{};
};

class IValidationInterface {
 public:
  virtual ~IValidationInterface() = default;
  virtual std::string request_command(const ValidationPrompt& prompt) = 0;
  virtual bool present_evolution_progress(const EvolutionProgress&) { return true; }
  virtual std::string request_evolution_completion(const EvolutionProgress&) { return "o"; }
};

class HumanValidation {
 public:
  HumanValidation(std::string data_directory, std::istream& input, std::ostream& output,
                  IValidationInterface* interface = nullptr);
  bool review_window(int day, int simulation_cycle);
  bool wait_for_evolution(const std::string& request_id);

 private:
  std::string data_directory_;
  std::istream& input_;
  std::ostream& output_;
  std::set<std::string> notices_;
  std::set<std::string> window_request_ids_;
  IValidationInterface* interface_{};
  bool review_devil(int day, int simulation_cycle);
};
}

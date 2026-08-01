#pragma once

#include "types.hpp"
#include <chrono>
#include <iosfwd>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace apo {
enum class ValidationStage { Empty, Choose, Confirm, Complete };
enum class ValidationPromptKind { Feature, Devil };
enum class ValidationWindowState { Pending, Resolved, StopRequested };

struct ValidationPrompt {
  ValidationStage stage{ValidationStage::Empty};
  int day{};
  int simulation_cycle{};
  std::vector<json> requests;
  std::size_t selected_index{};
  ValidationPromptKind kind{ValidationPromptKind::Feature};
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
  virtual std::optional<std::string> poll_command(const ValidationPrompt&) {
    return std::nullopt;
  }
  virtual bool present_evolution_progress(const EvolutionProgress&) { return true; }
  virtual std::string request_evolution_completion(const EvolutionProgress&) { return "o"; }
};

class HumanValidation {
 public:
  HumanValidation(std::string data_directory, std::istream& input, std::ostream& output,
                  IValidationInterface* interface = nullptr);
  ValidationWindowState advance_window(int day, int simulation_cycle, bool open_window);
  // Samples filesystem artefacts once and returns immediately. The simulation keeps ticking
  // while an approved evolution is queued, compiled, verified, or rejected.
  ValidationWindowState poll_evolution(const std::string& request_id);

 private:
  std::string data_directory_;
  std::istream& input_;
  std::ostream& output_;
  std::set<std::string> notices_;
  std::set<std::string> window_request_ids_;
  IValidationInterface* interface_{};
  struct ActiveWindow {
    int day{};
    int simulation_cycle{};
    std::vector<json> requests;
    std::optional<json> devil_request;
    std::optional<std::string> evolution_request_id;
    std::size_t selected_index{};
    bool prompt_dirty{true};
  };
  std::optional<ActiveWindow> active_window_;
  struct EvolutionMonitor {
    std::chrono::steady_clock::time_point queued_at;
    std::optional<std::chrono::steady_clock::time_point> started_at;
    std::string last_phase;
  };
  std::map<std::string,EvolutionMonitor> evolution_monitors_;
  std::optional<std::string> poll_command(const ValidationPrompt& prompt);
  ValidationWindowState advance_devil(ActiveWindow& window);
  ValidationWindowState advance_feature(ActiveWindow& window);
};
}

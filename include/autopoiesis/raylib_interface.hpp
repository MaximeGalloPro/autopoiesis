#pragma once

#include "ui_model.hpp"
#include "validation.hpp"
#include <memory>

namespace apo {
class RaylibInterface final : public IUserInterface, public IValidationInterface {
 public:
  explicit RaylibInterface(int initial_delay_ms = 500);
  ~RaylibInterface() override;
  RaylibInterface(const RaylibInterface&) = delete;
  RaylibInterface& operator=(const RaylibInterface&) = delete;
  bool present(const UiSnapshot& snapshot) override;
  bool idle_for(int milliseconds) override;
  bool present_activity(const UiActivity& activity) override;
  int simulation_delay_ms(int fallback) const override;
  bool restart_requested() const override;
  bool present_recompilation(const RecompileProgress& progress) override;
  void prepare_restart_environment() const;
  std::string request_command(const ValidationPrompt& prompt) override;
  bool present_evolution_progress(const EvolutionProgress& progress) override;
  std::string request_evolution_completion(const EvolutionProgress& progress) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}

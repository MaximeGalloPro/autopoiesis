#pragma once

#include "ui_model.hpp"
#include "validation.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace apo {
inline constexpr int web_protocol_version=1;
inline constexpr std::string_view web_event_prefix="AUTOPOIESIS_EVENT ";

enum class WebCommandKind {
  Pause, Resume, TogglePause, SetSpeed, SetDelayMs, SetApiEnabled, Validation, Stop
};

struct WebCommand {
  WebCommand()=default;
  explicit WebCommand(WebCommandKind command_kind):kind(command_kind) {}
  WebCommandKind kind{WebCommandKind::Stop};
  float speed{1.0F};
  int delay_ms{};
  bool enabled{};
  std::string validation_text;
};

std::optional<WebCommand> parse_web_command(std::string_view line,std::string& error);
bool web_validation_command_allowed(const ValidationPrompt& prompt,std::string_view command);
json web_snapshot_json(const UiSnapshot& snapshot);

class WebInterface final : public IUserInterface, public IValidationInterface {
 public:
  explicit WebInterface(std::istream& input,std::ostream& output,
                        int initial_delay_ms=500,int frame_interval_ms=16);
  bool present(const UiSnapshot& snapshot) override;
  bool idle_for(int milliseconds) override;
  bool present_activity(const UiActivity& activity) override;
  int simulation_delay_ms(int fallback) const override;
  bool restart_requested() const override;
  bool present_recompilation(const RecompileProgress& progress) override;
  std::string request_command(const ValidationPrompt& prompt) override;
  bool present_evolution_progress(const EvolutionProgress& progress) override;
  std::string request_evolution_completion(const EvolutionProgress& progress) override;

  bool paused() const { return paused_; }
  float speed_multiplier() const { return speed_; }
  bool stop_requested() const { return stop_requested_; }
  bool api_enabled() const { return api_available_&&api_enabled_; }
  void configure_api(bool available,bool enabled);
  void complete();
  void fail(const std::string& message);

 private:
  enum class ReadResult { None, Line, Closed };
  std::istream& input_;
  std::ostream& output_;
  int delay_ms_{};
  int frame_interval_ms_{};
  float speed_{1.0F};
  bool paused_{};
  bool stop_requested_{};
  bool restart_requested_{};
  bool completed_{};
  bool api_available_{};
  bool api_enabled_{};
  std::size_t snapshot_count_{};

  void emit(std::string_view type,json payload);
  void emit_status(std::string message,std::optional<bool> accepted=std::nullopt,
                   std::string command={});
  ReadResult read_line(bool blocking,std::string& line);
  bool drain_runtime_commands();
  bool wait_while_paused();
  bool apply_runtime_command(const WebCommand& command);
  std::string wait_for_validation(const ValidationPrompt& prompt,std::string_view event_type,
                                  json payload);
};
}

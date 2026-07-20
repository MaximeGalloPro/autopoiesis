#pragma once
#include "types.hpp"
#include "world.hpp"
#include <set>

namespace apo {
std::vector<std::string> available_actions(const Agent&, const World&, const std::vector<Agent>&,
                                           int current_day = 0,
                                           DayPhase phase = DayPhase::Day);
bool validate_decision(const Decision&, const Agent&, const World&, const std::vector<Agent>&,
                       std::string& error, int current_day = 0,
                       DayPhase phase = DayPhase::Day);
bool parse_decision(const json& value, Decision& out, std::string& error);
json decision_json(const Decision& d);
}

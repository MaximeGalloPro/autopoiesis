#pragma once

#include "types.hpp"

namespace apo {
enum class Season { Spring, Summer, Autumn, Winter };

struct CalendarDate {
  int absolute_day{};
  int day_of_month{};
  int month{};
  int year{};
  Season season{Season::Spring};
  friend bool operator==(const CalendarDate&, const CalendarDate&) = default;
};

struct ClimateState {
  int temperature_c{};
  int rainfall_mm{};
  std::string condition;
  friend bool operator==(const ClimateState&, const ClimateState&) = default;
};

CalendarDate date_from_absolute_day(int absolute_day);
ClimateState climate_for(const CalendarDate& date);
std::string season_name(Season season);
std::string calendar_label(const CalendarDate& date);
json calendar_json(const CalendarDate& date);
json climate_json(const ClimateState& climate);
}

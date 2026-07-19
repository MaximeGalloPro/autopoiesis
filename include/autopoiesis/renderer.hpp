#pragma once
#include "world.hpp"
#include "logger.hpp"
#include <iostream>
namespace apo { void render(const CalendarDate&, int simulation_cycle, const ClimateState&, const World&, const std::vector<Agent>&, const Logger&); }

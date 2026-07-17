#pragma once
#include "world.hpp"
#include "logger.hpp"
#include <iostream>
namespace apo { void render(int day, int simulation_cycle, const World&, const std::vector<Agent>&, const Logger&); }

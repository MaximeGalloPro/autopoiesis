#pragma once
#include "world.hpp"
#include "logger.hpp"
#include <iostream>
namespace apo { void render(int cycle, const World&, const std::vector<Agent>&, const Logger&); }

#pragma once

#include "types.hpp"

namespace apo {
bool validate_feature_request(const json& request, std::string& error);
json active_world_mechanisms();
}

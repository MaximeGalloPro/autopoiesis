#pragma once

#include "ui_model.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace apo {
std::vector<std::string> restart_arguments(const std::vector<std::string>& original,
                                           int remaining_days, int delay_ms);
int recompile_backend(const std::filesystem::path& project_root,
                      const std::filesystem::path& data_directory,
                      IUserInterface& interface);
[[noreturn]] void replace_process(const std::filesystem::path& executable,
                                  const std::vector<std::string>& arguments);
}

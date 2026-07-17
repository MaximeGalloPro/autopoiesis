#pragma once

#include "types.hpp"
#include <iosfwd>
#include <set>
#include <string>
#include <vector>

namespace apo {
class HumanValidation {
 public:
  HumanValidation(std::string data_directory, std::istream& input, std::ostream& output);
  bool review_window(int day, int simulation_cycle);
  bool wait_for_evolution(const std::string& request_id);

 private:
  std::string data_directory_;
  std::istream& input_;
  std::ostream& output_;
  std::set<std::string> notices_;
  std::set<std::string> window_request_ids_;
};
}

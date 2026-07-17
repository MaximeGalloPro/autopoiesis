#pragma once

#include "types.hpp"
#include <iosfwd>
#include <string>

namespace apo {
class HumanValidation {
 public:
  HumanValidation(std::string data_directory, std::istream& input, std::ostream& output);
  bool review_window(int day, int simulation_cycle);

 private:
  std::string data_directory_;
  std::istream& input_;
  std::ostream& output_;
};
}

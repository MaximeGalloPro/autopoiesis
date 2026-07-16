#pragma once
#include <cstddef>
#include <string>

namespace apo {
class ApiCallBudget {
 public:
  ApiCallBudget(std::string state_path, std::string run_id, std::size_t limit);
  bool try_acquire();
  std::size_t limit() const { return limit_; }
  const std::string& run_id() const { return run_id_; }
 private:
  std::string state_path_, lock_path_, run_id_; std::size_t limit_;
};
}

#pragma once

#include "ui_model.hpp"
#include <memory>

namespace apo {
class RaylibInterface final : public IUserInterface {
 public:
  RaylibInterface();
  ~RaylibInterface() override;
  RaylibInterface(const RaylibInterface&) = delete;
  RaylibInterface& operator=(const RaylibInterface&) = delete;
  bool present(const UiSnapshot& snapshot) override;
  bool idle_for(int milliseconds) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}

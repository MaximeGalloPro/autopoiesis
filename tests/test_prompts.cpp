#include "autopoiesis/openai_client.hpp"

#include <cassert>
#include <string>

int main() {
  const auto report = apo::period_report_instructions();
  const auto request = apo::evolution_request_instructions();

  assert(report.find("exclusivement en francais") != std::string::npos);
  assert(request.find("exclusivement en francais") != std::string::npos);
  assert(request.find("title") != std::string::npos);
  assert(request.find("acceptance_tests") != std::string::npos);
}

#include "autopoiesis/openai_client.hpp"

#include <cassert>
#include <string>

int main() {
  const auto report = apo::period_report_instructions();
  const auto request = apo::evolution_request_instructions();

  assert(report.find("exclusivement en francais") != std::string::npos);
  assert(report.find("memory_summary") != std::string::npos);
  assert(report.find("memory_feeling") != std::string::npos);
  assert(report.find("periodes precedentes") != std::string::npos);
  assert(request.find("exclusivement en francais") != std::string::npos);
  assert(request.find("title") != std::string::npos);
  assert(request.find("acceptance_tests") != std::string::npos);
  assert(request.find("projet") != std::string::npos);
  assert(request.find("aspiration") != std::string::npos);
  assert(request.find("evolution_key") != std::string::npos);
  assert(request.find("navigation") != std::string::npos);
}

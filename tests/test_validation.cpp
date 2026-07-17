#include "autopoiesis/validation.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace apo;

int main() {
  const std::filesystem::path directory = "/tmp/autopoiesis-validation-tests";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);

  std::ofstream requests(directory / "feature_requests.jsonl");
  requests << "{ this is not json }\n";
  requests << R"({"id":"request-1","status":"pending","day":3,"simulation_cycle":720,"agent_id":"a1","agent_name":"Ada","title":"Test request","need":"Food","obstacle":"None","proposed_change":"Test","mechanism":{"name":"test","summary":"Test","resources":["food"],"actions":["eat"],"preconditions":["hungry"],"deterministic_effects":["fed"]},"acceptance_tests":["It works"]})" << '\n';
  requests.close();

  std::istringstream input("exit\n");
  std::ostringstream output;
  HumanValidation validation(directory.string(), input, output);
  assert(!validation.review_window(3, 720));
  assert(output.str().find("request-1") != std::string::npos);
  assert(output.str().find("invalide") != std::string::npos);

  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  std::ofstream valid_requests(directory / "feature_requests.jsonl");
  valid_requests << R"({"id":"request-2","status":"pending","day":3,"simulation_cycle":720,"agent_id":"a1","agent_name":"Ada","title":"Approve me","need":"Food","obstacle":"None","proposed_change":"Test","mechanism":{"name":"test","summary":"Test","resources":["food"],"actions":["eat"],"preconditions":["hungry"],"deterministic_effects":["fed"]},"acceptance_tests":["It works"]})" << '\n';
  valid_requests << R"({"id":"request-3","status":"pending","day":3,"simulation_cycle":720,"agent_id":"a2","agent_name":"Borin","title":"Leave me pending","need":"Rest","obstacle":"None","proposed_change":"Test","mechanism":{"name":"test","summary":"Test","resources":["rest"],"actions":["sleep"],"preconditions":["tired"],"deterministic_effects":["rested"]},"acceptance_tests":["It works"]})" << '\n';
  valid_requests << R"({"id":"request-3","status":"pending","day":3,"simulation_cycle":720,"agent_id":"a2","agent_name":"Borin","title":"Leave me pending","need":"Rest","obstacle":"None","proposed_change":"Test","mechanism":{"name":"test","summary":"Test","resources":["rest"],"actions":["sleep"],"preconditions":["tired"],"deterministic_effects":["rested"]},"acceptance_tests":["It works"]})" << '\n';
  valid_requests.close();
  std::istringstream approval_input("1\na\no\n");
  std::ostringstream approval_output;
  HumanValidation approval(directory.string(), approval_input, approval_output);
  assert(approval.review_window(3, 720));
  assert(approval_output.str().find("doublon") != std::string::npos);
  std::ifstream approved(directory / "approved_feature_requests.jsonl");
  const std::string approved_content(std::istreambuf_iterator<char>(approved), {});
  assert(approved_content.find("request-2") != std::string::npos);
  assert(approved_content.find("request-3") == std::string::npos);
  std::ifstream remaining(directory / "feature_requests.jsonl");
  assert(std::string(std::istreambuf_iterator<char>(remaining), {}).find("request-3") != std::string::npos);
  assert(std::filesystem::exists(directory / "evolution_runs/request-2/validation-record.json"));
  std::filesystem::remove_all(directory);
}

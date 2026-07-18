#include "autopoiesis/validation.hpp"
#include <cassert>
#include <cstdlib>
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
  setenv("GOD_QUEUE_TIMEOUT_SECONDS", "1", 1);
  setenv("GOD_WAIT_TIMEOUT_SECONDS", "1", 1);
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

  std::filesystem::create_directories(directory / "evolution_runs/request-2");
  std::ofstream god_started(directory / "evolution_runs/request-2/god-started");
  god_started << "started\n";
  god_started.close();
  std::ofstream god_result(directory / "evolution_runs/request-2/god-result.txt");
  god_result << "TDD termine: le test et l'implementation sont prets.\n";
  god_result.close();
  std::ofstream verification(directory / "evolution_runs/request-2/verification.json");
  verification << R"({"status":"verified","cmake":"passed","tests":"passed","docker":"passed"})" << '\n';
  verification.close();
  std::istringstream progress_input("");
  std::ostringstream progress_output;
  HumanValidation progress(directory.string(), progress_input, progress_output);
  assert(progress.wait_for_evolution("request-2"));
  assert(progress_output.str().find("Dieu") != std::string::npos);
  assert(progress_output.str().find("verified") != std::string::npos);
  unsetenv("GOD_QUEUE_TIMEOUT_SECONDS");
  unsetenv("GOD_WAIT_TIMEOUT_SECONDS");

  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory / "evolution_runs/queued-request");
  setenv("GOD_QUEUE_TIMEOUT_SECONDS", "1", 1);
  std::istringstream queue_input("");
  std::ostringstream queue_output;
  HumanValidation queued(directory.string(), queue_input, queue_output);
  assert(queued.wait_for_evolution("queued-request"));
  assert(queue_output.str().find("file d'attente") != std::string::npos);
  assert(queue_output.str().find("GOD_QUEUE_TIMEOUT_SECONDS") != std::string::npos);
  unsetenv("GOD_QUEUE_TIMEOUT_SECONDS");

  std::filesystem::remove_all(directory);
  const auto failed_run = directory / "evolution_runs/failed-request";
  std::filesystem::create_directories(failed_run);
  std::ofstream(failed_run / "god-started") << "started\n";
  std::ofstream(failed_run / "god-failed") << "exit=1\n";
  std::ofstream(failed_run / "god.stderr.log")
      << "detail ancien\nraison precise de l'echec\n";
  std::istringstream failed_input("");
  std::ostringstream failed_output;
  HumanValidation failed(directory.string(), failed_input, failed_output);
  assert(!failed.wait_for_evolution("failed-request"));
  assert(failed_output.str().find("raison precise de l'echec") != std::string::npos);
  assert(failed_output.str().find("evolution_runs/failed-request") != std::string::npos);

  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  std::ofstream recent_requests(directory / "feature_requests.jsonl");
  recent_requests << R"({"id":"historical","status":"pending","day":3,"simulation_cycle":720,"agent_id":"a1","agent_name":"Ada","title":"Old","need":"Food","obstacle":"None","proposed_change":"Test","mechanism":{"name":"test","summary":"Test","resources":["food"],"actions":["eat"],"preconditions":["hungry"],"deterministic_effects":["fed"]},"acceptance_tests":["It works"]})" << '\n';
  recent_requests.close();
  std::ofstream(directory / "evolution-session-request-offset") << "1\n";
  recent_requests.open(directory / "feature_requests.jsonl", std::ios::app);
  for (int index = 2; index <= 4; ++index) {
    recent_requests << R"({"id":"recent-)" << index
                    << R"(","status":"pending","day":3,"simulation_cycle":720,"agent_id":"a1","agent_name":"Ada","title":"Recent )"
                    << index << R"(","need":"Food","obstacle":"None","proposed_change":"Test","mechanism":{"name":"test","summary":"Test","resources":["food"],"actions":["eat"],"preconditions":["hungry"],"deterministic_effects":["fed"]},"acceptance_tests":["It works"]})"
                    << '\n';
  }
  recent_requests.close();
  std::istringstream recent_input("exit\n");
  std::ostringstream recent_output;
  HumanValidation recent(directory.string(), recent_input, recent_output);
  assert(!recent.review_window(3, 720));
  assert(recent_output.str().find("historical") == std::string::npos);
  assert(recent_output.str().find("recent-2") != std::string::npos);
  assert(recent_output.str().find("recent-3") != std::string::npos);
  assert(recent_output.str().find("recent-4") != std::string::npos);
  assert(recent_output.str().find("ancienne(s)") == std::string::npos);

  const auto devil_request = R"({"id":"devil-1","status":"pending","source":"devil","day":3,"simulation_cycle":720,"agent_id":"devil","agent_name":"Le Diable","title":"Le froid nocturne","need":"Rendre les abris utiles","obstacle":"Aucune température","proposed_change":"Ajouter le froid","mechanism":{"name":"froid","summary":"Froid déterministe","resources":["température"],"actions":["calculer"],"preconditions":["nuit"],"deterministic_effects":["fatigue"]},"acceptance_tests":["La nuit fatigue"]})";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  std::ofstream(directory / "feature_requests.jsonl") << devil_request << '\n';
  std::istringstream devil_input("r\no\n");
  std::ostringstream devil_output;
  HumanValidation devil_validation(directory.string(), devil_input, devil_output);
  assert(devil_validation.review_window(3, 720));
  assert(devil_output.str().find("APPARITION DU DIABLE") != std::string::npos);
  assert(devil_output.str().find("Le froid nocturne") != std::string::npos);
  std::ifstream devil_rejected(directory / "rejected_feature_requests.jsonl");
  const std::string devil_rejected_content(std::istreambuf_iterator<char>(devil_rejected), {});
  assert(devil_rejected_content.find("devil-1") != std::string::npos);

  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  std::ofstream(directory / "feature_requests.jsonl") << devil_request << '\n';
  setenv("DEVIL_AUTO_APPROVE", "1", 1);
  setenv("GOD_QUEUE_TIMEOUT_SECONDS", "1", 1);
  std::istringstream automatic_input("o\n");
  std::ostringstream automatic_output;
  HumanValidation automatic(directory.string(), automatic_input, automatic_output);
  assert(automatic.review_window(3, 720));
  assert(automatic_output.str().find("Approbation automatique") != std::string::npos);
  std::ifstream devil_approved(directory / "approved_feature_requests.jsonl");
  const std::string devil_approved_content(std::istreambuf_iterator<char>(devil_approved), {});
  assert(devil_approved_content.find("devil-1") != std::string::npos);
  assert(devil_approved_content.find("devil_automatic") != std::string::npos);
  unsetenv("DEVIL_AUTO_APPROVE");
  unsetenv("GOD_QUEUE_TIMEOUT_SECONDS");
  std::filesystem::remove_all(directory);
}

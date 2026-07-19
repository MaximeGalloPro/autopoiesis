#include "autopoiesis/logger.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using namespace apo;

int main() {
  const std::filesystem::path directory = "/tmp/autopoiesis-period-memory-tests";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  std::ofstream reports(directory / "ai_reports.jsonl");
  reports << (json{{"day",3},{"agent_id","a1"},{"calendar",{{"label","jour 3, mois 1, année 1 (printemps)"}}},{"report",{{"memory_summary","Ada a trouvé de l'eau."},{"memory_feeling","Ada se sent rassurée."}}}}).dump() << '\n';
  reports << (json{{"day",6},{"agent_id","a2"},{"report",{{"memory_summary","Borin chasse."},{"memory_feeling","Borin est fier."}}}}).dump() << '\n';
  reports << (json{{"day",6},{"agent_id","a1"},{"report",{{"day_summary","Ada poursuit son abri."},{"state_assessment","Ada reste déterminée."}}}}).dump() << '\n';
  reports << (json{{"day",9},{"agent_id","a1"},{"report",{{"memory_summary",std::string(260,'x')},{"memory_feeling",std::string(260,'y')}}}}).dump() << '\n';
  reports.close();

  Logger logger(directory.string());
  const auto memories = logger.period_memories("a1", 2);
  assert(memories.is_array());
  assert(memories.size() == 2);
  assert(memories[0]["bilan"] == "Ada poursuit son abri.");
  assert(memories[0]["ressenti"] == "Ada reste déterminée.");
  assert(memories[1]["bilan"].get<std::string>().size() <= 180);
  assert(memories[1]["ressenti"].get<std::string>().size() <= 180);
  assert(!memories[1].contains("report"));
}

#include "autopoiesis/validation.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

namespace apo {
namespace {
using path = std::filesystem::path;

std::vector<json> read_jsonl(const path& file, std::ostream& output) {
  std::vector<json> items;
  std::ifstream input(file);
  if (!input) return items;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    if (line.empty()) continue;
    try {
      items.push_back(json::parse(line));
    } catch (const json::parse_error&) {
      output << "Ligne JSON invalide ignoree dans " << file.filename().string()
             << " (ligne " << line_number << ").\n";
    }
  }
  return items;
}

std::set<std::string> ids_from(const std::vector<json>& items) {
  std::set<std::string> ids;
  for (const auto& item : items) {
    if (item.is_object() && item.value("id", "") != "") ids.insert(item.value("id", ""));
  }
  return ids;
}

std::string timestamp() {
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  return std::to_string(now);
}

void append_jsonl(const path& file, const json& item) {
  std::ofstream output(file, std::ios::app);
  if (output) output << item.dump() << '\n';
}

void write_validation_record(const path& data_directory, const json& request,
                             const std::string& decision, const std::string& reason) {
  const auto request_id = request.value("id", "unknown");
  const auto run_directory = data_directory / "evolution_runs" / request_id;
  std::error_code error;
  std::filesystem::create_directories(run_directory, error);
  json record={{"request_id",request_id},{"status",decision=="approve"?"validated":"rejected"},
               {"recommendation",{{"decision",decision},{"reviewer","human"},{"reason",reason}}}};
  std::ofstream output(run_directory / "validation-record.json");
  if (output) output << record.dump(2) << '\n';
}

std::vector<json> current_requests(const path& data_directory, int day, int simulation_cycle,
                                    std::ostream& output) {
  const auto requests = read_jsonl(data_directory / "feature_requests.jsonl", output);
  const auto approved = ids_from(read_jsonl(data_directory / "approved_feature_requests.jsonl", output));
  const auto rejected = ids_from(read_jsonl(data_directory / "rejected_feature_requests.jsonl", output));
  std::vector<json> current;
  for (const auto& request : requests) {
    const auto id = request.value("id", "");
    if (id.empty() || request.value("status", "") != "pending" || approved.contains(id) || rejected.contains(id)) continue;
    if (request.value("day", -1) != day || request.value("simulation_cycle", -1) != simulation_cycle) continue;
    current.push_back(request);
  }
  return current;
}
}

HumanValidation::HumanValidation(std::string data_directory, std::istream& input,
                                 std::ostream& output)
    : data_directory_(std::move(data_directory)), input_(input), output_(output) {}

bool HumanValidation::review_window(int day, int simulation_cycle) {
  const path data_directory(data_directory_);
  while (true) {
    auto requests = current_requests(data_directory, day, simulation_cycle, output_);
    output_ << "\n=== VALIDATION HUMAINE ===\n"
            << "Jour " << day << " | Cycle elementaire " << simulation_cycle << "\n";
    if (requests.empty()) {
      output_ << "Aucune demande en attente pour cette fenêtre.\n"
               << "Tapez o pour reprendre, q ou exit pour arrêter.\n> " << std::flush;
    } else {
      output_ << requests.size() << " demande(s) à valider :\n";
      for (std::size_t index=0; index<requests.size(); ++index) {
        const auto& request=requests[index];
        output_ << "[" << index+1 << "] " << request.value("id", "?") << " — "
                 << request.value("agent_name", "?") << " — "
                 << request.value("title", "(sans titre)") << '\n';
      }
      output_ << "Commandes : a N approuver, r N refuser, d N détail, o reprendre, q/exit arrêter.\n> " << std::flush;
    }

    std::string line;
    if (!std::getline(input_, line)) return false;
    std::istringstream command(line);
    std::string action;
    command >> action;
    if (action=="o" || action=="O") {
      if (requests.empty()) return true;
      output_ << "Chaque demande doit être approuvée ou refusée avant la reprise.\n";
      continue;
    }
    if (action=="q" || action=="Q" || action=="exit" || action=="quit") return false;
    std::size_t index=0;
    command >> index;
    if (index==0 || index>requests.size()) {
      output_ << "Commande inconnue.\n";
      continue;
    }
    auto& request=requests[index-1];
    if (action=="d" || action=="D") {
      output_ << request.dump(2) << '\n';
      continue;
    }
    if (action!="a" && action!="A" && action!="r" && action!="R") {
      output_ << "Commande inconnue.\n";
      continue;
    }
    const bool approve=action=="a" || action=="A";
    const auto request_id=request.value("id", "unknown");
    request["status"] = approve ? "approved" : "rejected";
    request[approve?"approved_at":"rejected_at"] = timestamp();
    request[approve?"approval_mode":"rejection_mode"] = "human";
    if (!approve) request["rejection_reason"] = "Refus explicite dans l'interface intégrée";
    append_jsonl(data_directory / (approve ? "approved_feature_requests.jsonl" : "rejected_feature_requests.jsonl"), request);
    write_validation_record(data_directory, request, approve?"approve":"reject",
                            approve?"Approbation explicite dans l'interface intégrée":"Refus explicite dans l'interface intégrée");
    output_ << (approve ? "Demande approuvée : " : "Demande refusée : ") << request_id << '\n';
  }
}
}

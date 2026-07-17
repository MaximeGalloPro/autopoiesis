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
  std::size_t invalid_lines = 0;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    try {
      items.push_back(json::parse(line));
    } catch (const json::parse_error&) {
      ++invalid_lines;
    }
  }
  if (invalid_lines>0) output << invalid_lines << " ligne(s) JSON invalide(s) ignoree(s) dans "
                              << file.filename().string() << ".\n";
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
  std::size_t selected_index=0;
  bool decision_made = false;
  while (true) {
    auto requests = current_requests(data_directory, day, simulation_cycle, output_);
    output_ << "\n=== VALIDATION HUMAINE ===\n"
            << "Jour " << day << " | Cycle elementaire " << simulation_cycle << "\n";
    if (requests.empty()) {
      output_ << "Aucune demande en attente pour cette fenêtre.\n"
               << "Tapez o pour reprendre, q ou exit pour arrêter.\n> " << std::flush;
    } else if(selected_index==0 && !decision_made) {
      output_ << requests.size() << " proposition(s) disponibles :\n";
      for (std::size_t index=0; index<requests.size(); ++index) {
        const auto& request=requests[index];
        output_ << "[" << index+1 << "] " << request.value("id", "?") << " — "
                 << request.value("agent_name", "?") << " — "
                 << request.value("title", "(sans titre)") << '\n';
      }
      output_ << "Choisissez une proposition (1-" << requests.size()
              << ") ou n pour aucune, d N détail, q/exit arrêter.\n> " << std::flush;
    } else if(selected_index>0 && !decision_made) {
      const auto& request=requests[selected_index-1];
      output_ << "Proposition sélectionnée : " << request.value("title", "(sans titre)") << '\n'
              << "[a] approuver  [r] refuser  [b] revenir au choix  [d] détail  [q/exit] arrêter.\n> " << std::flush;
    } else {
      output_ << requests.size() << " proposition(s) restantes, conservées pending.\n"
               << "Tapez o pour reprendre, d N détail, q ou exit pour arrêter.\n> " << std::flush;
    }

    std::string line;
    if (!std::getline(input_, line)) return false;
    std::istringstream command(line);
    std::string action;
    command >> action;
    if (action=="o" || action=="O") {
      if (requests.empty() || decision_made) return true;
      output_ << "Choisissez d'abord une proposition, ou n pour n'en valider aucune.\n";
      continue;
    }
    if (action=="n" || action=="N") {
      if(selected_index==0 && !decision_made) {
        output_ << "Aucune proposition sélectionnée ; les demandes restent pending.\n";
        return true;
      }
      output_ << "Commande indisponible à cette étape.\n";
      continue;
    }
    if (action=="q" || action=="Q" || action=="exit" || action=="quit") return false;

    if (selected_index==0 && !decision_made) {
      if (action=="d" || action=="D") {
        std::size_t index=0;
        command >> index;
        if(index>0 && index<=requests.size()) output_ << requests[index-1].dump(2) << '\n';
        else output_ << "Numéro de proposition invalide.\n";
        continue;
      }
      try { selected_index=std::stoul(action); }
      catch (...) { selected_index=0; }
      if(selected_index==0 || selected_index>requests.size()) {
        selected_index=0;
        output_ << "Choisissez une proposition valide ou n.\n";
      }
      continue;
    }

    if (selected_index>0 && !decision_made) {
      const auto& request=requests[selected_index-1];
      if (action=="b" || action=="B") {
        selected_index=0;
        continue;
      }
      if (action=="d" || action=="D") {
        output_ << request.dump(2) << '\n';
        continue;
      }
      if (action!="a" && action!="A" && action!="r" && action!="R") {
        output_ << "Choisissez a, r, b ou d.\n";
        continue;
      }
      const bool approve=action=="a" || action=="A";
      const auto request_id=request.value("id", "unknown");
      auto decided=request;
      decided["status"] = approve ? "approved" : "rejected";
      decided[approve?"approved_at":"rejected_at"] = timestamp();
      decided[approve?"approval_mode":"rejection_mode"] = "human";
      if (!approve) decided["rejection_reason"] = "Refus explicite dans l'interface intégrée";
      append_jsonl(data_directory / (approve ? "approved_feature_requests.jsonl" : "rejected_feature_requests.jsonl"), decided);
      write_validation_record(data_directory, decided, approve?"approve":"reject",
                              approve?"Approbation explicite dans l'interface intégrée":"Refus explicite dans l'interface intégrée");
      output_ << (approve ? "Demande approuvée : " : "Demande refusée : ") << request_id << '\n';
      selected_index=0;
      decision_made=true;
      continue;
    }

    if (action=="d" || action=="D") {
      std::size_t index=0;
      command >> index;
      if(index>0 && index<=requests.size()) output_ << requests[index-1].dump(2) << '\n';
      else output_ << "Numéro de proposition invalide.\n";
      continue;
    }
    output_ << "Une seule proposition peut être traitée par fenêtre. Tapez o pour reprendre.\n";
  }
}
}

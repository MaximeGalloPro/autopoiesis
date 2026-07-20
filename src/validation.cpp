#include "autopoiesis/validation.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace apo {
namespace {
using path = std::filesystem::path;

std::vector<json> read_jsonl(const path& file, std::ostream& output,
                             std::set<std::string>& notices,
                             std::size_t first_line = 0) {
  std::vector<json> items;
  std::ifstream input(file);
  if (!input) return items;
  std::string line;
  std::size_t invalid_lines = 0;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    if (line_number++ < first_line) continue;
    if (line.empty()) continue;
    try {
      items.push_back(json::parse(line));
    } catch (const json::parse_error&) {
      ++invalid_lines;
    }
  }
  const auto notice_key="invalid:"+file.string();
  if (invalid_lines>0 && notices.insert(notice_key).second)
    output << invalid_lines << " ligne(s) JSON invalide(s) ignoree(s) dans "
           << file.filename().string() << ".\n";
  return items;
}

std::size_t request_offset(const path& data_directory) {
  std::ifstream input(data_directory / "evolution-session-request-offset");
  std::size_t offset = 0;
  if (input) input >> offset;
  return offset;
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

std::string read_text(const path& file) {
  std::ifstream input(file);
  if (!input) return {};
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string last_non_empty_line(const path& file) {
  std::istringstream input(read_text(file));
  std::string line;
  std::string last;
  while (std::getline(input, line)) {
    if (!line.empty()) last = line;
  }
  return last;
}

int timeout_seconds(const char* name, int fallback) {
  const char* value = std::getenv(name);
  if (!value || !*value) return fallback;
  try {
    return std::max(1, std::stoi(value));
  } catch (...) {
    return fallback;
  }
}

std::vector<std::string> tail_non_empty_lines(const path& file, std::size_t maximum) {
  std::istringstream input(read_text(file));
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    if (line.size() > 600) line = line.substr(0, 600) + "...";
    lines.push_back(line);
    if (lines.size() > maximum) lines.erase(lines.begin());
  }
  return lines;
}

void print_evolution_diagnostics(std::ostream& output, const path& data_directory,
                                 const path& run_directory) {
  output << "[Diagnostic] Artefacts complets : " << run_directory.string() << '\n';
  const std::vector<std::pair<std::string, path>> logs = {
      {"Dieu stderr", run_directory / "god.stderr.log"},
      {"Dieu stdout", run_directory / "god.stdout.log"},
      {"Tests", run_directory / "verify-tests.log"},
      {"Build", run_directory / "verify-build.log"},
      {"Docker", run_directory / "verify-docker.log"},
      {"Orchestrateur", data_directory / "evolution-daemon.log"},
  };
  for (const auto& [label, file] : logs) {
    const auto lines = tail_non_empty_lines(file, 10);
    if (lines.empty()) continue;
    output << "[Diagnostic] " << label << " (dernieres lignes) :\n";
    for (const auto& line : lines) output << "  " << line << '\n';
  }
}

int god_max_corrections() {
  const char* value = std::getenv("GOD_MAX_CORRECTIONS");
  if (!value || !*value) return 2;
  try {
    return std::max(0, std::stoi(value));
  } catch (...) {
    return 2;
  }
}

void append_jsonl(const path& file, const json& item) {
  std::ofstream output(file, std::ios::app);
  if (output) output << item.dump() << '\n';
}

void write_validation_record(const path& data_directory, const json& request,
                             const std::string& decision, const std::string& reason,
                             const std::string& reviewer = "human") {
  const auto request_id = request.value("id", "unknown");
  const auto run_directory = data_directory / "evolution_runs" / request_id;
  std::error_code error;
  std::filesystem::create_directories(run_directory, error);
  json record={{"request_id",request_id},{"status",decision=="approve"?"validated":"rejected"},
               {"recommendation",{{"decision",decision},{"reviewer",reviewer},{"reason",reason}}}};
  std::ofstream output(run_directory / "validation-record.json");
  if (output) output << record.dump(2) << '\n';
}

std::vector<json> current_requests(const path& data_directory, int day, int simulation_cycle,
                                    std::ostream& output, std::set<std::string>& notices,
                                    bool devil_only = false) {
  const auto requests = read_jsonl(data_directory / "feature_requests.jsonl", output, notices,
                                   request_offset(data_directory));
  const auto approved = ids_from(read_jsonl(data_directory / "approved_feature_requests.jsonl", output, notices));
  const auto rejected = ids_from(read_jsonl(data_directory / "rejected_feature_requests.jsonl", output, notices));
  std::vector<json> current;
  std::set<std::string> seen_ids;
  std::size_t duplicates=0;
  for (const auto& request : requests) {
    const auto id = request.value("id", "");
    if ((request.value("source", "") == "devil") != devil_only) continue;
    if (id.empty() || request.value("status", "") != "pending" || approved.contains(id) || rejected.contains(id)) continue;
    if (request.value("day", -1) != day || request.value("simulation_cycle", -1) != simulation_cycle) continue;
    if (!seen_ids.insert(id).second) { ++duplicates; continue; }
    current.push_back(request);
  }
  const auto notice_key="duplicates:"+std::to_string(day)+":"+std::to_string(simulation_cycle);
  if (duplicates>0 && notices.insert(notice_key).second)
    output << duplicates << " doublon(s) de demande ignore(s) pour cette fenêtre.\n";
  return current;
}

std::vector<json> select_window_requests(const std::vector<json>& available,
                                         std::set<std::string>& window_ids,
                                         std::ostream& output) {
  if (window_ids.empty() && !available.empty()) {
    const std::size_t first = available.size() > 3 ? available.size() - 3 : 0;
    for (std::size_t index = first; index < available.size(); ++index)
      window_ids.insert(available[index].value("id", ""));
    if (available.size() > 3)
      output << available.size() - 3 << " ancienne(s) proposition(s) masquee(s) : seules les 3 plus recentes sont proposees.\n";
  }
  std::vector<json> selected;
  for (const auto& request : available)
    if (window_ids.contains(request.value("id", ""))) selected.push_back(request);
  return selected;
}
}

HumanValidation::HumanValidation(std::string data_directory, std::istream& input,
                                 std::ostream& output, IValidationInterface* interface)
    : data_directory_(std::move(data_directory)), input_(input), output_(output),
      interface_(interface) {}

bool HumanValidation::wait_for_evolution(const std::string& request_id) {
  const path data_directory(data_directory_);
  const path run_directory = data_directory / "evolution_runs" / request_id;
  const auto queue_started_at = std::chrono::steady_clock::now();
  const auto queue_deadline = queue_started_at + std::chrono::seconds(
      timeout_seconds("GOD_QUEUE_TIMEOUT_SECONDS", 900));
  auto work_started_at = queue_started_at;
  auto work_deadline = queue_started_at;
  auto last_heartbeat = queue_started_at;
  bool work_started = false;
  std::string phase;
  std::string last_log;
  std::string last_daemon_log;
  std::string last_verification_status;
  bool reported_result = false;

  const auto progress_stage=[](const std::string& current_phase){
    if(current_phase=="preparation")return EvolutionProgressStage::Preparing;
    if(current_phase=="implementation")return EvolutionProgressStage::Implementing;
    if(current_phase=="compte-rendu")return EvolutionProgressStage::Reporting;
    if(current_phase=="verification")return EvolutionProgressStage::Verifying;
    if(current_phase=="correction")return EvolutionProgressStage::Correcting;
    if(current_phase=="activation")return EvolutionProgressStage::Activating;
    if(current_phase=="activation-terminee")return EvolutionProgressStage::Complete;
    if(current_phase=="echec"||current_phase=="activation-echec")return EvolutionProgressStage::Failed;
    return EvolutionProgressStage::Queued;
  };
  const auto progress_message=[](EvolutionProgressStage stage){
    switch(stage){
      case EvolutionProgressStage::Queued:return "En attente du daemon d'évolution";
      case EvolutionProgressStage::Preparing:return "Préparation de la demande pour Dieu";
      case EvolutionProgressStage::Implementing:return "Dieu écrit le test rouge puis l'implémentation minimale";
      case EvolutionProgressStage::Reporting:return "Compte rendu reçu, vérification en préparation";
      case EvolutionProgressStage::Verifying:return "Compilation, tests et Docker en cours";
      case EvolutionProgressStage::Correcting:return "Une correction est en cours";
      case EvolutionProgressStage::Activating:return "Commit, push et activation de la version vérifiée";
      case EvolutionProgressStage::Complete:return "La nouvelle évolution est active";
      case EvolutionProgressStage::Failed:return "L'évolution n'a pas pu être activée";
      case EvolutionProgressStage::TimedOut:return "Le délai de suivi est dépassé";
    }
    return "Traitement en cours";
  };
  const auto show_progress=[&](EvolutionProgressStage stage,const std::string& detail,bool successful){
    if(!interface_)return true;
    const auto now=std::chrono::steady_clock::now();
    const auto reference=work_started?work_started_at:queue_started_at;
    const auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-reference).count();
    return interface_->present_evolution_progress(
        {stage,request_id,progress_message(stage),detail,elapsed,successful});
  };
  const auto finish_progress=[&](EvolutionProgressStage stage,const std::string& detail,
                                 bool successful,bool can_continue){
    if(!interface_)return can_continue;
    const auto now=std::chrono::steady_clock::now();
    const auto reference=work_started?work_started_at:queue_started_at;
    const auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-reference).count();
    const EvolutionProgress progress{stage,request_id,progress_message(stage),detail,elapsed,successful};
    if(!interface_->present_evolution_progress(progress))return false;
    const auto command=interface_->request_evolution_completion(progress);
    return can_continue&&command!="q"&&command!="Q";
  };

  output_ << "\n=== SUIVI DE DIEU ===\n"
          << "Demande " << request_id << " approuvee."
          << " Le daemon lance Dieu automatiquement.\n"
          << "En attente des artefacts d'execution...\n" << std::flush;

  while (true) {
    const auto now = std::chrono::steady_clock::now();
    const bool prompt_ready = std::filesystem::exists(run_directory / "god-prompt.txt");
    const bool god_started = std::filesystem::exists(run_directory / "god-started");
    const bool god_result_ready = std::filesystem::exists(run_directory / "god-result.txt");
    const bool god_failed = std::filesystem::exists(run_directory / "god-failed");
    const bool correction_failed = std::filesystem::exists(run_directory / "god-correction-failed");
    const bool verification_started = std::filesystem::exists(run_directory / "verification-started");
    const bool verification_ready = std::filesystem::exists(run_directory / "verification.json");
    const bool activation_ready = std::filesystem::exists(run_directory / "activation.json");
    std::string verification_status;
    if (verification_ready) {
      try { verification_status = json::parse(read_text(run_directory / "verification.json")).value("status", ""); }
      catch (const json::parse_error&) { verification_status.clear(); }
    }
    const bool activation_failed = std::filesystem::exists(run_directory / "activation-failed");
    if (god_started && !work_started) {
      work_started = true;
      work_started_at = now;
      work_deadline = now + std::chrono::seconds(
          timeout_seconds("GOD_WAIT_TIMEOUT_SECONDS", 900));
      const auto queued_seconds = std::chrono::duration_cast<std::chrono::seconds>(
          now - queue_started_at).count();
      output_ << "[Orchestrateur] Dieu a demarre apres " << queued_seconds
              << " seconde(s) en file d'attente.\n";
    }
    const std::string next_phase = god_failed || correction_failed ? "echec" : activation_failed ? "activation-echec" : activation_ready ? "activation-terminee" :
                                   verification_status == "verified" ? "activation" :
                                   verification_ready ? "correction" : verification_started ? "verification" :
                                   god_result_ready ? "compte-rendu" :
                                   god_started ? "implementation" :
                                   prompt_ready ? "preparation" : "attente";
    if (next_phase != phase) {
      phase = next_phase;
      if (phase == "attente") output_ << "[Dieu] En attente du runner d'evolution.\n";
      if (phase == "preparation") output_ << "[Dieu] Prompt prepare, lancement de l'instance architecte.\n";
      if (phase == "implementation") output_ << "[Dieu] Instance active : lecture, test rouge, implementation minimale.\n";
      if (phase == "compte-rendu") output_ << "[Dieu] Compte rendu recu, passage a la verification.\n";
      if (phase == "verification") output_ << "[Verifier] Compilation, tests et Docker en cours.\n";
      if (phase == "correction") output_ << "[Verifier] Echec detecte : Dieu va recevoir le diagnostic pour correction.\n";
      if (phase == "activation") output_ << "[Activation] Verification reussie, commit, push et activation en cours.\n";
      if (phase == "activation-terminee") output_ << "[Activation] Version activee et poussee sur main.\n";
      if (phase == "activation-echec") output_ << "[Activation] Echec du commit ou du push ; la version reste inactive.\n";
      if (phase == "echec") output_ << "[Dieu] Le runner a echoue ; consultez les logs de cette execution.\n";
    }

    const auto stdout_line = last_non_empty_line(run_directory / "god.stdout.log");
    const auto stderr_line = last_non_empty_line(run_directory / "god.stderr.log");
    const auto current_log = stderr_line.empty() ? stdout_line : stderr_line;
    if (!current_log.empty() && current_log != last_log) {
      last_log = current_log;
      output_ << "[Dieu] " << current_log << '\n';
    }
    const auto daemon_log = last_non_empty_line(data_directory / "evolution-daemon.log");
    if (!daemon_log.empty() && daemon_log != last_daemon_log) {
      last_daemon_log = daemon_log;
      output_ << "[Orchestrateur] " << daemon_log << '\n';
    }

    if (god_result_ready && !reported_result) {
      const auto result = read_text(run_directory / "god-result.txt");
      if (!result.empty()) output_ << "[Dieu] Detail du compte rendu :\n" << result;
      reported_result = true;
    }

    if (verification_ready) {
      try {
        const auto verification = json::parse(read_text(run_directory / "verification.json"));
        const auto status = verification.value("status", "inconnu");
        if (status != last_verification_status) {
          last_verification_status = status;
          output_ << "[Verifier] statut=" << status
                  << " | cmake=" << verification.value("cmake", "inconnu")
                  << " | tests=" << verification.value("tests", "inconnu")
                  << " | docker=" << verification.value("docker", "inconnu") << '\n' << std::flush;
        }
        if (verification.value("status", "") == "verified" && activation_ready) {
          try {
            const auto activation = json::parse(read_text(run_directory / "activation.json"));
            const bool activated=activation.value("status", "")=="activated";
            output_ << "[Activation] statut=" << activation.value("status", "inconnu")
                    << " | commit=" << activation.value("commit", "inconnu") << '\n'
                    << "=== FIN DU SUIVI DE DIEU ===\n" << std::flush;
            return finish_progress(activated?EvolutionProgressStage::Complete:EvolutionProgressStage::Failed,
                                   "Commit "+activation.value("commit", "inconnu"),activated,activated);
          } catch (const json::parse_error&) {
            output_ << "[Activation] activation.json est encore en cours d'ecriture.\n";
          }
        } else if (verification.value("status", "") == "rejected") {
          const auto count_text = read_text(run_directory / "correction-count");
          try {
            if (std::stoi(count_text) >= god_max_corrections()) {
              output_ << "[Dieu] Limite de corrections atteinte.\n"
                      << "=== FIN DU SUIVI DE DIEU ===\n" << std::flush;
              return finish_progress(EvolutionProgressStage::Failed,
                                     "La limite de corrections est atteinte.",false,false);
            }
          } catch (...) {
          }
        }
      } catch (const json::parse_error&) {
        output_ << "[Verifier] verification.json est encore en cours d'ecriture.\n";
      }
    }
    if (god_failed || correction_failed || activation_failed) {
      print_evolution_diagnostics(output_, data_directory, run_directory);
      output_ << "=== FIN DU SUIVI DE DIEU ===\n" << std::flush;
      return finish_progress(EvolutionProgressStage::Failed,
                             current_log.empty()?"Consultez les diagnostics dans le terminal.":current_log,
                             false,false);
    }

    if (!work_started && now >= queue_deadline) {
      std::ofstream(run_directory / "ui-queue-timeout") << timestamp() << '\n';
      output_ << "[Orchestrateur] Delai de file d'attente depasse "
              << "(GOD_QUEUE_TIMEOUT_SECONDS). Dieu n'a pas encore demarre ; "
              << "le daemon peut poursuivre en arriere-plan.\n";
      print_evolution_diagnostics(output_, data_directory, run_directory);
      output_ << "=== FIN DU SUIVI DE DIEU ===\n" << std::flush;
      return finish_progress(EvolutionProgressStage::TimedOut,
                             "Dieu peut encore démarrer dans le daemon.",false,true);
    }
    if (work_started && now >= work_deadline) {
      std::ofstream(run_directory / "ui-work-timeout") << timestamp() << '\n';
      output_ << "[Dieu] Delai de travail depasse (GOD_WAIT_TIMEOUT_SECONDS). "
              << "L'instance a bien demarre et peut poursuivre dans le daemon.\n";
      print_evolution_diagnostics(output_, data_directory, run_directory);
      output_ << "=== FIN DU SUIVI DE DIEU ===\n" << std::flush;
      return finish_progress(EvolutionProgressStage::TimedOut,
                             "Le daemon peut poursuivre le travail en arrière-plan.",false,true);
    }
    if (now - last_heartbeat >= std::chrono::seconds(15)) {
      const auto reference = work_started ? work_started_at : queue_started_at;
      const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - reference).count();
      output_ << "[Suivi] " << (work_started ? "Dieu travaille" : "attente du daemon")
              << " depuis " << elapsed << " seconde(s).\n" << std::flush;
      last_heartbeat = now;
    }
    const auto detail=!current_log.empty()?current_log:last_daemon_log;
    if(!show_progress(progress_stage(phase),detail,phase=="activation-terminee"))return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

bool HumanValidation::review_devil(int day,int simulation_cycle) {
  const path data_directory(data_directory_);
  auto constraints=current_requests(data_directory,day,simulation_cycle,output_,notices_,true);
  if(constraints.empty())return true;
  const auto request=constraints.back();
  const auto request_id=request.value("id","unknown");
  const bool automatic=[](){const char* value=std::getenv("DEVIL_AUTO_APPROVE");return value&&std::string(value)=="1";}();

  output_ << "\n=== APPARITION DU DIABLE ===\n"
          << request.value("title","Contrainte sans titre") << "\n"
          << "Difficulté : " << request.value("difficulty",1) << "/5\n"
          << "Adaptation : " << request.value("adaptation",json::object()).value("rationale","non précisée") << "\n"
          << "Fondement réel : " << request.value("real_world_basis","non précisé") << "\n"
          << "Pression future : " << request.value("future_pressure","non précisée") << "\n";

  auto decide=[&](bool approve,const std::string& mode){
    auto decided=request;
    decided["status"]=approve?"approved":"rejected";
    decided[approve?"approved_at":"rejected_at"]=timestamp();
    decided[approve?"approval_mode":"rejection_mode"]=mode;
    if(!approve)decided["rejection_reason"]="Contrainte refusée par la politique du Diable";
    append_jsonl(data_directory/(approve?"approved_feature_requests.jsonl":"rejected_feature_requests.jsonl"),decided);
    const auto reason=approve?(automatic?"Approbation automatique configurée du Diable":"Approbation humaine explicite de la contrainte"):
                              "Refus humain explicite de la contrainte";
    write_validation_record(data_directory,decided,approve?"approve":"reject",reason,mode);
    output_ << (approve?"Contrainte approuvée : ":"Contrainte refusée : ") << request_id << "\n";
    return !approve||wait_for_evolution(request_id);
  };

  if(automatic){
    output_ << "Approbation automatique activée par DEVIL_AUTO_APPROVE=1.\n";
    return decide(true,"devil_automatic");
  }

  while(true){
    output_ << "[a] accepter  [r] refuser  [d] détail  [q/exit] arrêter.\n> " << std::flush;
    std::string line;
    if(!std::getline(input_,line))return false;
    if(line=="a"||line=="A")return decide(true,"human");
    if(line=="r"||line=="R")return decide(false,"human");
    if(line=="d"||line=="D"){output_<<request.dump(2)<<'\n';continue;}
    if(line=="q"||line=="Q"||line=="exit"||line=="quit")return false;
    output_ << "Choisissez a, r ou d.\n";
  }
}

bool HumanValidation::review_window(int day, int simulation_cycle) {
  const path data_directory(data_directory_);
  if(!review_devil(day,simulation_cycle))return false;
  window_request_ids_.clear();
  std::size_t selected_index=0;
  bool decision_made = false;
  while (true) {
    auto requests = select_window_requests(
        current_requests(data_directory, day, simulation_cycle, output_, notices_),
        window_request_ids_, output_);
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

    ValidationStage stage=ValidationStage::Complete;
    if(requests.empty())stage=ValidationStage::Empty;
    else if(selected_index==0&&!decision_made)stage=ValidationStage::Choose;
    else if(selected_index>0&&!decision_made)stage=ValidationStage::Confirm;
    std::string line;
    if(interface_)line=interface_->request_command(
        {stage,day,simulation_cycle,requests,selected_index});
    else if(!std::getline(input_,line))return false;
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
      output_ << (approve ? "Demande approuvée : " : "Demande refusée : ") << request_id << '\n'
              << "Statut enregistré : " << (approve ? "approved" : "rejected")
              << ".\n";
      if (approve && !wait_for_evolution(request_id)) return false;
      selected_index=0;
      decision_made=true;
      if(approve&&interface_)return true;
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

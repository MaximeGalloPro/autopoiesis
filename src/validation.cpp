#include "autopoiesis/validation.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/select.h>
#include <unistd.h>
#endif

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

bool input_ready(std::istream& input) {
  if(input.rdbuf()&&input.rdbuf()->in_avail()>0)return true;
#if defined(__unix__) || defined(__APPLE__)
  if(input.rdbuf()!=std::cin.rdbuf())return false;
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET(STDIN_FILENO,&readable);
  timeval timeout{};
  return select(STDIN_FILENO+1,&readable,nullptr,nullptr,&timeout)>0;
#else
  return false;
#endif
}
}

HumanValidation::HumanValidation(std::string data_directory, std::istream& input,
                                 std::ostream& output, IValidationInterface* interface)
    : data_directory_(std::move(data_directory)), input_(input), output_(output),
      interface_(interface) {}

ValidationWindowState HumanValidation::poll_evolution(const std::string& request_id) {
  const auto now=std::chrono::steady_clock::now();
  const auto monitor_it=evolution_monitors_.try_emplace(request_id,
      EvolutionMonitor{now,std::nullopt,{}}).first;
  auto& monitor=monitor_it->second;
  const path data_directory(data_directory_);
  const path run_directory=data_directory/"evolution_runs"/request_id;
  const bool prompt_ready=std::filesystem::exists(run_directory/"god-prompt.txt");
  const bool god_started=std::filesystem::exists(run_directory/"god-started");
  const bool god_failed=std::filesystem::exists(run_directory/"god-failed") ||
                        std::filesystem::exists(run_directory/"god-correction-failed");
  const bool activation_failed=std::filesystem::exists(run_directory/"activation-failed");
  const bool verification_started=std::filesystem::exists(run_directory/"verification-started");
  const bool verification_ready=std::filesystem::exists(run_directory/"verification.json");
  const bool activation_ready=std::filesystem::exists(run_directory/"activation.json");
  std::string verification_status;
  if(verification_ready)try{verification_status=json::parse(read_text(run_directory/"verification.json")).value("status","");}catch(const json::parse_error&){}
  if(god_started&&!monitor.started_at){
    monitor.started_at=now;
    output_ << "[Orchestrateur] Dieu a démarré pour " << request_id << ".\n";
  }
  const std::string phase=god_failed?"echec":activation_failed?"activation-echec":activation_ready?"activation-terminee":
      verification_status=="verified"?"activation":verification_ready?"correction":verification_started?"verification":
      god_started?"implementation":prompt_ready?"preparation":"attente";
  const auto stage=[&]{
    if(phase=="preparation")return EvolutionProgressStage::Preparing;
    if(phase=="implementation")return EvolutionProgressStage::Implementing;
    if(phase=="verification")return EvolutionProgressStage::Verifying;
    if(phase=="correction")return EvolutionProgressStage::Correcting;
    if(phase=="activation")return EvolutionProgressStage::Activating;
    if(phase=="activation-terminee")return EvolutionProgressStage::Complete;
    if(phase=="echec"||phase=="activation-echec")return EvolutionProgressStage::Failed;
    return EvolutionProgressStage::Queued;
  }();
  const auto reference=monitor.started_at.value_or(monitor.queued_at);
  const auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-reference).count();
  const auto detail=[&]{const auto error=last_non_empty_line(run_directory/"god.stderr.log");return error.empty()?last_non_empty_line(run_directory/"god.stdout.log"):error;}();
  if(phase!=monitor.last_phase){
    monitor.last_phase=phase;
    output_ << "[Dieu] " << request_id << " : " << phase << ".\n" << std::flush;
  }
  const auto notify=[&](EvolutionProgressStage current_stage,bool successful){
    return !interface_||interface_->present_evolution_progress({current_stage,request_id,{},detail,elapsed,successful});
  };
  if(god_failed||activation_failed){
    print_evolution_diagnostics(output_,data_directory,run_directory);
    evolution_monitors_.erase(monitor_it);
    return notify(EvolutionProgressStage::Failed,false)?ValidationWindowState::Resolved:ValidationWindowState::StopRequested;
  }
  if(activation_ready){
    bool activated=false;
    try{activated=json::parse(read_text(run_directory/"activation.json")).value("status","")=="activated";}catch(const json::parse_error&){}
    evolution_monitors_.erase(monitor_it);
    return notify(activated?EvolutionProgressStage::Complete:EvolutionProgressStage::Failed,activated)?
        ValidationWindowState::Resolved:ValidationWindowState::StopRequested;
  }
  const auto deadline=monitor.started_at?*monitor.started_at+std::chrono::seconds(timeout_seconds("GOD_WAIT_TIMEOUT_SECONDS",900)):
                                           monitor.queued_at+std::chrono::seconds(timeout_seconds("GOD_QUEUE_TIMEOUT_SECONDS",900));
  if(now>=deadline){
    std::ofstream(run_directory/(monitor.started_at?"ui-work-timeout":"ui-queue-timeout"))<<timestamp()<<'\n';
    evolution_monitors_.erase(monitor_it);
    return notify(EvolutionProgressStage::TimedOut,false)?ValidationWindowState::Resolved:ValidationWindowState::StopRequested;
  }
  return notify(stage,false)?ValidationWindowState::Pending:ValidationWindowState::StopRequested;
}

std::optional<std::string> HumanValidation::poll_command(const ValidationPrompt& prompt) {
  if(interface_)return interface_->poll_command(prompt);
  if(!input_ready(input_))return std::nullopt;
  std::string line;
  if(!std::getline(input_,line))return "q";
  return line;
}

ValidationWindowState HumanValidation::advance_devil(ActiveWindow& window) {
  const auto& request=*window.devil_request;
  const auto request_id=request.value("id","");
  const bool automatic=[](){const char* value=std::getenv("DEVIL_AUTO_APPROVE");return value&&std::string(value)=="1";}();
  const auto decide=[&](bool approve,const std::string& mode) {
    const path data_directory(data_directory_);
    auto decided=request;
    decided["status"]=approve?"approved":"rejected";
    decided[approve?"approved_at":"rejected_at"]=timestamp();
    decided[approve?"approval_mode":"rejection_mode"]=mode;
    if(!approve)decided["rejection_reason"]="Contrainte refusée par la politique du Diable";
    append_jsonl(data_directory/(approve?"approved_feature_requests.jsonl":"rejected_feature_requests.jsonl"),decided);
    const auto reason=approve?(automatic?"Approbation automatique configurée du Diable":"Approbation humaine explicite de la contrainte"):
                              "Refus humain explicite de la contrainte";
    write_validation_record(data_directory,decided,approve?"approve":"reject",reason,mode);
    output_ << (approve?"Contrainte approuvée : ":"Contrainte refusée : ")
            << request.value("id","unknown") << "\n";
  };

  if(automatic){
    output_ << "Approbation automatique activée par DEVIL_AUTO_APPROVE=1.\n";
    decide(true,"devil_automatic");
    window.devil_request.reset();
    window.evolution_request_id=request_id;
    window.prompt_dirty=true;
    return ValidationWindowState::Pending;
  }
  const ValidationPrompt prompt{ValidationStage::Confirm,window.day,window.simulation_cycle,
                                {request},1,ValidationPromptKind::Devil};
  if(window.prompt_dirty){
    output_ << "\n=== APPARITION DU DIABLE ===\n"
            << request.value("title","Contrainte sans titre") << "\n"
            << "Difficulté : " << request.value("difficulty",1) << "/5\n"
            << "Adaptation : " << request.value("adaptation",json::object()).value("rationale","non précisée") << "\n"
            << "Fondement réel : " << request.value("real_world_basis","non précisé") << "\n"
            << "Pression future : " << request.value("future_pressure","non précisée") << "\n"
            << "[a] accepter  [r] refuser  [d] détail  [q/exit] arrêter.\n> " << std::flush;
    window.prompt_dirty=false;
  }
  const auto line=poll_command(prompt);
  if(!line)return ValidationWindowState::Pending;
  if(*line=="a"||*line=="A")decide(true,"human");
  else if(*line=="r"||*line=="R")decide(false,"human");
  else if(*line=="d"||*line=="D"){output_<<request.dump(2)<<'\n';return ValidationWindowState::Pending;}
  else if(*line=="q"||*line=="Q"||*line=="exit"||*line=="quit"){
    active_window_.reset();
    return ValidationWindowState::StopRequested;
  }else{
    output_ << "Choisissez a, r ou d.\n";
    return ValidationWindowState::Pending;
  }
  window.devil_request.reset();
  if(*line=="a"||*line=="A"){
    window.evolution_request_id=request_id;
    window.prompt_dirty=true;
    return ValidationWindowState::Pending;
  }
  window.prompt_dirty=true;
  if(window.requests.empty()){
    active_window_.reset();
    return ValidationWindowState::Resolved;
  }
  return ValidationWindowState::Pending;
}

ValidationWindowState HumanValidation::advance_feature(ActiveWindow& window) {
  if(window.requests.empty()){
    active_window_.reset();
    return ValidationWindowState::Resolved;
  }
  const bool choosing=window.selected_index==0;
  const ValidationPrompt prompt{choosing?ValidationStage::Choose:ValidationStage::Confirm,
                                window.day,window.simulation_cycle,window.requests,
                                window.selected_index,ValidationPromptKind::Feature};
  if(window.prompt_dirty){
    output_ << "\n=== VALIDATION HUMAINE ===\n"
            << "Jour " << window.day << " | Cycle elementaire " << window.simulation_cycle << "\n";
    if(choosing){
      output_ << window.requests.size() << " proposition(s) disponibles :\n";
      for(std::size_t index=0;index<window.requests.size();++index){
        const auto& request=window.requests[index];
        output_ << "[" << index+1 << "] " << request.value("id","?") << " — "
                << request.value("agent_name","?") << " — "
                << request.value("title","(sans titre)") << '\n';
      }
      output_ << "Choisissez une proposition (1-" << window.requests.size()
              << ") ou n pour aucune, d N détail, q/exit arrêter.\n> " << std::flush;
    }else{
      output_ << "Proposition sélectionnée : "
              << window.requests[window.selected_index-1].value("title","(sans titre)") << '\n'
              << "[a] approuver  [r] refuser  [b] revenir au choix  [d] détail  [q/exit] arrêter.\n> " << std::flush;
    }
    window.prompt_dirty=false;
  }
  const auto line=poll_command(prompt);
  if(!line)return ValidationWindowState::Pending;
  std::istringstream command(*line);
  std::string action;
  command>>action;
  if(action=="q"||action=="Q"||action=="exit"||action=="quit"){
    active_window_.reset();
    return ValidationWindowState::StopRequested;
  }
  if(choosing){
    if(action=="n"||action=="N"){
      output_ << "Aucune proposition sélectionnée ; les demandes restent pending.\n";
      active_window_.reset();
      return ValidationWindowState::Resolved;
    }
    if(action=="d"||action=="D"){
      std::size_t index{};
      command>>index;
      if(index>0&&index<=window.requests.size())output_<<window.requests[index-1].dump(2)<<'\n';
      else output_ << "Numéro de proposition invalide.\n";
      return ValidationWindowState::Pending;
    }
    try{window.selected_index=std::stoul(action);}catch(...){window.selected_index=0;}
    if(window.selected_index==0||window.selected_index>window.requests.size()){
      window.selected_index=0;
      output_ << "Choisissez une proposition valide ou n.\n";
      return ValidationWindowState::Pending;
    }
    window.prompt_dirty=true;
    return ValidationWindowState::Pending;
  }
  const auto& request=window.requests[window.selected_index-1];
  if(action=="b"||action=="B"){
    window.selected_index=0;
    window.prompt_dirty=true;
    return ValidationWindowState::Pending;
  }
  if(action=="d"||action=="D"){
    output_<<request.dump(2)<<'\n';
    return ValidationWindowState::Pending;
  }
  if(action!="a"&&action!="A"&&action!="r"&&action!="R"){
    output_ << "Choisissez a, r, b ou d.\n";
    return ValidationWindowState::Pending;
  }
  const bool approve=action=="a"||action=="A";
  const path data_directory(data_directory_);
  auto decided=request;
  decided["status"]=approve?"approved":"rejected";
  decided[approve?"approved_at":"rejected_at"]=timestamp();
  decided[approve?"approval_mode":"rejection_mode"]="human";
  if(!approve)decided["rejection_reason"]="Refus explicite dans l'interface intégrée";
  append_jsonl(data_directory/(approve?"approved_feature_requests.jsonl":"rejected_feature_requests.jsonl"),decided);
  write_validation_record(data_directory,decided,approve?"approve":"reject",
                          approve?"Approbation explicite dans l'interface intégrée":"Refus explicite dans l'interface intégrée");
  output_ << (approve?"Demande approuvée : ":"Demande refusée : ") << request.value("id","unknown") << '\n'
          << "Statut enregistré : " << (approve?"approved":"rejected") << ".\n";
  if(approve){
    window.evolution_request_id=request.value("id","");
    window.prompt_dirty=true;
    return ValidationWindowState::Pending;
  }
  active_window_.reset();
  return ValidationWindowState::Resolved;
}

ValidationWindowState HumanValidation::advance_window(int day,int simulation_cycle,bool open_window) {
  if(open_window&&active_window_)return ValidationWindowState::Pending;
  if(!active_window_){
    const path data_directory(data_directory_);
    window_request_ids_.clear();
    ActiveWindow window;
    window.day=day;
    window.simulation_cycle=simulation_cycle;
    const auto constraints=current_requests(data_directory,day,simulation_cycle,output_,notices_,true);
    if(!constraints.empty())window.devil_request=constraints.back();
    window.requests=select_window_requests(
        current_requests(data_directory,day,simulation_cycle,output_,notices_),window_request_ids_,output_);
    active_window_=std::move(window);
  }
  if(active_window_->evolution_request_id){
    const auto state=poll_evolution(*active_window_->evolution_request_id);
    if(state!=ValidationWindowState::Pending)active_window_.reset();
    return state;
  }
  if(active_window_->devil_request)return advance_devil(*active_window_);
  return advance_feature(*active_window_);
}
}

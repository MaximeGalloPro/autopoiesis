#include "autopoiesis/openai_client.hpp"
#include "autopoiesis/runtime_handoff.hpp"
#include "autopoiesis/validation.hpp"
#include "autopoiesis/web_interface.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <type_traits>
#include <unistd.h>

using namespace apo;

namespace {
int non_negative_int_from_env(const char* name,int fallback) {
  const char* configured=std::getenv(name);
  if(!configured)return fallback;
  try{const int value=std::stoi(configured);return value>=0?value:fallback;}
  catch(...){return fallback;}
}

int positive_int_from_env(const char* name,int fallback) {
  const char* configured=std::getenv(name);
  if(!configured)return fallback;
  try{const int value=std::stoi(configured);return value>0?value:fallback;}
  catch(...){return fallback;}
}

bool enabled_from_env(const char* name,bool fallback) {
  const char* configured=std::getenv(name);
  return configured?std::string(configured)!="0":fallback;
}
}

int main(int argc,char** argv) {
  int days=positive_int_from_env("SIMULATION_DAYS",100);
  int delay=non_negative_int_from_env("SIMULATION_DELAY_MS",500);
  int render_every_days=non_negative_int_from_env("SIMULATION_RENDER_EVERY_DAYS",1);
  unsigned seed=42;
  bool no_api=false;
  bool new_world=false;
  for(int index=1;index<argc;++index){
    const std::string argument=argv[index];
    const auto next=[&](auto& value){
      if(index+1>=argc)throw std::runtime_error("missing argument for "+argument);
      value=std::remove_cvref_t<decltype(value)>(std::stoll(argv[++index]));
    };
    if(argument=="--days")next(days);
    else if(argument=="--seed")next(seed);
    else if(argument=="--delay-ms")next(delay);
    else if(argument=="--render-every-days")next(render_every_days);
    else if(argument=="--no-api")no_api=true;
    else if(argument=="--new-world")new_world=true;
    else{std::cerr<<"Unknown option: "<<argument<<'\n';return 2;}
  }

  std::ostream protocol_output(std::cout.rdbuf());
  std::cout.rdbuf(std::cerr.rdbuf());
  std::unique_ptr<WebInterface> interface;
  try{
    interface=std::make_unique<WebInterface>(std::cin,protocol_output,delay);
    delay=interface->simulation_delay_ms(delay);
    const std::string data_directory=std::getenv("AUTOPOIESIS_DATA_DIR")?
        std::getenv("AUTOPOIESIS_DATA_DIR"):"/data";
    const std::string checkpoint_path=data_directory+"/simulation-state.json";
    if(new_world)std::filesystem::remove(checkpoint_path);
    Logger logger(data_directory);
    std::mt19937 local_rng(seed);
    LocalDecider local(local_rng);
    std::unique_ptr<OpenAIClient> reporter;
    if(!no_api){
      const char* key=std::getenv("LLM_API_KEY");
      const char* model=std::getenv("OPENAI_MODEL");
      if(!key||!model)throw std::runtime_error(
          "LLM_API_KEY and OPENAI_MODEL are required (or use --no-api)");
      std::size_t limit=100;
      if(const char* configured=std::getenv("LIMIT_LLM_API_CALLS")){
        try{limit=std::stoull(configured);}
        catch(...){throw std::runtime_error("LIMIT_LLM_API_CALLS must be a non-negative integer");}
      }
      std::string run_id=std::getenv("AUTOPOIESIS_RUN_ID")?
          std::getenv("AUTOPOIESIS_RUN_ID"):"";
      if(run_id.empty())run_id="run-"+std::to_string(std::chrono::duration_cast<
          std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())+
          "-"+std::to_string(getpid());
      setenv("AUTOPOIESIS_RUN_ID",run_id.c_str(),1);
      const char* base=std::getenv("OPENAI_BASE_URL");
      reporter=std::make_unique<OpenAIClient>(key,model,
          base?base:"https://api.openai.com/v1",
          ApiCallBudget(data_directory+"/api_budget.json",run_id,limit),
          [&logger](const std::string& diagnostic){logger.message(diagnostic);});
    }

    Simulation simulation(seed,local,logger,reporter.get(),checkpoint_path);
    Simulation::ValidationGate validation_gate;
    if(enabled_from_env("WAIT_FOR_HUMAN_VALIDATION",true)){
      auto human_validation=std::make_shared<HumanValidation>(
          data_directory,std::cin,std::cerr,interface.get());
      validation_gate=[human_validation](int day,int simulation_cycle){
        return human_validation->review_window(day,simulation_cycle);
      };
    }
    SimulationRunResult result;
    if(interface->idle_for(0))
      result=simulation.run(days,delay,render_every_days,validation_gate,interface.get());
    if(result.restart_requested&&result.remaining_days>0){
      const auto executable=std::filesystem::absolute(argv[0]);
      const auto project_root=executable.parent_path().parent_path();
      const int selected_delay=interface->simulation_delay_ms(delay);
      const int compile_status=recompile_backend(project_root,data_directory,*interface);
      if(compile_status!=0)throw std::runtime_error(
          "backend recompilation failed; see "+data_directory+"/recompilation.log");
      std::vector<std::string> original;
      for(int index=0;index<argc;++index)original.emplace_back(argv[index]);
      replace_process(executable,restart_arguments(original,result.remaining_days,selected_delay));
    }
    interface->complete();
    return 0;
  }catch(const std::exception& exception){
    if(interface)interface->fail(exception.what());
    std::cerr<<"Fatal: "<<exception.what()<<'\n';
    return 1;
  }
}

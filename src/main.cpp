#include "autopoiesis/openai_client.hpp"
#include "autopoiesis/renderer.hpp"
#include "autopoiesis/validation.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <unistd.h>
using namespace apo;

static int non_negative_int_from_env(const char* name, int fallback) {
  const char* configured=std::getenv(name);
  if(!configured) return fallback;
  try { int delay=std::stoi(configured); return delay>=0 ? delay : fallback; }
  catch(...) { return fallback; }
}

static int positive_int_from_env(const char* name, int fallback) {
  const char* configured=std::getenv(name);
  if(!configured) return fallback;
  try {
    int value=std::stoi(configured);
    return value>0 ? value : fallback;
  } catch(...) {
    return fallback;
  }
}

static bool enabled_from_env(const char* name, bool fallback) {
  const char* configured=std::getenv(name);
  if(!configured) return fallback;
  return std::string(configured)!="0";
}

int main(int argc,char** argv){
  int days=positive_int_from_env("SIMULATION_DAYS",100),delay=non_negative_int_from_env("SIMULATION_DELAY_MS",500),render_every_days=non_negative_int_from_env("SIMULATION_RENDER_EVERY_DAYS",1);unsigned seed=42;bool no_api=false,new_world=false;
  for(int i=1;i<argc;++i){std::string a=argv[i];auto next=[&](auto&v){if(i+1>=argc)throw std::runtime_error("missing argument for "+a);v=std::remove_cvref_t<decltype(v)>(std::stoll(argv[++i]));};if(a=="--days")next(days);else if(a=="--seed")next(seed);else if(a=="--delay-ms")next(delay);else if(a=="--render-every-days")next(render_every_days);else if(a=="--no-api")no_api=true;else if(a=="--new-world")new_world=true;else{std::cerr<<"Unknown option: "<<a<<'\n';return 2;}}
  try{
    const std::string data_directory=std::getenv("AUTOPOIESIS_DATA_DIR")?std::getenv("AUTOPOIESIS_DATA_DIR"):"/data";
    const std::string checkpoint_path=data_directory+"/simulation-state.json";
    if(new_world)std::filesystem::remove(checkpoint_path);
    Logger logger(data_directory);std::mt19937 local_rng(seed);LocalDecider local(local_rng);std::unique_ptr<OpenAIClient> reporter;
    if(!no_api){const char*key=std::getenv("LLM_API_KEY");const char*model=std::getenv("OPENAI_MODEL");if(!key||!model){std::cerr<<"LLM_API_KEY and OPENAI_MODEL are required (or use --no-api).\n";return 2;}std::size_t limit=100;if(const char*configured=std::getenv("LIMIT_LLM_API_CALLS")){try{limit=std::stoull(configured);}catch(...){std::cerr<<"LIMIT_LLM_API_CALLS must be a non-negative integer.\n";return 2;}}std::string run_id=std::getenv("AUTOPOIESIS_RUN_ID")?std::getenv("AUTOPOIESIS_RUN_ID"):"";if(run_id.empty())run_id="run-"+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())+"-"+std::to_string(getpid());setenv("AUTOPOIESIS_RUN_ID",run_id.c_str(),1);const char*base=std::getenv("OPENAI_BASE_URL");reporter=std::make_unique<OpenAIClient>(key,model,base?base:"https://api.openai.com/v1",ApiCallBudget(data_directory+"/api_budget.json",run_id,limit),[&logger](const std::string& diagnostic){logger.message(diagnostic);});}
    Simulation simulation(seed,local,logger,reporter.get(),checkpoint_path);
    Simulation::ValidationGate validation_gate;
    if(enabled_from_env("WAIT_FOR_HUMAN_VALIDATION",true)) {
      auto human_validation=std::make_shared<HumanValidation>(data_directory,std::cin,std::cout);
      validation_gate=[human_validation](int day, int simulation_cycle) {
        return human_validation->review_window(day,simulation_cycle);
      };
    }
    simulation.run(days,delay,render_every_days,validation_gate);
    return 0;
  }catch(const std::exception&e){std::cerr<<"Fatal: "<<e.what()<<'\n';return 1;}
}

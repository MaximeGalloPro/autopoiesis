#include "autopoiesis/api_budget.hpp"
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/file.h>
#include <unistd.h>

namespace apo {
using json = nlohmann::json;
ApiCallBudget::ApiCallBudget(std::string state_path,std::string run_id,std::size_t limit):state_path_(std::move(state_path)),lock_path_(state_path_+".lock"),run_id_(std::move(run_id)),limit_(limit){std::error_code ec;std::filesystem::create_directories(std::filesystem::path(state_path_).parent_path(),ec);}
bool ApiCallBudget::try_acquire(){if(limit_==0)return false;int fd=open(lock_path_.c_str(),O_CREAT|O_RDWR,0600);if(fd<0)return false;if(flock(fd,LOCK_EX)!=0){close(fd);return false;}bool allowed=false;json state=json::object();std::ifstream in(state_path_);if(in){try{in>>state;if(!state.is_object())state=json::object();}catch(...){flock(fd,LOCK_UN);close(fd);return false;}}auto& runs=state["runs"];if(!runs.is_object())runs=json::object();std::size_t calls=runs.value(run_id_,json::object()).value("calls",0u);if(calls<limit_){runs[run_id_]={{"calls",calls+1},{"limit",limit_}};std::string temp=state_path_+".tmp."+std::to_string(getpid());{std::ofstream out(temp,std::ios::trunc);if(out){out<<state.dump()<<'\n';out.flush();allowed=static_cast<bool>(out);}}if(allowed){std::error_code ec;std::filesystem::rename(temp,state_path_,ec);allowed=!ec;}else{std::error_code ec;std::filesystem::remove(temp,ec);}}flock(fd,LOCK_UN);close(fd);return allowed;}
}

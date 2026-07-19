#include "autopoiesis/runtime_handoff.hpp"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace apo {
namespace {
std::string last_non_empty_line(const std::filesystem::path& path) {
  std::ifstream input(path);
  std::string line,last;
  while(std::getline(input,line))if(!line.empty())last=line;
  if(last.size()>500)last=last.substr(0,500)+"...";
  return last;
}
}

std::vector<std::string> restart_arguments(const std::vector<std::string>& original,
                                           int remaining_days,int delay_ms) {
  if(original.empty())throw std::invalid_argument("restart arguments require an executable");
  std::vector<std::string> result{original.front()};
  for(std::size_t index=1;index<original.size();++index){
    if(original[index]=="--new-world")continue;
    if(original[index]=="--days"||original[index]=="--delay-ms"){
      if(index+1<original.size())++index;
      continue;
    }
    result.push_back(original[index]);
  }
  result.push_back("--days");result.push_back(std::to_string(std::max(0,remaining_days)));
  result.push_back("--delay-ms");result.push_back(std::to_string(std::clamp(delay_ms,0,10000)));
  return result;
}

static int recompile_target(const std::filesystem::path& project_root,
                            const std::filesystem::path& data_directory,
                            IUserInterface& interface,const char* target) {
  std::filesystem::create_directories(data_directory);
  const auto log_path=data_directory/"recompilation.log";
  const auto started=std::chrono::steady_clock::now();
  const pid_t child=fork();
  if(child<0)throw std::runtime_error("unable to start background recompilation");
  if(child==0){
    const int log=open(log_path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(log>=0){dup2(log,STDOUT_FILENO);dup2(log,STDERR_FILENO);close(log);}
    const auto build=(project_root/"build").string();
    execlp("cmake","cmake","--build",build.c_str(),"--target",target,"-j2",
           static_cast<char*>(nullptr));
    _exit(127);
  }

  int status{};
  while(true){
    const auto waited=waitpid(child,&status,WNOHANG);
    if(waited==child)break;
    if(waited<0&&errno!=EINTR)throw std::runtime_error("unable to monitor background recompilation");
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-started).count();
    if(!interface.present_recompilation({RecompileStage::Compiling,elapsed,
                                         last_non_empty_line(log_path)})){
      kill(child,SIGTERM);waitpid(child,&status,0);return 130;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  const int code=WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now()-started).count();
  interface.present_recompilation({code==0?RecompileStage::Ready:RecompileStage::Failed,
                                    elapsed,last_non_empty_line(log_path)});
  return code;
}

int recompile_backend(const std::filesystem::path& project_root,
                      const std::filesystem::path& data_directory,
                      IUserInterface& interface) {
  return recompile_target(project_root,data_directory,interface,"autopoiesis_backend");
}

[[noreturn]] void replace_process(const std::filesystem::path& executable,
                                  const std::vector<std::string>& arguments) {
  std::vector<char*> values;
  values.reserve(arguments.size()+1);
  for(const auto& argument:arguments)values.push_back(const_cast<char*>(argument.c_str()));
  values.push_back(nullptr);
  const long maximum=std::min<long>(sysconf(_SC_OPEN_MAX),65536);
  for(int descriptor=3;descriptor<maximum;++descriptor){
    const int flags=fcntl(descriptor,F_GETFD);
    if(flags>=0)fcntl(descriptor,F_SETFD,flags|FD_CLOEXEC);
  }
  execv(executable.c_str(),values.data());
  throw std::runtime_error("unable to transfer to recompiled executable");
}
}

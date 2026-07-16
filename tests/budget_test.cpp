#include "autopoiesis/api_budget.hpp"
#include <cassert>
#include <iostream>
#include <unistd.h>
int main(){std::string path="/tmp/autopoiesis-budget-"+std::to_string(getpid())+".json";apo::ApiCallBudget budget(path,"test-run",2);assert(budget.try_acquire());assert(budget.try_acquire());assert(!budget.try_acquire());apo::ApiCallBudget resumed(path,"test-run",2);assert(!resumed.try_acquire());std::cout<<"api budget test passed\n";}

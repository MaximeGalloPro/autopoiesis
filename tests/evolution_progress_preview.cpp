#include "autopoiesis/raylib_interface.hpp"
#include "raylib.h"
#include <cstdlib>

using namespace apo;

int main() {
  const bool complete=std::getenv("AUTOPOIESIS_GUI_PREVIEW_EVOLUTION_COMPLETE");
  EvolutionProgress progress{
      complete?EvolutionProgressStage::Complete:EvolutionProgressStage::Implementing,
      "run-preview-day-3-cycle-720-a1-ai-1",
      complete?"La nouvelle évolution est active":
               "Dieu écrit le test rouge puis l'implémentation minimale",
      complete?"Commit 943ec92":"Tests ciblés en préparation dans le worktree isolé.",
      complete?427:84,
      complete};
  if(complete)setenv("AUTOPOIESIS_GUI_AUTOMATION_COMMAND","o",1);
  RaylibInterface interface;
  if(complete){
    return interface.request_evolution_completion(progress)=="o"?0:1;
  }
  for(int frame=0;frame<30;++frame)if(!interface.present_evolution_progress(progress))return 1;
  if(const char* screenshot=std::getenv("AUTOPOIESIS_GUI_EVOLUTION_SCREENSHOT"))
    TakeScreenshot(screenshot);
  return 0;
}

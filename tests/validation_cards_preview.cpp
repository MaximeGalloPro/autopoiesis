#include "autopoiesis/raylib_interface.hpp"
#include <cstdlib>

using namespace apo;

int main() {
  ValidationPrompt prompt;
  prompt.stage=ValidationStage::Choose;
  prompt.day=3;
  prompt.simulation_cycle=720;
  prompt.requests={
      {{"id","preview-1"},{"agent_id","a1"},{"agent_name","Ada"},
       {"title","Construire un abri durable"},{"need","Se protéger du froid et de la pluie"},
       {"proposed_change","Récolter du bois puis assembler progressivement un abri"},
       {"mechanism",{{"summary","Un chantier local consomme trois unités de bois"}}}},
      {{"id","preview-2"},{"agent_id","a2"},{"agent_name","Borin"},
       {"title","Conserver la viande par séchage"},{"need","Maintenir une réserve entre deux chasses"},
       {"proposed_change","Transformer la venaison fraîche en portions conservées"},
       {"mechanism",{{"summary","Le séchage demande du temps et un emplacement adapté"}}}},
      {{"id","preview-3"},{"agent_id","a3"},{"agent_name","Cyra"},
       {"title","Partager une carte persistante"},{"need","Transmettre les zones explorées au groupe"},
       {"proposed_change","Créer puis consulter une carte commune mise à jour localement"},
       {"mechanism",{{"summary","Chaque observation confirmée enrichit la carte du groupe"}}}},
  };
  if(std::getenv("AUTOPOIESIS_GUI_PREVIEW_CONFIRM")){
    prompt.stage=ValidationStage::Confirm;
    prompt.selected_index=2;
  }
  RaylibInterface interface;
  return interface.request_command(prompt)=="q"?0:1;
}

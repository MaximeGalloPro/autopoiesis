import type { WorldSnapshot } from "../protocol";

const recipeAction = "craft_camp_item";
const teachingAction = "teach_skill";
const mapSharingAction = "share_map";

export interface CampEncyclopediaEntry {
  action: string;
  residents: string[];
}

export interface CampEncyclopedia {
  recipes: CampEncyclopediaEntry[];
  learnableSkills: CampEncyclopediaEntry[];
  mapSharing: { available: boolean; residents: string[] };
}

/**
 * Projection de présentation uniquement : chaque information vient de l'action
 * que le moteur a rendue disponible dans l'instantané courant. Aucun état n'est
 * mémorisé ni déduit de la carte privée des personnages.
 */
export function campEncyclopediaFromSnapshot(snapshot: WorldSnapshot): CampEncyclopedia {
  const residentsWith = (action: string) => snapshot.agents
    .filter((agent) => agent.alive && agent.available_actions.includes(action))
    .map((agent) => agent.name);
  const recipeResidents = residentsWith(recipeAction);
  const teachingResidents = residentsWith(teachingAction);
  const mapSharingResidents = residentsWith(mapSharingAction);

  return {
    recipes: recipeResidents.length === 0 ? [] : [{ action: recipeAction, residents: recipeResidents }],
    learnableSkills: teachingResidents.length === 0 ? [] : [{ action: teachingAction, residents: teachingResidents }],
    mapSharing: { available: mapSharingResidents.length > 0, residents: mapSharingResidents },
  };
}

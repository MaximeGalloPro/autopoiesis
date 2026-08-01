import type { WorldCell, WorldSnapshot } from "../protocol";

const ADULT_AGE_DAYS = 16;

export interface CampCohort {
  label: string;
  count: number;
}

export interface CampStatus {
  stage: string;
  chest: { occupation: number; capacity: number; level: number } | null;
  population: {
    living: number;
    extinct: boolean;
    ages: CampCohort[];
    generations: CampCohort[];
  };
  newCivilization: { available: false; reason: string };
}

function validChest(cell: WorldCell): CampStatus["chest"] {
  const chest = cell.camp_chest;
  if (!chest || !Number.isSafeInteger(chest.occupation) || !Number.isSafeInteger(chest.capacity)
    || !Number.isSafeInteger(chest.level) || chest.occupation < 0 || chest.capacity < 0) return null;
  return { occupation: chest.occupation, capacity: chest.capacity, level: chest.level };
}

function inferredStage(snapshot: WorldSnapshot, chest: CampStatus["chest"]): string {
  if (snapshot.camp_stage) return snapshot.camp_stage;
  const hasCampfire = snapshot.cells.some((cell) => cell.campfire);
  if (!hasCampfire) return "Avant le foyer";
  if (chest?.level && chest.level >= 2) return "Camp renforcé";
  if (chest || snapshot.cells.some((cell) => cell.shelter_level > 0)) return "Foyer établi";
  return "Foyer naissant";
}

/**
 * Projection de lecture uniquement : elle ne complète jamais l’état du monde.
 * Les âges et générations restent signalés comme absents tant que le moteur
 * ne les transmet pas dans son instantané.
 */
export function campStatusFromSnapshot(snapshot: WorldSnapshot): CampStatus {
  const chest = snapshot.cells.map(validChest).find((candidate) => candidate !== null) ?? null;
  const residents = snapshot.agents.filter((agent) => agent.alive);
  const ages = { children: 0, adults: 0, unknown: 0 };
  const generations = new Map<number, number>();
  let unknownGeneration = 0;

  for (const resident of residents) {
    if (typeof resident.age_days !== "number") ages.unknown += 1;
    else if (resident.age_days < ADULT_AGE_DAYS) ages.children += 1;
    else ages.adults += 1;

    if (typeof resident.generation !== "number") unknownGeneration += 1;
    else generations.set(resident.generation, (generations.get(resident.generation) ?? 0) + 1);
  }

  return {
    stage: inferredStage(snapshot, chest),
    chest,
    population: {
      living: residents.length,
      extinct: snapshot.agents.length > 0 && residents.length === 0,
      ages: [
        { label: "Enfants", count: ages.children },
        { label: "Adultes", count: ages.adults },
        { label: "Âge non transmis", count: ages.unknown },
      ],
      generations: [
        ...[...generations.entries()]
          .sort(([left], [right]) => left - right)
          .map(([generation, count]) => ({ label: `Génération ${generation}`, count })),
        ...(unknownGeneration > 0 ? [{ label: "Génération non transmise", count: unknownGeneration }] : []),
      ],
    },
    // Le BFF ne déclare aucune commande de réinitialisation : ne jamais en simuler une côté client.
    newCivilization: {
      available: false,
      reason: "Aucune commande validée ne permet encore de créer une civilisation.",
    },
  };
}

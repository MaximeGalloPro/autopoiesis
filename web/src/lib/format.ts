import type { Attributes, Season } from "../protocol";

export const seasonLabels: Record<Season, string> = {
  spring: "Printemps",
  summer: "Été",
  autumn: "Automne",
  winter: "Hiver",
};

export const attributeLabels: Record<keyof Attributes, string> = {
  strength: "Force",
  agility: "Agilité",
  endurance: "Endurance",
  toughness: "Robustesse",
  recuperation: "Récupération",
  disease_resistance: "Résist. maladies",
  focus: "Concentration",
  willpower: "Volonté",
  memory: "Mémoire",
  spatial_sense: "Sens spatial",
};

export const actionLabels: Record<string, string> = {
  observe: "Observer",
  move: "Se déplacer",
  wait: "Attendre",
  talk: "Échanger",
  sleep: "Dormir",
  rest: "Se reposer",
  drink: "Boire",
  eat_food: "Manger",
  eat_berries: "Manger des baies",
  hunt_animal: "Chasser",
  hunt_rabbit: "Chasser un lapin",
  build_shelter: "Bâtir un abri",
  harvest_wood: "Récolter du bois",
  assemble_shelter: "Assembler l’abri",
  collect_branch: "Ramasser une branche",
  build_campfire: "Allumer un feu",
  rest_by_campfire: "Se reposer au feu",
  collect_food: "Collecter un aliment",
  deposit_food: "Déposer à la réserve",
  eat_carried_food: "Manger la provision",
  eat_camp_food: "Manger à la réserve",
};

export const animalLabels: Record<string, string> = {
  rabbit: "Lapin",
  deer: "Cerf",
  boar: "Sanglier",
  wolf: "Loup",
  fish: "Poisson",
};

export function clampPercent(value: number): number {
  return Math.max(0, Math.min(100, value));
}

export function formatDuration(milliseconds: number): string {
  const totalSeconds = Math.floor(milliseconds / 1_000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return minutes > 0 ? `${minutes} min ${seconds.toString().padStart(2, "0")} s` : `${seconds} s`;
}

export function stringifyMechanism(mechanism: string | Record<string, unknown>): string {
  if (typeof mechanism === "string") return mechanism;
  return Object.entries(mechanism)
    .map(([key, value]) => `${key.replaceAll("_", " ")} : ${Array.isArray(value) ? value.join(", ") : String(value)}`)
    .join(" · ");
}

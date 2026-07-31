import type { AiActivity } from "../protocol";

const MAX_REMEMBERED_CALLS = 96;

export interface ApiCallCounter {
  total: number;
  known_activity_keys: string[];
  has_milestone_alert: boolean;
}

export function initialApiCallCounter(): ApiCallCounter {
  return { total: 0, known_activity_keys: [], has_milestone_alert: false };
}

function activityKey(activity: AiActivity): string {
  return [
    activity.simulation_cycle ?? "stream",
    activity.kind,
    activity.agent_id,
    activity.call_number,
  ].join(":");
}

function isPersistentTotal(value: number | undefined): value is number {
  return value !== undefined && Number.isSafeInteger(value) && value >= 0;
}

function withMilestone(total: number, knownActivityKeys: string[]): ApiCallCounter {
  return {
    total,
    known_activity_keys: knownActivityKeys,
    has_milestone_alert: total > 0 && total % 10 === 0,
  };
}

/**
 * Compteur de secours purement visuel : il déduplique les trames animées d'un
 * même appel. Dès que le serveur expose son total persistant, celui-ci prévaut.
 */
export function nextApiCallCounter(
  current: ApiCallCounter,
  activity: AiActivity | null,
  totalApiCalls?: number,
): ApiCallCounter {
  if (isPersistentTotal(totalApiCalls)) {
    return withMilestone(totalApiCalls, current.known_activity_keys);
  }
  if (!activity) return current;
  const key = activityKey(activity);
  if (current.known_activity_keys.includes(key)) return current;
  const knownActivityKeys = [...current.known_activity_keys, key].slice(-MAX_REMEMBERED_CALLS);
  return withMilestone(current.total + 1, knownActivityKeys);
}

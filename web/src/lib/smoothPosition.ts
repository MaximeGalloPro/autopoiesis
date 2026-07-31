export interface SmoothPosition {
  x: number;
  y: number;
  z: number;
}

/** Move at a constant visual speed and stop exactly at the authoritative target. */
export function advancePosition(
  current: SmoothPosition,
  target: SmoothPosition,
  deltaSeconds: number,
  unitsPerSecond = 3.2,
): SmoothPosition {
  const safeDelta = Math.max(0, Math.min(deltaSeconds, 0.1));
  const dx = target.x - current.x;
  const dy = target.y - current.y;
  const dz = target.z - current.z;
  const distance = Math.hypot(dx, dy, dz);
  const step = unitsPerSecond * safeDelta;
  if (distance === 0 || distance <= step) return { ...target };
  const ratio = step / distance;
  return {
    x: current.x + dx * ratio,
    y: current.y + dy * ratio,
    z: current.z + dz * ratio,
  };
}

/** Advance a visual position without changing the authoritative target. */
export function interpolatePosition(
  current: SmoothPosition,
  target: SmoothPosition,
  deltaSeconds: number,
  responsiveness = 12,
): SmoothPosition {
  const safeDelta = Math.max(0, Math.min(deltaSeconds, 0.1));
  const factor = 1 - Math.exp(-responsiveness * safeDelta);
  return {
    x: current.x + (target.x - current.x) * factor,
    y: current.y + (target.y - current.y) * factor,
    z: current.z + (target.z - current.z) * factor,
  };
}

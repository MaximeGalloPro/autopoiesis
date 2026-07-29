export interface SmoothPosition {
  x: number;
  y: number;
  z: number;
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

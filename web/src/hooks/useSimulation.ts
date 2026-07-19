import { useCallback, useEffect, useRef, useState } from "react";
import type { BackendEvent, ClientMessage, ConnectionState, EngineCommand, PublicState } from "../protocol";

const emptyState: PublicState = {
  state: null,
  activity: null,
  validation: null,
  evolution: null,
  evolution_completion: null,
  recompilation: null,
  engine: { status: "starting", pid: null, restarts: 0, last_error: null },
  latest_event: null,
};

export function applyEvent(current: PublicState, event: BackendEvent): PublicState {
  const base = { ...current, latest_event: event };
  switch (event.type) {
    case "state": return {
      ...base,
      state: event.payload,
      activity: null,
      validation: null,
      evolution: null,
      evolution_completion: null,
      recompilation: null,
    };
    case "runtime": return {
      ...base,
      state: current.state ? {
        ...current.state,
        paused: event.payload.paused,
        speed: event.payload.speed,
        delay_ms: event.payload.delay_ms,
      } : null,
    };
    case "activity": return { ...base, activity: event.payload };
    case "validation": return { ...base, validation: event.payload, activity: null };
    case "evolution_progress": return { ...base, evolution: event.payload, validation: null };
    case "evolution_completion": return {
      ...base,
      evolution: event.payload,
      evolution_completion: event.payload,
      validation: null,
    };
    case "recompilation": return { ...base, recompilation: event.payload, evolution_completion: null };
    case "engine": return { ...base, engine: event.payload };
    case "notice": return base;
  }
}

export function useSimulation() {
  const [data, setData] = useState<PublicState>(emptyState);
  const [connection, setConnection] = useState<ConnectionState>("connecting");
  const [commandError, setCommandError] = useState<string | null>(null);
  const retryRef = useRef(0);

  useEffect(() => {
    let disposed = false;
    let socket: WebSocket | null = null;
    let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
    let stateFrame: number | null = null;
    let pendingState: Extract<BackendEvent, { type: "state" }> | null = null;

    const flushPendingState = () => {
      stateFrame = null;
      const event = pendingState;
      pendingState = null;
      if (event && !disposed) setData((current) => applyEvent(current, event));
    };

    const applyIncomingEvent = (event: BackendEvent) => {
      if (event.type === "state") {
        pendingState = event;
        if (stateFrame === null) stateFrame = window.requestAnimationFrame(flushPendingState);
        return;
      }

      const precedingState = pendingState;
      pendingState = null;
      if (stateFrame !== null) window.cancelAnimationFrame(stateFrame);
      stateFrame = null;
      setData((current) => applyEvent(
        precedingState ? applyEvent(current, precedingState) : current,
        event,
      ));
    };

    const connect = () => {
      if (disposed) return;
      setConnection(retryRef.current === 0 ? "connecting" : "reconnecting");
      const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
      socket = new WebSocket(`${protocol}//${window.location.host}/ws`);

      socket.addEventListener("open", () => {
        retryRef.current = 0;
        setConnection("connected");
      });
      socket.addEventListener("message", (message) => {
        try {
          const parsed = JSON.parse(String(message.data)) as ClientMessage;
          if (parsed.type === "snapshot") {
            pendingState = null;
            if (stateFrame !== null) window.cancelAnimationFrame(stateFrame);
            stateFrame = null;
            setData(parsed.payload);
          } else if (parsed.type === "event") {
            applyIncomingEvent(parsed.payload);
          }
        } catch {
          setConnection("error");
        }
      });
      socket.addEventListener("close", () => {
        if (disposed) return;
        retryRef.current += 1;
        setConnection("reconnecting");
        const delay = Math.min(8_000, 500 * 2 ** Math.min(retryRef.current, 4));
        reconnectTimer = setTimeout(connect, delay);
      });
      socket.addEventListener("error", () => socket?.close());
    };

    void fetch("/api/state")
      .then(async (response) => response.json() as Promise<PublicState>)
      .then((snapshot) => { if (!disposed) setData(snapshot); })
      .catch(() => { if (!disposed) setConnection("error"); });
    connect();

    return () => {
      disposed = true;
      if (reconnectTimer) clearTimeout(reconnectTimer);
      if (stateFrame !== null) window.cancelAnimationFrame(stateFrame);
      socket?.close();
    };
  }, []);

  const sendCommand = useCallback(async (command: EngineCommand): Promise<boolean> => {
    setCommandError(null);
    try {
      const response = await fetch("/api/commands", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(command),
      });
      if (!response.ok) {
        const payload = await response.json().catch(() => null) as { error?: string } | null;
        throw new Error(payload?.error ?? `Commande refusée (${response.status})`);
      }
      return true;
    } catch (error) {
      setCommandError(error instanceof Error ? error.message : "La commande n’a pas pu être transmise.");
      return false;
    }
  }, []);

  return { data, connection, commandError, dismissCommandError: () => setCommandError(null), sendCommand };
}

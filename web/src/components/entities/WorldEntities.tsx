import { Float, Html, Instance, Instances } from "@react-three/drei";
import { useFrame, type ThreeEvent } from "@react-three/fiber";
import { useEffect, useLayoutEffect, useRef, type ReactNode } from "react";
import type { Group } from "three";
import type { AgentState, AnimalState, Position, WorldCell, WorldSnapshot } from "../../protocol";
import { WORLD_HEIGHT, WORLD_WIDTH } from "../../protocol";
import { animalLabels } from "../../lib/format";
import { advancePosition, type SmoothPosition } from "../../lib/smoothPosition";

export type EntitySelection = { kind: "agent" | "animal"; id: string };

const agentPalette = ["#e7b85f", "#d4775d", "#82bca4", "#a68acb"];

export function worldPosition(position: Position, y = 0): [number, number, number] {
  return [position.x - WORLD_WIDTH / 2 + 0.5, y, position.y - WORLD_HEIGHT / 2 + 0.5];
}

function useSmoothWorldPosition(position: Position, y: number, baseRotation: [number, number, number] = [0, 0, 0]) {
  const groupRef = useRef<Group>(null);
  const initial = worldPosition(position, y);
  const targetRef = useRef<SmoothPosition>({ x: initial[0], y: initial[1], z: initial[2] });
  const visualRef = useRef<SmoothPosition>({ ...targetRef.current });
  const walkPhaseRef = useRef(0);

  useLayoutEffect(() => {
    groupRef.current?.position.set(visualRef.current.x, visualRef.current.y, visualRef.current.z);
    groupRef.current?.rotation.set(...baseRotation);
  }, [baseRotation]);
  useEffect(() => {
    const next = worldPosition(position, y);
    targetRef.current = { x: next[0], y: next[1], z: next[2] };
  }, [position.x, position.y, y]);
  useFrame((_, delta) => {
    if (!groupRef.current) return;
    const before = visualRef.current;
    visualRef.current = advancePosition(before, targetRef.current, delta);
    const dx = visualRef.current.x - before.x;
    const dz = visualRef.current.z - before.z;
    const moving = Math.hypot(targetRef.current.x - visualRef.current.x, targetRef.current.z - visualRef.current.z) > 0.001;
    if (moving) {
      walkPhaseRef.current += delta * 15;
      const heading = Math.atan2(dx, dz);
      groupRef.current.rotation.set(baseRotation[0], baseRotation[1] + heading, baseRotation[2]);
    } else {
      walkPhaseRef.current = 0;
      groupRef.current.rotation.set(...baseRotation);
    }
    const bob = moving ? Math.abs(Math.sin(walkPhaseRef.current)) * 0.035 : 0;
    groupRef.current.position.set(visualRef.current.x, visualRef.current.y + bob, visualRef.current.z);
  });
  return groupRef;
}

function SmoothPositionGroup({
  position,
  y,
  rotation,
  onClick,
  children,
}: {
  position: Position;
  y: number;
  rotation?: [number, number, number];
  onClick?: (event: ThreeEvent<MouseEvent>) => void;
  children: ReactNode;
}) {
  const positionRef = useSmoothWorldPosition(position, y, rotation);
  return <group ref={positionRef} rotation={rotation} onClick={onClick}>{children}</group>;
}

function offsetPosition(cell: WorldCell, y: number, x = 0, z = 0): [number, number, number] {
  const position = worldPosition(cell.position, y);
  return [position[0] + x, position[1], position[2] + z];
}

function markerHeight(cell: WorldCell, base: number): number {
  if (cell.terrain === "tree") return 1.55;
  if (cell.terrain === "bush") return 0.86;
  if (cell.terrain === "wall") return 0.94;
  return base;
}

function SelectionRing({ radius, y = 0 }: { radius: number; y?: number }) {
  return (
    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, y, 0]}>
      <ringGeometry args={[radius - 0.035, radius, 32]} />
      <meshBasicMaterial color="#f2ce80" transparent opacity={0.84} toneMapped={false} />
    </mesh>
  );
}

function ResourceMarkers({ cells }: { cells: WorldCell[] }) {
  const food = cells.filter((cell) => cell.food > 0);
  const wood = cells.filter((cell) => cell.wood > 0);
  const fibers = cells.filter((cell) => cell.fibers > 0);
  const branches = cells.filter((cell) => cell.branches > 0);
  const storedFood = cells.filter((cell) => cell.stored_food > 0);

  return (
    <>
      <Instances limit={Math.max(1, food.length * 3)} range={food.length * 3}>
        <sphereGeometry args={[0.1, 8, 6]} />
        <meshStandardMaterial color="#d85b50" roughness={0.58} />
        {food.flatMap((cell) => {
          const scale = Math.min(1.35, 0.82 + cell.food * 0.06);
          const y = markerHeight(cell, 0.39);
          return [
            <Instance key={`food-a-${cell.position.x}-${cell.position.y}`} position={offsetPosition(cell, y, -0.13, 0.04)} scale={scale} />,
            <Instance key={`food-b-${cell.position.x}-${cell.position.y}`} position={offsetPosition(cell, y + 0.01, 0.12, -0.05)} scale={scale * 0.92} />,
            <Instance key={`food-c-${cell.position.x}-${cell.position.y}`} position={offsetPosition(cell, y, 0.02, 0.14)} scale={scale * 0.8} />,
          ];
        })}
      </Instances>

      <Instances limit={Math.max(1, wood.length)} range={wood.length}>
        <cylinderGeometry args={[0.09, 0.1, 0.56, 7]} />
        <meshStandardMaterial color="#765039" roughness={0.95} />
        {wood.map((cell) => (
          <Instance
            key={`wood-${cell.position.x}-${cell.position.y}`}
            position={offsetPosition(cell, markerHeight(cell, 0.38))}
            rotation={[Math.PI / 2, 0, Math.PI / 5]}
            scale={Math.min(1.25, 0.88 + cell.wood * 0.05)}
          />
        ))}
      </Instances>

      <Instances limit={Math.max(1, fibers.length * 2)} range={fibers.length * 2}>
        <coneGeometry args={[0.11, 0.34, 5]} />
        <meshStandardMaterial color="#d8c47b" roughness={0.84} />
        {fibers.flatMap((cell) => {
          const y = markerHeight(cell, 0.43);
          return [
            <Instance key={`fiber-a-${cell.position.x}-${cell.position.y}`} position={offsetPosition(cell, y, -0.08, 0.06)} rotation={[0.15, 0, -0.2]} />,
            <Instance key={`fiber-b-${cell.position.x}-${cell.position.y}`} position={offsetPosition(cell, y, 0.09, -0.04)} rotation={[-0.12, 0, 0.18]} />,
          ];
        })}
      </Instances>

      <Instances limit={Math.max(1, branches.length * 2)} range={branches.length * 2}>
        <cylinderGeometry args={[0.035, 0.045, 0.48, 6]} />
        <meshStandardMaterial color="#a6754a" roughness={0.95} />
        {branches.flatMap((cell) => {
          const y = markerHeight(cell, 0.39);
          return [
            <Instance key={`branch-a-${cell.position.x}-${cell.position.y}`} position={offsetPosition(cell, y, -0.08, 0)} rotation={[Math.PI / 2, 0, Math.PI / 4]} />,
            <Instance key={`branch-b-${cell.position.x}-${cell.position.y}`} position={offsetPosition(cell, y + 0.02, 0.08, 0)} rotation={[Math.PI / 2, 0, -Math.PI / 4]} />,
          ];
        })}
      </Instances>

      <Instances limit={Math.max(1, storedFood.length)} range={storedFood.length}>
        <cylinderGeometry args={[0.2, 0.16, 0.22, 8]} />
        <meshStandardMaterial color="#b9823e" roughness={0.86} />
        {storedFood.map((cell) => (
          <Instance
            key={`store-${cell.position.x}-${cell.position.y}`}
            position={offsetPosition(cell, markerHeight(cell, 0.53))}
            scale={Math.min(1.3, 0.88 + cell.stored_food * 0.04)}
          />
        ))}
      </Instances>
      <Instances limit={Math.max(1, storedFood.length)} range={storedFood.length}>
        <sphereGeometry args={[0.11, 8, 6]} />
        <meshStandardMaterial color="#f0bf55" emissive="#5c3414" emissiveIntensity={0.22} roughness={0.55} />
        {storedFood.map((cell) => (
          <Instance key={`store-food-${cell.position.x}-${cell.position.y}`} position={offsetPosition(cell, markerHeight(cell, 0.69), 0, -0.03)} />
        ))}
      </Instances>
    </>
  );
}

function ShelterModels({ cells }: { cells: WorldCell[] }) {
  return cells.filter((cell) => cell.shelter_level > 0).map((cell) => {
    const scale = 0.78 + Math.min(3, cell.shelter_level) * 0.1;
    return (
      <group key={`shelter-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 0.18)} scale={scale}>
        <mesh castShadow receiveShadow>
          <cylinderGeometry args={[0.43, 0.43, 0.11, 6]} />
          <meshStandardMaterial color="#604632" roughness={1} />
        </mesh>
        <mesh castShadow position={[0, 0.34, 0]} rotation={[0, Math.PI / 4, 0]}>
          <coneGeometry args={[0.46, 0.62, 4]} />
          <meshStandardMaterial color={cell.shelter_level > 1 ? "#c39358" : "#a77648"} roughness={0.92} />
        </mesh>
        <mesh position={[0, 0.22, -0.39]}>
          <boxGeometry args={[0.15, 0.25, 0.05]} />
          <meshStandardMaterial color="#35291f" roughness={1} />
        </mesh>
        {cell.shelter_level > 1 && (
          <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.065, 0]}>
            <ringGeometry args={[0.47, 0.51, 4]} />
            <meshBasicMaterial color="#e2bd74" transparent opacity={0.45} />
          </mesh>
        )}
      </group>
    );
  });
}

function CampfireModels({
  cells,
  hasAlert = false,
  onCampfireClick,
}: {
  cells: WorldCell[];
  hasAlert?: boolean;
  onCampfireClick?: () => void;
}) {
  return cells.filter((cell) => cell.campfire).map((cell) => (
    <group
      key={`fire-${cell.position.x}-${cell.position.y}`}
      position={worldPosition(cell.position, 0.17)}
      onClick={hasAlert ? (event) => { event.stopPropagation(); onCampfireClick?.(); } : undefined}
    >
      {[0, 1, 2, 3, 4, 5].map((stone) => {
        const angle = (stone / 6) * Math.PI * 2;
        return (
          <mesh key={stone} position={[Math.cos(angle) * 0.27, 0, Math.sin(angle) * 0.27]} scale={[1, 0.55, 0.85]}>
            <dodecahedronGeometry args={[0.1, 0]} />
            <meshStandardMaterial color="#746758" roughness={1} />
          </mesh>
        );
      })}
      <mesh rotation={[0, 0, Math.PI / 2]} position={[0, 0.04, 0]}>
        <cylinderGeometry args={[0.055, 0.065, 0.53, 7]} />
        <meshStandardMaterial color="#5d3828" roughness={1} />
      </mesh>
      <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 0.055, 0]}>
        <cylinderGeometry args={[0.055, 0.065, 0.53, 7]} />
        <meshStandardMaterial color="#69412d" roughness={1} />
      </mesh>
      <Float speed={3.2} rotationIntensity={0.08} floatIntensity={0.1}>
        <mesh position={[0, 0.24, 0]}>
          <coneGeometry args={[0.18, 0.46, 7]} />
          <meshBasicMaterial color="#ff9442" toneMapped={false} />
        </mesh>
        <mesh position={[0.02, 0.3, -0.015]} scale={[0.56, 0.7, 0.56]}>
          <coneGeometry args={[0.18, 0.46, 7]} />
          <meshBasicMaterial color="#ffe08a" toneMapped={false} />
        </mesh>
      </Float>
      {hasAlert && (
        <Float speed={2.4} rotationIntensity={0.22} floatIntensity={0.26}>
          <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.62, 0]}>
            <ringGeometry args={[0.28, 0.35, 24]} />
            <meshBasicMaterial color="#ffe08a" transparent opacity={0.92} toneMapped={false} />
          </mesh>
        </Float>
      )}
      <pointLight color="#ff9d45" intensity={1.65} distance={4} decay={2} />
    </group>
  ));
}

type AgentMarker = "rest" | "provisions" | "materials" | "project" | "blocked" | null;

function markerFor(agent: AgentState): AgentMarker {
  if (agent.sleeping_days > 0 || agent.fatigue >= 85) return "rest";
  if (agent.carried_food) return "provisions";
  if (agent.wood_inventory > 0 || agent.branch_inventory > 0) return "materials";
  if (agent.project.status === "blocked") return "blocked";
  if (agent.project.status === "active") return "project";
  return null;
}

function AgentActivityMarker({ agent }: { agent: AgentState }) {
  const marker = markerFor(agent);
  if (!marker) return null;
  const color = marker === "rest" ? "#94c6db"
    : marker === "blocked" ? "#d89062"
      : marker === "materials" ? "#b78558"
        : marker === "provisions" ? "#e98562" : "#f0cb70";
  return (
    <Float speed={2.2} rotationIntensity={0.08} floatIntensity={0.12}>
      <group position={[0, 0.57, 0]}>
        {marker === "rest" && <>
          <mesh position={[-0.08, 0, 0]} scale={0.055}><sphereGeometry args={[1, 8, 6]} /><meshBasicMaterial color={color} toneMapped={false} /></mesh>
          <mesh position={[0.06, 0.05, 0]} scale={0.08}><sphereGeometry args={[1, 8, 6]} /><meshBasicMaterial color={color} toneMapped={false} /></mesh>
        </>}
        {marker === "materials" && <mesh rotation={[0, 0, Math.PI / 2]}><boxGeometry args={[0.22, 0.07, 0.07]} /><meshStandardMaterial color={color} roughness={0.8} /></mesh>}
        {marker === "provisions" && <mesh><sphereGeometry args={[0.1, 8, 6]} /><meshStandardMaterial color={color} roughness={0.55} /></mesh>}
        {marker === "project" && <mesh rotation={[0, Math.PI / 4, 0]}><octahedronGeometry args={[0.11, 0]} /><meshStandardMaterial color={color} emissive="#695015" emissiveIntensity={0.25} roughness={0.5} /></mesh>}
        {marker === "blocked" && <mesh rotation={[0, 0, Math.PI / 4]}><boxGeometry args={[0.13, 0.13, 0.08]} /><meshStandardMaterial color={color} roughness={0.72} /></mesh>}
      </group>
    </Float>
  );
}

function AgentModels({
  agents,
  selected,
  onSelect,
}: {
  agents: AgentState[];
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
}) {
  return agents.filter((agent) => agent.alive).map((agent, index) => {
    const isSelected = selected?.kind === "agent" && selected.id === agent.id;
    const hasPack = agent.wood_inventory > 0 || agent.branch_inventory > 0 || Boolean(agent.carried_food);
    return (
      <SmoothPositionGroup
        key={agent.id}
        position={agent.position}
        y={0.28}
        rotation={[0, (index % 4) * (Math.PI / 2), 0]}
        onClick={(event) => { event.stopPropagation(); onSelect({ kind: "agent", id: agent.id }); }}
      >
        <mesh castShadow position={[0, 0.1, 0.04]} rotation={[Math.PI / 2, 0, 0]}>
          <capsuleGeometry args={[0.15, 0.25, 4, 8]} />
          <meshStandardMaterial color={agentPalette[index % agentPalette.length]} roughness={0.63} />
        </mesh>
        <mesh castShadow position={[0, 0.17, -0.27]} scale={[1, 0.68, 1]}>
          <sphereGeometry args={[0.14, 12, 10]} />
          <meshStandardMaterial color="#e4b992" roughness={0.74} />
        </mesh>
        <mesh position={[0, 0.215, -0.3]} scale={[1.05, 0.24, 0.56]}>
          <sphereGeometry args={[0.14, 10, 8]} />
          <meshStandardMaterial color={["#483329", "#6a4731", "#342f2b", "#725343"][index % 4]} roughness={0.9} />
        </mesh>
        <mesh position={[0, 0.1, 0.11]}>
          <boxGeometry args={[0.38, 0.07, 0.1]} />
          <meshStandardMaterial color={agentPalette[index % agentPalette.length]} roughness={0.7} />
        </mesh>
        {hasPack && <mesh position={[0, 0.13, 0.25]}><boxGeometry args={[0.19, 0.17, 0.11]} /><meshStandardMaterial color="#68503b" roughness={0.94} /></mesh>}
        <AgentActivityMarker agent={agent} />
        {isSelected && <>
          <SelectionRing radius={0.39} y={-0.13} />
          <Html center position={[0, 0.88, 0]} distanceFactor={1} className="world-label">{agent.name}</Html>
        </>}
      </SmoothPositionGroup>
    );
  });
}

function AnimalSilhouette({ animal }: { animal: AnimalState }) {
  const dangerColor = animal.danger >= 60 ? "#bd6659" : null;
  if (animal.type === "rabbit") return <>
    <mesh scale={[0.84, 0.48, 1.16]}><sphereGeometry args={[0.21, 10, 8]} /><meshStandardMaterial color={dangerColor ?? "#c1a57a"} roughness={0.86} /></mesh>
    <mesh position={[0, 0.05, -0.17]} scale={[0.62, 0.5, 0.64]}><sphereGeometry args={[0.18, 10, 8]} /><meshStandardMaterial color={dangerColor ?? "#c1a57a"} roughness={0.86} /></mesh>
    <mesh position={[-0.065, 0.07, -0.31]} rotation={[Math.PI / 2, 0, 0]} scale={[0.35, 1, 0.7]}><capsuleGeometry args={[0.055, 0.16, 3, 6]} /><meshStandardMaterial color={dangerColor ?? "#d0b78c"} /></mesh>
    <mesh position={[0.065, 0.07, -0.31]} rotation={[Math.PI / 2, 0, 0]} scale={[0.35, 1, 0.7]}><capsuleGeometry args={[0.055, 0.16, 3, 6]} /><meshStandardMaterial color={dangerColor ?? "#d0b78c"} /></mesh>
  </>;
  if (animal.type === "deer") return <>
    <mesh rotation={[Math.PI / 2, 0, 0]}><capsuleGeometry args={[0.18, 0.36, 4, 8]} /><meshStandardMaterial color={dangerColor ?? "#bd8757"} roughness={0.84} /></mesh>
    <mesh position={[0, 0.06, -0.31]} scale={[0.76, 0.52, 0.86]}><sphereGeometry args={[0.16, 10, 8]} /><meshStandardMaterial color={dangerColor ?? "#bd8757"} roughness={0.84} /></mesh>
    {[-0.08, 0.08].map((x) => <mesh key={x} position={[x, 0.09, -0.45]} rotation={[0.35, 0, x * 3]}><coneGeometry args={[0.025, 0.2, 5]} /><meshStandardMaterial color="#e3d1a7" roughness={0.94} /></mesh>)}
  </>;
  if (animal.type === "boar") return <>
    <mesh scale={[1.25, 0.57, 0.88]}><sphereGeometry args={[0.25, 10, 8]} /><meshStandardMaterial color={dangerColor ?? "#76594a"} roughness={0.92} /></mesh>
    <mesh position={[0, 0.04, -0.23]} scale={[0.9, 0.48, 0.62]}><sphereGeometry args={[0.16, 10, 8]} /><meshStandardMaterial color={dangerColor ?? "#80604e"} roughness={0.92} /></mesh>
    {[-0.08, 0.08].map((x) => <mesh key={x} position={[x, 0.03, -0.35]} rotation={[Math.PI / 2, 0, 0]}><coneGeometry args={[0.027, 0.13, 5]} /><meshStandardMaterial color="#e5d7bb" /></mesh>)}
  </>;
  if (animal.type === "wolf") return <>
    <mesh rotation={[Math.PI / 2, 0, 0]} scale={[0.92, 0.78, 1.15]}><capsuleGeometry args={[0.16, 0.34, 4, 8]} /><meshStandardMaterial color={dangerColor ?? "#788077"} roughness={0.9} /></mesh>
    <mesh position={[0, 0.05, -0.32]} scale={[0.72, 0.5, 0.94]}><sphereGeometry args={[0.16, 10, 8]} /><meshStandardMaterial color={dangerColor ?? "#879087"} roughness={0.9} /></mesh>
    <mesh position={[0, 0.02, 0.35]} rotation={[Math.PI / 2, 0, Math.PI / 6]}><coneGeometry args={[0.1, 0.28, 5]} /><meshStandardMaterial color={dangerColor ?? "#6e766e"} roughness={0.9} /></mesh>
  </>;
  return <>
    <mesh scale={[1.26, 0.38, 0.72]}><sphereGeometry args={[0.19, 10, 8]} /><meshStandardMaterial color="#6aa7a8" roughness={0.5} metalness={0.08} /></mesh>
    <mesh position={[0.25, 0, 0]} rotation={[0, 0, -Math.PI / 2]}><coneGeometry args={[0.15, 0.3, 3]} /><meshStandardMaterial color="#4d8f99" roughness={0.55} /></mesh>
  </>;
}

function AnimalModels({
  animals,
  selected,
  onSelect,
}: {
  animals: AnimalState[];
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
}) {
  return animals.filter((animal) => animal.alive).map((animal) => {
    const isSelected = selected?.kind === "animal" && selected.id === animal.id;
    const radius = animal.type === "deer" || animal.type === "wolf" ? 0.42 : 0.35;
    const ringHeight = animal.type === "fish" ? -0.05 : -0.15;
    return (
      <SmoothPositionGroup
        key={animal.id}
        position={animal.position}
        y={animal.type === "fish" ? 0.2 : 0.31}
        onClick={(event) => { event.stopPropagation(); onSelect({ kind: "animal", id: animal.id }); }}
      >
        <AnimalSilhouette animal={animal} />
        {animal.danger >= 60 && <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, ringHeight, 0]}><ringGeometry args={[radius - 0.02, radius, 28]} /><meshBasicMaterial color="#b96358" transparent opacity={0.45} /></mesh>}
        {isSelected && <>
          <SelectionRing radius={radius} y={ringHeight} />
          <Html center position={[0, 0.7, 0]} distanceFactor={1} className="world-label">{animalLabels[animal.type] ?? animal.type}</Html>
        </>}
      </SmoothPositionGroup>
    );
  });
}

export function WorldEntities({
  snapshot,
  selected,
  onSelect,
  campfireAlert = false,
  onCampfireClick,
}: {
  snapshot: WorldSnapshot;
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
  campfireAlert?: boolean;
  onCampfireClick?: () => void;
}) {
  return <>
    <ShelterModels cells={snapshot.cells} />
    <ResourceMarkers cells={snapshot.cells} />
    <CampfireModels cells={snapshot.cells} hasAlert={campfireAlert} onCampfireClick={onCampfireClick} />
    <AgentModels agents={snapshot.agents} selected={selected} onSelect={onSelect} />
    <AnimalModels animals={snapshot.animals} selected={selected} onSelect={onSelect} />
  </>;
}

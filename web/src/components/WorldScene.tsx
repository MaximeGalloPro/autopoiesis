import { Canvas } from "@react-three/fiber";
import {
  AdaptiveDpr,
  Float,
  Grid,
  Html,
  Instance,
  Instances,
  OrbitControls,
  PerspectiveCamera,
  Sparkles,
  Stars,
} from "@react-three/drei";
import { memo, useMemo } from "react";
import type { AnimalState, Position, Terrain, WorldCell, WorldSnapshot } from "../protocol";
import { WORLD_HEIGHT, WORLD_WIDTH } from "../protocol";
import { animalLabels } from "../lib/format";

export type EntitySelection = { kind: "agent" | "animal"; id: string };

const terrainColors: Record<Terrain, string> = {
  ground: "#567057",
  wall: "#77766d",
  water: "#297385",
  tree: "#385d3c",
  bush: "#466d43",
};

function worldPosition(position: Position, y = 0): [number, number, number] {
  return [position.x - WORLD_WIDTH / 2 + 0.5, y, position.y - WORLD_HEIGHT / 2 + 0.5];
}

interface TileInstancesProps {
  cells: WorldCell[];
  terrain: Terrain;
}

const TileInstances = memo(function TileInstances({ cells, terrain }: TileInstancesProps) {
  const selected = cells.filter((cell) => cell.terrain === terrain);
  const height = terrain === "wall" ? 0.75 : terrain === "water" ? 0.08 : 0.18;
  return (
    <Instances limit={Math.max(1, selected.length)} range={selected.length}>
      <boxGeometry args={[0.94, height, 0.94]} />
      <meshStandardMaterial
        color={terrainColors[terrain]}
        roughness={terrain === "water" ? 0.28 : 0.9}
        metalness={terrain === "water" ? 0.12 : 0}
        transparent={terrain === "water"}
        opacity={terrain === "water" ? 0.86 : 1}
      />
      {selected.map((cell) => (
        <Instance key={`${terrain}-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, height / 2)} />
      ))}
    </Instances>
  );
});

interface MarkerInstancesProps {
  cells: WorldCell[];
  field: "food" | "wood" | "fibers" | "branches" | "stored_food";
  color: string;
  y: number;
  shape?: "sphere" | "box" | "cone";
}

function MarkerInstances({ cells, field, color, y, shape = "sphere" }: MarkerInstancesProps) {
  const selected = cells.filter((cell) => cell[field] > 0);
  return (
    <Instances limit={Math.max(1, selected.length)} range={selected.length}>
      {shape === "box" ? <boxGeometry args={[0.24, 0.22, 0.24]} />
        : shape === "cone" ? <coneGeometry args={[0.14, 0.34, 5]} />
          : <sphereGeometry args={[0.14, 8, 6]} />}
      <meshStandardMaterial color={color} roughness={0.68} />
      {selected.map((cell) => (
        <Instance
          key={`${field}-${cell.position.x}-${cell.position.y}`}
          position={worldPosition(cell.position, y)}
          scale={Math.min(1.6, 0.76 + cell[field] * 0.08)}
        />
      ))}
    </Instances>
  );
}

function Nature({ cells }: { cells: WorldCell[] }) {
  const trees = cells.filter((cell) => cell.terrain === "tree");
  const bushes = cells.filter((cell) => cell.terrain === "bush");
  const shelters = cells.filter((cell) => cell.shelter_level > 0);
  return (
    <>
      <Instances limit={Math.max(1, trees.length)} range={trees.length}>
        <cylinderGeometry args={[0.1, 0.15, 0.65, 6]} />
        <meshStandardMaterial color="#614936" roughness={1} />
        {trees.map((cell) => <Instance key={`trunk-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 0.48)} />)}
      </Instances>
      <Instances limit={Math.max(1, trees.length)} range={trees.length}>
        <coneGeometry args={[0.38, 0.86, 7]} />
        <meshStandardMaterial color="#244d37" roughness={0.94} />
        {trees.map((cell) => <Instance key={`crown-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 1.1)} />)}
      </Instances>
      <Instances limit={Math.max(1, bushes.length)} range={bushes.length}>
        <dodecahedronGeometry args={[0.3, 0]} />
        <meshStandardMaterial color="#3f744a" roughness={1} />
        {bushes.map((cell) => <Instance key={`bush-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 0.42)} />)}
      </Instances>
      <Instances limit={Math.max(1, shelters.length)} range={shelters.length}>
        <coneGeometry args={[0.42, 0.72, 4]} />
        <meshStandardMaterial color="#b78b52" roughness={0.86} />
        {shelters.map((cell) => (
          <Instance
            key={`shelter-${cell.position.x}-${cell.position.y}`}
            position={worldPosition(cell.position, 0.55)}
            rotation={[0, Math.PI / 4, 0]}
            scale={0.8 + cell.shelter_level * 0.1}
          />
        ))}
      </Instances>
    </>
  );
}

function Campfires({ cells }: { cells: WorldCell[] }) {
  return cells.filter((cell) => cell.campfire).map((cell) => (
    <group key={`fire-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 0.24)}>
      <mesh rotation={[0, 0, Math.PI / 2]}>
        <cylinderGeometry args={[0.06, 0.06, 0.46, 6]} />
        <meshStandardMaterial color="#5a3827" />
      </mesh>
      <Float speed={3.5} rotationIntensity={0.1} floatIntensity={0.12}>
        <mesh position={[0, 0.25, 0]}>
          <coneGeometry args={[0.16, 0.42, 7]} />
          <meshBasicMaterial color="#ff9d45" toneMapped={false} />
        </mesh>
      </Float>
      <pointLight color="#ff8b42" intensity={1.5} distance={4} decay={2} />
    </group>
  ));
}

function AgentMeshes({
  snapshot,
  selected,
  onSelect,
}: {
  snapshot: WorldSnapshot;
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
}) {
  return snapshot.agents.filter((agent) => agent.alive).map((agent, index) => {
    const isSelected = selected?.kind === "agent" && selected.id === agent.id;
    return (
      <group
        key={agent.id}
        position={worldPosition(agent.position, 0.78)}
        onClick={(event) => { event.stopPropagation(); onSelect({ kind: "agent", id: agent.id }); }}
      >
        <Float speed={1.8} rotationIntensity={0.04} floatIntensity={0.1}>
          <mesh castShadow>
            <capsuleGeometry args={[0.18, 0.38, 5, 8]} />
            <meshStandardMaterial color={["#efbe62", "#d67d5e", "#91c7b1", "#a88bd4"][index % 4]} roughness={0.58} />
          </mesh>
          <mesh position={[0, 0.39, 0]}>
            <sphereGeometry args={[0.19, 12, 10]} />
            <meshStandardMaterial color="#e4b992" roughness={0.7} />
          </mesh>
        </Float>
        {isSelected && (
          <>
            <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.56, 0]}>
              <ringGeometry args={[0.37, 0.45, 32]} />
              <meshBasicMaterial color="#ffd66e" toneMapped={false} />
            </mesh>
            <Html center position={[0, 0.95, 0]} distanceFactor={14} className="world-label">
              {agent.name}
            </Html>
          </>
        )}
      </group>
    );
  });
}

function animalGeometry(animal: AnimalState) {
  if (animal.type === "fish") return <coneGeometry args={[0.16, 0.42, 6]} />;
  if (animal.type === "deer") return <capsuleGeometry args={[0.16, 0.34, 4, 6]} />;
  if (animal.type === "wolf") return <dodecahedronGeometry args={[0.25, 0]} />;
  if (animal.type === "boar") return <sphereGeometry args={[0.26, 8, 6]} />;
  return <sphereGeometry args={[0.18, 8, 6]} />;
}

function AnimalMeshes({
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
    return (
      <group
        key={animal.id}
        position={worldPosition(animal.position, animal.type === "fish" ? 0.2 : 0.48)}
        onClick={(event) => { event.stopPropagation(); onSelect({ kind: "animal", id: animal.id }); }}
      >
        <mesh castShadow rotation={animal.type === "fish" ? [0, 0, Math.PI / 2] : [0, 0, 0]}>
          {animalGeometry(animal)}
          <meshStandardMaterial color={animal.danger >= 60 ? "#a85a50" : "#9b8b6e"} roughness={0.86} />
        </mesh>
        {isSelected && (
          <>
            <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.25, 0]}>
              <ringGeometry args={[0.3, 0.38, 28]} />
              <meshBasicMaterial color="#ffca76" toneMapped={false} />
            </mesh>
            <Html center position={[0, 0.66, 0]} distanceFactor={14} className="world-label">
              {animalLabels[animal.type] ?? animal.type}
            </Html>
          </>
        )}
      </group>
    );
  });
}

function EmptyWorld() {
  return (
    <>
      <Grid args={[40, 24]} cellSize={1} cellThickness={0.5} cellColor="#345047" sectionSize={4} sectionColor="#527667" fadeDistance={45} />
      <Sparkles count={45} scale={[36, 3, 20]} size={1.4} speed={0.25} color="#8dbda8" />
    </>
  );
}

export function WorldScene({
  snapshot,
  selected,
  onSelect,
}: {
  snapshot: WorldSnapshot | null;
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
}) {
  const cells = snapshot?.cells ?? [];
  const isNight = snapshot?.phase === "night";
  const cameraPosition = useMemo<[number, number, number]>(() => [0, 27, 25], []);
  return (
    <div className="world-canvas" role="img" aria-label="Vue tridimensionnelle du monde torique 40 par 24">
      <Canvas shadows="basic" dpr={[1, 1.75]} gl={{ antialias: true, alpha: false }}>
        <color attach="background" args={[isNight ? "#070d15" : "#b5c7b0"]} />
        <fog attach="fog" args={[isNight ? "#070d15" : "#9db5a0", 24, 55]} />
        <PerspectiveCamera makeDefault position={cameraPosition} fov={42} near={0.1} far={100} />
        <ambientLight intensity={isNight ? 0.3 : 1.1} color={isNight ? "#7082aa" : "#fff1d1"} />
        <directionalLight
          castShadow
          position={[-12, 24, 8]}
          intensity={isNight ? 0.35 : 2.1}
          color={isNight ? "#6e8ac0" : "#ffe1a8"}
          shadow-mapSize-width={1024}
          shadow-mapSize-height={1024}
        />
        {isNight && <Stars radius={45} depth={18} count={850} factor={2.5} saturation={0.2} fade speed={0.3} />}
        {snapshot ? (
          <>
            {(["ground", "wall", "water", "tree", "bush"] as Terrain[]).map((terrain) => (
              <TileInstances key={terrain} cells={cells} terrain={terrain} />
            ))}
            <Nature cells={cells} />
            <MarkerInstances cells={cells} field="food" color="#d25752" y={0.34} />
            <MarkerInstances cells={cells} field="wood" color="#704c32" y={0.36} shape="box" />
            <MarkerInstances cells={cells} field="fibers" color="#d8c47b" y={0.38} shape="cone" />
            <MarkerInstances cells={cells} field="branches" color="#a4764d" y={0.38} shape="box" />
            <MarkerInstances cells={cells} field="stored_food" color="#f0bf55" y={0.58} />
            <Campfires cells={cells} />
            <AgentMeshes snapshot={snapshot} selected={selected} onSelect={onSelect} />
            <AnimalMeshes animals={snapshot.animals} selected={selected} onSelect={onSelect} />
            <Grid
              args={[40, 24]}
              position={[0, 0.105, 0]}
              cellSize={1}
              cellThickness={0.25}
              cellColor="#1c352d"
              sectionSize={8}
              sectionThickness={0.7}
              sectionColor="#1a473a"
              fadeDistance={60}
              infiniteGrid={false}
            />
          </>
        ) : <EmptyWorld />}
        <OrbitControls
          makeDefault
          target={[0, 0, 0]}
          minDistance={12}
          maxDistance={48}
          maxPolarAngle={Math.PI / 2.15}
          enableDamping
          dampingFactor={0.08}
        />
        <AdaptiveDpr pixelated />
      </Canvas>
      <div className="torus-hint" aria-hidden="true">
        <span>↔ continuité torique ↔</span><span>40 × 24</span>
      </div>
    </div>
  );
}

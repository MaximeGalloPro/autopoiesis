import { Canvas } from "@react-three/fiber";
import {
  AdaptiveDpr,
  Float,
  Grid,
  Html,
  Instance,
  Instances,
  OrbitControls,
  OrthographicCamera,
  Sparkles,
  Stars,
} from "@react-three/drei";
import { memo, useMemo } from "react";
import type { AnimalState, Position, Terrain, WorldCell, WorldSnapshot } from "../protocol";
import { WORLD_HEIGHT, WORLD_WIDTH } from "../protocol";
import { animalLabels } from "../lib/format";

export type EntitySelection = { kind: "agent" | "animal"; id: string };

const terrainColors: Record<Terrain, readonly string[]> = {
  ground: ["#7fbd5a", "#75b253", "#8ac563"],
  wall: ["#9aa3a1", "#879592", "#a6afaa"],
  water: ["#3b9ed0", "#3293c8", "#49abd9"],
  tree: ["#418a54", "#367c4c", "#4b975b"],
  bush: ["#71af4e", "#659f46", "#7db85a"],
};

const nightTerrainColors: Record<Terrain, readonly string[]> = {
  ground: ["#5d9668", "#548c61", "#68a173"],
  wall: ["#718996", "#66808e", "#7c95a1"],
  water: ["#2f79b8", "#286fae", "#3988c4"],
  tree: ["#397a5e", "#306f54", "#448666"],
  bush: ["#5a9b59", "#508e50", "#65a765"],
};

function worldPosition(position: Position, y = 0): [number, number, number] {
  return [position.x - WORLD_WIDTH / 2 + 0.5, y, position.y - WORLD_HEIGHT / 2 + 0.5];
}

function tileColor(terrain: Terrain, position: Position, isNight: boolean) {
  const colors = (isNight ? nightTerrainColors : terrainColors)[terrain];
  return colors[Math.abs(position.x * 17 + position.y * 31) % colors.length] ?? colors[0];
}

interface TileInstancesProps {
  cells: WorldCell[];
  terrain: Terrain;
  isNight: boolean;
}

const TileInstances = memo(function TileInstances({ cells, terrain, isNight }: TileInstancesProps) {
  const selected = cells.filter((cell) => cell.terrain === terrain);
  const height = terrain === "wall" ? 0.58 : terrain === "water" ? 0.055 : terrain === "tree" ? 0.16 : 0.12;
  return (
    <Instances limit={Math.max(1, selected.length)} range={selected.length}>
      <boxGeometry args={[0.978, height, 0.978]} />
      <meshStandardMaterial
        color="#ffffff"
        roughness={terrain === "water" ? 0.28 : 0.9}
        metalness={terrain === "water" ? 0.12 : 0}
        emissive={terrain === "water" ? (isNight ? "#0c4f91" : "#1374af") : "#000000"}
        emissiveIntensity={terrain === "water" ? (isNight ? 0.42 : 0.15) : 0}
        transparent={terrain === "water"}
        opacity={terrain === "water" ? 0.94 : 1}
      />
      {selected.map((cell) => (
        <Instance
          key={`${terrain}-${cell.position.x}-${cell.position.y}`}
          color={tileColor(terrain, cell.position, isNight)}
          position={worldPosition(cell.position, height / 2)}
        />
      ))}
    </Instances>
  );
});

interface MarkerInstancesProps {
  cells: WorldCell[];
  field: "food" | "wood" | "fibers" | "branches" | "stored_food";
  color: string;
  y: number;
  shape?: "berry" | "crate" | "sprout" | "branch" | "stock";
}

function MarkerInstances({ cells, field, color, y, shape = "berry" }: MarkerInstancesProps) {
  const selected = cells.filter((cell) => cell[field] > 0);
  return (
    <Instances limit={Math.max(1, selected.length)} range={selected.length}>
      {shape === "crate" ? <boxGeometry args={[0.26, 0.2, 0.26]} />
        : shape === "sprout" ? <coneGeometry args={[0.15, 0.36, 5]} />
          : shape === "branch" ? <boxGeometry args={[0.35, 0.065, 0.1]} />
            : shape === "stock" ? <dodecahedronGeometry args={[0.17, 0]} />
              : <sphereGeometry args={[0.15, 8, 6]} />}
      <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.18} roughness={0.64} />
      {selected.map((cell) => (
        <Instance
          key={`${field}-${cell.position.x}-${cell.position.y}`}
          position={worldPosition(cell.position, y)}
          rotation={shape === "branch" ? [0, (cell.position.x * 0.8 + cell.position.y * 0.45) % Math.PI, 0] : undefined}
          scale={Math.min(1.55, 0.74 + Math.sqrt(cell[field]) * 0.16)}
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
        <cylinderGeometry args={[0.085, 0.13, 0.52, 6]} />
        <meshStandardMaterial color="#80563b" roughness={1} />
        {trees.map((cell) => <Instance key={`trunk-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 0.42)} />)}
      </Instances>
      <Instances limit={Math.max(1, trees.length)} range={trees.length}>
        <dodecahedronGeometry args={[0.43, 0]} />
        <meshStandardMaterial color="#246e43" roughness={0.94} />
        {trees.map((cell) => <Instance key={`crown-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 0.86)} />)}
      </Instances>
      <Instances limit={Math.max(1, bushes.length)} range={bushes.length}>
        <dodecahedronGeometry args={[0.32, 0]} />
        <meshStandardMaterial color="#4e9347" roughness={1} />
        {bushes.map((cell) => <Instance key={`bush-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 0.38)} />)}
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

function Campfires({ cells, isNight }: { cells: WorldCell[]; isNight: boolean }) {
  return cells.filter((cell) => cell.campfire).map((cell) => (
    <group key={`fire-${cell.position.x}-${cell.position.y}`} position={worldPosition(cell.position, 0.24)}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.17, 0]}>
        <circleGeometry args={[isNight ? 0.62 : 0.44, 24]} />
        <meshBasicMaterial color="#ffae54" transparent opacity={isNight ? 0.22 : 0.12} depthWrite={false} toneMapped={false} />
      </mesh>
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
      <pointLight color="#ffb45d" intensity={isNight ? 2.25 : 1.1} distance={isNight ? 4.8 : 3.2} decay={2} />
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
            <Html center position={[0, 0.95, 0]} distanceFactor={1} className="world-label">
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
              <Html center position={[0, 0.66, 0]} distanceFactor={1} className="world-label">
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
      <Grid args={[40, 24]} cellSize={1} cellThickness={0.15} cellColor="#47786e" sectionSize={8} sectionColor="#8cbf88" fadeDistance={45} />
      <Sparkles count={45} scale={[36, 3, 20]} size={1.4} speed={0.25} color="#b8dc8a" />
    </>
  );
}

export function WorldScene({
  snapshot,
  awaitingDawn = false,
  selected,
  onSelect,
}: {
  snapshot: WorldSnapshot | null;
  awaitingDawn?: boolean;
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
}) {
  const cells = snapshot?.cells ?? [];
  const isNight = snapshot?.phase === "night" && !awaitingDawn;
  const cameraPosition = useMemo<[number, number, number]>(() => [0, 40, 0], []);
  return (
    <div className="world-canvas" role="img" aria-label="Vue du dessus du monde torique 40 par 24">
      <Canvas shadows="basic" dpr={[1, 1.75]} gl={{ antialias: true, alpha: false }}>
        <color attach="background" args={[isNight ? "#1c3853" : "#bde2b0"]} />
        <fog attach="fog" args={[isNight ? "#274d71" : "#a9d09d", 36, 80]} />
        <OrthographicCamera
          makeDefault
          position={cameraPosition}
          left={-20.6}
          right={20.6}
          top={12.45}
          bottom={-12.45}
          near={0.1}
          far={100}
        />
        <ambientLight intensity={isNight ? 1.2 : 1.18} color={isNight ? "#bbd5f1" : "#fff4cb"} />
        <hemisphereLight args={[isNight ? "#b5d5f7" : "#e2f3bf", isNight ? "#1a4b54" : "#5f8743", isNight ? 0.52 : 0.32]} />
        <directionalLight
          castShadow
          position={[-12, 32, 8]}
          intensity={isNight ? 1.05 : 2.25}
          color={isNight ? "#b6d2fb" : "#ffe0a5"}
          shadow-mapSize-width={1024}
          shadow-mapSize-height={1024}
        />
        {isNight && <Stars radius={45} depth={18} count={850} factor={2.5} saturation={0.2} fade speed={0.3} />}
        {snapshot ? (
          <>
            {(["ground", "wall", "water", "tree", "bush"] as Terrain[]).map((terrain) => (
              <TileInstances key={terrain} cells={cells} terrain={terrain} isNight={isNight} />
            ))}
            <Nature cells={cells} />
            <MarkerInstances cells={cells} field="food" color="#f35f62" y={0.32} shape="berry" />
            <MarkerInstances cells={cells} field="wood" color="#a86d3d" y={0.3} shape="crate" />
            <MarkerInstances cells={cells} field="fibers" color="#f0d466" y={0.34} shape="sprout" />
            <MarkerInstances cells={cells} field="branches" color="#e6a45a" y={0.3} shape="branch" />
            <MarkerInstances cells={cells} field="stored_food" color="#ffd45c" y={0.52} shape="stock" />
            <Campfires cells={cells} isNight={isNight} />
            <AgentMeshes snapshot={snapshot} selected={selected} onSelect={onSelect} />
            <AnimalMeshes animals={snapshot.animals} selected={selected} onSelect={onSelect} />
            <Grid
              args={[40, 24]}
              position={[0, 0.205, 0]}
              cellSize={1}
              cellThickness={0.13}
              cellColor={isNight ? "#75a8d1" : "#416a57"}
              sectionSize={8}
              sectionThickness={0.36}
              sectionColor={isNight ? "#91c5e8" : "#79a96f"}
              fadeDistance={60}
              infiniteGrid={false}
            />
          </>
        ) : <EmptyWorld />}
        <OrbitControls
          makeDefault
          target={[0, 0, 0]}
          minZoom={0.88}
          maxZoom={2.25}
          minPolarAngle={0}
          maxPolarAngle={0}
          enableRotate={false}
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

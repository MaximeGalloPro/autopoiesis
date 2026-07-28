import { Canvas } from "@react-three/fiber";
import {
  AdaptiveDpr,
  Grid,
  Instance,
  Instances,
  OrbitControls,
  OrthographicCamera,
  Sparkles,
  Stars,
} from "@react-three/drei";
import { memo, useMemo } from "react";
import type { Position, Terrain, WorldCell, WorldSnapshot } from "../protocol";
import { WorldEntities, worldPosition, type EntitySelection } from "./entities/WorldEntities";

export type { EntitySelection } from "./entities/WorldEntities";

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

function Nature({ cells }: { cells: WorldCell[] }) {
  const trees = cells.filter((cell) => cell.terrain === "tree");
  const bushes = cells.filter((cell) => cell.terrain === "bush");
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
    </>
  );
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
            <WorldEntities snapshot={snapshot} selected={selected} onSelect={onSelect} />
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

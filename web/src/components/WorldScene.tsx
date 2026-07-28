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

const terrainColors: Record<Terrain, string> = {
  ground: "#567057",
  wall: "#77766d",
  water: "#297385",
  tree: "#385d3c",
  bush: "#466d43",
};

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

function Nature({ cells }: { cells: WorldCell[] }) {
  const trees = cells.filter((cell) => cell.terrain === "tree");
  const bushes = cells.filter((cell) => cell.terrain === "bush");
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
    </>
  );
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
        <color attach="background" args={[isNight ? "#24384b" : "#b5c7b0"]} />
        <fog attach="fog" args={[isNight ? "#304a60" : "#9db5a0", 36, 80]} />
        <OrthographicCamera
          makeDefault
          position={cameraPosition}
          left={-21}
          right={21}
          top={13}
          bottom={-13}
          near={0.1}
          far={100}
        />
        <ambientLight intensity={isNight ? 1.12 : 1.1} color={isNight ? "#b8cbe5" : "#fff1d1"} />
        <directionalLight
          castShadow
          position={[-12, 32, 8]}
          intensity={isNight ? 0.95 : 2.1}
          color={isNight ? "#a5bce4" : "#ffe1a8"}
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
            <WorldEntities snapshot={snapshot} selected={selected} onSelect={onSelect} />
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
          minZoom={0.72}
          maxZoom={1.8}
          minPolarAngle={0}
          maxPolarAngle={0}
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

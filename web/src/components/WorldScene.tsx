import { Canvas } from "@react-three/fiber";
import {
  AdaptiveDpr,
  Instance,
  Instances,
  OrbitControls,
  OrthographicCamera,
  Sparkles,
  Stars,
} from "@react-three/drei";
import { memo, useMemo } from "react";
import { MOUSE } from "three";
import type { WorldCell, WorldSnapshot } from "../protocol";
import { WorldEntities, worldPosition, type EntitySelection } from "./entities/WorldEntities";

export type { EntitySelection } from "./entities/WorldEntities";

interface TerrainFeaturesProps {
  cells: WorldCell[];
  isNight: boolean;
}

const TerrainFeatures = memo(function TerrainFeatures({ cells, isNight }: TerrainFeaturesProps) {
  const water = cells.filter((cell) => cell.terrain === "water");
  const walls = cells.filter((cell) => cell.terrain === "wall");
  return (
    <>
      <Instances limit={Math.max(1, water.length)} range={water.length}>
        <boxGeometry args={[1, 0.08, 1]} />
        <meshStandardMaterial
          color={isNight ? "#2e79b6" : "#389dcb"}
          roughness={0.3}
          metalness={0.12}
          emissive={isNight ? "#0c4f91" : "#1374af"}
          emissiveIntensity={isNight ? 0.42 : 0.15}
        />
        {water.map((cell) => (
          <Instance
            key={`water-${cell.position.x}-${cell.position.y}`}
            position={worldPosition(cell.position, 0.02)}
          />
        ))}
      </Instances>
      <Instances limit={Math.max(1, walls.length)} range={walls.length}>
        <boxGeometry args={[1, 0.58, 1]} />
        <meshStandardMaterial color={isNight ? "#718996" : "#929c99"} roughness={0.9} />
        {walls.map((cell) => (
          <Instance
            key={`wall-${cell.position.x}-${cell.position.y}`}
            position={worldPosition(cell.position, 0.29)}
          />
        ))}
      </Instances>
    </>
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

function WorldFloor({ isNight }: { isNight: boolean }) {
  return (
    <mesh receiveShadow rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.03, 0]}>
      <planeGeometry args={[40, 24]} />
      <meshStandardMaterial color={isNight ? "#5a8d66" : "#7fbd5a"} roughness={0.95} />
    </mesh>
  );
}

function EmptyWorld({ isNight }: { isNight: boolean }) {
  return (
    <>
      <WorldFloor isNight={isNight} />
      <Sparkles count={45} scale={[40, 3, 24]} size={1.4} speed={0.25} color={isNight ? "#a8cee7" : "#b8dc8a"} />
    </>
  );
}

export function WorldScene({
  snapshot,
  awaitingDawn = false,
  selected,
  onSelect,
  campfireAlert = false,
  onCampfireClick,
}: {
  snapshot: WorldSnapshot | null;
  awaitingDawn?: boolean;
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
  campfireAlert?: boolean;
  onCampfireClick?: () => void;
}) {
  const cells = snapshot?.cells ?? [];
  const isNight = snapshot?.phase === "night" && !awaitingDawn;
  const cameraPosition = useMemo<[number, number, number]>(() => [0, 40, 0], []);
  return (
    <div className="world-canvas" role="img" aria-label="Vue du dessus du monde">
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
            <WorldFloor isNight={isNight} />
            <TerrainFeatures cells={cells} isNight={isNight} />
            <Nature cells={cells} />
            <WorldEntities
              snapshot={snapshot}
              selected={selected}
              onSelect={onSelect}
              campfireAlert={campfireAlert}
              onCampfireClick={onCampfireClick}
            />
          </>
        ) : <EmptyWorld isNight={isNight} />}
        <OrbitControls
          makeDefault
          target={[0, 0, 0]}
          minZoom={0.88}
          maxZoom={2.25}
          minPolarAngle={0}
          maxPolarAngle={0}
          enableRotate={false}
          enablePan
          screenSpacePanning
          panSpeed={0.9}
          mouseButtons={{ LEFT: MOUSE.PAN, RIGHT: MOUSE.PAN }}
          enableDamping
          dampingFactor={0.08}
        />
        <AdaptiveDpr pixelated />
      </Canvas>
    </div>
  );
}

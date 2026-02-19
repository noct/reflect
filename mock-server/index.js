import express from 'express';
import cors from 'cors';

const app = express();
const PORT = 7700;

app.use(cors());
app.use(express.json());

// ---------------------------------------------------------------------------
// Deterministic pseudo-random from a seed (for stable per-entity properties)
// ---------------------------------------------------------------------------
function mulberry32(seed) {
  return function () {
    seed |= 0;
    seed = (seed + 0x6d2b79f5) | 0;
    let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// ---------------------------------------------------------------------------
// Scene data generator
// ---------------------------------------------------------------------------
const ENTITY_TYPES = [
  'Transform', 'MeshRenderer', 'Camera', 'Light', 'Collider',
  'AudioSource', 'RigidBody', 'Sprite', 'ParticleSystem', 'Script',
  'Animator', 'Canvas', 'Text', 'Image', 'Button', 'Zone',
];

const NAMES = [
  'Player', 'PlayerModel', 'Shadow', 'MainCamera', 'DirectionalLight',
  'PointLight', 'Ground', 'Sky', 'HUD', 'HealthBar', 'ScoreText',
  'Enemy_01', 'Enemy_02', 'Enemy_03', 'Projectile', 'Pickup_Health',
  'Pickup_Ammo', 'Tree_01', 'Tree_02', 'Rock_01', 'Fence',
  'Trigger_Zone', 'Spawn_Point', 'Checkpoint', 'BGM_Player',
  'SFX_Emitter', 'ParticleFX', 'UI_Root', 'Button_Start', 'Button_Quit',
];

let nextPtr = 0xBEEF0000;
function makeId() {
  const id = nextPtr.toString(10);
  nextPtr += 0x80; // simulate pointer spacing
  return id;
}

// Build a flat lookup + tree
const entityMap = new Map(); // id -> entity data (with properties)

function generateEntity(depth, rng, namePool) {
  const id = makeId();
  const type = ENTITY_TYPES[Math.floor(rng() * ENTITY_TYPES.length)];
  const hasName = rng() > 0.4 && namePool.length > 0;
  const name = hasName ? namePool.splice(Math.floor(rng() * namePool.length), 1)[0] : null;

  const childCount = depth < 3 ? Math.floor(rng() * 4) : 0;
  const children = [];
  for (let i = 0; i < childCount; i++) {
    children.push(generateEntity(depth + 1, rng, namePool));
  }

  const entity = { id, type, name, children: children.map(c => c.id) };
  entityMap.set(id, entity);

  // Generate properties based on type
  const props = generateProperties(type, id);
  entityMap.get(id).properties = props;

  return { id, type, name, children };
}

function generateProperties(type, id) {
  const rng = mulberry32(parseInt(id) || 0);
  const props = [];

  // Common: enabled flag
  props.push({ name: 'enabled', type: 'int', value: rng() > 0.1 ? 1 : 0 });

  switch (type) {
    case 'Transform':
      props.push({ name: 'position.x', type: 'float', value: round(rng() * 100 - 50) });
      props.push({ name: 'position.y', type: 'float', value: round(rng() * 50) });
      props.push({ name: 'position.z', type: 'float', value: round(rng() * 100 - 50) });
      props.push({ name: 'rotation', type: 'float', value: round(rng() * 360) });
      props.push({ name: 'scale', type: 'float', value: round(0.5 + rng() * 2) });
      break;
    case 'MeshRenderer':
      props.push({ name: 'mesh', type: 'string', value: pick(rng, ['cube', 'sphere', 'capsule', 'quad', 'custom']) });
      props.push({ name: 'material', type: 'string', value: pick(rng, ['default', 'metal', 'wood', 'glass', 'emissive']) });
      props.push({ name: 'color', type: 'color', value: randomColor(rng) });
      props.push({ name: 'castShadows', type: 'int', value: rng() > 0.3 ? 1 : 0 });
      break;
    case 'Camera':
      props.push({ name: 'fov', type: 'float', value: round(60 + rng() * 30) });
      props.push({ name: 'near', type: 'float', value: 0.1 });
      props.push({ name: 'far', type: 'float', value: round(500 + rng() * 500) });
      props.push({ name: 'clearColor', type: 'color', value: '#1a1a2e' });
      break;
    case 'Light':
      props.push({ name: 'lightType', type: 'string', value: pick(rng, ['directional', 'point', 'spot']) });
      props.push({ name: 'intensity', type: 'float', value: round(0.5 + rng() * 2) });
      props.push({ name: 'color', type: 'color', value: randomColor(rng) });
      props.push({ name: 'range', type: 'float', value: round(5 + rng() * 20) });
      break;
    case 'Collider':
      props.push({ name: 'shape', type: 'string', value: pick(rng, ['box', 'sphere', 'capsule', 'mesh']) });
      props.push({ name: 'isTrigger', type: 'int', value: rng() > 0.7 ? 1 : 0 });
      props.push({ name: 'size.x', type: 'float', value: round(0.5 + rng() * 3) });
      props.push({ name: 'size.y', type: 'float', value: round(0.5 + rng() * 3) });
      break;
    case 'RigidBody':
      props.push({ name: 'mass', type: 'float', value: round(0.1 + rng() * 10) });
      props.push({ name: 'drag', type: 'float', value: round(rng() * 2) });
      props.push({ name: 'useGravity', type: 'int', value: rng() > 0.2 ? 1 : 0 });
      props.push({ name: 'isKinematic', type: 'int', value: rng() > 0.7 ? 1 : 0 });
      break;
    case 'AudioSource':
      props.push({ name: 'clip', type: 'string', value: pick(rng, ['bgm_main', 'sfx_hit', 'sfx_jump', 'ambient_wind', 'ui_click']) });
      props.push({ name: 'volume', type: 'float', value: round(rng()) });
      props.push({ name: 'loop', type: 'int', value: rng() > 0.5 ? 1 : 0 });
      props.push({ name: 'spatial', type: 'int', value: rng() > 0.4 ? 1 : 0 });
      break;
    case 'Sprite':
      props.push({ name: 'texture', type: 'string', value: pick(rng, ['player_idle', 'enemy_walk', 'tile_grass', 'icon_heart', 'icon_star']) });
      props.push({ name: 'tint', type: 'color', value: randomColor(rng) });
      props.push({ name: 'flipX', type: 'int', value: rng() > 0.7 ? 1 : 0 });
      props.push({ name: 'sortOrder', type: 'int', value: Math.floor(rng() * 10) });
      break;
    case 'ParticleSystem':
      props.push({ name: 'maxParticles', type: 'int', value: Math.floor(50 + rng() * 200) });
      props.push({ name: 'emissionRate', type: 'float', value: round(5 + rng() * 50) });
      props.push({ name: 'startColor', type: 'color', value: randomColor(rng) });
      props.push({ name: 'lifetime', type: 'float', value: round(0.5 + rng() * 3) });
      break;
    case 'Zone': {
      const zoneType = pick(rng, ['trigger', 'patrol', 'spawn', 'nav_hint']);
      props.push({ name: 'zoneType', type: 'string', value: zoneType });
      props.push({ name: 'color', type: 'color', value: randomColor(rng) });
      if (zoneType === 'patrol') {
        props.push({ name: 'path', type: 'points2d', value: randomPath(rng, rng() * 100 - 50, rng() * 100 - 50, 4 + Math.floor(rng() * 4)) });
        props.push({ name: 'loop', type: 'int', value: rng() > 0.3 ? 1 : 0 });
      } else {
        props.push({ name: 'boundary', type: 'points2d', value: randomPolygon(rng, rng() * 100 - 50, rng() * 100 - 50, 5 + rng() * 20, 4 + Math.floor(rng() * 5)) });
      }
      break;
    }
    case 'Text':
      props.push({ name: 'text', type: 'string', value: pick(rng, ['Score: 0', 'Health', 'Game Over', 'Press Start', 'Loading...']) });
      props.push({ name: 'fontSize', type: 'int', value: Math.floor(12 + rng() * 24) });
      props.push({ name: 'color', type: 'color', value: '#f4f4f5' });
      break;
    default:
      props.push({ name: 'tag', type: 'string', value: pick(rng, ['default', 'interactive', 'static', 'dynamic']) });
      break;
  }

  return props;
}

function randomPolygon(rng, cx, cy, radius, sides) {
  const pts = [];
  for (let i = 0; i < sides; i++) {
    const angle = (i / sides) * Math.PI * 2;
    const r = radius * (0.6 + rng() * 0.4);
    pts.push([round(cx + Math.cos(angle) * r), round(cy + Math.sin(angle) * r)]);
  }
  return pts;
}

function randomPath(rng, startX, startY, segments) {
  const pts = [[round(startX), round(startY)]];
  let x = startX, y = startY;
  for (let i = 0; i < segments; i++) {
    x += (rng() - 0.3) * 40;
    y += (rng() - 0.3) * 40;
    pts.push([round(x), round(y)]);
  }
  return pts;
}

function round(v) { return Math.round(v * 100) / 100; }
function pick(rng, arr) { return arr[Math.floor(rng() * arr.length)]; }
function randomColor(rng) {
  const r = Math.floor(rng() * 256).toString(16).padStart(2, '0');
  const g = Math.floor(rng() * 256).toString(16).padStart(2, '0');
  const b = Math.floor(rng() * 256).toString(16).padStart(2, '0');
  return `#${r}${g}${b}`;
}

// Generate the scene tree
const rng = mulberry32(42);
const namePool = [...NAMES];
const sceneRoots = [];

// Create a "Root" container with two main branches: World and UI
const rootChildren = [];

// World branch
const worldChildren = [];
for (let i = 0; i < 8; i++) {
  worldChildren.push(generateEntity(2, rng, namePool));
}

// Add explicit zone entities for testing points2d
function addZoneEntity(name, zoneType, points) {
  const id = makeId();
  const entity = {
    id, type: 'Zone', name, children: [],
    properties: [
      { name: 'enabled', type: 'int', value: 1 },
      { name: 'zoneType', type: 'string', value: zoneType },
      { name: 'color', type: 'color', value: zoneType === 'patrol' ? '#4F95FF' : '#47B576' },
      { name: zoneType === 'patrol' ? 'path' : 'boundary', type: 'points2d', value: points },
    ],
  };
  entityMap.set(id, entity);
  return { id, type: 'Zone', name, children: [] };
}
worldChildren.push(addZoneEntity('Spawn_Zone_A', 'spawn', [
  [0, 0], [30, 0], [30, 20], [0, 20],
]));
worldChildren.push(addZoneEntity('Combat_Area', 'trigger', [
  [-10, -10], [50, -5], [60, 40], [30, 55], [-15, 35],
]));
worldChildren.push(addZoneEntity('Guard_Patrol', 'patrol', [
  [0, 0], [20, 5], [40, -10], [55, 15], [35, 30], [10, 25],
]));
const worldId = makeId();
const worldNode = { id: worldId, type: 'Transform', name: 'World', children: worldChildren };
entityMap.set(worldId, {
  id: worldId, type: 'Transform', name: 'World',
  children: worldChildren.map(c => c.id),
  properties: generateProperties('Transform', worldId),
});

// UI branch
const uiChildren = [];
for (let i = 0; i < 4; i++) {
  uiChildren.push(generateEntity(2, rng, namePool));
}
const uiId = makeId();
const uiNode = { id: uiId, type: 'Canvas', name: 'UI', children: uiChildren };
entityMap.set(uiId, {
  id: uiId, type: 'Canvas', name: 'UI',
  children: uiChildren.map(c => c.id),
  properties: [
    { name: 'enabled', type: 'int', value: 1 },
    { name: 'renderMode', type: 'string', value: 'screenSpace' },
    { name: 'sortOrder', type: 'int', value: 0 },
  ],
});

// Root
const rootId = makeId();
const rootNode = { id: rootId, type: 'Transform', name: 'Root', children: [worldNode, uiNode] };
entityMap.set(rootId, {
  id: rootId, type: 'Transform', name: 'Root',
  children: [worldId, uiId],
  properties: generateProperties('Transform', rootId),
});
sceneRoots.push(rootNode);

// Build tree response (recursive, only id/type/name/children)
function toTreeNode(node) {
  return {
    id: node.id,
    type: node.type,
    name: node.name,
    children: (node.children || []).map(c => typeof c === 'string' ? null : toTreeNode(c)).filter(Boolean),
  };
}

const sceneTree = { entities: sceneRoots.map(toTreeNode) };

// ---------------------------------------------------------------------------
// Perf simulation
// ---------------------------------------------------------------------------
const BASE_FRAME_TIME = 16.0; // ~60fps
let frameCounter = 0;

function currentPerf() {
  frameCounter++;
  // Simulate slight jitter + occasional spikes
  const jitter = (Math.sin(frameCounter * 0.1) * 1.5) + (Math.random() - 0.5) * 2;
  const spike = Math.random() > 0.95 ? Math.random() * 8 : 0;
  const frameTimeMs = Math.max(1, BASE_FRAME_TIME + jitter + spike);
  return {
    fps: Math.round((1000 / frameTimeMs) * 10) / 10,
    frameTimeMs: Math.round(frameTimeMs * 100) / 100,
    entityCount: entityMap.size,
  };
}

// ---------------------------------------------------------------------------
// Routes
// ---------------------------------------------------------------------------
app.get('/api/perf', (_req, res) => {
  res.json(currentPerf());
});

app.get('/api/scene', (_req, res) => {
  res.json(sceneTree);
});

app.get('/api/entity/:id', (req, res) => {
  const entity = entityMap.get(req.params.id);
  if (!entity) {
    return res.status(404).json({ error: 'Entity not found' });
  }
  res.json({ properties: entity.properties });
});

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------
app.listen(PORT, () => {
  console.log(`[reflector] Mock server running on http://localhost:${PORT}`);
  console.log(`[reflector] ${entityMap.size} entities generated`);
});

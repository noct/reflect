<script setup>
import { ref, watch, onMounted, onUnmounted } from 'vue';

const props = defineProps({
  zones: Array,       // profile.zones array
  windowSize: { type: Number, default: 1 },
});

const canvas = ref(null);
let animFrame = null;

const DISPLAY_LEN = 200;
const TARGET_MS = 16.67;

// Stable color assignment
const ZONE_COLORS = [
  '#4f95ff', '#3a72cc', '#6aadff', '#47b576', '#3a9460',
  '#5fd498', '#c0a030', '#a08828', '#b05fb0', '#b5894b',
  '#e06070', '#50b8b0', '#8870d0', '#d09050', '#60a0e0',
];
const colorMap = new Map();
function getColor(name) {
  if (!colorMap.has(name)) {
    colorMap.set(name, ZONE_COLORS[colorMap.size % ZONE_COLORS.length]);
  }
  return colorMap.get(name);
}

function windowHistory(raw, winSize) {
  const out = [];
  for (let end = raw.length; end > 0 && out.length < DISPLAY_LEN; end -= winSize) {
    const start = Math.max(0, end - winSize);
    const bucket = raw.slice(start, end);
    out.unshift(bucket.reduce((a, b) => a + b, 0) / bucket.length);
  }
  while (out.length < DISPLAY_LEN) out.unshift(out[0] || 0);
  return out;
}

function draw() {
  const el = canvas.value;
  if (!el || !props.zones?.length) return;

  const ctx = el.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  const rect = el.getBoundingClientRect();
  const w = rect.width;
  const h = rect.height;

  el.width = w * dpr;
  el.height = h * dpr;
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, w, h);

  // Window all zones, sort by base cost descending for stable stacking
  const windowed = props.zones.map(z => ({
    name: z.name,
    color: getColor(z.name),
    wh: windowHistory(z.history, props.windowSize),
    base: z.ema,
  })).sort((a, b) => b.base - a.base);

  // Max total
  let maxTotal = 0;
  for (let i = 0; i < DISPLAY_LEN; i++) {
    let sum = 0;
    for (const z of windowed) sum += z.wh[i];
    maxTotal = Math.max(maxTotal, sum);
  }
  maxTotal = Math.max(25, maxTotal);

  // Draw stacked areas bottom-up
  const bottoms = new Array(DISPLAY_LEN).fill(0);
  const xStep = w / (DISPLAY_LEN - 1);

  for (const z of windowed) {
    ctx.fillStyle = z.color + '55';
    ctx.strokeStyle = z.color + 'aa';
    ctx.lineWidth = 0.5;

    ctx.beginPath();
    for (let i = 0; i < DISPLAY_LEN; i++) {
      const x = i * xStep;
      const topVal = bottoms[i] + z.wh[i];
      const y = h - (topVal / maxTotal) * (h - 4);
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    for (let i = DISPLAY_LEN - 1; i >= 0; i--) {
      const x = i * xStep;
      const y = h - (bottoms[i] / maxTotal) * (h - 4);
      ctx.lineTo(x, y);
    }
    ctx.closePath();
    ctx.fill();
    ctx.stroke();

    for (let i = 0; i < DISPLAY_LEN; i++) {
      bottoms[i] += z.wh[i];
    }
  }

  // 60fps target
  const targetY = h - (TARGET_MS / maxTotal) * (h - 4);
  ctx.strokeStyle = '#666';
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 4]);
  ctx.beginPath();
  ctx.moveTo(0, targetY);
  ctx.lineTo(w, targetY);
  ctx.stroke();
  ctx.setLineDash([]);

  // Legend
  ctx.font = '10px "Roboto Mono", monospace';
  const legendX = 8;
  const legendStartY = 12;
  for (let i = 0; i < windowed.length; i++) {
    const z = windowed[i];
    const col = Math.floor(i / 5);
    const row = i % 5;
    const x = legendX + col * 160;
    const y = legendStartY + row * 13;
    ctx.fillStyle = z.color;
    ctx.fillRect(x, y - 6, 8, 8);
    ctx.fillStyle = '#888';
    ctx.fillText(z.name, x + 12, y + 1);
  }
}

watch(() => props.zones, () => {
  if (animFrame) cancelAnimationFrame(animFrame);
  animFrame = requestAnimationFrame(draw);
}, { deep: false });

watch(() => props.windowSize, () => {
  if (animFrame) cancelAnimationFrame(animFrame);
  animFrame = requestAnimationFrame(draw);
});

onMounted(() => {
  draw();
  window.addEventListener('resize', draw);
});

onUnmounted(() => {
  window.removeEventListener('resize', draw);
  if (animFrame) cancelAnimationFrame(animFrame);
});
</script>

<template>
  <div class="composition-section">
    <div class="section-label">Composition</div>
    <canvas ref="canvas" class="composition-canvas"></canvas>
  </div>
</template>

<style scoped>
.composition-section {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.section-label {
  font-size: var(--fs-xs);
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-tertiary);
  font-weight: 500;
}

.composition-canvas {
  width: 100%;
  height: 160px;
  background: var(--bg-input);
  border-radius: var(--br);
}
</style>

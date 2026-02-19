<script setup>
import { ref, watch, onMounted, onUnmounted, computed } from 'vue';

const props = defineProps({
  zones: Array,       // profile.zones array
  windowSize: { type: Number, default: 1 },
});

const canvas = ref(null);
let animFrame = null;

const DISPLAY_LEN = 200;
const TARGET_MS = 16.67;

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

  // Compute windowed totals
  const windowedZones = props.zones.map(z => windowHistory(z.history, props.windowSize));
  const totals = [];
  for (let i = 0; i < DISPLAY_LEN; i++) {
    let sum = 0;
    for (const wz of windowedZones) sum += wz[i];
    totals.push(sum);
  }

  const maxVal = Math.max(25, ...totals);

  // 60fps target line
  const targetY = h - (TARGET_MS / maxVal) * (h - 8) - 4;
  ctx.strokeStyle = 'rgba(56, 60, 70, 0.6)';
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 4]);
  ctx.beginPath();
  ctx.moveTo(0, targetY);
  ctx.lineTo(w, targetY);
  ctx.stroke();
  ctx.setLineDash([]);

  ctx.fillStyle = '#444';
  ctx.font = '10px "Roboto Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('16.67ms', 4, targetY - 3);

  // Draw bars
  const barW = w / DISPLAY_LEN;
  for (let i = 0; i < DISPLAY_LEN; i++) {
    const t = totals[i];
    const barH = (t / maxVal) * (h - 8);
    const x = i * barW;
    const y = h - barH - 4;

    if (t < TARGET_MS) ctx.fillStyle = '#47b576';
    else if (t < 22) ctx.fillStyle = '#c0a030';
    else ctx.fillStyle = '#b5494b';

    ctx.fillRect(x, y, Math.max(barW - 0.5, 1), barH);
  }

  // Current ms readout
  const cur = totals[totals.length - 1];
  ctx.fillStyle = cur > 22 ? '#e05555' : cur > TARGET_MS ? '#c0a030' : '#47b576';
  ctx.font = 'bold 14px "Roboto Mono", monospace';
  ctx.textAlign = 'right';
  ctx.fillText(cur.toFixed(1) + ' ms', w - 6, 16);
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
  <div class="ribbon-section">
    <div class="section-label">Frame Time</div>
    <canvas ref="canvas" class="ribbon-canvas"></canvas>
  </div>
</template>

<style scoped>
.ribbon-section {
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

.ribbon-canvas {
  width: 100%;
  height: 56px;
  background: var(--bg-input);
  border-radius: var(--br);
}
</style>

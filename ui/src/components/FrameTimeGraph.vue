<script setup>
import { ref, watch, onMounted, onUnmounted } from 'vue';

const props = defineProps({
  data: Array,      // array of frame time values (ms)
  maxSamples: { type: Number, default: 200 },
  label: { type: String, default: 'Frame Time (ms)' },
  lineColor: { type: String, default: '#47B576' },
  fillColor: { type: String, default: 'rgba(71, 181, 118, 0.1)' },
});

const canvas = ref(null);
let animFrame = null;

function draw() {
  const el = canvas.value;
  if (!el) return;

  const ctx = el.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  const rect = el.getBoundingClientRect();
  const w = rect.width;
  const h = rect.height;

  el.width = w * dpr;
  el.height = h * dpr;
  ctx.scale(dpr, dpr);

  ctx.clearRect(0, 0, w, h);

  const values = props.data;
  if (!values || values.length < 2) return;

  // Auto-scale Y axis
  const maxVal = Math.max(...values) * 1.2;
  const minVal = 0;
  const range = maxVal - minVal || 1;

  // Draw grid lines
  ctx.strokeStyle = 'rgba(56, 60, 70, 0.5)';
  ctx.lineWidth = 1;
  const gridSteps = 4;
  ctx.font = '10px Inter, sans-serif';
  ctx.fillStyle = '#71717a';
  ctx.textAlign = 'right';

  for (let i = 0; i <= gridSteps; i++) {
    const y = (i / gridSteps) * h;
    const val = maxVal - (i / gridSteps) * range;
    ctx.beginPath();
    ctx.moveTo(32, y);
    ctx.lineTo(w, y);
    ctx.stroke();
    ctx.fillText(val.toFixed(1), 28, y + 3);
  }

  const graphX = 36;
  const graphW = w - graphX - 4;

  // Draw the line
  const step = graphW / (props.maxSamples - 1);
  const startIdx = Math.max(0, values.length - props.maxSamples);
  const visible = values.slice(startIdx);

  ctx.beginPath();
  for (let i = 0; i < visible.length; i++) {
    const x = graphX + (props.maxSamples - visible.length + i) * step;
    const y = h - ((visible[i] - minVal) / range) * h;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }

  // Fill under the line
  ctx.strokeStyle = props.lineColor;
  ctx.lineWidth = 1.5;
  ctx.stroke();

  // Close path for fill
  const lastX = graphX + (props.maxSamples - 1) * step;
  const firstX = graphX + (props.maxSamples - visible.length) * step;
  ctx.lineTo(lastX, h);
  ctx.lineTo(firstX, h);
  ctx.closePath();
  ctx.fillStyle = props.fillColor;
  ctx.fill();
}

watch(() => props.data?.length, () => {
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
  <div class="graph-container">
    <div class="graph-label">{{ label }}</div>
    <canvas ref="canvas" class="graph-canvas"></canvas>
  </div>
</template>

<style scoped>
.graph-container {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.graph-label {
  font-size: var(--fs-sm);
  color: var(--text-secondary);
  font-weight: 500;
}

.graph-canvas {
  width: 100%;
  height: 160px;
  background: var(--bg-input);
  border-radius: var(--br);
  border: 1px solid var(--border);
}
</style>

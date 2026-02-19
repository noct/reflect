<script setup>
import { ref, watch, onMounted, onUnmounted } from 'vue';

const props = defineProps({
  points: Array,   // array of [x, y] pairs
  closed: { type: Boolean, default: true },
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

  const pts = props.points;
  if (!pts || pts.length < 2) return;

  // Compute bounding box
  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  for (const [x, y] of pts) {
    if (x < minX) minX = x;
    if (y < minY) minY = y;
    if (x > maxX) maxX = x;
    if (y > maxY) maxY = y;
  }

  const bw = maxX - minX || 1;
  const bh = maxY - minY || 1;

  // Fit to canvas with padding
  const pad = 12;
  const drawW = w - pad * 2;
  const drawH = h - pad * 2;
  const scale = Math.min(drawW / bw, drawH / bh);
  const offX = pad + (drawW - bw * scale) / 2;
  const offY = pad + (drawH - bh * scale) / 2;

  function tx(x) { return offX + (x - minX) * scale; }
  function ty(y) { return offY + (y - minY) * scale; }

  // Draw fill
  ctx.beginPath();
  ctx.moveTo(tx(pts[0][0]), ty(pts[0][1]));
  for (let i = 1; i < pts.length; i++) {
    ctx.lineTo(tx(pts[i][0]), ty(pts[i][1]));
  }
  if (props.closed) ctx.closePath();
  ctx.fillStyle = 'rgba(71, 181, 118, 0.08)';
  ctx.fill();

  // Draw outline
  ctx.beginPath();
  ctx.moveTo(tx(pts[0][0]), ty(pts[0][1]));
  for (let i = 1; i < pts.length; i++) {
    ctx.lineTo(tx(pts[i][0]), ty(pts[i][1]));
  }
  if (props.closed) ctx.closePath();
  ctx.strokeStyle = '#47B576';
  ctx.lineWidth = 1.5;
  ctx.lineJoin = 'round';
  ctx.stroke();

  // Draw vertices
  ctx.fillStyle = '#47B576';
  for (const [x, y] of pts) {
    ctx.beginPath();
    ctx.arc(tx(x), ty(y), 2.5, 0, Math.PI * 2);
    ctx.fill();
  }
}

watch(() => props.points, () => {
  if (animFrame) cancelAnimationFrame(animFrame);
  animFrame = requestAnimationFrame(draw);
}, { deep: true });

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
  <canvas ref="canvas" class="points-canvas"></canvas>
</template>

<style scoped>
.points-canvas {
  width: 100%;
  height: 100px;
  background: var(--bg-input);
  border-radius: var(--br);
  border: 1px solid var(--border);
}
</style>

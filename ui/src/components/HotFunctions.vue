<script setup>
import { ref, watch, onMounted, onUnmounted, computed } from 'vue';

const props = defineProps({
  zones: Array,       // profile.zones array
  windowSize: { type: Number, default: 1 },
});

const DISPLAY_LEN = 200;
const SPARKLINE_LEN = 60;

// Consistent zone colors — assigned once, kept stable across re-sorts
const ZONE_COLORS = [
  '#4f95ff', '#3a72cc', '#6aadff', '#47b576', '#3a9460',
  '#5fd498', '#c0a030', '#a08828', '#b05fb0', '#b5894b',
  '#e06070', '#50b8b0', '#8870d0', '#d09050', '#60a0e0',
];
const zoneColorMap = new Map();
function getZoneColor(name) {
  if (!zoneColorMap.has(name)) {
    zoneColorMap.set(name, ZONE_COLORS[zoneColorMap.size % ZONE_COLORS.length]);
  }
  return zoneColorMap.get(name);
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

const stats = computed(() => {
  if (!props.zones?.length) return [];

  return props.zones.map(z => {
    const wh = windowHistory(z.history, props.windowSize);
    const recent = wh.slice(-SPARKLINE_LEN);
    const now = recent[recent.length - 1];
    const avg = z.ema;
    const delta = avg > 0.01 ? ((now - avg) / avg) * 100 : 0;
    return { name: z.name, color: getZoneColor(z.name), now, avg, delta, recent };
  }).sort((a, b) => {
    // Positive deltas (getting slower) first
    const aPos = a.delta > 0;
    const bPos = b.delta > 0;
    if (aPos && !bPos) return -1;
    if (!aPos && bPos) return 1;
    if (aPos && bPos) return b.delta - a.delta;
    return a.delta - b.delta;
  });
});

// Draw sparklines after stats update
const sparklineRefs = ref([]);

function drawSparklines() {
  const list = stats.value;
  const canvases = sparklineRefs.value;
  if (!canvases) return;

  for (let idx = 0; idx < list.length; idx++) {
    const el = canvases[idx];
    if (!el) continue;
    const s = list[idx];
    const ctx = el.getContext('2d');
    const sw = el.width;
    const sh = el.height;
    ctx.clearRect(0, 0, sw, sh);

    const vals = s.recent;
    const sMax = Math.max(s.avg * 2.5, ...vals);

    // Avg baseline
    const avgY = sh - (s.avg / sMax) * sh;
    ctx.strokeStyle = 'rgba(56, 60, 70, 0.6)';
    ctx.lineWidth = 0.5;
    ctx.beginPath();
    ctx.moveTo(0, avgY);
    ctx.lineTo(sw, avgY);
    ctx.stroke();

    // Line
    ctx.strokeStyle = s.color;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    for (let i = 0; i < vals.length; i++) {
      const x = (i / (vals.length - 1)) * sw;
      const y = sh - (vals[i] / sMax) * sh;
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }
}

watch(stats, () => {
  requestAnimationFrame(drawSparklines);
}, { flush: 'post' });

function sevColor(delta) {
  const abs = Math.abs(delta);
  if (abs > 100) return '#e05555';
  if (abs > 30) return '#c0a030';
  return 'var(--border)';
}

function deltaClass(delta) {
  const abs = Math.abs(delta);
  if (abs > 100) return 'delta-hot';
  if (abs > 30) return 'delta-warm';
  return 'delta-flat';
}

function rowClass(delta) {
  const abs = Math.abs(delta);
  if (abs > 100) return 'row-hot';
  if (abs > 30) return 'row-warm';
  return '';
}

function formatDelta(delta) {
  const abs = Math.abs(delta);
  const arrow = abs > 30 ? (delta > 0 ? '\u25B2 ' : '\u25BC ') : '';
  const sign = delta >= 0 ? '+' : '';
  return `${arrow}${sign}${delta.toFixed(0)}%`;
}
</script>

<template>
  <div class="hot-functions">
    <div class="section-label">Hot Functions</div>
    <table v-if="stats.length">
      <thead>
        <tr>
          <th class="col-sev"></th>
          <th class="col-name">Zone</th>
          <th class="col-num">Avg</th>
          <th class="col-num">Now</th>
          <th class="col-num">Delta</th>
          <th class="col-spark">Trend</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="(s, i) in stats" :key="s.name" :class="rowClass(s.delta)">
          <td><span class="severity-bar" :style="{ background: sevColor(s.delta) }"></span></td>
          <td class="name-cell">{{ s.name }}</td>
          <td class="num-cell avg">{{ s.avg.toFixed(1) }}ms</td>
          <td class="num-cell now">{{ s.now.toFixed(1) }}ms</td>
          <td class="num-cell" :class="deltaClass(s.delta)">{{ formatDelta(s.delta) }}</td>
          <td><canvas :ref="el => { if (el) sparklineRefs[i] = el }" width="100" height="20" class="sparkline"></canvas></td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<style scoped>
.hot-functions {
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

table {
  width: 100%;
  border-collapse: collapse;
}

thead th {
  text-align: left;
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  padding: 4px 8px;
  border-bottom: 1px solid var(--border);
  font-weight: normal;
}

th.col-num, td.num-cell {
  text-align: right;
}

th.col-sev {
  width: 12px;
}

th.col-spark {
  width: 100px;
}

tbody tr {
  border-bottom: 1px solid rgba(56, 60, 70, 0.2);
}

tbody td {
  padding: 5px 8px;
  font-family: var(--font-mono);
  font-size: var(--fs-sm);
  white-space: nowrap;
}

.row-hot {
  background: rgba(181, 73, 75, 0.08);
}

.row-warm {
  background: rgba(200, 170, 60, 0.05);
}

.severity-bar {
  display: inline-block;
  width: 4px;
  height: 16px;
  border-radius: 2px;
  vertical-align: middle;
}

.name-cell {
  color: var(--text-primary);
}

.num-cell {
  color: var(--text-primary);
}

.num-cell.avg {
  color: var(--text-tertiary);
}

.delta-hot {
  color: #e05555;
}

.delta-warm {
  color: #c0a030;
}

.delta-flat {
  color: var(--text-tertiary);
}

.sparkline {
  vertical-align: middle;
  display: block;
}
</style>

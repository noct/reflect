<script setup>
import FrameTimeGraph from './FrameTimeGraph.vue';

defineProps({
  perf: Object,
  perfHistory: Array,
  connected: Boolean,
});
</script>

<template>
  <div class="performance-panel" v-if="connected">
    <FrameTimeGraph :data="perfHistory" />

    <div class="stats" v-if="perf">
      <div class="stat-row">
        <span class="stat-name">FPS</span>
        <span class="stat-value">{{ perf.fps.toFixed(1) }}</span>
      </div>
      <div class="stat-row">
        <span class="stat-name">Frame Time</span>
        <span class="stat-value">{{ perf.frameTimeMs.toFixed(2) }} ms</span>
      </div>
      <div class="stat-row">
        <span class="stat-name">Entities</span>
        <span class="stat-value">{{ perf.entityCount }}</span>
      </div>
    </div>
  </div>
  <div v-else class="placeholder">
    Not connected
  </div>
</template>

<style scoped>
.performance-panel {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.stats {
  display: flex;
  flex-direction: column;
  gap: 1px;
}

.stat-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 6px 8px;
  background: var(--bg-input);
  border-radius: var(--br-sm);
}

.stat-name {
  font-size: var(--fs-sm);
  color: var(--text-secondary);
}

.stat-value {
  font-family: var(--font-mono);
  font-size: var(--fs-sm);
  color: var(--value-number);
}

.placeholder {
  font-size: var(--fs-sm);
  color: var(--text-tertiary);
  padding: var(--p-4);
}
</style>

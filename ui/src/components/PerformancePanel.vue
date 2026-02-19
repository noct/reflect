<script setup>
import { ref } from 'vue';
import FrameTimeRibbon from './FrameTimeRibbon.vue';
import HotFunctions from './HotFunctions.vue';
import CompositionChart from './CompositionChart.vue';

defineProps({
  perf: Object,
  perfHistory: Array,
  profile: Object,
  connected: Boolean,
});

const windowSize = ref(5);
</script>

<template>
  <div class="performance-panel" v-if="connected && profile">
    <FrameTimeRibbon
      :zones="profile.zones"
      :windowSize="windowSize"
    />

    <HotFunctions
      :zones="profile.zones"
      :windowSize="windowSize"
    />

    <CompositionChart
      :zones="profile.zones"
      :windowSize="windowSize"
    />

    <div class="controls">
      <label class="control-label">
        Window
        <input type="range" v-model.number="windowSize" min="1" max="30" class="control-range" />
        <span class="control-value">{{ windowSize }}</span>
      </label>
      <div class="stat-pills" v-if="perf">
        <span class="pill">{{ perf.fps.toFixed(0) }} FPS</span>
        <span class="pill">{{ perf.entityCount }} entities</span>
      </div>
    </div>
  </div>
  <div v-else-if="connected" class="placeholder">
    Waiting for profile data...
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

.controls {
  display: flex;
  align-items: center;
  gap: 16px;
  padding-top: 4px;
}

.control-label {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  text-transform: uppercase;
  letter-spacing: 0.08em;
}

.control-range {
  width: 100px;
  accent-color: var(--accent-green);
}

.control-value {
  font-family: var(--font-mono);
  font-size: var(--fs-sm);
  color: var(--text-secondary);
  min-width: 20px;
}

.stat-pills {
  display: flex;
  gap: 8px;
  margin-left: auto;
}

.pill {
  font-family: var(--font-mono);
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  background: var(--bg-input);
  padding: 2px 8px;
  border-radius: 10px;
}

.placeholder {
  font-size: var(--fs-sm);
  color: var(--text-tertiary);
  padding: var(--p-4);
}
</style>

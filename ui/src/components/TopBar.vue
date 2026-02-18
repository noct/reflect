<script setup>
import ConnectionStatus from './ConnectionStatus.vue';

defineProps({
  connected: Boolean,
  perf: Object,
});
</script>

<template>
  <header class="topbar">
    <div class="topbar-left">
      <svg class="app-logo" viewBox="0 0 64 64" fill="none">
        <path d="M32 6 L6 32 L32 58" stroke="currentColor" stroke-width="4" stroke-linejoin="round" stroke-linecap="round"/>
        <path d="M32 6 L58 32 L32 58" stroke="currentColor" stroke-width="4" stroke-linejoin="round" stroke-linecap="round" opacity="0.35"/>
        <path d="M32 6 L6 32 L32 58 L58 32 Z" fill="currentColor" opacity="0.07"/>
        <circle cx="32" cy="32" r="4" fill="currentColor" opacity="0.8"/>
      </svg>
      <span class="app-title">Reflector</span>
      <ConnectionStatus :connected="connected" />
    </div>
    <div class="topbar-right" v-if="perf">
      <span class="stat">
        <span class="stat-value">{{ perf.fps.toFixed(1) }}</span>
        <span class="stat-label">fps</span>
      </span>
      <span class="stat-divider"></span>
      <span class="stat">
        <span class="stat-value">{{ perf.frameTimeMs.toFixed(2) }}</span>
        <span class="stat-label">ms/frame</span>
      </span>
      <span class="stat-divider"></span>
      <span class="stat">
        <span class="stat-value">{{ perf.entityCount }}</span>
        <span class="stat-label">entities</span>
      </span>
    </div>
  </header>
</template>

<style scoped>
.topbar {
  height: var(--topbar-height);
  background: var(--bg-panel-header);
  border-bottom: 1px solid var(--border);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 var(--p-4);
  flex-shrink: 0;
}

.topbar-left {
  display: flex;
  align-items: center;
  gap: 12px;
}

.app-logo {
  width: 20px;
  height: 20px;
  color: var(--accent-green);
}

.app-title {
  font-size: var(--fs-md);
  font-weight: 600;
  color: var(--text-primary);
  margin-right: 4px;
}

.topbar-right {
  display: flex;
  align-items: center;
  gap: 8px;
}

.stat {
  display: flex;
  align-items: baseline;
  gap: 4px;
}

.stat-value {
  font-family: var(--font-mono);
  font-size: var(--fs-sm);
  color: var(--value-number);
}

.stat-label {
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
}

.stat-divider {
  width: 1px;
  height: 14px;
  background: var(--border);
}
</style>

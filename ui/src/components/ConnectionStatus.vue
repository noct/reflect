<script setup>
defineProps({
  connected: Boolean,
});
</script>

<template>
  <div class="connection-status">
    <span class="dot" :class="{ connected }"></span>
    <span class="label" v-if="connected">Connected</span>
    <span class="label disconnected" v-else>Disconnected</span>
    <span class="address">localhost:7700</span>
  </div>
</template>

<style scoped>
.connection-status {
  display: flex;
  align-items: center;
  gap: 6px;
}

.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--accent-red);
  transition: background 0.3s ease;
  flex-shrink: 0;
}

.dot.connected {
  background: var(--accent-green);
  box-shadow: 0 0 6px rgba(71, 181, 118, 0.4);
}

.label {
  font-size: var(--fs-sm);
  color: var(--accent-green);
}

.label.disconnected {
  color: var(--accent-red);
}

.address {
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  font-family: var(--font-mono);
}
</style>

<script setup>
defineProps({
  activeTab: String,
});

defineEmits(['update:activeTab']);

const tabs = [
  { id: 'inspect', label: 'Inspect' },
  { id: 'performance', label: 'Performance' },
];
</script>

<template>
  <div class="tab-bar">
    <button
      v-for="tab in tabs"
      :key="tab.id"
      class="tab"
      :class="{ active: activeTab === tab.id }"
      @click="$emit('update:activeTab', tab.id)"
    >
      {{ tab.label }}
    </button>
  </div>
</template>

<style scoped>
.tab-bar {
  display: flex;
  background: var(--bg-panel-header);
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
}

.tab {
  padding: 6px 16px;
  font-family: var(--font-sans);
  font-size: var(--fs-sm);
  font-weight: 500;
  color: var(--text-secondary);
  background: none;
  border: none;
  border-bottom: 2px solid transparent;
  cursor: pointer;
  transition: color 0.15s ease, border-color 0.15s ease;
}

.tab:hover {
  color: var(--text-primary);
}

.tab.active {
  color: var(--accent-green);
  border-bottom-color: var(--accent-green);
}
</style>

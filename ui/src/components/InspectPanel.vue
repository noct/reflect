<script setup>
import { ref, watch } from 'vue';
import { useApi } from '../api.js';
import PointsPreview from './PointsPreview.vue';

const props = defineProps({
  entityId: String,
  connected: Boolean,
});

const { fetchEntity } = useApi();

const properties = ref(null);
const loading = ref(false);

watch(
  () => props.entityId,
  async (id) => {
    if (!id) {
      properties.value = null;
      return;
    }
    loading.value = true;
    try {
      const data = await fetchEntity(id);
      properties.value = data.properties;
    } catch {
      properties.value = null;
    } finally {
      loading.value = false;
    }
  },
  { immediate: true }
);

function valueClass(type) {
  switch (type) {
    case 'float':
    case 'int':
      return 'value-number';
    case 'string':
      return 'value-string';
    case 'color':
      return 'value-color';
    default:
      return '';
  }
}
</script>

<template>
  <div class="inspect-panel">
    <template v-if="properties">
      <div class="properties">
        <template v-for="prop in properties" :key="prop.name">
          <div v-if="prop.type === 'points2d'" class="prop-block">
            <div class="prop-block-header">
              <span class="prop-name">{{ prop.name }}</span>
              <span class="prop-count">{{ prop.value.length }} points</span>
              <span class="prop-type">{{ prop.type }}</span>
            </div>
            <PointsPreview :points="prop.value" />
          </div>
          <div v-else class="prop-row">
            <span class="prop-name">{{ prop.name }}</span>
            <span class="prop-value" :class="valueClass(prop.type)">
              <span v-if="prop.type === 'color'" class="color-swatch" :style="{ background: prop.value }"></span>
              {{ prop.value }}
            </span>
            <span class="prop-type">{{ prop.type }}</span>
          </div>
        </template>
      </div>
    </template>

    <div v-else-if="loading" class="placeholder">Loading...</div>
    <div v-else class="placeholder">Select an entity to inspect</div>
  </div>
</template>

<style scoped>
.inspect-panel {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.properties {
  display: flex;
  flex-direction: column;
  gap: 1px;
}

.prop-row {
  display: flex;
  align-items: center;
  padding: 5px 8px;
  background: var(--bg-input);
  border-radius: var(--br-sm);
  gap: 8px;
}

.prop-name {
  font-size: var(--fs-sm);
  color: var(--text-secondary);
  min-width: 120px;
  flex-shrink: 0;
}

.prop-value {
  font-family: var(--font-mono);
  font-size: var(--fs-sm);
  color: var(--text-primary);
  flex: 1;
  display: flex;
  align-items: center;
  gap: 6px;
}

.prop-value.value-number {
  color: var(--value-number);
}

.prop-value.value-string {
  color: var(--value-string);
}

.prop-value.value-color {
  color: var(--text-secondary);
}

.prop-type {
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  min-width: 36px;
  text-align: right;
}

.color-swatch {
  display: inline-block;
  width: 14px;
  height: 14px;
  border-radius: var(--br-sm);
  border: 1px solid var(--border);
  flex-shrink: 0;
}

.prop-block {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.prop-block-header {
  display: flex;
  align-items: center;
  padding: 5px 8px 0;
  gap: 8px;
}

.prop-count {
  font-family: var(--font-mono);
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  flex: 1;
}

.placeholder {
  font-size: var(--fs-sm);
  color: var(--text-tertiary);
  padding: var(--p-4);
}
</style>

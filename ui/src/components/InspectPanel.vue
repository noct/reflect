<script setup>
import { ref, watch } from 'vue';
import { useApi } from '../api.js';

const props = defineProps({
  entityId: String,
  connected: Boolean,
});

const { fetchEntity } = useApi();

const entity = ref(null);
const loading = ref(false);

watch(
  () => props.entityId,
  async (id) => {
    if (!id) {
      entity.value = null;
      return;
    }
    loading.value = true;
    try {
      entity.value = await fetchEntity(id);
    } catch {
      entity.value = null;
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

function formatId(id) {
  const num = parseInt(id);
  if (isNaN(num)) return id;
  return '0x' + num.toString(16).toUpperCase();
}
</script>

<template>
  <div class="inspect-panel">
    <template v-if="entity">
      <div class="entity-header">
        <span class="entity-type">{{ entity.type }}</span>
        <span class="entity-name" v-if="entity.name">{{ entity.name }}</span>
        <span class="entity-id">{{ formatId(entity.id) }}</span>
      </div>

      <div class="properties">
        <div class="prop-row" v-for="prop in entity.properties" :key="prop.name">
          <span class="prop-name">{{ prop.name }}</span>
          <span class="prop-value" :class="valueClass(prop.type)">
            <span v-if="prop.type === 'color'" class="color-swatch" :style="{ background: prop.value }"></span>
            {{ prop.value }}
          </span>
          <span class="prop-type">{{ prop.type }}</span>
        </div>
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

.entity-header {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.entity-type {
  font-size: var(--fs-md);
  font-weight: 500;
  color: var(--accent-green);
}

.entity-name {
  font-size: var(--fs-sm);
  color: var(--text-primary);
}

.entity-id {
  font-size: var(--fs-xs);
  font-family: var(--font-mono);
  color: var(--text-tertiary);
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

.placeholder {
  font-size: var(--fs-sm);
  color: var(--text-tertiary);
  padding: var(--p-4);
}
</style>

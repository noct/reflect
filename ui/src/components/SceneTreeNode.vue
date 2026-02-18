<script setup>
import { ref } from 'vue';

const props = defineProps({
  node: Object,
  depth: Number,
  selectedId: String,
});

defineEmits(['select']);

const expanded = ref(props.depth < 2);
const hasChildren = props.node.children && props.node.children.length > 0;

function toggle() {
  if (hasChildren) {
    expanded.value = !expanded.value;
  }
}

const displayName = props.node.name || props.node.type;
</script>

<template>
  <div class="tree-node">
    <div
      class="node-row"
      :class="{ selected: selectedId === node.id }"
      :style="{ paddingLeft: (depth * 16 + 8) + 'px' }"
      @click.stop="$emit('select', node.id)"
    >
      <span
        class="chevron"
        :class="{ expanded, invisible: !hasChildren }"
        @click.stop="toggle"
      >&#9654;</span>
      <span class="node-label">{{ displayName }}</span>
      <span class="node-id">{{ node.type }}</span>
    </div>

    <div v-if="expanded && hasChildren" class="children">
      <SceneTreeNode
        v-for="child in node.children"
        :key="child.id"
        :node="child"
        :depth="depth + 1"
        :selectedId="selectedId"
        @select="$emit('select', $event)"
      />
    </div>
  </div>
</template>

<style scoped>
.node-row {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 3px 8px;
  cursor: pointer;
  transition: background 0.1s ease;
  white-space: nowrap;
}

.node-row:hover {
  background: var(--bg-hover);
}

.node-row.selected {
  background: var(--bg-selected);
}

.chevron {
  font-size: 8px;
  color: var(--text-tertiary);
  transition: transform 0.1s ease-out;
  width: 12px;
  flex-shrink: 0;
  text-align: center;
  opacity: 0.6;
}

.chevron:hover {
  opacity: 1;
}

.chevron.expanded {
  transform: rotate(90deg);
}

.chevron.invisible {
  visibility: hidden;
}

.node-label {
  font-size: var(--fs-sm);
  color: var(--text-primary);
  overflow: hidden;
  text-overflow: ellipsis;
}

.node-id {
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  margin-left: auto;
  padding-left: 8px;
}
</style>

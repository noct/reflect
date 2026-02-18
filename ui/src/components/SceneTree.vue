<script setup>
import SceneTreeNode from './SceneTreeNode.vue';

defineProps({
  scene: Object,
  connected: Boolean,
  selectedId: String,
});

defineEmits(['select']);
</script>

<template>
  <div class="scene-tree">
    <template v-if="connected && scene?.entities?.length">
      <SceneTreeNode
        v-for="entity in scene.entities"
        :key="entity.id"
        :node="entity"
        :depth="0"
        :selectedId="selectedId"
        @select="$emit('select', $event)"
      />
    </template>
    <div v-else-if="connected" class="placeholder">
      Loading scene...
    </div>
    <div v-else class="placeholder">
      Not connected
    </div>
  </div>
</template>

<style scoped>
.scene-tree {
  user-select: none;
}

.placeholder {
  padding: var(--p-4);
  font-size: var(--fs-sm);
  color: var(--text-tertiary);
}
</style>

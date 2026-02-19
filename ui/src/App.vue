<script setup>
import { ref, provide } from 'vue';
import { useApi } from './api.js';
import TopBar from './components/TopBar.vue';
import SceneTree from './components/SceneTree.vue';
import TabBar from './components/TabBar.vue';
import InspectPanel from './components/InspectPanel.vue';
import PerformancePanel from './components/PerformancePanel.vue';

const { connected, perf, perfHistory, scene, profile, fetchEntity } = useApi();

const selectedEntityId = ref(null);
const activeTab = ref('inspect');

provide('selectedEntityId', selectedEntityId);
provide('fetchEntity', fetchEntity);
</script>

<template>
  <div class="app-layout">
    <TopBar :connected="connected" :perf="perf" />

    <div class="app-body">
      <aside class="sidebar">
        <div class="sidebar-header">Scene</div>
        <div class="sidebar-content">
          <SceneTree
            :scene="scene"
            :connected="connected"
            :selectedId="selectedEntityId"
            @select="selectedEntityId = $event"
          />
        </div>
      </aside>

      <main class="main-panel">
        <TabBar :activeTab="activeTab" @update:activeTab="activeTab = $event" />
        <div class="panel-content">
          <InspectPanel
            v-if="activeTab === 'inspect'"
            :entityId="selectedEntityId"
            :connected="connected"
          />
          <PerformancePanel
            v-else-if="activeTab === 'performance'"
            :perf="perf"
            :perfHistory="perfHistory"
            :profile="profile"
            :connected="connected"
          />
        </div>
      </main>
    </div>
  </div>
</template>

<style scoped>
.app-layout {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.app-body {
  display: flex;
  flex: 1;
  min-height: 0;
}

.sidebar {
  width: var(--sidebar-width);
  min-width: var(--sidebar-width);
  background: var(--bg-panel);
  border-right: 1px solid var(--border);
  display: flex;
  flex-direction: column;
}

.sidebar-header {
  padding: var(--p-3) var(--p-4);
  font-size: var(--fs-sm);
  font-weight: 500;
  color: var(--text-secondary);
  text-transform: uppercase;
  letter-spacing: 0.05em;
  border-bottom: 1px solid var(--border);
  background: var(--bg-panel-header);
}

.sidebar-content {
  flex: 1;
  overflow-y: auto;
  padding: var(--p-2) 0;
}

.main-panel {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-width: 0;
  background: var(--bg-panel);
}

.panel-content {
  flex: 1;
  overflow-y: auto;
  padding: var(--p-4);
}
</style>

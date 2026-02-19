import { ref, readonly, onUnmounted } from 'vue';

const POLL_INTERVAL = 2000;
const PERF_POLL_INTERVAL = 500;
const PROFILE_POLL_INTERVAL = 200;
const PERF_HISTORY_SIZE = 200;

// Shared reactive state
const connected = ref(false);
const perf = ref(null);
const perfHistory = ref([]);
const scene = ref(null);
const profile = ref(null);

let pollTimer = null;
let perfTimer = null;
let profileTimer = null;

async function fetchJson(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`${res.status}`);
  return res.json();
}

// ---------------------------------------------------------------------------
// Connection polling: try /api/perf to detect if server is up
// ---------------------------------------------------------------------------
async function pollConnection() {
  try {
    const data = await fetchJson('/api/perf');
    perf.value = data;
    pushPerfSample(data.frameTimeMs);

    if (!connected.value) {
      connected.value = true;
      // First connection: fetch scene tree
      await refreshScene();
      // Start fast perf polling
      startPerfPolling();
      startProfilePolling();
    }
  } catch {
    if (connected.value) {
      connected.value = false;
      perf.value = null;
      scene.value = null;
      profile.value = null;
      stopPerfPolling();
      stopProfilePolling();
    }
  }
}

function pushPerfSample(frameTimeMs) {
  const hist = perfHistory.value;
  if (hist.length >= PERF_HISTORY_SIZE) {
    hist.shift();
  }
  hist.push(frameTimeMs);
}

function startPerfPolling() {
  stopPerfPolling();
  perfTimer = setInterval(async () => {
    try {
      const data = await fetchJson('/api/perf');
      perf.value = data;
      pushPerfSample(data.frameTimeMs);
    } catch {
      // Connection poll will handle disconnect
    }
  }, PERF_POLL_INTERVAL);
}

function stopPerfPolling() {
  if (perfTimer) {
    clearInterval(perfTimer);
    perfTimer = null;
  }
}

function startProfilePolling() {
  stopProfilePolling();
  profileTimer = setInterval(async () => {
    try {
      profile.value = await fetchJson('/api/profile');
    } catch {
      // Connection poll will handle disconnect
    }
  }, PROFILE_POLL_INTERVAL);
}

function stopProfilePolling() {
  if (profileTimer) {
    clearInterval(profileTimer);
    profileTimer = null;
  }
}

async function refreshScene() {
  try {
    const data = await fetchJson('/api/scene');
    scene.value = data;
  } catch {
    // Ignore, will retry on next connection
  }
}

async function fetchEntity(id) {
  return fetchJson(`/api/entity/${id}`);
}

// ---------------------------------------------------------------------------
// Start / stop
// ---------------------------------------------------------------------------
function startPolling() {
  if (pollTimer) return;
  pollConnection(); // immediate first check
  pollTimer = setInterval(pollConnection, POLL_INTERVAL);
}

function stopPolling() {
  if (pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
  }
  stopPerfPolling();
  stopProfilePolling();
}

// Composable for components
export function useApi() {
  // Auto-start on first use
  startPolling();

  return {
    connected: readonly(connected),
    perf: readonly(perf),
    perfHistory: readonly(perfHistory),
    scene: readonly(scene),
    profile: readonly(profile),
    fetchEntity,
    refreshScene,
  };
}

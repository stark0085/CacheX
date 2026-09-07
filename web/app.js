const REST_BASE = 'http://localhost:8080';
const WS_URL = 'ws://localhost:8081';

const el = id => document.getElementById(id);
const cacheGrid = el('cacheGrid');
const logEl = el('log');
const wsDot = el('wsDot');
const wsStatus = el('wsStatus');

// Client-side mirror of what we believe is in the cache, purely for the
// visual grid — the server remains the source of truth for actual data.
let knownKeys = new Map(); // key -> { freq: number | null }
let currentPolicy = 'LRU';

function renderGrid() {
  if (knownKeys.size === 0) {
    cacheGrid.innerHTML = '<div class="empty-note">no keys yet — put one below</div>';
    return;
  }
  cacheGrid.innerHTML = '';
  for (const [key, meta] of knownKeys) {
    const slot = document.createElement('div');
    slot.className = 'slot';
    slot.dataset.key = key;
    slot.innerHTML = `<div class="k">${escapeHtml(key)}</div>` +
      (currentPolicy === 'LFU' && meta.freq != null ? `<div class="f">freq ${meta.freq}</div>` : '');
    cacheGrid.appendChild(slot);
  }
}

function escapeHtml(s) {
  const d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}

function flashSlot(key, className) {
  const slot = cacheGrid.querySelector(`.slot[data-key="${CSS.escape(key)}"]`);
  if (!slot) return;
  slot.classList.add(className);
  setTimeout(() => slot.classList.remove(className), 500);
}

function logEvent(evt) {
  if (logEl.querySelector('.log-empty')) logEl.innerHTML = '';
  const row = document.createElement('div');
  row.className = 'log-row';
  const time = new Date(evt.timestamp).toLocaleTimeString();
  row.innerHTML = `
    <span>${time}</span>
    <span class="type-${evt.type}">${evt.type}</span>
    <span>${escapeHtml(evt.key)}</span>
    <span>${evt.policy}</span>
  `;
  logEl.insertBefore(row, logEl.firstChild);
  // Cap log length so it doesn't grow unbounded during a long demo session.
  while (logEl.children.length > 200) logEl.removeChild(logEl.lastChild);
}

function setPolicyUI(policy) {
  currentPolicy = policy;
  el('btnLRU').classList.toggle('active', policy === 'LRU');
  el('btnLFU').classList.toggle('active', policy === 'LFU');
}

async function refreshStats() {
  try {
    const res = await fetch(`${REST_BASE}/stats`);
    const data = await res.json();
    el('statHits').textContent = data.hits;
    el('statMisses').textContent = data.misses;
    el('statEvictions').textContent = data.evictions;
    el('statHitRate').textContent = (data.hitRate * 100).toFixed(1) + '%';
    if (data.policy && data.policy !== currentPolicy) setPolicyUI(data.policy);
  } catch (e) {
    // Stats fetch failing usually means the REST server isn't reachable;
    // the WS connection indicator is the primary signal for that, so we
    // stay quiet here rather than duplicating an error state.
  }
}

// ---------- WebSocket ----------
let ws;
function connectWS() {
  ws = new WebSocket(WS_URL);

  ws.onopen = () => {
    wsDot.classList.remove('off');
    wsDot.classList.add('on');
    wsStatus.textContent = 'connected';
  };

  ws.onclose = () => {
    wsDot.classList.remove('on');
    wsDot.classList.add('off');
    wsStatus.textContent = 'disconnected — retrying…';
    setTimeout(connectWS, 2000);
  };

  ws.onerror = () => { /* onclose will fire right after; handled there */ };

  ws.onmessage = (msg) => {
    let evt;
    try { evt = JSON.parse(msg.data); } catch { return; }
    logEvent(evt);

    switch (evt.type) {
      case 'set':
        if (!knownKeys.has(evt.key)) knownKeys.set(evt.key, { freq: 1 });
        renderGrid();
        flashSlot(evt.key, 'flash-hit');
        break;
      case 'hit':
        flashSlot(evt.key, 'flash-hit');
        break;
      case 'miss':
        flashSlot(evt.key, 'flash-miss');
        if (knownKeys.has(evt.key)) {
          setTimeout(() => { knownKeys.delete(evt.key); renderGrid(); }, 480);
        }
        break;
      case 'eviction':
        flashSlot(evt.key, 'flash-evict');
        setTimeout(() => { knownKeys.delete(evt.key); renderGrid(); }, 480);
        break;
      case 'remove':
        knownKeys.delete(evt.key);
        renderGrid();
        break;
      case 'expired':
        flashSlot(evt.key, 'flash-evict');
        setTimeout(() => { knownKeys.delete(evt.key); renderGrid(); }, 480);
        break;
      case 'clear':
        knownKeys.clear();
        renderGrid();
        break;
      case 'policy_change':
        knownKeys.clear();
        renderGrid();
        setPolicyUI(evt.policy);
        break;
    }
    refreshStats();
  };
}
connectWS();

// ---------- REST actions ----------
async function doPut() {
  const key = el('keyInput').value.trim();
  const value = el('valueInput').value;
  const ttl = el('ttlInput').value.trim();
  if (!key) return;
  const url = ttl
    ? `${REST_BASE}/cache/${encodeURIComponent(key)}?ttl=${encodeURIComponent(ttl)}`
    : `${REST_BASE}/cache/${encodeURIComponent(key)}`;
  await fetch(url, { method: 'PUT', body: value });
}

async function doGet() {
  const key = el('keyInput').value.trim();
  if (!key) return;
  await fetch(`${REST_BASE}/cache/${encodeURIComponent(key)}`);
}

async function doDel() {
  const key = el('keyInput').value.trim();
  if (!key) return;
  await fetch(`${REST_BASE}/cache/${encodeURIComponent(key)}`, { method: 'DELETE' });
}

async function doFillRandom() {
  for (let i = 0; i < 20; i++) {
    const k = 'k' + Math.floor(Math.random() * 500);
    const v = 'v' + Math.random().toString(36).slice(2, 8);
    await fetch(`${REST_BASE}/cache/${k}`, { method: 'PUT', body: v });
    await new Promise(r => setTimeout(r, 40));
  }
}

async function doClearCache() {
  await fetch(`${REST_BASE}/cache`, { method: 'DELETE' });
}

async function switchPolicy(policy) {
  await fetch(`${REST_BASE}/policy`, { method: 'POST', body: policy });
  knownKeys.clear();
  renderGrid();
  setPolicyUI(policy);
  refreshStats();
}

el('btnPut').addEventListener('click', doPut);
el('btnGet').addEventListener('click', doGet);
el('btnDel').addEventListener('click', doDel);
el('btnFill').addEventListener('click', doFillRandom);
el('btnClear').addEventListener('click', () => { logEl.innerHTML = '<div class="log-empty">waiting for events…</div>'; });
el('btnClearCache').addEventListener('click', doClearCache);
el('btnLRU').addEventListener('click', () => switchPolicy('LRU'));
el('btnLFU').addEventListener('click', () => switchPolicy('LFU'));

el('keyInput').addEventListener('keydown', e => { if (e.key === 'Enter') doPut(); });

refreshStats();
setInterval(refreshStats, 4000);
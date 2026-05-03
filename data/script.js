let gateway = `ws://${window.location.hostname}/ws`;
let websocket = null;
let relayList = [];
let deleteTarget = null;
let tempTrendChart = null;
let humiTrendChart = null;
let lcdState = true;
let aiPrediction = 0;
let fanState = false;
let fanSpeed = 255;

const TEMP_HISTORY_POINTS = 300;
const tempHistory = [];
const humiHistory = [];

window.addEventListener('load', () => {
  initTheme();
  initWebSocket();
  initTempTrendChart();
  initHumiTrendChart();
  updateGauge('temp', 0);
  updateGauge('humi', 0);
  setStatus('LCD', true);
  setAiStatus(0);
  setStatus(8, false);
  syncFanSpeedUI(fanSpeed);
  updateSystemStats(0, 0, -127);
});

function initTheme() {
  const toggle = document.getElementById('themeToggle');
  const savedTheme = localStorage.getItem('dashboard-theme') || 'light';
  document.body.setAttribute('data-theme', savedTheme === 'dark' ? 'dark' : 'light');
  if (toggle) {
    toggle.checked = savedTheme === 'dark';
    toggle.addEventListener('change', () => {
      const next = toggle.checked ? 'dark' : 'light';
      document.body.setAttribute('data-theme', next);
      localStorage.setItem('dashboard-theme', next);
    });
  }
}

function initWebSocket() {
  websocket = new WebSocket(gateway);
  websocket.onopen = () => console.log('WebSocket connected');
  websocket.onclose = () => {
    console.log('WebSocket disconnected');
    setTimeout(initWebSocket, 2000);
  };
  websocket.onmessage = onMessage;
}

function Send_Data(data) {
  if (websocket && websocket.readyState === WebSocket.OPEN) {
    websocket.send(data);
  } else {
    alert('WebSocket chua ket noi!');
  }
}

function initTempTrendChart() {
  const canvas = document.getElementById('tempTrendChart');
  if (!canvas || typeof Chart === 'undefined') return;

  const ctx = canvas.getContext('2d');
  tempTrendChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Nhiet do',
        data: [],
        borderColor: '#5d8dff',
        borderWidth: 2,
        fill: true,
        tension: 0.35,
        pointRadius: 0,
        backgroundColor: 'rgba(93, 141, 255, 0.18)'
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      plugins: {
        legend: { display: false },
        tooltip: { mode: 'index', intersect: false }
      },
      scales: {
        x: {
          ticks: { display: false },
          grid: { display: false }
        },
        y: {
          ticks: { maxTicksLimit: 5 },
          grid: { color: 'rgba(130, 145, 173, 0.25)' }
        }
      }
    }
  });
}

function initHumiTrendChart() {
  const canvas = document.getElementById('humiTrendChart');
  if (!canvas || typeof Chart === 'undefined') return;

  const ctx = canvas.getContext('2d');
  humiTrendChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Do am',
        data: [],
        borderColor: '#28ae74',
        borderWidth: 2,
        fill: true,
        tension: 0.35,
        pointRadius: 0,
        backgroundColor: 'rgba(40, 174, 116, 0.18)'
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      plugins: {
        legend: { display: false },
        tooltip: { mode: 'index', intersect: false }
      },
      scales: {
        x: {
          ticks: { display: false },
          grid: { display: false }
        },
        y: {
          ticks: { maxTicksLimit: 5 },
          grid: { color: 'rgba(130, 145, 173, 0.25)' }
        }
      }
    }
  });
}

function pushTempTrend(value) {
  const now = new Date();
  const label = `${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;

  tempHistory.push({ label, value: Number(value) || 0 });
  if (tempHistory.length > TEMP_HISTORY_POINTS) tempHistory.shift();

  if (!tempTrendChart) return;
  tempTrendChart.data.labels = tempHistory.map((p) => p.label);
  tempTrendChart.data.datasets[0].data = tempHistory.map((p) => p.value);
  tempTrendChart.update('none');
}

function pushHumiTrend(value) {
  const now = new Date();
  const label = `${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;

  humiHistory.push({ label, value: Number(value) || 0 });
  if (humiHistory.length > TEMP_HISTORY_POINTS) humiHistory.shift();

  if (!humiTrendChart) return;
  humiTrendChart.data.labels = humiHistory.map((p) => p.label);
  humiTrendChart.data.datasets[0].data = humiHistory.map((p) => p.value);
  humiTrendChart.update('none');
}

function updateGauge(type, value) {
  const isTemp = type === 'temp';
  const max = isTemp ? 50 : 100;
  const clamped = Math.max(0, Math.min(Number(value) || 0, max));
  const percent = (clamped / max) * 100;

  const gaugeEl = document.getElementById(isTemp ? 'tempGauge' : 'humiGauge');
  const valueEl = document.getElementById(isTemp ? 'tempValue' : 'humiValue');
  if (!gaugeEl || !valueEl) return;

  gaugeEl.style.setProperty('--progress', percent.toFixed(2));
  valueEl.textContent = Math.round(clamped).toString();
}

function setStatus(gpio, isOn) {
  const el = document.getElementById(`status${gpio}`);
  if (!el) return;

  el.textContent = isOn ? 'ON' : 'OFF';
  el.classList.remove('status-on', 'status-off');
  el.classList.add(isOn ? 'status-on' : 'status-off');
}

function setAiStatus(prediction) {
  aiPrediction = Number(prediction) === 1 ? 1 : 0;
  const el = document.getElementById('aiStatus');
  if (!el) return;

  const isNormal = aiPrediction === 0;
  el.textContent = isNormal ? 'Normal' : 'Anomaly';
  el.classList.remove('status-on', 'status-off');
  el.classList.add(isNormal ? 'status-on' : 'status-off');
}

function formatUptime(totalSec) {
  const sec = Math.max(0, Number(totalSec) || 0);
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

function updateRSSIBars(rssi) {
  const bars = document.querySelectorAll('#rssiBars i');
  bars.forEach((b) => b.classList.remove('active'));

  let activeBars = 0;
  if (rssi >= -60) activeBars = 4;
  else if (rssi >= -70) activeBars = 3;
  else if (rssi >= -80) activeBars = 2;
  else if (rssi >= -90) activeBars = 1;

  for (let i = 0; i < activeBars; i++) {
    bars[i]?.classList.add('active');
  }
}

function updateSystemStats(uptime, heap, rssi) {
  const uptimeEl = document.getElementById('uptimeValue');
  const heapEl = document.getElementById('heapValue');
  const rssiEl = document.getElementById('rssiValue');

  if (uptimeEl) uptimeEl.textContent = formatUptime(uptime);
  if (heapEl) heapEl.textContent = `${Math.round((Number(heap) || 0) / 1024)} KB`;
  if (rssiEl) rssiEl.textContent = `${Number(rssi) || -127} dBm`;

  updateRSSIBars(Number(rssi) || -127);
}

function onMessage(event) {
  try {
    const data = JSON.parse(event.data);

    if (typeof data.temp === 'number') {
      updateGauge('temp', data.temp);
      pushTempTrend(data.temp);
    }
    if (typeof data.humi === 'number') {
      updateGauge('humi', data.humi);
      pushHumiTrend(data.humi);
    }
    if (typeof data.lcd !== 'undefined') {
      lcdState = Boolean(data.lcd);
      setStatus('LCD', lcdState);
    }
    if (typeof data.ai !== 'undefined') {
      setAiStatus(data.ai);
    }
    if (typeof data.fan !== 'undefined') {
      fanState = Boolean(data.fan);
      setStatus(8, fanState);
    }
    if (typeof data.fanSpeed === 'number') {
      syncFanSpeedUI(data.fanSpeed);
    }

    updateSystemStats(data.uptime, data.heap, data.rssi);
  } catch (e) {
    console.warn('Invalid JSON:', event.data);
  }
}

function syncFanSpeedUI(value) {
  fanSpeed = Math.max(0, Math.min(255, Number(value) || 0));
  const slider = document.getElementById('fanSpeedSlider');
  const valueEl = document.getElementById('fanSpeedValue');
  if (slider) slider.value = fanSpeed.toString();
  if (valueEl) valueEl.textContent = fanSpeed.toString();
}

function showSection(id, event) {
  document.querySelectorAll('.section').forEach((sec) => (sec.style.display = 'none'));
  document.getElementById(id).style.display = 'block';

  document.querySelectorAll('.nav-item').forEach((i) => i.classList.remove('active'));
  event.currentTarget.classList.add('active');
}

function openAddRelayDialog() {
  document.getElementById('addRelayDialog').style.display = 'flex';
}

function closeAddRelayDialog() {
  document.getElementById('addRelayDialog').style.display = 'none';
}

function saveRelay() {
  const name = document.getElementById('relayName').value.trim();
  const gpio = Number(document.getElementById('relayGPIO').value.trim());
  if (!name || Number.isNaN(gpio)) return alert('Vui long nhap day du thong tin!');

  relayList.push({ id: Date.now(), name, gpio, state: false });
  renderRelays();
  closeAddRelayDialog();
}

function renderRelays() {
  const container = document.getElementById('relayContainer');
  if (!container) return;
  container.innerHTML = '';

  relayList.forEach((r) => {
    const card = document.createElement('div');
    card.className = 'device-card';
    card.innerHTML = `
      <i class="fa-solid fa-bolt device-icon"></i>
      <h3>${r.name}</h3>
      <p>GPIO: ${r.gpio}</p>
      <button class="toggle-btn ${r.state ? 'on' : ''}" onclick="toggleRelay(${r.id})">
        ${r.state ? 'ON' : 'OFF'}
      </button>
      <i class="fa-solid fa-trash delete-icon" onclick="showDeleteDialog(${r.id})"></i>
    `;
    container.appendChild(card);
  });
}

function toggleRelay(id) {
  const relay = relayList.find((r) => r.id === id);
  if (!relay) return;

  relay.state = !relay.state;
  fetch(`/action?gpio=${encodeURIComponent(relay.gpio)}&status=${encodeURIComponent(relay.state ? 'ON' : 'OFF')}`)
    .then((response) => {
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      return response.text();
    })
    .then(() => renderRelays())
    .catch((error) => {
      relay.state = !relay.state;
      console.log('Relay action failed:', error);
      renderRelays();
    });
}

function onFanSpeedInput(value) {
  syncFanSpeedUI(value);
}

function setFanSpeed(value) {
  syncFanSpeedUI(value);
  fetch(`/action?gpio=8&status=SPEED&value=${encodeURIComponent(fanSpeed)}`)
    .then((response) => {
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      return response.text();
    })
    .catch((error) => console.log('Fan speed failed:', error));
}

function controlLcd(action) {
  fetch(`/action?device=lcd&status=${encodeURIComponent(action)}`)
    .then((response) => {
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      return response.text();
    })
    .then(() => {
      lcdState = action === 'ON';
      setStatus('LCD', lcdState);
    })
    .catch((error) => console.log('LCD action failed:', error));
}

function showDeleteDialog(id) {
  deleteTarget = id;
  document.getElementById('confirmDeleteDialog').style.display = 'flex';
}

function closeConfirmDelete() {
  document.getElementById('confirmDeleteDialog').style.display = 'none';
}

function confirmDelete() {
  relayList = relayList.filter((r) => r.id !== deleteTarget);
  renderRelays();
  closeConfirmDelete();
}

function controlDevice(gpioPin, action) {
  fetch(`/action?gpio=${encodeURIComponent(gpioPin)}&status=${encodeURIComponent(action)}`)
    .then((response) => {
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      return response.text();
    })
    .then(() => setStatus(gpioPin, action === 'ON'))
    .catch((error) => console.log('Action failed:', error));
}

document.getElementById('settingsForm').addEventListener('submit', function (e) {
  e.preventDefault();

  const ssid = document.getElementById('ssid').value.trim();
  const password = document.getElementById('password').value.trim();
  const token = document.getElementById('token').value.trim();
  const server = document.getElementById('server').value.trim();
  const port = document.getElementById('port').value.trim();

  Send_Data(
    JSON.stringify({
      page: 'setting',
      value: {
        ssid,
        password,
        token,
        server,
        port
      }
    })
  );

  alert('Da gui cau hinh den thiet bi');
});
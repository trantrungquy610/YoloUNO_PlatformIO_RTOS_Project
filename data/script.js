/* YOLO UNO Web UI - port 8080. No third-party scripts required (works in AP mode).
 * Sensor data: GET /api/sensors. Device info: GET /api/info.
 * Commands: WebSocket /ws, with request_id based acknowledgement.
 */
'use strict';

const el = id => document.getElementById(id);
const WS_RETRY_MS = 2500;
const POLL_SENSOR_MS = 3000;
const POLL_INFO_MS = 5000;
let socket = null;
let reconnectTimer = null;
let requestCounter = 0;
const pending = new Map();
let info = null;
let relayBusy = false;
let relayOn = false;
let fanBusy = false;
let fanDragging = false;
let managedDevices = [];

function setText(id, value) {
  const node = el(id);
  if (node) node.textContent = String(value);
}
function notify(message, kind = 'info') {
  const node = el('feedback');
  if (!node) return;
  node.textContent = message;
  node.dataset.kind = kind;
}
function socketReady() {
  return socket !== null && socket.readyState === WebSocket.OPEN;
}
function setSocketStatus(connected) {
  const node = el('connectionStatus');
  if (node) {
    node.textContent = connected ? 'WebSocket: Đã kết nối' : 'WebSocket: Mất kết nối';
    node.dataset.status = connected ? 'online' : 'offline';
  }
  renderRelay();
  renderFan();
}
function connectSocket() {
  if (reconnectTimer !== null) { clearTimeout(reconnectTimer); reconnectTimer = null; }
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;
  const scheme = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  if (!window.location.host) {
    setSocketStatus(false);
    notify('Hãy mở giao diện qua địa chỉ IP của YOLO UNO (cổng 8080), không mở file HTML trực tiếp.', 'error');
    return;
  }
  try {
    socket = new WebSocket(`${scheme}//${window.location.host}/ws`);
  } catch (_error) {
    setSocketStatus(false);
    reconnectTimer = setTimeout(connectSocket, WS_RETRY_MS);
    return;
  }
  const currentSocket = socket;
  currentSocket.addEventListener('open', () => {
    if (socket !== currentSocket) return;
    setSocketStatus(true);
    notify('Đã kết nối WebSocket với thiết bị.', 'ok');
  });
  currentSocket.addEventListener('message', event => {
    let data;
    try { data = JSON.parse(event.data); } catch (_error) { return; }
    const requestId = Number(data.request_id);
    if ((data.page === 'device_result' || data.page === 'fan_result' || data.page === 'setting_saved' || data.page === 'device_added' || data.page === 'device_removed' || data.page === 'managed_device_result') && pending.has(requestId)) {
      const op = pending.get(requestId);
      pending.delete(requestId);
      clearTimeout(op.timer);
      if (op.page !== data.page) return;
      if (data.status === 'ok') op.resolve(data);
      else op.reject(new Error(data.message || 'Thiết bị từ chối yêu cầu.'));
    } else if (data.page === 'error') {
      notify(data.message || 'WebSocket báo lỗi.', 'error');
    }
  });
  currentSocket.addEventListener('close', () => {
    if (socket !== currentSocket) return;
    setSocketStatus(false);
    for (const [, op] of pending) {
      clearTimeout(op.timer);
      op.reject(new Error('Mất kết nối trước khi nhận phản hồi của thiết bị.'));
    }
    pending.clear();
    notify('WebSocket bị ngắt. Giao diện sẽ tự kết nối lại.', 'error');
    reconnectTimer = setTimeout(connectSocket, WS_RETRY_MS);
  });
  currentSocket.addEventListener('error', () => { setSocketStatus(false); });
}
function requestDevice(page, value, expectedPage) {
  if (!socketReady()) return Promise.reject(new Error('WebSocket chưa kết nối.'));
  requestCounter += 1;
  const requestId = requestCounter;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pending.delete(requestId);
      reject(new Error('Không có phản hồi từ thiết bị sau 8 giây.'));
    }, 8000);
    pending.set(requestId, { resolve, reject, timer, page: expectedPage });
    try {
      socket.send(JSON.stringify({ page, request_id: requestId, value }));
    } catch (_error) {
      clearTimeout(timer);
      pending.delete(requestId);
      reject(new Error('Không gửi được lệnh WebSocket.'));
    }
  });
}

function showSection(id, event) {
  document.querySelectorAll('.section').forEach(section => {
    section.style.display = section.id === id ? (id === 'settings' ? 'flex' : 'block') : 'none';
  });
  document.querySelectorAll('.nav-item').forEach(node => node.classList.remove('active'));
  if (event && event.currentTarget) event.currentTarget.classList.add('active');
}

function setGauge(id, value, min, max, suffix) {
  const gauge = el(id);
  if (!gauge) return;
  const valid = Number.isFinite(value);
  const percent = valid ? Math.min(100, Math.max(0, (value - min) * 100 / (max - min))) : 0;
  gauge.style.setProperty('--progress', `${percent}%`);
  gauge.dataset.valid = valid ? 'true' : 'false';
  const label = gauge.querySelector('.gauge-value');
  if (label) label.textContent = valid ? `${value.toFixed(1)} ${suffix}` : '—';
}
async function pollSensors() {
  try {
    const response = await fetch('/api/sensors', { cache: 'no-store' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    const temperature = Number(data.temperature);
    const humidity = Number(data.humidity);
    if (data.valid === true && Number.isFinite(temperature) && Number.isFinite(humidity)) {
      setGauge('gauge_temp', temperature, -10, 50, '°C');
      setGauge('gauge_humi', humidity, 0, 100, '%');
      setText('sensorStatus', 'Cảm biến DHT20: Bình thường');
      setText('lastUpdate', `Lần cập nhật: ${new Date().toLocaleTimeString('vi-VN')}`);
    } else {
      setGauge('gauge_temp', NaN, -10, 50, '°C');
      setGauge('gauge_humi', NaN, 0, 100, '%');
      setText('sensorStatus', data.sensor_error ? 'Cảm biến DHT20: Lỗi đọc' : 'Đang chờ dữ liệu DHT20 hợp lệ');
      setText('lastUpdate', 'Chưa có dữ liệu hợp lệ');
    }
  } catch (_error) {
    setGauge('gauge_temp', NaN, -10, 50, '°C');
    setGauge('gauge_humi', NaN, 0, 100, '%');
    setText('sensorStatus', 'Không đọc được dữ liệu cảm biến từ WebServer');
    setText('lastUpdate', 'Dữ liệu không khả dụng');
  }
}
function prettyUptime(sec) {
  const seconds = Number(sec);
  if (!Number.isFinite(seconds) || seconds < 0) return '—';
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  return `${h} giờ ${m} phút`;
}
async function pollInfo() {
  try {
    const response = await fetch('/api/info', { cache: 'no-store' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    info = await response.json();
    setText('infoIp', info.ip || '—');
    setText('infoWifi', `${info.wifi_mode || '—'} (${info.wifi_connected ? 'Đã kết nối' : 'Chưa kết nối Internet'})`);
    setText('infoUptime', prettyUptime(info.uptime_seconds));
    setText('infoCoreIot', info.core_iot_configured ? 'Đã lưu cấu hình' : 'Chưa lưu cấu hình');
    setText('infoRelay', Number.isInteger(info.relay_gpio) && info.relay_gpio >= 0 ? `GPIO ${info.relay_gpio}` : 'Chưa được cấu hình');
    if (!relayBusy) relayOn = info.relay_on === true;
    renderRelay();
    renderFan();
  } catch (_error) {
    info = null;
    ['infoIp', 'infoWifi', 'infoUptime', 'infoCoreIot', 'infoRelay'].forEach(id => setText(id, 'Không khả dụng'));
    renderRelay();
    renderFan();
  }
}
function renderRelay() {
  const area = el('relayContainer');
  if (!area) return;
  area.replaceChildren();
  const gpio = info && Number.isInteger(info.relay_gpio) ? info.relay_gpio : -1;
  if (gpio < 0) {
    const text = document.createElement('p');
    text.className = 'empty-state';
    text.textContent = 'Relay Web chưa được cấu hình. Đề nghị thành viên phần cứng xác nhận chân GPIO an toàn rồi đặt WEB_RELAY_GPIO trong firmware.';
    area.appendChild(text);
    return;
  }
  const card = document.createElement('div');
  card.className = 'device-card';
  const title = document.createElement('h3');
  title.textContent = 'Relay được cấu hình';
  const pin = document.createElement('p');
  pin.textContent = `GPIO ${gpio} · ${relayOn ? 'Mức HIGH' : 'Mức LOW'}`;
  const description = document.createElement('p');
  description.className = 'device-caption';
  description.textContent = 'Trạng thái GPIO không xác nhận tải thực tế đã đóng/ngắt.';
  const button = document.createElement('button');
  button.type = 'button';
  button.className = `toggle-btn ${relayOn ? 'on' : ''}`;
  button.textContent = relayBusy ? 'Đang chờ phản hồi…' : relayOn ? 'Tắt relay' : 'Bật relay';
  button.disabled = relayBusy || !socketReady();
  button.addEventListener('click', () => toggleRelay(gpio));
  card.append(title, pin, description, button);
  area.appendChild(card);
}
async function toggleRelay(gpio) {
  if (relayBusy || !info || info.relay_gpio !== gpio) return;
  relayBusy = true;
  renderRelay();
  try {
    const result = await requestDevice('device', {
      gpio,
      status: relayOn ? 'OFF' : 'ON'
    }, 'device_result');
    relayOn = result.state === 'ON';
    notify(`Thiết bị đã xác nhận ghi GPIO ${gpio}: ${result.state}. Kiểm tra relay thật để xác nhận tải.`, 'ok');
  } catch (error) {
    notify(`Lệnh relay chưa được xác nhận: ${error.message}`, 'error');
  } finally {
    relayBusy = false;
    renderRelay();
  }
}
function renderFan() {
  const slider = el('fanSpeedSlider');
  const output = el('fanSpeedValue');
  if (!slider || !output) return;
  const reportedSpeed = info && Number(info.fan_speed_percent);
  const hasSpeed = Number.isInteger(reportedSpeed) && reportedSpeed >= 0 && reportedSpeed <= 100;
  slider.disabled = fanBusy || !hasSpeed || !socketReady();
  if (hasSpeed && !fanBusy && !fanDragging) {
    slider.value = String(reportedSpeed);
    output.value = `${reportedSpeed}%`;
    output.textContent = `${reportedSpeed}%`;
  }
  if (!hasSpeed) setText('fanControlStatus', 'Không đọc được trạng thái quạt từ thiết bị.');
  else if (fanBusy) setText('fanControlStatus', 'Đang chờ thiết bị áp dụng tốc độ…');
  else if (!socketReady()) setText('fanControlStatus', 'Kết nối WebSocket để điều khiển quạt.');
  else setText('fanControlStatus', 'Sẵn sàng điều khiển.');
}
async function pollManagedDevices() {
  try {
    const response = await fetch('/api/devices', { cache: 'no-store' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    managedDevices = Array.isArray(data.devices) ? data.devices : [];
    renderManagedDevices();
  } catch (_error) {
    setText('managedDeviceList', 'Không đọc được danh sách thiết bị đã thêm.');
  }
}
function renderManagedDevices() {
  const list = el('managedDeviceList');
  if (!list) return;
  list.replaceChildren();
  if (!managedDevices.length) {
    const empty = document.createElement('p');
    empty.className = 'empty-state';
    empty.textContent = 'Chưa có thiết bị nào được thêm.';
    list.appendChild(empty);
    return;
  }
  managedDevices.forEach(device => {
    const card = document.createElement('section');
    card.className = 'device-card managed-device-card';
    const title = document.createElement('h3');
    title.textContent = device.name;
    const detail = document.createElement('p');
    detail.className = 'device-caption';
    detail.textContent = `${device.type === 'rgb' ? 'NeoPixel' : device.type === 'fan' ? 'Quạt PWM' : 'Relay'} · GPIO ${device.gpio}${device.pixel_count ? ` · ${device.pixel_count} pixel` : ''}`;
    card.append(title, detail);
    if (device.type === 'relay') {
      const button = document.createElement('button');
      button.className = `toggle-btn ${device.on ? 'on' : ''}`;
      button.textContent = device.on ? 'Tắt relay' : 'Bật relay';
      button.disabled = !socketReady();
      button.addEventListener('click', () => controlManagedDevice(device, device.on ? 0 : 1));
      card.appendChild(button);
    } else if (device.type === 'fan') {
      const label = document.createElement('label');
      label.textContent = `Tốc độ: ${device.percent}%`;
      const slider = document.createElement('input');
      slider.type = 'range'; slider.min = '0'; slider.max = '100'; slider.value = String(device.percent || 0);
      slider.disabled = !socketReady();
      slider.addEventListener('change', () => controlManagedDevice(device, Number(slider.value)));
      card.append(label, slider);
    } else {
      const color = document.createElement('input');
      color.type = 'color'; color.value = /^#[0-9A-Fa-f]{6}$/.test(device.color) ? device.color : '#000000';
      const apply = document.createElement('button');
      apply.textContent = 'Đặt màu'; apply.disabled = !socketReady();
      apply.addEventListener('click', () => controlManagedDevice(device, parseInt(color.value.slice(1), 16)));
      card.append(color, apply);
    }
    const remove = document.createElement('button');
    remove.type = 'button'; remove.className = 'remove-device'; remove.textContent = 'Xóa thiết bị';
    remove.disabled = !socketReady();
    remove.addEventListener('click', () => removeManagedDevice(device));
    card.appendChild(remove);
    list.appendChild(card);
  });
}
async function controlManagedDevice(device, value) {
  try {
    await requestDevice('managed_device_control', { id: device.id, type: device.type, value }, 'managed_device_result');
    await pollManagedDevices();
    notify(`Đã gửi lệnh cho ${device.name}.`, 'ok');
  } catch (error) { notify(`Điều khiển chưa thành công: ${error.message}`, 'error'); }
}
async function removeManagedDevice(device) {
  try {
    await requestDevice('device_remove', { id: device.id }, 'device_removed');
    await pollManagedDevices();
    notify(`Đã xóa ${device.name} khỏi cấu hình.`, 'ok');
  } catch (error) { notify(`Chưa xóa được thiết bị: ${error.message}`, 'error'); }
}
async function addManagedDevice(event) {
  event.preventDefault();
  const type = el('newDeviceType').value;
  const value = {
    name: el('newDeviceName').value.trim(), type,
    gpio: Number(el('newDeviceGpio').value),
    pixel_count: Number(el('newDevicePixels').value || 4),
    active_low: el('newDeviceActiveLow').checked
  };
  const button = el('addDeviceButton'); button.disabled = true;
  try {
    await requestDevice('device_add', value, 'device_added');
    el('newDeviceName').value = ''; el('newDeviceGpio').value = '';
    await pollManagedDevices();
    notify('Đã thêm và lưu thiết bị. Cấu hình vẫn còn sau khi khởi động lại.', 'ok');
  } catch (error) { notify(`Chưa thêm được thiết bị: ${error.message}`, 'error'); }
  finally { button.disabled = false; }
}
async function setFanSpeed() {
  const slider = el('fanSpeedSlider');
  if (!slider || fanBusy || !socketReady()) return;
  fanDragging = false;
  const percent = Number(slider.value);
  if (!Number.isInteger(percent) || percent < 0 || percent > 100) {
    renderFan();
    return;
  }
  fanBusy = true;
  renderFan();
  try {
    const result = await requestDevice('fan', { percent }, 'fan_result');
    if (info) info.fan_speed_percent = result.fan_speed_percent;
    notify(`Đã đặt tốc độ quạt ${result.fan_speed_percent}%.`, 'ok');
  } catch (error) {
    notify(`Chưa đặt được tốc độ quạt: ${error.message}`, 'error');
  } finally {
    fanBusy = false;
    renderFan();
  }
}
function previewFanSpeed() {
  const slider = el('fanSpeedSlider');
  const output = el('fanSpeedValue');
  if (slider && output) {
    output.value = `${slider.value}%`;
    output.textContent = `${slider.value}%`;
  }
}
async function submitSettings(event) {
  event.preventDefault();
  const ssid = el('ssid').value.trim();
  const password = el('password').value;
  const token = el('token').value.trim();
  const server = el('server').value.trim();
  const port = el('port').value.trim();
  const portNum = Number(port);
  if (!ssid || ssid.length > 32 || password.length > 63 || !token || !server ||
      !Number.isInteger(portNum) || portNum < 1 || portNum > 65535) {
    notify('Kiểm tra SSID (1–32 ký tự), mật khẩu (tối đa 63 ký tự), token, server và port (1–65535).', 'error');
    return;
  }
  const saveButton = el('saveSettings');
  saveButton.disabled = true;
  notify('Đã gửi yêu cầu; đang đợi thiết bị xác nhận lưu LittleFS…');
  try {
    const response = await requestDevice('setting', { ssid, password, token, server, port }, 'setting_saved');
    notify(`${response.message || 'Đã lưu cấu hình.'} WebSocket sẽ ngắt khi thiết bị khởi động lại; hãy kết nối vào IP mới.`, 'ok');
    // Do not persist secrets in browser storage or log them.
    el('password').value = '';
    el('token').value = '';
  } catch (error) {
    notify(`Lưu cấu hình chưa thành công: ${error.message}`, 'error');
  } finally {
    saveButton.disabled = false;
  }
}
function init() {
  const form = el('settingsForm');
  if (form) form.addEventListener('submit', submitSettings);
  const addForm = el('addDeviceForm');
  if (addForm) addForm.addEventListener('submit', addManagedDevice);
  const typeSelect = el('newDeviceType');
  if (typeSelect) typeSelect.addEventListener('change', () => {
    const isRgb = typeSelect.value === 'rgb';
    const isRelay = typeSelect.value === 'relay';
    el('pixelCountLabel').hidden = !isRgb;
    el('newDevicePixels').hidden = !isRgb;
    el('activeLowLabel').hidden = !isRelay;
  });
  const fanSlider = el('fanSpeedSlider');
  if (fanSlider) {
    fanSlider.addEventListener('pointerdown', () => { fanDragging = true; });
    fanSlider.addEventListener('pointerup', () => { fanDragging = false; });
    fanSlider.addEventListener('input', previewFanSpeed);
    fanSlider.addEventListener('change', setFanSpeed);
  }
  setGauge('gauge_temp', NaN, -10, 50, '°C');
  setGauge('gauge_humi', NaN, 0, 100, '%');
  connectSocket();
  pollSensors();
  pollInfo();
  pollManagedDevices();
  setInterval(pollSensors, POLL_SENSOR_MS);
  setInterval(pollInfo, POLL_INFO_MS);
  setInterval(pollManagedDevices, 3000);
}
window.addEventListener('DOMContentLoaded', init);

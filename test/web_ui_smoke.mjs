// Run: node test/web_ui_smoke.mjs
// JS-only smoke test. Mocks browser + ESP32 APIs; NOT a hardware test.
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const html = readFileSync(new URL('../data/index.html', import.meta.url), 'utf8');
const js = readFileSync(new URL('../data/script.js', import.meta.url), 'utf8');
const ids = [...html.matchAll(/\sid="([^"]+)"/g)].map(m => m[1]);
assert.equal(new Set(ids).size, ids.length, 'HTML ids must be unique');

const elements = new Map();
function element(id = '') {
  return {
    id, value: '', dataset: {}, style: { setProperty(name, value) { this[name] = value; } },
    children: [], disabled: false, textContent: '',
    listeners: {}, classList: { add() {}, remove() {} },
    addEventListener(type, callback) { this.listeners[type] = callback; },
    querySelector(selector) { return selector === '.gauge-value' ? this.inner : null; },
    appendChild(child) { this.children.push(child); },
    append(...children) { this.children.push(...children); },
    replaceChildren(...children) { this.children = children; }
  };
}
for (const id of ids) {
  const e = element(id);
  if (id.startsWith('gauge_')) e.inner = element();
  elements.set(id, e);
}
let documentCallback;
const document = {
  getElementById: id => elements.get(id) || null,
  querySelectorAll: () => [],
  createElement: () => element()
};
let sensorResponse = { valid: true, temperature: 27.2, humidity: 66.5, sensor_error: false };
let infoResponse = {
  ip: '192.168.4.1', wifi_mode: 'AP', wifi_connected: false,
  uptime_seconds: 78, core_iot_configured: false, relay_gpio: -1
};
const fetch = async url => ({
  ok: true,
  json: async () => url.includes('sensors') ? sensorResponse : infoResponse
});
class FakeSocket {
  static OPEN = 1;
  static CONNECTING = 0;
  constructor(url) { this.url = url; this.readyState = 1; this.listeners = {}; this.sent = []; FakeSocket.current = this; }
  addEventListener(type, callback) { this.listeners[type] = callback; }
  send(value) { this.sent.push(JSON.parse(value)); }
  emit(type, value = {}) { if (type === 'close') this.readyState = 3; this.listeners[type]?.(value); }
}
const window = { location: { protocol: 'http:', host: '192.168.4.1:8080' },
  addEventListener(type, cb) { if (type === 'DOMContentLoaded') documentCallback = cb; } };
const timers = new Map();
let nextTimer = 0;
const setInterval = () => ++nextTimer;
const setTimeout = fn => { timers.set(++nextTimer, fn); return nextTimer; };
const clearTimeout = id => timers.delete(id);
const context = vm.createContext({ window, document, fetch, WebSocket: FakeSocket,
  setTimeout, clearTimeout, setInterval, console, Date, Map, Number, String, Error, Promise });
vm.runInContext(js, context, { filename: 'script.js' });
assert.equal(typeof documentCallback, 'function');
documentCallback();
const flush = async () => { await Promise.resolve(); await Promise.resolve(); await new Promise(resolve => process.nextTick(resolve)); };
await flush();
assert.match(elements.get('gauge_temp').inner.textContent, /27\.2/);
assert.match(elements.get('gauge_humi').inner.textContent, /66\.5/);
assert.match(elements.get('sensorStatus').textContent, /Bình thường/);
assert.match(elements.get('relayContainer').children[0].textContent, /chưa được cấu hình/);
FakeSocket.current.emit('open');
assert.match(elements.get('connectionStatus').textContent, /Đã kết nối/);

sensorResponse = { valid: false, temperature: 0, humidity: 0, sensor_error: true };
await vm.runInContext('pollSensors()', context);
assert.equal(elements.get('gauge_temp').inner.textContent, '—');
assert.match(elements.get('sensorStatus').textContent, /Lỗi đọc/);

infoResponse = { ...infoResponse, relay_gpio: 13, relay_on: false };
await vm.runInContext('pollInfo()', context);
let card = elements.get('relayContainer').children[0];
assert.equal(card.children[3].disabled, false);
card.children[3].listeners.click();
const command = FakeSocket.current.sent.at(-1);
assert.equal(command.page, 'device');
assert.equal(command.value.gpio, 13);
assert.equal(command.value.status, 'ON');
FakeSocket.current.emit('message', { data: JSON.stringify({
  page: 'device_result', request_id: command.request_id, status: 'ok', gpio: 13, state: 'ON'
}) });
await flush();
card = elements.get('relayContainer').children[0];
assert.match(card.children[1].textContent, /HIGH/);
assert.match(elements.get('feedback').textContent, /xác nhận ghi GPIO/);

for (const [id, val] of Object.entries({ ssid: 'TestNet', password: '12345678', token: 'TOP_SECRET', server: 'example.org', port: '1883' })) elements.get(id).value = val;
let prevent = false;
elements.get('settingsForm').listeners.submit({ preventDefault() { prevent = true; } });
assert(prevent);
const save = FakeSocket.current.sent.at(-1);
assert.equal(save.page, 'setting');
assert.equal(save.value.token, 'TOP_SECRET');
assert.equal(elements.get('saveSettings').disabled, true);
FakeSocket.current.emit('message', { data: JSON.stringify({
  page: 'setting_saved', request_id: save.request_id, status: 'ok', message: 'Saved'
}) });
await flush();
assert.equal(elements.get('saveSettings').disabled, false);
assert.equal(elements.get('token').value, '');
assert.equal(elements.get('password').value, '');
assert.match(elements.get('feedback').textContent, /Saved/);

FakeSocket.current.emit('close');
assert.match(elements.get('connectionStatus').textContent, /Mất kết nối/);
assert.equal(elements.get('relayContainer').children[0].children[3].disabled, true);
console.log('PASS: DOM references, real-sensor flow, invalid sensor, relay ACK, settings ACK, offline/reconnect status');

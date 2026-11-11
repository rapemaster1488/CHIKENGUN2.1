#!/usr/bin/env node
// ============================================================================
// CHIKENGUN 2.5 — Headless smoke-test WebGL2 движка (web/webgl2_arena.js).
// Заодно проверяет, что все текстуры, запрошенные движком, существуют на диске (anti-404).
// Запускает всю симуляцию в Node с заглушками WebGL2/DOM и прогоняет ~2 минуты
// боя: автоогонь, волны, кибер-пиридачки, коллайдеры, трупы, HUD.
// Любой ReferenceError/TypeError/NaN внутри tick() роняет тест.
//
// Запуск:  node tools/headless_smoke.js
// ============================================================================
"use strict";
const fs = require("fs");
const path = require("path");

const SRC = fs.readFileSync(path.join(__dirname, "..", "web", "webgl2_arena.js"), "utf8");

let simNow = 0;
let drawCalls = 0;
let lastHud = null;
let maxCorpses = 0;
let corpseFrames = 0;
const winHandlers = Object.create(null);
const docHandlers = Object.create(null);
const canvasHandlers = Object.create(null);
const addTo = (bag) => (type, fn) => { (bag[type] = bag[type] || []).push(fn); };
const fireBag = (bag, ev) => { (bag[ev.type] || []).forEach(fn => fn(ev)); };

// ---------- WebGL2 stub ----------
const GL_CONST = {
  ARRAY_BUFFER: 0x8892, COLOR_BUFFER_BIT: 0x4000, COMPILE_STATUS: 0x8B81,
  CULL_FACE: 0x0B44, DEPTH_BUFFER_BIT: 0x0100, DEPTH_TEST: 0x0B71,
  ELEMENT_ARRAY_BUFFER: 0x8893, FLOAT: 0x1406, FRAGMENT_SHADER: 0x8B30,
  LEQUAL: 0x0203, LESS: 0x0201, LINEAR: 0x2601, LINEAR_MIPMAP_LINEAR: 0x2703,
  LINK_STATUS: 0x8B82, REPEAT: 0x2901, RGBA: 0x1908, STATIC_DRAW: 0x88E4,
  TEXTURE0: 0x84C0, TEXTURE1: 0x84C1, TEXTURE2: 0x84C2, TEXTURE3: 0x84C3,
  TEXTURE_2D: 0x0DE1, TEXTURE_CUBE_MAP: 0x8513,
  TEXTURE_CUBE_MAP_POSITIVE_X: 0x8515, TEXTURE_CUBE_MAP_NEGATIVE_X: 0x8516,
  TEXTURE_CUBE_MAP_POSITIVE_Y: 0x8517, TEXTURE_CUBE_MAP_NEGATIVE_Y: 0x8518,
  TEXTURE_CUBE_MAP_POSITIVE_Z: 0x8519, TEXTURE_CUBE_MAP_NEGATIVE_Z: 0x851A,
  TEXTURE_MAG_FILTER: 0x2800, TEXTURE_MIN_FILTER: 0x2801,
  TEXTURE_WRAP_S: 0x2802, TEXTURE_WRAP_T: 0x2803,
  TRIANGLES: 0x0004, UNSIGNED_BYTE: 0x1401, UNSIGNED_SHORT: 0x1403,
  VERTEX_SHADER: 0x8B31, BLEND: 0x0BE2, SRC_ALPHA: 0x0302, ONE_MINUS_SRC_ALPHA: 0x0303,
  CLAMP_TO_EDGE: 0x812F, NEAREST: 0x2600, TEXTURE4: 0x84C4,
  FRONT: 0x0404, BACK: 0x0405,
  FRAMEBUFFER: 0x8D40, DEPTH_ATTACHMENT: 0x8D00, DEPTH_COMPONENT: 0x1902, DEPTH_COMPONENT24: 0x81A6,
  UNSIGNED_INT: 0x1405, FRAMEBUFFER_COMPLETE: 0x8CD5, NONE: 0x0000
};
const gl = { ...GL_CONST };
let uniformLocId = 0;
const noop = () => {};
[
  "activeTexture","attachShader","bindBuffer","bindTexture","bindVertexArray",
  "bufferData","clear","compileShader","depthFunc","disable","enable",
  "enableVertexAttribArray","generateMipmap","linkProgram","shaderSource",
  "texImage2D","texParameterf","texParameteri","useProgram","vertexAttribPointer","viewport",
  "blendFunc","depthMask","cullFace","clearColor",
  "createFramebuffer","bindFramebuffer","framebufferTexture2D","drawBuffers","readBuffer"
].forEach(fn => { gl[fn] = noop; });
gl.createFramebuffer = () => ({});
gl.checkFramebufferStatus = () => GL_CONST.FRAMEBUFFER_COMPLETE; // shadow-map FBO «поднялся» → глубина гоняется в тесте
gl.createVertexArray = () => ({});
gl.createBuffer = () => ({});
gl.createTexture = () => ({});
gl.createShader = () => ({});
gl.createProgram = () => ({});
gl.getShaderParameter = () => true;
gl.getProgramParameter = () => true;
gl.getShaderInfoLog = () => "";
gl.getProgramInfoLog = () => "";
gl.getUniformLocation = () => ({ loc: ++uniformLocId });
gl.getExtension = () => ({ TEXTURE_MAX_ANISOTROPY_EXT: 0x84FE, MAX_TEXTURE_MAX_ANISOTROPY_EXT: 0x84FF });
gl.getParameter = () => 16;
gl.uniform1f = noop; gl.uniform1i = noop; gl.uniform2f = noop; gl.uniform3f = noop;
gl.uniform4f = noop; gl.uniform3fv = noop; gl.uniformMatrix4fv = noop;
gl.drawElements = () => { drawCalls++; };

// ---------- DOM stubs ----------
const canvas = {
  width: 1280, height: 720, style: {},
  getContext: () => gl,
  addEventListener: addTo(canvasHandlers),
  requestPointerLock() { return Promise.resolve(); }
};
const document = {
  getElementById: (id) => (id === "glCanvas" ? canvas : undefined),
  addEventListener: addTo(docHandlers),
  // минимальный canvas 2D: shadowMat создаётся, blob-тени рисуются и проверяются в прогоне
  createElement: () => ({
    width: 0, height: 0, style: {},
    getContext: () => ({
      createRadialGradient: () => ({ addColorStop() {} }),
      fillRect() {},
      fillStyle: ""
    })
  }),
  pointerLockElement: canvas,   // притворяемся, что pointer lock захвачен → огонь работает
  hidden: false
};
const window = {
  addEventListener: addTo(winHandlers),
  innerWidth: 1280, innerHeight: 720
};
const localStorage = {
  _m: new Map(),
  getItem(k) { return this._m.has(k) ? this._m.get(k) : null; },
  setItem(k, v) { this._m.set(k, String(v)); },
  removeItem(k) { this._m.delete(k); }
};
const requestedSrcs = new Set();
class FakeImage {
  constructor() { this._src = ""; }
  set src(v) { this._src = v; requestedSrcs.add(String(v).split("?")[0]); if (this.onload) this.onload(); }
  get src() { return this._src; }
}
const performance = { now: () => simNow };
let rafCb = null;
const requestAnimationFrame = (cb) => { rafCb = cb; };
const alert = (msg) => { throw new Error("alert: " + msg); };

// ---------- opts stubs (как в index.html, но без DOM) ----------
const feed = [];
const opts = {
  playSfx: noop,
  pushFeed: (m) => feed.push(m),
  showBanner: noop, showToast: noop, cycleMusic: noop,
  flashHit: noop, flashDamage: noop, spawnDmgNum: noop, flashDamageFrom: noop,
  updateBossHud: noop,
  isPlaying: () => true,
  isPerkModalOpen: () => false,
  isBanyaOpen: () => false,
  openBanya: noop, exitArenaToMenu: noop, togglePerkModal: noop,
  buyPerk: noop,
  syncHud(S, wep, skin, atm, mut, vp) {
    drawCalls = S.drawCalls;
    if (S.corpses.length > maxCorpses) maxCorpses = S.corpses.length;
    if (S.corpses.length > 0) corpseFrames++;
    lastHud = { S, wep: wep.name, atm: atm.name, mut: mut.name, vpLen: vp.length };
    // жёсткая проверка: всё состояние обязано быть конечным
    const bad = (n) => !Number.isFinite(n);
    if (bad(S.x) || bad(S.y) || bad(S.z) || bad(S.hp) || bad(S.orbitalPct)) {
      throw new Error(`NaN/Inf в состоянии игрока: x=${S.x} y=${S.y} z=${S.z} hp=${S.hp}`);
    }
    for (const c of S.chickens) {
      if (bad(c.x) || bad(c.y) || bad(c.z) || bad(c.phase)) {
        throw new Error(`NaN/Inf у курицы типа ${c.type}: x=${c.x} y=${c.y} z=${c.z}`);
      }
    }
    for (const cp of S.corpses) {
      if (bad(cp.x) || bad(cp.z) || bad(cp.ttl)) throw new Error("NaN/Inf у трупа");
    }
  }
};

// ---------- boot engine ----------
const sandboxGlobals = { window, document, localStorage, performance, requestAnimationFrame, Image: FakeImage, alert, console };
globalThis.window = window;
globalThis.document = document;
globalThis.localStorage = localStorage;
globalThis.performance = performance;
globalThis.requestAnimationFrame = requestAnimationFrame;
globalThis.Image = FakeImage;
globalThis.alert = alert;

eval(SRC);

if (typeof window.StartChikenGunWebGL2 !== "function") {
  throw new Error("window.StartChikenGunWebGL2 не определён — движок не загрузился");
}
const engine = window.StartChikenGunWebGL2(opts);
console.log("[smoke] движок загружен, стартуем симуляцию…");

// ---------- симуляция ~2 минут боя ----------
const STEP = 1000 / 60;
const totalMs = 120000;
let firedAt = 500;
let rocketAt = 30000;
let railAt = 60000;
let grappleAt = 45000;
let orbitalAt = 90000;
for (let t = 0; t <= totalMs; t += STEP) {
  simNow = t;
  const cb = rafCb; rafCb = null;
  if (!cb) throw new Error("tick не запланировал следующий кадр (rAF потерян)");
  cb(t);
  if (t >= firedAt && firedAt > 0) {           // зажать ЛКМ (авто-огонь)
    fireBag(winHandlers, { type: "mousedown", button: 0, target: canvas, preventDefault: noop });
    firedAt = -1;
  }
  if (t >= rocketAt) {                          // [7] гранатомёт — проверка explodeAt
    fireBag(winHandlers, { type: "keydown", code: "Digit7", preventDefault: noop });
    fireBag(winHandlers, { type: "keydown", code: "KeyK", preventDefault: noop });
    rocketAt = Infinity;
  }
  if (t >= railAt) {                            // [8] рельсотрон — pierce
    fireBag(winHandlers, { type: "keydown", code: "Digit8", preventDefault: noop });
    fireBag(winHandlers, { type: "keydown", code: "KeyK", preventDefault: noop });
    railAt = Infinity;
  }
  if (t >= grappleAt) {                         // [Z] крюк
    fireBag(winHandlers, { type: "keydown", code: "KeyZ", preventDefault: noop });
    grappleAt = Infinity;
  }
  if (t >= orbitalAt) {                         // [X] орбитальный удар (читаем заряд из HUD)
    fireBag(winHandlers, { type: "keydown", code: "KeyX", preventDefault: noop });
    orbitalAt = Infinity;
  }
}

// ---------- отчёт ----------
const S = lastHud.S;
console.log("[smoke] Симуляция завершена без исключений ✔");
console.log(`        волна: ${S.wave} | счёт: ${S.score} | убийств: ${S.kills} | хэдшотов: ${S.headshots}`);
console.log(`        HP: ${Math.round(S.hp)}/${S.maxHp} | броня: ${Math.round(S.armor)} | ₵: ${S.credits}`);
console.log(`        живых кур: ${S.chickens.length} | трупов на арене: ${S.corpses.length} | орбита: ${Math.floor(S.orbitalPct)}%`);
console.log(`        draw calls в последнем кадре: ${drawCalls} (с учётом 2.4-оверхаула карты)`);
console.log(`        макс. трупов на экране одновременно: ${maxCorpses} | кадров с трупами: ${corpseFrames}`);
if (maxCorpses === 0) throw new Error("код трупов ни разу не отработал за 2 минуты боя");
console.log(`        сообщений ленты за бой: ${feed.length}`);
// все текстуры, которых движок пытался грузить, обязаны существовать на диске
const missingImgs = [...requestedSrcs].filter(src => !fs.existsSync(path.join(__dirname, "..", src)));
if (missingImgs.length) {
  throw new Error("движок запросил несуществующие текстуры:\n  " + missingImgs.join("\n  "));
}
console.log(`        текстур запрошено движком за бой: ${requestedSrcs.size} — все существуют ✔`);
if (!Number.isFinite(S.x) || !Number.isFinite(S.z)) throw new Error("игрок улетел в NaN");
if (S.kills === 0) console.warn("[smoke] ВНИМАНИЕ: за 2 минуты ни одного убийства — проверь fireWeapon/range");
console.log("[smoke] OK");

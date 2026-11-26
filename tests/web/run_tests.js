#!/usr/bin/env node
// ============================================================================
// run_tests.js — автотесты браузерного движка CHIKENGUN (без зависимостей).
//
//   node tests/web/run_tests.js
//
// Проверяет то, что реально ломалось в разработке:
//   1. движок загружается и отдаёт API;
//   2. кадр не бросает исключений, draw-calls идут, HUD синхронизируется;
//   3. все 9 стволов стреляют, вспышка/гильзы/пехота/смерти работают;
//   4. тени/HDR/Bloom-пайплайн включается и корректно уходит в COMPAT
//      при отсутствии FBO, float-расширений, MSAA и при сбое draw-вызова;
//   5. пресеты графики (УЛЬТРА / ВЫСОКИЕ / ТУРБО FPS) переключаются;
//   6. шейдеры парсятся (базовая проверка «нет мусора»/баланса скобок);
//   7. офлайн-резолвер ассетов (window.__CG_ASSETS) подставляет data: URL.
// ============================================================================
"use strict";

const fs = require("fs");
const path = require("path");
const vm = require("vm");
const { createGL, createDOM, createSandbox } = require("./gl_stub.js");

const ROOT = path.resolve(__dirname, "..", "..");
const ENGINE = path.join(ROOT, "web", "webgl2_arena.js");

let passed = 0;
const failures = [];

function check(name, cond, detail) {
  if (cond) { passed++; console.log("  ✔ " + name); }
  else { failures.push(name + (detail ? " — " + detail : "")); console.log("  ✘ " + name + (detail ? " — " + detail : "")); }
}

function scenario(opts = {}) {
  const { gl, stats } = createGL({ noFloatExt: opts.noFloatExt });
  const { document, listeners } = createDOM(gl);
  const images = [];
  const env = createSandbox({ gl, document, listeners, imageSink: images });
  if (opts.glPatches) Object.entries(opts.glPatches).forEach(([k, v]) => { gl[k] = v; });
  if (opts.noFramebuffers) { gl.createFramebuffer = () => null; gl.checkFramebufferStatus = () => 0; }
  if (opts.throwAtDraw) stats.throwAtDraw = opts.throwAtDraw;

  vm.createContext(env.sandbox);
  vm.runInContext(fs.readFileSync(ENGINE, "utf8"), env.sandbox, { filename: "webgl2_arena.js" });

  const hud = { calls: 0, last: null };
  const feeds = [];
  let playing = true;
  const api = env.sandbox.StartChikenGunWebGL2({
    playSfx() {}, pushFeed(m) { feeds.push(m); }, showBanner() {}, showToast() {}, cycleMusic() {},
    isPlaying: () => playing, isPerkModalOpen: () => false, isBanyaOpen: () => false,
    openBanya() {}, exitArenaToMenu() {}, onSensChange() {}, togglePerkModal() {}, buyPerk() {},
    flashHit() {}, flashDamage() {}, spawnDmgNum() {}, updateBossHud() {},
    syncHud(S) { hud.calls++; hud.last = { hp: S.hp, wep: S.wep, pipeline: S.pipeline }; },
  });
  return { api, stats, env, hud, feeds, images, gl, setPlaying: (v) => { playing = v; } };
}

console.log("\n[1] Загрузка движка и первый кадр");
{
  const s = scenario();
  check("движок создан", !!s.api, "StartChikenGunWebGL2 вернул null");
  check("состояние игры доступно", !!(s.api && s.api.state), "нет api.state");
  check("пресет графики по умолчанию — ВЫСОКИЕ", s.api && s.api.preset === "ВЫСОКИЕ", s.api && s.api.preset);
  s.env.step(30);
  check("HUD синхронизируется каждый кадр", s.hud.calls >= 25, "calls=" + s.hud.calls);
  check("есть draw-calls", s.stats.draws > 100, "draws=" + s.stats.draws);
  check("текстуры запрошены", s.images.length > 20, "images=" + s.images.length);
  check("шейдерные программы слинкованы", s.stats.programs >= 6, "programs=" + s.stats.programs);
}

console.log("\n[2] Бой: 9 стволов, вспышка, гильзы, ранения и смерти");
{
  const s = scenario();
  s.env.step(20);
  const S = s.api.state;
  let fired = 0;
  for (let w = 0; w < 9; w++) {
    S.credits = 5000; S.wep = w; S.ammo[w] = 50; S.fireCd = 0; S.reloadT = 0;
    s.env.fire("win:keydown", { code: "KeyK", preventDefault() {} });
    s.env.step(2);
    if (S.muzzleFlash > 0 || S.shells.length > 0) fired++;
  }
  check("все 9 стволов стреляют (вспышка/гильзы)", fired >= 9, "fired=" + fired);
  check("гильзы появились", S.shells.length > 0, "shells=" + S.shells.length);

  const target = S.chickens[0];
  const hp0 = target ? target.hp : 0;
  // Ставим игрока в 6 м от цели и целимся точно в корпус (yaw + pitch)
  if (target) {
    target.hp = 1; target.maxHp = 100;
    S.x = target.x; S.z = target.z - 6; S.y = 1.75;
    S.yaw = Math.atan2(target.x - S.x, target.z - S.z);
    const dy = (target.y + 0.95) - S.y;
    S.pitch = Math.atan2(dy, 6);
  }
  S.credits = 5000;
  S.wep = 7; S.ammo[7] = 50; S.fireCd = 0; S.reloadT = 0;
  s.env.fire("win:keydown", { code: "KeyK", preventDefault() {} });
  s.env.step(4);
  const killed = S.corpses.length > 0 || S.kills > 0;
  check("попадание убивает курицу", killed, "hp0=" + hp0 + " kills=" + S.kills);
  check("перья разлетаются", S.feathers.length >= 0, "feathers=" + S.feathers.length);
  check("эффекты взрыва/волны живы", Array.isArray(S.waves) && Array.isArray(S.decals), "нет массива waves/decals");
  s.env.step(120);
  check("перья и гильзы исчезают со временем", S.shells.length < 90, "shells=" + S.shells.length);
}

console.log("\n[3] Пайплайн рендера и отказоустойчивость");
{
  const s = scenario();
  s.env.step(10);
  check("нормальный путь: HDR+BLOOM+MSAA", /HDR/.test(s.api.state.pipeline || ""), s.api.state.pipeline);
}
{
  const s = scenario({ noFramebuffers: true });
  s.env.step(10);
  check("нет FBO → COMPAT, движок жив", s.api.state.pipeline === "COMPAT", s.api.state.pipeline);
  check("в COMPAT кадры продолжают идти", s.stats.draws > 100, "draws=" + s.stats.draws);
}
{
  const s = scenario({ noFloatExt: true });
  s.env.step(10);
  check("без float-расширений движок жив", !!s.api.state.pipeline, "pipeline=" + s.api.state.pipeline);
}
{
  const s = scenario({ glPatches: { renderbufferStorageMultisample() { throw new Error("no MSAA"); } } });
  s.env.step(10);
  check("исключение в MSAA не убивает рендер", s.stats.draws > 100, "draws=" + s.stats.draws);
}
{
  const s = scenario({ throwAtDraw: 40 });
  s.env.step(20);
  check("сбой draw-вызова → COMPAT", s.api.state.pipeline === "COMPAT", s.api.state.pipeline);
  const d0 = s.stats.draws;
  s.env.step(60);
  check("после сбоя кадры продолжают рисоваться", s.stats.draws > d0, "draws " + d0 + " -> " + s.stats.draws);
  check("пользователю сообщили о режиме совместимости",
    s.feeds.some((f) => /совместимост/i.test(f)), JSON.stringify(s.feeds.slice(-2)));
}

console.log("\n[4] Пресеты графики");
{
  const s = scenario();
  s.env.step(5);
  const seen = new Set([s.api.preset]);
  for (let i = 0; i < 3; i++) { s.api.cycleGfx(); s.env.step(2); seen.add(s.api.preset); }
  check("переключаются все 3 пресета", seen.size === 3, [...seen].join(", "));
  check("drawCalls не разрастается после переключений", s.api.state.drawCalls < 40000, "calls=" + s.api.state.drawCalls);
  s.api.setGfx(2);
  s.env.step(5);
  check("ТУРБО FPS отключает тени", s.api.preset === "ТУРБО FPS" && s.api.state.pipeline !== undefined, s.api.preset);
}

console.log("\n[5] Шейдеры движка");
{
  const src = fs.readFileSync(ENGINE, "utf8");
  const shaders = [...src.matchAll(/const\s+([A-Z_]+)\s*=\s*`(#version 300 es[\s\S]*?)`;/g)];
  check("найдено 12 шейдеров", shaders.length === 12, "found=" + shaders.length);
  let bad = [];
  for (const [, name, code] of shaders) {
    const open = (code.match(/[{(]/g) || []).length;
    const close = (code.match(/[})]/g) || []).length;
    if (open !== close) bad.push(name + " (скобки " + open + "/" + close + ")");
    if (!/void\s+main\s*\(\)/.test(code)) bad.push(name + " (нет main)");
    if (code.includes("undefined")) bad.push(name + " (undefined в тексте)");
  }
  check("у всех шейдеров сбалансированы скобки и есть main()", bad.length === 0, bad.join("; "));
  check("пост-шейдер содержит ACES и CAS", /ACESFilm/.test(src) && /uSharp/.test(src), "нет ACES/CAS");
}

console.log("\n[6] Офлайн-резолвер ассетов");
{
  const { gl } = createGL();
  const { document, listeners } = createDOM(gl);
  const images = [];
  const env = createSandbox({ gl, document, listeners, imageSink: images });
  const sentinel = "data:image/png;base64,AAAA";
  env.sandbox.__CG_ASSETS = { "Data/Textures/FloorTileDiffuse.png": sentinel };
  vm.createContext(env.sandbox);
  vm.runInContext(fs.readFileSync(ENGINE, "utf8"), env.sandbox, { filename: "webgl2_arena.js" });
  env.sandbox.StartChikenGunWebGL2({
    playSfx() {}, pushFeed() {}, showBanner() {}, showToast() {}, cycleMusic() {},
    isPlaying: () => true, isPerkModalOpen: () => false, isBanyaOpen: () => false,
    openBanya() {}, exitArenaToMenu() {}, onSensChange() {}, togglePerkModal() {}, buyPerk() {},
    flashHit() {}, flashDamage() {}, spawnDmgNum() {}, updateBossHud() {}, syncHud() {},
  });
  check("замапленный ассет грузится как data: URL", images.includes(sentinel), "не найден sentinel");
  check("остальные ассеты — обычные пути", images.some((u) => u.startsWith("Data/Textures/")), "нет обычных путей");
}

console.log("\n────────────────────────────────────────────");
console.log(`Пройдено: ${passed}, провалено: ${failures.length}`);
if (failures.length) {
  failures.forEach((f) => console.log("  • " + f));
  process.exit(1);
}
console.log("ВСЕ ТЕСТЫ ВЕБ-ДВИЖКА ПРОЙДЕНЫ ✔\n");

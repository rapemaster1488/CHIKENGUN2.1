#!/usr/bin/env node
/* ============================================================================
 * CHIKENGUN 2.4 — headless smoke/regression test for the browser WebGL2 arena
 *
 * Гоняет index.html + web/webgl2_arena.js в jsdom с замоканным WebGL2,
 * 2D-контекстом, аудио и детерминированным PRNG. Без GPU и без скачивания
 * браузера — годится для CI.
 *
 * Использование:
 *   npm install --no-save jsdom
 *   node tools/web_smoke_test.js [--verbose]
 * ========================================================================== */
"use strict";

const fs = require("fs");
const path = require("path");

let JSDOM;
try {
  ({ JSDOM } = require("jsdom"));
} catch (_) {
  console.error("✖ Не найден пакет jsdom. Установите: npm install --no-save jsdom");
  process.exit(2);
}

const ROOT = path.resolve(__dirname, "..");
const VERBOSE = process.argv.includes("--verbose");
const results = [];
let failures = 0;

function check(name, fn) {
  try {
    fn();
    results.push(`  ✔ ${name}`);
    if (VERBOSE) console.log(`  ✔ ${name}`);
  } catch (e) {
    failures++;
    results.push(`  ✖ ${name}\n      ${e && e.message}`);
    process.stderr.write(`  ✖ ${name}\n      ${e && e.message}\n`);
  }
}
const assert = (cond, msg) => { if (!cond) throw new Error(msg || "assertion failed"); };
const approx = (a, b, eps, msg) => {
  if (Math.abs(a - b) > (eps === undefined ? 1e-3 : eps)) throw new Error(`${msg || "approx"}: ${a} != ${b}`);
};
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ---------- WebGL2 mock ----------
function makeGL(canvas) {
  const consts = {
    VERTEX_SHADER: 1, FRAGMENT_SHADER: 2, COMPILE_STATUS: 3, LINK_STATUS: 4,
    ARRAY_BUFFER: 5, ELEMENT_ARRAY_BUFFER: 6, STATIC_DRAW: 7, FLOAT: 8, DYNAMIC_DRAW: 9,
    TRIANGLES: 10, UNSIGNED_SHORT: 11, UNSIGNED_BYTE: 12, UNSIGNED_INT: 13,
    COLOR_BUFFER_BIT: 16, DEPTH_BUFFER_BIT: 32, DEPTH_TEST: 14, CULL_FACE: 15,
    LEQUAL: 17, LESS: 18, BLEND: 19, SRC_ALPHA: 20, ONE_MINUS_SRC_ALPHA: 21,
    TEXTURE_2D: 22, TEXTURE_CUBE_MAP: 23, RGBA: 24, TEXTURE0: 25, LINEAR: 26,
    LINEAR_MIPMAP_LINEAR: 27, REPEAT: 28, TEXTURE_WRAP_S: 29, TEXTURE_WRAP_T: 30,
    TEXTURE_MIN_FILTER: 31, TEXTURE_MAG_FILTER: 32, FRAMEBUFFER: 33,
  };
  let next = 0x10000;
  const calls = Object.create(null);
  const api = {
    __calls: calls, canvas,
    getParameter: () => 16,
    getShaderParameter: () => true,
    getProgramParameter: () => true,
    getShaderInfoLog: () => "",
    getProgramInfoLog: () => "",
    getExtension: (name) =>
      name === "EXT_texture_filter_anisotropic"
        ? { MAX_TEXTURE_MAX_ANISOTROPY_EXT: 0x5001, TEXTURE_MAX_ANISOTROPY_EXT: 0x5002 }
        : null,
    getUniformLocation: (prog, name) => ({ prog, name }),
    getAttribLocation: () => 0,
    checkFramebufferStatus: () => 0x8cd5,
  };
  return new Proxy(api, {
    get(t, prop) {
      if (typeof prop !== "string") return undefined;
      if (prop in t) return t[prop];
      if (prop in consts) return consts[prop];
      if (/^[A-Z0-9_]+$/.test(prop)) return (consts[prop] = next++);
      return (...args) => {
        calls[prop] = (calls[prop] || 0) + 1;
        return { __globject: prop };
      };
    },
    set(t, prop, v) { t[prop] = v; return true; },
  });
}

function make2D() {
  const state = { canvas: { width: 144, height: 144 }, fillStyle: "", strokeStyle: "" };
  return new Proxy(state, {
    get(t, prop) {
      if (prop in t) return t[prop];
      if (prop === "measureText") return () => ({ width: 10 });
      return () => {};
    },
    set(t, prop, v) { t[prop] = v; return true; },
  });
}

// ---------- deterministic PRNG ----------
function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

(async () => {
  const html = fs.readFileSync(path.join(ROOT, "index.html"), "utf8");
  const arenaJs = fs.readFileSync(path.join(ROOT, "web", "webgl2_arena.js"), "utf8");
  const htmlNoExt = html.replace(/<script src="web\/webgl2_arena\.js"><\/script>/, "");

  const dom = new JSDOM(htmlNoExt, { runScripts: "outside-only", url: "http://localhost/index.html" });
  const { window } = dom;
  const { document } = window;

  // --- browser APIs jsdom lacks ---
  const rafQueue = [];
  window.requestAnimationFrame = (cb) => { rafQueue.push(cb); return rafQueue.length; };
  window.cancelAnimationFrame = () => {};
  let fakeNow = 0;
  window.performance.now = () => fakeNow;
  window.HTMLCanvasElement.prototype.getContext = function (kind) {
    if (kind === "webgl2" || kind === "webgl") {
      if (!this.__gl) this.__gl = makeGL(this);
      return this.__gl;
    }
    if (kind === "2d") {
      if (!this.__ctx2d) this.__ctx2d = make2D();
      return this.__ctx2d;
    }
    return null;
  };
  const FakeAudio = class {
    constructor(src) { this.src = src; this.volume = 1; this.loop = false; }
    play() { return Promise.resolve(); }
    pause() {}
    addEventListener() {}
    removeEventListener() {}
  };
  window.Audio = FakeAudio;
  window.HTMLMediaElement.prototype.play = function () { return Promise.resolve(); };
  window.HTMLMediaElement.prototype.pause = function () {};
  window.AudioContext = function () {
    return {
      state: "running", currentTime: 0, sampleRate: 48000, destination: {},
      resume: () => Promise.resolve(),
      createAnalyser: () => ({ fftSize: 256, connect() {}, getByteFrequencyData(a) { a.fill(0); } }),
      createGain: () => ({ gain: { value: 1, setValueAtTime() {}, linearRampToValueAtTime() {} }, connect() {}, disconnect() {} }),
      createBufferSource: () => ({ buffer: null, connect() {}, start() {}, stop() {}, disconnect() {} }),
      createMediaElementSource: () => ({ connect() {} }),
      decodeAudioData: () => Promise.resolve({ duration: 0.2 }),
    };
  };
  window.alert = (m) => { throw new Error("alert(): " + m); };
  let pointerLockEl = null;
  Object.defineProperty(document, "pointerLockElement", { get: () => pointerLockEl, configurable: true });
  window.HTMLElement.prototype.requestPointerLock = function () {
    pointerLockEl = this;
    document.dispatchEvent(new window.Event("pointerlockchange"));
  };
  document.exitPointerLock = function () {
    pointerLockEl = null;
    document.dispatchEvent(new window.Event("pointerlockchange"));
  };
  window.Math.random = mulberry32(0xc0ffee);

  const errors = [];
  const consoleErrors = [];
  window.addEventListener("error", (e) => errors.push(String((e.error && e.error.stack) || e.message)));
  const realConsoleError = console.error;
  console.error = (...a) => { consoleErrors.push(a.join(" ")); if (VERBOSE) realConsoleError(...a); };

  // --- execute page scripts in order ---
  window.eval(arenaJs);
  Array.from(document.querySelectorAll("script"))
    .filter((s) => !s.src && s.textContent.trim())
    .forEach((s) => window.eval(s.textContent));

  function step(frames, dtMs) {
    const stepMs = dtMs === undefined ? 16.6667 : dtMs;
    for (let i = 0; i < frames; i++) {
      fakeNow += stepMs;
      const cbs = rafQueue.splice(0, rafQueue.length);
      for (const cb of cbs) cb(fakeNow);
    }
  }
  const key = (code, type) => window.dispatchEvent(new window.KeyboardEvent(type, { code, bubbles: true, cancelable: true }));
  const press = (code) => { key(code, "keydown"); key(code, "keyup"); };

  console.log("CHIKENGUN 2.4 — headless WebGL2 arena smoke test");
  console.log("  jsdom:", require("jsdom/package.json").version, "| node:", process.version);

  step(3);
  const engine = window.__CHIKENGUN_ENGINE__;

  // ==========================================================================
  // 1. Boot
  // ==========================================================================
  check("движок арены стартует без исключений", () => {
    assert(engine && engine.state, "window.__CHIKENGUN_ENGINE__ не создан");
    assert(errors.length === 0, "window.onerror: " + errors.slice(0, 2).join(" | "));
  });
  check("WebGL2: шейдеры скомпилированы, сцена рисуется", () => {
    const gl = document.getElementById("glCanvas").__gl;
    assert(gl, "нет gl-контекста");
    assert(gl.__calls.compileShader >= 4, "шейдеров: " + gl.__calls.compileShader);
    assert(gl.__calls.drawElements > 0, "draw call'ов: " + gl.__calls.drawElements);
    assert(gl.__calls.linkProgram >= 2, "программ: " + gl.__calls.linkProgram);
  });
  check("2.4: инстансинг режет draw call'и (не 1 вызов на проп)", () => {
    const gl = document.getElementById("glCanvas").__gl;
    const instanced = gl.__calls.drawElementsInstanced || 0;
    const plain = gl.__calls.drawElements || 0;
    assert(instanced > 0, "drawElementsInstanced не вызывался: " + instanced);
    assert(plain <= instanced, `plain ${plain} > instanced ${instanced}`);
    const st = window.__CHIKENGUN_ENGINE__.state;
    assert(st.drawCalls < 900, "draw call'ов за кадр слишком много: " + st.drawCalls);
  });
  check("меню собрано: 12 достижений, 7 PBR-наборов, аудио-стенд", () => {
    assert(document.querySelectorAll("#achGrid > *").length === 12, "достижений: " + document.querySelectorAll("#achGrid > *").length);
    assert(document.querySelectorAll("#texGrid > *").length === 7, "PBR-карточек: " + document.querySelectorAll("#texGrid > *").length);
    assert(document.querySelectorAll("#sfxGrid > *").length >= 20, "аудио-кнопок: " + document.querySelectorAll("#sfxGrid > *").length);
  });

  await sleep(1600); // boot-чеклист страницы работает на setTimeout
  check("экран загрузки скрывается после старта", () => {
    assert(document.getElementById("bootScreen").classList.contains("hidden"), "bootScreen всё ещё видим");
  });

  const S = engine.state;

  // ==========================================================================
  // 2. Enter arena & movement
  // ==========================================================================
  document.getElementById("startArenaBtn").click();
  step(3);
  check("вход в арену: бой включён, HUD показан, камера у игрока", () => {
    assert(document.body.classList.contains("playing"), "body.playing не выставлен");
    assert(window.getComputedStyle(document.getElementById("hud")).display === "block", "HUD скрыт");
    approx(S.y, 1.75, 0.05, "высота глаз");
    assert(S.onGround === true, "onGround=false");
  });

  const pos0 = [S.x, S.z];
  key("KeyW", "keydown");
  step(30);
  key("KeyW", "keyup");
  check("WASD двигает игрока", () => {
    const moved = Math.hypot(S.x - pos0[0], S.z - pos0[1]);
    assert(moved > 0.5, "игрок не сдвинулся (" + moved.toFixed(2) + "м)");
  });

  // ==========================================================================
  // 3. Weapons: switch, fire, accuracy, reload
  // ==========================================================================
  press("Digit1");
  step(2);
  check("переключение на Пистолет [1]", () => {
    assert(S.wep === 0, "wep=" + S.wep);
    assert(S.ammo[0] === 12, "ammo=" + S.ammo[0]);
  });
  const ammo0 = S.ammo[0], fired0 = S.shotsFired;
  S.fireCd = 0;
  window.dispatchEvent(new window.MouseEvent("mousedown", { button: 0, bubbles: true }));
  window.dispatchEvent(new window.MouseEvent("mouseup", { button: 0, bubbles: true }));
  step(2);
  check("выстрел расходует патрон и засчитывается в статистику", () => {
    assert(S.ammo[0] === ammo0 - 1, `ammo ${S.ammo[0]} (было ${ammo0})`);
    assert(S.shotsFired === fired0 + 1, "shotsFired=" + S.shotsFired);
  });
  check("серія попаданий поднимает точность, промахи её не ломают", () => {
    const acc = S.shotsFired > 0 ? S.shotsHit / S.shotsFired : 0;
    assert(acc >= 0 && acc <= 1, "точность вне [0..1]: " + acc);
  });
  S.ammo[0] = 1;
  S.fireCd = 0;
  press("KeyR");
  step(140);
  check("R перезаряжает до размера магазина", () => {
    assert(S.ammo[0] === 12, "ammo после перезарядки: " + S.ammo[0]);
    assert(S.reloadT <= 0, "reloadT не сброшен");
  });

  // ==========================================================================
  // 4. Abilities
  // ==========================================================================
  S.credits = 500;
  step(2);
  press("KeyG");
  step(2);
  check("граната [G] летит в мир", () => assert(S.grenades.length === 1, "grenades=" + S.grenades.length));
  press("KeyT");
  step(2);
  check("турель [T] разворачивается за 100₵", () => {
    assert(S.turrets.length === 1, "turrets=" + S.turrets.length);
    assert(S.credits >= 300 && S.credits <= 500, "credits=" + S.credits);
  });
  S.grappleCd = 0;
  S.x = 0; S.z = 40; S.y = 1.75; S.yaw = 0; S.pitch = 0; // лицом к внешней стене (z=48)
  step(1);
  press("KeyZ");
  step(2);
  check("грэппл-крюк [Z] цепляется за геометрию", () => assert(S.grappleOn === true, "grappleOn=false"));
  S.orbitalPct = 100;
  press("KeyX");
  step(2);
  check("орбитальный удар [X] создаёт столб и сбрасывает заряд", () => {
    assert(S.orbitalStrikes.length === 1, "strikes=" + S.orbitalStrikes.length);
    assert(S.orbitalPct < 100, "заряд не сброшен: " + S.orbitalPct);
  });
  const dash0 = S.dashCharges;
  press("AltLeft");
  step(2);
  check("рывок [ALT] тратит заряд и запускается", () => {
    assert(S.dashCharges === dash0 - 1, `заряды ${dash0} -> ${S.dashCharges}`);
    assert(S.dashT > 0 || S.dashCd > 0, "dash не сработал");
  });

  // ==========================================================================
  // 5. Waves & combat
  // ==========================================================================
  check("волна спавнит кур вне геометрии", () => {
    assert(S.chickens.length >= 4, "кур: " + S.chickens.length);
    S.chickens.forEach((c) => {
      assert(!(Math.abs(c.x) > 46 || Math.abs(c.z) > 46), `курица за ареной: ${c.x.toFixed(1)},${c.z.toFixed(1)}`);
    });
  });
  const wave0 = S.wave;
  S.orbitalPct = 100;
  S.x = 0; S.z = 0; S.y = 1.75; // в центр арены, чтобы удар накрыл спавн
  step(1);
  press("KeyX");
  step(6);
  check("куры умирают от урона и дают очки", () => {
    assert(S.kills > 0, "kills=0");
    assert(S.score > 0, "score=0");
  });
  S.chickens.forEach((c) => { c.hp = -1; });
  step(90); // 1.1с паузы между волнами
  check("зачистка волны запускает следующую", () => {
    assert(S.wave === wave0 + 1, `волна ${S.wave}, ожидалась ${wave0 + 1}`);
    assert(S.chickens.length > 0, "новая волна пустая");
  });

  // ==========================================================================
  // 6. Damage, death, restart
  // ==========================================================================
  check("броня съедает часть урона", () => {
    const hp0 = S.hp, ar0 = S.armor;
    S.invulnT = 0;
    engine.hurtPlayer(40);
    assert(S.armor < ar0, `armor ${ar0} -> ${S.armor}`);
    assert(S.hp < hp0, `hp ${hp0} -> ${S.hp}`);
  });
  check("смерть ведёт к GAME OVER, а не к бесконечному респавну", () => {
    let guard = 0;
    while (!S.gameOver && guard++ < 40) { S.invulnT = 0; engine.hurtPlayer(200); }
    assert(S.gameOver === true, "gameOver не наступил");
    assert(document.getElementById("gameOverScreen").classList.contains("open"), "экран GAME OVER не показан");
  });
  check("статистика рейда попадает на экран смерти", () => {
    assert(Number(document.getElementById("goScore").textContent) >= 0, "goScore пуст");
    assert(document.getElementById("goAcc").textContent.includes("%"), "точность не показана");
    assert(document.getElementById("goKills").textContent === String(S.kills), "убийства не совпадают");
  });
  check("Enter перезапускает рейд из GAME OVER", () => {
    press("Enter");
    step(3);
    assert(S.gameOver === false, "gameOver не сброшен");
    assert(S.hp === S.maxHp, "HP не восстановлен: " + S.hp);
    assert(S.armor === 50, "броня не восстановлена: " + S.armor);
    assert(S.wave === 1, "волна не сброшена: " + S.wave);
    assert(S.score === 0, "счёт не сброшен: " + S.score);
    assert(!document.getElementById("gameOverScreen").classList.contains("open"), "экран GAME OVER не скрыт");
    assert(document.body.classList.contains("playing"), "бой не возобновлён");
  });

  // ==========================================================================
  // 7. Pause
  // ==========================================================================
  check("P ставит паузу и показывает меню паузы", () => {
    press("KeyP");
    step(2);
    assert(S.paused === true, "paused=false");
    assert(document.getElementById("pauseScreen").classList.contains("open"), "экран паузы не показан");
  });
  check("на паузе игрок и куры не двигаются", () => {
    const p0 = [S.x, S.z], hp0 = S.hp;
    key("KeyW", "keydown");
    step(40);
    key("KeyW", "keyup");
    assert(Math.hypot(S.x - p0[0], S.z - p0[1]) < 0.05, "игрок двигается на паузе");
    assert(S.hp === hp0, "HP меняется на паузе");
  });
  check("кнопка «Продолжить» снимает паузу", () => {
    document.getElementById("resumeBtn").click();
    step(2);
    assert(S.paused === false, "пауза не снята");
    assert(!document.getElementById("pauseScreen").classList.contains("open"), "экран паузы не скрыт");
  });

  // ==========================================================================
  // 8. Settings / perks / visuals
  // ==========================================================================
  check("F3 включает телеметрию, F8 меняет атмосферу", () => {
    const atm0 = S.atmIdx;
    press("F3"); step(1);
    assert(S.showFps === true, "F3 не включил телеметрию");
    press("F8"); step(1);
    assert(S.atmIdx !== atm0, "F8 не сменила атмосферу");
  });
  check("B открывает/закрывает лабораторию перков", () => {
    press("KeyB"); step(1);
    assert(document.getElementById("perkModal").classList.contains("open"), "модалка не открылась");
    press("KeyB"); step(1);
    assert(!document.getElementById("perkModal").classList.contains("open"), "модалка не закрылась");
  });
  check("покупка перка списывает кредиты и повышает уровень", () => {
    S.credits = 1000;
    const lvl0 = S.perks[2];
    window.__CHIKENGUN_BUY_PERK__(2);
    assert(S.perks[2] === lvl0 + 1, "перк не куплен");
    assert(S.credits < 1000, "кредиты не списаны");
  });
  check("экипировка перков реально меняет статы игрока", () => {
    const crit0 = S.critChance;
    S.perks[2] = 5;
    engine.applyPerkStats();
    assert(S.critChance > crit0, "крит-шанс не вырос");
    S.perks[0] = 5;
    engine.applyPerkStats();
    assert(S.regen > 0, "регенерация не включилась");
    S.perks = [0, 0, 0, 0];
    engine.applyPerkStats();
  });
  check("HUD показывает точность, комбо и остаток волны", () => {
    assert(document.getElementById("hudStats").textContent.includes("ТОЧНОСТЬ"), "нет строки точности");
    assert(document.getElementById("hudStats").textContent.includes("X-KILL"), "нет строки комбо");
  });

  // ==========================================================================
  // 9. Stability: 60 seconds of simulation
  // ==========================================================================
  check("60 секунд боя: без исключений, без утечек DOM", () => {
    document.getElementById("startArenaBtn").click();
    step(1);
    const nodes0 = document.querySelectorAll("*").length;
    for (let i = 0; i < 3600; i++) {
      if (i % 90 === 0) { S.invulnT = 0; engine.hurtPlayer(1); }
      if (i % 300 === 0) { press("Digit2"); press("KeyB"); press("KeyB"); }
      if (i % 600 === 0) { press("KeyZ"); press("KeyV"); press("KeyT"); }
      step(1);
    }
    const nodes1 = document.querySelectorAll("*").length;
    assert(errors.length === 0, "исключения: " + errors.slice(0, 2).join(" | "));
    assert(consoleErrors.length === 0, "console.error: " + consoleErrors.slice(0, 2).join(" | "));
    assert(nodes1 - nodes0 < 250, `DOM вырос на ${nodes1 - nodes0} узлов`);
    assert(S.chickens.length < 60, "куры копятся: " + S.chickens.length);
    assert(S.particles.length <= 460, "частицы копятся: " + S.particles.length);
  });

  // ==========================================================================
  // Summary
  // ==========================================================================
  console.log(results.join("\n"));
  if (failures) {
    console.error(`\n✖ ПРОВАЛЕНО: ${failures} из ${results.length}`);
    process.exit(1);
  }
  console.log(`\n✔ ВСЕ ${results.length} ПРОВЕРОК ПРОЙДЕНЫ (headless WebGL2 arena)`);
  process.exit(0);
})().catch((e) => {
  console.error("FATAL", e);
  process.exit(1);
});

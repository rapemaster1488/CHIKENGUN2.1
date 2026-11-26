// ============================================================================
// gl_stub.js — минимальный эмулятор WebGL2 + DOM для тестов движка.
// Ноль внешних зависимостей (важно для CI): только Node.js.
// Позволяет прогонять web/webgl2_arena.js без настоящей видеокарты и браузера,
// ловить ошибки шейдеров/пайплайна и проверять fallback-режимы.
// ============================================================================

function createGL(options = {}) {
  const stats = { draws: 0, programs: 0, textures: 0, framebuffers: 0, shaderErrors: [], throwAtDraw: 0 };
  let idc = 1;
  const C = {};
  const names = (
    "DEPTH_TEST CULL_FACE BLEND COLOR_BUFFER_BIT DEPTH_BUFFER_BIT FRAMEBUFFER " +
    "TEXTURE_2D TEXTURE_CUBE_MAP RGBA RGBA8 RGBA16F HALF_FLOAT UNSIGNED_BYTE UNSIGNED_INT FLOAT " +
    "DEPTH_COMPONENT DEPTH_COMPONENT24 COLOR_ATTACHMENT0 DEPTH_ATTACHMENT RENDERBUFFER " +
    "FRAMEBUFFER_COMPLETE NEAREST LINEAR LINEAR_MIPMAP_LINEAR CLAMP_TO_EDGE REPEAT TRIANGLES " +
    "UNSIGNED_SHORT ARRAY_BUFFER ELEMENT_ARRAY_BUFFER STATIC_DRAW VERTEX_SHADER FRAGMENT_SHADER " +
    "COMPILE_STATUS LINK_STATUS LEQUAL LESS ALWAYS NONE READ_FRAMEBUFFER DRAW_FRAMEBUFFER " +
    "TEXTURE0 TEXTURE1 TEXTURE2 TEXTURE3 TEXTURE4 TEXTURE5 VERSION MAX_TEXTURE_SIZE " +
    "MAX_TEXTURE_MAX_ANISOTROPY_EXT TEXTURE_MAX_ANISOTROPY_EXT TEXTURE_WRAP_S TEXTURE_WRAP_T " +
    "TEXTURE_MIN_FILTER TEXTURE_MAG_FILTER TEXTURE_CUBE_MAP_POSITIVE_X TEXTURE_CUBE_MAP_NEGATIVE_X " +
    "TEXTURE_CUBE_MAP_POSITIVE_Y TEXTURE_CUBE_MAP_NEGATIVE_Y TEXTURE_CUBE_MAP_POSITIVE_Z " +
    "TEXTURE_CUBE_MAP_NEGATIVE_Z FRONT BACK SCISSOR_TEST MAX_SAMPLES"
  ).split(" ");
  names.forEach((n, i) => (C[n] = 0x1000 + i));

  const overrides = {};
  const api = {
    ...C,
    createShader: () => ({ id: idc++ }),
    createProgram: () => { stats.programs++; return { id: idc++ }; },
    createBuffer: () => ({ id: idc++ }),
    createVertexArray: () => ({ id: idc++ }),
    createTexture: () => { stats.textures++; return { id: idc++ }; },
    createFramebuffer: () => { stats.framebuffers++; return { id: idc++ }; },
    createRenderbuffer: () => ({ id: idc++ }),
    deleteShader() {}, deleteProgram() {}, deleteBuffer() {}, deleteVertexArray() {},
    deleteTexture() {}, deleteFramebuffer() {}, deleteRenderbuffer() {},
    shaderSource() {}, compileShader() {},
    getShaderParameter: () => true,
    getShaderInfoLog: () => "",
    getProgramParameter: () => true,
    getProgramInfoLog: () => "",
    getUniformLocation: (_p, n) => ({ loc: n }),
    getAttribLocation: () => 0,
    checkFramebufferStatus: () => C.FRAMEBUFFER_COMPLETE,
    getParameter: (p) => {
      if (p === C.MAX_TEXTURE_SIZE) return 4096;
      if (p === C.MAX_SAMPLES) return 4;
      return 4096;
    },
    getExtension: (name) => {
      if (options.noFloatExt) return null;
      if (name === "EXT_color_buffer_float" || name === "OES_texture_float_linear") {
        return { TEXTURE_MAX_ANISOTROPY_EXT: C.TEXTURE_MAX_ANISOTROPY_EXT,
                 MAX_TEXTURE_MAX_ANISOTROPY_EXT: C.MAX_TEXTURE_MAX_ANISOTROPY_EXT };
      }
      if (name === "EXT_texture_filter_anisotropic") {
        return { TEXTURE_MAX_ANISOTROPY_EXT: C.TEXTURE_MAX_ANISOTROPY_EXT,
                 MAX_TEXTURE_MAX_ANISOTROPY_EXT: C.MAX_TEXTURE_MAX_ANISOTROPY_EXT };
      }
      return null;
    },
    drawElements: () => {
      stats.draws++;
      if (stats.throwAtDraw && stats.draws === stats.throwAtDraw) {
        throw new Error("stub: simulated GPU fault");
      }
    },
    drawArrays() {},
    bindVertexArray() {}, bindBuffer() {}, bufferData() {},
    enableVertexAttribArray() {}, vertexAttribPointer() {},
    bindTexture() {}, texImage2D() {}, texParameteri() {}, texParameterf() {},
    generateMipmap() {}, activeTexture() {},
    bindFramebuffer() {}, framebufferTexture2D() {}, framebufferRenderbuffer() {},
    bindRenderbuffer() {}, renderbufferStorage() {}, renderbufferStorageMultisample() {},
    blitFramebuffer() {}, drawBuffers() {}, readBuffer() {},
    attachShader() {}, detachShader() {}, linkProgram() {}, validateProgram() {}, bindAttribLocation() {},
    useProgram() {}, uniform1i() {}, uniform1f() {}, uniform2f() {}, uniform3f() {}, uniform4f() {},
    uniform3fv() {}, uniformMatrix4fv() {},
    viewport() {}, clear() {}, clearColor() {}, clearDepth() {}, depthFunc() {}, depthMask() {},
    enable() {}, disable() {}, cullFace() {}, blendFunc() {}, frontFace() {}, colorMask() {},
    finish() {}, flush() {}, pixelStorei() {},
  };

  // allow tests to break specific entry points (unsupported hardware simulation)
  const gl = new Proxy(api, {
    get(target, prop) {
      if (prop in overrides) return overrides[prop];
      const v = target[prop];
      if (v === undefined && typeof prop === "string" && /^[A-Z0-9_]+$/.test(prop)) {
        target[prop] = 0x9000 + Object.keys(target).length;
        return target[prop];
      }
      return v;
    },
    set(target, prop, value) { overrides[prop] = value; return true; },
  });

  return { gl, stats, overrides };
}

// ---- minimal DOM ----------------------------------------------------------
function createDOM(gl) {
  const listeners = {};
  const elements = {};

  function makeEl(tag = "div") {
    const el = {
      tagName: tag.toUpperCase(), style: {}, dataset: {}, children: [], textContent: "",
      innerHTML: "", className: "", width: 1280, height: 720,
      classList: {
        _s: new Set(),
        add(c) { this._s.add(c); }, remove(c) { this._s.delete(c); },
        contains(c) { return this._s.has(c); },
        toggle(c) { this._s.has(c) ? this._s.delete(c) : this._s.add(c); },
      },
      appendChild(c) { this.children.push(c); return c; },
      removeChild() {}, remove() {},
      setAttribute(k, v) { this[k] = v; }, getAttribute(k) { return this[k] ?? null; },
      addEventListener(t, f) { (listeners[tag + ":" + t] = listeners[tag + ":" + t] || []).push(f); },
      removeEventListener() {},
      getContext(kind) {
        if (kind === "webgl2") return gl;
        return {
          clearRect() {}, beginPath() {}, arc() {}, stroke() {}, fillRect() {}, fillText() {},
          measureText: () => ({ width: 10 }), createLinearGradient: () => ({ addColorStop() {} }),
          save() {}, restore() {}, translate() {}, rotate() {}, drawImage() {},
        };
      },
      requestPointerLock() { return Promise.resolve(); },
      getBoundingClientRect() { return { left: 0, top: 0, width: 1280, height: 720 }; },
    };
    el.parentNode = { removeChild() {}, appendChild() {} };
    return el;
  }

  const document = {
    getElementById(id) { return elements[id] || (elements[id] = makeEl()); },
    createElement: (t) => makeEl(t),
    addEventListener(t, f) { (listeners["doc:" + t] = listeners["doc:" + t] || []).push(f); },
    querySelectorAll: () => [],
    body: makeEl("body"),
    documentElement: makeEl("html"),
    pointerLockElement: null,
    exitPointerLock() {},
    createTextNode: (t) => ({ text: t }),
  };

  return { document, elements, listeners };
}

// ---- sandbox with fake timers/RAF ----------------------------------------
function createSandbox({ gl, document, listeners, imageSink }) {
  let rafQueue = [];
  let now = 0;

  class FakeImage {
    constructor() { this.onload = null; this._src = ""; }
    set src(v) {
      this._src = v;
      if (imageSink) imageSink.push(String(v));
      if (this.onload) setTimeout(() => this.onload(), 0);
    }
    get src() { return this._src; }
  }

  const sandbox = {
    console: { log() {}, warn() {}, error() {}, info() {} },
    document,
    performance: { now: () => now },
    requestAnimationFrame: (f) => { rafQueue.push(f); return rafQueue.length; },
    cancelAnimationFrame() {},
    localStorage: {
      _d: {},
      getItem(k) { return this._d[k] ?? null; },
      setItem(k, v) { this._d[k] = String(v); },
      removeItem(k) { delete this._d[k]; },
    },
    Image: FakeImage,
    Audio: class { constructor() { this.volume = 1; } play() { return Promise.resolve(); } pause() {} },
    AudioContext: class {
      createAnalyser() { return { connect() {}, frequencyBinCount: 128, getByteFrequencyData() {}, getByteTimeDomainData() {} }; }
      createMediaElementSource() { return { connect() {} }; }
      get destination() { return {}; }
      get state() { return "running"; }
      resume() {}
    },
    setTimeout, clearTimeout, setInterval: () => 0, clearInterval() {},
    innerWidth: 1280, innerHeight: 720, devicePixelRatio: 2,
    addEventListener(t, f) { (listeners["win:" + t] = listeners["win:" + t] || []).push(f); },
    removeEventListener() {},
    location: { protocol: "http:", href: "http://localhost/" },
    navigator: { userAgent: "node-test" },
    alert() {},
    Math, Date, JSON, Number, String, Boolean, Array, Object, Error, RegExp,
    Float32Array, Uint16Array, Uint8Array, Uint32Array,
    parseInt, parseFloat, isNaN, isFinite, Promise, Proxy, Reflect, Symbol, Map, Set,
  };
  sandbox.window = sandbox;
  sandbox.self = sandbox;
  sandbox.globalThis = sandbox;

  return {
    sandbox,
    step(frames = 1, dtMs = 16.7) {
      for (let i = 0; i < frames; i++) {
        const q = rafQueue;
        rafQueue = [];
        now += dtMs;
        q.forEach((f) => {
          try { f(now); } catch (e) { /* engine must never throw out of RAF */ }
        });
      }
      return now;
    },
    now: () => now,
    fire(kind, ev) {
      const list = listeners[kind] || [];
      list.forEach((f) => f(ev));
    },
  };
}

module.exports = { createGL, createDOM, createSandbox };

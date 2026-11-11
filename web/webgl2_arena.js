// ============================================================================
// CHIKENGUN 2.3 — Hardware-Accelerated WebGL2 4K PBR 3D Arena Engine
// Zero external dependencies. Uses real repository PBR textures (Albedo +
// Tangent-Space Normal + RoughMetal), 6-face Sky4K cubemap IBL, ACES Filmic
// HDR tonemapping, dynamic point lights, AABB 3D physics, Grapple Hook [Z],
// Orbital Ion Strike [X], Wave Mutators, Boss Enrage, 9 Weapons & 6 Breeds.
// ============================================================================
(() => {
  "use strict";

  // ---------- 1. Minimal Fast Mat4 / Vec3 Math ----------
  const M4 = {
    ident() {
      const m = new Float32Array(16);
      m[0] = m[5] = m[10] = m[15] = 1;
      return m;
    },
    persp(out, fovRad, aspect, near, far) {
      const f = 1.0 / Math.tan(fovRad * 0.5);
      const nf = 1.0 / (near - far);
      out.fill(0);
      out[0] = f / aspect;
      out[5] = f;
      out[10] = (far + near) * nf;
      out[11] = -1;
      out[14] = 2 * far * near * nf;
      return out;
    },
    mul(out, a, b) {
      const a00=a[0],a01=a[1],a02=a[2],a03=a[3], a10=a[4],a11=a[5],a12=a[6],a13=a[7],
            a20=a[8],a21=a[9],a22=a[10],a23=a[11], a30=a[12],a31=a[13],a32=a[14],a33=a[15];
      for (let i = 0; i < 4; i++) {
        const b0=b[i*4], b1=b[i*4+1], b2=b[i*4+2], b3=b[i*4+3];
        out[i*4]   = b0*a00 + b1*a10 + b2*a20 + b3*a30;
        out[i*4+1] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
        out[i*4+2] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
        out[i*4+3] = b0*a03 + b1*a13 + b2*a23 + b3*a33;
      }
      return out;
    },
    // 2.5: ортографическая проекция для shadow map (солнце → вся арена 96x96 + стены)
    ortho(out, l, r, b, t, n, f) {
      out.fill(0);
      out[0] = 2 / (r - l);
      out[5] = 2 / (t - b);
      out[10] = -2 / (f - n);
      out[12] = -(r + l) / (r - l);
      out[13] = -(t + b) / (t - b);
      out[14] = -(f + n) / (f - n);
      out[15] = 1;
      return out;
    },
    // 2.5: классический lookAt (z направлен от target к eye), стилем как viewFPS
    viewLookAt(out, ex, ey, ez, tx, ty, tz) {
      let zx = ex - tx, zy = ey - ty, zz = ez - tz;
      const zl = Math.hypot(zx, zy, zz) || 1; zx /= zl; zy /= zl; zz /= zl;
      // up ≈ (0,1,0); если смотрим почти вертикально — берём (1,0,0)
      const ux = 0, uy = 1, uz = 0;
      let xx = uy * zz - uz * zy, xy = uz * zx - ux * zz, xz = ux * zy - uy * zx;
      if (Math.hypot(xx, xy, xz) < 0.001) { xx = zz; xy = 0; xz = -zx; }
      const xl = Math.hypot(xx, xy, xz) || 1; xx /= xl; xy /= xl; xz /= xl;
      const yx = zy * xz - zz * xy, yy = zz * xx - zx * xz, yz = zx * xy - zy * xx;
      out[0]=xx; out[1]=yx; out[2]=zx; out[3]=0;
      out[4]=xy; out[5]=yy; out[6]=zy; out[7]=0;
      out[8]=xz; out[9]=yz; out[10]=zz;out[11]=0;
      out[12]=-(xx*ex + xy*ey + xz*ez);
      out[13]=-(yx*ex + yy*ey + yz*ez);
      out[14]=-(zx*ex + zy*ey + zz*ez);
      out[15]=1;
      return out;
    },
    viewFPS(out, ex, ey, ez, yaw, pitch, roll = 0) {
      // Forward = (+sin(yaw)*cos(pitch), +sin(pitch), +cos(yaw)*cos(pitch))
      const cy = Math.cos(yaw), sy = Math.sin(yaw);
      const cp = Math.cos(pitch), sp = Math.sin(pitch);
      const fx = sy * cp, fy = sp, fz = cy * cp;
      // Right = (cy, 0, -sy) rotated by roll
      let rx = cy, ry = 0, rz = -sy;
      let ux = -sy * sp, uy = cp, uz = -cy * sp;
      if (roll !== 0) {
        const cr = Math.cos(roll), sr = Math.sin(roll);
        const nrx = rx * cr + ux * sr, nry = ry * cr + uy * sr, nrz = rz * cr + uz * sr;
        ux = ux * cr - rx * sr; uy = uy * cr - ry * sr; uz = uz * cr - rz * sr;
        rx = nrx; ry = nry; rz = nrz;
      }
      out[0]=rx;  out[1]=ux;  out[2]=-fx;  out[3]=0;
      out[4]=ry;  out[5]=uy;  out[6]=-fy;  out[7]=0;
      out[8]=rz;  out[9]=uz;  out[10]=-fz; out[11]=0;
      out[12]=-(rx*ex + ry*ey + rz*ez);
      out[13]=-(ux*ex + uy*ey + uz*ez);
      out[14]= (fx*ex + fy*ey + fz*ez);
      out[15]=1;
      return out;
    },
    trs(out, tx, ty, tz, ry, rx, rz, sx, sy, sz) {
      const cy = Math.cos(ry), syy = Math.sin(ry);
      const cx = Math.cos(rx), sxx = Math.sin(rx);
      const cz = Math.cos(rz), szz = Math.sin(rz);
      const r00 = cy*cz + syy*sxx*szz, r01 = cx*szz, r02 = -syy*cz + cy*sxx*szz;
      const r10 = -cy*szz + syy*sxx*cz, r11 = cx*cz, r12 = syy*szz + cy*sxx*cz;
      const r20 = syy*cx,               r21 = -sxx,  r22 = cy*cx;
      out[0]=r00*sx; out[1]=r01*sx; out[2]=r02*sx;  out[3]=0;
      out[4]=r10*sy; out[5]=r11*sy; out[6]=r12*sy;  out[7]=0;
      out[8]=r20*sz; out[9]=r21*sz; out[10]=r22*sz; out[11]=0;
      out[12]=tx;    out[13]=ty;    out[14]=tz;     out[15]=1;
      return out;
    },
    beamBetween(out, x1, y1, z1, x2, y2, z2, thickness) {
      const dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
      const len = Math.hypot(dx, dy, dz) || 0.001;
      const ux = dx / len, uy = dy / len, uz = dz / len;
      let ax = 0, ay = 1, az = 0;
      if (Math.abs(uy) > 0.95) { ax = 1; ay = 0; }
      let rx = ay*uz - az*uy, ry = az*ux - ax*uz, rz = ax*uy - ay*ux;
      const rl = Math.hypot(rx, ry, rz) || 1;
      rx /= rl; ry /= rl; rz /= rl;
      const fx = uy*rz - uz*ry, fy = uz*rx - ux*rz, fz = ux*ry - uy*rx;
      out[0]=rx*thickness; out[1]=ry*thickness; out[2]=rz*thickness; out[3]=0;
      out[4]=ux*len;       out[5]=uy*len;       out[6]=uz*len;       out[7]=0;
      out[8]=fx*thickness; out[9]=fy*thickness; out[10]=fz*thickness;out[11]=0;
      out[12]=(x1+x2)*0.5; out[13]=(y1+y2)*0.5; out[14]=(z1+z2)*0.5; out[15]=1;
      return out;
    }
  };

  // ---------- 2. WebGL2 Shaders (GGX PBR + Normal Map + Cubemap IBL + ACES HDR) ----------
  const VS_PBR = `#version 300 es
  precision highp float;
  layout(location=0) in vec3 aPos;
  layout(location=1) in vec3 aNormal;
  layout(location=2) in vec3 aTangent;
  layout(location=3) in vec2 aUV;

  uniform mat4 uVP;
  uniform mat4 uModel;
  uniform vec2 uUVScale;

  out vec3 vWorldPos;
  out vec3 vNormal;
  out vec3 vTangent;
  out vec2 vUV;
  out vec2 vClip;

  void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    mat3 nm = mat3(uModel);
    vNormal  = normalize(nm * aNormal);
    vTangent = normalize(nm * aTangent);
    vUV = aUV * uUVScale;
    gl_Position = uVP * wp;
    vClip = gl_Position.xy / max(gl_Position.w, 0.0001);
  }`;

  const FS_PBR = `#version 300 es
  precision highp float;
  in vec3 vWorldPos;
  in vec3 vNormal;
  in vec3 vTangent;
  in vec2 vUV;
  in vec2 vClip;

  uniform sampler2D uAlbedoMap;
  uniform sampler2D uNormalMap;
  uniform sampler2D uRoughMetalMap;
  uniform samplerCube uEnvCube;
  uniform sampler2D uShadowMap;   // 2.5: directional sun shadow map
  uniform mat4 uLightVP;          // 2.5: light view-projection for shadow pass
  uniform float uUseShadows;      // 1 = FBO поднялось и тени активны

  uniform vec4  uTint;        // rgb + alpha
  uniform vec3  uEmissive;    // HDR linear emissive
  uniform vec2  uPbrOverride; // x = roughnessMul, y = metallicOverride (<0 = use texture)
  uniform float uUseTex;      // 1.0 = full PBR textures, 0.0 = solid PBR

  uniform vec3  uCamPos;
  uniform vec3  uSunDir;
  uniform vec3  uSunColor;
  uniform vec3  uSkyAmb;
  uniform vec3  uFogColor;
  uniform float uFogDensity;

  uniform vec4  uPointPos[4]; // xyz = pos, w = invRange
  uniform vec3  uPointCol[4]; // HDR color * intensity

  out vec4 fragColor;

  vec3 ACESFilm(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
  }

  void main() {
    vec3 N = normalize(vNormal);
    vec3 albedo = uTint.rgb;
    float rough = clamp(uPbrOverride.x, 0.06, 0.98);
    float metal = clamp(max(uPbrOverride.y, 0.0), 0.0, 1.0);
    float ao = 1.0;

    if (uUseTex > 0.5) {
      vec3 texCol = pow(texture(uAlbedoMap, vUV).rgb, vec3(2.2));
      albedo *= texCol;
      vec3 T = normalize(vTangent - N * dot(vTangent, N));
      vec3 B = cross(N, T);
      vec3 tn = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
      tn.xy *= 0.85;
      N = normalize(T * tn.x + B * tn.y + N * tn.z);
      // Urho3D RoughMetal-пакер (как у нативного клиента и генератора):
      // R = roughness, G = metallic, B = AO
      vec3 rm = texture(uRoughMetalMap, vUV).rgb;
      rough = clamp(rm.r * uPbrOverride.x, 0.06, 0.98);
      metal = (uPbrOverride.y >= 0.0) ? uPbrOverride.y : clamp(rm.g, 0.0, 1.0);
      ao = rm.b;
    }

    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metal);

    // Directional Sun (Cook-Torrance GGX simplified fast path)
    vec3 L = normalize(uSunDir);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float a2 = rough * rough * rough * rough;
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    float D = a2 / max(3.14159 * denom * denom, 0.0001);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    vec3 kD = (1.0 - F) * (1.0 - metal);
    // 2.5: Directional sun shadow (PCF 3x3, front-face culling в depth pass убирает acne)
    float sunShadow = 1.0;
    if (uUseShadows > 0.5) {
      vec4 lp = uLightVP * vec4(vWorldPos, 1.0);
      vec3 ls = lp.xyz / max(lp.w, 0.0001);
      vec2 suv = ls.xy * 0.5 + 0.5;
      float cur = ls.z * 0.5 + 0.5;
      if (suv.x > 0.002 && suv.x < 0.998 && suv.y > 0.002 && suv.y < 0.998 && cur < 1.0) {
        float ndl = max(dot(N, normalize(uSunDir)), 0.0);
        float bias = clamp(0.0022 * (1.0 - ndl), 0.0007, 0.0028);
        vec2 texel = vec2(1.0 / 1024.0);
        float acc = 0.0;
        for (int y = -1; y <= 1; y++) {
          for (int x = -1; x <= 1; x++) {
            float stored = texture(uShadowMap, suv + vec2(float(x), float(y)) * texel).r;
            acc += (stored > cur - bias) ? 1.0 : 0.0;
          }
        }
        sunShadow = mix(0.30, 1.0, acc / 9.0);
      }
    }
    vec3 sunLit = (kD * albedo / 3.14159 + D * F * 0.28) * uSunColor * NdotL * sunShadow;

    // 4 Dynamic Point Lights (muzzle, explosions, orbital beam, jump-pads)
    vec3 ptLit = vec3(0.0);
    for (int i = 0; i < 4; i++) {
      if (uPointPos[i].w <= 0.0) continue;
      vec3 toP = uPointPos[i].xyz - vWorldPos;
      float d = length(toP);
      float att = clamp(1.0 - d * uPointPos[i].w, 0.0, 1.0);
      att *= att;
      if (att <= 0.001) continue;
      vec3 Lp = toP / max(d, 0.001);
      vec3 Hp = normalize(V + Lp);
      float nl = max(dot(N, Lp), 0.0);
      float specP = pow(max(dot(N, Hp), 0.0), mix(96.0, 8.0, rough));
      ptLit += (albedo * (1.0 - metal * 0.6) * nl + F0 * specP * 0.8) * uPointCol[i] * att;
    }

    // Hemisphere + Cubemap IBL reflection
    vec3 R = reflect(-V, N);
    vec3 envSample = pow(texture(uEnvCube, R).rgb, vec3(2.2)) * uSkyAmb * 1.45;
    float hemi = N.y * 0.5 + 0.5;
    // AO (канал B RoughMetal-карты) затеняет окружённое/ambient освещение, солнце не трогаем
    vec3 amb = (kD * albedo * uSkyAmb * mix(0.38, 1.05, hemi) + F * envSample * (1.0 - rough * 0.72)) * mix(0.35, 1.0, ao);

    vec3 color = amb + sunLit + ptLit + uEmissive;

    // Exponential squared atmospheric fog
    float dist = length(vWorldPos - uCamPos);
    float fogF = 1.0 - exp(-pow(dist * uFogDensity, 1.65));
    color = mix(color, uFogColor, clamp(fogF, 0.0, 0.92));

    // ACES Filmic HDR + Gamma 2.2 (+ текстурная альфа для blob-теней)
    vec3 mapped = pow(ACESFilm(color * 1.08), vec3(1.0 / 2.2));
    float texA = (uUseTex > 0.5) ? texture(uAlbedoMap, vUV).a : 1.0;
    // vignette: мягкое затемнение углов (тоннельный эффект арены)
    float vig = 1.0 - smoothstep(0.42, 1.32, dot(vClip, vClip));
    mapped *= mix(0.66, 1.0, vig);
    fragColor = vec4(mapped, uTint.a * texA);
  }`;

  const VS_SKY = `#version 300 es
  precision highp float;
  layout(location=0) in vec3 aPos;
  uniform mat4 uVPNoTrans;
  out vec3 vDir;
  void main() {
    vDir = aPos;
    vec4 p = uVPNoTrans * vec4(aPos * 180.0, 1.0);
    gl_Position = p.xyww;
  }`;

  const FS_SKY = `#version 300 es
  precision highp float;
  in vec3 vDir;
  uniform samplerCube uEnvCube;
  uniform vec3 uSkyTint;
  uniform vec3 uFogColor;
  out vec4 fragColor;
  void main() {
    vec3 d = normalize(vDir);
    vec3 tex = texture(uEnvCube, d).rgb * uSkyTint;
    float h = clamp(1.0 - abs(d.y) * 2.2, 0.0, 1.0);
    vec3 col = mix(tex, pow(uFogColor, vec3(1.0/2.2)), h * h * 0.65);
    fragColor = vec4(col, 1.0);
  }`;

  // ---------- 3. Procedural Indexed GPU Meshes ----------
  function createMesh(gl, pos, norm, tan, uv, idx) {
    const vao = gl.createVertexArray();
    gl.bindVertexArray(vao);
    const bindBuf = (loc, data, size) => {
      const b = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, b);
      gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(data), gl.STATIC_DRAW);
      gl.enableVertexAttribArray(loc);
      gl.vertexAttribPointer(loc, size, gl.FLOAT, false, 0, 0);
    };
    bindBuf(0, pos, 3);
    bindBuf(1, norm, 3);
    bindBuf(2, tan, 3);
    bindBuf(3, uv, 2);
    const ib = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ib);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(idx), gl.STATIC_DRAW);
    gl.bindVertexArray(null);
    return { vao, count: idx.length };
  }

  function buildBoxMesh(gl) {
    const faces = [
      { n:[0,0,1],  t:[1,0,0],  v:[[-0.5,-0.5,0.5],[0.5,-0.5,0.5],[0.5,0.5,0.5],[-0.5,0.5,0.5]] },
      { n:[0,0,-1], t:[-1,0,0], v:[[0.5,-0.5,-0.5],[-0.5,-0.5,-0.5],[-0.5,0.5,-0.5],[0.5,0.5,-0.5]] },
      { n:[0,1,0],  t:[1,0,0],  v:[[-0.5,0.5,0.5],[0.5,0.5,0.5],[0.5,0.5,-0.5],[-0.5,0.5,-0.5]] },
      { n:[0,-1,0], t:[1,0,0],  v:[[-0.5,-0.5,-0.5],[0.5,-0.5,-0.5],[0.5,-0.5,0.5],[-0.5,-0.5,0.5]] },
      { n:[1,0,0],  t:[0,0,-1], v:[[0.5,-0.5,0.5],[0.5,-0.5,-0.5],[0.5,0.5,-0.5],[0.5,0.5,0.5]] },
      { n:[-1,0,0], t:[0,0,1],  v:[[-0.5,-0.5,-0.5],[-0.5,-0.5,0.5],[-0.5,0.5,0.5],[-0.5,0.5,-0.5]] }
    ];
    const P=[], N=[], T=[], U=[], I=[];
    faces.forEach((f, fi) => {
      const base = fi * 4;
      const uvs = [[0,1],[1,1],[1,0],[0,0]];
      for (let k = 0; k < 4; k++) {
        P.push(...f.v[k]); N.push(...f.n); T.push(...f.t); U.push(...uvs[k]);
      }
      I.push(base, base+1, base+2, base, base+2, base+3);
    });
    return createMesh(gl, P, N, T, U, I);
  }

  function buildCylinderMesh(gl, segs = 18, topR = 0.5, botR = 0.5) {
    const P=[], N=[], T=[], U=[], I=[];
    for (let i = 0; i <= segs; i++) {
      const u = i / segs, a = u * Math.PI * 2;
      const c = Math.cos(a), s = Math.sin(a);
      P.push(c * botR, -0.5, s * botR, c * topR, 0.5, s * topR);
      N.push(c, 0.15 * (botR - topR), s, c, 0.15 * (botR - topR), s);
      T.push(-s, 0, c, -s, 0, c);
      U.push(u * 2, 1, u * 2, 0);
    }
    for (let i = 0; i < segs; i++) {
      const b = i * 2;
      I.push(b, b+2, b+3, b, b+3, b+1);
    }
    // Top cap
    const topCenter = P.length / 3;
    P.push(0, 0.5, 0); N.push(0,1,0); T.push(1,0,0); U.push(0.5,0.5);
    for (let i = 0; i <= segs; i++) {
      const a = (i / segs) * Math.PI * 2;
      P.push(Math.cos(a)*topR, 0.5, Math.sin(a)*topR);
      N.push(0,1,0); T.push(1,0,0); U.push(0.5+Math.cos(a)*0.5, 0.5+Math.sin(a)*0.5);
    }
    for (let i = 0; i < segs; i++) I.push(topCenter, topCenter+1+i+1, topCenter+1+i);
    return createMesh(gl, P, N, T, U, I);
  }

  function buildSphereMesh(gl, lat = 12, lon = 16) {
    const P=[], N=[], T=[], U=[], I=[];
    for (let y = 0; y <= lat; y++) {
      const v = y / lat, phi = v * Math.PI;
      for (let x = 0; x <= lon; x++) {
        const u = x / lon, th = u * Math.PI * 2;
        const nx = Math.sin(phi)*Math.cos(th), ny = Math.cos(phi), nz = Math.sin(phi)*Math.sin(th);
        P.push(nx*0.5, ny*0.5, nz*0.5);
        N.push(nx, ny, nz);
        T.push(-Math.sin(th), 0, Math.cos(th));
        U.push(u * 2, v * 2);
      }
    }
    for (let y = 0; y < lat; y++) {
      for (let x = 0; x < lon; x++) {
        const a = y * (lon + 1) + x, b = a + lon + 1;
        I.push(a, b, a + 1, b, b + 1, a + 1);
      }
    }
    return createMesh(gl, P, N, T, U, I);
  }

  // ---------- 4. Boot & Engine Initialization ----------
  window.StartChikenGunWebGL2 = function(opts) {
    const canvas = document.getElementById("glCanvas");
    const gl = canvas.getContext("webgl2", { antialias: true, powerPreference: "high-performance" });
    if (!gl) {
      alert("WebGL2 не поддерживается этим браузером!");
      return;
    }

    const extAniso = gl.getExtension("EXT_texture_filter_anisotropic");
    const maxAniso = extAniso ? Math.min(16, gl.getParameter(extAniso.MAX_TEXTURE_MAX_ANISOTROPY_EXT)) : 1;

    function compileProg(vsSrc, fsSrc) {
      const sh = (type, src) => {
        const s = gl.createShader(type);
        gl.shaderSource(s, src); gl.compileShader(s);
        if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) console.error(gl.getShaderInfoLog(s));
        return s;
      };
      const p = gl.createProgram();
      gl.attachShader(p, sh(gl.VERTEX_SHADER, vsSrc));
      gl.attachShader(p, sh(gl.FRAGMENT_SHADER, fsSrc));
      gl.linkProgram(p);
      if (!gl.getProgramParameter(p, gl.LINK_STATUS)) console.error(gl.getProgramInfoLog(p));
      return p;
    }

    const pbrProg = compileProg(VS_PBR, FS_PBR);
    const skyProg = compileProg(VS_SKY, FS_SKY);

    const U = {};
    [
      "uVP","uModel","uUVScale","uAlbedoMap","uNormalMap","uRoughMetalMap","uEnvCube",
      "uTint","uEmissive","uPbrOverride","uUseTex","uCamPos","uSunDir","uSunColor",
      "uSkyAmb","uFogColor","uFogDensity",
      "uPointPos[0]","uPointPos[1]","uPointPos[2]","uPointPos[3]",
      "uPointCol[0]","uPointCol[1]","uPointCol[2]","uPointCol[3]",
      "uShadowMap","uLightVP","uUseShadows"
    ].forEach(n => U[n] = gl.getUniformLocation(pbrProg, n));

    const USky = {
      vp: gl.getUniformLocation(skyProg, "uVPNoTrans"),
      cube: gl.getUniformLocation(skyProg, "uEnvCube"),
      tint: gl.getUniformLocation(skyProg, "uSkyTint"),
      fog: gl.getUniformLocation(skyProg, "uFogColor")
    };

    const meshes = {
      box:  buildBoxMesh(gl),
      cyl:  buildCylinderMesh(gl, 18, 0.5, 0.5),
      cone: buildCylinderMesh(gl, 14, 0.04, 0.5),
      sph:  buildSphereMesh(gl, 12, 16)
    };

    // Load 2D PBR Texture Trios + Sky4K Cubemap
    function makeFallbackTex(r, g, b) {
      const t = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, t);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([r,g,b,255]));
      return t;
    }
    const whiteTex = makeFallbackTex(255, 255, 255);
    const flatNorm = makeFallbackTex(128, 128, 255);
    const defRM    = makeFallbackTex(255, 140, 20);

    // 2.4: сначала пробуем 1024px TexturesHD (Diffuse/Normal), при 404 — фолбэк на 512px
    function loadTex2D(url, fallback, fallbackUrl) {
      const tex = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                    fallback === flatNorm ? new Uint8Array([128,128,255,255]) : new Uint8Array([200,200,200,255]));
      const img = new Image();
      let triedFallback = false;
      img.onerror = () => {
        if (fallbackUrl && !triedFallback) { triedFallback = true; img.src = fallbackUrl; }
      };
      img.onload = () => {
        gl.bindTexture(gl.TEXTURE_2D, tex);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
        gl.generateMipmap(gl.TEXTURE_2D);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        if (extAniso) gl.texParameterf(gl.TEXTURE_2D, extAniso.TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
      };
      img.src = url;
      return tex;
    }

    function loadMaterialSet(prefix) {
      const sd = (kind) => `Data/Textures/${prefix}${kind}.png`;
      const hd = (kind) => `Data/TexturesHD/${prefix}${kind}.png`;
      // В TexturesHD лежат все 7 сетов (1024px Diffuse+Normal+RoughMetal); остальные — сразу 512px
      const hasHD = ["FloorTile", "StoneHD", "MetalPanel", "BrushedMetal", "CrateWood", "FeatherPBR", "HazardMetal"].includes(prefix);
      return {
        diff: loadTex2D(hasHD ? hd("Diffuse") : sd("Diffuse"), whiteTex, sd("Diffuse")),
        norm: loadTex2D(hasHD ? hd("Normal") : sd("Normal"), flatNorm, sd("Normal")),
        rm:   loadTex2D(hasHD ? hd("RoughMetal") : sd("RoughMetal"), defRM, sd("RoughMetal"))
      };
    }

    const MATS = {
      floor:   loadMaterialSet("FloorTile"),
      stone:   loadMaterialSet("StoneHD"),
      panel:   loadMaterialSet("MetalPanel"),
      metal:   loadMaterialSet("BrushedMetal"),
      crate:   loadMaterialSet("CrateWood"),
      feather: loadMaterialSet("FeatherPBR"),
      hazard:  loadMaterialSet("HazardMetal")
    };

    const envCube = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_CUBE_MAP, envCube);
    const cubeFaces = [
      [gl.TEXTURE_CUBE_MAP_POSITIVE_X, "Data/Textures/Sky4K_PosX.png"],
      [gl.TEXTURE_CUBE_MAP_NEGATIVE_X, "Data/Textures/Sky4K_NegX.png"],
      [gl.TEXTURE_CUBE_MAP_POSITIVE_Y, "Data/Textures/Sky4K_PosY.png"],
      [gl.TEXTURE_CUBE_MAP_NEGATIVE_Y, "Data/Textures/Sky4K_NegY.png"],
      [gl.TEXTURE_CUBE_MAP_POSITIVE_Z, "Data/Textures/Sky4K_PosZ.png"],
      [gl.TEXTURE_CUBE_MAP_NEGATIVE_Z, "Data/Textures/Sky4K_NegZ.png"]
    ];
    let cubeLoaded = 0;
    cubeFaces.forEach(([target, url]) => {
      gl.texImage2D(target, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([28, 42, 76, 255]));
      const im = new Image();
      im.onload = () => {
        gl.bindTexture(gl.TEXTURE_CUBE_MAP, envCube);
        gl.texImage2D(target, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, im);
        if (++cubeLoaded === 6) {
          gl.generateMipmap(gl.TEXTURE_CUBE_MAP);
          gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
        }
      };
      im.src = url;
    });
    gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MIN_FILTER, gl.LINEAR);

    // 2.4: радиальная градиентная текстура для мягких blob-теней под динамикой.
    // Если canvas 2D недоступен (headless/старый браузер) — будет null, теней не будет.
    const shadowRM = makeFallbackTex(255, 0, 255); // R=rough 1, G=metal 0, B=AO 1
    let shadowMat = null;
    try {
      const cv = document.createElement("canvas");
      cv.width = cv.height = 128;
      const ctx = cv.getContext && cv.getContext("2d");
      if (ctx) {
        const grd = ctx.createRadialGradient(64, 64, 8, 64, 64, 62);
        grd.addColorStop(0.00, "rgba(0,0,0,0.92)");
        grd.addColorStop(0.45, "rgba(0,0,0,0.55)");
        grd.addColorStop(0.80, "rgba(0,0,0,0.18)");
        grd.addColorStop(1.00, "rgba(0,0,0,0)");
        ctx.fillStyle = grd;
        ctx.fillRect(0, 0, 128, 128);
        const shadowTex = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, shadowTex);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, cv);
        gl.generateMipmap(gl.TEXTURE_2D);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        shadowMat = { diff: shadowTex, norm: flatNorm, rm: shadowRM };
      }
    } catch (_) { shadowMat = null; }

    // ---------- 2.5a. Sun-Shadow (directional depth map, orthographic) ----------
    // Если depth-FBO не собрался (старые драйверы/несовместимый iGPU) — тени просто выключаются
    // (uUseShadows=0), рендер идентичен 2.4. ?noshadows=1 — принудительный выключатель.
    const SHADOW_RES = 1024;
    const urlShadowsDisabled = (typeof location !== "undefined" && location.search) ? /[?&]noshadows=1/.test(location.search) : false;
    let shadowFB = null, shadowDepthTex = null, sunShadowsOn = false;
    try {
      shadowDepthTex = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, shadowDepthTex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.DEPTH_COMPONENT24, SHADOW_RES, SHADOW_RES, 0, gl.DEPTH_COMPONENT, gl.UNSIGNED_INT, null);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      shadowFB = gl.createFramebuffer();
      gl.bindFramebuffer(gl.FRAMEBUFFER, shadowFB);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, shadowDepthTex, 0);
      if (gl.drawBuffers) gl.drawBuffers([gl.NONE]);
      if (gl.readBuffer) gl.readBuffer(gl.NONE);
      sunShadowsOn = gl.checkFramebufferStatus(gl.FRAMEBUFFER) === gl.FRAMEBUFFER_COMPLETE && !urlShadowsDisabled;
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    } catch (_) { sunShadowsOn = false; }

    const VS_DEPTH = `#version 300 es
    precision highp float;
    layout(location=0) in vec3 aPos;
    uniform mat4 uLightVP;
    uniform mat4 uModel;
    void main() { gl_Position = uLightVP * uModel * vec4(aPos, 1.0); }`;
    const FS_DEPTH = `#version 300 es
    precision highp float;
    void main() {}`;
    const depthProg = compileProg(VS_DEPTH, FS_DEPTH);
    const UDepth = {
      lightVP: gl.getUniformLocation(depthProg, "uLightVP"),
      model:   gl.getUniformLocation(depthProg, "uModel")
    };
    function depthDraw(meshName, modelMat) {
      const m = meshes[meshName];
      gl.bindVertexArray(m.vao);
      gl.uniformMatrix4fv(UDepth.model, false, modelMat);
      gl.drawElements(gl.TRIANGLES, m.count, gl.UNSIGNED_SHORT, 0);
    }

    // ---------- 5. Static 3D Arena Architecture & AABB Colliders ----------
    const staticProps = []; // { mesh, mat, x,y,z, ry, sx,sy,sz, uvs:[u,v], tint, emissive, rough, metal }
    const colliders   = []; // { cx, cy, cz, hx, hy, hz }

    // 2.4: появились yaw/rx/rz — повороты статических пропсов (трубы, антенны, обломки)
    function addProp(mesh, mat, x, y, z, sx, sy, sz, uvs = [1,1], collide = true, tint = [1,1,1,1], emissive = [0,0,0], rough = 1.0, metal = -1, yaw = 0, rx = 0, rz = 0) {
      const m = M4.ident();
      M4.trs(m, x, y, z, yaw, rx, rz, sx, sy, sz);
      staticProps.push({ mesh, mat, model: m, uvs, tint, emissive, rough, metal });
      if (collide) {
        // AABB-коллайдер: для поворотов, близких к 90°, меняем местами extents
        const quarter = Math.round(yaw / (Math.PI / 2));
        const near90 = Math.abs(yaw - quarter * Math.PI / 2) < 0.35;
        const swap = near90 && (Math.abs(quarter) % 2 === 1);
        const hx = (swap ? sz : sx) * 0.5, hz = (swap ? sx : sz) * 0.5;
        colliders.push({ cx: x, cy: y, cz: z, hx, hy: sy*0.5, hz });
      }
    }

    // Floor 96x96m
    addProp("box", MATS.floor, 0, -0.25, 0, 96, 0.5, 96, [32, 32], false);
    // Outer fortress walls + glowing neon trim
    const H = 48;
    addProp("box", MATS.stone,  0, 3.5,  H, 96, 7, 1.5, [24, 2]);
    addProp("box", MATS.stone,  0, 3.5, -H, 96, 7, 1.5, [24, 2]);
    addProp("box", MATS.stone,  H, 3.5,  0, 1.5, 7, 96, [24, 2]);
    addProp("box", MATS.stone, -H, 3.5,  0, 1.5, 7, 96, [24, 2]);
    addProp("box", null, 0, 6.6,  H-0.8, 92, 0.18, 0.18, [1,1], false, [0.2,0.9,1,1], [0.4, 2.2, 2.6], 0.2, 0.8);
    addProp("box", null, 0, 6.6, -H+0.8, 92, 0.18, 0.18, [1,1], false, [0.2,0.9,1,1], [0.4, 2.2, 2.6], 0.2, 0.8);
    addProp("box", null,  H-0.8, 6.6, 0, 0.18, 0.18, 92, [1,1], false, [1,0.75,0.2,1], [2.4, 1.6, 0.3], 0.2, 0.8);
    addProp("box", null, -H+0.8, 6.6, 0, 0.18, 0.18, 92, [1,1], false, [1,0.75,0.2,1], [2.4, 1.6, 0.3], 0.2, 0.8);

    // 4 L-shaped Cyber Bunkers (climbable with Jump-Pad or [Z] Grapple!)
    [[-22,-22],[22,-22],[-22,22],[22,22]].forEach(([bx, bz], idx) => {
      const sx = bx < 0 ? 1 : -1, sz = bz < 0 ? 1 : -1;
      addProp("box", MATS.panel, bx, 1.6, bz, 7.5, 3.2, 1.6, [3, 1.2]);
      addProp("box", MATS.panel, bx + sx*2.9, 1.6, bz + sz*3.0, 1.6, 3.2, 4.6, [2, 1.2]);
      addProp("box", null, bx, 3.25, bz, 7.6, 0.12, 1.7, [1,1], false, [0.2,0.9,1,1], idx%2 ? [2.2,0.5,2.5] : [0.3,2.2,2.5], 0.2, 0.9);
    });

    // 8 Arena Pillars + Neon Rings
    for (let i = 0; i < 8; i++) {
      const a = (i / 8) * Math.PI * 2;
      const px = Math.cos(a) * 16.5, pz = Math.sin(a) * 16.5;
      addProp("cyl", MATS.stone, px, 3.2, pz, 1.7, 6.4, 1.7, [2, 3]);
      addProp("cyl", null, px, 5.4, pz, 1.92, 0.22, 1.92, [1,1], false, [1,0.85,0.3,1], [2.4, 1.8, 0.35], 0.2, 0.9);
    }

    // Stacked Wooden Crates
    [[-8, 6],[8, -6],[-9, -12],[10, 13],[0, 22],[0, -22],[-28, 4],[28, -4]].forEach(([cx, cz], i) => {
      addProp("box", MATS.crate, cx, 0.9, cz, 1.8, 1.8, 1.8, [1, 1]);
      if (i % 2 === 0) addProp("box", MATS.crate, cx + 1.85, 0.9, cz, 1.8, 1.8, 1.8, [1, 1]);
      if (i % 3 === 0) addProp("box", MATS.crate, cx + 0.5, 2.7, cz, 1.6, 1.8, 1.6, [1, 1]);
    });

    // 4 Stadium Floodlight Towers (2.4: + поперечная балка, парные лампы, красный маяк)
    [[-38,-38],[38,-38],[-38,38],[38,38]].forEach(([tx, tz]) => {
      addProp("cyl", MATS.metal, tx, 9, tz, 1.2, 18, 1.2, [1, 6]);
      addProp("box", MATS.panel, tx, 16.3, tz, 3.6, 0.65, 1.0, [2, 1], false, [0.38, 0.44, 0.56, 1], [0, 0, 0], 0.4, 0.82);
      addProp("box", null, tx, 17.85, tz, 1.15, 0.55, 1.55, [1,1], false, [1,0.95,0.85,1], [4.2, 3.9, 3.1], 0.12, 0.85);
      addProp("box", null, tx, 18.55, tz, 0.5, 0.4, 0.5, [1,1], false, [1,0.14,0.14,1], [3.4, 0.22, 0.18], 0.3, 0.5);
    });

    // ============ 2.4 ARENA OVERHAUL ============
    // Центральный ритуальный круг «Перо Судьбы» (плоский — не мешает движению и стрельбе)
    addProp("cyl", MATS.panel, 0, 0.05, 0, 10.5, 0.10, 10.5, [5, 5], false, [0.30, 0.42, 0.60, 1], [0.02, 0.10, 0.16], 0.3, 0.85);
    addProp("cyl", null, 0, 0.13, 0, 7.4, 0.07, 7.4, [1, 1], false, [0.16, 0.85, 1.0, 1], [0.15, 1.35, 1.75], 0.15, 0.85);
    addProp("cyl", null, 0, 0.175, 0, 3.3, 0.09, 3.3, [1, 1], false, [1.0, 0.82, 0.3, 1], [2.0, 1.4, 0.28], 0.2, 0.9);
    // 4 столба-маячка по диагоналям круга (укрытия)
    for (let i = 0; i < 4; i++) {
      const a = i * Math.PI / 2 + Math.PI / 4;
      const px = Math.cos(a) * 8.4, pz = Math.sin(a) * 8.4;
      addProp("cyl", MATS.metal, px, 0.55, pz, 0.55, 1.1, 0.55, [1, 1], true, [0.45, 0.55, 0.72, 1], [0.08, 0.5, 0.65], 0.3, 0.9);
      addProp("box", null, px, 1.17, pz, 0.62, 0.16, 0.62, [1, 1], false, [0.2, 0.95, 1, 1], [0.3, 1.9, 2.3], 0.15, 0.85);
    }

    // Полуразрушенные стены на осях — укрытия и разрыватели длинных прострелов
    const RUIN_WALLS = [
      { x: 0,    z: 12.5,  sx: 5.4,  sz: 1.15, h: 1.5 },
      { x: 0,    z: -12.5, sx: 5.4,  sz: 1.15, h: 1.5 },
      { x: 12.5, z: 0,     sx: 1.15, sz: 5.4,  h: 1.5 },
      { x: -12.5,z: 0,     sx: 1.15, sz: 5.4,  h: 1.5 },
      { x: 0,    z: 27.5,  sx: 7.2,  sz: 1.0,  h: 2.3 },
      { x: 0,    z: -27.5, sx: 7.2,  sz: 1.0,  h: 2.3 }
    ];
    RUIN_WALLS.forEach(rw => {
      addProp("box", MATS.stone, rw.x, rw.h * 0.5, rw.z, rw.sx, rw.h, rw.sz, [Math.max(rw.sx, rw.sz) * 0.8, rw.h * 0.8], true);
      // рваный «оплавленный» неоновый верх обломка
      addProp("box", null, rw.x, rw.h + 0.04, rw.z, rw.sx * 0.55, 0.1, rw.sz * 0.92, [1, 1], false, [1, 0.62, 0.16, 1], [1.7, 0.72, 0.1], 0.3, 0.6);
    });

    // Бетонные ежи (жёлтый hazard) по ближним диагоналям — противо-чарджер/камикадзе
    [[8.2, 8.2], [-8.2, -8.2], [8.2, -8.2], [-8.2, 8.2]].forEach(([hx, hz]) => {
      addProp("box", MATS.hazard, hx, 0.42, hz, 1.5, 0.84, 1.5, [1, 1], true);
    });

    // Неоновые арки-ворота на южном и северном подходах
    [22.5, -22.5].forEach(az => {
      [-2.7, 2.7].forEach(ox => {
        addProp("box", MATS.panel, ox, 1.9, az, 0.7, 3.8, 0.7, [1, 3], true, [0.32, 0.38, 0.50, 1], [0.05, 0.35, 0.5], 0.35, 0.85);
      });
      addProp("box", MATS.panel, 0, 4.05, az, 6.4, 0.55, 0.9, [4, 1], false, [0.30, 0.36, 0.48, 1], [0, 0, 0], 0.35, 0.85);
      addProp("box", null, 0, 4.05, az, 6.5, 0.16, 0.95, [1, 1], false, [0.2, 0.95, 1, 1], [0.3, 2.3, 2.7], 0.15, 0.9);
    });

    // Антенны, маяки и вент-короба на крышах 4 кибер-бункеров
    [[-22,-22],[22,-22],[-22,22],[22,22]].forEach(([bx, bz]) => {
      addProp("cyl", MATS.metal, bx - 2.1, 5.3, bz + 0.4, 0.16, 4.2, 0.16, [1, 3], false);
      addProp("box", null, bx - 2.1, 7.55, bz + 0.4, 0.48, 0.34, 0.48, [1, 1], false, [1, 0.2, 0.2, 1], [2.6, 0.3, 0.25], 0.3, 0.5);
      addProp("cyl", MATS.metal, bx + 1.4, 3.6, bz - 0.9, 0.5, 0.78, 0.5, [1, 1], false);
      addProp("cone", null, bx + 1.4, 4.14, bz - 0.9, 0.68, 0.5, 0.68, [1, 1], false, [0.72, 0.78, 0.88, 1], [0, 0, 0], 0.35, 0.7);
    });

    // Трубы вдоль внутренних стен + жёлтые вентили (2.4)
    [47.0, -47.0].forEach(off => {
      // труба вдоль оси X (north/south)
      addProp("cyl", MATS.metal, 0, 1.18, off, 0.48, 88, 0.48, [10, 1], false, [0.55, 0.60, 0.72, 1], [0, 0, 0], 0.32, 0.9, 0, 0, Math.PI / 2);
      // труба вдоль оси Z (east/west)
      addProp("cyl", MATS.metal, off, 1.18, 0, 0.48, 88, 0.48, [10, 1], false, [0.55, 0.60, 0.72, 1], [0, 0, 0], 0.32, 0.9, 0, Math.PI / 2, 0);
      [-33, -11, 11, 33].forEach(t => {
        // вентиль на X-трубе (ось Z)
        addProp("cyl", MATS.hazard, t, 1.18, (off - Math.sign(off) * 0.32), 0.36, 0.16, 0.36, [1, 1], false, [0.9, 0.75, 0.2, 1], [0, 0, 0], 0.5, 0.4, 0, Math.PI / 2, 0);
        // вентиль на Z-трубе (ось X)
        addProp("cyl", MATS.hazard, (off - Math.sign(off) * 0.32), 1.18, t, 0.36, 0.16, 0.36, [1, 1], false, [0.9, 0.75, 0.2, 1], [0, 0, 0], 0.5, 0.4, 0, 0, Math.PI / 2);
      });
    });

    // Светящиеся «взлётные полосы» от круга к 4 аркам (эмиссив, без коллизий)
    [[0, 31.5, 0], [0, -31.5, 0], [31.5, 0, 1], [-31.5, 0, 1]].forEach(([gx, gz, rot]) => {
      addProp("box", null, gx, 0.035, gz, rot ? 15 : 0.55, 0.035, rot ? 0.55 : 15, [1, 1], false, [0.15, 0.8, 1, 1], [0.08, 0.75, 0.95], 0.25, 0.2);
    });

    // Обломки «техномусора» на полу (визуальный слой)
    [[-16, -8, 0.3], [14, 9, 1.1], [-6, 18, 0.8], [5, -17, 2.0], [18, -16, 1.6], [-18, 15, 0.5]].forEach(([dx, dz, ry], i) => {
      addProp("box", MATS.hazard, dx, 0.17, dz, 0.95 + (i % 3) * 0.4, 0.34, 0.72, [1, 1], false, [0.92, 0.78, 0.25, 1], [0, 0, 0], 0.65, 0.35, ry);
    });

    // 3D Wooden Steamhouse «Банька Смотрящего» at (-18, 0, -34) — enter with [P]!
    const BANYA_DOOR = { x: -18, z: -30.6 };
    addProp("box", MATS.crate, -18, 1.9, -34, 6.4, 3.8, 5.6, [3, 2], true, [0.96, 0.78, 0.56, 1], [0,0,0], 0.72, 0.05);
    addProp("box", MATS.crate, -18, 4.05, -34, 7.3, 0.55, 6.5, [3, 2], false, [0.54, 0.34, 0.20, 1], [0,0,0], 0.78, 0.05);
    addProp("box", MATS.crate, -18, 4.55, -34, 5.4, 0.45, 3.8, [2, 1], false, [0.46, 0.28, 0.16, 1], [0,0,0], 0.80, 0.05);
    addProp("cyl", MATS.stone, -19.8, 5.4, -35.2, 0.95, 3.2, 0.95, [1, 2], false);
    addProp("cyl", null, -19.8, 7.05, -35.2, 1.08, 0.22, 1.08, [1,1], false, [1,0.55,0.2,1], [2.8, 1.15, 0.25], 0.2, 0.8);
    addProp("box", null, -18, 1.35, -31.16, 1.55, 2.55, 0.14, [1,1], false, [1,0.74,0.36,1], [2.3, 1.35, 0.42], 0.25, 0.4);
    addProp("box", null, -18, 3.22, -31.12, 3.9, 0.42, 0.16, [1,1], false, [1,0.86,0.45,1], [2.8, 1.95, 0.62], 0.2, 0.8);
    addProp("box", null, -14.76, 2.15, -34, 0.14, 1.1, 1.5, [1,1], false, [0.85,0.95,1,1], [1.2, 1.9, 2.4], 0.15, 0.6);

    // ---------- 6. Weapons,Skins, Atmospheres & Mutators ----------
    const WEAPONS = [
      { name: "PISTOL",   dmg: 34,  rof: 0.21, mag: 12,  reload: 1.1, spread: 0.010, color: [0.65,0.72,0.85], sfx: "Shot_Pistol.wav",  len: 0.42 },
      { name: "SMG",      dmg: 19,  rof: 0.075,mag: 30,  reload: 1.4, spread: 0.026, color: [0.35,0.88,1.00], sfx: "Shot_SMG.wav",     len: 0.58 },
      { name: "RIFLE",    dmg: 28,  rof: 0.10, mag: 30,  reload: 1.6, spread: 0.015, color: [0.20,1.00,0.65], sfx: "Shot_Rifle.wav",   len: 0.78 },
      { name: "SHOTGUN",  dmg: 18,  rof: 0.78, mag: 6,   reload: 1.9, spread: 0.052, color: [1.00,0.68,0.22], sfx: "Shot_Shotgun.wav", len: 0.74, pellets: 8, semi: true },
      { name: "SNIPER",   dmg: 120, rof: 1.05, mag: 5,   reload: 2.1, spread: 0.002, color: [1.00,0.32,0.48], sfx: "Shot_Sniper.wav",  len: 0.98, semi: true, zoom: 34 },
      { name: "MINIGUN",  dmg: 15,  rof: 0.045,mag: 120, reload: 2.6, spread: 0.038, color: [1.00,0.84,0.25], sfx: "Shot_Minigun.wav", len: 0.86 },
      { name: "LAUNCHER", dmg: 155, rof: 1.25, mag: 4,   reload: 2.2, spread: 0.015, color: [1.00,0.42,0.18], sfx: "Shot_Grenade.wav", len: 0.80, semi: true, explosive: true },
      { name: "RAILGUN",  dmg: 195, rof: 1.22, mag: 4,   reload: 2.0, spread: 0.001, color: [0.22,0.95,1.00], sfx: "Shot_Railgun.wav", len: 0.96, semi: true, pierce: true },
      { name: "PLASMA",   dmg: 58,  rof: 0.18, mag: 20,  reload: 1.6, spread: 0.012, color: [0.18,1.00,0.58], sfx: "Shot_Plasma.wav",  len: 0.72, plasma: true }
    ];

    const SKINS = [
      { name: "CYBER NEON",   albedo: [0.16,0.22,0.28], accent: [0.18,2.4,1.6], rough: 0.22, metal: 0.85 },
      { name: "GOLD 24K",     albedo: [1.00,0.76,0.22], accent: [2.2,1.8,0.5],  rough: 0.14, metal: 0.98 },
      { name: "OBSIDIAN 4K",  albedo: [0.08,0.09,0.12], accent: [2.5,1.6,0.3],  rough: 0.12, metal: 0.90 },
      { name: "VOID QUANTUM", albedo: [0.38,0.12,0.68], accent: [2.2,0.4,2.6],  rough: 0.20, metal: 0.82 },
      { name: "CRIMSON FURY", albedo: [0.78,0.08,0.14], accent: [2.6,0.4,0.3],  rough: 0.18, metal: 0.88 },
      { name: "SOLAR FLARE",  albedo: [0.95,0.38,0.08], accent: [2.6,1.9,0.3],  rough: 0.20, metal: 0.86 }
    ];

    const ATMOSPHERES = [
      { name: "ЗАКАТ 4K",      sunDir:[0.55,0.62,0.45], sunCol:[2.6,1.85,1.25], skyAmb:[0.36,0.42,0.62], fogCol:[0.38,0.22,0.32], fogD:0.0085, skyTint:[1.05,0.90,0.88] },
      { name: "КИБЕР-ПОЛНОЧЬ", sunDir:[-0.4,0.75,0.35], sunCol:[0.75,1.35,2.4], skyAmb:[0.14,0.28,0.55], fogCol:[0.04,0.10,0.24], fogD:0.0105, skyTint:[0.38,0.65,1.15] },
      { name: "ЗАТМЕНИЕ",      sunDir:[0.35,0.50,-0.6], sunCol:[2.8,0.65,0.55], skyAmb:[0.35,0.10,0.14], fogCol:[0.28,0.05,0.08], fogD:0.0115, skyTint:[1.15,0.35,0.38] },
      { name: "АРКТИКА",       sunDir:[0.25,0.82,0.45], sunCol:[2.2,2.45,2.75], skyAmb:[0.48,0.62,0.78], fogCol:[0.36,0.52,0.68], fogD:0.0090, skyTint:[0.95,1.08,1.20] }
    ];

    const MUTATORS = [
      { name: "СТАНДАРТ",          grav: 1.00, spd: 1.00, cred: 1.00, boom: false },
      { name: "ЛУННАЯ ГРАВИТАЦИЯ", grav: 0.52, spd: 1.00, cred: 1.15, boom: false },
      { name: "ТУРБО-СТАЯ",        grav: 1.00, spd: 1.28, cred: 1.35, boom: false },
      { name: "ЗОЛОТАЯ ЛИХОРАДКА", grav: 1.00, spd: 1.05, cred: 2.00, boom: false },
      { name: "ПИРО-ШТОРМ",        grav: 1.00, spd: 1.08, cred: 1.25, boom: true  }
    ];

    // ---------- 7. Game State ----------
    let savedAch = 0, savedHi = 0, savedCred = 100, savedBuff = 0, savedSens = 1.0;
    try {
      savedAch  = parseInt(localStorage.getItem("cg23_ach") || "0", 10) || 0;
      savedHi   = parseInt(localStorage.getItem("cg23_hi")  || "0", 10) || 0;
      const sc  = parseInt(localStorage.getItem("cg23_credits") || "100", 10);
      savedCred = Number.isFinite(sc) ? Math.max(0, sc) : 100;
      const sb  = parseInt(localStorage.getItem("cg23_banya_buff") || "0", 10);
      savedBuff = (sb === 1 || sb === 2) ? sb : 0;
      const ss  = parseFloat(localStorage.getItem("cg23_sens") || "1.0");
      if (Number.isFinite(ss) && ss >= 0.2 && ss <= 3.0) savedSens = ss;
    } catch (_) {}

    const initBonusHp  = savedBuff >= 2 ? 50 : (savedBuff === 1 ? 25 : 0);
    const initBonusArm = savedBuff >= 2 ? 50 : (savedBuff === 1 ? 25 : 0);

    const S = {
      x: 0, y: 1.75, z: -26, vx: 0, vy: 0, vz: 0, yaw: 0, pitch: 0.04,
      sunShadows: sunShadowsOn,   // 2.5: статус directional shadow map (для F3-телеметрии)
      cluckCd: 2.5,               // 2.5: таймер атмосферных отдалённых кудахтаний
      onGround: true, fromPad: false, crouch: false, sensMul: savedSens,
      hp: 100 + initBonusHp, maxHp: 100 + initBonusHp, armor: Math.min(100, 50 + initBonusArm),
      score: 0, highScore: savedHi, credits: savedCred, banyaBuff: savedBuff, banyaDoorPos: BANYA_DOOR,
      banyaHintCd: 0, banyaSteamCd: 0,
      kills: 0, headshots: 0, wave: 1, mutator: 0, atmIdx: 0,
      wep: 2, skin: 0, ammo: WEAPONS.map(w => w.mag), reloadT: 0, fireCd: 0, recoil: 0,
      bobPhase: 0, inspectT: 0, aiming: false, laserOn: true,
      dashCd: 0, dashT: 0, dashDir: [0,0],
      grappleOn: false, grappleCd: 0, grappleT: 0, grapplePos: [0,0,0],
      orbitalPct: savedBuff >= 2 ? 75 : 40, orbitalStrikes: [],
      grenCd: 0, turretCd: 0, meleeCd: 0, meleeT: 0, overdriveT: 0, slowMo: 0, shake: 0,
      perks: [0, 0, 0, 0], achiev: savedAch,
      multiKill: 0, multiT: 0,
      barrels: [], pads: [], chickens: [], corpses: [], orbs: [], enemyEggs: [], grenades: [], turrets: [], beams: [], particles: [], pickups: [],
      weather: Array.from({ length: 42 }, () => ({ ox: (Math.random()-0.5)*34, oy: Math.random()*13, oz: (Math.random()-0.5)*34, sp: 2+Math.random()*3, ph: Math.random()*6.28 })),
      showFps: false, fps: 60, drawCalls: 0
    };

    function rayBox(ox,oy,oz, dx,dy,dz, cx,cy,cz, hx,hy,hz) {
      let tmin = 0, tmax = 1e9;
      const O=[ox,oy,oz], D=[dx,dy,dz], C=[cx,cy,cz], H=[hx,hy,hz];
      for (let a = 0; a < 3; a++) {
        if (Math.abs(D[a]) < 1e-7) {
          if (O[a] < C[a]-H[a] || O[a] > C[a]+H[a]) return -1;
        } else {
          let t1 = (C[a]-H[a]-O[a]) / D[a], t2 = (C[a]+H[a]-O[a]) / D[a];
          if (t1 > t2) { const tmp=t1; t1=t2; t2=tmp; }
          if (t1 > tmin) tmin = t1;
          if (t2 < tmax) tmax = t2;
          if (tmin > tmax) return -1;
        }
      }
      return tmin;
    }

    function raycastWorld(ox,oy,oz, dx,dy,dz, maxDist) {
      let best = maxDist, hit = false;
      if (dy < -1e-5) {
        const tf = -oy / dy;
        if (tf >= 0 && tf < best) { best = tf; hit = true; }
      }
      for (let i = 0; i < colliders.length; i++) {
        const c = colliders[i];
        const t = rayBox(ox,oy,oz, dx,dy,dz, c.cx,c.cy,c.cz, c.hx,c.hy,c.hz);
        if (t >= 0.05 && t < best) { best = t; hit = true; }
      }
      return hit ? best : -1;
    }

    // 2.4: курицы уважают статические AABB-укрытия. Свободное движение +
    // выталкивание по оси минимальной пенетрации (2 прохода): курица скользит
    // вдоль стены и никогда не застревает внутри коллайдера (включая нокаут-
    // импульсы и спавн внахлёст). Дроны-валькирии летают выше и коллизий не замечают.
    function resolveChickenMove(c, mx, mz) {
      if (c.type === "drone") return [c.x + mx, c.z + mz];
      let nx = c.x + mx, nz = c.z + mz;
      const R = 0.42, bodyBot = c.y + 0.15, bodyTop = c.y + (c.type === "boss" ? 2.1 : 1.3);
      for (let iter = 0; iter < 2; iter++) {
        let pushed = false;
        for (let i = 0; i < colliders.length; i++) {
          const col = colliders[i];
          if (col.cy - col.hy > bodyTop || col.cy + col.hy < bodyBot) continue;
          const dx = nx - col.cx, dz = nz - col.cz;
          const px = (col.hx + R) - Math.abs(dx);
          const pz = (col.hz + R) - Math.abs(dz);
          if (px > 0 && pz > 0) {
            if (px < pz) nx = col.cx + (dx >= 0 ? 1 : -1) * (col.hx + R);
            else         nz = col.cz + (dz >= 0 ? 1 : -1) * (col.hz + R);
            pushed = true;
          }
        }
        if (!pushed) break;
      }
      return [nx, nz];
    }

    function saveCredits() {
      try { localStorage.setItem("cg23_credits", String(Math.max(0, Math.round(S.credits)))); } catch (_) {}
    }

    function unlockAch(bit, title) {
      if (S.achiev & (1 << bit)) return;
      S.achiev |= (1 << bit);
      S.score += 250; S.credits += 50;
      saveCredits();
      try { localStorage.setItem("cg23_ach", String(S.achiev)); } catch (_) {}
      opts.playSfx("Achievement.wav", 0.88);
      opts.showToast(`🏆 ДОСТИЖЕНИЕ ОТКРЫТО: ${title} (+250 ОЧК / +50₵)`);
    }

    function resetWorldItems() {
      S.barrels = [
        { x: 11, z: 0, hp: 45, alive: true, resp: 0 }, { x: -11, z: 0, hp: 45, alive: true, resp: 0 },
        { x: 0, z: 11, hp: 45, alive: true, resp: 0 }, { x: 0, z: -11, hp: 45, alive: true, resp: 0 },
        { x: 18, z: 18, hp: 45, alive: true, resp: 0 }, { x: -18, z: -18, hp: 45, alive: true, resp: 0 }
      ];
      S.pads = [
        { x: 14, z: 14 }, { x: -14, z: 14 }, { x: 14, z: -14 }, { x: -14, z: -14 }
      ];
      S.pickups = [
        { x: -15, z: 0, kind: "hp", cd: 0 },
        { x:  15, z: 0, kind: "armor", cd: 0 },
        { x:  0,  z: 0, kind: "overdrive", cd: 0 }
      ];
    }

    function spawnWave(num) {
      S.wave = num;
      S.mutator = num <= 1 ? 0 : (num % 5 === 0 ? 3 : ((num * 3) % 5));
      if (num > 1) { S.credits += 45; saveCredits(); opts.pushFeed(`+45₵ БОНУС ЗА ВОЛНУ ${num}!`); }
      if (num >= 5) unlockAch(4, "ШТУРМОВИК 5-Й ВОЛНЫ");
      if (num >= 10) unlockAch(9, "ЛЕГЕНДА КУРЯТНИКА (10 ВОЛН)");
      opts.playSfx("WaveStart.wav", 0.72);

      const bossWave = (num % 5 === 0);
      const cnt = Math.min(4 + num * 2, 16);
      for (let i = 0; i < cnt; i++) {
        let a = (i / cnt) * Math.PI * 2 + Math.random() * 0.4;
        const r = 20 + Math.random() * 18;
        // 2.4: не спавним курицу внутри статических укрытий (баня, руины, бункеры)
        for (let tries = 0; tries < 8; tries++) {
          const tx = Math.cos(a) * r, tz = Math.sin(a) * r;
          let inside = false;
          for (let j = 0; j < colliders.length; j++) {
            const col = colliders[j];
            if (col.cy - col.hy > 1.3) continue;
            if (Math.abs(tx - col.cx) < col.hx + 0.6 && Math.abs(tz - col.cz) < col.hz + 0.6) { inside = true; break; }
          }
          if (!inside) break;
          a = Math.random() * Math.PI * 2;
        }
        let type = "melee";
        if (bossWave && i === 0) type = "boss";
        else {
          const rl = Math.random();
          if (num >= 2 && rl < 0.20) type = "kamikaze";
          else if (num >= 2 && rl < 0.42) type = "drone";
          else if (rl < 0.68) type = "charger";
          else if (rl < 0.86) type = "shooter";
        }
        const hp = type === "boss" ? 560 + num * 30 : type === "drone" ? 90 : type === "kamikaze" ? 55 : 95 + num * 10;
        S.chickens.push({
          x: Math.cos(a) * r, y: type === "drone" ? 4.4 : 0, z: Math.sin(a) * r,
          type, hp, maxHp: hp, phase: Math.random() * 6.28, atkCd: 1.0 + Math.random(), enraged: false, beepT: 0.4, strafeSeed: Math.random() * 6.28
        });
      }
      if (bossWave) opts.playSfx("RoosterBoss.wav", 0.9);
      opts.showBanner(`ВОЛНА ${num}  •  ${MUTATORS[S.mutator].name}${bossWave ? " [МЕГА-ИНДЮК!]" : ""}`);
    }

    resetWorldItems();
    spawnWave(1);

    // Dynamic lights ring buffer
    const dynLights = []; // { x,y,z, r,g,b, range, ttl }
    function addFlashLight(x, y, z, r, g, b, range, ttl) {
      dynLights.unshift({ x, y, z, r, g, b, range, ttl });
      if (dynLights.length > 6) dynLights.pop();
    }

    function explodeAt(ex, ey, ez, radius, maxDmg) {
      opts.playSfx("BarrelBoom.wav", 0.85);
      addFlashLight(ex, ey + 1.0, ez, 4.5, 2.2, 0.5, 18, 0.35);
      S.shake = Math.min(0.35, S.shake + 0.18);
      for (let k = 0; k < 24; k++) {
        const a = Math.random() * 6.28, sp = 4 + Math.random() * 12;
        S.particles.push({
          x: ex, y: ey + 0.6, z: ez,
          vx: Math.cos(a) * sp, vy: 3 + Math.random() * 9, vz: Math.sin(a) * sp,
          col: k % 2 ? [2.8, 1.0, 0.2] : [2.8, 2.2, 0.4], sz: 0.22, ttl: 0.8
        });
      }
      S.chickens.forEach(c => {
        if (c.hp <= 0) return;
        const d = Math.hypot(c.x - ex, (c.y + 0.9) - ey, c.z - ez);
        if (d < radius) damageChicken(c, maxDmg * (1 - 0.65 * d / radius), false);
      });
      S.barrels.forEach(b => {
        if (b.alive && Math.hypot(b.x - ex, b.z - ez) < radius * 0.85) {
          b.alive = false; b.resp = 14;
          unlockAch(2, "ПИРОМАН КУРЯТНИКА");
          setTimeout(() => explodeAt(b.x, 0.6, b.z, 8.5, 185), 80);
        }
      });
    }

    function damageChicken(c, dmg, headshot) {
      if (c.hp <= 0) return;
      c.hp -= dmg;
      // 2.4: импульс отбрасывания + flinch для анимации
      const awayX = c.x - S.x, awayZ = c.z - S.z, awayD = Math.hypot(awayX, awayZ) || 1;
      if (c.type !== "boss") { c.x += (awayX / awayD) * 0.22; c.z += (awayZ / awayD) * 0.22; }
      c.flinchT = 0.22;
      opts.flashHit(headshot, c.hp <= 0);
      opts.spawnDmgNum(c.x, c.y + 1.8, c.z, Math.round(dmg), headshot);
      if (headshot) {
        S.headshots++;
        opts.playSfx("Headshot.wav", 0.72);
        if (S.headshots >= 10) unlockAch(1, "КУРИНЫЙ СНАЙПЕР (10 ХЭДШОТОВ)");
      } else {
        opts.playSfx("ChickenHurt.wav", 0.4 + Math.random() * 0.14);
      }
      // 2.5: короткий взмах крыльев при попадании (хлёст воздуха) — родной ассет движка
      opts.playSfx("WingSlap.wav", 0.22);

      const fCol = c.type === "kamikaze" ? [1.8,0.4,0.1] : c.type === "drone" ? [0.2,1.6,2.2] : [1.2,1.2,1.15];
      const burstN = c.hp <= 0 ? (c.type === "boss" ? 34 : 14) : 3;
      for (let i = 0; i < burstN; i++) {
        const a = Math.random() * 6.28;
        S.particles.push({
          x: c.x, y: c.y + 1.0, z: c.z,
          vx: Math.cos(a) * 4.5, vy: 2.5 + Math.random() * 5, vz: Math.sin(a) * 4.5,
          col: fCol, sz: 0.16, ttl: 0.95
        });
      }

      if (c.hp <= 0) {
        // 2.4: тело падает и лежит на арене несколько секунд
        S.corpses.push({
          x: c.x, y: c.y, z: c.z,
          yaw: Math.atan2(awayX, awayZ), sc: c.type === "boss" ? 1.85 : c.type === "kamikaze" ? 0.82 : 1.0,
          type: c.type, ttl: 4.2, spin: (Math.random() - 0.5) * 0.8
        });
        opts.playSfx(c.type === "boss" ? "RoosterBoss.wav" : "ChickenDie.wav", 0.78);
        S.kills++;
        const mut = MUTATORS[S.mutator];
        const pts = c.type === "boss" ? 950 : c.type === "drone" ? 260 : 165;
        const cr  = Math.round((c.type === "boss" ? 120 : 28) * mut.cred);
        S.score += pts; S.credits += cr; saveCredits();
        if (S.score > S.highScore) {
          S.highScore = S.score;
          try { localStorage.setItem("cg23_hi", String(S.highScore)); } catch (_) {}
        }
        S.orbitalPct = Math.min(100, S.orbitalPct + (c.type === "boss" ? 48 : headshot ? 20 : 13.5));

        if (S.kills === 1) unlockAch(0, "ПЕРВОЕ ПЕРО");
        if (!S.onGround && S.fromPad) {
          S.score += 100; S.credits += 20;
          opts.pushFeed("🚀 АЭРО-ФРАГ В ПОЛЁТЕ! +100");
          unlockAch(3, "НЕБЕСНЫЙ ПЕТУХ");
        }
        if (c.type === "boss") {
          S.slowMo = 0.42; unlockAch(6, "УБИЙЦА МЕГА-ИНДЮКА");
          addFlashLight(c.x, 1.6, c.z, 2.8, 2.1, 0.9, 24, 0.55);   // золотая «корона»
          opts.showBanner("👑 МЕГА-ИНДЮК ПОВЕРЖЕН!");
        }
        if (c.type === "kamikaze" || mut.boom) explodeAt(c.x, 0.6, c.z, c.type === "kamikaze" ? 6.5 : 4.2, c.type === "kamikaze" ? 120 : 55);

        S.multiKill = S.multiT > 0 ? S.multiKill + 1 : 1;
        S.multiT = 3.2;
        if (S.multiKill === 3) opts.pushFeed("🔥🔥 ТРОЙНОЙ ШАШЛЫК!");
        else if (S.multiKill >= 5) { opts.pushFeed(`💀 КУРИНЫЙ АПОКАЛИПСИС x${S.multiKill}!`); S.slowMo = 0.32; }
        opts.pushFeed(`${c.type === "boss" ? "👑 БОСС ПАЛ" : "🐔 Курица ощипана"} +${pts} (+${cr}₵)`);
      }
    }

    function getAimDir(spread = 0) {
      const fx = Math.sin(S.yaw) * Math.cos(S.pitch);
      const fy = Math.sin(S.pitch);
      const fz = Math.cos(S.yaw) * Math.cos(S.pitch);
      if (spread <= 0) return [fx, fy, fz];
      const dx = fx + (Math.random() - 0.5) * spread;
      const dy = fy + (Math.random() - 0.5) * spread;
      const dz = fz + (Math.random() - 0.5) * spread;
      const l = Math.hypot(dx, dy, dz) || 1;
      return [dx / l, dy / l, dz / l];
    }

    function fireWeapon() {
      const w = WEAPONS[S.wep];
      if (S.fireCd > 0 || S.reloadT > 0) return;
      if (S.ammo[S.wep] <= 0) {
        S.reloadT = w.reload * Math.pow(0.88, S.perks[1]);
        opts.playSfx("EmptyClick.wav", 0.7);
        opts.pushFeed(`🔄 Перезарядка ${w.name}...`);
        return;
      }
      S.ammo[S.wep]--;
      S.inspectT = 0;
      const rofMul = Math.pow(0.88, S.perks[1]) * (S.overdriveT > 0 ? 0.65 : 1.0);
      S.fireCd = w.rof * rofMul;
      S.recoil = Math.min(0.26, S.recoil + (w.pierce ? 0.16 : 0.055));
      opts.playSfx(w.sfx, 0.75);

      const [fx, fy, fz] = getAimDir(0);
      addFlashLight(S.x + fx * 1.1, S.y - 0.1 + fy * 1.1, S.z + fz * 1.1, w.color[0]*3, w.color[1]*3, w.color[2]*3, 9, 0.06);

      const banyaDmgMul = S.banyaBuff >= 2 ? 1.15 : 1.0;
      const dmgMul = (1 + 0.15 * S.perks[2]) * banyaDmgMul * (S.overdriveT > 0 ? 2.0 : 1.0);
      if (w.plasma) {
        S.orbs.push({ x: S.x + fx*0.9, y: S.y - 0.12 + fy*0.9, z: S.z + fz*0.9, vx: fx*42, vy: fy*42, vz: fz*42, dmg: w.dmg * dmgMul, ttl: 2.2, bounces: 2 });
        return;
      }

      const pellets = w.pellets || 1;
      let railKills = 0;
      for (let p = 0; p < pellets; p++) {
        const [dx, dy, dz] = getAimDir(S.aiming ? w.spread * 0.35 : w.spread);
        let wallDist = raycastWorld(S.x, S.y, S.z, dx, dy, dz, 110);
        if (wallDist < 0) wallDist = 110;

        const hits = [];
        S.chickens.forEach(c => {
          if (c.hp <= 0) return;
          const sc = c.type === "boss" ? 1.65 : 0.85;
          const t = rayBox(S.x, S.y, S.z, dx, dy, dz, c.x, c.y + 0.95*sc, c.z, 0.65*sc, 0.95*sc, 0.65*sc);
          if (t > 0 && t < wallDist) {
            const hy = S.y + dy * t;
            hits.push({ kind: "chicken", obj: c, t, head: hy > c.y + 1.25 * sc });
          }
        });
        S.barrels.forEach(b => {
          if (!b.alive) return;
          const t = rayBox(S.x, S.y, S.z, dx, dy, dz, b.x, 0.85, b.z, 0.62, 0.85, 0.62);
          if (t > 0 && t < wallDist) hits.push({ kind: "barrel", obj: b, t });
        });
        hits.sort((a, b) => a.t - b.t);

        const maxT = (!w.pierce && hits.length > 0) ? hits[0].t : wallDist;
        S.beams.push({
          x1: S.x + dx*0.6 + Math.cos(S.yaw)*0.22, y1: S.y - 0.18 + dy*0.6, z1: S.z + dz*0.6 - Math.sin(S.yaw)*0.22,
          x2: S.x + dx*maxT, y2: S.y + dy*maxT, z2: S.z + dz*maxT,
          col: w.color.map(v => v * 2.6), th: w.pierce ? 0.11 : 0.032, ttl: w.pierce ? 0.28 : 0.065
        });

        const list = w.pierce ? hits : hits.slice(0, 1);
        if (w.explosive && list.length === 0 && wallDist < 109) {
          explodeAt(S.x + dx*wallDist, S.y + dy*wallDist, S.z + dz*wallDist, 7.5, w.dmg * dmgMul);
        }
        list.forEach(h => {
          if (w.explosive) {
            explodeAt(S.x + dx*h.t, S.y + dy*h.t, S.z + dz*h.t, 7.5, w.dmg * dmgMul);
          } else if (h.kind === "barrel") {
            h.obj.alive = false; h.obj.resp = 14;
            unlockAch(2, "ПИРОМАН КУРЯТНИКА");
            explodeAt(h.obj.x, 0.7, h.obj.z, 8.5, 190);
          } else {
            const wasAlive = h.obj.hp > 0;
            damageChicken(h.obj, w.dmg * dmgMul * (h.head ? 2.0 : 1.0), h.head);
            if (w.pierce && wasAlive && h.obj.hp <= 0) railKills++;
          }
        });
      }
      if (railKills >= 2) { S.slowMo = 0.36; unlockAch(5, "СКВОЗНОЙ РЕЛЬС (2+ ЗА ВЫСТРЕЛ)"); }
    }

    function triggerGrapple() {
      if (S.grappleOn) { S.grappleOn = false; return; }
      if (S.grappleCd > 0) return;
      const [dx, dy, dz] = getAimDir(0);
      const t = raycastWorld(S.x, S.y, S.z, dx, dy, dz, 58);
      if (t < 2.2) { opts.playSfx("EmptyClick.wav", 0.5); return; }
      S.grappleOn = true;
      S.grappleT = 1.45;
      S.grappleCd = 3.2 * Math.pow(0.88, S.perks[3]);
      S.grapplePos = [S.x + dx*t, S.y + dy*t, S.z + dz*t];
      S.onGround = false; S.fromPad = true;
      opts.playSfx("Grapple.wav", 0.88);
      unlockAch(10, "НЕБЕСНЫЙ АБОРДАЖ (ГРЭППЛ-КРЮК [Z])");
    }

    function triggerOrbitalStrike() {
      if (S.orbitalPct < 99.9) {
        if (S.credits >= 150) {
          S.credits -= 150; saveCredits();
          opts.pushFeed("🛰 СРОЧНЫЙ ОРБИТАЛЬНЫЙ ЗАЛП ЗА 150₵!");
        } else {
          opts.playSfx("EmptyClick.wav", 0.6);
          opts.pushFeed(`Орбита [X]: заряд ${Math.floor(S.orbitalPct)}% (нужно 100% или 150₵)`);
          return;
        }
      } else S.orbitalPct = 0;

      const [dx, dy, dz] = getAimDir(0);
      let t = raycastWorld(S.x, S.y, S.z, dx, dy, dz, 110);
      const sx = t > 0 ? S.x + dx*t : S.x + Math.sin(S.yaw)*18;
      const sz = t > 0 ? S.z + dz*t : S.z + Math.cos(S.yaw)*18;

      S.orbitalStrikes.push({ x: sx, z: sz, ttl: 1.25, maxTtl: 1.25 });
      addFlashLight(sx, 4.0, sz, 1.5, 4.5, 5.0, 32, 1.1);
      opts.playSfx("OrbitalStrike.wav", 0.95);
      S.shake = 0.32;

      const maxDmg = 380 * (1 + 0.15 * S.perks[2]);
      let kills = 0;
      S.chickens.forEach(c => {
        if (c.hp <= 0) return;
        const d = Math.hypot(c.x - sx, c.z - sz);
        if (d <= 16.5) {
          const alive0 = c.hp > 0;
          damageChicken(c, maxDmg * (1 - 0.5 * d / 16.5), true);
          if (alive0 && c.hp <= 0) kills++;
        }
      });
      S.barrels.forEach(b => {
        if (b.alive && Math.hypot(b.x - sx, b.z - sz) <= 16.5) {
          b.alive = false; b.resp = 14;
          explodeAt(b.x, 0.7, b.z, 8.5, 190);
        }
      });
      if (kills >= 3) { S.slowMo = 0.42; unlockAch(11, "ЯЙЦЕГЕДДОН С ОРБИТЫ (3+ ФРАГА)"); }
      opts.showBanner("☢ ОРБИТАЛЬНЫЙ УДАР «ЯЙЦЕГЕДДОН»!");
    }

    // ---------- 8. Input Hooks (Touchpad-Tamed + Raw PointerLock + Unlocked Fallback) ----------
    const keys = {};
    let mouseDown = false;
    let lockAcquiredAt = 0, skipFirstLockDelta = false;
    let lastTapAt = 0, rmbDownAt = 0, rmbWasAiming = false;
    let smoothDx = 0, smoothDy = 0;
    let lastPtrX = null, lastPtrY = null;
    let wheelAcc = 0, lastWheelStepAt = 0, lastWheelEventAt = 0;

    function requestLockSmart() {
      if (!canvas || !canvas.requestPointerLock) return;
      try {
        const res = canvas.requestPointerLock({ unadjustedMovement: true });
        if (res && typeof res.catch === "function") {
          res.catch(() => { try { canvas.requestPointerLock(); } catch (_) {} });
        }
      } catch (_) {
        try { canvas.requestPointerLock(); } catch (__) {}
      }
    }

    function setSensitivity(mul, announce = false) {
      S.sensMul = Math.max(0.25, Math.min(2.5, Math.round(mul * 20) / 20));
      try { localStorage.setItem("cg23_sens", String(S.sensMul)); } catch (_) {}
      if (announce) opts.pushFeed(`🎯 Чувствительность камеры: ${Math.round(S.sensMul * 100)}% (клавиши - / +)`);
      opts.onSensChange?.(S.sensMul);
    }

    function applyLookDelta(rawDx, rawDy, now) {
      // Reject Chromium/macOS trackpad PointerLock jump spikes
      if (!Number.isFinite(rawDx) || !Number.isFinite(rawDy)) return;
      if (Math.abs(rawDx) > 190 || Math.abs(rawDy) > 190) return;

      // Damp cursor micro-jerk during trackpad tap-to-click
      const tapDamp = (now - lastTapAt < 45) ? 0.25 : 1.0;
      const compress = d => Math.sign(d) * Math.min(54, Math.abs(d) / (1 + Math.abs(d) * 0.015));
      const tx = compress(rawDx * tapDamp);
      const ty = compress(rawDy * tapDamp);

      smoothDx = smoothDx * 0.30 + tx * 0.70;
      smoothDy = smoothDy * 0.30 + ty * 0.70;

      const baseSens = (S.aiming ? 0.00082 : 0.00148) * S.sensMul;
      S.yaw  += smoothDx * baseSens;
      S.pitch = Math.max(-1.42, Math.min(1.42, S.pitch - smoothDy * baseSens));
    }

    document.addEventListener("pointerlockchange", () => {
      if (document.pointerLockElement === canvas) {
        lockAcquiredAt = performance.now();
        skipFirstLockDelta = true;
        smoothDx = 0; smoothDy = 0;
      } else {
        mouseDown = false;
        lastPtrX = null; lastPtrY = null;
      }
    });

    window.addEventListener("mousemove", e => {
      const now = performance.now();
      if (document.pointerLockElement === canvas) {
        if (skipFirstLockDelta || (now - lockAcquiredAt < 45)) {
          skipFirstLockDelta = false;
          return;
        }
        applyLookDelta(e.movementX || 0, e.movementY || 0, now);
      } else if (opts.isPlaying() && !opts.isPerkModalOpen() && !(opts.isBanyaOpen && opts.isBanyaOpen())) {
        // Fallback look if browser/iframe blocks PointerLock
        if (lastPtrX !== null && lastPtrY !== null) {
          applyLookDelta((e.clientX - lastPtrX) * 1.15, (e.clientY - lastPtrY) * 1.15, now);
        }
        lastPtrX = e.clientX; lastPtrY = e.clientY;
      }
    });
    canvas.addEventListener("mouseleave", () => { lastPtrX = null; lastPtrY = null; });

    window.addEventListener("mousedown", e => {
      const activePlay = opts.isPlaying() && !opts.isPerkModalOpen() && !(opts.isBanyaOpen && opts.isBanyaOpen());
      if (!activePlay) return;
      if (document.pointerLockElement !== canvas && e.target !== canvas) return;
      if (document.pointerLockElement !== canvas) requestLockSmart();

      const now = performance.now();
      lastTapAt = now;
      // On macOS trackpads, Ctrl+Click (while crouching with Ctrl) reports button===2 + ctrlKey
      if (e.button === 0 || (e.button === 2 && e.ctrlKey)) {
        mouseDown = true;
        fireWeapon();
      } else if (e.button === 2) {
        rmbDownAt = now;
        rmbWasAiming = S.aiming;
        S.aiming = !rmbWasAiming;
      }
    });
    window.addEventListener("mouseup", e => {
      if (e.button === 0 || (e.button === 2 && e.ctrlKey)) mouseDown = false;
      else if (e.button === 2) {
        // Quick 2-finger touchpad tap (<240ms) toggles ADS; long physical RMB hold releases ADS on mouseup
        const heldMs = performance.now() - rmbDownAt;
        if (heldMs >= 240 && !rmbWasAiming) S.aiming = false;
      }
    });
    window.addEventListener("contextmenu", e => {
      if (document.pointerLockElement === canvas || opts.isPlaying()) e.preventDefault();
    });
    window.addEventListener("wheel", e => {
      const activePlay = opts.isPlaying() && !opts.isPerkModalOpen() && !(opts.isBanyaOpen && opts.isBanyaOpen());
      if (!activePlay) return;
      e.preventDefault();

      const now = performance.now();
      if (now - lastWheelEventAt > 140) wheelAcc = 0;
      lastWheelEventAt = now;

      // Ignore touchpad palm/two-finger micro-jitter and horizontal-dominant swipes
      if (Math.abs(e.deltaX) > Math.abs(e.deltaY) * 1.2) return;
      if (e.deltaMode === 0 && Math.abs(e.deltaY) < 5) return;

      const stepThreshold = (e.deltaMode === 1) ? 1 : 135;
      wheelAcc += e.deltaY;
      if (Math.abs(wheelAcc) >= stepThreshold && (now - lastWheelStepAt) > 250) {
        const dir = wheelAcc > 0 ? 1 : WEAPONS.length - 1;
        S.wep = (S.wep + dir) % WEAPONS.length;
        S.reloadT = 0;
        wheelAcc = 0;
        lastWheelStepAt = now;
      }
    }, { passive: false });

    window.addEventListener("keydown", e => {
      keys[e.code] = true;
      if (opts.isBanyaOpen && opts.isBanyaOpen()) return;
      if (opts.isPerkModalOpen()) {
        if (e.code >= "Digit1" && e.code <= "Digit4") opts.buyPerk(Number(e.code.slice(5)) - 1);
        if (e.code === "KeyB" || e.code === "Escape") opts.togglePerkModal();
        return;
      }
      if (e.code === "F8") { e.preventDefault(); S.atmIdx = (S.atmIdx + 1) % ATMOSPHERES.length; opts.pushFeed(`🌅 Атмосфера: ${ATMOSPHERES[S.atmIdx].name}`); return; }
      if (e.code === "F3") { e.preventDefault(); S.showFps = !S.showFps; return; }
      if (e.code === "KeyM") { opts.cycleMusic(); return; }
      if (e.code === "Minus" || e.code === "BracketLeft") { setSensitivity(S.sensMul - 0.15, true); return; }
      if (e.code === "Equal" || e.code === "BracketRight") { setSensitivity(S.sensMul + 0.15, true); return; }
      if (e.code === "KeyP") {
        e.preventDefault();
        opts.openBanya?.();
        return;
      }
      if (!opts.isPlaying()) return;
      if (e.code === "Escape" && document.pointerLockElement !== canvas) {
        opts.exitArenaToMenu?.();
        return;
      }

      if (e.code >= "Digit1" && e.code <= "Digit9") { S.wep = Number(e.code.slice(5)) - 1; S.reloadT = 0; }
      if (e.code === "KeyF") S.aiming = !S.aiming;
      if (e.code === "KeyK" || e.code === "Enter") fireWeapon();
      if (e.code === "KeyQ") S.skin = (S.skin + 1) % SKINS.length;
      if (e.code === "KeyE") S.skin = (S.skin + SKINS.length - 1) % SKINS.length;
      if (e.code === "KeyI") S.inspectT = 2.0;
      if (e.code === "KeyL") S.laserOn = !S.laserOn;
      if (e.code === "KeyR" && S.reloadT <= 0 && S.ammo[S.wep] < WEAPONS[S.wep].mag) {
        S.reloadT = WEAPONS[S.wep].reload * Math.pow(0.88, S.perks[1]);
        opts.playSfx("EmptyClick.wav", 0.6);
      }
      if (e.code === "KeyB") opts.togglePerkModal();
      if ((e.code === "AltLeft" || e.code === "AltRight") && S.dashCd <= 0) {
        e.preventDefault();
        S.dashCd = 2.1 * Math.pow(0.86, S.perks[3]);
        S.dashT = 0.18;
        S.dashDir = [Math.sin(S.yaw), Math.cos(S.yaw)];
        opts.playSfx("Dash.wav", 0.8);
      }
      if (e.code === "KeyZ") triggerGrapple();
      if (e.code === "KeyX") triggerOrbitalStrike();
      if (e.code === "KeyV" && S.meleeCd <= 0) {
        S.meleeCd = 0.55; S.meleeT = 0.28;
        opts.playSfx("WingSlap.wav", 0.85);
        S.chickens.forEach(c => {
          if (c.hp > 0 && Math.hypot(c.x - S.x, c.z - S.z) < 3.6) damageChicken(c, 75, false);
        });
      }
      if (e.code === "KeyG" && S.grenCd <= 0) {
        S.grenCd = 6.2;
        const [dx, dy, dz] = getAimDir(0);
        S.grenades.push({ x: S.x + dx*0.6, y: S.y, z: S.z + dz*0.6, vx: dx*19, vy: dy*19 + 5.5, vz: dz*19, fuse: 1.25 });
        opts.playSfx("WingSlap.wav", 0.7);
      }
      if (e.code === "KeyT" && S.turretCd <= 0 && S.credits >= 100) {
        S.credits -= 100; saveCredits(); S.turretCd = 14;
        S.turrets.push({ x: S.x + Math.sin(S.yaw)*2.6, z: S.z + Math.cos(S.yaw)*2.6, yaw: S.yaw, ttl: 24, cd: 0.2 });
        opts.playSfx("TurretDeploy.wav", 0.85);
        unlockAch(8, "ИНЖЕНЕР ПТИЦЕФАБРИКИ (ТУРЕЛЬ [T])");
      }
    });
    window.addEventListener("keyup", e => { keys[e.code] = false; });

    // ---------- 9. Draw Helpers ----------
    const vpMat = M4.ident(), projMat = M4.ident(), viewMat = M4.ident(), skyVP = M4.ident(), tmpM = M4.ident();
    const lightProjMat = M4.ident(), lightViewMat = M4.ident(), lightVPMat = M4.ident();

    function drawMesh(meshName, matSet, modelMat, uvs, tint, emissive, rough, metal) {
      const m = meshes[meshName];
      gl.bindVertexArray(m.vao);
      gl.uniformMatrix4fv(U.uModel, false, modelMat);
      gl.uniform2f(U.uUVScale, uvs[0], uvs[1]);
      gl.uniform4f(U.uTint, tint[0], tint[1], tint[2], tint[3]);
      gl.uniform3f(U.uEmissive, emissive[0], emissive[1], emissive[2]);
      gl.uniform2f(U.uPbrOverride, rough, metal);
      if (matSet) {
        gl.uniform1f(U.uUseTex, 1.0);
        gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, matSet.diff);
        gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, matSet.norm);
        gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, matSet.rm);
      } else {
        gl.uniform1f(U.uUseTex, 0.0);
      }
      gl.drawElements(gl.TRIANGLES, m.count, gl.UNSIGNED_SHORT, 0);
      S.drawCalls++;
    }

    // ---------- 10. Main Simulation & WebGL2 Render Loop ----------
    let lastNow = performance.now(), fpsAcc = 0, fpsFrames = 0;

    function tick(now) {
      const rawDt = Math.min(0.05, (now - lastNow) / 1000);
      lastNow = now;
      fpsAcc += rawDt; fpsFrames++;
      if (fpsAcc >= 0.4) { S.fps = Math.round(fpsFrames / fpsAcc); fpsAcc = 0; fpsFrames = 0; }

      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      const cw = Math.floor(window.innerWidth * dpr), ch = Math.floor(window.innerHeight * dpr);
      if (canvas.width !== cw || canvas.height !== ch) { canvas.width = cw; canvas.height = ch; }

      const dt = S.slowMo > 0 ? (S.slowMo -= rawDt, rawDt * 0.38) : rawDt;
      const active = opts.isPlaying() && !opts.isPerkModalOpen();

      // Update timers & player physics
      if (active) {
        S.fireCd    = Math.max(0, S.fireCd - dt);
        S.dashCd    = Math.max(0, S.dashCd - dt);
        S.dashT     = Math.max(0, S.dashT - dt);
        S.grappleCd = Math.max(0, S.grappleCd - dt);
        S.grenCd    = Math.max(0, S.grenCd - dt);
        S.turretCd  = Math.max(0, S.turretCd - dt);
        S.meleeCd   = Math.max(0, S.meleeCd - dt);
        S.meleeT    = Math.max(0, S.meleeT - dt);
        S.inspectT  = Math.max(0, S.inspectT - dt);
        S.overdriveT= Math.max(0, S.overdriveT - dt);
        S.multiT    = Math.max(0, S.multiT - dt);
        S.recoil    = Math.max(0, S.recoil - dt * 1.45);
        S.shake     = Math.max(0, S.shake - dt * 1.6);

        if (S.reloadT > 0) {
          S.reloadT -= dt;
          if (S.reloadT <= 0) S.ammo[S.wep] = WEAPONS[S.wep].mag;
        }
        if ((mouseDown || keys["KeyK"] || keys["Enter"]) && !WEAPONS[S.wep].semi) fireWeapon();

        // Optional smooth keyboard arrow-key look (helpful on laptops when OS palm-guard blocks touchpad during WASD)
        const keyTurnSpd = (S.aiming ? 1.35 : 2.35) * S.sensMul;
        if (keys["ArrowLeft"])  S.yaw -= keyTurnSpd * rawDt;
        if (keys["ArrowRight"]) S.yaw += keyTurnSpd * rawDt;
        if (keys["ArrowUp"])    S.pitch = Math.min(1.42,  S.pitch + keyTurnSpd * 0.75 * rawDt);
        if (keys["ArrowDown"])  S.pitch = Math.max(-1.42, S.pitch - keyTurnSpd * 0.75 * rawDt);

        S.crouch = !!(keys["ControlLeft"] || keys["KeyC"]);
        const eyeH = S.crouch ? 1.05 : 1.75;

        if (S.grappleOn) {
          S.grappleT -= dt;
          const dx = S.grapplePos[0] - S.x, dy = S.grapplePos[1] - S.y, dz = S.grapplePos[2] - S.z;
          const dist = Math.hypot(dx, dy, dz);
          if (S.grappleT <= 0 || dist < 2.0) {
            S.grappleOn = false;
            S.vy = Math.max(S.vy, 6.2);
          } else {
            const pull = 28 * (1 + 0.08 * S.perks[3]) / dist;
            S.vx = dx * pull; S.vy = dy * pull + 1.5; S.vz = dz * pull;
          }
        } else if (S.dashT > 0) {
          const dspd = 22 * (1 + 0.08 * S.perks[3]);
          S.vx = S.dashDir[0] * dspd; S.vz = S.dashDir[1] * dspd;
        } else {
          let mx = 0, mz = 0;
          const sy = Math.sin(S.yaw), cy = Math.cos(S.yaw);
          if (keys["KeyW"]) { mx += sy; mz += cy; }
          if (keys["KeyS"]) { mx -= sy; mz -= cy; }
          if (keys["KeyD"]) { mx += cy; mz -= sy; }
          if (keys["KeyA"]) { mx -= cy; mz += sy; }
          const ml = Math.hypot(mx, mz);
          const baseSpd = (keys["ShiftLeft"] && !S.crouch ? 12.2 : S.crouch ? 4.8 : 8.2) * (1 + 0.08 * S.perks[3]) * (S.overdriveT > 0 ? 1.2 : 1.0);
          const tx = ml > 0 ? (mx / ml) * baseSpd : 0;
          const tz = ml > 0 ? (mz / ml) * baseSpd : 0;
          const acc = S.onGround ? 12 * dt : 3.5 * dt;
          S.vx += (tx - S.vx) * Math.min(1, acc);
          S.vz += (tz - S.vz) * Math.min(1, acc);
          if (S.onGround && ml > 0) S.bobPhase += dt * (keys["ShiftLeft"] ? 13 : 9);
        }

        if (!S.grappleOn) {
          if (keys["Space"] && S.onGround) { S.vy = 8.6; S.onGround = false; opts.playSfx("Dash.wav", 0.35); }
          S.vy -= 21.5 * MUTATORS[S.mutator].grav * dt;
        }

        // Horizontal AABB collision & step-top support!
        const nx = Math.max(-46.2, Math.min(46.2, S.x + S.vx * dt));
        const nz = Math.max(-46.2, Math.min(46.2, S.z + S.vz * dt));
        const feetY = S.y - eyeH;
        let canX = true, canZ = true, groundTop = 0;
        for (let i = 0; i < colliders.length; i++) {
          const c = colliders[i];
          const topY = c.cy + c.hy;
          if (Math.abs(nx - c.cx) < c.hx + 0.42 && Math.abs(S.z - c.cz) < c.hz + 0.42) {
            if (feetY >= topY - 0.35) groundTop = Math.max(groundTop, topY);
            else if (S.y > c.cy - c.hy) canX = false;
          }
          if (Math.abs((canX ? nx : S.x) - c.cx) < c.hx + 0.42 && Math.abs(nz - c.cz) < c.hz + 0.42) {
            if (feetY >= topY - 0.35) groundTop = Math.max(groundTop, topY);
            else if (S.y > c.cy - c.hy) canZ = false;
          }
        }
        if (canX) S.x = nx; else S.vx = 0;
        if (canZ) S.z = nz; else S.vz = 0;

        S.y += S.vy * dt;
        if (S.y <= groundTop + eyeH && S.vy <= 0) {
          S.y = groundTop + eyeH; S.vy = 0; S.onGround = true; S.fromPad = false;
        } else if (S.y > groundTop + eyeH + 0.15) {
          S.onGround = false;
        }

        // Jump pads
        S.pads.forEach(p => {
          if (Math.hypot(S.x - p.x, S.z - p.z) < 1.9 && S.y - eyeH < 0.6) {
            S.vy = 15.2; S.onGround = false; S.fromPad = true;
            opts.playSfx("JumpPad.wav", 0.85);
            addFlashLight(p.x, 0.8, p.z, 0.4, 2.8, 3.2, 10, 0.25);
          }
        });

        // Pickups
        S.pickups.forEach(pk => {
          if (pk.cd > 0) { pk.cd -= dt; return; }
          if (Math.hypot(S.x - pk.x, S.z - pk.z) < 1.7) {
            pk.cd = 16;
            if (pk.kind === "hp") { S.hp = Math.min(S.maxHp, S.hp + 35); opts.playSfx("ArmorPickup.wav", 0.7); opts.pushFeed("💚 +35 HP АПТЕЧКА"); }
            else if (pk.kind === "armor") { S.armor = Math.min(100, S.armor + 50); opts.playSfx("ArmorPickup.wav", 0.8); opts.pushFeed("🛡 +50 ТИТАНОВАЯ БРОНЯ"); }
            else { S.overdriveT = 10; opts.playSfx("Overdrive.wav", 0.85); opts.pushFeed("⚡ КИБЕР-ОВЕРДРАЙВ 2X УРОН (10с)!"); }
          }
        });

        // Barrels respawn
        S.barrels.forEach(b => { if (!b.alive) { b.resp -= dt; if (b.resp <= 0) { b.alive = true; b.hp = 45; } } });

        // 3D Steamhouse «Банька Смотрящего» chimney steam & doorway [P] prompt
        S.banyaSteamCd -= dt;
        if (S.banyaSteamCd <= 0) {
          S.banyaSteamCd = 0.22;
          S.particles.push({
            x: -19.8 + (Math.random() - 0.5) * 0.45, y: 7.25, z: -35.2 + (Math.random() - 0.5) * 0.45,
            vx: (Math.random() - 0.5) * 0.9, vy: 2.1 + Math.random() * 1.4, vz: (Math.random() - 0.5) * 0.9,
            col: [1.55, 1.48, 1.38], sz: 0.34, ttl: 1.45
          });
        }
        if (S.banyaHintCd > 0) S.banyaHintCd -= dt;
        if (Math.hypot(S.x - BANYA_DOOR.x, S.z - BANYA_DOOR.z) < 5.4 && S.banyaHintCd <= 0) {
          S.banyaHintCd = 7.5;
          opts.pushFeed("♨ БАНЬКА СМОТРЯЩЕГО — нажмите [P], чтобы попариться (бафф + ₵)!");
        }

        // Plasma bolts
        S.orbs = S.orbs.filter(o => {
          o.ttl -= dt; o.x += o.vx*dt; o.y += o.vy*dt; o.z += o.vz*dt;
          if (o.y < 0.28 && o.vy < 0) { o.y = 0.28; o.vy = -o.vy * 0.9; o.bounces--; }
          if (Math.abs(o.x) > 46.5) { o.vx = -o.vx; o.bounces--; }
          if (Math.abs(o.z) > 46.5) { o.vz = -o.vz; o.bounces--; }
          let hit = false;
          S.chickens.forEach(c => {
            if (!hit && c.hp > 0 && Math.hypot(c.x - o.x, (c.y + 0.95) - o.y, c.z - o.z) < 1.35) {
              damageChicken(c, o.dmg, false); hit = true;
            }
          });
          if (hit || o.bounces < 0) {
            addFlashLight(o.x, o.y, o.z, 0.3, 2.8, 1.4, 9, 0.16);
            return false;
          }
          return o.ttl > 0;
        });

        // Grenades
        S.grenades = S.grenades.filter(g => {
          g.fuse -= dt; g.vy -= 19 * dt;
          g.x += g.vx*dt; g.y += g.vy*dt; g.z += g.vz*dt;
          // 2.5: тонкий дымный след гранаты
          if (Math.random() < dt * 9) S.particles.push({ x: g.x, y: g.y, z: g.z, vx: (Math.random()-0.5)*0.4, vy: 0.6, vz: (Math.random()-0.5)*0.4, col: [0.6, 0.6, 0.66], sz: 0.11, ttl: 0.45 });
          if (g.y < 0.22) { g.y = 0.22; g.vy = Math.abs(g.vy)*0.55; g.vx *= 0.78; g.vz *= 0.78; }
          if (g.fuse <= 0) { explodeAt(g.x, g.y, g.z, 9.0, 175 * (1 + 0.15*S.perks[2])); return false; }
          return true;
        });

        // Turrets
        S.turrets = S.turrets.filter(t => {
          t.ttl -= dt; t.cd -= dt;
          let best = null, bd = 30;
          S.chickens.forEach(c => {
            if (c.hp <= 0) return;
            const d = Math.hypot(c.x - t.x, c.z - t.z);
            if (d < bd) { bd = d; best = c; }
          });
          if (best) {
            t.yaw = Math.atan2(best.x - t.x, best.z - t.z);
            if (t.cd <= 0) {
              t.cd = 0.26;
              S.beams.push({ x1: t.x, y1: 1.15, z1: t.z, x2: best.x, y2: best.y + 0.95, z2: best.z, col: [0.3, 2.4, 2.8], th: 0.04, ttl: 0.07 });
              opts.playSfx("TurretShot.wav", 0.35);
              damageChicken(best, 24 * (1 + 0.15*S.perks[2]), false);
            }
          }
          return t.ttl > 0;
        });

        // Enemy eggs
        S.enemyEggs = S.enemyEggs.filter(eg => {
          eg.ttl -= dt; eg.x += eg.vx*dt; eg.y += eg.vy*dt; eg.z += eg.vz*dt;
          if (Math.hypot(eg.x - S.x, eg.y - (S.y - 0.3), eg.z - S.z) < 0.85) {
            hurtPlayer(eg.dmg); return false;
          }
          return eg.ttl > 0 && eg.y > 0.05;
        });

        // Chickens AI
        let aliveCnt = 0, bossRef = null;
        const mutSpd = MUTATORS[S.mutator].spd;
        S.chickens.forEach(c => {
          if (c.hp <= 0) return;
          aliveCnt++;
          if (c.type === "boss") {
            bossRef = c;
            if (!c.enraged && c.hp <= c.maxHp * 0.5) {
              c.enraged = true;
              opts.playSfx("RoosterBoss.wav", 0.95);
              opts.showBanner("🔥 МЕГА-ИНДЮК ВПАЛ В ЯРОСТЬ!");
            }
          }
          c.phase += dt * 8.5;
          if (c.flinchT > 0) c.flinchT -= dt;
          if (c.lungeT > 0) c.lungeT -= dt;
          const dx = S.x - c.x, dz = S.z - c.z, d = Math.hypot(dx, dz) || 1;
          const baseSpd = (c.type === "kamikaze" ? 6.2 : c.type === "charger" ? 4.6 : c.type === "drone" ? 3.4 : 2.8) * mutSpd * (c.enraged ? 1.32 : 1.0);

          if (c.type === "kamikaze") {
            c.beepT -= dt;
            if (c.beepT <= 0) { c.beepT = Math.max(0.12, d / 26); if (d < 18) opts.playSfx("KamikazeBeep.wav", 0.35); }
          }

          if (c.type === "shooter" || c.type === "drone") {
            const sx = -dz / d, sz = dx / d;
            const approach = d > 15 ? 1 : (d < 8 ? -0.6 : 0);
            const mv = resolveChickenMove(c,
              (dx/d * approach + sx * Math.sin(c.phase*0.25) * 0.7) * baseSpd * dt,
              (dz/d * approach + sz * Math.sin(c.phase*0.25) * 0.7) * baseSpd * dt);
            c.x = mv[0]; c.z = mv[1];
            if (c.type === "drone") c.y = 4.2 + Math.sin(c.phase * 0.35) * 0.65;
            c.atkCd -= dt;
            if (c.atkCd <= 0 && d < 32) {
              c.atkCd = c.type === "drone" ? 1.55 : 1.9;
              const ey = c.y + 1.0, len = Math.hypot(dx, (S.y - 0.2) - ey, dz) || 1;
              S.enemyEggs.push({ x: c.x, y: ey, z: c.z, vx: dx/len*19, vy: ((S.y-0.2)-ey)/len*19, vz: dz/len*19, dmg: 10, ttl: 2.5 });
            }
          } else {
            if (d > 1.7) {
              // 2.5: рыскание при сближении (weaving) — ходоки/чарджеры/боссы вьются, у камикадзе почти нет
              const weave = c.type === "boss" ? 0.42 : c.type === "charger" ? 0.3 : c.type === "kamikaze" ? 0.12 : 0.55;
              const sway = Math.sin(c.phase * 0.5 + (c.strafeSeed || 0)) * weave;
              const perpX = -dz / d, perpZ = dx / d;
              const mv = resolveChickenMove(c, ((dx / d) + perpX * sway) * baseSpd * dt, ((dz / d) + perpZ * sway) * baseSpd * dt);
              c.x = mv[0]; c.z = mv[1];
            }
            c.atkCd -= dt;
            if (d <= 2.1 && c.atkCd <= 0) {
              c.atkCd = c.enraged ? 0.85 : 1.15;
              c.lungeT = 0.28;
              if (c.type === "kamikaze") { c.hp = 0; explodeAt(c.x, 0.5, c.z, 6.0, 38); hurtPlayer(26); }
              else hurtPlayer(c.type === "boss" ? (c.enraged ? 30 : 22) : 12);
            } else if (c.type === "boss" && c.atkCd <= 0 && d > 6) {
              c.atkCd = c.enraged ? 1.5 : 2.3;
              c.lungeT = 0.34;
              const spreadSet = c.enraged ? [-0.18, 0, 0.18] : [0];
              spreadSet.forEach(ang => {
                const ca = Math.cos(ang), sa = Math.sin(ang);
                const rdx = (dx/d)*ca - (dz/d)*sa, rdz = (dx/d)*sa + (dz/d)*ca;
                S.enemyEggs.push({ x: c.x, y: 1.9, z: c.z, vx: rdx*21, vy: -0.5, vz: rdz*21, dmg: 14, ttl: 2.6 });
              });
            }
          }
          c.x = Math.max(-45.5, Math.min(45.5, c.x));
          c.z = Math.max(-45.5, Math.min(45.5, c.z));
        });
        S.chickens = S.chickens.filter(c => c.hp > 0);
        if (aliveCnt === 0) spawnWave(S.wave + 1);
        opts.updateBossHud(bossRef);
        // 2.5: атмосферные отдалённые кудахтания — стая «живая» за пределами видимости
        S.cluckCd -= dt;
        if (S.cluckCd <= 0) {
          S.cluckCd = 3.5 + Math.random() * 4.5;
          let nd = Infinity;
          S.chickens.forEach(c => { const d = Math.hypot(c.x - S.x, c.z - S.z); if (d < nd) nd = d; });
          if (isFinite(nd)) {
            const vol = Math.max(0.05, Math.min(0.5, 0.55 - nd * 0.010));
            opts.playSfx(Math.random() < 0.5 ? "Chicken_Cluck.wav" : "Chicken_Cluck2.wav", vol);
          }
        }
      } else {
        // Menu orbit camera!
        const t = now * 0.00022;
        S.x = Math.sin(t) * 24;
        S.z = Math.cos(t) * 24;
        S.y = 6.5 + Math.sin(t * 1.7) * 1.2;
        S.yaw = Math.atan2(-S.x, -S.z);
        S.pitch = -0.18;
      }

      function hurtPlayer(raw) {
        const takeA = Math.min(S.armor, raw * 0.65);
        S.armor -= takeA;
        S.hp = Math.max(0, S.hp - (raw - takeA));
        S.shake = Math.min(0.3, S.shake + 0.12);
        opts.flashDamage();
        // 2.5: дуга-индикатор направления угрозы (на ближайшего живого врага)
        let bd = Infinity, bx = 0, bz = 0, found = false;
        S.chickens.forEach(c => { const d = Math.hypot(c.x - S.x, c.z - S.z); if (d < bd) { bd = d; bx = c.x; bz = c.z; found = true; } });
        if (found && typeof opts.flashDamageFrom === "function") {
          const fx = Math.sin(S.yaw), fz = Math.cos(S.yaw);
          const rx = Math.cos(S.yaw), rz = -Math.sin(S.yaw);
          const dxA = bx - S.x, dzA = bz - S.z;
          opts.flashDamageFrom(Math.atan2(dxA * rx + dzA * rz, dxA * fx + dzA * fz));
        }
        if (S.hp <= 0) {
          S.hp = S.maxHp; S.armor = 50;
          opts.pushFeed("⚡ РЕЗЕРВНЫЙ КИБЕР-КЛОН АКТИВИРОВАН!");
        }
      }

      // Decay FX
      S.beams = S.beams.filter(b => (b.ttl -= dt) > 0);
      S.orbitalStrikes = S.orbitalStrikes.filter(o => (o.ttl -= dt) > 0);
      S.corpses = S.corpses.filter(cp => (cp.ttl -= dt) > 0);
      S.particles = S.particles.filter(p => {
        p.ttl -= dt; p.vy -= 16 * dt;
        p.x += p.vx*dt; p.y = Math.max(0.06, p.y + p.vy*dt); p.z += p.vz*dt;
        return p.ttl > 0;
      });
      for (let i = dynLights.length - 1; i >= 0; i--) {
        dynLights[i].ttl -= dt;
        if (dynLights[i].ttl <= 0) dynLights.splice(i, 1);
      }

      // ---------- RENDER PASS ----------
      S.drawCalls = 0;
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
      gl.enable(gl.DEPTH_TEST);
      gl.enable(gl.CULL_FACE);

      const atm = ATMOSPHERES[S.atmIdx];

      // 2.5a) Sun shadow map (depth-only, ортографический охват всей арены)
      if (sunShadowsOn) {
        const ld = atm.sunDir;
        M4.viewLookAt(lightViewMat, ld[0] * 175, ld[1] * 175, ld[2] * 175, 0, 0, 0);
        M4.ortho(lightProjMat, -80, 80, -80, 80, 20, 400);
        M4.mul(lightVPMat, lightProjMat, lightViewMat);
        gl.bindFramebuffer(gl.FRAMEBUFFER, shadowFB);
        gl.viewport(0, 0, SHADOW_RES, SHADOW_RES);
        gl.clear(gl.DEPTH_BUFFER_BIT);
        gl.disable(gl.CULL_FACE);
        gl.depthFunc(gl.LEQUAL);
        gl.useProgram(depthProg);
        gl.uniformMatrix4fv(UDepth.lightVP, false, lightVPMat);
        for (let i = 0; i < staticProps.length; i++) depthDraw(staticProps[i].mesh, staticProps[i].model);
        S.barrels.forEach(b => { if (b.alive) { M4.trs(tmpM, b.x, 0.85, b.z, 0, 0, 0, 1.15, 1.7, 1.15); depthDraw("cyl", tmpM); } });
        S.turrets.forEach(t => { M4.trs(tmpM, t.x, 0.95, t.z, t.yaw, 0, 0, 0.65, 1.9, 0.95); depthDraw("cone", tmpM); });
        S.pads.forEach(p => { M4.trs(tmpM, p.x, 0.12, p.z, 0, 0, 0, 2.9, 0.24, 2.9); depthDraw("cyl", tmpM); });
        // курицы — аппроксимация объёмом туловища (дёшево, для солнечной тени достаточно)
        S.chickens.forEach(c => {
          const sc = c.type === "boss" ? 1.85 : c.type === "kamikaze" ? 0.82 : 1.0;
          const yawC = Math.atan2(S.x - c.x, S.z - c.z);
          M4.trs(tmpM, c.x, (c.y || 0) + 0.95 * sc, c.z, yawC, 0, 0, 1.5 * sc, 1.6 * sc, 1.15 * sc);
          depthDraw("box", tmpM);
        });
        S.corpses.forEach(cp => {
          M4.trs(tmpM, cp.x, 0.45 * cp.sc, cp.z, cp.yaw || 0, 0, 0, 1.5 * cp.sc, 0.85 * cp.sc, 1.1 * cp.sc);
          depthDraw("box", tmpM);
        });
        gl.enable(gl.CULL_FACE);
        gl.depthFunc(gl.LESS);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, canvas.width, canvas.height);   // вернуть viewport основного пасса
      }
      const baseFov = S.aiming ? (WEAPONS[S.wep].zoom || 62) : (S.dashT > 0 || S.grappleOn ? 98 : 88);
      M4.persp(projMat, baseFov * Math.PI / 180, canvas.width / Math.max(1, canvas.height), 0.12, 260.0);

      const bobY = active ? Math.abs(Math.sin(S.bobPhase)) * 0.055 : 0;
      const shX = (Math.random() - 0.5) * S.shake, shY = (Math.random() - 0.5) * S.shake;
      const camX = S.x + shX, camY = S.y + bobY + shY, camZ = S.z;
      M4.viewFPS(viewMat, camX, camY, camZ, S.yaw, S.pitch + S.recoil * 0.55, S.grappleOn ? 0.04 : 0);
      M4.mul(vpMat, projMat, viewMat);

      // 1) Skybox Cubemap Pass
      gl.depthFunc(gl.LEQUAL);
      gl.disable(gl.CULL_FACE);
      gl.useProgram(skyProg);
      const viewNoT = new Float32Array(viewMat);
      viewNoT[12] = viewNoT[13] = viewNoT[14] = 0;
      M4.mul(skyVP, projMat, viewNoT);
      gl.uniformMatrix4fv(USky.vp, false, skyVP);
      gl.activeTexture(gl.TEXTURE3);
      gl.bindTexture(gl.TEXTURE_CUBE_MAP, envCube);
      gl.uniform1i(USky.cube, 3);
      gl.uniform3fv(USky.tint, atm.skyTint);
      gl.uniform3fv(USky.fog, atm.fogCol);
      gl.bindVertexArray(meshes.box.vao);
      gl.drawElements(gl.TRIANGLES, meshes.box.count, gl.UNSIGNED_SHORT, 0);
      gl.enable(gl.CULL_FACE);
      gl.depthFunc(gl.LESS);

      // 2) Main PBR Scene Pass
      gl.useProgram(pbrProg);
      gl.uniformMatrix4fv(U.uVP, false, vpMat);
      gl.uniform1i(U.uAlbedoMap, 0);
      gl.uniform1i(U.uNormalMap, 1);
      gl.uniform1i(U.uRoughMetalMap, 2);
      gl.uniform1i(U.uEnvCube, 3);
      gl.uniform1i(U.uShadowMap, 4);
      gl.uniform1f(U.uUseShadows, sunShadowsOn ? 1.0 : 0.0);
      if (sunShadowsOn) {
        gl.uniformMatrix4fv(U.uLightVP, false, lightVPMat);
        gl.activeTexture(gl.TEXTURE4);
        gl.bindTexture(gl.TEXTURE_2D, shadowDepthTex);
      }
      gl.uniform3f(U.uCamPos, camX, camY, camZ);
      gl.uniform3fv(U.uSunDir, atm.sunDir);
      gl.uniform3fv(U.uSunColor, atm.sunCol);
      gl.uniform3fv(U.uSkyAmb, atm.skyAmb);
      gl.uniform3fv(U.uFogColor, atm.fogCol);
      gl.uniform1f(U.uFogDensity, atm.fogD);

      for (let i = 0; i < 4; i++) {
        const L = dynLights[i];
        if (L) {
          gl.uniform4f(U[`uPointPos[${i}]`], L.x, L.y, L.z, 1.0 / L.range);
          gl.uniform3f(U[`uPointCol[${i}]`], L.r, L.g, L.b);
        } else {
          gl.uniform4f(U[`uPointPos[${i}]`], 0, 0, 0, 0);
        }
      }

      // Static Arena Props
      for (let i = 0; i < staticProps.length; i++) {
        const p = staticProps[i];
        drawMesh(p.mesh, p.mat, p.model, p.uvs, p.tint, p.emissive, p.rough, p.metal);
      }

      // 2.4: Blob-тени под динамическими объектами (контактное затемнение)
      if (shadowMat) {
        gl.enable(gl.BLEND);
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
        gl.depthMask(false);
        const drawBlobShadow = (sx, sz, radius, height, strength) => {
          const h = Math.max(0, height || 0);
          const fade = Math.max(0.15, 1.0 - h / 7.5);           // выше → тень бледнее
          const spread = 1.0 + h * 0.10;                        // и чуть шире
          M4.trs(tmpM, sx, 0.06, sz, 0, 0, 0, radius * 2 * spread, 0.012, radius * 2 * spread);
          drawMesh("box", shadowMat, tmpM, [1, 1], [1, 1, 1, strength * fade], [0, 0, 0], 1.0, -1);
        };
        S.chickens.forEach(c => drawBlobShadow(c.x, c.z, c.type === "boss" ? 2.0 : c.type === "drone" ? 1.3 : 1.0, c.y || 0, 0.5));
        S.corpses.forEach(cp => drawBlobShadow(cp.x, cp.z, 1.1 * cp.sc, 0, 0.4));
        S.barrels.forEach(b => { if (b.alive) drawBlobShadow(b.x, b.z, 0.72, 0, 0.42); });
        S.turrets.forEach(t => drawBlobShadow(t.x, t.z, 0.68, 0, 0.4));
        gl.depthMask(true);
        gl.disable(gl.BLEND);
      }

      // Overhead Spinning Holographic Sky-Ring
      M4.trs(tmpM, 0, 14.5 + Math.sin(now*0.0015)*0.6, 0, now*0.0006, 0, 0, 18, 0.35, 18);
      drawMesh("cyl", null, tmpM, [1,1], [0.2,0.9,1,1], [0.35, 2.2, 2.8], 0.15, 0.9);

      // Jump Pads
      S.pads.forEach((p, idx) => {
        const pulse = 1.6 + Math.sin(now*0.006 + idx)*0.45;
        M4.trs(tmpM, p.x, 0.12, p.z, 0, 0, 0, 2.9, 0.24, 2.9);
        drawMesh("cyl", MATS.metal, tmpM, [1,1], [0.3,0.4,0.55,1], [0,0,0], 0.3, 0.9);
        M4.trs(tmpM, p.x, 0.27, p.z, now*0.003, 0, 0, 2.2, 0.08, 2.2);
        drawMesh("cyl", null, tmpM, [1,1], [0.2,0.95,1,1], [0.2*pulse, 1.3*pulse, 1.7*pulse], 0.15, 0.8);
      });

      // Pickups
      S.pickups.forEach(pk => {
        if (pk.cd > 0) return;
        const bob = 0.85 + Math.sin(now*0.004 + pk.x)*0.18;
        const em = pk.kind === "hp" ? [0.2, 2.4, 0.7] : pk.kind === "armor" ? [0.2, 1.7, 2.6] : [2.6, 1.8, 0.3];
        M4.trs(tmpM, pk.x, bob, pk.z, now*0.0025, 0.4, 0, 0.75, 0.75, 0.75);
        drawMesh("box", null, tmpM, [1,1], [1,1,1,1], em, 0.15, 0.8);
      });

      // Explosive C4 Barrels
      S.barrels.forEach(b => {
        if (!b.alive) return;
        M4.trs(tmpM, b.x, 0.85, b.z, 0, 0, 0, 1.15, 1.7, 1.15);
        drawMesh("cyl", MATS.hazard, tmpM, [1,1], [1,1,1,1], [0.35, 0.04, 0.02], 0.55, 0.65);
      });

      // Ally Cyber-Turrets
      S.turrets.forEach(t => {
        M4.trs(tmpM, t.x, 0.5, t.z, 0, 0, 0, 0.85, 1.0, 0.85);
        drawMesh("cone", MATS.metal, tmpM, [1,1], [0.5,0.6,0.75,1], [0,0,0], 0.3, 0.9);
        M4.trs(tmpM, t.x, 1.15, t.z, t.yaw, 0, 0, 0.65, 0.42, 0.95);
        drawMesh("box", MATS.panel, tmpM, [1,1], [0.3,0.85,1,1], [0.1,0.8,1.1], 0.25, 0.88);
      });

      // 3D Articulated Cyber-Chickens (6 Breeds!) — 2.4: ноги с шагом, хвост-веер, шея,
      // выпад клювом при атаке, откидывание при попадании, ярость босса пульсирует
      S.chickens.forEach(c => {
        const scBase = c.type === "boss" ? 1.85 : c.type === "kamikaze" ? 0.82 : 1.0;
        const ragePulse = c.enraged ? 1.0 + 0.055 * Math.sin(now * 0.012) : 1.0;
        const sc = scBase * ragePulse;
        const dpX = S.x - c.x, dpZ = S.z - c.z, distP = Math.hypot(dpX, dpZ);
        const panic = c.type === "kamikaze" && distP < 7 ? 1.0 + 0.07 * Math.sin(now * 0.03) : 1.0;
        const yawC = Math.atan2(dpX, dpZ);
        const cy = Math.cos(yawC), sy = Math.sin(yawC);
        const fwdX = sy, fwdZ = cy;
        const waddle = Math.sin(c.phase) * 0.08;
        const flinchK = c.flinchT > 0 ? Math.sin((Math.min(0.22, Math.max(0, c.flinchT)) / 0.22) * Math.PI) : 0;
        const lungeDen = c.type === "boss" ? 0.34 : 0.28;
        const lungeK = c.lungeT > 0 ? Math.sin((1 - Math.min(1, Math.max(0, c.lungeT) / lungeDen)) * Math.PI) : 0;
        const bodyBack = flinchK * 0.34;
        const bodyX = c.x - fwdX * bodyBack, bodyZ = c.z - fwdZ * bodyBack;
        const bodyPitch = -flinchK * 0.5;
        const breathe = 1.0 + 0.028 * Math.sin(now * 0.004 + c.phase * 0.3);
        const tint = c.type === "kamikaze" ? [1.0,0.34,0.14,1] : c.type === "drone" ? [0.32,0.88,1.0,1]
                   : c.type === "charger"  ? [0.92,0.48,0.18,1] : c.type === "shooter" ? [0.22,0.25,0.34,1]
                   : c.type === "boss"     ? [0.95,0.76,0.28,1] : [0.96,0.95,0.90,1];
        const emBody = c.enraged ? [1.4, 0.15, 0.1] : [0,0,0];

        // Ноги + лапы (чередующийся шаг; дрон поджимает лапы)
        const legSwing = c.type === "drone" ? -0.62 : Math.sin(c.phase) * 0.72;
        const hipY = c.y + 0.42 * sc, legLen = 0.46 * sc;
        const legW = (c.type === "boss" ? 0.17 : 0.095) * sc;
        [-1, 1].forEach(side => {
          const swing = legSwing * side;
          const hipX = c.x + cy * side * 0.30 * sc, hipZ = c.z - sy * side * 0.30 * sc;
          const legM = M4.ident();
          M4.trs(legM, hipX, hipY, hipZ, yawC, swing, 0, 1, 1, 1);
          const legOff = M4.ident();
          M4.trs(legOff, 0, -legLen * 0.5, 0, 0, 0, 0, legW, legLen, legW);
          M4.mul(legM, legM, legOff);
          drawMesh("cyl", null, legM, [1,1], [0.98, 0.62, 0.12, 1], [0,0,0], 0.45, 0.2);
          const footY = hipY - legLen * Math.cos(swing) + 0.045;
          const footFwd = -Math.sin(swing) * legLen;
          M4.trs(tmpM, hipX + fwdX * footFwd, footY, hipZ + fwdZ * footFwd, yawC, 0, 0, 0.20 * sc, 0.09 * sc, 0.30 * sc);
          drawMesh("box", null, tmpM, [1,1], [0.82, 0.47, 0.07, 1], [0,0,0], 0.5, 0.15);
        });

        // Body ellipsoid (дыхание + паника камикадзе + flinch-откидывание)
        M4.trs(tmpM, bodyX, c.y + 0.88*sc, bodyZ, yawC, bodyPitch, waddle, 0.95*sc*breathe*panic, 0.88*sc/breathe, 1.18*sc*panic);
        drawMesh("sph", MATS.feather, tmpM, [2,2], tint, emBody, 0.72, 0.05);

        // Кибер-пластина «задней брони» сзади
        M4.trs(tmpM, bodyX - fwdX*0.52*sc, c.y + 0.74*sc, bodyZ - fwdZ*0.52*sc, yawC, bodyPitch, 0, 0.52*sc, 0.3*sc, 0.18*sc);
        drawMesh("box", MATS.panel, tmpM, [1,1], [0.3, 0.35, 0.45, 1], c.type === "drone" ? [0.1, 0.7, 0.9] : [0, 0, 0], 0.3, 0.88);

        // Хвост — веер из трёх перьев
        const tailBob = Math.sin(c.phase * 0.5 + 1.3) * 0.09;
        [-0.5, 0, 0.5].forEach((fan, ti) => {
          const tailY = c.y + (1.04 + (ti === 1 ? 0.08 : 0)) * sc;
          const td = 0.8;
          M4.trs(tmpM, bodyX - fwdX*0.60*sc, tailY, bodyZ - fwdZ*0.60*sc, yawC + fan*0.6, -0.98 + tailBob + (ti === 1 ? -0.12 : 0), fan*0.55, 0.30*sc, 0.55*sc, 0.09*sc);
          drawMesh("box", MATS.feather, tmpM, [1,1], [tint[0]*td, tint[1]*td, tint[2]*td, 1], [0,0,0], 0.72, 0.05);
        });

        // Шея (перьевое кольцо)
        M4.trs(tmpM, bodyX + fwdX*0.14*sc, c.y + 1.24*sc, bodyZ + fwdZ*0.14*sc, yawC, 0.4 + bodyPitch*0.5, 0, 0.32*sc, 0.28*sc, 0.32*sc);
        drawMesh("cyl", MATS.feather, tmpM, [1,1], tint, emBody, 0.7, 0.05);

        // Head (выпад вперёд при атаке)
        const headLunge = lungeK * 0.38 * sc;
        const hx = bodyX + sy*0.42*sc + fwdX*headLunge, hy = c.y + 1.50*sc + Math.abs(Math.sin(c.phase))*0.06 + lungeK*0.04, hz = bodyZ + cy*0.42*sc + fwdZ*headLunge;
        M4.trs(tmpM, hx, hy, hz, yawC, bodyPitch + lungeK*0.6, 0, 0.56*sc, 0.56*sc, 0.58*sc);
        drawMesh("sph", MATS.feather, tmpM, [1,1], tint, emBody, 0.68, 0.05);

        // Red Comb (or Golden Crown for Boss!)
        M4.trs(tmpM, hx, hy + 0.34*sc, hz, yawC, bodyPitch + lungeK*0.6, 0, 0.16*sc, 0.28*sc, 0.36*sc);
        drawMesh("box", null, tmpM, [1,1], c.type === "boss" ? [1,0.85,0.2,1] : [0.9,0.1,0.18,1],
                 c.type === "boss" ? [2.2,1.6,0.2] : [0.5,0.02,0.04], 0.25, c.type === "boss" ? 0.95 : 0.1);

        // Orange Beak
        M4.trs(tmpM, hx + sy*0.34*sc + fwdX*headLunge*0.4, hy - 0.03*sc, hz + cy*0.34*sc + fwdZ*headLunge*0.4, yawC, Math.PI*0.5 + bodyPitch + lungeK*0.45, 0, 0.20*sc, 0.32*sc, 0.20*sc);
        drawMesh("cone", null, tmpM, [1,1], [1.0,0.68,0.12,1], [0,0,0], 0.35, 0.1);

        // Красная бородка под клювом
        M4.trs(tmpM, hx + sy*0.26*sc, hy - 0.20*sc, hz + cy*0.26*sc, yawC, bodyPitch + lungeK*0.5, 0, 0.10*sc, 0.16*sc, 0.08*sc);
        drawMesh("box", null, tmpM, [1,1], [0.92, 0.10, 0.14, 1], [0.35, 0.01, 0.02], 0.4, 0.1);

        // Cyber Glowing Visor
        M4.trs(tmpM, hx + sy*0.24*sc, hy + 0.06*sc, hz + cy*0.24*sc, yawC, bodyPitch + lungeK*0.6, 0, 0.42*sc, 0.11*sc, 0.14*sc);
        drawMesh("box", null, tmpM, [1,1], [1,0.2,0.2,1], c.type === "drone" ? [0.2,2.4,2.8] : [2.8,0.25,0.3], 0.1, 0.9);

        // Flapping Wings (размах при flinch; дрон машет быстрее)
        const flapAmp = c.type === "drone" ? 2.2 : 1.0;
        const flap = Math.sin(c.phase * flapAmp) * (0.45 + flinchK * 0.42);
        [-1, 1].forEach(side => {
          M4.trs(tmpM, bodyX + cy*side*0.52*sc, c.y + 0.92*sc, bodyZ - sy*side*0.52*sc, yawC, bodyPitch, side*(flap + flinchK*0.3), 0.22*sc, 0.46*sc, 0.78*sc);
          drawMesh("box", MATS.feather, tmpM, [1,1], tint, [0,0,0], 0.7, 0.05);
        });

        // Special breed gear: Kamikaze C4 blinking beacon / Drone gyro-halo / Boss golden pauldrons
        if (c.type === "kamikaze") {
          const near = distP < 7;
          const blink = (Math.sin(now * (near ? 0.055 : 0.025)) > 0) ? 3.6 : 0.35;
          M4.trs(tmpM, bodyX, c.y + 1.34*sc, bodyZ - cy*0.15, yawC, 0, 0, 0.35*panic, 0.25*panic, 0.35*panic);
          drawMesh("box", null, tmpM, [1,1], [1,0.2,0.1,1], [blink, 0.2, 0.1], 0.2, 0.8);
        } else if (c.type === "drone") {
          M4.trs(tmpM, bodyX - sy*0.45, c.y + 0.55, bodyZ - cy*0.45, yawC, 0, 0, 0.55, 0.35, 0.32);
          drawMesh("cyl", null, tmpM, [1,1], [0.2,0.9,1,1], [0.3, 2.4, 2.8], 0.2, 0.9);
          // гироскопическое «кольцо-нимб» над дроном
          M4.trs(tmpM, bodyX, c.y + 1.02*sc, bodyZ, yawC, 0, 0, 0.72*sc, 0.06*sc, 0.72*sc);
          drawMesh("cyl", null, tmpM, [1,1], [0.45, 0.85, 1, 1], [0.2, 1.5, 1.9], 0.25, 0.75);
        } else if (c.type === "boss") {
          // золотые наплечники Мега-Индюка
          [-1, 1].forEach(side => {
            M4.trs(tmpM, bodyX + cy*side*0.55*sc, c.y + 1.16*sc, bodyZ - sy*side*0.55*sc, yawC, 0, side*0.22, 0.44*sc, 0.20*sc, 0.52*sc);
            drawMesh("box", MATS.metal, tmpM, [1,1], [0.78, 0.62, 0.22, 1], [0.9, 0.62, 0.08], 0.26, 0.93);
          });
        }
      });

      // 2.4: Трупы кибер-куриц — опрокинуты, остывают и уходят под пол
      S.corpses.forEach(cp => {
        const fall = Math.min(1, (4.2 - cp.ttl) / 0.5);
        const ease = 1 - Math.pow(1 - fall, 3);
        const sink = cp.ttl < 1.15 ? (1.15 - cp.ttl) * 0.5 : 0;
        const sc = cp.sc;
        const cy0 = Math.cos(cp.yaw), sy0 = Math.sin(cp.yaw);
        const dropFrom = cp.y || 0;
        const bodyTop = dropFrom + 0.88 * sc;
        const bodyY = bodyTop + (0.34 * sc - bodyTop) * ease - sink;
        const dead = cp.type === "kamikaze" ? [0.55,0.22,0.12,1] : cp.type === "drone" ? [0.20,0.38,0.46,1]
                   : cp.type === "charger"  ? [0.50,0.30,0.16,1] : cp.type === "shooter" ? [0.16,0.17,0.22,1]
                   : cp.type === "boss"     ? [0.52,0.43,0.20,1] : [0.44,0.44,0.42,1];
        // тело на спине
        M4.trs(tmpM, cp.x, bodyY, cp.z, cp.yaw, -1.45 * ease, cp.spin, 0.95*sc, 0.80*sc, 1.18*sc);
        drawMesh("sph", MATS.feather, tmpM, [2,2], dead, [0,0,0], 0.8, 0.02);
        // голова, отвалившаяся набок
        const headTop = dropFrom + 1.5 * sc;
        const headY = headTop + (0.24 * sc - headTop) * ease - sink;
        const hx = cp.x + cy0 * 1.0 * sc, hz = cp.z - sy0 * 1.0 * sc;
        M4.trs(tmpM, hx, Math.max(0.24 * sc - sink, headY), hz, cp.yaw + cp.spin * 1.7, -1.5, cp.spin, 0.52*sc, 0.48*sc, 0.52*sc);
        drawMesh("sph", MATS.feather, tmpM, [1,1], dead, [0,0,0], 0.8, 0.02);
        // обломок гребня
        M4.trs(tmpM, cp.x - cy0 * 0.85 * sc, 0.07 * sc - sink, cp.z + sy0 * 0.85 * sc, cp.yaw + cp.spin, 0, cp.spin * 2, 0.16*sc, 0.07*sc, 0.3*sc);
        drawMesh("box", null, tmpM, [1,1], [0.66,0.08,0.11,1], [0,0,0], 0.5, 0.1);
      });

      // Plasma Orbs, Enemy Eggs, Grenades & Particles
      S.orbs.forEach(o => {
        M4.trs(tmpM, o.x, o.y, o.z, 0, 0, 0, 0.42, 0.42, 0.42);
        drawMesh("sph", null, tmpM, [1,1], [0.2,1,0.6,1], [0.4, 3.0, 1.5], 0.1, 0.5);
      });
      S.enemyEggs.forEach(eg => {
        M4.trs(tmpM, eg.x, eg.y, eg.z, 0, 0, 0, 0.32, 0.40, 0.32);
        drawMesh("sph", null, tmpM, [1,1], [1,0.3,0.4,1], [2.4, 0.4, 0.5], 0.2, 0.3);
      });
      S.grenades.forEach(g => {
        M4.trs(tmpM, g.x, g.y, g.z, now*0.01, now*0.01, 0, 0.36, 0.44, 0.36);
        drawMesh("sph", null, tmpM, [1,1], [1,0.85,0.2,1], [2.2, 1.5, 0.2], 0.2, 0.8);
      });
      S.particles.forEach(p => {
        M4.trs(tmpM, p.x, p.y, p.z, p.ttl*6, p.ttl*4, 0, p.sz, p.sz, p.sz);
        drawMesh("box", null, tmpM, [1,1], [1,1,1,1], p.col, 0.3, 0.2);
      });

      // 3D Weather Motes matching [F8] Atmosphere
      const wEm = S.atmIdx === 1 ? [0.2,2.2,2.8] : S.atmIdx === 2 ? [2.6,0.35,0.3] : S.atmIdx === 3 ? [1.8,2.0,2.3] : [2.4,1.6,0.4];
      S.weather.forEach(m => {
        m.oy += (S.atmIdx === 2 ? 1 : -1) * m.sp * (S.atmIdx === 1 ? 3.2 : 1.0) * rawDt;
        if (m.oy < -2) m.oy = 12; if (m.oy > 12) m.oy = -2;
        M4.trs(tmpM, camX + m.ox, camY + m.oy - 4, camZ + m.oz, 0, 0, 0, 0.05, S.atmIdx === 1 ? 0.55 : 0.05, 0.05);
        drawMesh("box", null, tmpM, [1,1], [1,1,1,1], wEm, 0.2, 0.2);
      });

      // Weapon Tracers, Railgun Pierce Beams, Grapple Tether & Orbital Ion Strike!
      S.beams.forEach(b => {
        M4.beamBetween(tmpM, b.x1, b.y1, b.z1, b.x2, b.y2, b.z2, b.th);
        drawMesh("cyl", null, tmpM, [1,1], [1,1,1,1], b.col, 0.1, 0.8);
      });
      if (S.grappleOn) {
        const hx = camX + Math.cos(S.yaw)*0.32, hy = camY - 0.24, hz = camZ - Math.sin(S.yaw)*0.32;
        M4.beamBetween(tmpM, hx, hy, hz, S.grapplePos[0], S.grapplePos[1], S.grapplePos[2], 0.055);
        drawMesh("cyl", null, tmpM, [1,1], [0.2,1,1,1], [0.4, 2.8, 3.2], 0.1, 0.9);
      }
      S.orbitalStrikes.forEach(o => {
        const u = 1 - o.ttl / o.maxTtl;
        const w = Math.max(0.1, 3.2 * (1 - u * u));
        M4.trs(tmpM, o.x, 30, o.z, 0, 0, 0, w, 60, w);
        drawMesh("cyl", null, tmpM, [1,1], [0.4,1,1,1], [1.2, 3.6, 4.2], 0.1, 0.9);
        const r = 2 + u * 32;
        M4.trs(tmpM, o.x, 0.18, o.z, 0, 0, 0, r, 0.12, r);
        drawMesh("cyl", null, tmpM, [1,1], [1,0.85,0.2,1], [3.2*(1-u), 2.2*(1-u), 0.4], 0.1, 0.9);
      });

      // 3) 3D First-Person Viewmodel Pass (Weapon + Chicken Wing Hand!)
      if (active) {
        gl.clear(gl.DEPTH_BUFFER_BIT);
        const vmId = M4.ident();
        gl.uniformMatrix4fv(U.uVP, false, projMat);
        gl.uniform3f(U.uCamPos, 0, 0, 0);

        const w = WEAPONS[S.wep];
        const sk = SKINS[S.skin];
        const bobX = Math.cos(S.bobPhase) * 0.018, bobV = Math.abs(Math.sin(S.bobPhase * 2)) * 0.018;
        const adsT = S.aiming ? 1 : 0;
        const wx = (adsT ? 0.0 : 0.28) + bobX;
        const wy = (adsT ? -0.19 : -0.26) - bobV - (S.reloadT > 0 ? 0.18 : 0);
        const wz = -0.52 + S.recoil * 0.32;
        const inspYaw = S.inspectT > 0 ? Math.sin((2.0 - S.inspectT) * Math.PI) * 0.65 : 0;
        const slapRoll = S.meleeT > 0 ? Math.sin((1 - S.meleeT / 0.28) * Math.PI) * 1.1 : 0;

        // Chicken Wing gripping the gun!
        M4.trs(vmId, wx - 0.08, wy - 0.09, wz + 0.12, 0.25 + inspYaw, 0.1, -0.35 + slapRoll, 0.14, 0.09, 0.34);
        drawMesh("sph", MATS.feather, vmId, [1,1], [0.97,0.95,0.89,1], [0,0,0], 0.68, 0.05);

        // Gun Receiver (BrushedMetal PBR + Skin Albedo)
        M4.trs(vmId, wx, wy, wz, inspYaw, S.recoil * 0.6, 0, 0.095, 0.13, w.len);
        drawMesh("box", MATS.metal, vmId, [1,2], [...sk.albedo, 1], [0,0,0], sk.rough, sk.metal);

        // Glowing Neon Barrel / Rail Coils / Plasma Core
        M4.trs(vmId, wx, wy + 0.045, wz - w.len * 0.22, inspYaw, S.recoil * 0.6, 0, 0.048, 0.042, w.len * 0.85);
        drawMesh("box", null, vmId, [1,1], [1,1,1,1], w.pierce ? [0.4,2.8,3.4] : w.plasma ? [0.3,3.2,1.4] : sk.accent, 0.15, 0.9);

        // Scope on Sniper / Railgun
        if (S.wep === 4 || S.wep === 7) {
          M4.trs(vmId, wx, wy + 0.10, wz + 0.02, inspYaw, Math.PI * 0.5 + S.recoil * 0.6, 0, 0.055, 0.26, 0.055);
          drawMesh("cyl", MATS.metal, vmId, [1,1], [0.15,0.18,0.22,1], [0.1,0.5,0.7], 0.2, 0.92);
        }

        // ---- 2.4 VIEWMODEL KIT: дульный тормоз, мушка, целики, лазер-диод, зарядка, скин-кольцо ----
        // дульный тормоз (вдоль ствола)
        M4.trs(vmId, wx, wy + 0.045, wz - w.len*0.62, inspYaw, Math.PI*0.5 + S.recoil*0.6, 0, 0.052, 0.16, 0.052);
        drawMesh("cyl", MATS.metal, vmId, [1,1], [0.20, 0.22, 0.28, 1], [0,0,0], 0.28, 0.95);
        // мушка
        M4.trs(vmId, wx, wy + 0.115, wz - w.len*0.40, inspYaw, S.recoil*0.6, 0, 0.018, 0.075, 0.018);
        drawMesh("box", null, vmId, [1,1], [0.9, 0.75, 0.2, 1], [0.5, 0.35, 0.05], 0.3, 0.7);
        // целик (задняя прицельная планка)
        M4.trs(vmId, wx, wy + 0.105, wz + w.len*0.17, inspYaw, S.recoil*0.6, 0, 0.075, 0.05, 0.025);
        drawMesh("box", MATS.metal, vmId, [1,1], [0.30, 0.32, 0.40, 1], [0,0,0], 0.3, 0.9);
        // лазерный диод (пульсирует, гаснет если лазер выключен)
        const laserPulse = S.laserOn ? 1.6 + Math.sin(now*0.02) * 0.5 : 0.15;
        M4.trs(vmId, wx + 0.075, wy + 0.012, wz - w.len*0.18, inspYaw, S.recoil*0.6, 0, 0.024, 0.024, 0.06);
        drawMesh("box", null, vmId, [1,1], [0.1, 0.9, 0.25, 1], [0.12*laserPulse, 0.9*laserPulse, 0.3*laserPulse], 0.2, 0.6);
        // зарядная рукоять слева
        M4.trs(vmId, wx - 0.085, wy + 0.02, wz + w.len*0.12, inspYaw, S.recoil*0.6, 0, 0.05, 0.028, 0.09);
        drawMesh("box", MATS.metal, vmId, [1,1], [0.55, 0.60, 0.70, 1], [0,0,0], 0.35, 0.9);
        // скин-кольцо на стволе у казённой части (светится в цвет скина)
        M4.trs(vmId, wx, wy + 0.045, wz + w.len*0.30, inspYaw, Math.PI*0.5 + S.recoil*0.6, 0, 0.056, 0.07, 0.056);
        drawMesh("cyl", null, vmId, [1,1], [sk.accent[0], sk.accent[1], sk.accent[2], 1], [sk.accent[0]*0.85, sk.accent[1]*0.85, sk.accent[2]*0.85], 0.18, 0.85);
        // выброс гильзы (тёмная прорезь справа)
        M4.trs(vmId, wx + 0.055, wy + 0.078, wz + w.len*0.02, inspYaw, S.recoil*0.6, 0, 0.012, 0.035, 0.095);
        drawMesh("box", null, vmId, [1,1], [0.08, 0.09, 0.11, 1], [0,0,0], 0.4, 0.6);
      }

      opts.syncHud(S, WEAPONS[S.wep], SKINS[S.skin], ATMOSPHERES[S.atmIdx], MUTATORS[S.mutator], vpMat);
      requestAnimationFrame(tick);
    }

    requestAnimationFrame(tick);
    return {
      state: S,
      saveCredits,
      requestLockSmart,
      setSensitivity,
      cycleAtmosphere() { S.atmIdx = (S.atmIdx + 1) % ATMOSPHERES.length; },
      applyBanyaReward(data) {
        if (!data) return;
        if (typeof data.totalCredits === "number" && Number.isFinite(data.totalCredits)) {
          S.credits = data.totalCredits;
        } else if (typeof data.creditsEarned === "number") {
          S.credits += data.creditsEarned;
          saveCredits();
        }
        if (typeof data.pts === "number" && data.pts > 0) {
          S.score += data.pts;
          if (S.score > S.highScore) {
            S.highScore = S.score;
            try { localStorage.setItem("cg23_hi", String(S.highScore)); } catch (_) {}
          }
        }
        if (typeof data.achMask === "number") {
          S.achiev |= data.achMask;
          try { localStorage.setItem("cg23_ach", String(S.achiev)); } catch (_) {}
        }
        const tier = Math.max(S.banyaBuff || 0, data.bestBuffTier || 0, data.buffTier || 0);
        if (tier > S.banyaBuff) S.banyaBuff = tier;
        if ((data.buffTier || 0) > 0) {
          const bonusHp  = tier >= 2 ? 50 : 25;
          const bonusArm = tier >= 2 ? 50 : 25;
          S.maxHp = Math.max(S.maxHp, 100 + bonusHp + 25 * S.perks[0]);
          S.hp    = Math.min(S.maxHp, S.hp + 60);
          S.armor = Math.min(100, S.armor + bonusArm);
          if (tier >= 2) S.orbitalPct = Math.min(100, S.orbitalPct + 35);
          opts.playSfx("Achievement.wav", 0.85);
        }
      }
    };
  };
})();

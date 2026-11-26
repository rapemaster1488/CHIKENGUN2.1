// ============================================================================
// CHIKENGUN 2.4 — Hardware-Accelerated WebGL2 HDR PBR 3D Arena Engine
// Zero external dependencies. Multi-pass renderer:
//   1) Directional sun shadow map (DEPTH_COMPONENT24, 1024/2048², 3x3 PCF)
//   2) GGX Cook-Torrance PBR in linear HDR (RGBA16F + optional MSAA, resolved
//      with blitFramebuffer) — real repo textures (Albedo + Tangent Normal +
//      RoughMetal), 6-face Sky4K cubemap IBL, ground contact AO, 4 point lights
//   3) Unlit FX pass — soft additive ember/flash billboards, scorch decals,
//      shockwave rings, 3D HP bars, muzzle starbursts
//   4) Post chain — bright pass, separable 9-tap gaussian bloom on two mips,
//      ACES filmic tonemap, contrast-adaptive sharpening, radial chromatic
//      warp on dash/grapple/hurt, overdrive + slow-mo grading, vignette, grain
// plus hierarchical 3D rigs: 9 distinct animated weapon viewmodels with
// 3-toed claw hands, brass/energy shell ejection, [L] laser sight, and 6 fully
// articulated cyber-chicken breeds (legs, claws, crest, wattle, beak, pupils,
// wing fans, tail fans, armor) with hit-flash, flinch, tumbling death ragdolls
// and fluttering 3D feathers.
// AABB 3D physics, Grapple Hook [Z], Orbital Ion Strike [X], Wave Mutators,
// Boss Enrage + ground slam, 9 Weapons & 6 Breeds, adaptive resolution and the
// [F4] quality presets (УЛЬТРА / ВЫСОКИЕ / ТУРБО FPS) for laptop GPUs.
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
    },
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
    lookAt(out, ex, ey, ez, cx, cy, cz, ux, uy, uz) {
      let zx = ex - cx, zy = ey - cy, zz = ez - cz;
      let zl = Math.hypot(zx, zy, zz) || 1;
      zx /= zl; zy /= zl; zz /= zl;
      let xx = uy * zz - uz * zy, xy = uz * zx - ux * zz, xz = ux * zy - uy * zx;
      const xl = Math.hypot(xx, xy, xz) || 1;
      xx /= xl; xy /= xl; xz /= xl;
      const yx = zy * xz - zz * xy, yy = zz * xx - zx * xz, yz = zx * xy - zy * xx;
      out[0] = xx; out[4] = xy; out[8] = xz;  out[12] = -(xx*ex + xy*ey + xz*ez);
      out[1] = yx; out[5] = yy; out[9] = yz;  out[13] = -(yx*ex + yy*ey + yz*ez);
      out[2] = zx; out[6] = zy; out[10] = zz; out[14] = -(zx*ex + zy*ey + zz*ez);
      out[3] = 0;  out[7] = 0;  out[11] = 0;  out[15] = 1;
      return out;
    },
    // out = a * b, safe when out aliases neither a nor b only
    clone(m) { return new Float32Array(m); }
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

  void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    mat3 nm = mat3(uModel);
    vNormal  = normalize(nm * aNormal);
    vTangent = normalize(nm * aTangent);
    vUV = aUV * uUVScale;
    gl_Position = uVP * wp;
  }`;

  const FS_PBR = `#version 300 es
  precision highp float;
  in vec3 vWorldPos;
  in vec3 vNormal;
  in vec3 vTangent;
  in vec2 vUV;

  uniform sampler2D uAlbedoMap;
  uniform sampler2D uNormalMap;
  uniform sampler2D uRoughMetalMap;
  uniform samplerCube uEnvCube;

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

  uniform sampler2D uShadowMap;
  uniform mat4 uLightVP;
  uniform float uShadowTexel;
  uniform float uShadowOn;
  uniform float uTonemap;   // 1.0 = tonemap here (no post-process framebuffer available)

  out vec4 fragColor;

  vec3 ACESFilm(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
  }

  // 3x3 rotated PCF + slope-scaled bias
  float shadowFactor(vec3 wp, vec3 N, vec3 L) {
    if (uShadowOn < 0.5) return 1.0;
    vec4 lp = uLightVP * vec4(wp + N * 0.055, 1.0);
    vec3 pc = lp.xyz / lp.w * 0.5 + 0.5;
    if (pc.x < 0.004 || pc.x > 0.996 || pc.y < 0.004 || pc.y > 0.996 || pc.z > 1.0) return 1.0;
    float bias = max(0.0009, 0.0045 * (1.0 - max(dot(N, L), 0.0)));
    float sum = 0.0;
    for (int y = -1; y <= 1; y++) {
      for (int x = -1; x <= 1; x++) {
        float d = texture(uShadowMap, pc.xy + vec2(float(x), float(y)) * uShadowTexel * 1.35).r;
        sum += (pc.z - bias) > d ? 0.0 : 1.0;
      }
    }
    return sum / 9.0;
  }

  void main() {
    vec3 N = normalize(vNormal);
    vec3 albedo = uTint.rgb;
    float rough = clamp(uPbrOverride.x, 0.06, 0.98);
    float metal = clamp(max(uPbrOverride.y, 0.0), 0.0, 1.0);

    if (uUseTex > 0.5) {
      vec3 texCol = pow(texture(uAlbedoMap, vUV).rgb, vec3(2.2));
      albedo *= texCol;
      vec3 T = normalize(vTangent - N * dot(vTangent, N));
      vec3 B = cross(N, T);
      vec3 tn = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
      tn.xy *= 0.85;
      N = normalize(T * tn.x + B * tn.y + N * tn.z);
      vec3 rm = texture(uRoughMetalMap, vUV).rgb;
      rough = clamp(rm.g * uPbrOverride.x, 0.06, 0.98);
      metal = (uPbrOverride.y >= 0.0) ? uPbrOverride.y : clamp(rm.b, 0.0, 1.0);
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
    float shadow = shadowFactor(vWorldPos, N, L);
    vec3 sunLit = (kD * albedo / 3.14159 + D * F * 0.28) * uSunColor * NdotL * mix(0.26, 1.0, shadow);

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
    // Ground contact AO: fades in near the floor & on downward faces
    float groundAO = clamp(smoothstep(0.0, 1.35, vWorldPos.y) * 0.86 + 0.14 + N.y * 0.16, 0.0, 1.0);
    vec3 amb = (kD * albedo * uSkyAmb * mix(0.38, 1.05, hemi) + F * envSample * (1.0 - rough * 0.72)) * groundAO;

    vec3 color = amb + sunLit + ptLit + uEmissive;

    // Exponential squared atmospheric fog
    float dist = length(vWorldPos - uCamPos);
    float fogF = 1.0 - exp(-pow(dist * uFogDensity, 1.65));
    color = mix(color, uFogColor, clamp(fogF, 0.0, 0.92));

    // Usually linear HDR out (ACES + bloom + CAS live in the post pass).
    // uTonemap = 1 is the safe fallback when no FBO can be created.
    if (uTonemap > 0.5) color = pow(ACESFilm(color * 1.06), vec3(1.0 / 2.2));
    fragColor = vec4(color, uTint.a);
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
  uniform vec3 uSunDirSky;
  uniform vec3 uSunColSky;
  out vec4 fragColor;
  void main() {
    vec3 d = normalize(vDir);
    vec3 tex = texture(uEnvCube, d).rgb * uSkyTint;
    float h = clamp(1.0 - abs(d.y) * 2.2, 0.0, 1.0);
    vec3 col = mix(tex, uFogColor, h * h * 0.65);
    // Sun glow halo on the sky dome (linear HDR, bloom picks this up)
    float sd = max(dot(d, normalize(uSunDirSky)), 0.0);
    col += uSunColSky * (pow(sd, 620.0) * 2.1 + pow(sd, 26.0) * 0.16);
    fragColor = vec4(col, 1.0);
  }`;

  // Shadow depth-only pass (1024/2048² directional sun shadow map, 3x3 PCF on lookup)
  const VS_SHADOW = `#version 300 es
  precision highp float;
  layout(location=0) in vec3 aPos;
  uniform mat4 uLightVP;
  uniform mat4 uModel;
  void main() { gl_Position = uLightVP * uModel * vec4(aPos, 1.0); }`;

  const FS_SHADOW = `#version 300 es
  precision highp float;
  void main() { }`;

  // Unlit additive FX (embers, flash sprites, scorch decals, telegraph rings)
  const VS_FX = `#version 300 es
  precision highp float;
  layout(location=0) in vec3 aPos;
  layout(location=3) in vec2 aUV;
  uniform mat4 uVP;
  uniform mat4 uModel;
  out vec2 vUV;
  out vec3 vWP;
  void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWP = wp.xyz;
    vUV = aUV;
    gl_Position = uVP * wp;
  }`;

  const FS_FX = `#version 300 es
  precision highp float;
  in vec2 vUV;
  in vec3 vWP;
  uniform vec3 uColor;
  uniform float uAlpha;
  uniform float uMode;     // 0 = soft round core, 1 = flat decal, 2 = hero glow
  uniform float uAdditive;
  uniform vec3 uCamPos;
  uniform vec3 uFogColor;
  uniform float uFogDensity;
  uniform float uFlicker;
  out vec4 fragColor;
  void main() {
    vec2 p = vUV * 2.0 - 1.0;
    float r2 = dot(p, p);
    if (r2 > 1.0) discard;
    float a = 1.0;
    if (uMode > 1.5) {
      a = pow(1.0 - sqrt(r2), 2.4) * 1.6 + pow(max(0.0, 1.0 - sqrt(r2) * 3.4), 2.0) * 1.9;
    } else if (uMode > 0.5) {
      a = 1.0;
    } else {
      a = pow(1.0 - sqrt(r2), 1.7);
    }
    float dist = length(vWP - uCamPos);
    float fogF = 1.0 - exp(-pow(dist * uFogDensity, 1.65));
    vec3 col = uColor * a * uAlpha * uFlicker;
    if (uAdditive > 0.5) {
      col *= (1.0 - clamp(fogF, 0.0, 0.92) * 0.8);
      fragColor = vec4(col, 1.0);
    } else {
      fragColor = vec4(mix(col, uFogColor * 0.1, clamp(fogF, 0.0, 0.9)), a * uAlpha);
    }
  }`;

  // Screen-space post: 4-tap bright-pass downsample, separable gaussian bloom, final grade
  const VS_FULL = `#version 300 es
  precision highp float;
  layout(location=0) in vec3 aPos;
  layout(location=3) in vec2 aUV;
  out vec2 vUV;
  void main() {
    vUV = aUV;
    // quad vertices are -0.5..0.5, so scale up to full NDC coverage
    gl_Position = vec4(aPos.xy * 2.0, 0.0, 1.0);
  }`;

  const FS_BRIGHT = `#version 300 es
  precision highp float;
  in vec2 vUV;
  uniform sampler2D uTex;
  uniform vec2 uTexel;
  uniform float uThreshold;
  out vec4 fragColor;
  void main() {
    vec3 c = texture(uTex, vUV + uTexel * vec2(-1.0, -1.0)).rgb
           + texture(uTex, vUV + uTexel * vec2( 1.0, -1.0)).rgb
           + texture(uTex, vUV + uTexel * vec2(-1.0,  1.0)).rgb
           + texture(uTex, vUV + uTexel * vec2( 1.0,  1.0)).rgb;
    c *= 0.25;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float knee = uThreshold * 0.65;
    float soft = clamp((lum - uThreshold + knee) / max(knee, 0.0001), 0.0, 1.0);
    soft = soft * soft * knee;
    float contrib = max(soft, lum - uThreshold) / max(lum, 0.0001);
    fragColor = vec4(c * clamp(contrib, 0.0, 1.0), 1.0);
  }`;

  const FS_BLUR = `#version 300 es
  precision highp float;
  in vec2 vUV;
  uniform sampler2D uTex;
  uniform vec2 uDir;       // texel-space * direction
  out vec4 fragColor;
  void main() {
    // 9-tap gaussian (3.0,2.0,1.0 weights)
    vec3 c = texture(uTex, vUV).rgb * 0.2270270270;
    c += texture(uTex, vUV + uDir * 1.3846153846).rgb * 0.3162162162;
    c += texture(uTex, vUV - uDir * 1.3846153846).rgb * 0.3162162162;
    c += texture(uTex, vUV + uDir * 3.2307692308).rgb * 0.0702702703;
    c += texture(uTex, vUV - uDir * 3.2307692308).rgb * 0.0702702703;
    fragColor = vec4(c, 1.0);
  }`;

  const FS_POST = `#version 300 es
  precision highp float;
  in vec2 vUV;
  uniform sampler2D uScene;
  uniform sampler2D uBloom;
  uniform sampler2D uBloom2;
  uniform float uBloomStr;
  uniform vec2  uResolution;
  uniform float uTime;
  uniform float uHurt;
  uniform float uOverdrive;
  uniform float uSlowMo;
  uniform float uVignette;
  uniform float uSaturation;
  uniform float uExposure;
  uniform float uSharp;
  uniform float uDash;
  uniform float uGrapple;
  out vec4 fragColor;

  vec3 ACESFilm(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
  }
  float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

  void main() {
    vec2 uv = vUV;
    float speedWarp = uDash * 0.9 + uGrapple * 0.55 + uHurt * 0.5;

    // Radial chromatic warp: dash bursts, grapple tether, player damage
    vec2 d = uv - 0.5;
    float r2 = dot(d, d);
    if (speedWarp > 0.001) {
      float warp = (0.012 + r2 * 0.05) * speedWarp;
      uv = 0.5 + d * (1.0 - warp * r2 * 2.0);
    }

    // Barrel-style chromatic aberration on the edges while hurt / speed
    float bank = 0.0009 + uHurt * 0.0035 + speedWarp * 0.0012;
    float ca = (abs(d.x) * 0.7 + abs(d.y)) * bank;
    vec3 col;
    col.r = texture(uScene, uv + vec2(ca, 0.0)).r;
    col.g = texture(uScene, uv).g;
    col.b = texture(uScene, uv - vec2(ca, 0.0)).b;

    // Bloom (two mip levels, wide + tight)
    vec3 bloom = texture(uBloom, uv).rgb * 0.62 + texture(uBloom2, uv).rgb * 0.85;
    col += bloom * uBloomStr * (0.55 + uOverdrive * 0.85);

    // Exposure, overdrive tint, slow-mo cold grade
    col *= uExposure * (1.0 + uOverdrive * 0.16);
    col = mix(col, col * vec3(1.06, 0.96, 1.16), uSlowMo);
    col = mix(col, col * vec3(1.22, 0.86, 0.80) + vec3(0.05, 0.0, 0.0), clamp(uHurt * 0.85, 0.0, 1.0));

    // ACES Filmic tonemap + gamma
    vec3 mapped = pow(ACESFilm(col * 1.06), vec3(1.0 / 2.2));

    // Contrast-adaptive sharpening (CAS-lite, 5-tap)
    vec2 texel = 1.0 / uResolution;
    vec3 n = texture(uScene, vUV + vec2(0.0, texel.y)).rgb;
    vec3 s = texture(uScene, vUV - vec2(0.0, texel.y)).rgb;
    vec3 w = texture(uScene, vUV - vec2(texel.x, 0.0)).rgb;
    vec3 e = texture(uScene, vUV + vec2(texel.x, 0.0)).rgb;
    vec3 nb = pow(ACESFilm((n + s + w + e) * 0.25 * uExposure), vec3(1.0 / 2.2));
    vec3 sharp = mapped + (mapped - nb) * uSharp;
    mapped = mix(mapped, clamp(sharp, 0.0, 1.0), 0.85);

    // Saturation / vibrance
    float l = luma(mapped);
    mapped = mix(vec3(l), mapped, uSaturation);
    mapped = mix(mapped, mapped * mapped * (3.0 - 2.0 * mapped), 0.16);

    // Overdrive electric rim + slow-mo cyan vignette
    mapped += vec3(0.16, 0.42, 0.52) * uOverdrive * pow(r2 * 1.9, 2.2) * 0.5;

    // Vignette + damage pulse
    float vig = smoothstep(0.92, 0.24, sqrt(r2) * (1.18 + uVignette * 0.5));
    mapped *= mix(1.0, vig, 0.55 + uVignette * 0.32);
    float hurtEdge = smoothstep(0.34, 0.86, sqrt(r2));
    mapped = mix(mapped, vec3(0.62, 0.03, 0.05), hurtEdge * uHurt * 0.72);

    // Film grain (cheap hash) keeps the big gradients from banding
    float grain = fract(sin(dot(vUV * uResolution + uTime * 61.7, vec2(12.9898, 78.233))) * 43758.5453);
    mapped += (grain - 0.5) * 0.022;

    fragColor = vec4(clamp(mapped, 0.0, 1.0), 1.0);
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
    function fatal(msg) {
      console.error("[CHIKENGUN]", msg);
      try {
        let el = document.getElementById("glFatal");
        if (!el) {
          el = document.createElement("div");
          el.id = "glFatal";
          el.style.cssText = "position:fixed;left:50%;top:50%;transform:translate(-50%,-50%);z-index:9999;" +
            "max-width:min(720px,92vw);padding:18px 22px;border-radius:10px;background:rgba(20,4,8,0.96);" +
            "border:2px solid #ff3b4e;color:#ffd7db;font:13px/1.55 monospace;white-space:pre-wrap;" +
            "box-shadow:0 20px 60px rgba(0,0,0,0.8);pointer-events:auto";
          document.body.appendChild(el);
        }
        el.textContent = "⚠ ОШИБКА РЕНДЕРА\n\n" + msg +
          "\n\nНажмите [Ctrl+F5] для полной перезагрузки.\nЕсли повторяется — нажмите [F4] и выберите ТУРБО FPS.";
      } catch (_) {}
    }
    window.__cgFatal = fatal;
    const gl = canvas.getContext("webgl2", {
      antialias: false, alpha: false, depth: true, stencil: false,
      powerPreference: "high-performance", preserveDrawingBuffer: false, failIfMajorPerformanceCaveat: false
    });
    if (!gl) {
      fatal("WebGL2 недоступен в этом браузере.\nВключите аппаратное ускорение (chrome://gpu) и перезагрузите страницу.");
      return null;
    }

    const extAniso = gl.getExtension("EXT_texture_filter_anisotropic");
    const maxAniso = extAniso ? Math.min(16, gl.getParameter(extAniso.MAX_TEXTURE_MAX_ANISOTROPY_EXT)) : 1;

    function compileProg(vsSrc, fsSrc) {
      const sh = (type, src) => {
        const s = gl.createShader(type);
        gl.shaderSource(s, src); gl.compileShader(s);
        if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
          const log = gl.getShaderInfoLog(s);
          console.error(log);
          console.error("[CHIKENGUN] shader source:\n" + src.split("\n").map((l, i) => (i+1) + ": " + l).join("\n"));
          window.__cgFatal && window.__cgFatal("Ошибка компиляции шейдера:\n" + log);
        }
        return s;
      };
      const p = gl.createProgram();
      gl.attachShader(p, sh(gl.VERTEX_SHADER, vsSrc));
      gl.attachShader(p, sh(gl.FRAGMENT_SHADER, fsSrc));
      gl.linkProgram(p);
      if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
        const log = gl.getProgramInfoLog(p);
        console.error(log);
        window.__cgFatal && window.__cgFatal("Ошибка линковки шейдерной программы:\n" + log);
      }
      return p;
    }

    const pbrProg    = compileProg(VS_PBR, FS_PBR);
    const skyProg    = compileProg(VS_SKY, FS_SKY);
    const shadowProg = compileProg(VS_SHADOW, FS_SHADOW);
    const fxProg     = compileProg(VS_FX, FS_FX);
    const brightProg = compileProg(VS_FULL, FS_BRIGHT);
    const blurProg   = compileProg(VS_FULL, FS_BLUR);
    const postProg   = compileProg(VS_FULL, FS_POST);

    const extCBF = gl.getExtension("EXT_color_buffer_float");
    const extLinFloat = gl.getExtension("OES_texture_float_linear");
    let HDR_OK_forced_off = false;
    let HDR_OK = !!(extCBF && extLinFloat);

    const U = {};
    [
      "uVP","uModel","uUVScale","uAlbedoMap","uNormalMap","uRoughMetalMap","uEnvCube",
      "uTint","uEmissive","uPbrOverride","uUseTex","uCamPos","uSunDir","uSunColor",
      "uSkyAmb","uFogColor","uFogDensity",
      "uPointPos[0]","uPointPos[1]","uPointPos[2]","uPointPos[3]",
      "uPointCol[0]","uPointCol[1]","uPointCol[2]","uPointCol[3]",
      "uShadowMap","uLightVP","uShadowTexel","uShadowOn","uTonemap"
    ].forEach(n => U[n] = gl.getUniformLocation(pbrProg, n));

    const USky = {
      vp: gl.getUniformLocation(skyProg, "uVPNoTrans"),
      cube: gl.getUniformLocation(skyProg, "uEnvCube"),
      tint: gl.getUniformLocation(skyProg, "uSkyTint"),
      fog: gl.getUniformLocation(skyProg, "uFogColor"),
      sunDir: gl.getUniformLocation(skyProg, "uSunDirSky"),
      sunCol: gl.getUniformLocation(skyProg, "uSunColSky")
    };

    const USh = {
      vp: gl.getUniformLocation(shadowProg, "uLightVP"),
      model: gl.getUniformLocation(shadowProg, "uModel")
    };

    const UFX = {
      vp: gl.getUniformLocation(fxProg, "uVP"),
      model: gl.getUniformLocation(fxProg, "uModel"),
      color: gl.getUniformLocation(fxProg, "uColor"),
      alpha: gl.getUniformLocation(fxProg, "uAlpha"),
      mode: gl.getUniformLocation(fxProg, "uMode"),
      additive: gl.getUniformLocation(fxProg, "uAdditive"),
      camPos: gl.getUniformLocation(fxProg, "uCamPos"),
      fog: gl.getUniformLocation(fxProg, "uFogColor"),
      fogD: gl.getUniformLocation(fxProg, "uFogDensity"),
      flicker: gl.getUniformLocation(fxProg, "uFlicker")
    };

    const UBr = {
      tex: gl.getUniformLocation(brightProg, "uTex"),
      texel: gl.getUniformLocation(brightProg, "uTexel"),
      thr: gl.getUniformLocation(brightProg, "uThreshold")
    };
    const UBl = {
      tex: gl.getUniformLocation(blurProg, "uTex"),
      dir: gl.getUniformLocation(blurProg, "uDir")
    };
    const UP = {};
    ["uScene","uBloom","uBloom2","uBloomStr","uResolution","uTime","uHurt","uOverdrive",
     "uSlowMo","uVignette","uSaturation","uExposure","uSharp","uDash","uGrapple"]
      .forEach(n => UP[n] = gl.getUniformLocation(postProg, n));

    const meshes = {
      box:  buildBoxMesh(gl),
      cyl:  buildCylinderMesh(gl, 18, 0.5, 0.5),
      cone: buildCylinderMesh(gl, 14, 0.04, 0.5),
      sph:  buildSphereMesh(gl, 12, 16),
      // camera-independent unit quad in XY (-0.5..0.5) with 0..1 UV — used for FX billboards,
      // decals and the fullscreen post pass
      quad: (() => {
        const P=[],N=[],T=[],Uv=[],I=[];
        [[-0.5,-0.5,0,0],[0.5,-0.5,1,0],[0.5,0.5,1,1],[-0.5,0.5,0,1]].forEach(([x,y,u,v]) => {
          P.push(x, y, 0); N.push(0,0,1); T.push(1,0,0); Uv.push(u, v);
        });
        I.push(0,1,2,0,2,3);
        return createMesh(gl, P, N, T, Uv, I);
      })()
    };

    // ---------- 4B. Render Targets: HDR scene (+ MSAA), bloom chain, sun shadow map ----------
    let GFX_PRESETS = [
      { name: "УЛЬТРА",     shadow: 2048, samples: 4, bloom: true,  scale: 1.00, sharp: 0.42, sat: 1.10 },
      { name: "ВЫСОКИЕ",    shadow: 1024, samples: 2, bloom: true,  scale: 0.94, sharp: 0.34, sat: 1.07 },
      { name: "ТУРБО FPS",  shadow: 0,    samples: 0, bloom: false, scale: 0.78, sharp: 0.26, sat: 1.04 }
    ];
    let gfxIdx = 1; // ВЫСОКИЕ is the safe default; [F4] cycles to УЛЬТРА / ТУРБО FPS
    try {
      const g = parseInt(localStorage.getItem("cg23_gfx") || "1", 10);
      if (g >= 0 && g < GFX_PRESETS.length) gfxIdx = g;
    } catch (_) {}
    // Adaptive render scale — keeps a stable frame time on weaker laptop GPUs
    let dynScale = 1.0, dynAcc = 0, dynFrames = 0;

    const RT = {
      w: 1, h: 1, bw: 1, bh: 1,
      scene: null, sceneFBO: null, depthRB: null,
      msFBO: null, msColor: null, msDepth: null,
      brightTex: null, brightFBO: null,
      blurA: null, blurAFBO: null, blurB: null, blurBFBO: null,
      shadowTex: null, shadowFBO: null, shadowSize: 0
    };

    function makeTex(w, h, hdr, filter) {
      const useHdr = hdr && HDR_OK && !HDR_OK_forced_off;
      const t = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, t);
      const internal = useHdr ? gl.RGBA16F : gl.RGBA8;
      const type = useHdr ? gl.HALF_FLOAT : gl.UNSIGNED_BYTE;
      gl.texImage2D(gl.TEXTURE_2D, 0, internal, w, h, 0, gl.RGBA, type, null);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      return t;
    }

    function destroyRT() {
      ["scene","brightTex","blurA","blurB"].forEach(k => { if (RT[k]) { gl.deleteTexture(RT[k]); RT[k] = null; } });
      ["sceneFBO","brightFBO","blurAFBO","blurBFBO","msFBO"].forEach(k => { if (RT[k]) { gl.deleteFramebuffer(RT[k]); RT[k] = null; } });
      ["depthRB","msColor","msDepth"].forEach(k => { if (RT[k]) { gl.deleteRenderbuffer(RT[k]); RT[k] = null; } });
    }

    let RT_OK = true;

    function buildRT(w, h, retried) {
      destroyRT();
      // Uniform pixel-budget cap: an RGBA16F + MSAA target at Retina sizes can
      // exhaust VRAM on integrated laptop GPUs (black screen). Scaling both axes
      // keeps the aspect ratio intact so the post pass never stretches.
      const maxTex = gl.getParameter(gl.MAX_TEXTURE_SIZE) || 4096;
      const budget = gfxIdx === 2 ? 1_500_000 : 2_800_000;
      const f = Math.min(1, Math.sqrt(budget / Math.max(1, w * h)));
      RT.w = Math.max(2, Math.min(maxTex, Math.floor(w * f)));
      RT.h = Math.max(2, Math.min(maxTex, Math.floor(h * f)));
      RT_OK = true;
      const preset = GFX_PRESETS[gfxIdx];

      try {
      RT.scene = makeTex(RT.w, RT.h, HDR_OK && !HDR_OK_forced_off, gl.LINEAR);
      RT.sceneFBO = gl.createFramebuffer();
      gl.bindFramebuffer(gl.FRAMEBUFFER, RT.sceneFBO);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, RT.scene, 0);
      RT.depthRB = gl.createRenderbuffer();
      gl.bindRenderbuffer(gl.RENDERBUFFER, RT.depthRB);
      gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT24, RT.w, RT.h);
      gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, RT.depthRB);
      const okScene = gl.checkFramebufferStatus(gl.FRAMEBUFFER) === gl.FRAMEBUFFER_COMPLETE;
      if (!okScene) {
        // HDR (or the depth attachment) unsupported — rebuild as plain RGBA8 exactly once
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        destroyRT();
        if (!retried && (HDR_OK || (GFX_PRESETS[gfxIdx].samples > 0))) {
          HDR_OK_forced_off = true;
          GFX_PRESETS[gfxIdx] = Object.assign({}, GFX_PRESETS[gfxIdx], { samples: 0 });
          buildRT(w, h, true);
          return;
        }
        RT_OK = false;
        RT.failed = true;
      }

      // Optional multisampled scene target, resolved with blitFramebuffer each frame
      if (preset.samples > 0 && okScene) try {
        RT.msFBO = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, RT.msFBO);
        RT.msColor = gl.createRenderbuffer();
        gl.bindRenderbuffer(gl.RENDERBUFFER, RT.msColor);
        gl.renderbufferStorageMultisample(gl.RENDERBUFFER, preset.samples,
          (HDR_OK && !HDR_OK_forced_off) ? gl.RGBA16F : gl.RGBA8, RT.w, RT.h);
        gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.RENDERBUFFER, RT.msColor);
        RT.msDepth = gl.createRenderbuffer();
        gl.bindRenderbuffer(gl.RENDERBUFFER, RT.msDepth);
        gl.renderbufferStorageMultisample(gl.RENDERBUFFER, preset.samples, gl.DEPTH_COMPONENT24, RT.w, RT.h);
        gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, RT.msDepth);
        if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) !== gl.FRAMEBUFFER_COMPLETE) {
          gl.deleteFramebuffer(RT.msFBO); RT.msFBO = null;
          gl.deleteRenderbuffer(RT.msColor); RT.msColor = null;
          gl.deleteRenderbuffer(RT.msDepth); RT.msDepth = null;
        }
      } catch (e) {
        // Some drivers throw here — just drop MSAA and keep rendering
        console.warn("[CHIKENGUN] MSAA unavailable:", e && e.message);
        try { if (RT.msFBO) gl.deleteFramebuffer(RT.msFBO); } catch (_) {}
        RT.msFBO = RT.msColor = RT.msDepth = null;
      }

      // Bloom chain at 1/2 and 1/4 resolution
      const bw = Math.max(2, Math.floor(RT.w / 2)), bh = Math.max(2, Math.floor(RT.h / 2));
      const qw = Math.max(2, Math.floor(RT.w / 4)), qh = Math.max(2, Math.floor(RT.h / 4));
      RT.bw = bw; RT.bh = bh;
      const mkFBO = (tex) => {
        const f = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, f);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
        return f;
      };
      RT.brightTex = makeTex(bw, bh, HDR_OK, gl.LINEAR);
      RT.brightFBO = mkFBO(RT.brightTex);
      RT.blurA = makeTex(qw, qh, HDR_OK, gl.LINEAR);
      RT.blurAFBO = mkFBO(RT.blurA);
      RT.blurB = makeTex(qw, qh, HDR_OK, gl.LINEAR);
      RT.blurBFBO = mkFBO(RT.blurB);

      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.bindRenderbuffer(gl.RENDERBUFFER, null);
      } catch (e) {
        console.warn("[CHIKENGUN] render target setup failed, using compatibility path:", e && e.message);
        destroyRT();
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        RT_OK = false;
        RT.failed = true;
      }
    }

    function buildShadowMap(size) {
      if (RT.shadowFBO) { gl.deleteFramebuffer(RT.shadowFBO); RT.shadowFBO = null; }
      if (RT.shadowTex) { gl.deleteTexture(RT.shadowTex); RT.shadowTex = null; }
      RT.shadowSize = size;
      if (!size) return;
      RT.shadowTex = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, RT.shadowTex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.DEPTH_COMPONENT24, size, size, 0,
                    gl.DEPTH_COMPONENT, gl.UNSIGNED_INT, null);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      RT.shadowFBO = gl.createFramebuffer();
      gl.bindFramebuffer(gl.FRAMEBUFFER, RT.shadowFBO);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, RT.shadowTex, 0);
      gl.drawBuffers([gl.NONE]); gl.readBuffer(gl.NONE);
      const ok = gl.checkFramebufferStatus(gl.FRAMEBUFFER) === gl.FRAMEBUFFER_COMPLETE;
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      if (!ok) {
        // Graceful fallback: no sun shadows on this GPU/driver
        gl.deleteFramebuffer(RT.shadowFBO); RT.shadowFBO = null;
        gl.deleteTexture(RT.shadowTex); RT.shadowTex = null;
        RT.shadowSize = 0;
      }
    }

    let applyGfxHooked = false;
    function applyGfx(idx) {
      gfxIdx = Math.max(0, Math.min(GFX_PRESETS.length - 1, idx));
      try { localStorage.setItem("cg23_gfx", String(gfxIdx)); } catch (_) {}
      buildShadowMap(GFX_PRESETS[gfxIdx].shadow);
      // Changing MSAA/bloom needs fresh render targets; the render loop rebuilds
      // them lazily from RT.resetNext instead of allocating here.
      RT.resetNext = true;
    }

    buildShadowMap(GFX_PRESETS[gfxIdx].shadow);
    buildRT(1, 1);
    // Light-space matrices for the sun shadow pass
    const lightView = M4.ident(), lightProj = M4.ident(), lightVP = M4.ident(), lightVPNoShadow = M4.ident();
    const SHADOW_RADIUS = 62;
    function updateLightVP(sunDir) {
      const sl = Math.hypot(sunDir[0], sunDir[1], sunDir[2]) || 1;
      const dx = sunDir[0] / sl, dy = sunDir[1] / sl, dz = sunDir[2] / sl;
      M4.lookAt(lightView, dx * 90, dy * 90, dz * 90, 0, 0, 0, 0, 1, 0);
      M4.ortho(lightProj, -SHADOW_RADIUS, SHADOW_RADIUS, -SHADOW_RADIUS, SHADOW_RADIUS, 1, 260);
      M4.mul(lightVP, lightProj, lightView);
      return lightVP;
    }

    // Load 2D PBR Texture Trios + Sky4K Cubemap
    // Offline / single-file builds (tools/build_web.py) embed every texture as a
    // data: URL in window.__CG_ASSETS. Over HTTP this map is absent and the plain
    // relative paths are used, so both modes share one code path.
    function assetUrl(url) {
      const map = (typeof window !== "undefined") && window.__CG_ASSETS;
      return (map && map[url]) ? map[url] : url;
    }

    function makeFallbackTex(r, g, b) {
      const t = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, t);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([r,g,b,255]));
      return t;
    }
    const whiteTex = makeFallbackTex(255, 255, 255);
    const flatNorm = makeFallbackTex(128, 128, 255);
    const defRM    = makeFallbackTex(255, 140, 20);

    function loadTex2D(url, fallback) {
      const tex = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                    fallback === flatNorm ? new Uint8Array([128,128,255,255]) : new Uint8Array([200,200,200,255]));
      const img = new Image();
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
      img.src = assetUrl(url);
      return tex;
    }

    function loadMaterialSet(prefix) {
      return {
        diff: loadTex2D(`Data/Textures/${prefix}Diffuse.png`, whiteTex),
        norm: loadTex2D(`Data/Textures/${prefix}Normal.png`, flatNorm),
        rm:   loadTex2D(`Data/Textures/${prefix}RoughMetal.png`, defRM)
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
      im.src = assetUrl(url);
    });
    gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MIN_FILTER, gl.LINEAR);

    // ---------- 5. Static 3D Arena Architecture & AABB Colliders ----------
    const staticProps = []; // { mesh, mat, x,y,z, ry, sx,sy,sz, uvs:[u,v], tint, emissive, rough, metal }
    const colliders   = []; // { cx, cy, cz, hx, hy, hz }

    function addProp(mesh, mat, x, y, z, sx, sy, sz, uvs = [1,1], collide = true, tint = [1,1,1,1], emissive = [0,0,0], rough = 1.0, metal = -1) {
      const m = M4.ident();
      M4.trs(m, x, y, z, 0, 0, 0, sx, sy, sz);
      // bounding sphere for frustum culling
      const br = 0.55 * Math.hypot(sx, sy, sz);
      staticProps.push({ mesh, mat, model: m, uvs, tint, emissive, rough, metal, cx: x, cy: y, cz: z, br });
      if (collide) colliders.push({ cx: x, cy: y, cz: z, hx: sx*0.5, hy: sy*0.5, hz: sz*0.5 });
    }

    // Frustum plane extraction from a column-major view-projection matrix
    const frustum = new Float32Array(24);
    function extractFrustum(m) {
      const r0x=m[0], r0y=m[4], r0z=m[8], r0w=m[12];
      const r1x=m[1], r1y=m[5], r1z=m[9], r1w=m[13];
      const r2x=m[2], r2y=m[6], r2z=m[10], r2w=m[14];
      const r3x=m[3], r3y=m[7], r3z=m[11], r3w=m[15];
      const set = (i, ax, ay, az, aw) => {
        const l = Math.hypot(ax, ay, az) || 1;
        frustum[i*4] = ax/l; frustum[i*4+1] = ay/l; frustum[i*4+2] = az/l; frustum[i*4+3] = aw/l;
      };
      set(0, r3x+r0x, r3y+r0y, r3z+r0z, r3w+r0w); // left
      set(1, r3x-r0x, r3y-r0y, r3z-r0z, r3w-r0w); // right
      set(2, r3x+r1x, r3y+r1y, r3z+r1z, r3w+r1w); // bottom
      set(3, r3x-r1x, r3y-r1y, r3z-r1z, r3w-r1w); // top
      set(4, r3x+r2x, r3y+r2y, r3z+r2z, r3w+r2w); // near
      set(5, r3x-r2x, r3y-r2y, r3z-r2z, r3w-r2w); // far
    }
    function sphereVisible(x, y, z, r) {
      for (let i = 0; i < 24; i += 4) {
        if (frustum[i]*x + frustum[i+1]*y + frustum[i+2]*z + frustum[i+3] < -r) return false;
      }
      return true;
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

    // 4 Stadium Floodlight Towers
    [[-38,-38],[38,-38],[-38,38],[38,38]].forEach(([tx, tz]) => {
      addProp("cyl", MATS.metal, tx, 9, tz, 1.2, 18, 1.2, [1, 6]);
      addProp("box", null, tx, 18.2, tz, 4.2, 1.1, 1.4, [1,1], false, [1,0.95,0.85,1], [3.6, 3.3, 2.8], 0.15, 0.9);
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

    // ---- Floor medallion: glowing rings, radial rune spokes, hazard inner disc ----
    addProp("cyl", null, 0, 0.03, 0, 30, 0.06, 30, [1,1], false, [0.10,0.24,0.30,1], [0.05,0.16,0.22], 0.35, 0.7);
    addProp("cyl", MATS.hazard, 0, 0.08, 0, 26, 0.05, 26, [8,8], false, [0.85,0.88,0.92,1], [0.04,0.05,0.06], 0.46, 0.72);
    for (let i = 0; i < 3; i++) {
      const rr = 8.6 + i * 6.4;
      addProp("cyl", null, 0, 0.13, 0, rr*2, 0.035, rr*2, [1,1], false,
        i % 2 ? [1,0.82,0.28,1] : [0.22,0.95,1,1],
        i % 2 ? [1.5,1.1,0.18] : [0.16,1.5,1.8], 0.18, 0.86);
    }
    for (let i = 0; i < 12; i++) {
      const a = (i / 12) * Math.PI * 2;
      addProp("box", null, Math.cos(a)*13.2, 0.10, Math.sin(a)*13.2, 0.7, 0.05, 4.6, [1,1], false,
        i % 3 ? [0.16,0.9,1,1] : [1,0.85,0.3,1],
        i % 3 ? [0.14,1.3,1.7] : [1.6,1.2,0.22], 0.2, 0.8);
    }
    // Central neon dais
    addProp("cyl", MATS.metal, 0, 0.16, 0, 5.4, 0.32, 5.4, [2,2], false, [0.30,0.36,0.44,1], [0,0,0], 0.28, 0.92);
    addProp("cyl", null, 0, 0.34, 0, 4.3, 0.10, 4.3, [1,1], false, [0.3,1,1,1], [0.2,1.6,2.0], 0.15, 0.9);

    // ---- Flank catwalk rings + connecting ramps (vertical gameplay & sightlines) ----
    [[1, 0], [-1, 0], [0, 1], [0, -1]].forEach(([ux, uz], idx) => {
      const baseX = ux * 33, baseZ = uz * 33;
      addProp("box", MATS.metal, baseX, 4.2, baseZ, ux !== 0 ? 22 : 3.2, 0.4, uz !== 0 ? 22 : 3.2, [6,1]);
      addProp("box", null, baseX, 4.44, baseZ, ux !== 0 ? 22.1 : 3.3, 0.12, uz !== 0 ? 22.1 : 3.3, [1,1], false,
        [0.2,0.95,1,1], idx % 2 ? [0.9,0.25,1.4] : [0.2,1.2,1.5], 0.2, 0.88);
      // balustrade posts
      for (let i = -3; i <= 3; i++) {
        const px = ux !== 0 ? baseX + i * 3 : baseX + 1.5;
        const pz = uz !== 0 ? baseZ + i * 3 : baseZ + 1.5;
        addProp("box", null, px, 4.9, pz, 0.12, 1.0, 0.12, [1,1], false, [0.7,0.85,1,1], [0.5,1.1,1.4], 0.2, 0.8);
      }
      // ramp down toward the middle
      const rampx = ux * 22.4, rampz = uz * 22.4;
      addProp("box", MATS.hazard, rampx + ux * 3.0, 2.1, rampz + uz * 3.0,
        ux !== 0 ? 6.2 : 3.0, 0.35, uz !== 0 ? 6.2 : 3.0, [3,1]);
    });

    // ---- Outer cyber skyline: towers, antenna masts, blinking beacons ----
    const skyline = [
      [-72, -58, 26, 6], [-56, -72, 19, 4.5], [-84, -36, 34, 7], [72, -62, 22, 5],
      [58, -80, 30, 6.5], [86, -34, 16, 4], [-78, 56, 24, 5.5], [-94, 30, 38, 8],
      [64, 74, 20, 5], [88, 48, 28, 6], [-40, -92, 44, 9], [40, -96, 36, 8],
      [-96, -70, 30, 7], [96, -76, 26, 6], [8, 96, 32, 7], [-8, -104, 40, 9]
    ];
    skyline.forEach(([sx, sz, sh, sw], i) => {
      addProp("box", MATS.stone, sx, sh * 0.5, sz, sw, sh, sw, [Math.max(2, sh/6), Math.max(2, sh/6)], false,
        i % 3 === 0 ? [0.24,0.30,0.42,1] : [0.18,0.22,0.32,1], [0,0,0], 0.55, 0.55);
      // window bands
      const bands = Math.max(2, Math.floor(sh / 7));
      for (let b = 0; b < bands; b++) {
        addProp("box", null, sx, 3.4 + b * (sh / bands), sz, sw * 1.012, 0.42, sw * 1.012, [1,1], false,
          [0.7,0.85,1,1], i % 2 ? [0.18,0.62,1.0] : [1.0,0.72,0.28], 0.12, 0.7);
      }
      // antenna mast + beacon
      addProp("cyl", MATS.metal, sx, sh + 5, sz, 0.42, 10, 0.42, [1,3], false);
      addProp("box", null, sx, sh + 10.2, sz, 0.5, 0.5, 0.5, [1,1], false,
        [1,0.25,0.3,1], [2.6,0.2,0.25], 0.2, 0.4);
    });

    // ---- Side barrier bunkers / HESCO-like cover blocks for close-quarters fights ----
    [[-34, 12, 0.0], [-34, -12, 0.0], [34, 12, Math.PI], [34, -12, Math.PI],
     [12, 34, Math.PI*0.5], [-12, 34, Math.PI*0.5], [12, -34, -Math.PI*0.5], [-12, -34, -Math.PI*0.5]].forEach(([bx, bz, ry]) => {
      addProp("box", MATS.hazard, bx, 1.3, bz, 4.2, 2.6, 1.2, [3,1]);
      addProp("box", null, bx, 2.66, bz, 4.3, 0.12, 1.3, [1,1], false, [1,0.6,0.2,1], [1.1,0.45,0.1], 0.25, 0.7);
    });

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

    // Pre-warm wave-1 spawn coordinates so startAudioFor() can build them before the loop
    const initBonusHp  = savedBuff >= 2 ? 50 : (savedBuff === 1 ? 25 : 0);
    const initBonusArm = savedBuff >= 2 ? 50 : (savedBuff === 1 ? 25 : 0);

    const S = {
      x: 0, y: 1.75, z: -26, vx: 0, vy: 0, vz: 0, yaw: 0, pitch: 0.04,
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
      barrels: [], pads: [], chickens: [], orbs: [], enemyEggs: [], grenades: [], turrets: [], beams: [], particles: [], pickups: [],
      decals: [], waves: [], shells: [], feathers: [], corpses: [],
      muzzleFlash: 0, hurtFx: 0, hitstop: 0, adsBlend: 0, sprintBlend: 0, gfx: 0,
      introT: 1.15, wasPlaying: false,
      shellCd: 0, featherCd: 0,
      weather: Array.from({ length: 26 }, () => ({ ox: (Math.random()-0.5)*34, oy: Math.random()*13, oz: (Math.random()-0.5)*34, sp: 2+Math.random()*3, ph: Math.random()*6.28 })),
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
        const a = (i / cnt) * Math.PI * 2 + Math.random() * 0.4;
        const r = 20 + Math.random() * 18;
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
          type, hp, maxHp: hp, phase: Math.random() * 6.28, atkCd: 1.0 + Math.random(), enraged: false, beepT: 0.4
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
      S.hurtFx = Math.max(S.hurtFx || 0, Math.hypot(S.x - ex, S.z - ez) < radius * 1.4 ? 0.55 : 0.22);

      // --- Layered explosion: fireball core, shockwave rings, sparks, embers, smoke, scorch decal ---
      S.waves.push({ x: ex, y: 0.05, z: ez, r0: 0.6, r1: radius * 1.5, ttl: 0.55, maxTtl: 0.55, col: [3.0, 1.5, 0.4] });
      S.waves.push({ x: ex, y: 0.08, z: ez, r0: 0.2, r1: radius * 0.95, ttl: 0.9, maxTtl: 0.9, col: [1.2, 2.4, 2.8] });
      S.decals.push({ x: ex, y: 0.05, z: ez, r: radius * 0.85, ttl: 22, maxTtl: 22, col: [0.06, 0.05, 0.05], alpha: 0.82 });
      if (S.decals.length > 26) S.decals.shift();

      for (let k = 0; k < 34; k++) {
        const a = Math.random() * 6.28, sp = 4 + Math.random() * 15;
        const up = 2 + Math.random() * 11;
        S.particles.push({
          x: ex, y: ey + 0.6, z: ez,
          vx: Math.cos(a) * sp, vy: up, vz: Math.sin(a) * sp,
          col: k % 3 === 0 ? [3.2, 2.6, 1.1] : k % 3 === 1 ? [3.0, 1.05, 0.22] : [2.2, 0.55, 0.10],
          sz: 0.10 + Math.random() * 0.20, ttl: 0.55 + Math.random() * 0.75, mode: 0
        });
      }
      // Slow rising smoke puffs (alpha-blended billboards)
      for (let k = 0; k < 12; k++) {
        const a = Math.random() * 6.28, sp = 1 + Math.random() * 3.4;
        S.particles.push({
          x: ex + Math.cos(a) * 0.4, y: ey + 0.9 + Math.random() * 1.5, z: ez + Math.sin(a) * 0.4,
          vx: Math.cos(a) * sp * 0.45, vy: 1.4 + Math.random() * 1.8, vz: Math.sin(a) * sp * 0.45,
          col: [0.30, 0.28, 0.30], sz: 0.55 + Math.random() * 0.65, ttl: 1.6 + Math.random(), mode: 1, alpha: 0.55
        });
      }
      // Feathers blasted off nearby birds
      S.chickens.forEach(c => {
        if (c.hp <= 0) return;
        if (Math.hypot(c.x - ex, c.z - ez) < radius * 1.2) spawnFeatherBurst(c, 5);
      });
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

    // Loose 3D feathers that flutter down with drag + tumble
    function spawnFeatherBurst(c, n) {
      const st = BREED_STYLE[c.type] || BREED_STYLE.melee;
      for (let i = 0; i < n; i++) {
        const a = Math.random() * 6.28, sp = 1.4 + Math.random() * 4.2;
        S.feathers.push({
          x: c.x + (Math.random()-0.5)*0.5, y: c.y + 0.5 + Math.random()*0.9, z: c.z + (Math.random()-0.5)*0.5,
          vx: Math.cos(a)*sp, vy: 1.2 + Math.random()*3.0, vz: Math.sin(a)*sp,
          rx: Math.random()*6.28, ry: Math.random()*6.28, rz: Math.random()*6.28,
          wx: (Math.random()-0.5)*9, wy: (Math.random()-0.5)*9, wz: (Math.random()-0.5)*9,
          sz: 0.10 + Math.random()*0.10, ttl: 2.4 + Math.random()*1.8,
          col: st.tint
        });
      }
      if (S.feathers.length > 220) S.feathers.splice(0, S.feathers.length - 220);
    }

    function damageChicken(c, dmg, headshot) {
      if (c.hp <= 0) return;
      c.hp -= dmg;
      c.hitFlash = 1.0;
      c.flinch = 1.0;
      const ddx = c.x - S.x, ddz = c.z - S.z, dl = Math.hypot(ddx, ddz) || 1;
      c.kbx = ddx / dl; c.kbz = ddz / dl;
      S.hitstop = Math.min(0.05, (S.hitstop || 0) + (c.hp <= 0 ? 0.045 : 0.012));
      if (c.hp <= 0) S.hurtFx = Math.max(S.hurtFx || 0, 0.14);
      opts.flashHit(headshot, c.hp <= 0);
      opts.spawnDmgNum(c.x, c.y + 1.8, c.z, Math.round(dmg), headshot);
      if (headshot) {
        S.headshots++;
        opts.playSfx("Headshot.wav", 0.72);
        if (S.headshots >= 10) unlockAch(1, "КУРИНЫЙ СНАЙПЕР (10 ХЭДШОТОВ)");
      } else {
        opts.playSfx("ChickenHurt.wav", 0.45);
      }

      const fCol = c.type === "kamikaze" ? [1.8,0.4,0.1] : c.type === "drone" ? [0.2,1.6,2.2] : [1.2,1.2,1.15];
      for (let i = 0; i < (c.hp <= 0 ? 14 : 3); i++) {
        const a = Math.random() * 6.28;
        S.particles.push({
          x: c.x, y: c.y + 1.0, z: c.z,
          vx: Math.cos(a) * 4.5, vy: 2.5 + Math.random() * 5, vz: Math.sin(a) * 4.5,
          col: fCol, sz: 0.16, ttl: 0.95
        });
      }

      if (c.hp <= 0) {
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
        if (c.type === "boss") { S.slowMo = 0.42; unlockAch(6, "УБИЙЦА МЕГА-ИНДЮКА"); }

        // Knockback death ragdoll: the rig tumbles away instead of blinking out
        const kforce = c.type === "boss" ? 0.7 : 1.0;
        S.corpses.push({
          x: c.x, y: c.y, z: c.z, yaw: Math.atan2(S.x - c.x, S.z - c.z),
          vx: (c.kbx || 0) * (5.5 * kforce) + (Math.random()-0.5)*2.0,
          vy: 3.4 * kforce + Math.random()*2.2,
          vz: (c.kbz || 0) * (5.5 * kforce) + (Math.random()-0.5)*2.0,
          tilt: 0, tiltV: (Math.random()-0.5) * 6.5,
          spin: 0, spinV: (Math.random()-0.5) * 7.5,
          type: c.type, sc: c.type === "boss" ? 1.85 : c.type === "kamikaze" ? 0.82 : 1.0,
          ttl: c.type === "boss" ? 3.4 : 2.3, maxTtl: c.type === "boss" ? 3.4 : 2.3
        });
        if (S.corpses.length > 18) S.corpses.shift();
        spawnFeatherBurst(c, c.type === "boss" ? 26 : c.type === "drone" ? 8 : 15);
        S.waves.push({ x: c.x, y: c.y + 0.05, z: c.z, r0: 0.3, r1: c.type === "boss" ? 8 : 3.2, ttl: 0.5, maxTtl: 0.5, col: [2.4, 1.4, 0.5] });
        if (c.type === "boss") {
          for (let k = 0; k < 3; k++) {
            setTimeout(() => {
              explodeAt(c.x + (Math.random()-0.5)*4, 0.7, c.z + (Math.random()-0.5)*4, 7, 60);
            }, 130 + k * 160);
          }
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
      S.muzzleFlash = 1.0;
      opts.playSfx(w.sfx, 0.75);

      // 3D ejected shell casing (brass for kinetic, glowing energy cell for plasma/rail)
      if (S.shellCd <= 0) {
        S.shellCd = w.pierce || w.plasma ? 0.12 : 0.045;
        const rightX = Math.cos(S.yaw), rightZ = -Math.sin(S.yaw);
        const energy = !!(w.pierce || w.plasma);
        S.shells.push({
          x: S.x + rightX*0.32 + Math.sin(S.yaw)*0.35, y: S.y - 0.16, z: S.z + rightZ*0.32 + Math.cos(S.yaw)*0.35,
          vx: rightX * (2.4 + Math.random()) + (Math.random()-0.5)*0.8,
          vy: 2.0 + Math.random()*1.1,
          vz: rightZ * (2.4 + Math.random()) + (Math.random()-0.5)*0.8,
          rx: Math.random()*6.28, ry: Math.random()*6.28, rz: Math.random()*6.28,
          wx: (Math.random()-0.5)*22, wy: (Math.random()-0.5)*22, wz: (Math.random()-0.5)*22,
          ttl: 1.9 + Math.random()*0.7, energy
        });
        if (S.shells.length > 90) S.shells.shift();
      }

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
      if (e.code === "F4") {
        e.preventDefault();
        applyGfx((gfxIdx + 1) % GFX_PRESETS.length);
        const pr = GFX_PRESETS[gfxIdx];
        opts.pushFeed?.(`🎛 ГРАФИКА [F4]: ${pr.name} — тени ${pr.shadow ? pr.shadow + "²" : "ВЫКЛ"}, MSAA x${pr.samples}, Bloom ${pr.bloom ? "ON" : "OFF"}`);
        opts.showToast?.(`🎛 Пресет графики: ${pr.name}`);
        return;
      }
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

    // ---------- 9B. Hierarchical Transform Stack (real 3D rigs, not flat groups) ----------
    const _stack = [M4.ident()];
    let _top = M4.ident(), _mid = M4.ident(), _loc = M4.ident();
    function pushT(x, y, z, ry, rx, rz, sx, sy, sz) {
      M4.trs(_loc, x, y, z, ry || 0, rx || 0, rz || 0, sx, sy, sz);
      const par = _stack[_stack.length - 1];
      M4.mul(_top, par, _loc);
      _stack.push(new Float32Array(_top));
      return _top;
    }
    function popT() { if (_stack.length > 1) _stack.pop(); }
    const curT = () => _stack[_stack.length - 1];
    function resetT() { _stack.length = 1; _stack[0] = M4.ident(); }

    // ---------- 9C. FX Helpers (billboards, decals, scorch marks, telegraphs) ----------
    const camRight = [1, 0, 0], camUp = [0, 1, 0], camFwd = [0, 0, -1];
    let fxCamX = 0, fxCamY = 0, fxCamZ = 0;
    function setFxCam(x, y, z, yaw, pitch) {
      fxCamX = x; fxCamY = y; fxCamZ = z;
      const cy = Math.cos(yaw), sy = Math.sin(yaw), cp = Math.cos(pitch), sp = Math.sin(pitch);
      camRight[0] = cy; camRight[1] = 0; camRight[2] = -sy;
      camUp[0] = -sy * sp; camUp[1] = cp; camUp[2] = -cy * sp;
      camFwd[0] = sy * cp; camFwd[1] = sp; camFwd[2] = cy * cp;
    }
    function beginFx(vpOverride) {
      gl.useProgram(fxProg);
      gl.uniformMatrix4fv(UFX.vp, false, vpOverride || vpMat);
      gl.uniform3f(UFX.camPos, fxCamX, fxCamY, fxCamZ);
      gl.enable(gl.BLEND);
      gl.depthMask(false);
      gl.disable(gl.CULL_FACE);
    }
    function endFx() {
      gl.disable(gl.BLEND);
      gl.depthMask(true);
      gl.enable(gl.CULL_FACE);
      // FX passes switch programs — restore the PBR program (its uniforms are program state)
      gl.useProgram(pbrProg);
    }
    const fxM = M4.ident();
    // additive / alpha billboard sprite facing the camera
    function drawBillboard(x, y, z, size, col, mode, alpha) {
      const hr = size * 0.5;
      const m = fxM;
      m[0] = camRight[0]*hr*2; m[1] = camRight[1]*hr*2; m[2] = camRight[2]*hr*2; m[3] = 0;
      m[4] = camUp[0]*hr*2;    m[5] = camUp[1]*hr*2;    m[6] = camUp[2]*hr*2;    m[7] = 0;
      m[8] = camFwd[0]*hr;     m[9] = camFwd[1]*hr;     m[10] = camFwd[2]*hr;    m[11] = 0;
      m[12] = x; m[13] = y; m[14] = z; m[15] = 1;
      gl.uniformMatrix4fv(UFX.model, false, m);
      gl.uniform3f(UFX.color, col[0], col[1], col[2]);
      gl.uniform1f(UFX.alpha, alpha === undefined ? 1 : alpha);
      gl.uniform1f(UFX.mode, mode);
      gl.uniform1f(UFX.additive, mode === 1 ? 0.0 : 1.0);
      gl.uniform1f(UFX.flicker, 1.0);
      gl.uniform3fv(UFX.fog, ATMOSPHERES[S.atmIdx].fogCol);
      gl.uniform1f(UFX.fogD, ATMOSPHERES[S.atmIdx].fogD);
      gl.bindVertexArray(meshes.quad.vao);
      gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
      S.drawCalls++;
    }
    // flat ground-aligned disc (shockwave, scorch, pickup aura, telegraph)
    function drawGroundDisc(x, y, z, radius, col, alpha, mode, additive) {
      const m = fxM;
      const r = radius;
      m[0] = r*2; m[1] = 0; m[2] = 0; m[3] = 0;
      m[4] = 0; m[5] = 0; m[6] = r*2; m[7] = 0;
      m[8] = r; m[9] = 0; m[10] = 0; m[11] = 0;
      m[12] = x; m[13] = y; m[14] = z; m[15] = 1;
      gl.uniformMatrix4fv(UFX.model, false, m);
      gl.uniform3f(UFX.color, col[0], col[1], col[2]);
      gl.uniform1f(UFX.alpha, alpha);
      gl.uniform1f(UFX.mode, mode === undefined ? 1 : mode);
      gl.uniform1f(UFX.additive, additive === false ? 0.0 : 1.0);
      gl.uniform1f(UFX.flicker, 1.0);
      gl.uniform3fv(UFX.fog, ATMOSPHERES[S.atmIdx].fogCol);
      gl.uniform1f(UFX.fogD, ATMOSPHERES[S.atmIdx].fogD);
      gl.bindVertexArray(meshes.quad.vao);
      gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
      S.drawCalls++;
    }

    // ---------- 9D. Weapon Rig Library — 9 distinct multi-part 3D viewmodels ----------
    // All coordinates are in view space: -Z is forward, +X right, +Y up.
    function metalOf(sk) { return [sk.albedo[0], sk.albedo[1], sk.albedo[2], 1]; }
    function drawDrum(px, py, pz, rad, len, ry, sk) {
      pushT(px, py, pz, ry, Math.PI * 0.5, 0, rad * 2, len, rad * 2);
      drawMesh("cyl", MATS.metal, curT(), [2,2], metalOf(sk), [0,0,0], sk.rough, sk.metal);
      popT();
    }
    function drawRail(px, py, pz, len, thick, ry, sk) {
      pushT(px, py, pz, ry, 0, 0, thick, thick, len);
      drawMesh("box", MATS.metal, curT(), [2,2], metalOf(sk), [0,0,0], sk.rough, sk.metal);
      popT();
    }
    function drawGlowBar(px, py, pz, sx, sy, sz, ry, rx, col) {
      pushT(px, py, pz, ry || 0, rx || 0, 0, sx, sy, sz);
      drawMesh("box", null, curT(), [1,1], [1,1,1,1], col, 0.12, 0.9);
      popT();
    }
    function drawChickenWing(px, py, pz, ry, rx, rz, s, flapAmt) {
      // upper arm (feather material) then 3 primaries fanning outward
      pushT(px, py, pz, ry, rx, rz, s, s, s);
      pushT(0, 0, 0, 0, 0, 0, 0.12, 0.10, 0.30);
      drawMesh("sph", MATS.feather, curT(), [1,1], [0.97,0.96,0.9,1], [0,0,0], 0.62, 0.06);
      popT();
      for (let i = 0; i < 3; i++) {
        const a = -0.34 + i * 0.34;
        pushT(0.02 * (i - 1), -0.03 - i * 0.012, -0.20 - i * 0.02, a, 0, flapAmt + (i - 1) * 0.18, 0.055, 0.018, 0.22);
        drawMesh("box", MATS.feather, curT(), [1,1], [0.99,0.98,0.93,1], [0,0,0], 0.55, 0.08);
        popT();
      }
      popT();
    }

    function drawWeaponRig(w, sk, wx, wy, wz, yaw, pitch, roll, adsT, ammoFrac, muzzleFlash, rigScale) {
      resetT();
      const id = WEAPONS.indexOf(w);
      const t = performance.now() * 0.001;
      // reload cycle progress: mag drops out and back in
      const reloadPhase = S.reloadT > 0 ? 1 - S.reloadT / (w.reload || 1) : 0;
      const magDrop = S.reloadT > 0 ? Math.sin(Math.min(1, reloadPhase * 1.6) * Math.PI) : 0;
      const kick = S.recoil;

      const rs = rigScale === undefined ? 1 : rigScale;
      pushT(wx, wy - (1 - rs) * 0.30, wz, yaw, pitch + kick * 0.5, roll, rs, rs, rs);
      const skA = sk.accent;

      // ---- shared: grip + trigger guard + chicken claw hand (3 toes) ----
      pushT(0, -0.055, 0.12, 0, 0.16, 0, 0.055, 0.13, 0.055);
      drawMesh("cyl", MATS.metal, curT(), [2,2], [0.22,0.24,0.28,1], [0,0,0], 0.5, 0.4);
      popT();
      if (id !== 6) { // launcher uses its own grip
        pushT(0, -0.02, 0.05, 0, 0, 0, 0.10, 0.02, 0.13);
        drawMesh("box", MATS.metal, curT(), [1,1], metalOf(sk), [0,0,0], sk.rough, sk.metal);
        popT();
      }

      if (id === 0) {
        // PISTOL: slide, frame, exposed barrel, rear sight, hammer
        drawRail(0, 0.045, -0.06, 0.30, 0.055, 0, sk);
        pushT(0, 0.072, -0.06, 0, 0, 0, 0.052, 0.028, 0.30);
        drawMesh("box", MATS.metal, curT(), [2,2], [0.42,0.45,0.5,1], [0,0,0], 0.24, 0.94); popT();
        pushT(0, 0.008, -0.20, 0, Math.PI*0.5, 0, 0.032, 0.09, 0.032);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.3,0.32,0.36,1], [0,0,0], 0.18, 0.96); popT();
        drawGlowBar(0, 0.076, 0.04, 0.012, 0.008, 0.16, 0, 0, skA);
        drawGlowBar(0, 0.088, -0.10, 0.016, 0.012, 0.016, 0, 0, [2.4,0.3,0.35]);
        pushT(0, -0.02, -0.02, 0, 0, 0, 0.07, 0.05, 0.16);
        drawMesh("box", MATS.metal, curT(), [1,1], metalOf(sk), [0,0,0], sk.rough, sk.metal); popT();
      } else if (id === 1) {
        // SMG: perforated shroud, top rail, folding stock, mag, foregrip
        pushT(0, 0.02, -0.08, 0, 0, 0, 0.10, 0.115, 0.40);
        drawMesh("box", MATS.metal, curT(), [2,2], metalOf(sk), [0,0,0], sk.rough, sk.metal); popT();
        for (let i = 0; i < 5; i++) drawGlowBar(0.052, 0.045, -0.18 + i*0.055, 0.006, 0.02, 0.018, 0, 0, [0.3,0.9,1.2]);
        for (let i = -1; i <= 1; i++) drawGlowBar(i*0.032, 0.082, -0.09, 0.018, 0.012, 0.34, 0, 0, [0.5,0.62,0.75]);
        pushT(0, -0.075, -0.02 - magDrop*0.16, 0, 0, 0, 0.058, 0.20, 0.075);
        drawMesh("box", MATS.metal, curT(), [1,1], [0.26,0.3,0.36,1], [0,0,0], 0.4, 0.75); popT();
        pushT(0, -0.05, -0.24, 0.1, 0, 0, 0.05, 0.11, 0.05);
        drawMesh("cyl", MATS.metal, curT(), [1,1], metalOf(sk), [0,0,0], 0.5, 0.5); popT();
        drawGlowBar(0, 0.02, -0.32, 0.03, 0.03, 0.09, 0, 0, skA);
      } else if (id === 2) {
        // ASSAULT RIFLE: receiver, long barrel w/ gas block, handguard, stock, optic, mag
        pushT(0, 0.02, -0.02, 0, 0, 0, 0.095, 0.125, 0.34);
        drawMesh("box", MATS.metal, curT(), [2,2], metalOf(sk), [0,0,0], sk.rough, sk.metal); popT();
        pushT(0, 0.03, -0.34, 0, 0, 0, 0.085, 0.075, 0.26);
        drawMesh("box", MATS.panel, curT(), [2,1], [0.3,0.34,0.38,1], [0,0,0], 0.55, 0.35); popT();
        for (let i = 0; i < 4; i++) drawGlowBar(0.045, 0.03, -0.26 - i*0.06, 0.005, 0.045, 0.012, 0, 0, skA);
        pushT(0, 0.035, -0.52, 0, Math.PI*0.5, 0, 0.026, 0.24, 0.026);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.24,0.26,0.3,1], [0,0,0], 0.22, 0.95); popT();
        pushT(0, 0.075, -0.58, 0, Math.PI*0.5, 0, 0.045, 0.06, 0.045);
        drawMesh("cyl", null, curT(), [1,1], [1,1,1,1], skA, 0.2, 0.8); popT();
        pushT(0, 0.105, -0.10, 0, 0, 0, 0.045, 0.055, 0.15);
        drawMesh("box", MATS.metal, curT(), [1,1], [0.12,0.14,0.18,1], [0.05,0.35,0.5], 0.2, 0.7); popT();
        pushT(0, -0.085, -0.04 - magDrop*0.2, 0.12, 0, 0, 0.06, 0.22, 0.09);
        drawMesh("box", MATS.metal, curT(), [1,1], [0.22,0.26,0.32,1], [0,0,0], 0.45, 0.7); popT();
        pushT(0, 0.02, 0.20, 0, 0, 0, 0.06, 0.09, 0.14);
        drawMesh("box", MATS.panel, curT(), [1,1], metalOf(sk), [0,0,0], 0.6, 0.3); popT();
      } else if (id === 3) {
        // SHOTGUN: double tube, pump forend, wide muzzle, shell saddle
        pushT(0, 0.03, -0.04, 0, 0, 0, 0.095, 0.10, 0.28);
        drawMesh("box", MATS.crate, curT(), [2,2], [0.36,0.24,0.16,1], [0,0,0], 0.7, 0.1); popT();
        pushT(0, 0.055, -0.30, 0, Math.PI*0.5, 0, 0.035, 0.42, 0.035);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.2,0.22,0.26,1], [0,0,0], 0.25, 0.95); popT();
        pushT(0, 0.005, -0.30, 0, Math.PI*0.5, 0, 0.032, 0.36, 0.032);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.24,0.26,0.3,1], [0,0,0], 0.3, 0.9); popT();
        pushT(0, -0.02, -0.26 - magDrop * 0.05, 0, 0, 0, 0.075, 0.06, 0.16);
        drawMesh("box", MATS.crate, curT(), [2,1], [0.30,0.20,0.13,1], [0,0,0], 0.72, 0.1); popT();
        pushT(0, 0.095, -0.52, 0, Math.PI*0.5, 0, 0.052, 0.05, 0.052);
        drawMesh("cyl", null, curT(), [1,1], [1,1,1,1], [2.4,1.2,0.35], 0.2, 0.9); popT();
        for (let i = 0; i < 4; i++) {
          pushT(-0.055 + (i%2)*0.11, 0.005 + Math.floor(i/2)*0.05, 0.02, 0, 0, Math.PI*0.5, 0.02, 0.055, 0.02);
          drawMesh("cyl", null, curT(), [1,1], [1,0.25,0.2,1], [0.35,0.05,0.04], 0.3, 0.5); popT();
        }
      } else if (id === 4) {
        // SNIPER: long chassis, fluted barrel, brake, big scope w/ lens flare, bipod
        pushT(0, 0.02, 0.02, 0, 0, 0, 0.08, 0.12, 0.36);
        drawMesh("box", MATS.panel, curT(), [2,2], metalOf(sk), [0,0,0], sk.rough, sk.metal); popT();
        pushT(0, 0.035, -0.55, 0, Math.PI*0.5, 0, 0.022, 0.62, 0.022);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.16,0.18,0.22,1], [0,0,0], 0.18, 0.97); popT();
        for (let i = 0; i < 5; i++) {
          pushT(0, 0.035, -0.36 - i*0.075, 0, Math.PI*0.5, 0, 0.036, 0.018, 0.036);
          drawMesh("cyl", MATS.metal, curT(), [1,1], [0.1,0.12,0.15,1], [0,0,0], 0.15, 0.98); popT();
        }
        pushT(0, 0.035, -0.88, 0, Math.PI*0.5, 0, 0.042, 0.10, 0.042);
        drawMesh("cyl", null, curT(), [1,1], [1,1,1,1], [0.7,1.6,2.2], 0.2, 0.9); popT();
        pushT(0, 0.135, -0.10, 0, 0, 0, 0.055, 0.055, 0.20);
        drawMesh("cyl", MATS.metal, curT(), [2,2], [0.1,0.12,0.16,1], [0.15,0.45,0.6], 0.14, 0.92); popT();
        pushT(0, 0.135, -0.21, 0, 0, 0, 0.062, 0.062, 0.02);
        drawMesh("cyl", null, curT(), [1,1], [1,1,1,1], [1.2,2.6,3.0], 0.05, 0.95); popT();
        drawGlowBar(0, 0.02, -0.02, 0.006, 0.03, 0.30, 0, 0, skA);
        pushT(0, -0.075, -0.22, 0, 0, 0.5, 0.02, 0.16, 0.02);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.2,0.22,0.26,1], [0,0,0], 0.3, 0.9); popT();
      } else if (id === 5) {
        // MINIGUN: 6-barrel rotating cluster + spin ring + ammo belt + motor
        const spin = t * (S.fireCd > 0 ? 34 : 5);
        pushT(0, 0.02, 0.02, 0, 0, 0, 0.115, 0.13, 0.26);
        drawMesh("box", MATS.metal, curT(), [2,2], metalOf(sk), [0,0,0], sk.rough, sk.metal); popT();
        for (let i = 0; i < 6; i++) {
          const a = spin + (i / 6) * Math.PI * 2;
          pushT(Math.cos(a) * 0.052, 0.045 + Math.sin(a) * 0.052, -0.42, 0, Math.PI*0.5, 0, 0.026, 0.62, 0.026);
          drawMesh("cyl", MATS.metal, curT(), [1,1], [0.28,0.3,0.34,1], [0,0,0], 0.22, 0.96); popT();
        }
        for (const zz of [-0.16, -0.70]) {
          pushT(0, 0.045, zz, 0, 0, 0, 0.16, 0.16, 0.035);
          drawMesh("cyl", MATS.metal, curT(), [2,2], metalOf(sk), [0,0,0], sk.rough, sk.metal); popT();
        }
        pushT(0, -0.02, -0.14, 0, 0, 0, 0.09, 0.09, 0.16);
        drawMesh("cyl", null, curT(), [1,1], [1,1,1,1], [1.4,1.1,0.2], 0.2, 0.8); popT();
        for (let i = 0; i < 6; i++) {
          pushT(0.075, -0.06 - i*0.012, -0.02 + i*0.03, 0, 0, 0.4, 0.018, 0.018, 0.05);
          drawMesh("box", null, curT(), [1,1], [1,0.72,0.2,1], [0.8,0.55,0.15], 0.35, 0.5); popT();
        }
      } else if (id === 6) {
        // GRENADE LAUNCHER: fat tube, revolving cylinder w/ 4 warheads, iron ladder
        pushT(0, 0.03, -0.06, 0, 0, 0, 0.13, 0.13, 0.34);
        drawMesh("cyl", MATS.hazard, curT(), [2,2], [0.34,0.34,0.26,1], [0,0,0], 0.5, 0.6); popT();
        pushT(0, 0.03, -0.26, 0, 0, 0, 0.115, 0.115, 0.20);
        drawMesh("cyl", MATS.metal, curT(), [2,2], [0.24,0.26,0.3,1], [0,0,0], 0.3, 0.8); popT();
        for (let i = 0; i < 4; i++) {
          const a = t * 1.4 + (i/4) * Math.PI * 2;
          const mz = -0.30;
          const ox = Math.cos(a) * 0.075, oy = 0.03 + Math.sin(a) * 0.075;
          pushT(ox, oy, mz, 0, Math.PI*0.5, 0, 0.042, 0.12, 0.042);
          drawMesh("cyl", null, curT(), [1,1], [1,0.55,0.2,1], [1.5,0.4,0.12], 0.3, 0.7); popT();
        }
        pushT(0, 0.135, -0.06, 0, 0, 0, 0.02, 0.05, 0.28);
        drawMesh("box", MATS.metal, curT(), [1,1], [0.2,0.22,0.26,1], [0,0,0], 0.35, 0.9); popT();
        drawGlowBar(0, 0.145, -0.20, 0.045, 0.012, 0.045, 0, 0, [2.6,0.9,0.2]);
      } else if (id === 7) {
        // RAILGUN: twin rails, coil stack, capacitor glow, skeletal stock
        for (const side of [-1, 1]) {
          pushT(side * 0.045, 0.03, -0.34, 0, 0, 0, 0.024, 0.035, 0.70);
          drawMesh("box", MATS.metal, curT(), [2,2], [0.2,0.24,0.3,1], [0.1,0.5,0.7], 0.2, 0.96); popT();
        }
        for (let i = 0; i < 6; i++) {
          pushT(0, 0.03, -0.12 - i * 0.105, 0, 0, 0, 0.115, 0.075, 0.028);
          drawMesh("box", MATS.metal, curT(), [1,1], metalOf(sk), [0,0,0], sk.rough, sk.metal); popT();
          pushT(0, 0.03, -0.12 - i * 0.105, 0, 0, 0, 0.125, 0.05, 0.012);
          drawMesh("cyl", null, curT(), [1,1], [1,1,1,1], [0.3 * (i+1) / 3, 2.2, 2.9], 0.1, 0.9); popT();
        }
        pushT(0, 0.03, -0.74, 0, 0, 0, 0.09, 0.09, 0.05);
        drawMesh("cyl", null, curT(), [1,1], [0.4,1,1,1], [0.6,3.2,3.8], 0.05, 0.9); popT();
        drawGlowBar(0, 0.03, -0.02, 0.13, 0.045, 0.10, 0, 0, [0.26,1.5,1.9]);
        pushT(0, 0.115, -0.02, 0, 0, 0, 0.045, 0.045, 0.20);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.12,0.14,0.18,1], [0,0,0], 0.2, 0.9); popT();
      } else {
        // PLASMA: open frame, toroidal emitter, liquid cells, vents
        pushT(0, 0.02, -0.02, 0, 0, 0, 0.095, 0.115, 0.30);
        drawMesh("box", MATS.panel, curT(), [2,2], metalOf(sk), [0,0,0], sk.rough, sk.metal); popT();
        pushT(0, 0.04, -0.30, 0, 0, 0, 0.14, 0.14, 0.05);
        drawMesh("cyl", null, curT(), [1,1], [0.3,1.6,0.7,1], [0.5,3.0,1.2], 0.1, 0.9); popT();
        pushT(0, 0.04, -0.30, 0, 0, 0, 0.09, 0.09, 0.055);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.16,0.3,0.24,1], [0,0,0], 0.25, 0.9); popT();
        for (let i = 0; i < 3; i++) {
          pushT(-0.05 + i * 0.05, 0.10, -0.06, 0, Math.PI*0.5, 0, 0.024, 0.14, 0.024);
          drawMesh("cyl", null, curT(), [1,1], [0.4,2.2,1.0,1], [0.4,2.0,0.9], 0.15, 0.7); popT();
        }
        for (let i = 0; i < 3; i++) drawGlowBar(0.052, 0.0, -0.16 + i*0.07, 0.008, 0.06, 0.02, 0, 0, [0.4,2.4,1.1]);
      }

      // ---- 3-toed claw gripping the frame ----
      for (let i = 0; i < 3; i++) {
        pushT(0.005 * (i - 1), -0.10, 0.09 + i * 0.035, 0, 0.9 + i * 0.18, (i - 1) * 0.25, 0.018, 0.055, 0.012);
        drawMesh("box", null, curT(), [1,1], [0.96,0.76,0.28,1], [0.25,0.16,0.05], 0.35, 0.3); popT();
      }

      // ---- Muzzle flash: 3 crossed additive starburst sheets + hot core ----
      if (muzzleFlash > 0.001) {
        const mz = -((w.len || 0.7) * 1.05 + 0.10);
        const mf = muzzleFlash;
        const fl = 0.85 + Math.sin(performance.now() * 0.06) * 0.15;
        const fcol = w.pierce ? [0.5, 2.4, 3.0] : w.plasma ? [0.5, 2.6, 1.2] : w.explosive ? [2.8, 1.2, 0.3] : [2.9, 2.1, 0.9];
        beginFx(projMat);
        gl.depthFunc(gl.ALWAYS);
        for (let k = 0; k < 3; k++) {
          const ang = (k / 3) * Math.PI + t * 0.7;
          const rr = 0.16 * mf * fl, hh = 0.44 * mf * fl;
          const cxr = Math.cos(ang), sxr = Math.sin(ang);
          const m = curT();
          m[0] = cxr * rr * 2; m[1] = sxr * rr * 2; m[2] = 0; m[3] = 0;
          m[4] = -sxr * hh;    m[5] = cxr * hh;    m[6] = 0; m[7] = 0;
          m[8] = 0; m[9] = 0; m[10] = 1; m[11] = 0;
          m[12] = 0; m[13] = 0.03; m[14] = mz; m[15] = 1;
          gl.uniformMatrix4fv(UFX.model, false, m);
          gl.uniform3f(UFX.color, fcol[0], fcol[1], fcol[2]);
          gl.uniform1f(UFX.alpha, Math.min(1, mf * 1.4));
          gl.uniform1f(UFX.mode, 2);
          gl.uniform1f(UFX.additive, 1.0);
          gl.uniform1f(UFX.flicker, 1.0);
          gl.bindVertexArray(meshes.quad.vao);
          gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
        }
        endFx();
        gl.depthFunc(gl.LESS);
      }
      popT();
      resetT();
    }

    // ---------- 9E. Articulated Cyber-Chicken Rig (6 breeds, full anatomy) ----------
    // World-aligned parts use billboards/discs for glow; body parts use the transform stack.
    const BREED_STYLE = {
      melee:    { tint: [0.97,0.96,0.91,1], comb: [0.92,0.13,0.16,1], combEm: [0.55,0.03,0.05], visor: [2.8,0.30,0.35], leg: [0.95,0.68,0.18,1] },
      charger:  { tint: [0.90,0.47,0.19,1], comb: [0.85,0.10,0.12,1], combEm: [0.6,0.03,0.03], visor: [3.0,0.45,0.2], leg: [0.88,0.55,0.14,1] },
      shooter:  { tint: [0.24,0.27,0.36,1], comb: [0.55,0.18,0.72,1], combEm: [0.45,0.1,0.6], visor: [2.2,0.5,3.0], leg: [0.30,0.33,0.42,1] },
      kamikaze: { tint: [1.00,0.36,0.16,1], comb: [1.00,0.55,0.10,1], combEm: [0.9,0.4,0.05], visor: [3.2,0.5,0.15], leg: [0.95,0.30,0.12,1] },
      drone:    { tint: [0.33,0.88,1.00,1], comb: [0.20,0.95,1.00,1], combEm: [0.15,0.8,1.2], visor: [0.5,3.0,3.4], leg: [0.35,0.70,0.85,1] },
      boss:     { tint: [0.95,0.77,0.30,1], comb: [1.00,0.86,0.22,1], combEm: [1.5,1.0,0.15], visor: [3.2,0.6,0.2], leg: [0.90,0.70,0.25,1] }
    };
    const eggKind = 96;

    function drawChickenRig(c, now) {
      const st = BREED_STYLE[c.type] || BREED_STYLE.melee;
      const sc = c.type === "boss" ? 1.85 : c.type === "kamikaze" ? 0.82 : 1.0;
      const yawC = Math.atan2(S.x - c.x, S.z - c.z);
      const cy = Math.cos(yawC), sy = Math.sin(yawC);
      const ph = c.phase;
      const hit = Math.min(1, c.hitFlash || 0);
      const runSpd = c.hp > 0 ? 1 : 0;
      const bob = c.type === "drone" ? 0 : Math.abs(Math.sin(ph)) * 0.055 * sc;

      // Tints: hot white flash when hit, enraged red pulse for boss
      const tint = st.tint.slice();
      if (hit > 0.01) { tint[0] += hit * 1.6; tint[1] += hit * 1.6; tint[2] += hit * 1.6; }
      const emBody = c.enraged ? [1.0, 0.12, 0.08] : [0, 0, 0];
      const flinch = (c.flinch || 0);
      const leanBack = flinch * 0.30;
      const bodyY = c.y + 0.86 * sc + bob - Math.abs(Math.sin(flinch * Math.PI)) * 0.10;

      resetT();

      // ---- LOD: distant birds collapse into a cheap 5-piece silhouette ----
      const camDist = Math.hypot(c.x - fxCamX, c.z - fxCamZ);
      if (camDist > 34) {
        pushT(c.x, bodyY, c.z, yawC, 0, Math.sin(ph) * 0.07, 1, 1, 1);
        pushT(0, 0, 0.02 * sc, 0, 0, 0, 0.95 * sc, 0.90 * sc, 1.14 * sc);
        drawMesh("sph", MATS.feather, curT(), [2,2], tint, emBody, 0.68, 0.06); popT();
        pushT(0, 0.52 * sc, 0.26 * sc, 0, 0.12, 0, 0.52 * sc, 0.52 * sc, 0.56 * sc);
        drawMesh("sph", MATS.feather, curT(), [1,1], tint, emBody, 0.62, 0.06); popT();
        pushT(0, 0.74 * sc, 0.26 * sc, 0, 0, 0, 0.14 * sc, 0.22 * sc, 0.18 * sc);
        drawMesh("box", null, curT(), [1,1], st.comb, st.combEm, 0.25, 0.1); popT();
        pushT(0, 0.52 * sc, 0.50 * sc, 0, 0, 0, 0.44 * sc, 0.09 * sc, 0.12 * sc);
        drawMesh("box", null, curT(), [1,1], [1,0.22,0.24,1], st.visor, 0.1, 0.9); popT();
        popT();
        if (c.hp < c.maxHp && c.hp > 0) {
          const frac = Math.max(0, c.hp / c.maxHp);
          beginFx();
          const bw2 = 1.1 * sc;
          gl.uniformMatrix4fv(UFX.model, false, (() => {
            const m = curT();
            m[0] = camRight[0]*bw2; m[1] = camRight[1]*bw2; m[2] = camRight[2]*bw2; m[3] = 0;
            m[4] = camUp[0]*0.09;  m[5] = camUp[1]*0.09;  m[6] = camUp[2]*0.09;  m[7] = 0;
            m[8] = 0; m[9] = 0; m[10] = 1; m[11] = 0;
            m[12] = c.x; m[13] = c.y + 1.85 * sc; m[14] = c.z; m[15] = 1;
            return m;
          })());
          gl.uniform3f(UFX.color, frac > 0.5 ? 0.3 : 1.6, frac > 0.25 ? 1.4 : 0.2, frac > 0.5 ? 0.6 : 0.2);
          gl.uniform1f(UFX.alpha, 0.9); gl.uniform1f(UFX.mode, 2); gl.uniform1f(UFX.additive, 1.0);
          gl.uniform1f(UFX.flicker, 1.0);
          gl.uniform3fv(UFX.fog, ATMOSPHERES[S.atmIdx].fogCol); gl.uniform1f(UFX.fogD, 0.0);
          gl.bindVertexArray(meshes.quad.vao);
          gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
          endFx();
        }
        resetT();
        return;
      }

      // ---- LEGS: 2 articulated legs, thigh/shin/foot + 3 claw toes, walk cycle ----
      const legs = [[-1, 0], [1, Math.PI]];
      legs.forEach(([side, phaseOff], li) => {
        const swing = Math.sin(ph + phaseOff) * 0.55 * runSpd;
        const lift = Math.max(0, Math.cos(ph + phaseOff)) * 0.22 * sc;
        const hipX = c.x + cy * side * 0.26 * sc;
        const hipZ = c.z - sy * side * 0.26 * sc;
        pushT(hipX, c.y + 0.60 * sc, hipZ, yawC, 0, side * 0.10, 1, 1, 1);
        // thigh
        pushT(0, -0.16 * sc, 0, 0, swing * 0.9, 0, 0.085 * sc, 0.34 * sc, 0.085 * sc);
        drawMesh("cyl", null, curT(), [1,1], st.leg, [0,0,0], 0.55, 0.45); popT();
        pushT(0, -0.32 * sc, 0, 0, swing * 0.9 - 0.35, 0, 1, 1, 1);
        // shin
        pushT(0, -0.12 * sc, 0, 0, 0, 0, 0.055 * sc, 0.26 * sc, 0.055 * sc);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.34,0.36,0.40,1], [0,0,0], 0.35, 0.6); popT();
        // cyber ankle joint
        pushT(0, -0.24 * sc, 0, 0, 0, 0, 0.075 * sc, 0.06 * sc, 0.075 * sc);
        drawMesh("sph", null, curT(), [1,1], [0.55,0.6,0.68,1], [0.1,0.16,0.2], 0.25, 0.9); popT();
        // foot + toes
        pushT(0, -0.27 * sc + lift * 0.4, 0.04 * sc, 0, -0.18, 0, 1, 1, 1);
        pushT(0, 0, 0, 0, 0, 0, 0.10 * sc, 0.045 * sc, 0.16 * sc);
        drawMesh("box", null, curT(), [1,1], st.leg, [0,0,0], 0.5, 0.25); popT();
        for (let ti = -1; ti <= 1; ti++) {
          pushT(ti * 0.045 * sc, -0.005, 0.13 * sc, ti * 0.35, 0.25, 0, 0.028 * sc, 0.028 * sc, 0.10 * sc);
          drawMesh("cone", null, curT(), [1,1], [0.92,0.80,0.42,1], [0.1,0.1,0.03], 0.35, 0.5); popT();
        }
        popT(); popT();
      });

      // ---- TAIL FAN: 5 layered feathers, splayed + idle sway ----
      const tailSway = Math.sin(ph * 0.6) * 0.10;
      pushT(c.x - sy * 0.62 * sc, bodyY + 0.22 * sc, c.z - cy * 0.62 * sc, yawC, 0, 0, 1, 1, 1);
      for (let i = 0; i < 5; i++) {
        const a = (i - 2) * 0.30;
        pushT(Math.sin(a) * 0.10 * sc, (i === 2 ? 0.06 : Math.abs(i - 2) * -0.02) * sc, -0.10 * sc,
              yawC + a + tailSway, -0.55 - Math.abs(i - 2) * 0.08, a * 0.3, 0.09 * sc, 0.035 * sc, 0.40 * sc);
        drawMesh("box", MATS.feather, curT(), [1,1], tint, [0,0,0], 0.62, 0.06);
        popT();
      }
      popT();

      // ---- MAIN BODY: breast, back, belly plates, cyber harness ----
      pushT(c.x, bodyY, c.z, yawC, -leanBack, Math.sin(ph) * 0.07, 1, 1, 1);
      pushT(0, 0, 0.02 * sc, 0, 0, 0, 0.93 * sc, 0.88 * sc, 1.12 * sc);
      drawMesh("sph", MATS.feather, curT(), [2,2], tint, emBody, 0.68, 0.06); popT();
      // breast plate (armor)
      pushT(0, -0.10 * sc, 0.42 * sc, 0, 0.1, 0, 0.62 * sc, 0.50 * sc, 0.34 * sc);
      drawMesh("sph", MATS.panel, curT(), [1,1], [0.30,0.35,0.44,1], [0.05,0.08,0.12], 0.38, 0.72); popT();
      // spine glow strip
      pushT(0, 0.34 * sc, -0.02 * sc, 0, 0, 0, 0.16 * sc, 0.05 * sc, 0.86 * sc);
      drawMesh("box", null, curT(), [1,1], [1,1,1,1], st.visor, 0.2, 0.8); popT();
      popT();

      // ---- NECK: 2 segments, slight forward curve ----
      const neckYaw = yawC;
      pushT(c.x + sy * 0.18 * sc, bodyY + 0.34 * sc, c.z + cy * 0.18 * sc, neckYaw, 0.22, 0, 1, 1, 1);
      pushT(0, 0.13 * sc, 0, 0, 0, 0, 0.19 * sc, 0.26 * sc, 0.19 * sc);
      drawMesh("cyl", MATS.feather, curT(), [1,1], tint, emBody, 0.66, 0.06); popT();
      // head
      const hx = 0, hy = 0.34 * sc, hz = 0.02 * sc;
      pushT(hx, hy, hz, 0, 0, 0, 0.52 * sc, 0.50 * sc, 0.54 * sc);
      drawMesh("sph", MATS.feather, curT(), [1,1], tint, emBody, 0.62, 0.06); popT();

      // ---- 3-CREST COMB (razor plates) + wattle + beak halves + eyes ----
      pushT(hx, hy + 0.24 * sc, hz, 0, 0, 0, 1, 1, 1);
      for (let i = 0; i < 3; i++) {
        const s = 1 - Math.abs(i - 1) * 0.22;
        pushT(0, 0.10 * sc * s, (-0.10 + i * 0.10) * sc, 0, -0.12, 0, 0.075 * sc * s, 0.20 * sc * s, 0.16 * sc);
        drawMesh("box", null, curT(), [1,1], st.comb, st.combEm, 0.25, c.type === "boss" ? 0.95 : 0.1); popT();
      }
      popT();
      // wattle
      pushT(hx, hy - 0.20 * sc, hz + 0.24 * sc, 0, 0.35 + Math.sin(now * 0.006) * 0.12, 0, 0.05 * sc, 0.16 * sc, 0.05 * sc);
      drawMesh("sph", null, curT(), [1,1], [0.85,0.12,0.16,1], [0.30,0.02,0.03], 0.4, 0.1); popT();
      // upper + lower beak (lower animates a cluck)
      const cluck = Math.max(0, Math.sin(now * 0.004 + c.phase)) * 0.12;
      pushT(hx, hy - 0.02 * sc, hz + 0.26 * sc, 0, Math.PI * 0.5, 0, 0.17 * sc, 0.26 * sc, 0.14 * sc);
      drawMesh("cone", null, curT(), [1,1], [1.0,0.70,0.16,1], [0.22,0.10,0.02], 0.32, 0.12); popT();
      pushT(hx, hy - 0.09 * sc - cluck * sc, hz + 0.24 * sc, 0, Math.PI * 0.5 + 0.22 + cluck, 0, 0.13 * sc, 0.20 * sc, 0.10 * sc);
      drawMesh("cone", null, curT(), [1,1], [0.92,0.60,0.12,1], [0.14,0.06,0.01], 0.34, 0.12); popT();
      // eyes: sclera + dark pupil + cyber visor across both eyes
      for (const side of [-1, 1]) {
        pushT(hx + cy * side * 0.20 * sc, hy + 0.09 * sc, hz - sy * side * 0.20 * sc, 0, 0, 0, 0.13 * sc, 0.15 * sc, 0.13 * sc);
        drawMesh("sph", null, curT(), [1,1], [0.99,0.98,0.94,1], [0.12,0.12,0.12], 0.18, 0.1); popT();
        pushT(hx + cy * side * 0.26 * sc, hy + 0.09 * sc, hz - sy * side * 0.26 * sc, 0, 0, 0, 0.065 * sc, 0.075 * sc, 0.065 * sc);
        drawMesh("sph", null, curT(), [1,1], [0.04,0.04,0.06,1], [0,0,0], 0.1, 0.2); popT();
      }
      pushT(hx, hy + 0.06 * sc, hz + 0.22 * sc, 0, 0, 0, 0.50 * sc, 0.10 * sc, 0.14 * sc);
      drawMesh("box", null, curT(), [1,1], [1,0.22,0.24,1], st.visor, 0.1, 0.9); popT();

      // ---- WINGS: 3 feathers per side, flap + banking ----
      const flap = Math.sin(ph * (c.type === "drone" ? 2.4 : 1.15)) * 0.55;
      [-1, 1].forEach(side => {
        pushT(c.x + cy * side * 0.46 * sc, bodyY + 0.20 * sc, c.z - sy * side * 0.46 * sc, yawC, 0, side * flap, 1, 1, 1);
        for (let i = 0; i < 3; i++) {
          const a = 0.18 + i * 0.30;
          pushT(side * (0.10 + i * 0.10) * sc, -i * 0.045 * sc, -i * 0.06 * sc, side * a, 0.1 * i, side * (flap * 0.25), 0.075 * sc, 0.035 * sc, (0.42 - i * 0.06) * sc);
          drawMesh("box", MATS.feather, curT(), [1,1], tint, [0,0,0], 0.6, 0.06); popT();
        }
        popT();
      });
      popT();

      // ---- BREED GEAR ----
      if (c.type === "kamikaze") {
        const blink = (Math.sin(now * 0.025) > 0) ? 3.4 : 0.35;
        pushT(c.x, bodyY + 0.40 * sc, c.z - cy * 0.12 * sc, yawC, 0, 0, 1, 1, 1);
        // C4 pack + 4 sticks + status lamp + antenna
        pushT(0, 0, 0, 0, 0, 0, 0.44 * sc, 0.30 * sc, 0.30 * sc);
        drawMesh("box", MATS.crate, curT(), [1,1], [0.30,0.26,0.18,1], [0,0,0], 0.7, 0.2); popT();
        for (let i = -1; i <= 1; i++) {
          pushT(i * 0.11 * sc, 0.02, 0.02, 0, 0, 0, 0.055 * sc, 0.34 * sc, 0.055 * sc);
          drawMesh("cyl", null, curT(), [1,1], [1,0.42,0.16,1], [0.4,0.1,0.02], 0.4, 0.2); popT();
        }
        pushT(0, 0.19 * sc, 0, 0, 0, 0, 0.11 * sc, 0.11 * sc, 0.11 * sc);
        drawMesh("sph", null, curT(), [1,1], [1,0.15,0.1,1], [blink, 0.15, 0.08], 0.15, 0.5); popT();
        pushT(0.12 * sc, 0.20 * sc, 0, 0, 0, 0, 0.012, 0.30 * sc, 0.012);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.4,0.4,0.44,1], [0,0,0], 0.3, 0.9); popT();
        popT();
      } else if (c.type === "drone") {
        // twin turbojets with spinning intake fans + nozzle cones
        [-1, 1].forEach(side => {
          pushT(c.x + cy * side * 0.52 * sc, bodyY - 0.02 * sc, c.z - sy * side * 0.52 * sc, yawC, 0, 0, 1, 1, 1);
          pushT(0, 0, 0, 0, Math.PI * 0.5, 0, 0.20 * sc, 0.34 * sc, 0.20 * sc);
          drawMesh("cyl", MATS.metal, curT(), [1,1], [0.26,0.30,0.36,1], [0.05,0.4,0.55], 0.25, 0.9); popT();
          const fanA = now * 0.03 * side;
          for (let b = 0; b < 4; b++) {
            const a2 = fanA + b * Math.PI * 0.5;
            pushT(Math.cos(a2) * 0.09 * sc, 0, Math.sin(a2) * 0.09 * sc, 0, Math.PI*0.5, 0, 0.14 * sc, 0.012, 0.05 * sc);
            drawMesh("box", null, curT(), [1,1], [0.6,0.85,1,1], [0.1,0.5,0.7], 0.25, 0.8); popT();
          }
          pushT(0, -0.22 * sc, 0, 0, 0, 0, 0.14 * sc, 0.24 * sc, 0.14 * sc);
          drawMesh("cone", null, curT(), [1,1], [0.3,0.9,1,1], [0.3,2.2,2.9], 0.15, 0.85); popT();
          popT();
        });
      } else if (c.type === "shooter") {
        // shoulder-mounted egg cannon + ammo drum
        pushT(c.x + cy * 0.30 * sc, bodyY + 0.34 * sc, c.z - sy * 0.30 * sc, yawC, -0.18, 0, 1, 1, 1);
        pushT(0, 0.06, 0.12 * sc, 0, Math.PI * 0.5, 0, 0.11 * sc, 0.44 * sc, 0.11 * sc);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.22,0.24,0.3,1], [0.05,0.2,0.35], 0.3, 0.85); popT();
        pushT(0, 0.06, 0.36 * sc, 0, Math.PI * 0.5, 0, 0.13 * sc, 0.06 * sc, 0.13 * sc);
        drawMesh("cyl", null, curT(), [1,1], [1,0.35,0.45,1], [1.6,0.3,0.4], 0.2, 0.7); popT();
        pushT(0, -0.02, -0.06 * sc, 0, 0, 0, 0.14 * sc, 0.14 * sc, 0.14 * sc);
        drawMesh("cyl", MATS.metal, curT(), [1,1], [0.3,0.3,0.34,1], [0,0,0], 0.4, 0.7); popT();
        popT();
      } else if (c.type === "charger") {
        // spiked ram helm + forward horn plate
        pushT(c.x + sy * 0.20 * sc, bodyY + 0.62 * sc, c.z + cy * 0.20 * sc, yawC, 0, 0, 1, 1, 1);
        for (let i = -1; i <= 1; i++) {
          pushT(i * 0.10 * sc, 0.12 * sc, -0.06 * sc, 0, -0.35, i * 0.25, 0.05 * sc, 0.22 * sc, 0.05 * sc);
          drawMesh("cone", MATS.metal, curT(), [1,1], [0.72,0.74,0.8,1], [0.25,0.18,0.1], 0.22, 0.95); popT();
        }
        popT();
      } else if (c.type === "boss") {
        // golden crown spikes, shoulder cannons, cape plates
        pushT(c.x + cy * 0.34 * sc, bodyY + 0.30 * sc, c.z - sy * 0.34 * sc, yawC, 0, 0, 1, 1, 1);
        [-1, 1].forEach(side => {
          pushT(side * 0.28 * sc, 0.12 * sc, 0, 0, 0, side * 0.18, 0.14 * sc, 0.16 * sc, 0.34 * sc);
          drawMesh("cyl", MATS.metal, curT(), [1,1], [0.9,0.72,0.24,1], [0.5,0.35,0.05], 0.18, 0.96); popT();
          pushT(side * 0.28 * sc, 0.12 * sc, 0.24 * sc, 0, Math.PI*0.5, 0, 0.10 * sc, 0.18 * sc, 0.10 * sc);
          drawMesh("cyl", null, curT(), [1,1], [1,0.4,0.2,1], [2.4,0.6,0.2], 0.2, 0.8); popT();
        });
        popT();
        pushT(c.x, bodyY - 0.05 * sc, c.z - cy * 0.5 * sc, yawC, 0.12, 0, 0.86 * sc, 0.06 * sc, 0.66 * sc);
        drawMesh("box", null, curT(), [1,1], [0.62,0.10,0.36,1], [0.4,0.05,0.2], 0.4, 0.5); popT();
      }

      // ---- Floating 3D HP bar above damaged enemies ----
      if (c.hp < c.maxHp && c.hp > 0) {
        const bw = (c.type === "boss" ? 2.2 : 1.15) * sc;
        const frac = Math.max(0, c.hp / c.maxHp);
        const barY = c.y + (c.type === "boss" ? 2.55 : 1.72) * sc;
        const hcol = frac > 0.55 ? [0.30,1.6,0.7] : frac > 0.25 ? [1.8,1.1,0.2] : [1.9,0.25,0.3];
        beginFx();
        // track
        pushT(c.x, barY, c.z, 0, 0, 0, 1, 1, 1);
        gl.uniformMatrix4fv(UFX.model, false, (() => {
          const m = curT();
          m[0] = camRight[0]*bw; m[1] = camRight[1]*bw; m[2] = camRight[2]*bw; m[3] = 0;
          m[4] = camUp[0]*0.11;  m[5] = camUp[1]*0.11;  m[6] = camUp[2]*0.11;  m[7] = 0;
          m[8] = 0; m[9] = 0; m[10] = 1; m[11] = 0;
          m[12] = c.x; m[13] = barY; m[14] = c.z; m[15] = 1;
          return m;
        })());
        gl.uniform3f(UFX.color, 0.03, 0.05, 0.08);
        gl.uniform1f(UFX.alpha, 0.72);
        gl.uniform1f(UFX.mode, 1);
        gl.uniform1f(UFX.additive, 0.0);
        gl.uniform3fv(UFX.fog, ATMOSPHERES[S.atmIdx].fogCol);
        gl.uniform1f(UFX.fogD, 0.0);
        gl.bindVertexArray(meshes.quad.vao);
        gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
        // fill
        gl.uniform3f(UFX.color, hcol[0], hcol[1], hcol[2]);
        gl.uniform1f(UFX.mode, 2);
        gl.uniform1f(UFX.additive, 1.0);
        const m2 = curT();
        m2[0] = camRight[0]*bw*frac; m2[1] = camRight[1]*bw*frac; m2[2] = camRight[2]*bw*frac; m2[3] = 0;
        m2[4] = camUp[0]*0.085; m2[5] = camUp[1]*0.085; m2[6] = camUp[2]*0.085; m2[7] = 0;
        m2[8] = 0; m2[9] = 0; m2[10] = 1; m2[11] = 0;
        m2[12] = c.x - camRight[0]*bw*0.5*(1-frac); m2[13] = barY; m2[14] = c.z - camRight[2]*bw*0.5*(1-frac); m2[15] = 1;
        gl.uniformMatrix4fv(UFX.model, false, m2);
        gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
        endFx();
        popT();
      }
      resetT();
    }

    // ---------- 10. Main Simulation & WebGL2 Render Loop ----------
    let lastNow = performance.now(), fpsAcc = 0, fpsFrames = 0;

    let frameErrors = 0, firstFrameAt = 0;
    function tick(now) {
      if (!firstFrameAt) firstFrameAt = now;
      // Safety net: if after 2.5 s of running nothing was ever flushed, the
      // FBO path is silently dead on this driver -> drop to compat automatically.
      if (!RT.failed && !RT.sceneDrawn && now - firstFrameAt > 2500) {
        RT.failed = true;
        try { opts.pushFeed?.("⚠ Драйвер не отрисовал HDR-проход — включён режим совместимости"); } catch (_) {}
        if (window.__cgFatal) console.warn("[CHIKENGUN] FBO path produced no frames; compat mode");
      }
      try {
        frameBody(now);
      } catch (err) {
        frameErrors++;
        console.error("[CHIKENGUN] frame error #" + frameErrors, err);
        // First failure: abandon the FBO pipeline and continue straight-to-screen.
        RT.failed = true;
        RT.msFBO = null;
        if (typeof gl.bindFramebuffer === "function") gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        if (frameErrors === 1) {
          try { opts.pushFeed?.("⚠ Графика: включён режим совместимости (без HDR/Bloom)"); } catch (_) {}
        }
      }
      // Always keep the loop alive — a single bad frame must never freeze the game
      requestAnimationFrame(tick);
    }

    function frameBody(now) {
      const rawDt = Math.min(0.05, (now - lastNow) / 1000);
      lastNow = now;
      fpsAcc += rawDt; fpsFrames++;
      if (fpsAcc >= 0.4) { S.fps = Math.round(fpsFrames / fpsAcc); fpsAcc = 0; fpsFrames = 0; }

      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      const cw = Math.floor(window.innerWidth * dpr), ch = Math.floor(window.innerHeight * dpr);
      if (canvas.width !== cw || canvas.height !== ch) { canvas.width = cw; canvas.height = ch; }

      const stopMul = (S.hitstop || 0) > 0 ? 0.35 : 1.0;
      const dt = (S.slowMo > 0 ? (S.slowMo -= rawDt, rawDt * 0.38) : rawDt) * stopMul;
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
          const dx = S.x - c.x, dz = S.z - c.z, d = Math.hypot(dx, dz) || 1;
          const baseSpd = (c.type === "kamikaze" ? 6.2 : c.type === "charger" ? 4.6 : c.type === "drone" ? 3.4 : 2.8) * mutSpd * (c.enraged ? 1.32 : 1.0);

          if (c.type === "kamikaze") {
            c.beepT -= dt;
            if (c.beepT <= 0) { c.beepT = Math.max(0.12, d / 26); if (d < 18) opts.playSfx("KamikazeBeep.wav", 0.35); }
          }

          if (c.type === "shooter" || c.type === "drone") {
            const sx = -dz / d, sz = dx / d;
            const approach = d > 15 ? 1 : (d < 8 ? -0.6 : 0);
            c.x += (dx/d * approach + sx * Math.sin(c.phase*0.25) * 0.7) * baseSpd * dt;
            c.z += (dz/d * approach + sz * Math.sin(c.phase*0.25) * 0.7) * baseSpd * dt;
            if (c.type === "drone") c.y = 4.2 + Math.sin(c.phase * 0.35) * 0.65;
            c.atkCd -= dt;
            if (c.atkCd <= 0 && d < 32) {
              c.atkCd = c.type === "drone" ? 1.55 : 1.9;
              const ey = c.y + 1.0, len = Math.hypot(dx, (S.y - 0.2) - ey, dz) || 1;
              S.enemyEggs.push({ x: c.x, y: ey, z: c.z, vx: dx/len*19, vy: ((S.y-0.2)-ey)/len*19, vz: dz/len*19, dmg: 10, ttl: 2.5 });
            }
          } else {
            if (d > 1.7) { c.x += (dx/d)*baseSpd*dt; c.z += (dz/d)*baseSpd*dt; }
            c.atkCd -= dt;
            if (d <= 2.1 && c.atkCd <= 0) {
              c.atkCd = c.enraged ? 0.85 : 1.15;
              if (c.type === "kamikaze") { c.hp = 0; explodeAt(c.x, 0.5, c.z, 6.0, 38); hurtPlayer(26); }
              else hurtPlayer(c.type === "boss" ? (c.enraged ? 30 : 22) : 12);
            } else if (c.type === "boss" && c.atkCd <= 0 && d <= 7.5) {
              // Ground slam: shockwave ring + radial shove
              c.atkCd = c.enraged ? 1.1 : 1.8;
              S.shake = Math.min(0.4, S.shake + 0.26);
              S.waves.push({ x: c.x, y: 0.06, z: c.z, r0: 0.6, r1: 13, ttl: 0.7, maxTtl: 0.7, col: [2.6, 1.1, 0.35] });
              S.waves.push({ x: c.x, y: 0.1, z: c.z, r0: 0.3, r1: 9, ttl: 0.95, maxTtl: 0.95, col: [1.2, 2.2, 2.8] });
              addFlashLight(c.x, 1.2, c.z, 3.0, 1.4, 0.4, 16, 0.3);
              for (let k = 0; k < 18; k++) {
                const a = Math.random() * 6.28, sp = 3 + Math.random() * 7;
                S.particles.push({ x: c.x, y: 0.4, z: c.z, vx: Math.cos(a)*sp, vy: 2 + Math.random()*5, vz: Math.sin(a)*sp,
                  col: [2.4, 1.2, 0.45], sz: 0.12, ttl: 0.7, mode: 0 });
              }
              opts.playSfx("BarrelBoom.wav", 0.45);
              hurtPlayer(c.enraged ? 16 : 10);
            } else if (c.type === "boss" && c.atkCd <= 0 && d > 6) {
              c.atkCd = c.enraged ? 1.5 : 2.3;
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
        S.hurtFx = Math.min(1, Math.max(S.hurtFx || 0, 0.55 + raw * 0.004));
        opts.flashDamage();
        if (S.hp <= 0) {
          S.hp = S.maxHp; S.armor = 50;
          opts.pushFeed("⚡ РЕЗЕРВНЫЙ КИБЕР-КЛОН АКТИВИРОВАН!");
        }
      }

      // Decay FX
      S.beams = S.beams.filter(b => (b.ttl -= dt) > 0);
      S.orbitalStrikes = S.orbitalStrikes.filter(o => (o.ttl -= dt) > 0);
      S.particles = S.particles.filter(p => {
        p.ttl -= dt;
        if (p.mode === 1) { // smoke drifts up, no gravity
          p.vy += 0.9 * dt;
          p.vx *= (1 - 1.6 * dt); p.vz *= (1 - 1.6 * dt);
        } else p.vy -= 16 * dt;
        p.x += p.vx*dt; p.y = Math.max(0.06, p.y + p.vy*dt); p.z += p.vz*dt;
        return p.ttl > 0;
      });
      S.muzzleFlash = Math.max(0, S.muzzleFlash - rawDt * (WEAPONS[S.wep].rof < 0.1 ? 13 : 8));
      S.shellCd = Math.max(0, S.shellCd - rawDt);
      S.featherCd = Math.max(0, S.featherCd - rawDt);
      S.decals = S.decals.filter(d => (d.ttl -= rawDt) > 0);
      S.waves = S.waves.filter(w => (w.ttl -= rawDt) > 0);
      S.chickens.forEach(c => {
        c.hitFlash = Math.max(0, (c.hitFlash || 0) - rawDt * 5.5);
        c.flinch = Math.max(0, (c.flinch || 0) - rawDt * 4.0);
      });
      // Shell casings: ballistic bounce, spin, fade
      S.shells = S.shells.filter(sh => {
        sh.ttl -= rawDt;
        sh.vy -= 19 * rawDt;
        sh.x += sh.vx*rawDt; sh.y += sh.vy*rawDt; sh.z += sh.vz*rawDt;
        if (sh.y < 0.06) { sh.y = 0.06; sh.vy = Math.abs(sh.vy)*0.42; sh.vx *= 0.7; sh.vz *= 0.7; sh.wx *= 0.6; sh.wz *= 0.6; }
        sh.rx += sh.wx*rawDt; sh.ry += sh.wy*rawDt; sh.rz += sh.wz*rawDt;
        return sh.ttl > 0;
      });
      // Feathers flutter with heavy air drag
      S.feathers = S.feathers.filter(f => {
        f.ttl -= rawDt;
        f.vy -= 3.1 * rawDt;
        f.vy = Math.max(-1.15, f.vy);
        f.vx *= (1 - 1.9 * rawDt); f.vz *= (1 - 1.9 * rawDt);
        f.vx += Math.sin(f.ttl * 7 + f.rx) * 0.9 * rawDt;
        f.vz += Math.cos(f.ttl * 6 + f.rz) * 0.9 * rawDt;
        f.x += f.vx*rawDt; f.y = Math.max(0.03, f.y + f.vy*rawDt); f.z += f.vz*rawDt;
        f.rx += f.wx*rawDt; f.ry += f.wy*rawDt; f.rz += f.wz*rawDt;
        return f.ttl > 0;
      });
      // Death ragdolls tumble, settle, then fade
      S.corpses = S.corpses.filter(cp => {
        cp.ttl -= rawDt;
        const settled = cp.y <= 0.32;
        if (!settled) {
          cp.vy -= 20 * rawDt;
          cp.x += cp.vx*rawDt; cp.z += cp.vz*rawDt; cp.y += cp.vy*rawDt;
          cp.tilt += cp.tiltV * rawDt; cp.spin += cp.spinV * rawDt;
          if (cp.y <= 0.30) { cp.y = 0.30; cp.vy = Math.abs(cp.vy) * 0.34; cp.vx *= 0.55; cp.vz *= 0.55; cp.tiltV *= 0.45; cp.spinV *= 0.45; }
        }
        return cp.ttl > 0;
      });
      for (let i = dynLights.length - 1; i >= 0; i--) {
        dynLights[i].ttl -= dt;
        if (dynLights[i].ttl <= 0) dynLights.splice(i, 1);
      }

      // ---------- RENDER PASS ----------
      S.drawCalls = 0;
      const gfx = GFX_PRESETS[gfxIdx];
      // Adaptive resolution: nudge render scale down/up to hold ~55-60 FPS
      dynAcc += rawDt; dynFrames++;
      if (dynAcc >= 1.0) {
        const avgFps = dynFrames / dynAcc;
        if (avgFps < 46 && dynScale > 0.62) dynScale = Math.max(0.62, dynScale - 0.08);
        else if (avgFps > 57 && dynScale < 1.0) dynScale = Math.min(1.0, dynScale + 0.04);
        dynAcc = 0; dynFrames = 0;
      }
      S.renderScale = dynScale;
      // Render-target sizing is capped inside buildRT (VRAM safety on Retina laptops)
      const rtW = Math.max(2, Math.floor(canvas.width * gfx.scale * dynScale));
      const rtH = Math.max(2, Math.floor(canvas.height * gfx.scale * dynScale));
      if (RT.resetNext || RT.w !== rtW || RT.h !== rtH) { RT.resetNext = false; buildRT(rtW, rtH); }
      // If the FBO chain is unusable, render straight to the backbuffer like before
      const USE_FBO = RT_OK && RT.sceneFBO && !RT.failed;
      const TONE_IN_SHADER = USE_FBO ? 0 : 1;
      S.pipeline = USE_FBO ? (gfx.bloom ? "HDR+BLOOM+MSAA" : "HDR") : "COMPAT";
      const sceneW = USE_FBO ? RT.w : canvas.width;
      const sceneH = USE_FBO ? RT.h : canvas.height;

      if (active) {
        S.hurtFx = Math.max(0, (S.hurtFx || 0) - rawDt * 1.6);
        S.hitstop = Math.max(0, (S.hitstop || 0) - rawDt);
      }

      const atm = ATMOSPHERES[S.atmIdx];
      const baseFov0 = S.aiming ? (WEAPONS[S.wep].zoom || 62) : (S.dashT > 0 || S.grappleOn ? 98 : 88);

      // ===== PASS 0: Directional sun shadow map =====
      if (RT.shadowFBO && gfx.shadow) {
        const lvp = updateLightVP(atm.sunDir);
        gl.bindFramebuffer(gl.FRAMEBUFFER, RT.shadowFBO);
        gl.viewport(0, 0, RT.shadowSize, RT.shadowSize);
        gl.disable(gl.BLEND);
        gl.enable(gl.DEPTH_TEST);
        gl.depthMask(true);
        gl.clearColor(1, 1, 1, 1);
        gl.clear(gl.DEPTH_BUFFER_BIT);
        if (gl.cullFace) { gl.enable(gl.CULL_FACE); gl.cullFace(gl.FRONT); }
        gl.useProgram(shadowProg);
        gl.uniformMatrix4fv(USh.vp, false, lvp);
        extractFrustum(lvp);
        for (let i = 0; i < staticProps.length; i++) {
          const p = staticProps[i];
          if (p.br && !sphereVisible(p.cx, p.cy, p.cz, p.br)) continue;
          gl.bindVertexArray(meshes[p.mesh].vao);
          gl.uniformMatrix4fv(USh.model, false, p.model);
          gl.drawElements(gl.TRIANGLES, meshes[p.mesh].count, gl.UNSIGNED_SHORT, 0);
        }
        // dynamic shadow casters: chickens + barrels
        S.chickens.forEach(c => {
          const sc = c.type === "boss" ? 1.85 : c.type === "kamikaze" ? 0.82 : 1.0;
          M4.trs(tmpM, c.x, c.y + 0.85*sc, c.z, Math.atan2(S.x - c.x, S.z - c.z), 0, 0, 1.05*sc, 0.95*sc, 1.25*sc);
          gl.bindVertexArray(meshes.sph.vao);
          gl.uniformMatrix4fv(USh.model, false, tmpM);
          gl.drawElements(gl.TRIANGLES, meshes.sph.count, gl.UNSIGNED_SHORT, 0);
        });
        S.barrels.forEach(b => {
          if (!b.alive) return;
          M4.trs(tmpM, b.x, 0.85, b.z, 0, 0, 0, 1.15, 1.7, 1.15);
          gl.bindVertexArray(meshes.cyl.vao);
          gl.uniformMatrix4fv(USh.model, false, tmpM);
          gl.drawElements(gl.TRIANGLES, meshes.cyl.count, gl.UNSIGNED_SHORT, 0);
        });
        if (gl.cullFace) gl.cullFace(gl.BACK);
        gl.bindVertexArray(null);
      }

      // Bind the HDR scene target (MSAA variant when available) or the backbuffer
      if (USE_FBO) {
        gl.bindFramebuffer(gl.FRAMEBUFFER, RT.msFBO || RT.sceneFBO);
        gl.viewport(0, 0, RT.w, RT.h);
      } else {
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, canvas.width, canvas.height);
      }
      gl.clearColor(0, 0, 0, 1);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
      RT.sceneDrawn = true;
      gl.enable(gl.DEPTH_TEST);
      gl.enable(gl.CULL_FACE);

      M4.persp(projMat, baseFov0 * Math.PI / 180, canvas.width / Math.max(1, canvas.height), 0.12, 260.0);

      // Intro dolly: 1.15 s swoop from behind/above the player into the eye position
      if (active && !S.wasPlaying) { S.wasPlaying = true; S.introT = 0.0; }
      if (!active) S.wasPlaying = false;
      if (S.introT < 1.15) S.introT = Math.min(1.15, S.introT + rawDt);
      const introK = S.introT < 1.15 ? Math.max(0, 1 - S.introT / 1.15) : 0;
      const introEase = introK * introK * (3 - 2 * introK);

      const bobY = active ? Math.abs(Math.sin(S.bobPhase)) * 0.055 : 0;
      const shX = (Math.random() - 0.5) * S.shake, shY = (Math.random() - 0.5) * S.shake;
      let camX = S.x + shX, camY = S.y + bobY + shY, camZ = S.z;
      let camYaw = S.yaw, camPitch = S.pitch + S.recoil * 0.55;
      if (introEase > 0.002) {
        const backX = S.x - Math.sin(S.yaw) * 6.2 * introEase;
        const backZ = S.z - Math.cos(S.yaw) * 6.2 * introEase;
        const backY = S.y + 3.4 * introEase;
        camX = backX + (S.x - backX) * (1 - introEase * 0.55);
        camY = backY + (S.y + bobY - backY) * (1 - introEase * 0.35);
        camZ = backZ + (S.z - backZ) * (1 - introEase * 0.55);
        camYaw = S.yaw;
        camPitch = S.pitch - 0.16 * introEase;
      }
      M4.viewFPS(viewMat, camX, camY, camZ, camYaw, camPitch, S.grappleOn ? 0.04 : 0);
      S.fovKick = (S.fovKick || 0) + ((S.dashT > 0 || S.grappleOn ? 1 : 0) - (S.fovKick || 0)) * Math.min(1, rawDt * 7);
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
      gl.uniform3fv(USky.sunDir, atm.sunDir);
      gl.uniform3fv(USky.sunCol, atm.sunCol);
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
      gl.uniform3f(U.uCamPos, camX, camY, camZ);
      gl.uniform3fv(U.uSunDir, atm.sunDir);
      gl.uniform3fv(U.uSunColor, atm.sunCol);
      gl.uniform3fv(U.uSkyAmb, atm.skyAmb);
      gl.uniform3fv(U.uFogColor, atm.fogCol);
      gl.uniform1f(U.uFogDensity, atm.fogD);
      gl.uniform1f(U.uShadowOn, (RT.shadowFBO && gfx.shadow) ? 1.0 : 0.0);
      if (U.uTonemap) gl.uniform1f(U.uTonemap, TONE_IN_SHADER);
      gl.uniform1f(U.uShadowTexel, RT.shadowSize ? 1.0 / RT.shadowSize : 0.0);
      gl.uniformMatrix4fv(U.uLightVP, false, lightVP);
      gl.activeTexture(gl.TEXTURE4);
      gl.bindTexture(gl.TEXTURE_2D, RT.shadowTex || whiteTex);
      gl.uniform1i(U.uShadowMap, 4);

      for (let i = 0; i < 4; i++) {
        const L = dynLights[i];
        if (L) {
          gl.uniform4f(U[`uPointPos[${i}]`], L.x, L.y, L.z, 1.0 / L.range);
          gl.uniform3f(U[`uPointCol[${i}]`], L.r, L.g, L.b);
        } else {
          gl.uniform4f(U[`uPointPos[${i}]`], 0, 0, 0, 0);
        }
      }

      // Static Arena Props (frustum-culled)
      extractFrustum(vpMat);
      let visibleProps = 0;
      for (let i = 0; i < staticProps.length; i++) {
        const p = staticProps[i];
        if (p.br && !sphereVisible(p.cx, p.cy, p.cz, p.br)) continue;
        drawMesh(p.mesh, p.mat, p.model, p.uvs, p.tint, p.emissive, p.rough, p.metal);
        visibleProps++;
      }
      S.visibleProps = visibleProps;

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

      // 3D Articulated Cyber-Chickens (6 breeds, full anatomy rigs)
      setFxCam(camX, camY, camZ, S.yaw, S.pitch);
      S.chickens.forEach(c => drawChickenRig(c, now));

      // Boss presence: pulsing ground aura + floating crown sigil
      beginFx();
      S.chickens.forEach(c => {
        if (c.type !== "boss" || c.hp <= 0) return;
        const pulse = 0.55 + Math.sin(now * 0.005) * 0.35;
        drawGroundDisc(c.x, 0.07, c.z, 4.6 + pulse * 1.4, c.enraged ? [2.6, 0.3, 0.25] : [2.2, 1.5, 0.35], 0.42 + pulse * 0.22, 2);
        drawBillboard(c.x, c.y + 4.2, c.z, 1.5 + pulse * 0.5, c.enraged ? [2.4, 0.25, 0.2] : [2.0, 1.4, 0.3], 2, 0.55);
      });
      endFx();

      // Ground FX: scorch decals, shockwave rings & pickup aura (alpha-blended, then additive)
      beginFx();
      S.decals.forEach(d => {
        const a = Math.min(1, d.ttl / Math.min(4, d.maxTtl)) * d.alpha;
        drawGroundDisc(d.x, d.y, d.z, d.r, d.col, a, 1, false);
      });
      endFx();
      beginFx();
      S.waves.forEach(wv => {
        const u = 1 - wv.ttl / wv.maxTtl;
        drawGroundDisc(wv.x, wv.y + 0.02, wv.z, wv.r0 + (wv.r1 - wv.r0) * u,
          wv.col, (1 - u) * 0.85, 2);
      });
      // Pickup aura + pulse rings
      S.pickups.forEach(pk => {
        if (pk.cd > 0) return;
        const em = pk.kind === "hp" ? [0.25,1.6,0.55] : pk.kind === "armor" ? [0.25,1.1,1.8] : [1.8,1.3,0.25];
        const pulse = 0.55 + Math.sin(now * 0.006 + pk.x) * 0.30;
        drawGroundDisc(pk.x, 0.06, pk.z, 1.5 + pulse * 0.45, em, 0.42 * pulse + 0.2, 2);
      });
      endFx();

      // Emissive 3D pickups (spinning capsule + inner core + cross icon)
      S.pickups.forEach(pk => {
        if (pk.cd > 0) return;
        const bob = 0.9 + Math.sin(now*0.004 + pk.x)*0.16;
        const em = pk.kind === "hp" ? [0.2,2.4,0.7] : pk.kind === "armor" ? [0.2,1.7,2.6] : [2.6,1.8,0.3];
        const tint = pk.kind === "hp" ? [0.55,1,0.7,1] : pk.kind === "armor" ? [0.6,0.85,1,1] : [1,0.85,0.35,1];
        M4.trs(tmpM, pk.x, bob, pk.z, now*0.0025, 0, 0, 0.72, 0.72, 0.72);
        drawMesh("sph", null, tmpM, [1,1], tint, em, 0.16, 0.85);
        M4.trs(tmpM, pk.x, bob, pk.z, -now*0.0035, 0.4, 0, 0.30, 0.30, 0.30);
        drawMesh("box", null, tmpM, [1,1], [1,1,1,1], em.map(v => v*2.4), 0.1, 0.9);
        // cage frame
        for (let k = 0; k < 6; k++) {
          const a = now * 0.0016 + k * Math.PI / 3;
          M4.trs(tmpM, pk.x + Math.cos(a)*0.72, bob + Math.sin(a*2)*0.22, pk.z + Math.sin(a)*0.72, a, 0, 0, 0.06, 0.06, 0.32);
          drawMesh("box", null, tmpM, [1,1], [1,1,1,1], em, 0.2, 0.9);
        }
      });

      // Explosive C4 Barrels: hazard drum, rings, warning label, blinking fuse LED
      S.barrels.forEach(b => {
        if (!b.alive) return;
        M4.trs(tmpM, b.x, 0.85, b.z, 0, 0, 0, 1.15, 1.7, 1.15);
        drawMesh("cyl", MATS.hazard, tmpM, [1,1], [1,1,1,1], [0.35, 0.04, 0.02], 0.55, 0.65);
        for (const yy of [0.36, 1.36]) {
          M4.trs(tmpM, b.x, yy, b.z, 0, 0, 0, 1.19, 0.10, 1.19);
          drawMesh("cyl", MATS.metal, tmpM, [1,1], [0.7,0.72,0.76,1], [0,0,0], 0.3, 0.9);
        }
        M4.trs(tmpM, b.x, 0.95, b.z, 0, 0, 0, 1.17, 0.42, 1.17);
        drawMesh("cyl", null, tmpM, [1,1], [1,0.72,0.2,1], [0.5,0.28,0.04], 0.4, 0.4);
        const blink = (Math.sin(now * 0.012) > 0.2) ? 3.0 : 0.25;
        M4.trs(tmpM, b.x, 1.76, b.z, 0, 0, 0, 0.22, 0.16, 0.22);
        drawMesh("box", null, tmpM, [1,1], [1,0.2,0.15,1], [blink, 0.15, 0.1], 0.15, 0.7);
      });

      // Ally Cyber-Turrets: base, ring, rotating head, twin barrels, ammo box
      S.turrets.forEach(t => {
        M4.trs(tmpM, t.x, 0.34, t.z, 0, 0, 0, 1.1, 0.68, 1.1);
        drawMesh("cyl", MATS.metal, tmpM, [2,2], [0.5,0.6,0.75,1], [0,0,0], 0.3, 0.9);
        M4.trs(tmpM, t.x, 0.74, t.z, t.yaw, 0, 0, 0.85, 0.16, 0.85);
        drawMesh("cyl", null, tmpM, [1,1], [0.3,0.95,1,1], [0.2,1.4,1.8], 0.2, 0.85);
        M4.trs(tmpM, t.x, 1.14, t.z, t.yaw, 0, 0, 0.62, 0.46, 0.9);
        drawMesh("box", MATS.panel, tmpM, [1,1], [0.3,0.85,1,1], [0.1,0.8,1.1], 0.25, 0.88);
        for (const side of [-1, 1]) {
          M4.trs(tmpM, t.x + Math.cos(t.yaw)*side*0.20, 1.16, t.z - Math.sin(t.yaw)*side*0.20, t.yaw, Math.PI*0.5, 0, 0.09, 0.78, 0.09);
          drawMesh("cyl", MATS.metal, tmpM, [1,1], [0.2,0.24,0.3,1], [0,0,0], 0.22, 0.95);
        }
        M4.trs(tmpM, t.x + Math.cos(t.yaw)*0.02, 1.42, t.z - Math.sin(t.yaw)*0.02, t.yaw, 0, 0, 0.16, 0.10, 0.16);
        drawMesh("sph", null, tmpM, [1,1], [1,0.3,0.3,1], [2.2,0.3,0.3], 0.15, 0.6);
      });

      // Environment props: jump-pad towers, spires (procedural, animated)
      S.pads.forEach((p, idx) => {
        const pulse = 1.6 + Math.sin(now*0.006 + idx)*0.45;
        for (let k = 0; k < 4; k++) {
          const a = now * 0.0022 + k * Math.PI * 0.5;
          M4.trs(tmpM, p.x + Math.cos(a)*1.55, 0.55, p.z + Math.sin(a)*1.55, a, 0, 0, 0.10, 1.0, 0.10);
          drawMesh("box", null, tmpM, [1,1], [0.6,1,1,1], [0.2*pulse, 1.0*pulse, 1.3*pulse], 0.2, 0.9);
        }
      });

      // Plasma Orbs, Enemy Eggs, Grenades & Particles (all 3D now)
      S.orbs.forEach(o => {
        M4.trs(tmpM, o.x, o.y, o.z, now*0.01, 0, 0, 0.42, 0.42, 0.42);
        drawMesh("sph", null, tmpM, [1,1], [0.2,1,0.6,1], [0.4, 3.0, 1.5], 0.1, 0.5);
        M4.trs(tmpM, o.x, o.y, o.z, -now*0.014, 0, 0, 0.62, 0.62, 0.62);
        drawMesh("sph", null, tmpM, [1,1], [0.5,1,0.8,1], [0.2, 1.2, 0.6], 0.1, 0.3);
      });
      S.enemyEggs.forEach(eg => {
        M4.trs(tmpM, eg.x, eg.y, eg.z, now*0.01, 0.3, 0, 0.30, 0.40, 0.30);
        drawMesh("sph", null, tmpM, [1,1], [1,0.9,0.85,1], [1.6, 0.35, 0.4], 0.25, 0.25);
        M4.trs(tmpM, eg.x, eg.y, eg.z, 0, 0, 0, 0.46, 0.46, 0.46);
        drawMesh("sph", null, tmpM, [1,1], [1,0.3,0.4,1], [0.8, 0.12, 0.18], 0.2, 0.3);
      });
      S.grenades.forEach(g => {
        M4.trs(tmpM, g.x, g.y, g.z, now*0.01, now*0.01, 0, 0.30, 0.38, 0.30);
        drawMesh("sph", MATS.metal, tmpM, [1,1], [0.35,0.4,0.3,1], [0,0,0], 0.4, 0.55);
        M4.trs(tmpM, g.x, g.y, g.z, 0, 0, 0, 0.42, 0.20, 0.42);
        drawMesh("cyl", null, tmpM, [1,1], [1,0.85,0.2,1], [2.4, 1.6, 0.2], 0.2, 0.8);
      });

      // Particles: glowing additive FX sprites instead of cubes
      beginFx();
      S.particles.forEach(p => {
        const fade = Math.min(1, p.ttl * 2.2);
        drawBillboard(p.x, p.y, p.z, Math.max(0.06, p.sz * 3.2), p.col, p.mode === 1 ? 1 : 0, fade * (p.alpha || 1));
      });
      // Muzzle light halo + explosion core sprites
      dynLights.forEach(L => {
        const k = Math.max(0, Math.min(1, L.ttl / 0.35));
        drawBillboard(L.x, L.y, L.z, 1.6 + (1 - k) * 2.6, [L.r * 0.55, L.g * 0.55, L.b * 0.55], 2, k * 0.7);
      });
      endFx();

      // 3D Weather Motes matching [F8] Atmosphere (now glowing volumetric flakes)
      const wEm = S.atmIdx === 1 ? [0.2,1.6,2.2] : S.atmIdx === 2 ? [2.2,0.35,0.3] : S.atmIdx === 3 ? [1.6,1.8,2.1] : [2.2,1.5,0.4];
      S.weather.forEach(m => {
        m.oy += (S.atmIdx === 2 ? 1 : -1) * m.sp * (S.atmIdx === 1 ? 3.2 : 1.0) * rawDt;
        if (m.oy < -2) m.oy = 12; if (m.oy > 12) m.oy = -2;
        M4.trs(tmpM, camX + m.ox, camY + m.oy - 4, camZ + m.oz, 0, 0, 0,
              0.035, S.atmIdx === 1 ? 0.55 : 0.075, 0.035);
        drawMesh("box", null, tmpM, [1,1], [1,1,1,1], wEm, 0.2, 0.2);
      });

      // Weapon Tracers, Railgun Pierce Beams, Grapple Tether & Orbital Ion Strike
      S.beams.forEach(b => {
        M4.beamBetween(tmpM, b.x1, b.y1, b.z1, b.x2, b.y2, b.z2, b.th);
        drawMesh("cyl", null, tmpM, [1,1], [1,1,1,1], b.col, 0.1, 0.8);
        // hot core through the tracer
        M4.beamBetween(tmpM, b.x1, b.y1, b.z1, b.x2, b.y2, b.z2, b.th * 0.34);
        drawMesh("cyl", null, tmpM, [1,1], [1,1,1,1], b.col.map(v => v * 2.1), 0.05, 0.9);
      });
      if (S.grappleOn) {
        const hx = camX + Math.cos(S.yaw)*0.32, hy = camY - 0.24, hz = camZ - Math.sin(S.yaw)*0.32;
        M4.beamBetween(tmpM, hx, hy, hz, S.grapplePos[0], S.grapplePos[1], S.grapplePos[2], 0.062);
        drawMesh("cyl", null, tmpM, [1,1], [0.2,1,1,1], [0.4, 2.8, 3.2], 0.1, 0.9);
        // hook head
        M4.trs(tmpM, S.grapplePos[0], S.grapplePos[1], S.grapplePos[2], now*0.004, now*0.003, 0, 0.34, 0.34, 0.34);
        drawMesh("cone", MATS.metal, tmpM, [1,1], [0.8,0.9,1,1], [0.6,1.6,2.0], 0.2, 0.9);
      }
      // [L] 3D laser sight — beam from the muzzle plus a glowing dot on the world hit point
      if (S.laserOn && active) {
        const lw = WEAPONS[S.wep];
        if (!lw.explosive) {
          const [ldx, ldy, ldz] = getAimDir(0);
          const hitT = raycastWorld(S.x, camY, S.z, ldx, ldy, ldz, 90);
          const lt = hitT > 0 ? hitT : 90;
          const sx2 = camX + ldx * lt, sy2 = camY + ldy * lt, sz2 = camZ + ldz * lt;
          const mx2 = camX + Math.cos(S.yaw) * 0.28 + ldx * 0.9;
          const my2 = camY - 0.18 + ldy * 0.9;
          const mz2 = camZ - Math.sin(S.yaw) * 0.28 + ldz * 0.9;
          const lcol = lw.pierce ? [0.5, 2.6, 3.2] : lw.plasma ? [0.4, 2.6, 1.2] : [2.6, 0.35, 0.4];
          M4.beamBetween(tmpM, mx2, my2, mz2, sx2, sy2, sz2, 0.0075);
          drawMesh("cyl", null, tmpM, [1,1], [1,1,1,1], lcol.map(v => v * 0.55), 0.1, 0.8);
          beginFx();
          drawBillboard(sx2, sy2, sz2, 0.13, lcol, 2, 0.95);
          drawBillboard(sx2, sy2, sz2, 0.42, lcol.map(v => v * 0.45), 2, 0.3);
          endFx();
        }
      }
      S.orbitalStrikes.forEach(o => {
        const u = 1 - o.ttl / o.maxTtl;
        const w = Math.max(0.1, 3.2 * (1 - u * u));
        M4.trs(tmpM, o.x, 30, o.z, 0, 0, 0, w, 60, w);
        drawMesh("cyl", null, tmpM, [1,1], [0.4,1,1,1], [1.2, 3.6, 4.2], 0.1, 0.9);
        M4.trs(tmpM, o.x, 30, o.z, 0, 0, 0, w * 0.34, 60, w * 0.34);
        drawMesh("cyl", null, tmpM, [1,1], [1,1,1,1], [3.0, 4.2, 4.6], 0.05, 0.9);
        const r = 2 + u * 32;
        M4.trs(tmpM, o.x, 0.18, o.z, 0, 0, 0, r, 0.12, r);
        drawMesh("cyl", null, tmpM, [1,1], [1,0.85,0.2,1], [3.2*(1-u), 2.2*(1-u), 0.4], 0.1, 0.9);
        M4.trs(tmpM, o.x, 0.30, o.z, 0, 0, 0, r * 1.5, 0.08, r * 1.5);
        drawMesh("cyl", null, tmpM, [1,1], [0.6,1,1,1], [1.4*(1-u), 2.6*(1-u), 3.0*(1-u)], 0.1, 0.9);
      });

      // ---- Debris & gore layer: shell casings, fluttering feathers, tumbling ragdolls ----
      S.shells.forEach(sh => {
        const fade = Math.min(1, sh.ttl * 1.6);
        M4.trs(tmpM, sh.x, sh.y, sh.z, sh.ry, sh.rx, sh.rz, 0.038, 0.038, 0.085);
        drawMesh("cyl", null, tmpM, [1,1],
          sh.energy ? [0.5,1.4,1.2,1] : [1.0,0.78,0.30,1],
          sh.energy ? [0.5*fade, 2.2*fade, 1.6*fade] : [0.55*fade, 0.34*fade, 0.05*fade], 0.25, 0.9);
      });
      S.feathers.forEach(f => {
        const fade = Math.min(1, f.ttl * 1.4);
        M4.trs(tmpM, f.x, f.y, f.z, f.ry, f.rx, f.rz, f.sz * 1.5, f.sz * 0.22, f.sz * 3.4);
        drawMesh("box", MATS.feather, tmpM, [1,1],
          [Math.min(1, f.col[0] + fade * 0.15), Math.min(1, f.col[1] + fade * 0.15), Math.min(1, f.col[2] + fade * 0.15), 1],
          [0,0,0], 0.62, 0.05);
      });
      S.corpses.forEach(cp => {
        const life = cp.ttl / cp.maxTtl;
        const fade = life > 0.35 ? 1 : Math.max(0, life / 0.35);
        const st = BREED_STYLE[cp.type] || BREED_STYLE.melee;
        const tint = [st.tint[0] * (0.4 + fade * 0.6), st.tint[1] * (0.4 + fade * 0.6), st.tint[2] * (0.4 + fade * 0.6), 1];
        const em = [1.2 * fade * fade, 0.18 * fade * fade, 0.05 * fade * fade];
        const sc = cp.sc;
        resetT();
        pushT(cp.x, Math.max(0.3, cp.y + 0.85 * sc), cp.z, cp.yaw + cp.spin, cp.tilt, cp.tilt * 0.5, sc, sc, sc);
        // body
        pushT(0, 0, 0.02, 0, 0, 0, 0.95, 0.88, 1.14);
        drawMesh("sph", MATS.feather, curT(), [2,2], tint, em, 0.68, 0.06); popT();
        // head + comb flopped over
        pushT(0.10, 0.60, 0.42, 0.5, 0.8, 0, 0.54, 0.52, 0.56);
        drawMesh("sph", MATS.feather, curT(), [1,1], tint, em, 0.62, 0.06); popT();
        pushT(0.10, 0.78, 0.44, 0.5, 0.8, 0, 0.14, 0.24, 0.30);
        drawMesh("box", null, curT(), [1,1], st.comb, em, 0.3, 0.1); popT();
        // splayed legs
        for (const side of [-1, 1]) {
          pushT(side * 0.26, -0.42, 0.05, 0, side * 0.9, side * 0.3, 0.085, 0.55, 0.085);
          drawMesh("cyl", null, curT(), [1,1], st.leg, [0,0,0], 0.55, 0.4); popT();
          pushT(side * 0.30, -0.66, 0.30, 0, side * 1.35, side * 0.3, 0.10, 0.14, 0.16);
          drawMesh("box", null, curT(), [1,1], st.leg, [0,0,0], 0.5, 0.25); popT();
        }
        // tail fan
        for (let i = 0; i < 4; i++) {
          const a = (i - 1.5) * 0.34;
          pushT(Math.sin(a) * 0.10, 0.22, -0.62, a, -0.7, a * 0.3, 0.09, 0.035, 0.40);
          drawMesh("box", MATS.feather, curT(), [1,1], tint, em, 0.62, 0.06); popT();
        }
        popT();
        resetT();
      });

      // 3) 3D First-Person Viewmodel Pass (9 distinct weapon rigs + chicken claw + muzzle flash)
      if (active) {
        gl.clear(gl.DEPTH_BUFFER_BIT);
        gl.uniformMatrix4fv(U.uVP, false, projMat);
        gl.uniform3f(U.uCamPos, 0, 0, 0);
        gl.uniform3f(U.uSunDir, 0.35, 0.72, 0.55);
        gl.uniform3f(U.uSunColor, atm.sunCol);
        gl.uniform3fv(U.uSkyAmb, atm.skyAmb);
        gl.uniform1f(U.uShadowOn, 0.0);
        if (U.uTonemap) gl.uniform1f(U.uTonemap, TONE_IN_SHADER);

        const w = WEAPONS[S.wep];
        const sk = SKINS[S.skin];
        const bobX = Math.cos(S.bobPhase) * 0.020, bobV = Math.abs(Math.sin(S.bobPhase * 2)) * 0.022;
        const adsEase = (S.adsBlend = (S.adsBlend || 0) + ((S.aiming ? 1 : 0) - (S.adsBlend || 0)) * Math.min(1, rawDt * 12));
        const sprintTilt = ((keys["ShiftLeft"] && !S.aiming && S.onGround) ? 1 : 0);
        S.sprintBlend = (S.sprintBlend || 0) + (sprintTilt - (S.sprintBlend || 0)) * Math.min(1, rawDt * 6);
        const inspYaw = S.inspectT > 0 ? Math.sin((2.0 - S.inspectT) * Math.PI) * 0.65 : 0;
        const slapRoll = S.meleeT > 0 ? Math.sin((1 - S.meleeT / 0.28) * Math.PI) * 1.1 : 0;
        const sway = Math.sin(now * 0.0013) * 0.010 * (1 - adsEase * 0.7);
        const wx = (adsEase * 0.0 + (1 - adsEase) * 0.255) + bobX + sway;
        const wy = (adsEase * -0.135 + (1 - adsEase) * -0.235) - bobV - S.reloadT * 0.06 - S.sprintBlend * 0.09;
        const wz = -0.50 + S.recoil * 0.30 + (1 - adsEase) * 0.02;
        const wYaw = inspYaw + (1 - adsEase) * 0.055 + S.sprintBlend * 0.30;
        const wPitch = S.recoil * 0.55 - adsEase * 0.02 + S.sprintBlend * -0.18;
        const wRoll = slapRoll + S.sprintBlend * -0.34 + sway * 2.2;

        const muzzleFlash = S.muzzleFlash || 0;
        const rigScale = Math.min(1, Math.max(0, (1.15 - S.introT) / 0.55));
        if (rigScale > 0.02) {
          drawWeaponRig(w, sk, wx, wy, wz, wYaw, wPitch, wRoll, adsEase,
                        S.ammo[S.wep] / w.mag, muzzleFlash, rigScale);
        }

        // Support claw wing (visible in hip-fire, folded away when aiming)
        if (adsEase < 0.92 && rigScale > 0.3) {
          resetT();
          drawChickenWing(wx - 0.13, wy - 0.10, wz + 0.10, 0.30 + inspYaw, 0.12, -0.42 + slapRoll,
                          1, Math.sin(S.bobPhase * 0.5) * 0.12);
          resetT();
        }

      }

      // ===== POST-PROCESS: resolve MSAA, bloom chain, ACES + CAS + combat FX =====
      if (USE_FBO && RT.msFBO) {
        gl.bindFramebuffer(gl.READ_FRAMEBUFFER, RT.msFBO);
        gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, RT.sceneFBO);
        gl.blitFramebuffer(0, 0, RT.w, RT.h, 0, 0, RT.w, RT.h, gl.COLOR_BUFFER_BIT, gl.NEAREST);
      }
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.disable(gl.DEPTH_TEST);
      gl.disable(gl.CULL_FACE);
      gl.disable(gl.BLEND);

      if (gfx.bloom && RT.brightFBO && RT.blurAFBO && RT.blurBFBO) {
        // bright pass -> 1/2
        gl.bindFramebuffer(gl.FRAMEBUFFER, RT.brightFBO);
        gl.viewport(0, 0, RT.bw, RT.bh);
        gl.useProgram(brightProg);
        gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, RT.scene);
        gl.uniform1i(UBr.tex, 0);
        gl.uniform2f(UBr.texel, 1 / RT.w, 1 / RT.h);
        gl.uniform1f(UBr.thr, 0.72);
        gl.bindVertexArray(meshes.quad.vao);
        gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
        // downsample bright -> 1/4 (blurA) using blur prog (horizontal)
        gl.bindFramebuffer(gl.FRAMEBUFFER, RT.blurAFBO);
        gl.viewport(0, 0, Math.max(2, Math.floor(RT.w/4)), Math.max(2, Math.floor(RT.h/4)));
        gl.useProgram(blurProg);
        gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, RT.brightTex);
        gl.uniform1i(UBl.tex, 0);
        gl.uniform2f(UBl.dir, 1 / RT.w, 0);
        gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
        // vertical
        gl.bindFramebuffer(gl.FRAMEBUFFER, RT.blurBFBO);
        gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, RT.blurA);
        gl.uniform2f(UBl.dir, 0, 1 / RT.h);
        gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
        // second, wider horizontal pass back into blurA
        gl.bindFramebuffer(gl.FRAMEBUFFER, RT.blurAFBO);
        gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, RT.blurB);
        gl.uniform2f(UBl.dir, 2.2 / RT.w, 0);
        gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
      }

      if (!USE_FBO) {
        // Direct-to-backbuffer fallback: the scene is already on screen
        gl.enable(gl.DEPTH_TEST);
        gl.enable(gl.CULL_FACE);
        gl.bindVertexArray(null);
        S.drawCalls = S.drawCalls || 0;
        S.fovNow = baseFov0;
        S.gfxName = GFX_PRESETS[gfxIdx].name + " · COMPAT";
        opts.syncHud(S, WEAPONS[S.wep], SKINS[S.skin], ATMOSPHERES[S.atmIdx], MUTATORS[S.mutator], vpMat);
        return true;
      }

      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.useProgram(postProg);
      gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, RT.scene);
      gl.uniform1i(UP.uScene, 0);
      const bloomOn = !!(gfx.bloom && RT.blurAFBO && RT.blurBFBO);
      gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, bloomOn ? RT.blurA : RT.scene);
      gl.uniform1i(UP.uBloom, 1);
      gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, bloomOn ? RT.blurB : RT.scene);
      gl.uniform1i(UP.uBloom2, 2);
      gl.uniform1f(UP.uBloomStr, bloomOn ? 0.85 : 0.0);
      gl.uniform2f(UP.uResolution, RT.w, RT.h);
      gl.uniform1f(UP.uTime, now * 0.001);
      gl.uniform1f(UP.uHurt, Math.min(1, (S.hurtFx || 0) * 0.9 + (S.hp < S.maxHp * 0.3 ? 0.18 : 0)));
      gl.uniform1f(UP.uOverdrive, S.overdriveT > 0 ? Math.min(1, S.overdriveT / 10) : 0);
      gl.uniform1f(UP.uSlowMo, S.slowMo > 0 ? 0.9 : 0);
      gl.uniform1f(UP.uVignette, S.slowMo > 0 ? 0.55 : 0);
      gl.uniform1f(UP.uSaturation, gfx.sat);
      gl.uniform1f(UP.uExposure, 1.0);
      gl.uniform1f(UP.uSharp, gfx.sharp);
      gl.uniform1f(UP.uDash, (S.dashT > 0 ? 1 : 0) * 0.75 + S.sprintBlend * 0.18);
      gl.uniform1f(UP.uGrapple, S.grappleOn ? 1 : 0);
      gl.bindVertexArray(meshes.quad.vao);
      gl.drawElements(gl.TRIANGLES, meshes.quad.count, gl.UNSIGNED_SHORT, 0);
      gl.enable(gl.DEPTH_TEST);
      gl.enable(gl.CULL_FACE);
      gl.bindVertexArray(null);

      S.drawCalls = S.drawCalls || 0;
      S.fovNow = baseFov0;
      S.gfxName = GFX_PRESETS[gfxIdx].name;
      opts.syncHud(S, WEAPONS[S.wep], SKINS[S.skin], ATMOSPHERES[S.atmIdx], MUTATORS[S.mutator], vpMat);
    }

    requestAnimationFrame(tick);
    return {
      state: S,
      saveCredits,
      requestLockSmart,
      setSensitivity,
      cycleAtmosphere() { S.atmIdx = (S.atmIdx + 1) % ATMOSPHERES.length; },
      get preset() { return GFX_PRESETS[gfxIdx].name; },
      get presetIndex() { return gfxIdx; },
      cycleGfx() {
        applyGfx((gfxIdx + 1) % GFX_PRESETS.length);
        const p = GFX_PRESETS[gfxIdx];
        opts.pushFeed?.(`🎛 ГРАФИКА [F4]: ${p.name} — тени ${p.shadow ? p.shadow + "²" : "ВЫКЛ"}, MSAA x${p.samples}, Bloom ${p.bloom ? "ON" : "OFF"}`);
        opts.showToast?.(`🎛 Пресет графики: ${p.name}`);
        return p.name;
      },
      setGfx(i) { applyGfx(i); return GFX_PRESETS[gfxIdx].name; },
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

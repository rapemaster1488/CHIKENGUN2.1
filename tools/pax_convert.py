#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pax_convert.py — мини-конвертер Urho3D ParticleEffect XML -> бинарный .pfx.

Формат .pfx (Urho3D, сериализация ParticleEffect::Save):
  String materialName            (shortString)
  unsigned numParticles          (LE32)
  bool   isRelative / isScaled / isSorted / updateInvisible (4 байта bool)
  float  animationLodBias        (LE32)
  int    emitterType             (LE32: Point=0 Box=1 Sphere=2 Ring=3 Ellipse=4)
  Vector3 emitterSize            (3x LE32)
  Vector3 directionMin/Max       (по 3x LE32)
  Vector3 constantForce          (3x LE32)
  float  dampingForce
  float  activeTime, inActiveTime
  float  emissionRateMin/Max
  float  particleSizeMin/Max
  float  timeToLiveMin/Max
  float  velocityMin/Max
  float  rotationMin/Max
  float  rotationSpeedMin/Max
  Vector3 sizeAddMin/Max         (в XML это <sizedelta add=...>)
  float  sizeMultiplyMin/Max
  unsigned numColorStops
  для каждого стопа: Color(4x byte RGBA) + float time
  unsigned numTextureStops
  для каждого стопа: float xOff, yOff (Vector2) + float time
  short  textureScrollRevX/Y     (i16)
  short  textureScaleRevX/Y      (i16)
  float  scaleXRate/scaleYRate
  float  translateX/translateY
  Vector3 gravityVelocity        (3x LE32)
  Vector3 radialVelocity         (3x LE32)
  Vector3 rotAxis                (3x LE32)
  Vector3 rotAxisVel             (3x LE32)
  Vector3 rotCenter              (3x LE32)
  float  inertiaMax
  float  friction
  bool   resetOnActivate         (1 байт)
  float  weight                  (для LOD; пишем 1.0)

Использование: pax_convert.py input.xml output.pfx
"""
import struct, sys, re
import xml.etree.ElementTree as ET

EMITTER_TYPES = {"Point": 0, "Box": 1, "Sphere": 2, "Ring": 3, "Ellipse": 4}

def v3(text, default=(0.0, 0.0, 0.0)):
    if text is None:
        return default
    parts = [float(x) for x in str(text).split()]
    if len(parts) == 1:
        return (parts[0], parts[0], parts[0])
    while len(parts) < 3:
        parts.append(0.0)
    return tuple(parts[:3])

def main():
    if len(sys.argv) != 3:
        print("usage: pax_convert.py input.xml output.pfx", file=sys.stderr)
        sys.exit(2)
    src, dst = sys.argv[1], sys.argv[2]
    root = ET.parse(src).getroot()

    def get(tag):
        return root.find(tag)

    out = bytearray()

    def short_str(s):
        b = s.encode("utf-8")
        out.append(len(b))
        out.extend(b)

    def f32(x):
        out.extend(struct.pack("<f", float(x)))

    def u32(x):
        out.extend(struct.pack("<I", int(x)))

    def i32(x):
        out.extend(struct.pack("<i", int(x)))

    def i16(x):
        out.extend(struct.pack("<h", int(x)))

    def boolean(x):
        out.append(1 if x else 0)

    def vec3(t):
        for c in t:
            f32(c)

    # material
    m = get("material")
    mat_name = m.get("name") if m is not None else "Materials/Particle.xml"
    short_str(mat_name or "Materials/Particle.xml")

    def val(tag, dflt=None, node=None):
        e = (node or root).find(tag)
        if e is None:
            return dflt
        return e.get("value", e.text)

    def mn(tag):
        e = get(tag)
        return (e.get("min") if e is not None else None), (e.get("max") if e is not None else None)

    u32(int(val("numparticles", 100)))
    boolean(val("relative", "false") == "true")
    boolean(val("scaled", "true") == "true")
    boolean(val("sorted", "false") == "true")
    boolean(val("updateinvisible", "false") == "true")
    f32(float(val("animlodbias", 0)))

    et = (val("emittertype", "Sphere") or "Sphere").strip().capitalize()
    i32(EMITTER_TYPES.get(et, 2))
    vec3(v3(val("emittersize", "0 0 0")))

    lo, hi = mn("direction")
    vec3(v3(lo, (-0.1, 0.1, -0.1)))
    vec3(v3(hi, (0.1, 1.0, 0.1)))
    vec3(v3(val("constantforce", "0 0 0")))
    f32(float(val("dampingforce", 0)))
    f32(float(val("activetime", 0)))
    f32(float(val("inactivetime", 0)))

    def pair(x, dflt):
        # min/max могут быть векторами ("0.1 0.1") — берём первую компоненту
        if x is None:
            return float(dflt)
        return float(str(x).split()[0])

    lo, hi = mn("emissionrate")
    f32(pair(lo, 10)); f32(pair(hi, 10))
    lo, hi = mn("particlesize")
    f32(pair(lo, 1)); f32(pair(hi, 1))
    lo, hi = mn("timetolive")
    f32(pair(lo, 1)); f32(pair(hi, 1))
    lo, hi = mn("velocity")
    f32(pair(lo, 0)); f32(pair(hi, 0))
    lo, hi = mn("rotation")
    f32(pair(lo, 0)); f32(pair(hi, 0))
    lo, hi = mn("rotationspeed")
    f32(pair(lo, 0)); f32(pair(hi, 0))

    sd = get("sizedelta")
    add = v3(sd.get("add") if sd is not None else None, (0, 0, 0))
    mul = sd.get("mul") if sd is not None else None
    mulv = v3(mul, (1, 1, 1))
    vec3(add)  # sizeAddMin
    vec3(add)  # sizeAddMax
    f32(mulv[0]); f32(mulv[0])  # sizeMultiplyMin (x,y)
    f32(mulv[0]); f32(mulv[0])  # sizeMultiplyMax (x,y)

    stops = root.findall("colorfade")
    u32(len(stops))
    for s in stops:
        col = [float(x) for x in s.get("color", "1 1 1 1").split()]
        while len(col) < 4:
            col.append(1.0)
        for c in col[:4]:
            out.append(max(0, min(255, int(round(c * 255)))))
        f32(float(s.get("time", 0)))

    tstops = root.findall("texturestroke")
    u32(len(tstops))
    for s in tstops:
        off = [float(x) for x in s.get("offset", "0 0").split()]
        f32(off[0]); f32(off[1] if len(off) > 1 else 0.0)
        f32(float(s.get("time", 0)))

    i16(0); i16(0)  # textureScrollRevX/Y
    i16(0); i16(0)  # textureScaleRevX/Y
    f32(0); f32(0)  # scaleXRate/scaleYRate
    f32(0); f32(0)  # translateX/Y
    vec3((0, 0, 0))  # gravityVelocity
    vec3((0, 0, 0))  # radialVelocity
    vec3((0, 1, 0))  # rotAxis
    vec3((0, 0, 0))  # rotAxisVel
    vec3((0, 0, 0))  # rotCenter
    f32(0)           # inertiaMax
    f32(0)           # friction
    boolean(False)   # resetOnActivate
    f32(1.0)         # weight

    with open(dst, "wb") as fh:
        fh.write(out)
    print(f"OK: {src} -> {dst} ({len(out)} bytes)")

if __name__ == "__main__":
    main()

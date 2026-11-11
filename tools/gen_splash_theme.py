#!/usr/bin/env python3
# ============================================================================
# CHIKENGUN 2.5 «ПЕРО СУДЬБЫ» — синтезатор эпичного интро-трека «koch.studios»
# Генерирует Data/Music/KochSplashTheme.wav (PCM 16-bit) — звучит на загрузочном
# экране во время заставки  koch.studios → powered by fall.os → CHICHKENCUM 2.1
#   D minor, 140 BPM • riser → pad/bells (fall.os) → SLAM → грув → финал
# Запуск: python3 tools/gen_splash_theme.py
# Требует: numpy, soundfile (pip install numpy soundfile)
# ============================================================================
import numpy as np
import soundfile as sf
import os

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "Data", "Music", "KochSplashTheme.wav")

rng = np.random.default_rng(20250611)
t_global = 0.0

def T():
    global t_global
    return t_global

def t_arr(dur):
    return np.arange(int(dur * SR), dtype=np.float32) / SR

def sine(freq, dur, amp=1.0, phase=0.0):
    t = t_arr(dur)
    return (amp * np.sin(2 * np.pi * freq * t + phase)).astype(np.float32)

def sweep_sine(f0, f1, dur, amp=1.0):
    t = t_arr(dur)
    freq = f0 * (f1 / f0) ** t
    phase = 2 * np.pi * f0 * (np.log(f1 / f0) ** -1) * ((f1 / f0) ** t - 1) if f1 != f0 else 2 * np.pi * f0 * t
    return (amp * np.sin(phase)).astype(np.float32)

def saw(freq, dur, amp=1.0, detune=0.0):
    """Антиелая пила через additive-синтез с лёгким детюном (max ~24 гармоники)."""
    t = t_arr(dur)
    out = np.zeros_like(t)
    base_f = freq * (1.0 + detune)
    for k in range(1, 25):
        fk = base_f * k
        if fk > SR * 0.45:
            break
        out += (1.0 / k) * np.sin(2 * np.pi * fk * t)
    return (amp * out / 1.8).astype(np.float32)

def square(freq, dur, amp=1.0):
    t = t_arr(dur)
    return (amp * np.sign(np.sin(2 * np.pi * freq * t))).astype(np.float32)

def noise(dur, amp=1.0):
    return (amp * rng.standard_normal(int(dur * SR))).astype(np.float32)

def lowpass_1pole(x, cutoff):
    alpha = min(1.0, 2 * np.pi * cutoff / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc += alpha * (v - acc)
        y[i] = acc
    return y

def bandpass(x, lo, hi):
    return lowpass_1pole(x, hi) - lowpass_1pole(x, lo)

def adsr(n, attack, decay, sustain, release, sustain_level=0.7):
    env = np.ones(n, dtype=np.float32)
    a = min(n, int(attack * SR)); d = min(n - a, int(decay * SR)); r = min(n, int(release * SR))
    if a > 0: env[:a] = np.linspace(0.0, 1.0, a, dtype=np.float32)
    if d > 0: env[a:a + d] = np.linspace(1.0, sustain_level, d, dtype=np.float32)
    sus_end = max(a + d, n - r)
    env[a + d:sus_end] = sustain_level
    if r > 0: env[n - r:] = sustain_level * np.linspace(1.0, 0.0, r, dtype=np.float32)
    return env

def place(dst, src, at_sec, gain=1.0):
    i = int(at_sec * SR)
    j = min(len(dst), i + len(src))
    if i < len(dst):
        dst[i:j] += src[:j - i] * gain

def kick(dur=0.55, f0=150.0, f1=47.0, amp=1.0):
    body = sweep_sine(f0, f1, dur, amp) * np.exp(-t_arr(dur) * 5.5)
    click = noise(0.012, 0.5) * np.exp(-t_arr(0.012) * 200)
    out = body
    out[:len(click)] += click
    return out

def thump(dur=0.30, amp=0.9):
    return sweep_sine(95.0, 38.0, dur, amp) * np.exp(-t_arr(dur) * 9.0)

def taiko(dur=0.9, amp=0.95):
    lo = sine(82.4, dur, amp * 0.8) * np.exp(-t_arr(dur) * 4.2)
    mid = sine(164.8, dur * 0.7, amp * 0.45) * np.exp(-t_arr(dur * 0.7) * 7.0)
    slap = bandpass(noise(0.06, 0.5), 900, 4200) * np.exp(-t_arr(0.06) * 60)
    out = lo
    out[:len(mid)] += mid
    out[:len(slap)] += slap
    return out

def brass(freqs, dur, amp=0.55, cutoff=1600.0):
    out = np.zeros(int(dur * SR), dtype=np.float32)
    for f in freqs:
        out += saw(f, dur, amp / len(freqs), detune=0.004)
        out += saw(f, dur, amp / len(freqs), detune=-0.005)
    out = lowpass_1pole(out, cutoff)
    return out * adsr(len(out), 0.02, 0.09, 0.8, 0.08, 0.62)

def pad(freqs, dur, amp=0.5):
    out = np.zeros(int(dur * SR), dtype=np.float32)
    for f in freqs:
        for det in (-0.006, 0.0, 0.006):
            out += saw(f, dur, amp / (len(freqs) * 3), detune=det)
    out = lowpass_1pole(out, 950.0)
    env = np.ones(len(out), dtype=np.float32)
    a = min(len(out), int(0.9 * SR)); r = min(len(out), int(1.1 * SR))
    env[:a] = np.linspace(0.0, 1.0, a, dtype=np.float32)
    env[-r:] = np.linspace(1.0, 0.0, r, dtype=np.float32)
    lfo = 0.9 + 0.1 * np.sin(2 * np.pi * 0.7 * t_arr(dur))
    return out * env * lfo.astype(np.float32)

def bell(freq, dur=2.6, amp=0.35):
    t = t_arr(dur)
    partials = [(1.0, 1.0), (2.76, 0.35), (5.4, 0.12)]
    out = np.zeros_like(t)
    for m, a in partials:
        out += a * np.sin(2 * np.pi * freq * m * t) * np.exp(-t * (2.1 if m == 1.0 else 3.2))
    return (amp * out).astype(np.float32)

def lead(freq, dur, amp=0.30):
    t = t_arr(dur)
    vib = 1.0 + 0.006 * np.sin(2 * np.pi * 5.5 * t)
    f_inst = freq * vib
    phase = 2 * np.pi * np.cumsum(f_inst) / SR
    out = np.sin(phase)
    out += 0.35 * np.sin(2 * phase) + 0.12 * np.sin(3 * phase)
    out = lowpass_1pole(out.astype(np.float32), 2600.0)
    return (amp * out * adsr(len(out), 0.03, 0.08, 0.85, 0.10, 0.7)).astype(np.float32)

def hat(dur=0.05, amp=0.16, open_=False):
    if open_:
        dur = 0.24
        amp *= 0.75
    hp = bandpass(noise(dur, amp), 5500, 16000)
    return (hp * np.exp(-t_arr(dur) * (26 if open_ else 90))).astype(np.float32)

def riser(dur, f_lo=380.0, f_hi=12500.0, amp=0.30):
    t = t_arr(dur)
    n = noise(dur, 1.0)
    y = np.empty_like(n)
    cutoff = f_lo * (f_hi / f_lo) ** (t / t[-1])
    alpha = 2 * np.pi * cutoff / SR
    acc = 0.0
    for i in range(len(n)):
        acc += alpha[i] * (n[i] - acc)
        y[i] = acc
    y *= np.linspace(0.08, 1.0, len(y), dtype=np.float32) ** 2
    return (amp * y).astype(np.float32)

def boom_final(dur=2.6, amp=1.0):
    body = sweep_sine(120.0, 40.0, dur, amp) * np.exp(-t_arr(dur) * 2.4)
    crash = bandpass(noise(min(1.8, dur), 0.6), 500, 9000) * np.exp(-t_arr(min(1.8, dur)) * 2.6)
    out = body
    out[:len(crash)] += crash
    return out

# === ноты D-minor ===
D1, D2, G2, A2 = 36.71, 73.42, 98.00, 110.00
Bb1, A1 = 58.27, 55.00
D3, F3, A3, Bb2, C3, E3 = 146.83, 174.61, 220.00, 116.54, 130.81, 164.81
D4, E4, F4, G4, A4 = 293.66, 329.63, 349.23, 392.00, 440.00
Bb3, C4 = 233.08, 261.63
Ds5, F5, A5 = 587.33, 698.46, 880.00

CHORDS = {
    "Dm": [D3, F3, A3, D4],
    "Bb": [Bb2, D3, F3],
    "Gm": [G2 * 2, Bb2, D3],
    "A":  [A2, 138.59, E3],
}

TOTAL = 15.2
mix = np.zeros(int(TOTAL * SR), dtype=np.float32)

# ---- 0.00–1.30: ТЬМА + РАЙЗЕР + сердцебиение
place(mix, riser(1.30, 300.0, 9500.0, 0.34), 0.0)
place(mix, sine(D2, 1.30, 0.40) * np.linspace(0.0, 1.0, int(1.30 * SR), dtype=np.float32), 0.0)
place(mix, thump(0.28, 0.9), 0.72)
place(mix, thump(0.24, 0.65), 1.02)

# ---- 1.30: «powered by fall.os» — мягкий удар + пады + колокола
place(mix, sweep_sine(70.0, 33.0, 1.35, 0.95) * np.exp(-t_arr(1.35) * 2.6), 1.30)
place(mix, pad(CHORDS["Dm"], 6.4, 0.42), 1.30)
place(mix, bell(Ds5, 3.1, 0.30), 1.55)
place(mix, bell(A5, 2.6, 0.22), 1.95)
place(mix, bell(F5, 3.4, 0.26), 2.33)

# ---- 2.50: SLAM «CHICHKENCUM 2.1»
place(mix, boom_final(1.6, 1.0), 2.50)
place(mix, brass(CHORDS["Dm"], 0.62, 0.62, 1900.0), 2.50)
place(mix, taiko(0.8, 0.9), 2.50)

# ---- 2.5–6.45: ГРУВ A (Dm – Bb – Gm – A), 4 такта по 1.714 с
beat = 0.4286
bars = [("Dm", D2), ("Bb", Bb1 * 2), ("Gm", G2), ("A", A1 * 2)]
t0 = 2.5
for bar_i, (name, bass_f) in enumerate(bars):
    bt = t0 + bar_i * 4 * beat
    for b in range(4):
        place(mix, kick(0.5, 150.0, 47.0, 0.95 if b in (0, 2) else 0.8), bt + b * beat)
    for e in range(8):
        f = bass_f if e not in (3, 7) else bass_f * 2.0
        place(mix, saw(f, 0.24, 0.34, detune=0.003) * adsr(int(0.24 * SR), 0.008, 0.05, 0.9, 0.06, 0.75),
              bt + e * beat / 2)
    for off in (1.5, 3.5):
        place(mix, brass(CHORDS[name], 0.30, 0.42, 1500.0), bt + off * beat)
    for e in range(8):
        place(mix, hat(0.05, 0.14), bt + e * beat / 2)
    place(mix, hat(0.24, 0.10, open_=True), bt + 3.5 * beat)

# ---- 6.45–8.15: СЛОМ + райзер
t_br = 6.45
place(mix, kick(0.55, 140.0, 44.0, 0.9), t_br)
place(mix, kick(0.55, 140.0, 44.0, 0.8), t_br + 1.5 * beat)
place(mix, pad(CHORDS["Dm"], 2.6, 0.5), t_br + 0.2)
place(mix, taiko(1.1, 1.0), t_br + 3 * beat)
place(mix, riser(1.75, 500.0, 13500.0, 0.36), t_br)

# ---- 8.15–12.8: КУЛЬМИНАЦИЯ (Dm – Bb – Gm – A) + соло
t_cl = 8.15
for bar_i, (name, bass_f) in enumerate(bars):
    bt = t_cl + bar_i * 4 * beat
    for b in range(4):
        place(mix, kick(0.5, 152.0, 46.0, 1.0), bt + b * beat)
        place(mix, kick(0.35, 148.0, 48.0, 0.55), bt + (b + 0.75) * beat) if b in (1, 3) else None
    for e in range(8):
        f = bass_f if e not in (3, 7) else bass_f * 2.0
        place(mix, saw(f, 0.23, 0.40, detune=0.004) * adsr(int(0.23 * SR), 0.006, 0.05, 0.9, 0.05, 0.78),
              bt + e * beat / 2)
    for off in (0.75, 1.5, 2.75, 3.5):
        place(mix, brass(CHORDS[name], 0.22, 0.5, 2100.0), bt + off * beat)
    for beat_i in (1, 3):
        place(mix, taiko(0.7, 0.75), bt + beat_i * beat)
    for e in range(8):
        place(mix, hat(0.05, 0.17), bt + e * beat / 2)

MELODY = [
    # (time_from_climax, note, len_beats)
    (0.0 * beat, A4, 1.0), (1.0 * beat, A4, 0.5), (1.5 * beat, G4, 0.5),
    (2.0 * beat, F4, 1.0), (3.0 * beat, E4, 1.0),
    (4.0 * beat, D4, 1.0), (5.0 * beat, C4, 0.5), (5.5 * beat, D4, 0.5),
    (6.0 * beat, E4, 1.5), (7.5 * beat, D4, 0.5),
    (8.0 * beat, D4, 1.0), (9.0 * beat, D4, 0.5), (9.5 * beat, C4, 0.5),
    (10.0 * beat, Bb3, 1.0), (11.0 * beat, A3 := 220.0, 1.0),
    (12.0 * beat, A3, 0.75), (12.75 * beat, Bb3, 0.5), (13.25 * beat, 138.59, 0.5),
]
for dt_, f, beats in MELODY:
    place(mix, lead(f, beats * beat + 0.08, 0.30), t_cl + dt_)

# ---- 12.8: ФИНАЛЬНЫЙ УДАР + хвост
place(mix, boom_final(2.4, 1.0), 12.8)
place(mix, brass(CHORDS["Dm"] + [D4 * 2], 1.9, 0.55, 2400.0), 12.8)
place(mix, taiko(1.6, 0.9), 12.8)

# === шина: реверб + мягкий клип + стерео ===
# простой стерео-реверб (сгенерированный IR, экспоненциальный шум)
ir_len = int(1.9 * SR)
ir_l = rng.standard_normal(ir_len).astype(np.float32) * np.exp(-np.linspace(0, 6.2, ir_len)).astype(np.float32)
ir_r = rng.standard_normal(ir_len).astype(np.float32) * np.exp(-np.linspace(0, 6.6, ir_len)).astype(np.float32)
ir_l /= np.max(np.abs(ir_l)); ir_r /= np.max(np.abs(ir_r))
nfft = 1 << int(np.ceil(np.log2(len(mix) + ir_len)))
big = np.fft.rfft(mix, nfft)
wet_l = np.fft.irfft(big * np.fft.rfft(ir_l, nfft))[:len(mix)].astype(np.float32)
wet_r = np.fft.irfft(big * np.fft.rfft(ir_r, nfft))[:len(mix)].astype(np.float32)
# чуть отличающийся ранний отражения левого/правого = ширина
left = mix + 0.16 * wet_l
right = mix * 0.98 + 0.16 * wet_r
left *= 0.92; right *= 0.92
stereo = np.stack([left, right], axis=1)
stereo = np.tanh(stereo * 1.05)  # мягкий лимитер
peak = np.max(np.abs(stereo))
stereo = stereo / peak * 0.94   # -0.55 dBFS пик

os.makedirs(os.path.dirname(OUT), exist_ok=True)
sf.write(OUT, stereo, SR, format="WAV", subtype="PCM_16")
print("wrote", os.path.normpath(OUT), "| dur", round(len(stereo) / SR, 2), "s | peak", round(float(np.max(np.abs(stereo))), 3))
info = sf.info(OUT)
print("info:", info.samplerate, "Hz", info.channels, "ch", info.format, "/", info.subtype)

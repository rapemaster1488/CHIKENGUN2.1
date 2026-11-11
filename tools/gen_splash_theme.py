#!/usr/bin/env python3
# ============================================================================
# CHIKENGUN 2.5.6 — интро-тема «ОСЕНЬ В ЛАГЕРЯХ» (оригинальное произведение, A minor)
# Тихий заунывный шансон: дрожащая гармонь, дождь, винил, минорные i–iv–V.
# Звучит на заставке koch.studios → powered by fall.os → CHICHKENCUM 2.1
# Сгенерировано tools/gen_splash_theme.py → Data/Music/KochSplashTheme.wav
# ВНИМАНИЕ: это ОРИГИНАЛЬНАЯ композиция. «Какая осень в лагерях» — чужое
# произведение, в репозиторий не вшивается. Своя лицензионная копия —
# локально поверх этого файла (не коммитить!).
#   Запуск: python3 tools/gen_splash_theme.py   (нужны numpy, soundfile)
# ============================================================================
import numpy as np
import soundfile as sf
import os

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "Data", "Music", "KochSplashTheme.wav")

rng = np.random.default_rng(19540703)   # застывшая дата осени, для стабильного рендера

def t_arr(dur):
    return np.arange(int(dur * SR), dtype=np.float32) / SR

def sine(freq, dur, amp=1.0):
    t = t_arr(dur)
    return (amp * np.sin(2 * np.pi * freq * t)).astype(np.float32)

def sweep_sine(f0, f1, dur, amp=1.0):
    t = t_arr(dur)
    phase = 2 * np.pi * f0 * (np.log(f1 / f0) ** -1) * ((f1 / f0) ** t - 1) if f1 != f0 else 2 * np.pi * f0 * t
    return (amp * np.sin(phase)).astype(np.float32)

def saw(freq, dur, amp=1.0, detune=0.0, harmonics=26):
    t = t_arr(dur)
    f = freq * (1.0 + detune)
    out = np.zeros_like(t)
    for k in range(1, harmonics + 1):
        fk = f * k
        if fk > SR * 0.45:
            break
        out += (1.0 / k) * np.sin(2 * np.pi * fk * t)
    return (amp * out / 1.9).astype(np.float32)

def lowpass(x, cutoff):
    a = min(1.0, 2 * np.pi * cutoff / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc += a * (v - acc)
        y[i] = acc
    return y

def highpass(x, cutoff):
    return x - lowpass(x, cutoff)

def bandpass(x, lo, hi):
    return lowpass(x, hi) - lowpass(x, lo)

def adsr(n, attack, decay, sustain_level, release):
    env = np.full(n, sustain_level, dtype=np.float32)
    a = min(n, int(attack * SR)); d = min(n - a, int(decay * SR)); r = min(n, int(release * SR))
    if a > 0: env[:a] = np.linspace(0.0, 1.0, a, dtype=np.float32)
    if d > 0: env[a:a + d] = np.linspace(1.0, sustain_level, d, dtype=np.float32)
    if r > 0: env[-r:] = sustain_level * np.linspace(1.0, 0.0, r, dtype=np.float32)
    return env

def tremolo(sig, rate=5.8, depth=0.38):
    t = np.arange(len(sig), dtype=np.float32) / SR
    lfo = (1.0 - depth) + depth * (0.5 + 0.5 * np.sin(2 * np.pi * rate * t))
    return (sig * lfo.astype(np.float32)).astype(np.float32)

def accordion(freqs, dur, amp=0.30, cutoff=2100.0, rate=5.8):
    """Гармонь: две слегка расстроенные пилы в узкой полосе + тремоло."""
    out = np.zeros(int(dur * SR), dtype=np.float32)
    for f in freqs:
        for det in (-0.006, 0.006):
            out += saw(f, dur, amp / (len(freqs) * 2), detune=det, harmonics=18)
    out = bandpass(out, 260.0, cutoff)
    out = tremolo(out, rate, 0.36)
    return (out * adsr(len(out), 0.03, 0.12, 0.72, 0.14)).astype(np.float32)

def pad(freqs, dur, amp=0.40):
    out = np.zeros(int(dur * SR), dtype=np.float32)
    for f in freqs:
        for det in (-0.007, 0.0, 0.007):
            out += saw(f, dur, amp / (len(freqs) * 3), detune=det, harmonics=22)
    out = lowpass(out, 1050.0)
    env = np.ones(len(out), dtype=np.float32)
    a = min(len(out), int(1.1 * SR)); r = min(len(out), int(1.3 * SR))
    env[:a] = np.linspace(0.0, 1.0, a, dtype=np.float32)
    env[-r:] = np.linspace(1.0, 0.0, r, dtype=np.float32)
    return out * env

def pluck(freq, dur=1.1, amp=0.5):
    """Щипок баса: резонансный sine + щелчок — имитация струны."""
    t = t_arr(dur)
    tone = np.sin(2 * np.pi * freq * t) * np.exp(-t * 2.6)
    tone += 0.4 * np.sin(4 * np.pi * freq * t) * np.exp(-t * 5.0)
    click = bandpass(rng.standard_normal(int(0.02 * SR)).astype(np.float32), 700, 5200) * np.exp(-t_arr(0.02) * 160)
    out = tone.astype(np.float32)
    out[:len(click)] += click * 0.6
    return (amp * out).astype(np.float32)

def lead(freq, dur, amp=0.30):
    """Заунывная гармонь-соло: расстроенные пилы, высокий тремоло, вибрато."""
    t = t_arr(dur)
    vib = 1.0 + 0.007 * np.sin(2 * np.pi * 5.2 * t)
    phase = 2 * np.pi * np.cumsum(freq * vib) / SR
    out = np.sin(phase) + 0.55 * np.sin(2 * phase) + 0.22 * np.sin(3 * phase) + 0.07 * np.sin(4 * phase)
    out = bandpass(out.astype(np.float32), 180.0, 2350.0)
    out = tremolo(out, 6.1, 0.30)
    return (amp * out * adsr(len(out), 0.05, 0.14, 0.78, 0.16)).astype(np.float32)

def brush(dur=0.09, amp=0.13):
    n = bandpass(rng.standard_normal(int(dur * SR)).astype(np.float32), 3200, 15000)
    return (n * np.exp(-t_arr(dur) * 55)).astype(np.float32)

def kick(dur=0.5, f0=135.0, f1=44.0, amp=0.62):
    body = sweep_sine(f0, f1, dur, amp) * np.exp(-t_arr(dur) * 6.5)
    click = rng.standard_normal(int(0.010 * SR)).astype(np.float32)
    click *= np.exp(-t_arr(0.010) * 220) * 0.4
    out = body
    out[:len(click)] += click
    return out

def thump(dur=0.34, amp=0.85):
    return sweep_sine(82.0, 34.0, dur, amp) * np.exp(-t_arr(dur) * 8.0)

def taiko(dur=0.95, amp=0.8):
    lo = sine(73.4, dur, amp * 0.8) * np.exp(-t_arr(dur) * 3.4)
    mid = sine(146.8, dur * 0.8, amp * 0.35) * np.exp(-t_arr(dur * 0.8) * 5.5)
    hit = bandpass(rng.standard_normal(int(0.07 * SR)).astype(np.float32), 800, 3600) * np.exp(-t_arr(0.07) * 50)
    out = lo
    out[:len(mid)] += mid
    out[:len(hit)] += hit * 0.5
    return out

def boom_slam(dur=1.7, amp=1.0):
    body = sweep_sine(88.0, 36.0, dur, amp) * np.exp(-t_arr(dur) * 2.3)
    crash = bandpass(rng.standard_normal(int(min(1.5, dur) * SR)).astype(np.float32), 450, 7200) * np.exp(-t_arr(min(1.5, dur)) * 2.4)
    out = body
    out[:len(crash)] += crash * 0.5
    return out

def rain(dur, amp=0.045):
    """Дождь: мягкий шум + редкие капли."""
    n = rng.standard_normal(int(dur * SR)).astype(np.float32)
    base = lowpass(bandpass(n, 150, 2600), 1500)
    drops = np.zeros_like(n)
    n_drops = int(dur * 7.5)
    for _ in range(n_drops):
        i = rng.integers(0, len(drops) - 40)
        drops[i] = rng.uniform(0.5, 1.0)
    drops = lowpass(drops, 5200) * 3.0
    return (amp * (base + drops * 0.4)).astype(np.float32)

def vinyl_crackle(dur, amp=0.012):
    n = np.zeros(int(dur * SR), dtype=np.float32)
    for _ in range(int(dur * 22)):
        i = rng.integers(0, len(n) - 30)
        n[i] = rng.uniform(0.4, 1.0) * (1 if rng.random() < 0.5 else -1)
    return lowpass(n, 6300) * amp

def riser(dur, f_lo=240.0, f_hi=5200.0, amp=0.22):
    t = t_arr(dur)
    n = rng.standard_normal(len(t)).astype(np.float32)
    y = np.empty_like(n)
    cutoff = f_lo * (f_hi / f_lo) ** (t / t[-1])
    a = 2 * np.pi * cutoff / SR
    acc = 0.0
    for i in range(len(n)):
        acc += a[i] * (n[i] - acc)
        y[i] = acc
    y *= np.linspace(0.06, 1.0, len(y), dtype=np.float32) ** 2
    return (amp * y).astype(np.float32)

def place(dst, src, at_sec, gain=1.0):
    i = int(at_sec * SR)
    j = min(len(dst), i + len(src))
    if i < len(dst):
        dst[i:j] += src[:j - i] * gain

# === ноты A minor (i–iv–V с шестыми — шансонная гармония) ===
A2, E2, D2, F2, G2 = 110.00, 82.41, 73.42, 87.31, 98.00
A3, C4, D4, E4, F4, G4, B4 = 220.00, 261.63, 293.66, 329.63, 349.23, 392.00, 493.88
A4, B4n, C5, D5, E5 = 440.00, 493.88, 523.25, 587.33, 659.26
Gsharp4 = 415.30
CH = {
    "Am": [A3, C4, E4, A4],
    "F":  [F2 * 2, A3, C4],
    "Dm": [D4, F4, A4],
    "E7": [E4, Gsharp4, B4],
    "Am6": [A3, C4, E4, F4],
}

TOTAL = 15.4
mix = np.zeros(int(TOTAL * SR), dtype=np.float32)

# ---- 0.00–1.30: ДОЖДЬ + низкий дроон + сердцебиение
place(mix, rain(TOTAL, 0.030), 0.0)
place(mix, vinyl_crackle(TOTAL, 0.010), 0.0)
place(mix, sine(A2, 1.30, 0.34) * np.linspace(0.0, 1.0, int(1.30 * SR), dtype=np.float32), 0.0)
place(mix, thump(0.30, 0.8), 0.72)
place(mix, thump(0.26, 0.55), 1.04)
place(mix, riser(1.30, 200.0, 3800.0, 0.20), 0.0)

# ---- 1.30: «powered by fall.os» — мягкий удар + Am пад + заунывные голоса
place(mix, sweep_sine(58.0, 27.0, 1.5, 0.9) * np.exp(-t_arr(1.5) * 2.4), 1.30)
place(mix, pad(CH["Am"], 7.4, 0.40), 1.30)
place(mix, accordion([A3, E4, A4], 2.3, 0.22, 1600.0, 4.6), 1.32)

# ---- 2.50: SLAM «CHICHKENCUM 2.1»
place(mix, boom_slam(1.9, 1.0), 2.50)
place(mix, accordion(CH["Am6"], 0.9, 0.34, 1900.0, 6.4), 2.50)
place(mix, taiko(0.8, 0.7), 2.50)

# ---- 2.5–6.6: ГРУВ (Am – F – Dm – E7): полупритушенный шансон-шаффл
beat = 0.517          # 116 BPM
bars = [("Am", A2), ("F", F2 * 2), ("Dm", D2 * 2), ("E7", E2 * 2)]
t0 = 2.50
for bar_i, (name, bass_f) in enumerate(bars):
    bt = t0 + bar_i * 4 * beat
    for b in (0, 2):
        place(mix, kick(0.45, 128.0, 42.0, 0.66), bt + b * beat)
    for b in (1, 3):
        place(mix, brush(0.09, 0.16), bt + b * beat)      # щётки на 2 и 4
    for b in range(4):
        place(mix, brush(0.05, 0.06), bt + (b + 0.5) * beat)
    for e in range(8):
        f = bass_f if e not in (3, 7) else bass_f * (4.0 / 3.0 if name == "E7" else 1.5)
        place(mix, pluck(f, 0.42, 0.36), bt + e * beat / 2)
    if bar_i in (0, 2):
        place(mix, accordion(CH[name], 0.34, 0.26, 1750.0, 6.2), bt + 1.5 * beat)
        place(mix, accordion(CH[name], 0.30, 0.22, 1600.0, 6.2), bt + 3.5 * beat)
    else:
        place(mix, accordion(CH[name], 0.30, 0.30, 2100.0, 6.6), bt + 1.5 * beat)
        place(mix, accordion(CH[name], 0.26, 0.24, 1900.0, 6.6), bt + 3.5 * beat)

# Заунывная партия 1 (бары 1–4)
LEAD1 = [
    (2.50 + 0.0 * beat, A4, 1.2), (2.50 + 1.5 * beat, C5, 0.8),
    (2.50 + 2.5 * beat, B4n, 0.6), (2.50 + 3.2 * beat, A4, 0.8),
    (2.50 + 4.0 * beat, G4, 0.9), (2.50 + 5.0 * beat, E4, 1.1),
    (2.50 + 6.5 * beat, F4, 0.9), (2.50 + 7.6 * beat, E4, 0.9),
]
for dt_, f, beats in LEAD1:
    place(mix, lead(f, beats * beat + 0.06, 0.21), dt_)

# ---- 6.6–8.3: ДОЖДЕВОЙ СЛОМ — только пад, дождь и далёкое сердцебиение
t_br = 6.60
place(mix, pad(CH["Am"], 2.6, 0.32), t_br + 0.1)
place(mix, taiko(1.3, 0.55), t_br + 1.0 * beat)
place(mix, taiko(1.3, 0.42), t_br + 3.0 * beat)
place(mix, riser(1.7, 260.0, 4300.0, 0.24), t_br + 0.2)
place(mix, accordion([A3, E4, A4], 2.2, 0.16, 1200.0, 3.4), t_br + 0.3)

# ---- 8.3–13.1: КУЛЬМИНАЦИЯ — партия 2, тайко, полные стэбы
t_cl = 8.30
for bar_i, (name, bass_f) in enumerate(bars):
    bt = t_cl + bar_i * 4 * beat
    for b in range(4):
        place(mix, kick(0.4, 132.0, 43.0, 0.72), bt + b * beat)
    for b in (1, 3):
        place(mix, taiko(0.7, 0.5), bt + b * beat)
        place(mix, brush(0.08, 0.14), bt + b * beat)
    for e in range(8):
        f = bass_f if e not in (3, 7) else bass_f * 1.5
        place(mix, pluck(f, 0.38, 0.38), bt + e * beat / 2)
    for off in (0.75, 1.5, 2.75, 3.5):
        place(mix, accordion(CH[name], 0.30, 0.30, 2300.0, 6.8), bt + off * beat)

LEAD2 = [
    (8.30 + 0.0 * beat, A4, 1.0), (8.30 + 1.0 * beat, C5, 0.7), (8.30 + 1.75 * beat, D5, 0.7),
    (8.30 + 2.5 * beat, E5, 1.3), (8.30 + 4.0 * beat, D5, 0.8), (8.30 + 4.8 * beat, C5, 0.8),
    (8.30 + 5.6 * beat, B4n, 1.4), (8.30 + 7.2 * beat, A4, 0.8),
    (8.30 + 8.0 * beat, E4, 1.0), (8.30 + 9.2 * beat, F4, 0.7), (8.30 + 10.0 * beat, E4, 0.7),
    (8.30 + 10.8 * beat, D4, 1.0), (8.30 + 12.0 * beat, E4, 1.4),
]
for dt_, f, beats in LEAD2:
    place(mix, lead(f, beats * beat + 0.08, 0.27), dt_)

# ---- 13.1: ФИНАЛ — tutti Am, гармонь гаснет, дождь доигрывает
place(mix, boom_slam(2.3, 0.9), 13.1)
place(mix, accordion(CH["Am"] + [E5], 2.6, 0.30, 2400.0, 5.2), 13.1)
place(mix, pad(CH["Am"], 2.6, 0.30), 13.1)

# === шина: реверб + мягкий клип ===
ir_len = int(2.3 * SR)
ir_l = rng.standard_normal(ir_len).astype(np.float32) * np.exp(-np.linspace(0, 5.6, ir_len)).astype(np.float32)
ir_r = rng.standard_normal(ir_len).astype(np.float32) * np.exp(-np.linspace(0, 6.1, ir_len)).astype(np.float32)
ir_l /= np.max(np.abs(ir_l)); ir_r /= np.max(np.abs(ir_r))
nfft = 1 << int(np.ceil(np.log2(len(mix) + ir_len)))
big = np.fft.rfft(mix, nfft)
wet_l = np.fft.irfft(big * np.fft.rfft(ir_l, nfft))[:len(mix)].astype(np.float32)
wet_r = np.fft.irfft(big * np.fft.rfft(ir_r, nfft))[:len(mix)].astype(np.float32)
left = mix + 0.20 * wet_l
right = mix * 0.98 + 0.20 * wet_r
left *= 0.90; right *= 0.90
stereo = np.stack([left, right], axis=1)
stereo = np.tanh(stereo * 1.02)
peak = float(np.max(np.abs(stereo)))
stereo = (stereo / peak * 0.94).astype(np.float32)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
sf.write(OUT, stereo, SR, format="WAV", subtype="PCM_16")
print("wrote", os.path.normpath(OUT), "| dur", round(len(stereo) / SR, 2), "s | peak", round(float(np.max(np.abs(stereo))), 3))

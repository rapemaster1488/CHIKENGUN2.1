#!/usr/bin/env python3
# ============================================================================
# CHIKENGUN 2.5.6 — интро-тема «Осенняя заунывость» (оригинальное произведение, A minor)
# Меланхоличный гармонь-шансон: felt piano, щипковый бас, дождь, ветер, винил.
# Без единой расстроенной пилы: всё синусоидальное -> трансляция-«кукольное» звучание.
# Звучит на заставке koch.studios → powered by fall.os → CHICKENCUM 2.1
# Сгенерировано tools/gen_splash_theme.py → Data/Music/KochSplashTheme.wav
# ВНИМАНИЕ: это ОРИГИНАЛЬНАЯ композиция. Песни в репозиторий не продаются.
#   Запуск: python3 tools/gen_splash_theme.py   (нужны numpy, soundfile)
# ============================================================================
import numpy as np
import soundfile as sf
import os

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "Data", "Music", "KochSplashTheme.wav")

rng = np.random.default_rng(19541007)   # стабильный рендер из раза в раз

def t_arr(dur):
    return np.arange(int(dur * SR), dtype=np.float32) / SR

def sine(freq, dur, amp=1.0):
    return (amp * np.sin(2 * np.pi * freq * t_arr(dur))).astype(np.float32)

def sweep_sine(f0, f1, dur, amp=1.0):
    t = t_arr(dur)
    T = max(dur, 1e-6)
    if f1 != f0 and f0 > 0 and f1 > 0:
        # экспоненциальное скольжение: f(t) = f0 * (f1/f0)^(t/T) — интеграл точный
        k = np.log(f1 / f0) / T
        phase = 2 * np.pi * f0 * np.expm1(k * t) / k
    else:
        phase = 2 * np.pi * f0 * t
    return (amp * np.sin(phase)).astype(np.float32)

def lowpass(x, cutoff):
    """Однополюсный интегратор — сглаживает пилу в «бархат»."""
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

def humanize(delay=0.014, amp=0.10):
    """Человеческая неточность: задержка и громкость ±случайность."""
    return rng.uniform(-delay, delay), 1.0 + rng.uniform(-amp, amp)

def felt_piano(freq, dur, amp=0.5):
    """Felt piano / рояль под войлок: синус + 2-я/3-я/4-я частичные с затуханием."""
    t = t_arr(dur)
    out = (1.00 * np.sin(2 * np.pi * freq * t) * np.exp(-t * 2.1)
         + 0.42 * np.sin(2 * np.pi * freq * 2 * t + 0.3) * np.exp(-t * 3.4)
         + 0.16 * np.sin(2 * np.pi * freq * 3 * t + 0.7) * np.exp(-t * 5.2)
         + 0.06 * np.sin(2 * np.pi * freq * 4.16 * t + 1.1) * np.exp(-t * 7.0))
    # молоточек
    hit_len = min(len(t), int(0.008 * SR))
    hit = rng.standard_normal(hit_len).astype(np.float32) * np.exp(-t_arr(0.008) * 300)
    out = out.astype(np.float32)
    out[:hit_len] += hit * 0.20
    out = lowpass(out, 4600.0)
    # короткая посылка вместо ADSR: пиковая атака 12 мс
    a = min(len(out), int(0.012 * SR))
    out[:a] *= np.linspace(0.0, 1.0, a, dtype=np.float32)
    return (amp * out).astype(np.float32)

def harmonium(freq, dur, amp=0.30, rate=5.4, depth=0.26):
    """Гармонь: треугольная волна из 3 синусов, два расстроенных голоса, тремоло+вибрато."""
    t = t_arr(dur)
    vib = 1.0 + 0.0055 * np.sin(2 * np.pi * 5.1 * t)
    phase = 2 * np.pi * np.cumsum(freq * vib) / SR
    voice = (np.sin(phase)
             + 0.42 * np.sin(2 * phase + 0.15)
             + 0.15 * np.sin(3 * phase + 0.4))
    phase2 = 2 * np.pi * np.cumsum(freq * 0.997 * vib) / SR   # второй голос ниже на ~5 центов
    voice += 0.55 * (np.sin(phase2) + 0.38 * np.sin(2 * phase2))
    out = bandpass(voice.astype(np.float32), 210.0, 2450.0)
    lfo = (1.0 - depth) + depth * (0.5 + 0.5 * np.sin(2 * np.pi * rate * t))
    out *= lfo.astype(np.float32)
    return (amp * out * adsr(len(out), 0.07, 0.18, 0.80, 0.22)).astype(np.float32)

def bass_pluck(freq, dur=1.6, amp=0.42):
    """Щипковая басовая струна: быстрый щелчок + медленный хвост."""
    t = t_arr(dur)
    tone = np.sin(2 * np.pi * freq * t) * np.exp(-t * 1.9)
    tone += 0.35 * np.sin(4 * np.pi * freq * t + 0.2) * np.exp(-t * 4.2)
    tone = lowpass(tone.astype(np.float32), 1300.0)
    click_len = min(len(t), int(0.006 * SR))
    click = rng.standard_normal(click_len).astype(np.float32) * np.exp(-t_arr(0.006) * 260)
    out = tone
    out[:click_len] += click * 0.25
    return (amp * out).astype(np.float32)

def brush(dur=0.09, amp=0.10):
    n = bandpass(rng.standard_normal(int(dur * SR)).astype(np.float32), 2900, 14000)
    return (amp * n * np.exp(-t_arr(dur) * 55)).astype(np.float32)

def kick(dur=0.42, f0=118.0, f1=46.0, amp=0.5):
    body = sweep_sine(f0, f1, dur, amp) * np.exp(-t_arr(dur) * 7.5)
    click_len = int(0.008 * SR)
    click = rng.standard_normal(click_len).astype(np.float32) * np.exp(-t_arr(0.008) * 240) * 0.25
    out = body
    out[:click_len] += click
    return out

def taiko(dur=1.1, amp=0.66):
    lo = sine(73.42, dur, amp * 0.75) * np.exp(-t_arr(dur) * 3.1)
    mid = sine(146.83, dur * 0.72, amp * 0.30) * np.exp(-t_arr(dur * 0.72) * 4.6)
    hit_len = int(0.05 * SR)
    hit = bandpass(rng.standard_normal(hit_len).astype(np.float32), 700, 3200) * np.exp(-t_arr(0.05) * 55)
    out = lo
    out[:len(mid)] += mid
    out[:hit_len] += hit * 0.4
    return out

def soft_slam(dur=1.7, amp=0.8):
    body = sweep_sine(84.0, 37.0, dur, amp) * np.exp(-t_arr(dur) * 2.6)
    wash = bandpass(rng.standard_normal(int(min(1.4, dur) * SR)).astype(np.float32), 380, 5200) * np.exp(-t_arr(min(1.4, dur)) * 2.9)
    out = body
    out[:len(wash)] += wash * 0.30
    return out

def heartbeat(dur=0.3, amp=0.55):
    return sweep_sine(62.0, 40.0, dur, amp) * np.exp(-t_arr(dur) * 9.0)

def rain(dur, amp=0.020):
    n = rng.standard_normal(int(dur * SR)).astype(np.float32)
    base = lowpass(bandpass(n, 160, 2300), 1050.0)
    drops = np.zeros_like(n)
    for _ in range(int(dur * 6)):
        i = rng.integers(0, len(drops) - 40)
        drops[i] = rng.uniform(0.4, 1.0)
    drops = lowpass(drops, 4600.0) * 2.4
    return (amp * (base + drops * 0.35)).astype(np.float32)

def wind(dur, amp=0.010):
    n = lowpass(rng.standard_normal(int(dur * SR)).astype(np.float32), 420.0)
    t = t_arr(dur)
    lfo = 0.6 + 0.4 * np.sin(2 * np.pi * 0.11 * t + 1.3)   # медленное дыхание ветра
    return (amp * n * lfo).astype(np.float32)

def vinyl(dur, amp=0.006):
    n = np.zeros(int(dur * SR), dtype=np.float32)
    for _ in range(int(dur * 14)):
        i = rng.integers(0, len(n) - 30)
        n[i] = rng.uniform(0.4, 1.0) * (1 if rng.random() < 0.5 else -1)
    return lowpass(n, 6200.0) * amp

def riser(dur, f_lo=300.0, f_hi=2400.0, amp=0.10):
    t = t_arr(dur)
    n = rng.standard_normal(len(t)).astype(np.float32)
    y = np.empty_like(n)
    cutoff = f_lo * (f_hi / f_lo) ** (t / max(t[-1], 1e-6))
    a = 2 * np.pi * cutoff / SR
    acc = 0.0
    for i in range(len(n)):
        acc += a[i] * (n[i] - acc)
        y[i] = acc
    y *= np.linspace(0.10, 1.0, len(y), dtype=np.float32) ** 2.2
    return (amp * y).astype(np.float32)

def place(dst, src, at_sec, gain=1.0):
    i = int(at_sec * SR)
    j = min(len(dst), i + len(src))
    if i < len(dst):
        dst[i:j] += src[:j - i] * gain

def chord_notes(notes, dur, amp, fn):
    """Сыграет аккорд из нот одной артикуляцией, возвращает просуммированный сигнал."""
    out = np.zeros(int(dur * SR), dtype=np.float32)
    for f in notes:
        out += fn(f, dur, amp / len(notes) ** 0.5)
    return out

# === НОТЫ (A minor) ===
A1, A2, D2, E2, F2 = 55.00, 110.00, 73.42, 82.41, 87.31
A3, C4, D4, E4, F4, G4 = 220.00, 261.63, 293.66, 329.63, 349.23, 392.00
A4, B4, C5, D5, E5, F5 = 440.00, 493.88, 523.25, 587.33, 659.26, 698.46
Gsharp4 = 415.30
Am  = [A3, C4, E4, A4]
Dm  = [D4, F4, A4]
E7  = [E4, Gsharp4, B4]
F   = [F4, A4, C5]
Am_low = [A3, E4, A4]

BEAT = 0.625            # 96 BPM
BAR = 3 * BEAT          # 3/4
TOTAL = 16.6
mix = np.zeros(int(TOTAL * SR), dtype=np.float32)

# ---- 0.0–2.5: дождь, ветер, винил; дроон Am; сердцебиение; вздох гармони
place(mix, rain(TOTAL, 0.020), 0.0)
place(mix, wind(TOTAL, 0.009), 0.0)
place(mix, vinyl(TOTAL, 0.0055), 0.0)
place(mix, sine(A1, 2.6, 0.10) * np.linspace(0.0, 1.0, int(2.6 * SR), dtype=np.float32), 0.0)
place(mix, heartbeat(0.30, 0.42), 1.15)
place(mix, heartbeat(0.26, 0.30), 1.78)
place(mix, riser(2.3, 320.0, 2100.0, 0.075), 0.2)
# «вздох» — одинокая нота E4 издалека, как первое воспоминание о мелодии
place(mix, harmonium(E4, 1.5, 0.085, rate=4.2, depth=0.40), 0.95)

# ---- 2.5: музыкальный акцент — felt piano Am в полную силу + taiko
place(mix, soft_slam(1.7, 0.85), 2.50)
place(mix, taiko(1.1, 0.55), 2.50)
place(mix, chord_notes(Am, 2.6, 0.55, felt_piano), 2.50)

# ---- Вступление гармониума с мелодией -------------------------------
# Мелодия: форма A (тт.3–6) + A' (тт.7–9), всё A minor, без одной фальшивой ноты.
MELODY = [
    # (beat_offset_from_2_5, freq, dur_in_beats)
    (0.0,  A4, 1.0), (1.0,  C5, 1.0), (2.0,  E5, 1.0),          # b3: Am
    (3.0,  D5, 2.0), (5.0,  C5, 1.0),                          # b4: Dm
    (6.0,  B4, 2.0), (8.0,  Gsharp4, 1.0),                     # b5: E7
    (9.0,  A4, 3.0),                                           # b6: Am
    (12.0, F5, 1.0), (13.0, E5, 1.0), (14.0, D5, 1.0),         # b7: F
    (15.0, C5, 1.0), (16.0, B4, 1.0), (17.0, Gsharp4, 1.0),    # b8: E7
    (18.0, A4, 3.0),                                           # b9: Am
]
for beat_off, f, beats in MELODY:
    dly, amp_mul = humanize()
    t = 2.5 + beat_off * BEAT
    dur = beats * BEAT + 0.12
    place(mix, harmonium(f, dur, 0.155 * amp_mul, rate=5.2, depth=0.22), max(0.0, t + dly))

# Бас (щипковые корни) и felt piano подкладка по тактам
BARS = [
    (2.50,  A2, Am),   # b3
    (4.375, D2, Dm),   # b4
    (6.25,  E2, E7),   # b5
    (8.125, A2, Am),   # b6
    (10.00, F2, F),    # b7
    (11.875, E2, E7),  # b8
    (13.75, A2, Am),   # b9
]
for bt, bass_f, ch in BARS:
    dly, amp_mul = humanize(0.010, 0.08)
    place(mix, bass_pluck(bass_f, 2.2, 0.34 * amp_mul), bt + dly)
    place(mix, chord_notes(ch, 2.1, 0.26, felt_piano), bt + 0.015)
    # лёгкие доли 2 и 3 — пустые, дышим
    place(mix, chord_notes(ch, 0.9, 0.10, felt_piano), bt + 2 * BEAT)

# ---- Перкуссия: щётки и мягкий кик со второй фразы -------------------
for bar_i, (bt, _, _) in enumerate(BARS):
    if bar_i < 2:
        continue                      # первые два такта — тишина, только дождь и фортепиано
    accent = bar_i >= 4               # кульминация (b7–b9)
    place(mix, kick(0.40, 110.0, 45.0, 0.40 if not accent else 0.52), bt)
    place(mix, brush(0.09, 0.085 if not accent else 0.11), bt + BEAT)
    place(mix, brush(0.09, 0.085 if not accent else 0.11), bt + 2 * BEAT)
    if accent:
        place(mix, brush(0.045, 0.055), bt + 1.5 * BEAT)
        place(mix, brush(0.045, 0.055), bt + 2.5 * BEAT)
    if bar_i in (3, 6):               # b6 и b9: taiko-акцент на сильную долю
        place(mix, taiko(1.25, 0.48), bt)

# ---- 13.75: финал — tutti Am, гармонь гаснет, дождь доигрывает --------
place(mix, soft_slam(2.3, 0.52), 13.75)
place(mix, chord_notes(Am + [E5], 2.9, 0.40, felt_piano), 13.75)
place(mix, harmonium(A4, 3.1, 0.15, rate=4.6, depth=0.34), 13.75)

# === шина: конволюционный реверб + мягкий лимитер ===
ir_len = int(2.6 * SR)
ir_l = rng.standard_normal(ir_len).astype(np.float32) * np.exp(-np.linspace(0, 5.2, ir_len)).astype(np.float32)
ir_r = rng.standard_normal(ir_len).astype(np.float32) * np.exp(-np.linspace(0, 5.9, ir_len)).astype(np.float32)
ir_l = bandpass(ir_l, 140.0, 6800.0); ir_r = bandpass(ir_r, 140.0, 6800.0)
ir_l /= np.max(np.abs(ir_l)); ir_r /= np.max(np.abs(ir_r))
nfft = 1 << int(np.ceil(np.log2(len(mix) + ir_len)))
big = np.fft.rfft(mix, nfft)
wet_l = np.fft.irfft(big * np.fft.rfft(ir_l, nfft))[:len(mix)].astype(np.float32)
wet_r = np.fft.irfft(big * np.fft.rfft(ir_r, nfft))[:len(mix)].astype(np.float32)
mix_dc = mix - float(np.mean(mix))
left = mix_dc + 0.17 * wet_l
right = mix_dc * 0.97 + 0.17 * wet_r
# «институтский» компрессор-тангенс: мягкое удовольствие без клиппинга
stereo = np.tanh(np.stack([left, right], axis=1) * 1.22).astype(np.float32)
peak = float(np.max(np.abs(stereo)))
stereo = stereo / peak * 0.89

os.makedirs(os.path.dirname(OUT), exist_ok=True)
sf.write(OUT, stereo, SR, format="WAV", subtype="PCM_16")
print("wrote", os.path.normpath(OUT), "| dur", round(len(stereo) / SR, 2), "s | peak", round(float(np.max(np.abs(stereo))), 3))

#!/usr/bin/env python3
"""
Генератор звуковых эффектов для CHIKENGUN 2.1 — 4K ULTRA EDITION.
Синтезирует 16-bit 22050 Hz PCM WAV без внешних зависимостей:
куриное кудахтанье, крик босса, комбо, фанфары волн, рывок (Dash),
критический хэдшот (Headshot), подбор брони, овердрайв, удар крылом и взрыв бочки!
"""
import math, os, random, struct, wave

SR = 22050


def save_wav(path, samples):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    peak = max(1e-6, max(abs(x) for x in samples))
    scale = 0.92 / peak if peak > 0.92 else 1.0
    data = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s * scale))
        data += struct.pack("<h", int(v * 32767))
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SR)
        wf.writeframes(bytes(data))
    print(f"  [OK] {path} ({len(samples)/SR:.2f}s)")


def env_ad(t, dur, atk=0.008, exp=2.5):
    if t < 0 or t > dur:
        return 0.0
    if t < atk:
        return t / atk
    return ((1.0 - (t - atk) / max(1e-5, dur - atk)) ** exp)


def gen_cluck(seed=1, base_f=520.0, dur=0.22):
    rng = random.Random(seed)
    n = int(SR * dur)
    out = [0.0] * n
    ph = 0.0
    for i in range(n):
        t = i / SR
        f = base_f * (1.0 + 0.55 * math.exp(-t * 28.0) - 0.18 * (t / dur))
        f += 22.0 * math.sin(2 * math.pi * 38.0 * t)
        ph += 2 * math.pi * f / SR
        sig = 0.62 * math.sin(ph) + 0.26 * math.sin(2 * ph) + 0.14 * math.sin(3 * ph)
        sig += 0.15 * (rng.random() * 2 - 1) * math.exp(-t * 35.0)
        e = env_ad(t, dur, 0.005, 2.8)
        out[i] = sig * e
    return out


def gen_chicken_hurt():
    a = gen_cluck(7, 680.0, 0.11)
    b = gen_cluck(13, 860.0, 0.16)
    gap = [0.0] * int(SR * 0.02)
    return a + gap + b


def gen_chicken_die():
    dur = 0.52
    n = int(SR * dur)
    rng = random.Random(42)
    out = [0.0] * n
    ph = 0.0
    for i in range(n):
        t = i / SR
        f = 940.0 * math.exp(-t * 2.4) + 35.0 * math.sin(2 * math.pi * 29.0 * t)
        ph += 2 * math.pi * f / SR
        sig = 0.55 * math.sin(ph) + 0.30 * math.sin(2.02 * ph) + 0.18 * math.sin(3.01 * ph)
        sig = math.tanh(sig * 1.9)
        sig += 0.18 * (rng.random() * 2 - 1) * math.exp(-t * 9.0)
        out[i] = sig * env_ad(t, dur, 0.01, 1.9)
    return out


def gen_rooster_boss():
    dur = 0.95
    n = int(SR * dur)
    out = [0.0] * n
    ph = 0.0
    notes = [(0.00, 0.16, 420), (0.17, 0.32, 540), (0.33, 0.48, 460), (0.49, 0.95, 680)]
    for i in range(n):
        t = i / SR
        f = 300.0
        amp = 0.0
        for t0, t1, nf in notes:
            if t0 <= t < t1:
                lt = (t - t0) / (t1 - t0)
                f = nf * (1.0 - 0.12 * lt * lt) + 18.0 * math.sin(2 * math.pi * 22.0 * t)
                amp = math.sin(math.pi * min(1.0, lt * 6.0)) if lt < 0.15 else (1.0 - lt) ** 0.7
                break
        ph += 2 * math.pi * f / SR
        sig = math.tanh((0.65 * math.sin(ph) + 0.35 * math.sin(2 * ph) + 0.25 * math.sin(3 * ph)) * 2.1)
        out[i] = sig * amp * 0.85
    return out


def gen_wave_start():
    dur = 0.70
    n = int(SR * dur)
    out = [0.0] * n
    freqs = [293.66, 440.0, 587.33, 880.0]
    for i in range(n):
        t = i / SR
        s = 0.0
        for k, f in enumerate(freqs):
            st = k * 0.075
            if t >= st:
                lt = t - st
                s += 0.28 * math.sin(2 * math.pi * f * lt) * math.exp(-lt * 3.8)
                s += 0.14 * math.sin(4 * math.pi * f * lt) * math.exp(-lt * 5.5)
        out[i] = math.tanh(s * 1.4)
    return out


def gen_combo():
    dur = 0.28
    n = int(SR * dur)
    out = [0.0] * n
    for i in range(n):
        t = i / SR
        f = 880.0 if t < 0.09 else 1318.5
        lt = t if t < 0.09 else (t - 0.09)
        s = 0.65 * math.sin(2 * math.pi * f * lt) + 0.25 * math.sin(4 * math.pi * f * lt)
        out[i] = s * math.exp(-lt * 14.0)
    return out


def gen_empty_click():
    dur = 0.07
    n = int(SR * dur)
    rng = random.Random(99)
    out = [0.0] * n
    for i in range(n):
        t = i / SR
        s = 0.6 * math.sin(2 * math.pi * 1650.0 * t) * math.exp(-t * 95.0)
        s += 0.5 * (rng.random() * 2 - 1) * math.exp(-t * 130.0)
        out[i] = s
    return out


def gen_dash():
    """Тактический рывок (Dash): воздушный импульс + плазменный свист."""
    dur = 0.26
    n = int(SR * dur)
    rng = random.Random(2026)
    out = [0.0] * n
    ph = 0.0
    lp = 0.0
    for i in range(n):
        t = i / SR
        f = 220.0 + 780.0 * math.exp(-t * 14.0)
        ph += 2 * math.pi * f / SR
        noise = rng.random() * 2.0 - 1.0
        lp = lp * 0.72 + noise * 0.28
        whoosh = lp * 1.35 * math.sin(math.pi * min(1.0, t / dur))
        tone = 0.42 * math.sin(ph) * math.exp(-t * 11.0)
        sub = 0.45 * math.sin(2 * math.pi * 95.0 * t) * math.exp(-t * 18.0)
        out[i] = math.tanh((whoosh + tone + sub) * 1.35)
    return out


def gen_headshot():
    """Критический хэдшот (2x): металлический звон каски + хрустальный обертон."""
    dur = 0.34
    n = int(SR * dur)
    rng = random.Random(777)
    out = [0.0] * n
    for i in range(n):
        t = i / SR
        ping1 = 0.55 * math.sin(2 * math.pi * 1975.5 * t) * math.exp(-t * 14.0)
        ping2 = 0.40 * math.sin(2 * math.pi * 2960.0 * t) * math.exp(-t * 19.0)
        ping3 = 0.25 * math.sin(2 * math.pi * 4186.0 * t) * math.exp(-t * 28.0)
        snap = 0.65 * (rng.random() * 2 - 1) * math.exp(-t * 95.0)
        out[i] = math.tanh((ping1 + ping2 + ping3 + snap) * 1.3)
    return out


def gen_armor_pickup():
    """Подбор бронежилета: плотный сервопривод + защитный аккорд."""
    dur = 0.38
    n = int(SR * dur)
    out = [0.0] * n
    notes = [(0.00, 330.0), (0.08, 493.88), (0.16, 659.25)]
    for i in range(n):
        t = i / SR
        s = 0.30 * math.sin(2 * math.pi * (120.0 + 180.0 * t) * t) * math.exp(-t * 12.0)
        for t0, f in notes:
            if t >= t0:
                lt = t - t0
                s += 0.32 * math.sin(2 * math.pi * f * lt) * math.exp(-lt * 9.0)
        out[i] = math.tanh(s * 1.3)
    return out


def gen_overdrive():
    """Овердрайв (x2 урон): восходящий кибер-резонанс."""
    dur = 0.62
    n = int(SR * dur)
    out = [0.0] * n
    ph1 = 0.0
    ph2 = 0.0
    for i in range(n):
        t = i / SR
        u = t / dur
        f1 = 220.0 * (2.0 ** (u * 1.8))
        f2 = f1 * 1.5
        ph1 += 2 * math.pi * f1 / SR
        ph2 += 2 * math.pi * f2 / SR
        env = math.sin(math.pi * min(1.0, t * 14.0)) if t < 0.07 else (1.0 - (t - 0.07) / (dur - 0.07)) ** 0.8
        s = 0.48 * math.sin(ph1) + 0.34 * math.sin(ph2) + 0.20 * math.sin(ph1 * 2.0)
        out[i] = math.tanh(s * 1.6) * env
    return out


def gen_wing_slap():
    """Ближний удар крылом [V]: рассекающий взмах + плотный шлепок."""
    dur = 0.24
    n = int(SR * dur)
    rng = random.Random(314)
    out = [0.0] * n
    lp = 0.0
    for i in range(n):
        t = i / SR
        noise = rng.random() * 2.0 - 1.0
        lp = lp * 0.62 + noise * 0.38
        whoosh = lp * 0.9 * math.exp(-((t - 0.05) ** 2) * 550.0)
        slap = 0.0
        if t >= 0.065:
            lt = t - 0.065
            slap = (0.75 * math.sin(2 * math.pi * 155.0 * lt) * math.exp(-lt * 28.0) +
                    0.55 * noise * math.exp(-lt * 65.0))
        out[i] = math.tanh((whoosh + slap) * 1.5)
    return out


def gen_barrel_boom():
    """Детонация взрывоопасной бочки: глубокий суб-бас + металлический гул."""
    dur = 0.85
    n = int(SR * dur)
    rng = random.Random(1999)
    out = [0.0] * n
    lp = 0.0
    for i in range(n):
        t = i / SR
        noise = rng.random() * 2.0 - 1.0
        lp = lp * 0.86 + noise * 0.14
        sub = 0.90 * math.sin(2 * math.pi * (88.0 * math.exp(-t * 3.5) + 32.0) * t) * math.exp(-t * 4.2)
        crack = 0.65 * noise * math.exp(-t * 26.0)
        rumble = 1.15 * lp * math.exp(-t * 2.8)
        ring = 0.16 * math.sin(2 * math.pi * 620.0 * t) * math.exp(-t * 9.0)
        out[i] = math.tanh((sub + crack + rumble + ring) * 1.65)
    return out


def gen_shot_railgun():
    """Квантовый рельсотрон [8]: электромагнитный свип + гиперзвуковой хлопок + суб-гул."""
    dur = 0.68
    n = int(SR * dur)
    rng = random.Random(8080)
    out = [0.0] * n
    ph = 0.0
    for i in range(n):
        t = i / SR
        f = 2400.0 * math.exp(-t * 16.0) + 75.0
        ph += 2 * math.pi * f / SR
        beam = 0.62 * math.sin(ph) * math.exp(-t * 6.5)
        harm = 0.35 * math.sin(ph * 2.51) * math.exp(-t * 12.0)
        crack = 0.85 * (rng.random() * 2.0 - 1.0) * math.exp(-t * 58.0)
        sub = 0.72 * math.sin(2 * math.pi * 54.0 * t) * math.exp(-t * 4.5)
        out[i] = math.tanh((beam + harm + crack + sub) * 1.65)
    return out


def gen_shot_plasma():
    """Плазменный яйцемёт [9]: резонансный биоплазменный импульс."""
    dur = 0.28
    n = int(SR * dur)
    rng = random.Random(9090)
    out = [0.0] * n
    ph = 0.0
    for i in range(n):
        t = i / SR
        f = 1320.0 * math.exp(-t * 13.0) + 190.0 + 45.0 * math.sin(2 * math.pi * 62.0 * t)
        ph += 2 * math.pi * f / SR
        zap = 0.68 * math.sin(ph) + 0.32 * math.sin(2.03 * ph)
        fizz = 0.28 * (rng.random() * 2.0 - 1.0) * math.exp(-t * 32.0)
        out[i] = math.tanh((zap + fizz) * 1.45) * env_ad(t, dur, 0.004, 1.8)
    return out


def gen_jump_pad():
    """Гравитационная джамп-платформа: турбинный антиграв-подброс."""
    dur = 0.46
    n = int(SR * dur)
    rng = random.Random(404)
    out = [0.0] * n
    ph = 0.0
    lp = 0.0
    for i in range(n):
        t = i / SR
        u = t / dur
        f = 180.0 * (3.6 ** u)
        ph += 2 * math.pi * f / SR
        noise = rng.random() * 2.0 - 1.0
        lp = lp * 0.78 + noise * 0.22
        tone = 0.52 * math.sin(ph) + 0.26 * math.sin(ph * 1.5)
        air = 0.55 * lp * math.sin(math.pi * u)
        out[i] = math.tanh((tone + air) * 1.35) * env_ad(t, dur, 0.015, 1.3)
    return out


def gen_turret_deploy():
    """Установка авто-турели [T]: сервопривод + гидравлика + боевой пик."""
    dur = 0.44
    n = int(SR * dur)
    rng = random.Random(515)
    out = [0.0] * n
    for i in range(n):
        t = i / SR
        servo = 0.42 * math.sin(2 * math.pi * (240.0 + 360.0 * t) * t) * math.exp(-t * 7.5)
        clank = 0.58 * (rng.random() * 2.0 - 1.0) * math.exp(-abs(t - 0.12) * 75.0)
        beep = 0.0
        if t >= 0.20:
            lt = t - 0.20
            f = 987.77 if lt < 0.09 else 1479.98
            beep = 0.48 * math.sin(2 * math.pi * f * lt) * math.exp(-lt * 10.0)
        out[i] = math.tanh((servo + clank + beep) * 1.35)
    return out


def gen_turret_shot():
    """Короткий лазерный импульс союзной турели."""
    dur = 0.13
    n = int(SR * dur)
    out = [0.0] * n
    ph = 0.0
    for i in range(n):
        t = i / SR
        f = 1680.0 * math.exp(-t * 24.0) + 310.0
        ph += 2 * math.pi * f / SR
        out[i] = math.tanh(0.75 * math.sin(ph) * 1.4) * env_ad(t, dur, 0.003, 2.2)
    return out


def gen_perk_buy():
    """Покупка кибер-перка [B]: восходящее арпеджио."""
    dur = 0.48
    n = int(SR * dur)
    out = [0.0] * n
    notes = [(0.00, 523.25), (0.07, 659.25), (0.14, 783.99), (0.21, 1046.50)]
    for i in range(n):
        t = i / SR
        s = 0.0
        for t0, f in notes:
            if t >= t0:
                lt = t - t0
                s += 0.34 * math.sin(2 * math.pi * f * lt) * math.exp(-lt * 8.5)
                s += 0.14 * math.sin(4 * math.pi * f * lt) * math.exp(-lt * 12.0)
        out[i] = math.tanh(s * 1.35)
    return out


def gen_achievement():
    """Разблокировка достижения: торжественная золотая фанфара."""
    dur = 0.82
    n = int(SR * dur)
    out = [0.0] * n
    notes = [(0.00, 587.33), (0.10, 739.99), (0.20, 880.00), (0.32, 1174.66)]
    for i in range(n):
        t = i / SR
        s = 0.0
        for t0, f in notes:
            if t >= t0:
                lt = t - t0
                s += 0.33 * math.sin(2 * math.pi * f * lt) * math.exp(-lt * 4.8)
                s += 0.16 * math.sin(3 * math.pi * f * lt) * math.exp(-lt * 7.0)
        out[i] = math.tanh(s * 1.4)
    return out


def gen_kamikaze_beep():
    """Тревожный писк камикадзе-цыплёнка с бомбой."""
    dur = 0.14
    n = int(SR * dur)
    out = [0.0] * n
    for i in range(n):
        t = i / SR
        s = 0.65 * math.sin(2 * math.pi * 2349.3 * t) + 0.25 * math.sin(2 * math.pi * 4698.6 * t)
        out[i] = s * env_ad(t, dur, 0.004, 2.6)
    return out


def gen_grapple():
    """Энергетический грэппл-крюк [Z]: пневмовыстрел гарпуна + магнитный захват троса."""
    dur = 0.36
    n = int(SR * dur)
    rng = random.Random(2323)
    out = [0.0] * n
    ph = 0.0
    for i in range(n):
        t = i / SR
        u = t / dur
        f = 340.0 + 1480.0 * (u ** 0.72)
        ph += 2 * math.pi * f / SR
        snap = 0.78 * (rng.random() * 2.0 - 1.0) * math.exp(-t * 42.0)
        whir = 0.52 * math.sin(ph) + 0.24 * math.sin(ph * 2.02)
        clink = 0.45 * math.sin(2 * math.pi * 2760.0 * max(0.0, t - 0.09)) * math.exp(-max(0.0, t - 0.09) * 28.0) if t > 0.09 else 0.0
        out[i] = math.tanh((snap + whir * env_ad(t, dur, 0.006, 1.7) + clink) * 1.45)
    return out


def gen_orbital_strike():
    """Орбитальный лазер «ЯЙЦЕГЕДДОН» [X]: наведение спутника + нисходящий ионный столб + ударная волна."""
    dur = 1.15
    n = int(SR * dur)
    rng = random.Random(9991)
    out = [0.0] * n
    ph = 0.0
    lp = 0.0
    for i in range(n):
        t = i / SR
        noise = rng.random() * 2.0 - 1.0
        lp = lp * 0.89 + noise * 0.11
        if t < 0.22:
            f = 880.0 + 1320.0 * (t / 0.22)
            ph += 2 * math.pi * f / SR
            lock = 0.55 * math.sin(ph) * (1.0 if int(t * 36) % 2 == 0 else 0.35)
            out[i] = lock
        else:
            tb = t - 0.22
            f = 1650.0 * math.exp(-tb * 4.5) + 46.0
            ph += 2 * math.pi * f / SR
            beam = 0.65 * math.sin(ph) * math.exp(-tb * 2.2)
            sub  = 0.88 * math.sin(2 * math.pi * 48.0 * tb) * math.exp(-tb * 2.4)
            boom = 1.05 * lp * math.exp(-tb * 2.1) + 0.55 * noise * math.exp(-tb * 18.0)
            out[i] = math.tanh((beam + sub + boom) * 1.65)
    return out


def gen_banya_steam():
    """Поддача на камни [1]: шипящий залп пара + потрескивание раскалённых камней."""
    dur = 0.95
    n = int(SR * dur)
    rng = random.Random(7701)
    out = [0.0] * n
    bp = 0.0
    hp_prev = 0.0
    for i in range(n):
        t = i / SR
        noise = rng.random() * 2.0 - 1.0
        hp = noise - hp_prev * 0.72
        hp_prev = noise
        bp = bp * 0.58 + hp * 0.42
        env = env_ad(t, dur, 0.035, 1.45)
        crackle = 0.55 * noise if rng.random() < 0.008 * math.exp(-t * 2.2) else 0.0
        sizzle = (0.72 * bp + 0.28 * hp) * env + crackle
        out[i] = math.tanh(sizzle * 1.55)
    return out


def gen_banya_venik():
    """Парение берёзовым веником [2]: двойной шелестящий взмах листьев."""
    dur = 0.58
    n = int(SR * dur)
    rng = random.Random(7702)
    out = [0.0] * n
    lp = 0.0
    for i in range(n):
        t = i / SR
        noise = rng.random() * 2.0 - 1.0
        lp = lp * 0.42 + noise * 0.58
        rustle = noise - lp * 0.68
        w1 = env_ad(t, 0.27, 0.018, 2.1)
        w2 = env_ad(t - 0.24, 0.30, 0.018, 2.2) if t >= 0.24 else 0.0
        thump = 0.28 * math.sin(2 * math.pi * 145.0 * t) * math.exp(-t * 24.0)
        if t >= 0.24:
            thump += 0.24 * math.sin(2 * math.pi * 132.0 * (t - 0.24)) * math.exp(-(t - 0.24) * 24.0)
        out[i] = math.tanh((rustle * (w1 + 0.85 * w2) * 0.95 + thump) * 1.45)
    return out


def gen_banya_splash():
    """Окатиться из шайки [3]: плеск прохладной воды и звон капель."""
    dur = 0.52
    n = int(SR * dur)
    rng = random.Random(7703)
    out = [0.0] * n
    lp = 0.0
    ph = 0.0
    for i in range(n):
        t = i / SR
        noise = rng.random() * 2.0 - 1.0
        alpha = max(0.12, 0.65 - 0.45 * (t / dur))
        lp = lp * (1.0 - alpha) + noise * alpha
        f = 240.0 * math.exp(-t * 4.5) + 95.0
        ph += 2 * math.pi * f / SR
        drop = 0.22 * math.sin(2 * math.pi * 1180.0 * max(0.0, t - 0.16)) * math.exp(-max(0.0, t - 0.16) * 28.0) if t > 0.16 else 0.0
        out[i] = math.tanh((lp * env_ad(t, dur, 0.012, 1.8) * 1.15 + 0.32 * math.sin(ph) * math.exp(-t * 9.0) + drop) * 1.4)
    return out


def gen_banya_door():
    """Проветривание [4]: скрип деревянной двери и порыв морозного воздуха."""
    dur = 0.62
    n = int(SR * dur)
    rng = random.Random(7704)
    out = [0.0] * n
    ph = 0.0
    lp = 0.0
    for i in range(n):
        t = i / SR
        creak_f = 165.0 + 48.0 * math.sin(2 * math.pi * 6.5 * t)
        ph += 2 * math.pi * creak_f / SR
        creak = (0.38 * math.sin(ph) + 0.18 * math.sin(3 * ph)) * env_ad(t, 0.28, 0.02, 1.8)
        noise = rng.random() * 2.0 - 1.0
        lp = lp * 0.84 + noise * 0.16
        wind = lp * 1.35 * env_ad(max(0.0, t - 0.06), dur - 0.06, 0.06, 1.4)
        out[i] = math.tanh((creak + wind) * 1.45)
    return out


def main():
    targets = ["Data/Sounds", "bin/Data/Sounds"]
    sounds = {
        "ChickenCluck.wav":  gen_cluck(1, 540.0, 0.21),
        "ChickenHurt.wav":   gen_chicken_hurt(),
        "ChickenDie.wav":    gen_chicken_die(),
        "RoosterBoss.wav":   gen_rooster_boss(),
        "WaveStart.wav":     gen_wave_start(),
        "Combo.wav":         gen_combo(),
        "EmptyClick.wav":    gen_empty_click(),
        "Dash.wav":          gen_dash(),
        "Headshot.wav":      gen_headshot(),
        "ArmorPickup.wav":   gen_armor_pickup(),
        "Overdrive.wav":     gen_overdrive(),
        "WingSlap.wav":      gen_wing_slap(),
        "BarrelBoom.wav":    gen_barrel_boom(),
        "Shot_Railgun.wav":  gen_shot_railgun(),
        "Shot_Plasma.wav":   gen_shot_plasma(),
        "JumpPad.wav":       gen_jump_pad(),
        "TurretDeploy.wav":  gen_turret_deploy(),
        "TurretShot.wav":    gen_turret_shot(),
        "PerkBuy.wav":       gen_perk_buy(),
        "Achievement.wav":   gen_achievement(),
        "KamikazeBeep.wav":  gen_kamikaze_beep(),
        "Grapple.wav":       gen_grapple(),
        "OrbitalStrike.wav": gen_orbital_strike(),
        "BanyaSteam.wav":    gen_banya_steam(),
        "BanyaVenik.wav":    gen_banya_venik(),
        "BanyaSplash.wav":   gen_banya_splash(),
        "BanyaDoor.wav":     gen_banya_door(),
    }
    for d in targets:
        if os.path.isdir(os.path.dirname(d)) or d == "Data/Sounds":
            for name, smp in sounds.items():
                save_wav(os.path.join(d, name), smp)


if __name__ == "__main__":
    main()

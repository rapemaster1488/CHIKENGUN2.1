// ============================================================================
// gen_textures_4k.cpp — Многопоточный генератор 4K PBR текстур (4096x4096),
// 360° бесшовного HDR-style Skybox Cubemap и 4K UHD фона загрузочного экрана
// для CHIKENGUN 2.1 — 4K ULTRA EDITION.
//
// Сборка:  g++ -O3 -std=c++17 -pthread tools/gen_textures_4k.cpp -o build/gen_textures_4k
// Запуск:  ./build/gen_textures_4k build/Data/Textures 4096
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace {

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline uint8_t toByte(float v) {
    int iv = static_cast<int>(clamp01(v) * 255.0f + 0.5f);
    return static_cast<uint8_t>(iv < 0 ? 0 : (iv > 255 ? 255 : iv));
}
inline float smoothstep(float e0, float e1, float x) {
    float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Детерминированный быстрый хэш на торе (бесшовный тайлинг по periodX/periodY)
inline uint32_t hash2(int x, int y, int px, int py, uint32_t seed) {
    int wx = ((x % px) + px) % px;
    int wy = ((y % py) + py) % py;
    uint32_t h = static_cast<uint32_t>(wx) * 374761393u + static_cast<uint32_t>(wy) * 668265263u + seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

inline float hash01(int x, int y, int px, int py, uint32_t seed) {
    return (hash2(x, y, px, py, seed) & 0xFFFFFFu) / float(0x1000000u);
}

// Бесшовный 2D Value Noise (тайлится по [0..1]x[0..1])
float tileNoise(float u, float v, int cells, uint32_t seed) {
    float gx = u * cells;
    float gy = v * cells;
    int ix = static_cast<int>(std::floor(gx));
    int iy = static_cast<int>(std::floor(gy));
    float fx = gx - ix;
    float fy = gy - iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float v00 = hash01(ix,     iy,     cells, cells, seed);
    float v10 = hash01(ix + 1, iy,     cells, cells, seed);
    float v01 = hash01(ix,     iy + 1, cells, cells, seed);
    float v11 = hash01(ix + 1, iy + 1, cells, cells, seed);
    return lerpf(lerpf(v00, v10, fx), lerpf(v01, v11, fx), fy);
}

float tileFbm(float u, float v, int baseCells, int octaves, uint32_t seed) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    int c = baseCells;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * tileNoise(u, v, c, seed + o * 101u);
        norm += amp;
        amp *= 0.5f;
        c *= 2;
    }
    return sum / norm;
}

// Бесшовный Worley (клеточный камень/поры бетона)
float tileWorley(float u, float v, int cells, uint32_t seed) {
    float gx = u * cells;
    float gy = v * cells;
    int ix = static_cast<int>(std::floor(gx));
    int iy = static_cast<int>(std::floor(gy));
    float best = 10.0f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = ix + dx, cy = iy + dy;
            float px = cx + hash01(cx, cy, cells, cells, seed);
            float py = cy + hash01(cx, cy, cells, cells, seed + 77u);
            float ddx = gx - px, ddy = gy - py;
            float d2 = ddx * ddx + ddy * ddy;
            if (d2 < best) best = d2;
        }
    }
    return std::sqrt(best);
}

// 3D шум для бесшовного 360° Skybox Cubemap
inline float hash3(int x, int y, int z, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(y) * 668265263u +
                 static_cast<uint32_t>(z) * 2147483647u + seed * 1664525u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFFFFu) / float(0x1000000u);
}

float noise3D(float x, float y, float z, uint32_t seed) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    int iz = static_cast<int>(std::floor(z));
    float fx = x - ix, fy = y - iy, fz = z - iz;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    fz = fz * fz * (3.0f - 2.0f * fz);
    auto c = [&](int dx, int dy, int dz) { return hash3(ix + dx, iy + dy, iz + dz, seed); };
    float x00 = lerpf(c(0,0,0), c(1,0,0), fx);
    float x10 = lerpf(c(0,1,0), c(1,1,0), fx);
    float x01 = lerpf(c(0,0,1), c(1,0,1), fx);
    float x11 = lerpf(c(0,1,1), c(1,1,1), fx);
    return lerpf(lerpf(x00, x10, fy), lerpf(x01, x11, fy), fz);
}

float fbm3D(float x, float y, float z, int octaves, uint32_t seed) {
    float s = 0.0f, a = 1.0f, n = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        s += a * noise3D(x, y, z, seed + i * 53u);
        n += a;
        a *= 0.5f;
        x *= 2.03f; y *= 2.03f; z *= 2.03f;
    }
    return s / n;
}

// ============================================================================
// Запись PNG: быстрый сжатый PNG через python3 zlib (если доступен),
// иначе встроенный автономный RFC-1950/1951 PNG-кодер без внешних библиотек!
// ============================================================================

uint32_t crc32_calc(const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void write_be32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

void write_chunk(FILE* f, const char type[4], const uint8_t* data, size_t len) {
    uint8_t hdr[8];
    hdr[0] = (len >> 24) & 0xFF; hdr[1] = (len >> 16) & 0xFF;
    hdr[2] = (len >> 8) & 0xFF;  hdr[3] = len & 0xFF;
    std::memcpy(hdr + 4, type, 4);
    std::fwrite(hdr, 1, 8, f);
    if (len > 0) std::fwrite(data, 1, len, f);
    // CRC over type + data
    std::vector<uint8_t> tmp(4 + len);
    std::memcpy(tmp.data(), type, 4);
    if (len > 0) std::memcpy(tmp.data() + 4, data, len);
    uint32_t crc = crc32_calc(tmp.data(), tmp.size());
    uint8_t crcb[4] = { uint8_t((crc >> 24) & 0xFF), uint8_t((crc >> 16) & 0xFF),
                        uint8_t((crc >> 8) & 0xFF),  uint8_t(crc & 0xFF) };
    std::fwrite(crcb, 1, 4, f);
}

bool hasPythonZlib() {
    static int cached = -1;
    if (cached < 0) {
        int rc = std::system("python3 -c 'import zlib' >/dev/null 2>&1");
        cached = (rc == 0) ? 1 : 0;
    }
    return cached == 1;
}

bool savePngStandalone(const std::string& path, int w, int h, const std::vector<uint8_t>& rgb) {
    // Если есть python3 с сишным zlib — сжимаем поток за ~0.12 с (4K PNG ~3-5 МБ вместо 50 МБ)
    if (hasPythonZlib()) {
        std::string rawTmp = path + ".rawtmp";
        if (FILE* rf = std::fopen(rawTmp.c_str(), "wb")) {
            std::fwrite(rgb.data(), 1, rgb.size(), rf);
            std::fclose(rf);
            char cmd[2048];
            std::snprintf(cmd, sizeof(cmd),
                "python3 -c \""
                "import struct,zlib;"
                "w,h=%d,%d;"
                "raw=open('%s','rb').read();"
                "stride=w*3;"
                "scan=b''.join(b'\\x00'+raw[y*stride:(y+1)*stride] for y in range(h));"
                "comp=zlib.compress(scan,1);"
                "ch=lambda t,d: struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff);"
                "open('%s','wb').write(b'\\x89PNG\\r\\n\\x1a\\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ch(b'IDAT',comp)+ch(b'IEND',b''))"
                "\"", w, h, rawTmp.c_str(), path.c_str());
            int rc = std::system(cmd);
            std::remove(rawTmp.c_str());
            if (rc == 0) return true;
        }
    }

    // Автономный C++17 fallback (валидный PNG со stored deflate блоками)
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    std::fwrite(sig, 1, 8, f);

    uint8_t ihdr[13] = {};
    ihdr[0] = (w >> 24) & 0xFF; ihdr[1] = (w >> 16) & 0xFF; ihdr[2] = (w >> 8) & 0xFF; ihdr[3] = w & 0xFF;
    ihdr[4] = (h >> 24) & 0xFF; ihdr[5] = (h >> 16) & 0xFF; ihdr[6] = (h >> 8) & 0xFF; ihdr[7] = h & 0xFF;
    ihdr[8] = 8; ihdr[9] = 2; // 8-bit RGB
    write_chunk(f, "IHDR", ihdr, 13);

    size_t rowBytes = static_cast<size_t>(w) * 3 + 1;
    std::vector<uint8_t> raw(rowBytes * h);
    for (int y = 0; y < h; ++y) {
        raw[y * rowBytes] = 0;
        std::memcpy(&raw[y * rowBytes + 1], &rgb[static_cast<size_t>(y) * w * 3], static_cast<size_t>(w) * 3);
    }

    std::vector<uint8_t> zstream;
    zstream.reserve(raw.size() + (raw.size() / 65535 + 2) * 5 + 6);
    zstream.push_back(0x78);
    zstream.push_back(0x01);

    uint32_t s1 = 1, s2 = 0;
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t blk = std::min<size_t>(65535, raw.size() - pos);
        bool last = (pos + blk == raw.size());
        zstream.push_back(last ? 0x01 : 0x00);
        uint16_t len16 = static_cast<uint16_t>(blk);
        uint16_t nlen16 = ~len16;
        zstream.push_back(len16 & 0xFF); zstream.push_back((len16 >> 8) & 0xFF);
        zstream.push_back(nlen16 & 0xFF); zstream.push_back((nlen16 >> 8) & 0xFF);
        zstream.insert(zstream.end(), raw.begin() + pos, raw.begin() + pos + blk);
        for (size_t i = 0; i < blk; ++i) {
            s1 = (s1 + raw[pos + i]) % 65521u;
            s2 = (s2 + s1) % 65521u;
        }
        pos += blk;
    }
    write_be32(zstream, (s2 << 16) | s1);
    write_chunk(f, "IDAT", zstream.data(), zstream.size());
    write_chunk(f, "IEND", nullptr, 0);
    std::fclose(f);
    return true;
}

template <typename Fn>
void parallelRows(int h, Fn&& fn) {
    unsigned nt = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
    std::vector<std::thread> pool;
    pool.reserve(nt);
    for (unsigned t = 0; t < nt; ++t) {
        int y0 = (h * static_cast<int>(t)) / static_cast<int>(nt);
        int y1 = (h * static_cast<int>(t + 1)) / static_cast<int>(nt);
        pool.emplace_back([y0, y1, &fn]() {
            for (int y = y0; y < y1; ++y) fn(y);
        });
    }
    for (auto& th : pool) th.join();
}

void heightToNormal(const std::vector<float>& hf, int S, float strength, std::vector<uint8_t>& normRgb) {
    normRgb.resize(static_cast<size_t>(S) * S * 3);
    // Масштабируем градиент под разрешение, чтобы на 4096 микрорельеф был чётким!
    float scale = strength * (float(S) / 1024.0f);
    parallelRows(S, [&](int y) {
        int yu = (y - 1 + S) % S;
        int yd = (y + 1) % S;
        for (int x = 0; x < S; ++x) {
            int xl = (x - 1 + S) % S;
            int xr = (x + 1) % S;
            float l = hf[static_cast<size_t>(y) * S + xl];
            float r = hf[static_cast<size_t>(y) * S + xr];
            float u = hf[static_cast<size_t>(yu) * S + x];
            float d = hf[static_cast<size_t>(yd) * S + x];
            float nx = (l - r) * scale;
            float ny = (u - d) * scale;
            float nz = 1.0f;
            float inv = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            normRgb[idx + 0] = toByte(nx * inv * 0.5f + 0.5f);
            normRgb[idx + 1] = toByte(ny * inv * 0.5f + 0.5f);
            normRgb[idx + 2] = toByte(nz * inv * 0.5f + 0.5f);
        }
    });
}

struct PbrSet {
    std::vector<uint8_t> diff;
    std::vector<uint8_t> norm;
    std::vector<uint8_t> roughMetal; // R = Roughness, G = Metallic, B = AO
};

// 1. StoneHD — 4K архитектурный базальт и армированный бетон
PbrSet genStone4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float macro = tileFbm(u, v, 8, 5, 101u);
            float detail = tileFbm(u, v, 48, 4, 202u);
            float micro = tileNoise(u, v, 256, 303u);
            float pores = tileWorley(u, v, 64, 404u);

            // Швы бетонных плит (опалубка 4x2 на тайл)
            float seamX = std::min(std::fmod(u * 2.0f, 1.0f), 1.0f - std::fmod(u * 2.0f, 1.0f));
            float seamY = std::min(std::fmod(v * 4.0f, 1.0f), 1.0f - std::fmod(v * 4.0f, 1.0f));
            float seam = smoothstep(0.0f, 0.012f, std::min(seamX, seamY * 0.6f));

            // Тонкие микротрещины
            float crackVal = std::fabs(tileFbm(u * 1.5f, v * 1.5f, 12, 4, 505u) - 0.5f);
            float crack = smoothstep(0.003f, 0.018f, crackVal);

            float pit = smoothstep(0.08f, 0.22f, pores);
            float h = (macro * 0.52f + detail * 0.32f + micro * 0.16f) * seam * (0.65f + 0.35f * crack) * (0.82f + 0.18f * pit);
            hf[static_cast<size_t>(y) * S + x] = h;

            float base = (0.36f + macro * 0.22f + detail * 0.10f + (micro - 0.5f) * 0.06f);
            base *= (0.58f + 0.42f * seam) * (0.72f + 0.28f * crack) * (0.88f + 0.12f * pit);

            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(base * 1.01f);
            out.diff[idx + 1] = toByte(base * 1.00f);
            out.diff[idx + 2] = toByte(base * 0.98f);

            float rough = clamp01(0.76f - detail * 0.16f + (1.0f - seam) * 0.18f);
            float metal = 0.03f;
            float ao = clamp01(seam * crack * (0.80f + 0.20f * pit));
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte(ao);
        }
    });
    heightToNormal(hf, S, 2.2f, out.norm);
    return out;
}

// 2. FloorTile — 4K полированный кибер-обсидиан + сланец с медными/неоновыми фасками
PbrSet genFloor4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float tx = u * 8.0f, ty = v * 8.0f;
            int ix = static_cast<int>(std::floor(tx));
            int iy = static_cast<int>(std::floor(ty));
            float fx = tx - ix, fy = ty - iy;
            float edge = std::min({ fx, 1.0f - fx, fy, 1.0f - fy });

            bool alt = ((ix + iy) & 1) == 0;
            float marble = tileFbm(u + 0.15f * tileNoise(u, v, 16, 11u), v, 16, 5, 22u);
            float microScratch = tileNoise(u * 4.0f, v * 0.5f, 128, 33u);

            bool grout = edge < 0.028f;
            bool copperInlay = (edge >= 0.042f && edge < 0.056f);
            float bevel = smoothstep(0.025f, 0.050f, edge);

            float r, g, b, rough, metal, h;
            if (grout) {
                r = 0.09f; g = 0.10f; b = 0.12f;
                rough = 0.90f; metal = 0.0f; h = 0.12f;
            } else if (copperInlay) {
                // Тонкая металлическая золотисто-бронзовая инкрустация по периметру плиты
                r = 0.78f; g = 0.58f; b = 0.30f;
                rough = 0.20f; metal = 0.90f; h = 0.78f;
            } else if (alt) {
                // Светлый полированный кварцит-сланец
                float val = 0.54f + marble * 0.20f + (microScratch - 0.5f) * 0.03f;
                r = val * 0.98f; g = val * 0.99f; b = val * 1.02f;
                rough = clamp01(0.24f + marble * 0.14f);
                metal = 0.08f;
                h = 0.55f + bevel * 0.20f + marble * 0.08f;
            } else {
                // Тёмный зеркальный кибер-обсидиан (даёт сочные отражения неона!)
                float val = 0.15f + marble * 0.11f + (microScratch - 0.5f) * 0.025f;
                r = val * 0.92f; g = val * 0.98f; b = val * 1.12f;
                rough = clamp01(0.15f + marble * 0.12f);
                metal = 0.18f;
                h = 0.52f + bevel * 0.20f + marble * 0.06f;
            }

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte(grout ? 0.45f : 1.0f);
        }
    });
    heightToNormal(hf, S, 1.9f, out.norm);
    return out;
}

// 3. MetalPanel — 4K титановые sci-fi бронепанели с заклёпками и вентрешётками
PbrSet genMetalPanel4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float px = u * 8.0f, py = v * 8.0f;
            int ix = static_cast<int>(std::floor(px));
            int iy = static_cast<int>(std::floor(py));
            float fx = px - ix, fy = py - iy;
            float edge = std::min({ fx, 1.0f - fx, fy, 1.0f - fy });

            float wear = tileFbm(u, v, 24, 4, 707u);
            float brush = tileNoise(u * 8.0f, v * 0.5f, 64, 808u);

            bool seam = edge < 0.032f;
            float bevel = smoothstep(0.032f, 0.075f, edge);

            // Заклёпки по углам панелей
            float rivetDist = 10.0f;
            const float rc[4][2] = { {0.13f, 0.13f}, {0.87f, 0.13f}, {0.13f, 0.87f}, {0.87f, 0.87f} };
            for (auto& c : rc) {
                float dx = fx - c[0], dy = fy - c[1];
                rivetDist = std::min(rivetDist, std::sqrt(dx * dx + dy * dy));
            }
            bool rivet = rivetDist < 0.042f;

            // Вентиляционные прорези на части панелей
            bool ventPanel = ((ix * 3 + iy) % 5 == 0);
            bool ventSlot = ventPanel && (fx > 0.24f && fx < 0.76f && fy > 0.30f && fy < 0.70f)
                            && (std::fmod(fy * 18.0f, 1.0f) < 0.45f);

            float r, g, b, rough, metal, h;
            if (seam || ventSlot) {
                r = 0.06f; g = 0.07f; b = 0.09f;
                rough = 0.82f; metal = 0.15f; h = 0.10f;
            } else if (rivet) {
                float dome = std::sqrt(std::max(0.0f, 1.0f - (rivetDist / 0.042f) * (rivetDist / 0.042f)));
                r = 0.58f; g = 0.62f; b = 0.70f;
                rough = 0.22f; metal = 0.94f; h = 0.75f + dome * 0.22f;
            } else {
                float shade = ((ix + iy) & 1) ? 0.28f : 0.34f;
                float val = shade + wear * 0.10f + (brush - 0.5f) * 0.04f;
                r = val * 0.84f; g = val * 0.95f; b = val * 1.14f;
                rough = clamp01(0.28f + wear * 0.16f);
                metal = 0.82f;
                h = 0.45f + bevel * 0.28f + wear * 0.06f;
            }

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte((seam || ventSlot) ? 0.35f : 1.0f);
        }
    });
    heightToNormal(hf, S, 2.0f, out.norm);
    return out;
}

// 4. BrushedMetal — 4K анодированная оружейная сталь / титан
PbrSet genBrushedMetal4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        float rowSeed = hash01(0, y, 1, S, 909u);
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            // Анизотропные продольные микроборозды шлифовки
            float streak = tileFbm(u * 0.25f, v * 16.0f, 16, 4, 911u);
            float fine = std::sin(u * 180.0f + rowSeed * 35.0f) * 0.5f + 0.5f;
            float sheen = tileFbm(u, v, 8, 3, 922u);

            float h = 0.45f + streak * 0.35f + fine * 0.12f + sheen * 0.08f;
            hf[static_cast<size_t>(y) * S + x] = h;

            float val = 0.52f + streak * 0.18f + (fine - 0.5f) * 0.05f + (sheen - 0.5f) * 0.08f;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(val * 0.96f);
            out.diff[idx + 1] = toByte(val * 0.99f);
            out.diff[idx + 2] = toByte(val * 1.05f);

            out.roughMetal[idx + 0] = toByte(clamp01(0.22f + (1.0f - streak) * 0.14f));
            out.roughMetal[idx + 1] = toByte(0.90f);
            out.roughMetal[idx + 2] = toByte(1.0f);
        }
    });
    heightToNormal(hf, S, 0.85f, out.norm);
    return out;
}

// 5. CrateWood — 4K тактический ящик: дубовые доски + кованая стальная окантовка
PbrSet genCrateWood4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float border = std::min({ u, 1.0f - u, v, 1.0f - v });
            bool ironFrame = (border < 0.065f) || (std::fabs(u - v) < 0.042f && border < 0.35f);

            float plankY = v * 7.0f;
            int plankIdx = static_cast<int>(std::floor(plankY));
            float pf = plankY - plankIdx;
            bool plankGap = (pf < 0.035f || pf > 0.965f);

            float r, g, b, rough, metal, h;
            if (ironFrame) {
                float wear = tileFbm(u, v, 32, 3, 601u);
                float val = 0.26f + wear * 0.12f;
                r = val * 0.96f; g = val * 0.98f; b = val * 1.04f;
                rough = clamp01(0.28f + wear * 0.15f);
                metal = 0.88f;
                h = 0.88f + wear * 0.06f;
            } else if (plankGap) {
                r = 0.09f; g = 0.06f; b = 0.03f;
                rough = 0.92f; metal = 0.0f; h = 0.10f;
            } else {
                float warp = tileFbm(u * 0.6f, v * 2.5f, 12, 4, 615u + plankIdx * 17u);
                float rings = 0.5f + 0.5f * std::sin((v * 42.0f + warp * 5.5f) * 3.14159f + plankIdx * 2.1f);
                float fibers = tileNoise(u * 12.0f, v * 2.0f, 128, 633u);
                float tone = 0.44f + rings * 0.18f + (fibers - 0.5f) * 0.06f + (hash01(plankIdx, 0, 16, 16, 7u) - 0.5f) * 0.06f;
                r = tone * 1.04f; g = tone * 0.72f; b = tone * 0.42f;
                rough = clamp01(0.68f + (1.0f - rings) * 0.14f);
                metal = 0.0f;
                h = 0.48f + rings * 0.18f + fibers * 0.08f;
            }

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte(plankGap ? 0.35f : 1.0f);
        }
    });
    heightToNormal(hf, S, 1.7f, out.norm);
    return out;
}

// 6. FeatherPBR — 4K микрорельеф куриного оперения (нейтральное белое альбедо под MatDiffColor тинт!)
PbrSet genFeather4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float rowF = v * 12.0f;
            int row = static_cast<int>(std::floor(rowF));
            float ry = rowF - row;
            float colF = u * 10.0f + ((row & 1) ? 0.5f : 0.0f);
            float cx = (colF - std::floor(colF)) * 2.0f - 1.0f; // -1..1 от стержня пера

            // Центральный стержень (рахис) + диагональные бородки пера
            float shaft = std::exp(-cx * cx * 90.0f);
            float barbs = 0.5f + 0.5f * std::sin((std::fabs(cx) * 28.0f - ry * 18.0f) * 6.28318f);
            float arch = clamp01(1.0f - cx * cx * 0.65f - ry * 0.35f);
            float fluff = tileFbm(u, v, 32, 3, 1201u);

            float h = arch * 0.55f + shaft * 0.25f + barbs * 0.14f + fluff * 0.06f;
            hf[static_cast<size_t>(y) * S + x] = h;

            float shade = 0.88f + arch * 0.08f + barbs * 0.03f + shaft * 0.03f - (1.0f - arch) * 0.06f;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(shade);
            out.diff[idx + 1] = toByte(shade * 0.995f);
            out.diff[idx + 2] = toByte(shade * 0.985f);

            out.roughMetal[idx + 0] = toByte(clamp01(0.58f - shaft * 0.18f + (1.0f - barbs) * 0.10f));
            out.roughMetal[idx + 1] = toByte(0.0f);
            out.roughMetal[idx + 2] = toByte(clamp01(0.78f + arch * 0.22f));
        }
    });
    heightToNormal(hf, S, 1.5f, out.norm);
    return out;
}

// 7. HazardMetal — 4K взрывоопасная красная бочка с жёлто-чёрной полосой и потёртой сталью
PbrSet genHazardMetal4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            bool rib = std::fabs(v - 0.28f) < 0.024f || std::fabs(v - 0.72f) < 0.024f;
            bool band = (v > 0.42f && v < 0.58f);
            float scratch = tileFbm(u, v, 32, 4, 1505u);
            bool bareSteel = rib || (scratch > 0.74f);

            float r, g, b, rough, metal, h;
            if (bareSteel) {
                r = 0.62f; g = 0.64f; b = 0.68f;
                rough = 0.24f; metal = 0.92f;
                h = rib ? 0.92f : 0.42f;
            } else if (band) {
                bool yellow = std::fmod(u * 12.0f + v * 6.0f, 1.0f) < 0.52f;
                if (yellow) { r = 0.95f; g = 0.76f; b = 0.10f; }
                else        { r = 0.10f; g = 0.10f; b = 0.12f; }
                rough = 0.36f; metal = 0.18f; h = 0.56f;
            } else {
                float shade = 0.72f + (scratch - 0.5f) * 0.16f;
                r = shade; g = shade * 0.14f; b = shade * 0.12f;
                rough = 0.34f; metal = 0.25f; h = 0.52f + scratch * 0.05f;
            }

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte(1.0f);
        }
    });
    heightToNormal(hf, S, 1.8f, out.norm);
    return out;
}

// 8. BanyaLog — 4K оцилиндрованный липовый/кедровый сруб баньки с конопаткой и влажным блеском пара
PbrSet genBanyaLog4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    const float numLogs = 6.0f;
    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        float logPos = v * numLogs;
        int logIdx = static_cast<int>(std::floor(logPos)) % static_cast<int>(numLogs);
        float ly = logPos - std::floor(logPos);
        float cy = ly - 0.5f; // -0.5..0.5

        // Цилиндрический профиль бревна + паз с конопаткой
        float edgeDist = std::fabs(cy) * 2.0f; // 0 в центре бревна, 1 в пазу
        float cyl = std::sqrt(std::max(0.0f, 1.0f - std::min(1.0f, edgeDist * edgeDist * 0.94f)));
        float seamMask = smoothstep(0.84f, 0.96f, edgeDist);
        float logRand  = hash01(logIdx, 3, 16, 16, 701u);
        float knotU1   = hash01(logIdx, 7, 16, 16, 751u);
        float knotV1   = 0.34f + 0.32f * hash01(logIdx, 9, 16, 16, 761u);
        float knotU2   = std::fmod(knotU1 + 0.38f + 0.24f * hash01(logIdx, 13, 16, 16, 769u), 1.0f);
        float knotV2   = 0.34f + 0.32f * hash01(logIdx, 15, 16, 16, 773u);

        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float uShift = std::fmod(u + logRand * 0.67f, 1.0f);

            // Пара реалистичных сучков с индивидуальными координатами для каждого венца
            auto evalKnot = [&](float ku, float kv, float rx, float ry) {
                float du = std::fmod(u - ku + 1.5f, 1.0f) - 0.5f;
                float dv = (ly - kv);
                return std::sqrt((du / rx) * (du / rx) + (dv / ry) * (dv / ry));
            };
            float k1 = evalKnot(knotU1, knotV1, 0.028f, 0.11f);
            float k2 = evalKnot(knotU2, knotV2, 0.024f, 0.095f);
            float knotDist = std::min(k1, k2);
            float knotCore = 1.0f - smoothstep(0.28f, 0.95f, knotDist);
            float knotRing = 1.0f - smoothstep(0.65f, 1.60f, knotDist);
            float knotWarp = std::exp(-knotDist * 1.65f) * 0.72f;

            // Продольные волокна липы и годичные кольца (целые периоды по uShift -> 0 швов!)
            float warp = tileFbm(uShift, v * 2.0f, 6, 4, 709u + static_cast<uint32_t>(logIdx) * 23u);
            float grainFine = tileNoise(uShift * 14.0f, v * 2.0f, 128, 719u);
            float rings = 0.5f + 0.5f * std::sin((cy * 9.5f + warp * 4.2f + knotWarp) * 6.28318f + logRand * 6.28f);

            // Волокнистая конопатка (золотистый мох / пакля) в пазах между венцами
            float moss = tileFbm(u * 4.0f, v * 24.0f, 32, 3, 727u);
            float mossFib = tileNoise(u * 28.0f, v * 10.0f, 64, 731u);

            // Влажные банные потёки пара по тёплой древесине (целый период по u!)
            float steamRun = smoothstep(0.58f, 0.82f, tileFbm(u * 2.0f, v * 1.0f, 16, 3, 739u));

            // Тонкие трещинки усушки вдоль бревна
            float crackNoise = std::fabs(tileFbm(uShift * 2.0f, v * 2.0f, 16, 3, 743u) - 0.5f);
            float crack = (1.0f - smoothstep(0.0035f, 0.012f, crackNoise)) * smoothstep(0.45f, 0.72f, grainFine);

            float woodTone = 0.57f + rings * 0.16f + (grainFine - 0.5f) * 0.07f + (logRand - 0.5f) * 0.07f;
            woodTone = lerpf(woodTone, 0.44f, knotRing * 0.35f);
            woodTone = lerpf(woodTone, 0.25f, knotCore * 0.80f);
            woodTone *= (1.0f - crack * 0.42f);
            woodTone *= (0.62f + 0.38f * cyl);
            woodTone *= lerpf(1.0f, 0.83f, steamRun * 0.55f); // мокрая древесина чуть темнее и насыщеннее

            float wr = clamp01(woodTone * 1.12f);
            float wg = clamp01(woodTone * 0.76f);
            float wb = clamp01(woodTone * 0.42f);

            // Цвет пакли/мха
            float mr = clamp01((0.34f + moss * 0.18f + mossFib * 0.10f));
            float mg = clamp01((0.28f + moss * 0.14f + mossFib * 0.06f));
            float mb = clamp01((0.12f + moss * 0.07f));

            float r = lerpf(wr, mr, seamMask);
            float g = lerpf(wg, mg, seamMask);
            float b = lerpf(wb, mb, seamMask);

            float hWood = 0.22f + cyl * 0.64f + rings * 0.06f + grainFine * 0.04f - crack * 0.12f - knotCore * 0.04f;
            float hSeam = 0.08f + moss * 0.09f + mossFib * 0.04f;
            float h = lerpf(hWood, hSeam, seamMask);

            float roughWood = clamp01(0.62f - rings * 0.08f - steamRun * 0.36f + knotCore * 0.08f);
            float rough = lerpf(roughWood, 0.94f, seamMask);
            float ao = clamp01((0.55f + 0.45f * cyl) * lerpf(1.0f, 0.48f, seamMask));

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(0.0f);
            out.roughMetal[idx + 2] = toByte(ao);
        }
    });
    heightToNormal(hf, S, 2.35f, out.norm);
    return out;
}

// 9. BanyaRooster — 4K PBR распаренное золотисто-огненное оперение Кочета в баньке с каплями пара
PbrSet genBanyaRooster4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    const float rows = 14.0f;
    const float cols = 10.0f;
    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            // Перья растут сверху вниз: верхний ряд накладывается ЗАКРУГЛЁННЫМ кончиком поверх основания нижнего!
            float bestScore = -1e9f;
            float bestPx = 0.0f, bestPy = 0.4f, bestEdge = 0.6f, bestSeed = 0.5f;
            int baseRow = static_cast<int>(std::floor(v * rows));
            for (int dr = -2; dr <= 0; ++dr) {
                int rIdx = baseRow + dr;
                float ry = v * rows - rIdx; // 0..2.99
                float shift = (rIdx & 1) ? 0.5f : 0.0f;
                float uCol = u * cols + shift;
                int baseCol = static_cast<int>(std::floor(uCol));
                for (int dc = -1; dc <= 1; ++dc) {
                    int cIdx = baseCol + dc;
                    float cx = (uCol - cIdx) * 2.0f - 1.0f; // -1..1
                    float cy = ry / 1.65f;                  // длинное каплевидное перо с нахлёстом на 1.65 ряда
                    if (cy < 0.0f || cy > 1.0f) continue;

                    // Плавная миндалевидная форма пера (широкое у основания/середины, мягкий круглый кончик внизу)
                    float widthCurve = (cy < 0.30f)
                        ? 1.18f
                        : 1.18f * std::pow(std::max(0.0f, 1.0f - (cy - 0.30f) / 0.70f), 0.52f);
                    float distNorm = std::fabs(cx) / std::max(0.02f, widthCurve);
                    if (distNorm <= 1.0f) {
                        // Меньший rIdx (верхний ряд) лежит ПОВЕРХ большего rIdx -> никаких прямых верхних срезов!
                        float score = -float(rIdx) + (1.0f - distNorm) * 0.08f;
                        if (score > bestScore) {
                            bestScore = score;
                            bestPx = cx / std::max(0.08f, widthCurve); // -1..1 внутри пера
                            bestPy = cy;
                            bestEdge = clamp01(1.0f - distNorm);
                            bestSeed = hash01(cIdx, rIdx, static_cast<int>(cols), static_cast<int>(rows), 907u);
                        }
                    }
                }
            }

            float ax = std::fabs(bestPx);
            float shaft = std::exp(-bestPx * bestPx * 110.0f) * smoothstep(0.95f, 0.18f, bestPy);
            float barbs = 0.5f + 0.5f * std::sin((ax * 26.0f - bestPy * 22.0f + bestSeed * 2.0f) * 6.28318f);
            float microBarbs = 0.5f + 0.5f * std::sin((ax * 90.0f - bestPy * 72.0f) * 6.28318f);
            float fluff = tileFbm(u, v, 24, 4, 919u);

            // Капельки банного пота / конденсата поверх перьев
            float dropD = tileWorley(u, v, 14, 941u);
            float dropR = 0.125f;
            float dropMask = 1.0f - smoothstep(dropR * 0.72f, dropR, dropD);
            float dropDome = dropMask > 0.01f ? std::sqrt(std::max(0.0f, 1.0f - (dropD / dropR) * (dropD / dropR))) * dropMask : 0.0f;

            // Тёплая палитра Кочета в баньке: кремово-золотой центр + огненно-медный кант по краю и кончику пера
            float rimBand = smoothstep(0.42f, 0.0f, bestEdge) * smoothstep(0.12f, 0.45f, bestPy);
            float tipWarm = smoothstep(0.55f, 0.98f, bestPy);
            float coreR = lerpf(0.99f, 0.95f, bestSeed);
            float coreG = lerpf(0.91f, 0.79f, bestSeed);
            float coreB = lerpf(0.73f, 0.52f, bestSeed);

            float rimR = lerpf(0.89f, 0.78f, bestSeed);
            float rimG = lerpf(0.42f, 0.25f, bestSeed);
            float rimB = lerpf(0.14f, 0.09f, bestSeed);

            float rimMix = clamp01(rimBand * 0.75f + tipWarm * 0.40f);
            float r = lerpf(coreR, rimR, rimMix);
            float g = lerpf(coreG, rimG, rimMix);
            float b = lerpf(coreB, rimB, rimMix);

            // Прожилки бородок и светлый блестящий стержень пера
            float barbShade = 0.90f + barbs * 0.08f + microBarbs * 0.04f + (fluff - 0.5f) * 0.05f;
            r *= barbShade; g *= barbShade; b *= barbShade;
            r = lerpf(r, 1.00f, shaft * 0.55f);
            g = lerpf(g, 0.95f, shaft * 0.55f);
            b = lerpf(b, 0.82f, shaft * 0.55f);

            // Мягкая тень нахлёста по краю и кончику пера + блик капель воды
            float edgeShadow = 0.74f + 0.26f * smoothstep(0.0f, 0.30f, bestEdge);
            r *= edgeShadow; g *= edgeShadow; b *= edgeShadow;
            r = clamp01(r + dropDome * 0.09f);
            g = clamp01(g + dropDome * 0.10f);
            b = clamp01(b + dropDome * 0.13f);

            float h = 0.26f + 0.42f * smoothstep(0.0f, 0.48f, bestEdge) * (1.0f - bestPy * 0.20f)
                      + shaft * 0.18f + barbs * 0.07f + microBarbs * 0.03f + fluff * 0.04f + dropDome * 0.34f;
            float rough = clamp01(0.45f - shaft * 0.18f + (1.0f - barbs) * 0.10f - dropMask * 0.39f);
            float metal = lerpf(0.04f, 0.11f, rimMix); // лёгкий шёлково-медный отлив петушиного пера
            float ao = clamp01(0.68f + 0.32f * smoothstep(0.0f, 0.34f, bestEdge));

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte(ao);
        }
    });
    heightToNormal(hf, S, 1.85f, out.norm);
    return out;
}

// ============================================================================
// Бесшовный 6-гранный 360° Skybox + IBL Cubemap (закат + кибер-мегаполис + звёзды)
// ============================================================================
void evalSkyDir(float dx, float dy, float dz, float& outR, float& outG, float& outB) {
    // Направление на солнце совпадает с обратным вектором ключевого света в CreateLighting()!
    const float sx = 0.58f, sy = 0.50f, sz = 0.52f;
    const float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
    const float sunX = sx / sl, sunY = sy / sl, sunZ = sz / sl;

    float sunDot = std::max(0.0f, dx * sunX + dy * sunY + dz * sunZ);
    float up = clamp01(dy);
    float horizon = std::exp(-std::fabs(dy) * 5.2f);

    // Базовый сумеречный градиент зенит -> горизонт -> надир
    float r = lerpf(0.28f, 0.035f, std::pow(up, 0.55f));
    float g = lerpf(0.22f, 0.055f, std::pow(up, 0.62f));
    float b = lerpf(0.38f, 0.145f, std::pow(up, 0.48f));
    if (dy < 0.0f) {
        float dn = clamp01(-dy * 3.0f);
        r = lerpf(r, 0.04f, dn);
        g = lerpf(g, 0.05f, dn);
        b = lerpf(b, 0.08f, dn);
    }

    // Атмосферное рассеяние Ми и золотое зарево заката
    float glow = std::pow(sunDot, 5.0f) * (0.45f + 0.55f * horizon);
    r += glow * 0.72f + horizon * 0.22f * std::pow(sunDot, 1.5f);
    g += glow * 0.38f + horizon * 0.09f * std::pow(sunDot, 1.5f);
    b += glow * 0.12f;

    // Солнечный диск с мягкой короной
    float sunDisc = smoothstep(0.9965f, 0.9992f, sunDot);
    float corona = std::pow(sunDot, 64.0f) * 0.55f;
    r += corona * 1.0f + sunDisc * 1.2f;
    g += corona * 0.78f + sunDisc * 0.95f;
    b += corona * 0.42f + sunDisc * 0.70f;

    // Объёмные облака в верхней полусфере
    if (dy > 0.02f) {
        float px = dx / (dy + 0.28f) * 2.4f;
        float pz = dz / (dy + 0.28f) * 2.4f;
        float cl = fbm3D(px, dy * 2.0f, pz, 5, 777u);
        float cloudMask = smoothstep(0.50f, 0.76f, cl) * smoothstep(0.02f, 0.18f, dy);
        float lit = 0.35f + 0.65f * std::pow(sunDot, 2.2f);
        float cr = lerpf(0.24f, 0.98f, lit);
        float cg = lerpf(0.26f, 0.68f, lit);
        float cb = lerpf(0.38f, 0.52f, lit);
        r = lerpf(r, cr, cloudMask * 0.78f);
        g = lerpf(g, cg, cloudMask * 0.78f);
        b = lerpf(b, cb, cloudMask * 0.78f);
    }

    // Звёзды в высоком зените
    if (dy > 0.22f) {
        float st = noise3D(dx * 190.0f, dy * 190.0f, dz * 190.0f, 999u);
        if (st > 0.968f) {
            float br = smoothstep(0.968f, 0.995f, st) * smoothstep(0.22f, 0.65f, dy) * (1.0f - sunDot * 0.8f);
            r += br * 0.75f; g += br * 0.82f; b += br * 0.95f;
        }
    }

    // Далёкий силуэт кибер-мегаполиса вдоль горизонта (с неоновыми окнами!)
    if (dy < 0.16f) {
        float az = std::atan2(dz, dx); // -pi..pi
        float colIdx = std::floor((az + 3.14159265f) / 6.2831853f * 140.0f);
        float bldgH = 0.015f + 0.095f * std::pow(hash3(int(colIdx), 7, 13, 4242u), 1.8f);
        if (dy < bldgH) {
            float br = 0.032f, bg = 0.038f, bb = 0.062f;
            // Светящиеся окна небоскрёбов (только над горизонтом dy > 0.002f)
            float winX = std::fmod((az + 3.14159265f) * 260.0f, 1.0f);
            float winY = std::fmod((dy + 0.05f) * 340.0f, 1.0f);
            float winOn = hash3(int(colIdx * 7), int((dy + 0.05f) * 340.0f), int((az + 4.0f) * 260.0f), 8888u);
            if (dy > 0.002f && winX > 0.25f && winX < 0.75f && winY > 0.25f && winY < 0.72f && winOn > 0.74f) {
                bool cyan = (winOn > 0.88f);
                br = cyan ? 0.18f : 0.85f;
                bg = cyan ? 0.78f : 0.62f;
                bb = cyan ? 0.95f : 0.24f;
            }
            float haze = clamp01(0.35f * smoothstep(-0.18f, 0.02f, dy));
            r = lerpf(br, r, haze);
            g = lerpf(bg, g, haze);
            b = lerpf(bb, b, haze);
        }
    }

    outR = clamp01(r);
    outG = clamp01(g);
    outB = clamp01(b);
}

void genSkyCubemap(const std::string& outDir, int cubeS) {
    const char* faceNames[6] = { "PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ" };
    for (int f = 0; f < 6; ++f) {
        std::vector<uint8_t> rgb(static_cast<size_t>(cubeS) * cubeS * 3);
        parallelRows(cubeS, [&](int y) {
            float t = 2.0f * (y + 0.5f) / float(cubeS) - 1.0f;
            for (int x = 0; x < cubeS; ++x) {
                float s = 2.0f * (x + 0.5f) / float(cubeS) - 1.0f;
                float dx = 0, dy = 0, dz = 0;
                switch (f) {
                    case 0: dx =  1.0f; dy = -t;    dz = -s;    break; // +X
                    case 1: dx = -1.0f; dy = -t;    dz =  s;    break; // -X
                    case 2: dx =  s;    dy =  1.0f; dz =  t;    break; // +Y
                    case 3: dx =  s;    dy = -1.0f; dz = -t;    break; // -Y
                    case 4: dx =  s;    dy = -t;    dz =  1.0f; break; // +Z
                    case 5: dx = -s;    dy = -t;    dz = -1.0f; break; // -Z
                }
                float inv = 1.0f / std::sqrt(dx * dx + dy * dy + dz * dz);
                float r, g, b;
                evalSkyDir(dx * inv, dy * inv, dz * inv, r, g, b);
                size_t idx = (static_cast<size_t>(y) * cubeS + x) * 3;
                rgb[idx + 0] = toByte(r);
                rgb[idx + 1] = toByte(g);
                rgb[idx + 2] = toByte(b);
            }
        });
        savePngStandalone(outDir + "/Sky4K_" + faceNames[f] + ".png", cubeS, cubeS, rgb);
    }
    // Совместимый SkyDay.png (панорамный срез +Z)
    std::vector<uint8_t> sky2d(static_cast<size_t>(cubeS) * cubeS * 3);
    parallelRows(cubeS, [&](int y) {
        float t = 2.0f * (y + 0.5f) / float(cubeS) - 1.0f;
        for (int x = 0; x < cubeS; ++x) {
            float s = 2.0f * (x + 0.5f) / float(cubeS) - 1.0f;
            float dx = s, dy = -t * 0.7f + 0.25f, dz = 1.0f;
            float inv = 1.0f / std::sqrt(dx * dx + dy * dy + dz * dz);
            float r, g, b;
            evalSkyDir(dx * inv, dy * inv, dz * inv, r, g, b);
            size_t idx = (static_cast<size_t>(y) * cubeS + x) * 3;
            sky2d[idx + 0] = toByte(r);
            sky2d[idx + 1] = toByte(g);
            sky2d[idx + 2] = toByte(b);
        }
    });
    savePngStandalone(outDir + "/SkyDay.png", cubeS, cubeS, sky2d);
}

// ============================================================================
// 4K UHD (3840x2160) кибер-арт для загрузочного экрана (LoadingBg4K.png)
// ============================================================================
void genLoadingArt4K(const std::string& outDir, int W, int H) {
    std::vector<uint8_t> rgb(static_cast<size_t>(W) * H * 3);
    parallelRows(H, [&](int y) {
        float v = float(y) / float(H); // 0 top .. 1 bottom
        for (int x = 0; x < W; ++x) {
            float u = float(x) / float(W);
            float cx = (u - 0.5f) * (float(W) / float(H));
            float cy = v - 0.52f;

            // Тёмный киберпанк-космос + закатное зарево на горизонте y=0.56
            float hor = std::exp(-std::fabs(v - 0.56f) * 9.0f);
            float radGlow = std::exp(-(cx * cx * 2.4f + cy * cy * 5.0f));
            float r = 0.015f + hor * 0.26f + radGlow * 0.22f;
            float g = 0.022f + hor * 0.10f + radGlow * 0.12f;
            float b = 0.055f + hor * 0.24f + radGlow * 0.20f;

            // Перспективная неоновая сетка кибер-арены ниже горизонта
            if (v > 0.56f) {
                float p = (v - 0.56f) / 0.44f;
                float z = 1.0f / std::max(0.035f, p);
                float gx = std::fabs(std::fmod(std::fabs(cx * z * 1.8f) + 0.5f, 1.0f) - 0.5f);
                float gz = std::fabs(std::fmod(z * 1.6f, 1.0f) - 0.5f);
                float line = std::max(smoothstep(0.035f, 0.005f, gx), smoothstep(0.045f, 0.008f, gz));
                float fade = smoothstep(0.0f, 0.18f, p) * (1.0f - p * 0.45f);
                r += line * fade * 0.08f;
                g += line * fade * 0.52f;
                b += line * fade * 0.72f;
            } else {
                // Лучи прожекторов вверх от арены
                float ang = std::atan2(-cy, cx);
                float rays = std::pow(0.5f + 0.5f * std::sin(ang * 14.0f), 8.0f) * radGlow;
                r += rays * 0.16f;
                g += rays * 0.13f;
                b += rays * 0.06f;
            }

            // Тонкие сканлайны + глубокая кинематографичная виньетка
            float scan = 0.96f + 0.04f * std::sin(float(y) * 1.5708f);
            float vig = clamp01(1.12f - (cx * cx * 0.65f + (v - 0.5f) * (v - 0.5f) * 1.4f));
            size_t idx = (static_cast<size_t>(y) * W + x) * 3;
            rgb[idx + 0] = toByte(r * scan * vig);
            rgb[idx + 1] = toByte(g * scan * vig);
            rgb[idx + 2] = toByte(b * scan * vig);
        }
    });
    savePngStandalone(outDir + "/LoadingBg4K.png", W, H, rgb);
}

void writeSkyboxXml(const std::string& outDir) {
    std::string xmlPath = outDir + "/Skybox4K.xml";
    if (FILE* f = std::fopen(xmlPath.c_str(), "wb")) {
        const char* txt =
            "<cubemap>\n"
            "    <face name=\"Sky4K_PosX.png\" />\n"
            "    <face name=\"Sky4K_NegX.png\" />\n"
            "    <face name=\"Sky4K_PosY.png\" />\n"
            "    <face name=\"Sky4K_NegY.png\" />\n"
            "    <face name=\"Sky4K_PosZ.png\" />\n"
            "    <face name=\"Sky4K_NegZ.png\" />\n"
            "    <quality low=\"0\" />\n"
            "</cubemap>\n";
        std::fwrite(txt, 1, std::strlen(txt), f);
        std::fclose(f);
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string outDir = (argc > 1) ? argv[1] : "build/Data/Textures";
    int S = (argc > 2) ? std::max(256, std::min(8192, std::atoi(argv[2]))) : 4096;
    {
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
    }

    std::printf("[4K PBR Generator] Generating %dx%d PBR texture suite -> %s ...\n", S, S, outDir.c_str());

    auto saveSet = [&](const char* name, int textureSize, PbrSet&& s) {
        savePngStandalone(outDir + "/" + name + "Diffuse.png", textureSize, textureSize, s.diff);
        savePngStandalone(outDir + "/" + name + "Normal.png", textureSize, textureSize, s.norm);
        savePngStandalone(outDir + "/" + name + "RoughMetal.png", textureSize, textureSize, s.roughMetal);
        std::printf("  [OK] %-14s (%dx%d Diffuse + Normal + RoughMetal)\n", name, textureSize, textureSize);
    };

    const int smallSize = std::min(S, 2048);
    saveSet("StoneHD",      S,         genStone4K(S));
    saveSet("FloorTile",    S,         genFloor4K(S));
    saveSet("MetalPanel",   S,         genMetalPanel4K(S));
    saveSet("BrushedMetal", S,         genBrushedMetal4K(S));
    saveSet("CrateWood",    S,         genCrateWood4K(S));
    saveSet("FeatherPBR",   smallSize, genFeather4K(smallSize));
    saveSet("HazardMetal",  smallSize, genHazardMetal4K(smallSize));
    saveSet("BanyaLog",     S,         genBanyaLog4K(S));
    saveSet("BanyaRooster", S,         genBanyaRooster4K(S));

    int cubeS = 1024; // IBL.glsl использует cubeMapSize = 1024.0 (точные mip-уровни 0..10)
    genSkyCubemap(outDir, cubeS);
    writeSkyboxXml(outDir);
    std::printf("  [OK] Skybox4K       (6x %dx%d seamless 360 cubemap + IBL)\n", cubeS, cubeS);

    int loadW = (S >= 2048) ? 3840 : 1920;
    int loadH = (S >= 2048) ? 2160 : 1080;
    genLoadingArt4K(outDir, loadW, loadH);
    std::printf("  [OK] LoadingBg4K    (%dx%d UHD loading backdrop)\n", loadW, loadH);

    // Маркер версии/разрешения, чтобы build.sh знал, что 4K-набор актуален
    if (FILE* mf = std::fopen((outDir + "/.4k_pbr_stamp").c_str(), "wb")) {
        std::fprintf(mf, "%d\n", S);
        std::fclose(mf);
    }
    std::printf("[4K PBR Generator] Done!\n");
    return 0;
}

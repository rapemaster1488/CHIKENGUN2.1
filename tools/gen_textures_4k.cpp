// ============================================================================
// gen_textures_4k.cpp — Многопоточный генератор PBR текстур высокого качества
// (1024..4096), 360° бесшовного Skybox Cubemap и UHD фона загрузочного экрана
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

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = 6.28318530717958647692f;

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline uint8_t toByte(float v) {
    int iv = static_cast<int>(clamp01(v) * 255.0f + 0.5f);
    return static_cast<uint8_t>(iv < 0 ? 0 : (iv > 255 ? 255 : iv));
}
inline float smoothstep(float e0, float e1, float x) {
    float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
inline float quintic(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

inline uint32_t hash2(int x, int y, int px, int py, uint32_t seed) {
    int wx = ((x % px) + px) % px;
    int wy = ((y % py) + py) % py;
    uint32_t h = static_cast<uint32_t>(wx) * 374761393u +
                 static_cast<uint32_t>(wy) * 668265263u +
                 seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

inline float hash01(int x, int y, int px, int py, uint32_t seed) {
    return (hash2(x, y, px, py, seed) & 0xFFFFFFu) / float(0x1000000u);
}

// Бесшовный 2D Perlin Gradient Noise на торе (без квадратных артефактов!)
float tileGradNoise(float u, float v, int cells, uint32_t seed) {
    float gx = u * cells;
    float gy = v * cells;
    int ix = static_cast<int>(std::floor(gx));
    int iy = static_cast<int>(std::floor(gy));
    float fx = gx - ix;
    float fy = gy - iy;
    float ux = quintic(fx);
    float uy = quintic(fy);

    auto grad = [&](int cx, int cy, float dx, float dy) {
        float ang = hash01(cx, cy, cells, cells, seed) * TAU;
        return std::cos(ang) * dx + std::sin(ang) * dy;
    };

    float n00 = grad(ix,     iy,     fx,        fy);
    float n10 = grad(ix + 1, iy,     fx - 1.0f, fy);
    float n01 = grad(ix,     iy + 1, fx,        fy - 1.0f);
    float n11 = grad(ix + 1, iy + 1, fx - 1.0f, fy - 1.0f);
    return clamp01(0.5f + 0.707f * lerpf(lerpf(n00, n10, ux), lerpf(n01, n11, ux), uy));
}

// Анизотропный тайлящийся градиентный шум с разными частотами по X/Y
float tileGradNoiseXY(float u, float v, int cx, int cy, uint32_t seed) {
    float gx = u * cx;
    float gy = v * cy;
    int ix = static_cast<int>(std::floor(gx));
    int iy = static_cast<int>(std::floor(gy));
    float fx = gx - ix;
    float fy = gy - iy;
    float ux = quintic(fx);
    float uy = quintic(fy);

    auto grad = [&](int x0, int y0, float dx, float dy) {
        float ang = hash01(x0, y0, cx, cy, seed) * TAU;
        return std::cos(ang) * dx + std::sin(ang) * dy;
    };

    float n00 = grad(ix,     iy,     fx,        fy);
    float n10 = grad(ix + 1, iy,     fx - 1.0f, fy);
    float n01 = grad(ix,     iy + 1, fx,        fy - 1.0f);
    float n11 = grad(ix + 1, iy + 1, fx - 1.0f, fy - 1.0f);
    return clamp01(0.5f + 0.707f * lerpf(lerpf(n00, n10, ux), lerpf(n01, n11, ux), uy));
}

float tileFbm(float u, float v, int baseCells, int octaves, uint32_t seed) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    int c = baseCells;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * tileGradNoise(u, v, c, seed + static_cast<uint32_t>(o) * 101u);
        norm += amp;
        amp *= 0.5f;
        c *= 2;
    }
    return sum / norm;
}

float tileFbmXY(float u, float v, int cx, int cy, int octaves, uint32_t seed) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * tileGradNoiseXY(u, v, cx, cy, seed + static_cast<uint32_t>(o) * 137u);
        norm += amp;
        amp *= 0.5f;
        cx *= 2;
        cy *= 2;
    }
    return sum / norm;
}

// Доменное искажение (domain-warped fBm) для природного камня и волокон дерева
float tileWarpedFbm(float u, float v, int baseCells, int octaves, float warpAmp, uint32_t seed) {
    float wx = tileFbm(u, v, baseCells, 3, seed + 17u) - 0.5f;
    float wy = tileFbm(u + 0.37f, v + 0.61f, baseCells, 3, seed + 53u) - 0.5f;
    return tileFbm(u + wx * warpAmp, v + wy * warpAmp, baseCells, octaves, seed);
}

// Бесшовный Worley F1 и F2 (поры, гравий и трещины по F2 - F1)
void tileWorley2(float u, float v, int cells, uint32_t seed, float& f1Out, float& f2Out) {
    float gx = u * cells;
    float gy = v * cells;
    int ix = static_cast<int>(std::floor(gx));
    int iy = static_cast<int>(std::floor(gy));
    float d1 = 100.0f, d2 = 100.0f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = ix + dx, cy = iy + dy;
            float px = cx + 0.12f + 0.76f * hash01(cx, cy, cells, cells, seed);
            float py = cy + 0.12f + 0.76f * hash01(cx, cy, cells, cells, seed + 77u);
            float ddx = gx - px, ddy = gy - py;
            float d = ddx * ddx + ddy * ddy;
            if (d < d1) { d2 = d1; d1 = d; }
            else if (d < d2) { d2 = d; }
        }
    }
    f1Out = std::sqrt(d1);
    f2Out = std::sqrt(d2);
}

// SDF прямоугольника со скруглением (удобно для бронепанелей и лючков)
inline float sdRoundBox(float px, float py, float bx, float by, float r) {
    float qx = std::fabs(px) - bx + r;
    float qy = std::fabs(py) - by + r;
    float ox = std::max(qx, 0.0f);
    float oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
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
    fx = quintic(fx);
    fy = quintic(fy);
    fz = quintic(fz);
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
        s += a * noise3D(x, y, z, seed + static_cast<uint32_t>(i) * 53u);
        n += a;
        a *= 0.5f;
        x *= 2.03f; y *= 2.03f; z *= 2.03f;
    }
    return s / n;
}

// ============================================================================
// Запись PNG: быстрый сжатый PNG через python3 zlib (Sub-фильтр для компактности),
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
                "comp=zlib.compress(scan,5);"
                "ch=lambda t,d: struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff);"
                "open('%s','wb').write(b'\\x89PNG\\r\\n\\x1a\\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ch(b'IDAT',comp)+ch(b'IEND',b''))"
                "\"", w, h, rawTmp.c_str(), path.c_str());
            int rc = std::system(cmd);
            std::remove(rawTmp.c_str());
            if (rc == 0) return true;
        }
    }

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    std::fwrite(sig, 1, 8, f);

    uint8_t ihdr[13] = {};
    ihdr[0] = (w >> 24) & 0xFF; ihdr[1] = (w >> 16) & 0xFF; ihdr[2] = (w >> 8) & 0xFF; ihdr[3] = w & 0xFF;
    ihdr[4] = (h >> 24) & 0xFF; ihdr[5] = (h >> 16) & 0xFF; ihdr[6] = (h >> 8) & 0xFF; ihdr[7] = h & 0xFF;
    ihdr[8] = 8; ihdr[9] = 2;
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

// Изотропный 3x3 фильтр Шарра для гладких и чётких карт нормалей без ступенек
void heightToNormal(const std::vector<float>& hf, int S, float strength, std::vector<uint8_t>& normRgb) {
    normRgb.resize(static_cast<size_t>(S) * S * 3);
    float scale = (strength * (float(S) / 1024.0f)) / 16.0f;
    parallelRows(S, [&](int y) {
        int yu = (y - 1 + S) % S;
        int yd = (y + 1) % S;
        const float* rowU = &hf[static_cast<size_t>(yu) * S];
        const float* rowC = &hf[static_cast<size_t>(y)  * S];
        const float* rowD = &hf[static_cast<size_t>(yd) * S];
        for (int x = 0; x < S; ++x) {
            int xl = (x - 1 + S) % S;
            int xr = (x + 1) % S;
            float tl = rowU[xl], tc = rowU[x], tr = rowU[xr];
            float ml = rowC[xl],                mr = rowC[xr];
            float bl = rowD[xl], bc = rowD[x], br = rowD[xr];
            float gx = (3.0f * tr + 10.0f * mr + 3.0f * br) - (3.0f * tl + 10.0f * ml + 3.0f * bl);
            float gy = (3.0f * bl + 10.0f * bc + 3.0f * br) - (3.0f * tl + 10.0f * tc + 3.0f * tr);
            float nx = -gx * scale;
            float ny = -gy * scale;
            float nz = 1.0f;
            float inv = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            normRgb[idx + 0] = toByte(nx * inv * 0.5f + 0.5f);
            normRgb[idx + 1] = toByte(ny * inv * 0.5f + 0.5f);
            normRgb[idx + 2] = toByte(nz * inv * 0.5f + 0.5f);
        }
    });
}

// Запекание мягкой многонаправленной щелевой AO прямо в Diffuse (PBRLitSolid не читает RoughMetal.b!)
void applyCavityAO(const std::vector<float>& hf, int S, float strength, std::vector<uint8_t>& diff) {
    int step1 = std::max(1, S / 256);
    int step2 = std::max(2, S / 96);
    parallelRows(S, [&](int y) {
        for (int x = 0; x < S; ++x) {
            float hc = hf[static_cast<size_t>(y) * S + x];
            float occ = 0.0f;
            const int dirs[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1} };
            for (int d = 0; d < 8; ++d) {
                int x1 = (x + dirs[d][0] * step1 + S) % S;
                int y1 = (y + dirs[d][1] * step1 + S) % S;
                int x2 = (x + dirs[d][0] * step2 + S) % S;
                int y2 = (y + dirs[d][1] * step2 + S) % S;
                float d1 = hf[static_cast<size_t>(y1) * S + x1] - hc;
                float d2 = hf[static_cast<size_t>(y2) * S + x2] - hc;
                occ += clamp01(d1 * 3.8f) * 0.55f + clamp01(d2 * 2.2f) * 0.45f;
            }
            float ao = clamp01(1.0f - (occ / 8.0f) * strength);
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            diff[idx + 0] = static_cast<uint8_t>(diff[idx + 0] * ao);
            diff[idx + 1] = static_cast<uint8_t>(diff[idx + 1] * ao);
            diff[idx + 2] = static_cast<uint8_t>(diff[idx + 2] * ao);
        }
    });
}

struct PbrSet {
    std::vector<uint8_t> diff;
    std::vector<uint8_t> norm;
    std::vector<uint8_t> roughMetal; // R = Roughness (квадратируется в шейдере), G = Metallic, B = AO
};

// 1. StoneHD — архитектурный монолитный бетон с опалубкой, анкерами и минеральной фактурой (2x2 плиты)
PbrSet genStone4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            float pu = u * 2.0f, pv = v * 2.0f;
            int ix = static_cast<int>(std::floor(pu));
            int iy = static_cast<int>(std::floor(pv));
            float fx = pu - ix, fy = pv - iy;
            float panelRand = hash01(ix, iy, 2, 2, 107u);
            float panelEdge = std::min({ fx, 1.0f - fx, fy, 1.0f - fy });

            // Чёткий V-образный шов и скошенная фаска по краю монолитной плиты
            float joint = smoothstep(0.008f, 0.017f, panelEdge);
            float bevel = smoothstep(0.015f, 0.044f, panelEdge);

            // Тонкий след деревянной дощатой опалубки (горизонтальные отпечатки досок)
            float boardF = fy * 6.0f;
            int boardIdx = static_cast<int>(std::floor(boardF));
            float boardMod = hash01(ix * 6 + boardIdx, iy, 12, 2, 149u);
            float formGrain = tileFbmXY(u + panelRand * 0.41f, v, 6, 48, 3, 181u) * (0.65f + 0.35f * boardMod);

            // Минеральный гравий и мелкая зернистость бетона (без равномерных точек)
            float macro = tileWarpedFbm(u, v, 4, 5, 0.08f, 101u);
            float mineral = tileFbm(u, v, 24, 4, 211u);
            float microSand = tileGradNoise(u, v, 192, 307u);
            float microPit = smoothstep(0.76f, 0.86f, tileFbm(u, v, 64, 3, 401u)) * smoothstep(0.55f, 0.80f, microSand);
            float f1, f2;
            tileWorley2(u, v, 20, 409u, f1, f2);
            // Тонкая микротрещина только по редким участкам
            float stressMask = smoothstep(0.66f, 0.84f, tileFbm(u, v, 4, 3, 503u));
            float crack = (1.0f - smoothstep(0.004f, 0.016f, f2 - f1)) * stressMask * 0.45f;
            (void)f1;

            // Анкерные конусы опалубки по 4 углам каждой плиты + подтёк влаги под ними
            float tieCone = 0.0f, weepStain = 0.0f;
            const float anchors[4][2] = { {0.16f, 0.18f}, {0.84f, 0.18f}, {0.16f, 0.82f}, {0.84f, 0.82f} };
            for (int a = 0; a < 4; ++a) {
                float dx = fx - anchors[a][0];
                float dy = fy - anchors[a][1];
                float r = std::sqrt(dx * dx + dy * dy);
                if (r < 0.034f) {
                    float rim = smoothstep(0.034f, 0.024f, r) * (1.0f - smoothstep(0.022f, 0.013f, r));
                    float hole = smoothstep(0.020f, 0.005f, r);
                    tieCone = std::max(tieCone, hole * 0.85f - rim * 0.25f);
                }
                if (dy > 0.01f && dy < 0.28f && std::fabs(dx) < 0.032f) {
                    float streak = std::exp(-(dx * dx) / 0.00028f) * (1.0f - dy / 0.28f);
                    weepStain = std::max(weepStain, streak * (0.5f + 0.5f * mineral));
                }
            }

            float h = 0.45f + bevel * 0.28f + (macro - 0.5f) * 0.12f + (formGrain - 0.5f) * 0.07f
                      + (mineral - 0.5f) * 0.07f + (microSand - 0.5f) * 0.045f
                      - microPit * 0.09f - crack * 0.15f - tieCone * 0.30f;
            h *= (0.18f + 0.82f * joint);
            hf[static_cast<size_t>(y) * S + x] = h;

            // Тёплый архитектурный серый бетон с лёгким базальтовым оттенком
            float tone = 0.44f + (panelRand - 0.5f) * 0.05f + (macro - 0.5f) * 0.11f
                         + (formGrain - 0.5f) * 0.05f + (mineral - 0.5f) * 0.07f
                         + (microSand - 0.5f) * 0.045f;
            tone *= (0.42f + 0.58f * joint) * (1.0f - microPit * 0.16f) * (1.0f - crack * 0.38f) * (1.0f - tieCone * 0.55f);

            float r = tone * 1.01f;
            float g = tone * 1.00f;
            float b = tone * 0.98f;
            // Ржаво-минеральный подтёк от стального анкера
            r = lerpf(r, r * 0.86f + 0.05f, weepStain * 0.65f);
            g = lerpf(g, g * 0.78f + 0.01f, weepStain * 0.65f);
            b = lerpf(b, b * 0.68f,         weepStain * 0.65f);

            float rough = clamp01(0.84f - (macro - 0.5f) * 0.14f - bevel * 0.04f + (1.0f - joint) * 0.12f);
            float ao = clamp01(joint * (1.0f - tieCone * 0.5f) * (1.0f - crack * 0.35f));

            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(0.02f);
            out.roughMetal[idx + 2] = toByte(ao);
        }
    });
    applyCavityAO(hf, S, 0.34f, out.diff);
    heightToNormal(hf, S, 2.6f, out.norm);
    return out;
}

// 2. FloorTile — тёмные архитектурные базальто-сланцевые плиты 2x2 с дренажом и титановыми стыками
PbrSet genFloor4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            // Крупные 2x2 модульные плиты (никакой кричащей шахматной доски!)
            float tx = u * 2.0f, ty = v * 2.0f;
            int ix = static_cast<int>(std::floor(tx));
            int iy = static_cast<int>(std::floor(ty));
            float fx = tx - ix, fy = ty - iy;
            float slabRand = hash01(ix, iy, 2, 2, 29u);
            float edge = std::min({ fx, 1.0f - fx, fy, 1.0f - fy });

            bool grout = edge < 0.016f;
            bool steelChannel = (edge >= 0.016f && edge < 0.030f);
            float bevel = smoothstep(0.028f, 0.056f, edge);

            // Угловые крепёжные болты с шестигранным углублением
            float boltMask = 0.0f, boltSocket = 0.0f;
            const float corners[4][2] = { {0.075f, 0.075f}, {0.925f, 0.075f}, {0.075f, 0.925f}, {0.925f, 0.925f} };
            for (int c = 0; c < 4; ++c) {
                float dx = fx - corners[c][0], dy = fy - corners[c][1];
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < 0.026f) {
                    boltMask = std::max(boltMask, smoothstep(0.026f, 0.020f, d));
                    boltSocket = std::max(boltSocket, smoothstep(0.013f, 0.006f, d));
                }
            }

            // Натуральный сланцевый базальт + тёртые следы обуви/техники + микропоры (без повторяющихся решёток)
            float vein = tileWarpedFbm(u + slabRand * 0.23f, v + slabRand * 0.19f, 5, 5, 0.11f, 37u);
            float stoneFine = tileFbm(u, v, 32, 4, 59u);
            float scuff = tileFbmXY(u, v, 10, 40, 3, 73u);
            float micro = tileGradNoise(u, v, 192, 97u);

            float r, g, b, rough, metal, h;
            if (grout) {
                r = 0.06f; g = 0.065f; b = 0.075f;
                rough = 0.92f; metal = 0.05f; h = 0.08f;
            } else if (steelChannel || boltMask > 0.3f) {
                float grain = tileFbmXY(u, v, 48, 8, 3, 131u);
                float baseM = (boltMask > 0.3f) ? 0.34f : 0.27f;
                float val = baseM + (grain - 0.5f) * 0.07f;
                val *= (1.0f - boltSocket * 0.65f);
                r = val * 0.95f; g = val * 0.98f; b = val * 1.05f;
                rough = clamp01(0.32f + (1.0f - grain) * 0.14f + boltSocket * 0.45f);
                metal = 0.88f * (1.0f - boltSocket * 0.5f);
                h = (steelChannel ? 0.40f : 0.64f) + grain * 0.05f - boltSocket * 0.28f;
            } else {
                // Глубокий графитовый базальт с мягким сатиновым блеском и лёгкой разницей тона плит
                float slabBase = 0.19f + ((ix ^ iy) & 1) * 0.025f + (slabRand - 0.5f) * 0.024f;
                float val = slabBase + (vein - 0.5f) * 0.09f + (stoneFine - 0.5f) * 0.05f + (micro - 0.5f) * 0.025f;
                // Тонкий латунно-бронзовый кант только по внутренней фаске плиты
                float trimLine = smoothstep(0.030f, 0.035f, edge) * (1.0f - smoothstep(0.035f, 0.042f, edge));
                r = lerpf(val * 0.95f, 0.44f, trimLine * 0.45f);
                g = lerpf(val * 0.99f, 0.36f, trimLine * 0.45f);
                b = lerpf(val * 1.06f, 0.24f, trimLine * 0.45f);

                // Полированные зоны и потёртости дают живую игру света при движении камеры
                float wearPolish = smoothstep(0.42f, 0.72f, scuff);
                rough = clamp01(0.38f - wearPolish * 0.14f + (stoneFine - 0.5f) * 0.12f);
                metal = lerpf(0.06f, 0.45f, trimLine * 0.45f);
                h = 0.48f + bevel * 0.22f + vein * 0.08f + (stoneFine - 0.5f) * 0.05f;
            }

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte(grout ? 0.35f : 1.0f);
        }
    });
    applyCavityAO(hf, S, 0.36f, out.diff);
    heightToNormal(hf, S, 2.3f, out.norm);
    return out;
}

// 3. MetalPanel — асимметричная титановая бронепанель со скосами, лючком и вентжалюзи
PbrSet genMetalPanel4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            // 3 асимметричные секции (верхняя бронеплита + левый модуль жалюзи + правый сервисный лючок)
            float dTop   = sdRoundBox(u - 0.50f, v - 0.28f, 0.465f, 0.245f, 0.025f);
            float dVent  = sdRoundBox(u - 0.275f, v - 0.765f, 0.240f, 0.200f, 0.022f);
            float dHatch = sdRoundBox(u - 0.755f, v - 0.765f, 0.210f, 0.200f, 0.022f);

            float dPanel = std::min({ dTop, dVent, dHatch });
            bool inGroove = dPanel > -0.008f;
            float panelBevel = smoothstep(-0.008f, -0.032f, dPanel);
            float edgeCrest = smoothstep(-0.042f, -0.026f, dPanel) * (1.0f - smoothstep(-0.022f, -0.008f, dPanel));

            // Вентиляционная выемка с 7 наклонными стальными ламелями
            float dVentHole = sdRoundBox(u - 0.275f, v - 0.765f, 0.175f, 0.125f, 0.012f);
            bool inVent = (dVentHole < 0.0f);
            float louverH = 0.0f;
            bool inLouverGap = false;
            if (inVent) {
                float ly = (v - 0.640f) / 0.250f;
                float slat = std::fmod(ly * 7.0f, 1.0f);
                inLouverGap = (slat < 0.24f);
                louverH = 0.18f + 0.34f * (1.0f - slat);
            }

            // Утопленная ручка на сервисном лючке
            float dHandle = sdRoundBox(u - 0.635f, v - 0.765f, 0.026f, 0.068f, 0.008f);
            bool inHandleRecess = (dHandle < 0.0f);

            // Шестигранные винты по углам панелей
            const float bolts[10][2] = {
                {0.075f, 0.075f}, {0.925f, 0.075f}, {0.075f, 0.485f}, {0.925f, 0.485f},
                {0.500f, 0.075f}, {0.500f, 0.485f},
                {0.075f, 0.605f}, {0.475f, 0.605f}, {0.585f, 0.605f}, {0.925f, 0.605f}
            };
            float boltHead = 0.0f, boltHex = 0.0f;
            for (int b = 0; b < 10; ++b) {
                float dx = u - bolts[b][0], dy = v - bolts[b][1];
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < 0.020f) {
                    boltHead = std::max(boltHead, smoothstep(0.020f, 0.015f, d));
                    boltHex  = std::max(boltHex, smoothstep(0.009f, 0.005f, d));
                }
            }

            float brush = tileFbmXY(u, v, 6, 72, 3, 809u);
            float wear = tileWarpedFbm(u, v, 8, 4, 0.07f, 709u);
            float grime = smoothstep(-0.055f, -0.005f, dPanel) * (0.45f + 0.55f * wear);

            float r, g, b, rough, metal, h;
            if (inGroove || inLouverGap) {
                r = 0.065f; g = 0.072f; b = 0.088f;
                rough = 0.84f; metal = 0.25f; h = 0.08f;
            } else if (inVent) {
                float vval = 0.22f + brush * 0.06f;
                r = vval * 0.90f; g = vval * 0.95f; b = vval * 1.04f;
                rough = 0.34f; metal = 0.88f; h = louverH;
            } else if (inHandleRecess) {
                bool bar = std::fabs(u - 0.635f) < 0.008f;
                r = bar ? 0.42f : 0.09f; g = bar ? 0.44f : 0.10f; b = bar ? 0.48f : 0.12f;
                rough = bar ? 0.28f : 0.78f; metal = bar ? 0.92f : 0.30f; h = bar ? 0.56f : 0.16f;
            } else if (boltHead > 0.3f) {
                float bv = 0.46f * (1.0f - boltHex * 0.75f);
                r = bv * 0.94f; g = bv * 0.97f; b = bv * 1.04f;
                rough = boltHex > 0.3f ? 0.78f : 0.22f;
                metal = boltHex > 0.3f ? 0.35f : 0.94f;
                h = 0.82f - boltHex * 0.34f;
            } else {
                // Основная бронеплита: физически корректное альбедо титановой стали для PBR F0
                bool isTop = (dTop < 0.0f);
                float baseTone = isTop ? 0.56f : 0.49f;
                float val = baseTone + (wear - 0.5f) * 0.10f + (brush - 0.5f) * 0.06f + edgeCrest * 0.14f;
                val *= (1.0f - grime * 0.42f);

                // Небольшой янтарно-стальной маркер на верхней плите
                float stripe = (isTop && u > 0.07f && u < 0.19f && v > 0.18f && v < 0.205f) ? 1.0f : 0.0f;
                r = lerpf(val * 0.90f, 0.84f, stripe * 0.88f);
                g = lerpf(val * 0.96f, 0.58f, stripe * 0.88f);
                b = lerpf(val * 1.05f, 0.16f, stripe * 0.88f);

                rough = clamp01(0.32f + (wear - 0.5f) * 0.14f - edgeCrest * 0.10f + grime * 0.35f);
                metal = lerpf(0.86f, 0.28f, grime * 0.65f + stripe * 0.75f);
                h = 0.38f + panelBevel * 0.34f + (brush - 0.5f) * 0.04f;
            }

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte(inGroove ? 0.35f : 1.0f);
        }
    });
    applyCavityAO(hf, S, 0.38f, out.diff);
    heightToNormal(hf, S, 2.4f, out.norm);
    return out;
}

// 4. BrushedMetal — тонкая шлифованная оружейная сталь / титан (для оружия и деталей)
PbrSet genBrushedMetal4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float brushFine = tileFbmXY(u, v, 3, 128, 3, 911u);
            float brushMed  = tileFbmXY(u, v, 6, 48, 3, 937u);
            float crossHone = tileFbmXY(u, v, 40, 24, 2, 953u);
            float patina    = tileWarpedFbm(u, v, 4, 3, 0.06f, 977u);

            float h = 0.45f + (brushFine - 0.5f) * 0.24f + (brushMed - 0.5f) * 0.16f + (crossHone - 0.5f) * 0.08f;
            hf[static_cast<size_t>(y) * S + x] = h;

            float val = 0.74f + (brushFine - 0.5f) * 0.12f + (brushMed - 0.5f) * 0.09f + (patina - 0.5f) * 0.09f;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(val * 0.96f);
            out.diff[idx + 1] = toByte(val * 0.99f);
            out.diff[idx + 2] = toByte(val * 1.04f);

            out.roughMetal[idx + 0] = toByte(clamp01(0.24f + (1.0f - brushFine) * 0.12f + (patina - 0.5f) * 0.08f));
            out.roughMetal[idx + 1] = toByte(0.92f);
            out.roughMetal[idx + 2] = toByte(1.0f);
        }
    });
    heightToNormal(hf, S, 1.05f, out.norm);
    return out;
}

// 5. CrateWood — тактический дубовый ящик: доски + цельная стальная рама с заклёпками (без обрубков!)
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
            // Цельный кованый уголок по периметру + угловые косынки + центральная стяжка
            bool frameOuter = border < 0.085f;
            bool centerStrap = std::fabs(v - 0.5f) < 0.048f;
            float cornerSum1 = u + v;
            float cornerSum2 = (1.0f - u) + v;
            float cornerSum3 = u + (1.0f - v);
            float cornerSum4 = (1.0f - u) + (1.0f - v);
            bool gusset = (cornerSum1 < 0.22f) || (cornerSum2 < 0.22f) || (cornerSum3 < 0.22f) || (cornerSum4 < 0.22f);
            bool steelPart = frameOuter || centerStrap || gusset;

            // Заклёпки по стальной раме и центральной стяжке
            float rivetDist = 10.0f;
            const float rpts[14][2] = {
                {0.042f, 0.042f}, {0.958f, 0.042f}, {0.042f, 0.958f}, {0.958f, 0.958f},
                {0.135f, 0.042f}, {0.865f, 0.042f}, {0.135f, 0.958f}, {0.865f, 0.958f},
                {0.042f, 0.135f}, {0.958f, 0.135f}, {0.042f, 0.865f}, {0.958f, 0.865f},
                {0.160f, 0.500f}, {0.840f, 0.500f}
            };
            for (int i = 0; i < 14; ++i) {
                float dx = u - rpts[i][0], dy = v - rpts[i][1];
                rivetDist = std::min(rivetDist, std::sqrt(dx * dx + dy * dy));
            }
            bool rivet = steelPart && (rivetDist < 0.019f);

            // Горизонтальные дубовые доски (6 штук) внутри стальной рамы
            float plankF = ((v - 0.085f) / 0.830f) * 6.0f;
            int plankIdx = std::max(0, std::min(5, static_cast<int>(std::floor(plankF))));
            float pf = plankF - plankIdx;
            float plankEdge = std::min(pf, 1.0f - pf);
            bool plankGap = !steelPart && (plankEdge < 0.032f);
            float plankBevel = smoothstep(0.030f, 0.095f, plankEdge);

            float r, g, b, rough, metal, h;
            if (steelPart) {
                float edgeWear = smoothstep(0.085f, 0.070f, border) * (1.0f - smoothstep(0.070f, 0.055f, border));
                float metalBrush = tileFbmXY(u, v, 32, 32, 3, 601u);
                float rustPatch = smoothstep(0.64f, 0.85f, tileWarpedFbm(u, v, 8, 3, 0.08f, 607u));
                float val = 0.22f + (metalBrush - 0.5f) * 0.08f + edgeWear * 0.14f;
                r = lerpf(val * 0.96f, 0.34f, rustPatch * 0.45f);
                g = lerpf(val * 0.99f, 0.21f, rustPatch * 0.45f);
                b = lerpf(val * 1.06f, 0.14f, rustPatch * 0.45f);
                rough = clamp01(0.32f + rustPatch * 0.40f - edgeWear * 0.10f);
                metal = lerpf(0.88f, 0.30f, rustPatch * 0.5f);
                h = 0.80f + (metalBrush - 0.5f) * 0.04f;
                if (rivet) {
                    float dome = std::sqrt(std::max(0.0f, 1.0f - (rivetDist / 0.019f) * (rivetDist / 0.019f)));
                    r *= (1.15f + 0.25f * dome);
                    g *= (1.15f + 0.25f * dome);
                    b *= (1.18f + 0.25f * dome);
                    rough = 0.20f;
                    metal = 0.95f;
                    h = 0.88f + dome * 0.12f;
                }
            } else if (plankGap) {
                r = 0.07f; g = 0.045f; b = 0.025f;
                rough = 0.94f; metal = 0.0f; h = 0.12f;
            } else {
                float pSeed = hash01(plankIdx, 3, 8, 8, 613u);
                float uShift = std::fmod(u + pSeed * 0.71f, 1.0f);

                // Один естественный сучок на доску с мягким обтеканием волокон
                float ku = 0.18f + 0.64f * hash01(plankIdx, 7, 8, 8, 619u);
                float kv = 0.32f + 0.36f * hash01(plankIdx, 11, 8, 8, 629u);
                float du = u - ku;
                float dv = (pf - kv) * 0.22f;
                float kdist = std::sqrt(du * du + dv * dv * 10.0f);
                float knotDeflect = std::exp(-kdist * 14.0f) * 0.16f;
                float knotCore = smoothstep(0.055f, 0.012f, kdist);

                float warp = tileFbmXY(uShift, v * 2.0f, 3, 12, 4, 631u + static_cast<uint32_t>(plankIdx) * 19u);
                float ringCoord = (pf - 0.5f) * 4.2f + (warp - 0.5f) * 3.8f + knotDeflect * 12.0f;
                float rings = 0.5f + 0.5f * std::sin(ringCoord * PI);
                rings = std::pow(rings, 0.65f);
                float pores = tileFbmXY(uShift, v, 16, 128, 3, 643u);
                float poreStreak = smoothstep(0.62f, 0.82f, pores);

                float tone = 0.44f + (pSeed - 0.5f) * 0.08f + (rings - 0.5f) * 0.15f - poreStreak * 0.10f;
                tone = lerpf(tone, 0.24f, knotCore * 0.72f);
                tone *= (0.76f + 0.24f * plankBevel);

                r = tone * 1.08f;
                g = tone * 0.74f;
                b = tone * 0.42f;
                rough = clamp01(0.68f + (1.0f - rings) * 0.12f + poreStreak * 0.10f);
                metal = 0.0f;
                h = 0.40f + plankBevel * 0.22f + rings * 0.08f - poreStreak * 0.07f;
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
    applyCavityAO(hf, S, 0.35f, out.diff);
    heightToNormal(hf, S, 2.2f, out.norm);
    return out;
}

// 6. FeatherPBR — органичное многослойное оперение со смещением, углом и прямыми бородками
PbrSet genFeather4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    const int cols = 10, rows = 14;
    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            float bestScore = -1e9f;
            float bestRx = 0.0f, bestPx = 0.0f, bestPy = 0.5f, bestEdge = 0.35f, bestSeed = 0.5f;
            int baseRow = static_cast<int>(std::floor(v * rows));

            for (int dr = -3; dr <= 0; ++dr) {
                int rIdx = baseRow + dr;
                int wr = ((rIdx % rows) + rows) % rows;
                float shift = (wr & 1) ? 0.5f : 0.0f;
                float uCol = u * cols + shift;
                int baseCol = static_cast<int>(std::floor(uCol));
                for (int dc = -1; dc <= 1; ++dc) {
                    int cIdx = baseCol + dc;
                    int wc = ((cIdx % cols) + cols) % cols;
                    float jx = (hash01(wc, wr, cols, rows, 1201u) - 0.5f) * 0.22f;
                    float jy = (hash01(wc, wr, cols, rows, 1217u) - 0.5f) * 0.12f;
                    float tilt = (hash01(wc, wr, cols, rows, 1229u) - 0.5f) * 0.18f;

                    float cx = ((uCol - cIdx) * 2.0f - 1.0f) + jx;
                    float cy = (v * rows - rIdx + jy + 0.15f) / 2.25f;
                    if (cy < 0.0f || cy > 1.0f) continue;

                    float rx = cx * std::cos(tilt) - (cy - 0.4f) * std::sin(tilt);
                    float widthCurve = (cy < 0.28f)
                        ? 1.18f
                        : 1.18f * std::pow(std::max(0.0f, 1.0f - (cy - 0.28f) / 0.72f), 0.55f);
                    float notch = 0.025f * std::sin(cy * 24.0f + wc * 1.7f);
                    float dNorm = std::fabs(rx) / std::max(0.04f, widthCurve + notch);
                    if (dNorm <= 1.0f) {
                        float score = -float(rIdx) + (1.0f - dNorm) * 0.05f;
                        if (score > bestScore) {
                            bestScore = score;
                            bestRx = rx;
                            bestPx = rx / std::max(0.08f, widthCurve);
                            bestPy = cy;
                            bestEdge = clamp01(1.0f - dNorm);
                            bestSeed = hash01(wc, wr, cols, rows, 1237u);
                        }
                    }
                }
            }

            // Прямые V-образные бородки от стержня (не изогнутые в овал!)
            float ax = std::fabs(bestRx);
            float shaft = std::exp(-bestPx * bestPx * 140.0f) * smoothstep(0.92f, 0.12f, bestPy);
            float barbs = 0.5f + 0.5f * std::sin((ax * 14.0f - bestPy * 16.0f + bestSeed * 2.5f) * TAU);
            float microBarbs = 0.5f + 0.5f * std::sin((ax * 48.0f - bestPy * 52.0f) * TAU);
            float fluff = tileFbm(u, v, 32, 3, 1249u);

            float h = 0.24f + 0.44f * smoothstep(0.0f, 0.46f, bestEdge) * (1.0f - bestPy * 0.14f)
                      + shaft * 0.18f + barbs * 0.08f + microBarbs * 0.04f + fluff * 0.03f;
            hf[static_cast<size_t>(y) * S + x] = h;

            float underAO = 0.78f + 0.22f * smoothstep(0.0f, 0.28f, bestEdge);
            float shade = (0.87f + (bestSeed - 0.5f) * 0.06f + barbs * 0.05f + microBarbs * 0.025f + shaft * 0.05f) * underAO;

            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(shade);
            out.diff[idx + 1] = toByte(shade * 0.992f);
            out.diff[idx + 2] = toByte(shade * 0.978f);

            out.roughMetal[idx + 0] = toByte(clamp01(0.56f - shaft * 0.18f + (1.0f - barbs) * 0.12f));
            out.roughMetal[idx + 1] = toByte(0.0f);
            out.roughMetal[idx + 2] = toByte(underAO);
        }
    });
    heightToNormal(hf, S, 1.85f, out.norm);
    return out;
}

// 7. HazardMetal — промышленная топливная бочка с рельефными рёбрами и чёткими сколами краски
PbrSet genHazardMetal4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            float dRib1 = std::fabs(v - 0.28f);
            float dRib2 = std::fabs(v - 0.72f);
            float dChime = std::min(v, 1.0f - v);
            float ribProf = std::max(
                dRib1 < 0.036f ? std::cos((dRib1 / 0.036f) * (PI * 0.5f)) : 0.0f,
                dRib2 < 0.036f ? std::cos((dRib2 / 0.036f) * (PI * 0.5f)) : 0.0f
            );
            float chimeProf = (dChime < 0.038f) ? std::cos((dChime / 0.038f) * (PI * 0.5f)) : 0.0f;
            float ridge = std::max(ribProf, chimeProf);

            float chipDetail = tileFbm(u, v, 24, 4, 1505u);
            float fineBrush = tileFbmXY(u, v, 48, 8, 3, 1511u);
            float soot = tileFbm(u, v, 6, 3, 1523u);

            // Чёткие сколы краски только на рёбрах жёсткости и кромках (без серого дыма по краске!)
            float wearSignal = chipDetail * 0.65f + ridge * 0.55f;
            float bareSteelMask = smoothstep(0.76f, 0.78f, wearSignal);
            float chipBorder = smoothstep(0.742f, 0.76f, wearSignal) * (1.0f - bareSteelMask);

            bool band = (v > 0.415f && v < 0.585f);
            bool pinStripe = (std::fabs(v - 0.415f) < 0.008f || std::fabs(v - 0.585f) < 0.008f);

            float r, g, b, rough, metal, h;
            if (bareSteelMask > 0.5f) {
                float sv = 0.50f + (fineBrush - 0.5f) * 0.14f;
                r = sv * 0.96f; g = sv * 0.98f; b = sv * 1.04f;
                rough = clamp01(0.24f + (1.0f - fineBrush) * 0.14f);
                metal = 0.94f;
                h = 0.48f + ridge * 0.44f + (fineBrush - 0.5f) * 0.04f;
            } else {
                if (pinStripe) {
                    r = 0.09f; g = 0.09f; b = 0.11f;
                    rough = 0.42f; metal = 0.25f;
                } else if (band) {
                    float diag = std::fmod(u * 10.0f + v * 5.0f, 1.0f);
                    bool yellow = diag < 0.50f;
                    float grime = 0.88f + 0.12f * (1.0f - soot);
                    if (yellow) { r = 0.88f * grime; g = 0.67f * grime; b = 0.09f * grime; }
                    else        { r = 0.09f;         g = 0.09f;         b = 0.10f; }
                    rough = clamp01(0.34f + soot * 0.14f); metal = 0.16f;
                } else {
                    float enamel = 0.60f + (soot - 0.5f) * 0.12f + (fineBrush - 0.5f) * 0.04f;
                    r = enamel * 1.06f; g = enamel * 0.14f; b = enamel * 0.10f;
                    rough = clamp01(0.32f + soot * 0.15f); metal = 0.22f;
                }
                // Чёткая тонкая тёмная каёмка толщины краски вокруг скола
                r = lerpf(r, r * 0.45f, chipBorder);
                g = lerpf(g, g * 0.45f, chipBorder);
                b = lerpf(b, b * 0.45f, chipBorder);
                h = 0.52f + ridge * 0.44f + (fineBrush - 0.5f) * 0.03f;
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
    applyCavityAO(hf, S, 0.30f, out.diff);
    heightToNormal(hf, S, 2.5f, out.norm);
    return out;
}

// 8. BanyaLog — оцилиндрованный сруб липы/кедра с естественными сучками (без клякс!) и кручёной паклей
PbrSet genBanyaLog4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    const int numLogs = 5;
    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        float logPos = v * numLogs;
        int logIdx = static_cast<int>(std::floor(logPos)) % numLogs;
        float ly = logPos - std::floor(logPos);
        float cy = ly - 0.5f;

        float edgeDist = std::fabs(cy) * 2.0f;
        float cyl = std::sqrt(std::max(0.0f, 1.0f - std::min(1.0f, edgeDist * edgeDist * 0.92f)));
        float seamMask = smoothstep(0.83f, 0.95f, edgeDist);
        float logRand  = hash01(logIdx, 3, 16, 16, 701u);

        float ku1 = hash01(logIdx, 7, 16, 16, 751u);
        float kv1 = 0.34f + 0.32f * hash01(logIdx, 9, 16, 16, 761u);
        float ku2 = std::fmod(ku1 + 0.46f + 0.16f * hash01(logIdx, 13, 16, 16, 769u), 1.0f);
        float kv2 = 0.34f + 0.32f * hash01(logIdx, 15, 16, 16, 773u);

        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float uShift = std::fmod(u + logRand * 0.67f, 1.0f);

            // Естественный сучок с обтекающими древесными кольцами (чёткое ядро, без размытого пятна)
            auto evalKnot = [&](float ku, float kv) {
                float du = std::fmod(u - ku + 1.5f, 1.0f) - 0.5f;
                float dv = (ly - kv) * 0.24f;
                return std::sqrt(du * du + dv * dv * 12.0f);
            };
            float kd = std::min(evalKnot(ku1, kv1), evalKnot(ku2, kv2));
            float knotDeflect = std::exp(-kd * 16.0f) * 0.36f;
            float knotCore = smoothstep(0.038f, 0.012f, kd);
            float knotRing = smoothstep(0.055f, 0.032f, kd) * (1.0f - knotCore);

            float warp = tileFbmXY(uShift, v * 2.0f, 4, 16, 4, 709u + static_cast<uint32_t>(logIdx) * 23u);
            float ringWave = 0.5f + 0.5f * std::sin(((cy * 3.8f + (warp - 0.5f) * 2.4f + knotDeflect * 6.0f)) * TAU + logRand * TAU);
            ringWave = std::pow(ringWave, 0.72f);

            float fiber = tileFbmXY(uShift, v, 14, 128, 3, 719u);
            // Продольные трещины усушки строго вдоль волокон древесины (без извилистых полос)
            float splitLine = tileFbmXY(uShift, v, 4, 84, 3, 743u);
            float crack = smoothstep(0.76f, 0.86f, splitLine) * smoothstep(0.45f, 0.72f, fiber) * 0.65f;

            // Кручёная золотистая пакля / джут (конопатка) в глубоком пазу
            float juteTwist = 0.5f + 0.5f * std::sin((u * 42.0f + ly * 18.0f + tileFbm(u, v, 16, 2, 727u) * 3.0f) * TAU);
            float juteFibers = tileFbmXY(u, v, 32, 96, 3, 731u);
            float steamPatina = smoothstep(0.55f, 0.80f, tileFbm(u, v, 6, 3, 739u));

            float woodTone = 0.54f + (logRand - 0.5f) * 0.08f + (ringWave - 0.5f) * 0.16f + (fiber - 0.5f) * 0.08f;
            woodTone = lerpf(woodTone, 0.38f, knotRing * 0.55f);
            woodTone = lerpf(woodTone, 0.24f, knotCore * 0.78f);
            woodTone *= (1.0f - crack * 0.45f) * (0.64f + 0.36f * cyl) * lerpf(1.0f, 0.84f, steamPatina * 0.50f);

            float wr = woodTone * 1.12f;
            float wg = woodTone * 0.76f;
            float wb = woodTone * 0.41f;

            float jt = 0.30f + juteTwist * 0.12f + (juteFibers - 0.5f) * 0.10f;
            float mr = jt * 1.15f;
            float mg = jt * 0.88f;
            float mb = jt * 0.45f;

            float r = lerpf(wr, mr, seamMask);
            float g = lerpf(wg, mg, seamMask);
            float b = lerpf(wb, mb, seamMask);

            float hWood = 0.22f + cyl * 0.62f + ringWave * 0.07f + fiber * 0.05f - crack * 0.13f;
            float hSeam = 0.10f + juteTwist * 0.09f + juteFibers * 0.04f;
            float h = lerpf(hWood, hSeam, seamMask);

            float roughWood = clamp01(0.64f - ringWave * 0.10f - steamPatina * 0.34f);
            float rough = lerpf(roughWood, 0.94f, seamMask);
            float ao = clamp01((0.58f + 0.42f * cyl) * lerpf(1.0f, 0.45f, seamMask));

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
    applyCavityAO(hf, S, 0.30f, out.diff);
    heightToNormal(hf, S, 2.5f, out.norm);
    return out;
}

// 9. BanyaRooster — переливающиеся медно-золотистые ланцетовидные перья Кочета Смотрящего
PbrSet genBanyaRooster4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    const int cols = 10, rows = 14;
    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            float bestScore = -1e9f;
            float bestRx = 0.0f, bestPx = 0.0f, bestPy = 0.5f, bestEdge = 0.35f, bestSeed = 0.5f;
            int baseRow = static_cast<int>(std::floor(v * rows));

            for (int dr = -3; dr <= 0; ++dr) {
                int rIdx = baseRow + dr;
                int wr = ((rIdx % rows) + rows) % rows;
                float shift = (wr & 1) ? 0.5f : 0.0f;
                float uCol = u * cols + shift;
                int baseCol = static_cast<int>(std::floor(uCol));
                for (int dc = -1; dc <= 1; ++dc) {
                    int cIdx = baseCol + dc;
                    int wc = ((cIdx % cols) + cols) % cols;
                    float jx = (hash01(wc, wr, cols, rows, 907u) - 0.5f) * 0.18f;
                    float jy = (hash01(wc, wr, cols, rows, 911u) - 0.5f) * 0.11f;
                    float tilt = (hash01(wc, wr, cols, rows, 919u) - 0.5f) * 0.16f;

                    float cx = ((uCol - cIdx) * 2.0f - 1.0f) + jx;
                    float cy = (v * rows - rIdx + jy + 0.15f) / 2.25f;
                    if (cy < 0.0f || cy > 1.0f) continue;

                    float rx = cx * std::cos(tilt) - (cy - 0.45f) * std::sin(tilt);
                    float widthCurve = (cy < 0.24f)
                        ? 1.15f
                        : 1.15f * std::pow(std::max(0.0f, 1.0f - (cy - 0.24f) / 0.76f), 0.68f);
                    float distNorm = std::fabs(rx) / std::max(0.03f, widthCurve);
                    if (distNorm <= 1.0f) {
                        float score = -float(rIdx) + (1.0f - distNorm) * 0.05f;
                        if (score > bestScore) {
                            bestScore = score;
                            bestRx = rx;
                            bestPx = rx / std::max(0.08f, widthCurve);
                            bestPy = cy;
                            bestEdge = clamp01(1.0f - distNorm);
                            bestSeed = hash01(wc, wr, cols, rows, 929u);
                        }
                    }
                }
            }

            // Прямые V-бородки пера от центрального стержня
            float ax = std::fabs(bestRx);
            float shaft = std::exp(-bestPx * bestPx * 145.0f) * smoothstep(0.93f, 0.12f, bestPy);
            float barbs = 0.5f + 0.5f * std::sin((ax * 15.0f - bestPy * 18.0f + bestSeed * 2.0f) * TAU);
            float microBarbs = 0.5f + 0.5f * std::sin((ax * 52.0f - bestPy * 58.0f) * TAU);
            float fluff = tileFbm(u, v, 24, 3, 937u);

            // Благородная палитра: бронзово-медное основание -> золотисто-огненный гребень -> тёмно-медный кант
            float rimBand = smoothstep(0.38f, 0.0f, bestEdge) * smoothstep(0.10f, 0.45f, bestPy);
            float tipDark = smoothstep(0.65f, 0.99f, bestPy);

            float coreR = lerpf(0.96f, 0.88f, bestSeed);
            float coreG = lerpf(0.72f, 0.54f, bestSeed);
            float coreB = lerpf(0.28f, 0.16f, bestSeed);

            float rimR  = lerpf(0.68f, 0.52f, bestSeed);
            float rimG  = lerpf(0.24f, 0.14f, bestSeed);
            float rimB  = lerpf(0.07f, 0.04f, bestSeed);

            float mixVal = clamp01(rimBand * 0.72f + tipDark * 0.48f);
            float r = lerpf(coreR, rimR, mixVal);
            float g = lerpf(coreG, rimG, mixVal);
            float b = lerpf(coreB, rimB, mixVal);

            float barbShade = 0.88f + barbs * 0.10f + microBarbs * 0.04f + (fluff - 0.5f) * 0.04f;
            r *= barbShade; g *= barbShade; b *= barbShade;

            // Золотистый блестящий стержень пера
            r = lerpf(r, 1.00f, shaft * 0.52f);
            g = lerpf(g, 0.88f, shaft * 0.52f);
            b = lerpf(b, 0.56f, shaft * 0.52f);

            float edgeShadow = 0.66f + 0.34f * smoothstep(0.0f, 0.32f, bestEdge);
            r *= edgeShadow; g *= edgeShadow; b *= edgeShadow;

            float h = 0.24f + 0.45f * smoothstep(0.0f, 0.48f, bestEdge) * (1.0f - bestPy * 0.20f)
                      + shaft * 0.20f + barbs * 0.07f + microBarbs * 0.03f;
            float rough = clamp01(0.36f - shaft * 0.16f + (1.0f - barbs) * 0.12f);
            float metal = lerpf(0.08f, 0.18f, (1.0f - mixVal) * barbs);
            float ao = clamp01(edgeShadow);

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
    heightToNormal(hf, S, 2.0f, out.norm);
    return out;
}

// 10. BanyaPlank — распаренная липовая вагонка и доски полка с фасками и каплями влаги
PbrSet genBanyaPlank4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    const int numPlanks = 6;
    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        float plankPos = v * numPlanks;
        int pIdx = static_cast<int>(std::floor(plankPos)) % numPlanks;
        float py = plankPos - std::floor(plankPos);
        float cy = py - 0.5f;
        float edgeDist = std::fabs(cy) * 2.0f;

        // Шпунтовая доска: глубокий аккуратный паз и фаска
        float groove = 1.0f - smoothstep(0.84f, 0.96f, edgeDist);
        float bevel  = 1.0f - smoothstep(0.76f, 0.89f, edgeDist);
        float pRand  = hash01(pIdx, 5, numPlanks, 16, 1009u);

        // Тёплый оттенок каждой планки липы/абаши (медовый -> кремово-липовый)
        float toneShift = (pRand - 0.5f) * 0.14f;

        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;
            float uShift = std::fmod(u + pRand * 0.73f, 1.0f);

            // Вертикальный стык досок (разбежка по длине)
            float seamU = std::fmod(0.27f + pRand * 0.61f, 1.0f);
            float duSeam = std::fabs(std::fmod(u - seamU + 1.5f, 1.0f) - 0.5f);
            float endJoint = smoothstep(0.008f, 0.002f, duSeam);

            // Шелковистая прямослойная липа + тонкие годовые кольца
            float warp = tileFbmXY(uShift, v * 1.5f, 3, 12, 3, 1013u + static_cast<uint32_t>(pIdx) * 19u);
            float ring = 0.5f + 0.5f * std::sin((cy * 5.5f + (warp - 0.5f) * 2.1f) * TAU + pRand * TAU);
            ring = std::pow(ring, 0.65f);
            float fiber = tileFbmXY(uShift, v, 10, 140, 3, 1019u);
            float microPores = tileFbmXY(uShift, v, 36, 220, 2, 1021u);

            // Мокрые пятна брызг воды и конденсата на полке (резко снижают roughness!)
            float wetBlob = smoothstep(0.56f, 0.72f, tileWarpedFbm(u, v, 5, 4, 0.06f, 1031u));
            float wetDroplet = smoothstep(0.68f, 0.82f, tileFbm(u, v, 24, 3, 1033u));
            float wetMask = clamp01(wetBlob * 0.80f + wetDroplet * 0.35f);

            float baseT = 0.64f + toneShift + (ring - 0.5f) * 0.13f + (fiber - 0.5f) * 0.07f;
            baseT *= lerpf(1.0f, 0.72f, wetMask * 0.65f); // мокрое дерево темнее и глубже!
            baseT *= lerpf(0.28f, 1.0f, groove) * (1.0f - endJoint * 0.68f);

            float r = baseT * 1.08f;
            float g = baseT * 0.84f;
            float b = baseT * 0.56f;

            float h = 0.25f + groove * 0.45f + bevel * 0.12f + ring * 0.04f + fiber * 0.03f - microPores * 0.02f - endJoint * 0.30f;
            float rough = lerpf(0.60f - ring * 0.08f, 0.14f, wetMask);
            rough = lerpf(0.92f, rough, groove);

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(clamp01(rough));
            out.roughMetal[idx + 1] = toByte(0.0f);
            out.roughMetal[idx + 2] = toByte(clamp01(0.45f + 0.55f * groove));
        }
    });
    applyCavityAO(hf, S, 0.32f, out.diff);
    heightToNormal(hf, S, 2.4f, out.norm);
    return out;
}

// 11. BanyaStone — колотый банный габбро-диабаз и жадеит (натуральные сколы породы без «панцирной сетки»)
PbrSet genBanyaStone4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            // Органические деформации координат для рваных природных сколов породы
            float wu = u + (tileFbm(u, v, 5, 4, 1103u) - 0.5f) * 0.14f;
            float wv = v + (tileFbm(u + 0.37f, v + 0.21f, 5, 4, 1109u) - 0.5f) * 0.14f;
            float w1_f1, w1_f2, w2_f1, w2_f2;
            tileWorley2(wu, wv, 5,  1117u, w1_f1, w1_f2);
            tileWorley2(wu + 0.19f, wv + 0.43f, 11, 1123u, w2_f1, w2_f2);

            // Разновысотные сколотые плоскости (бесшовный перепад рельефа без чёрных обводок)
            float bevel1 = clamp01((w1_f2 - w1_f1) * 3.8f);
            float bevel2 = clamp01((w2_f2 - w2_f1) * 4.5f);
            float ridge  = bevel1 * 0.65f + bevel2 * 0.35f;

            float rockMacro = tileWarpedFbm(u, v, 6, 5, 0.11f, 1129u);
            float rockDetail = tileFbm(u, v, 24, 4, 1141u);
            float microGrain = tileGradNoise(u, v, 160, 1151u);

            // Редкая природная трещина только по напряжённым участкам
            float stressMask = smoothstep(0.58f, 0.80f, tileFbm(u, v, 4, 3, 1171u));
            float rareCrack  = (1.0f - smoothstep(0.0f, 0.028f, w1_f2 - w1_f1)) * stressMask * 0.65f;

            // Прожилки нефрита/жадеита и кристаллы белого кварца + искры слюды
            float veinBand = smoothstep(0.62f, 0.80f, tileWarpedFbm(u, v, 3, 4, 0.16f, 1153u));
            float quartzVein = (1.0f - smoothstep(0.0f, 0.022f, std::fabs(tileWarpedFbm(u, v, 4, 4, 0.14f, 1163u) - 0.50f)))
                             * smoothstep(0.45f, 0.75f, rockMacro);
            float micaSpark = smoothstep(0.76f, 0.90f, microGrain) * smoothstep(0.48f, 0.70f, rockDetail);

            // Натуральный глубокий серый габбро-диабаз с тёплым минеральным оттенком
            float baseTone = 0.21f + rockMacro * 0.17f + rockDetail * 0.10f + bevel1 * 0.07f + (microGrain - 0.5f) * 0.06f;
            float r = baseTone * 0.97f;
            float g = baseTone * 1.00f;
            float b = baseTone * 1.03f;

            // Вкрапления зелёного банного жадеита
            r = lerpf(r, 0.15f, veinBand * 0.45f);
            g = lerpf(g, 0.28f, veinBand * 0.45f);
            b = lerpf(b, 0.20f, veinBand * 0.45f);

            // Светлая кварцевая жилка и слюда
            r = lerpf(r, 0.54f, quartzVein * 0.52f) + micaSpark * 0.12f;
            g = lerpf(g, 0.55f, quartzVein * 0.52f) + micaSpark * 0.12f;
            b = lerpf(b, 0.51f, quartzVein * 0.52f) + micaSpark * 0.12f;

            r *= (1.0f - rareCrack * 0.55f);
            g *= (1.0f - rareCrack * 0.55f);
            b *= (1.0f - rareCrack * 0.55f);

            float h = 0.22f + ridge * 0.34f + rockMacro * 0.24f + rockDetail * 0.12f + (microGrain - 0.5f) * 0.05f - rareCrack * 0.18f;
            float rough = clamp01(0.76f - micaSpark * 0.34f - quartzVein * 0.20f - bevel1 * 0.10f + rareCrack * 0.20f);
            float metal = clamp01(micaSpark * 0.30f);

            hf[static_cast<size_t>(y) * S + x] = h;
            size_t idx = (static_cast<size_t>(y) * S + x) * 3;
            out.diff[idx + 0] = toByte(r);
            out.diff[idx + 1] = toByte(g);
            out.diff[idx + 2] = toByte(b);
            out.roughMetal[idx + 0] = toByte(rough);
            out.roughMetal[idx + 1] = toByte(metal);
            out.roughMetal[idx + 2] = toByte(clamp01(0.65f + 0.35f * ridge));
        }
    });
    applyCavityAO(hf, S, 0.28f, out.diff);
    heightToNormal(hf, S, 2.7f, out.norm);
    return out;
}

// 12. BanyaCopper — полированная и деликатно чеканная красная медь/бронза (чистый металл без пятен-горошка)
PbrSet genBanyaCopper4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            // Мягкие широкие грани ручной выколотки + тонкая круговая/продольная шлифовка
            float w1_f1, w1_f2;
            tileWorley2(u + 0.03f * tileFbm(u, v, 6, 2, 1201u), v + 0.03f * tileFbm(u + 0.2f, v, 6, 2, 1207u), 9, 1213u, w1_f1, w1_f2);
            float hammerFacet = clamp01((w1_f2 - w1_f1) * 1.8f); // плавные грани выколотки в нормалях!
            float dish = clamp01(1.0f - w1_f1 * 0.95f);

            float brush = tileFbmXY(u, v, 140, 8, 3, 1223u);
            float fineSpin = 0.5f + 0.5f * std::sin((v * 56.0f + (tileFbm(u, v, 12, 2, 1229u) - 0.5f) * 1.4f) * TAU);
            float warmWear = tileWarpedFbm(u, v, 4, 3, 0.08f, 1237u);

            // Настоящий физический альбедо-цвет полированной меди (высокий R, средний G, тёплый B)
            float shadeMod = 0.92f + (brush - 0.5f) * 0.05f + (warmWear - 0.5f) * 0.06f;
            float r = clamp01(0.95f * shadeMod);
            float g = clamp01(0.62f * shadeMod);
            float b = clamp01(0.45f * shadeMod);

            // Рельеф мягкой ручной выколотки + микроцарапины полировки
            float h = 0.52f + hammerFacet * 0.14f - dish * 0.09f + (brush - 0.5f) * 0.05f + (fineSpin - 0.5f) * 0.025f;
            // Roughness от 0.17 (зеркальный полированный блик) до 0.34 (слегка матовая медь в патине)
            float rough = clamp01(0.18f + (1.0f - hammerFacet) * 0.08f + (1.0f - brush) * 0.06f + warmWear * 0.05f);
            float metal = 0.96f;

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
    applyCavityAO(hf, S, 0.10f, out.diff);
    heightToNormal(hf, S, 1.45f, out.norm);
    return out;
}

// 13. BanyaBirch — пышный берёзовый веник (перекрывающиеся резные листья с жилками и каплями пара)
PbrSet genBanyaBirch4K(int S) {
    PbrSet out;
    out.diff.resize(static_cast<size_t>(S) * S * 3);
    out.roughMetal.resize(static_cast<size_t>(S) * S * 3);
    std::vector<float> hf(static_cast<size_t>(S) * S);

    const int cols = 8, rows = 8;
    parallelRows(S, [&](int y) {
        float v = float(y) / S;
        for (int x = 0; x < S; ++x) {
            float u = float(x) / S;

            float bestLayer = -1e9f;
            float bestDist = 1.0f, bestLx = 0.0f, bestLy = 0.5f, bestSeed = 0.5f;
            int baseRow = static_cast<int>(std::floor(v * rows));

            for (int dr = -2; dr <= 1; ++dr) {
                int rIdx = baseRow + dr;
                int wr = ((rIdx % rows) + rows) % rows;
                float uCol = u * cols + ((wr & 1) ? 0.5f : 0.0f);
                int baseCol = static_cast<int>(std::floor(uCol));
                for (int dc = -1; dc <= 1; ++dc) {
                    int cIdx = baseCol + dc;
                    int wc = ((cIdx % cols) + cols) % cols;
                    float jx = (hash01(wc, wr, cols, rows, 1301u) - 0.5f) * 0.45f;
                    float jy = (hash01(wc, wr, cols, rows, 1303u) - 0.5f) * 0.45f;
                    float ang = (hash01(wc, wr, cols, rows, 1307u) - 0.5f) * 2.2f;
                    float sc  = 0.85f + hash01(wc, wr, cols, rows, 1319u) * 0.45f;

                    float dx = (uCol - cIdx - 0.5f + jx) * sc;
                    float dy = (v * rows - rIdx - 0.5f + jy) * sc;
                    float rx = dx * std::cos(ang) - dy * std::sin(ang);
                    float ry = dx * std::sin(ang) + dy * std::cos(ang);

                    // Сердцевидно-яйцевидный силуэт берёзового листа с зубчатым краем
                    float ly = ry * 0.65f + 0.5f; // 0..1 от основания к кончику
                    if (ly < 0.02f || ly > 0.98f) continue;
                    float wCurve = std::sin(std::pow(ly, 0.72f) * PI) * 0.68f;
                    float serrate = 0.045f * std::sin(ly * 42.0f);
                    float dNorm = std::fabs(rx) / std::max(0.03f, wCurve + serrate);
                    if (dNorm <= 1.0f) {
                        float layer = hash01(wc, wr, cols, rows, 1321u) + (1.0f - dNorm) * 0.05f;
                        if (layer > bestLayer) {
                            bestLayer = layer;
                            bestDist = dNorm;
                            bestLx = rx;
                            bestLy = ly;
                            bestSeed = hash01(wc, wr, cols, rows, 1327u);
                        }
                    }
                }
            }

            float veinNoise = tileFbm(u, v, 16, 3, 1361u);
            if (bestLayer < -1e8f) {
                // Тёмная глубина между листьями и берёзовые веточки
                float twigs = smoothstep(0.72f, 0.86f, tileFbmXY(u, v, 12, 48, 2, 1367u));
                float r = lerpf(0.08f, 0.24f, twigs);
                float g = lerpf(0.11f, 0.19f, twigs);
                float b = lerpf(0.05f, 0.12f, twigs);
                hf[static_cast<size_t>(y) * S + x] = 0.10f + twigs * 0.12f;
                size_t idx = (static_cast<size_t>(y) * S + x) * 3;
                out.diff[idx + 0] = toByte(r);
                out.diff[idx + 1] = toByte(g);
                out.diff[idx + 2] = toByte(b);
                out.roughMetal[idx + 0] = toByte(0.78f);
                out.roughMetal[idx + 1] = toByte(0.0f);
                out.roughMetal[idx + 2] = toByte(0.35f);
            } else {
                // Центральная и боковые жилки берёзового листа
                float midRib = std::exp(-std::fabs(bestLx) * 38.0f) * smoothstep(0.95f, 0.10f, bestLy);
                float sideRib = std::pow(0.5f + 0.5f * std::sin((std::fabs(bestLx) * 18.0f - bestLy * 14.0f) * TAU), 4.0f) * (1.0f - bestDist * 0.5f);
                float edgeRim = smoothstep(1.0f, 0.72f, bestDist);

                // Богатая палитра запаренного берёзового веника (оливково-изумрудные и молодые листья)
                float r = lerpf(0.16f, 0.28f, bestSeed) + midRib * 0.16f + sideRib * 0.07f + (veinNoise - 0.5f) * 0.05f;
                float g = lerpf(0.36f, 0.52f, bestSeed) + midRib * 0.18f + sideRib * 0.10f + (veinNoise - 0.5f) * 0.06f;
                float b = lerpf(0.09f, 0.16f, bestSeed) + midRib * 0.06f;

                float shade = 0.70f + 0.30f * edgeRim;
                r *= shade; g *= shade; b *= shade;

                float h = 0.35f + 0.42f * edgeRim + midRib * 0.12f + sideRib * 0.05f + bestSeed * 0.06f;
                float dew = smoothstep(0.78f, 0.90f, tileFbm(u, v, 28, 2, 1373u));
                float rough = clamp01(lerpf(0.42f, 0.14f, dew));

                hf[static_cast<size_t>(y) * S + x] = h;
                size_t idx = (static_cast<size_t>(y) * S + x) * 3;
                out.diff[idx + 0] = toByte(r);
                out.diff[idx + 1] = toByte(g);
                out.diff[idx + 2] = toByte(b);
                out.roughMetal[idx + 0] = toByte(rough);
                out.roughMetal[idx + 1] = toByte(0.0f);
                out.roughMetal[idx + 2] = toByte(clamp01(shade));
            }
        }
    });
    applyCavityAO(hf, S, 0.30f, out.diff);
    heightToNormal(hf, S, 2.3f, out.norm);
    return out;
}

// ============================================================================
// Бесшовный 6-гранный 360° Skybox + IBL Cubemap (сумеречный кибер-закат + многослойный мегаполис)
// ============================================================================
void evalSkyDir(float dx, float dy, float dz, float& outR, float& outG, float& outB) {
    const float sx = 0.58f, sy = 0.44f, sz = 0.54f;
    const float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
    const float sunX = sx / sl, sunY = sy / sl, sunZ = sz / sl;

    float sunDot = std::max(0.0f, dx * sunX + dy * sunY + dz * sunZ);
    float up = clamp01(dy);
    float horizon = std::exp(-std::fabs(dy) * 5.6f);

    // Богатый сумеречный градиент без провала в мёртвый чёрный низ
    float r = lerpf(0.26f, 0.030f, std::pow(up, 0.55f));
    float g = lerpf(0.20f, 0.052f, std::pow(up, 0.62f));
    float b = lerpf(0.36f, 0.145f, std::pow(up, 0.48f));
    if (dy < 0.0f) {
        float dn = clamp01(-dy * 2.5f);
        r = lerpf(0.11f, 0.038f, dn);
        g = lerpf(0.12f, 0.048f, dn);
        b = lerpf(0.19f, 0.085f, dn);
    }

    // Солнечное рассеяние Ми
    float glow = std::pow(sunDot, 5.5f) * (0.42f + 0.58f * horizon);
    r += glow * 0.78f + horizon * 0.24f * std::pow(sunDot, 1.5f);
    g += glow * 0.42f + horizon * 0.10f * std::pow(sunDot, 1.5f);
    b += glow * 0.14f;

    float sunDisc = smoothstep(0.9968f, 0.9993f, sunDot);
    float corona = std::pow(sunDot, 56.0f) * 0.52f;
    r += corona * 1.0f + sunDisc * 1.15f;
    g += corona * 0.76f + sunDisc * 0.95f;
    b += corona * 0.44f + sunDisc * 0.72f;

    // Многослойные объёмные облака с тёплой подсветкой со стороны солнца и тенью снизу
    if (dy > 0.015f) {
        float px = dx / (dy + 0.24f) * 2.1f;
        float pz = dz / (dy + 0.24f) * 2.1f;
        float clBase = fbm3D(px, dy * 1.6f, pz, 5, 777u);
        float clDetail = fbm3D(px * 2.4f + 3.1f, dy * 2.2f, pz * 2.4f - 1.7f, 3, 789u);
        float cl = clBase * 0.78f + clDetail * 0.22f;
        float cloudMask = smoothstep(0.49f, 0.74f, cl) * smoothstep(0.015f, 0.16f, dy);
        float sunEdge = smoothstep(0.48f, 0.62f, clBase) * (1.0f - smoothstep(0.62f, 0.78f, clBase));
        float lit = 0.28f + 0.72f * std::pow(sunDot, 1.9f);
        float cr = lerpf(0.16f, 0.98f, lit) + sunEdge * std::pow(sunDot, 2.0f) * 0.25f;
        float cg = lerpf(0.18f, 0.66f, lit) + sunEdge * std::pow(sunDot, 2.0f) * 0.16f;
        float cb = lerpf(0.29f, 0.52f, lit);
        r = lerpf(r, cr, cloudMask * 0.82f);
        g = lerpf(g, cg, cloudMask * 0.82f);
        b = lerpf(b, cb, cloudMask * 0.82f);
    }

    // Точечные звёзды в зените
    if (dy > 0.20f) {
        float st = noise3D(dx * 210.0f, dy * 210.0f, dz * 210.0f, 999u);
        if (st > 0.972f) {
            float br = smoothstep(0.972f, 0.996f, st) * smoothstep(0.20f, 0.60f, dy) * (1.0f - sunDot * 0.85f);
            r += br * 0.78f; g += br * 0.84f; b += br * 0.96f;
        }
    }

    // Двухуровневый силуэт кибер-мегаполиса со структурными этажами окон и атмосферной дымкой
    if (dy > -0.02f && dy < 0.15f) {
        float az = (std::atan2(dz, dx) + PI) / TAU; // 0..1 бесшовно

        // Дальний слой башен в дымке
        float farCols = 110.0f;
        int fIdx = static_cast<int>(std::floor(az * farCols)) % static_cast<int>(farCols);
        float farH = 0.015f + 0.105f * std::pow(hash01(fIdx, 1, static_cast<int>(farCols), 4, 4111u), 1.9f);
        if (dy > 0.0f && dy < farH) {
            r = lerpf(0.065f, r, 0.42f);
            g = lerpf(0.068f, g, 0.42f);
            b = lerpf(0.115f, b, 0.42f);
        }

        // Ближний архитектурный силуэт башен со ступенчатыми фасадами и аккуратными световыми этажами
        float nearCols = 72.0f;
        float posN = az * nearCols;
        int nIdx = static_cast<int>(std::floor(posN)) % static_cast<int>(nearCols);
        float inTower = posN - std::floor(posN);
        float gap = hash01(nIdx, 2, static_cast<int>(nearCols), 4, 4242u);
        if (gap > 0.22f && inTower > 0.06f && inTower < 0.94f) {
            float bldgH = 0.012f + 0.088f * std::pow(hash01(nIdx, 5, static_cast<int>(nearCols), 4, 4337u), 1.5f);
            // Ступенчатая крыша
            if (inTower < 0.22f || inTower > 0.78f) bldgH *= 0.84f;
            if (dy > 0.0f && dy < bldgH) {
                float br = 0.032f, bg = 0.036f, bb = 0.058f;
                // Аккуратные горизонтальные этажи и окна внутри башни
                float floorY = dy * 280.0f;
                int floorNum = static_cast<int>(std::floor(floorY));
                float fy = floorY - floorNum;
                float floorActive = hash01(nIdx, floorNum, static_cast<int>(nearCols), 64, 4519u);
                float colX = inTower * 6.0f;
                int winCol = static_cast<int>(std::floor(colX));
                float wx = colX - winCol;
                float winActive = hash01(nIdx * 8 + winCol, floorNum, static_cast<int>(nearCols) * 8, 64, 4673u);
                if (dy > 0.004f && dy < bldgH - 0.005f && fy > 0.30f && fy < 0.68f && wx > 0.20f && wx < 0.78f && floorActive > 0.42f && winActive > 0.56f) {
                    bool cyan = (hash01(nIdx, floorNum, static_cast<int>(nearCols), 64, 4799u) > 0.68f);
                    br = cyan ? 0.18f : 0.78f;
                    bg = cyan ? 0.72f : 0.56f;
                    bb = cyan ? 0.92f : 0.22f;
                }
                float haze = clamp01(0.18f + 0.35f * (1.0f - smoothstep(0.0f, bldgH, dy)));
                r = lerpf(br, r, haze * 0.25f);
                g = lerpf(bg, g, haze * 0.25f);
                b = lerpf(bb, b, haze * 0.25f);
            }
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
                    case 0: dx =  1.0f; dy = -t;    dz = -s;    break;
                    case 1: dx = -1.0f; dy = -t;    dz =  s;    break;
                    case 2: dx =  s;    dy =  1.0f; dz =  t;    break;
                    case 3: dx =  s;    dy = -1.0f; dz = -t;    break;
                    case 4: dx =  s;    dy = -t;    dz =  1.0f; break;
                    case 5: dx = -s;    dy = -t;    dz = -1.0f; break;
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
// Сдержанный кинематографичный UHD фон загрузки (без дешёвых лучей-вееров)
// ============================================================================
void genLoadingArt4K(const std::string& outDir, int W, int H) {
    std::vector<uint8_t> rgb(static_cast<size_t>(W) * H * 3);
    parallelRows(H, [&](int y) {
        float v = float(y) / float(H);
        for (int x = 0; x < W; ++x) {
            float u = float(x) / float(W);
            float cx = (u - 0.5f) * (float(W) / float(H));
            float cy = v - 0.55f;

            // Глубокий атмосферный ночной пейзаж арены с плавным закатным заревом и дымкой (без чёрных полос)
            float mist = tileWarpedFbm(u, v, 4, 4, 0.08f, 991u);
            float clouds = smoothstep(0.48f, 0.76f, tileFbm(u * 1.5f, v * 2.2f, 5, 4, 997u)) * smoothstep(0.58f, 0.20f, v);
            float hor = std::exp(-std::fabs(v - 0.58f) * 8.0f);
            float coreGlow = std::exp(-(cx * cx * 1.15f + cy * cy * 5.6f));

            float r = 0.024f + mist * 0.030f + hor * 0.15f + coreGlow * 0.18f + clouds * 0.038f;
            float g = 0.034f + mist * 0.038f + hor * 0.10f + coreGlow * 0.13f + clouds * 0.032f;
            float b = 0.062f + mist * 0.056f + hor * 0.17f + coreGlow * 0.18f + clouds * 0.056f;

            // Силуэт дальнего кибер-мегаполиса строго на горизонте (до v=0.58)
            if (v > 0.45f && v <= 0.58f) {
                float colF = u * 64.0f;
                int cIdx = static_cast<int>(std::floor(colF));
                float bh = 0.015f + 0.095f * std::pow(hash01(cIdx, 3, 64, 4, 5119u), 1.6f);
                if (0.58f - v < bh) {
                    r = lerpf(r, 0.018f, 0.80f);
                    g = lerpf(g, 0.022f, 0.80f);
                    b = lerpf(b, 0.038f, 0.80f);
                }
            }

            // Лёгкая тонкая тактическая сетка перспективы ниже горизонта
            if (v > 0.58f) {
                float p = (v - 0.58f) / 0.42f;
                float z = 1.0f / std::max(0.04f, p);
                float gx = std::fabs(std::fmod(std::fabs(cx * z * 1.4f) + 0.5f, 1.0f) - 0.5f);
                float gz = std::fabs(std::fmod(z * 1.2f, 1.0f) - 0.5f);
                float line = std::max(smoothstep(0.018f, 0.002f, gx), smoothstep(0.024f, 0.003f, gz));
                float fade = smoothstep(0.0f, 0.25f, p) * (1.0f - p * 0.55f) * 0.45f;
                r += line * fade * 0.14f;
                g += line * fade * 0.58f;
                b += line * fade * 0.78f;
            }

            // Мягкая оптическая виньетка
            float vig = clamp01(1.10f - (cx * cx * 0.55f + (v - 0.5f) * (v - 0.5f) * 1.25f));
            size_t idx = (static_cast<size_t>(y) * W + x) * 3;
            rgb[idx + 0] = toByte(r * vig);
            rgb[idx + 1] = toByte(g * vig);
            rgb[idx + 2] = toByte(b * vig);
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
    saveSet("BanyaPlank",   S,         genBanyaPlank4K(S));
    saveSet("BanyaStone",   S,         genBanyaStone4K(S));
    saveSet("BanyaCopper",  smallSize, genBanyaCopper4K(smallSize));
    saveSet("BanyaBirch",   smallSize, genBanyaBirch4K(smallSize));

    int cubeS = 1024;
    genSkyCubemap(outDir, cubeS);
    writeSkyboxXml(outDir);
    std::printf("  [OK] Skybox4K       (6x %dx%d seamless 360 cubemap + IBL)\n", cubeS, cubeS);

    int loadW = (S >= 2048) ? 3840 : 1920;
    int loadH = (S >= 2048) ? 2160 : 1080;
    genLoadingArt4K(outDir, loadW, loadH);
    std::printf("  [OK] LoadingBg4K    (%dx%d UHD loading backdrop)\n", loadW, loadH);

    if (FILE* mf = std::fopen((outDir + "/.4k_pbr_stamp").c_str(), "wb")) {
        std::fprintf(mf, "%d\n", S);
        std::fclose(mf);
    }
    std::printf("[4K PBR Generator] Done!\n");
    return 0;
}

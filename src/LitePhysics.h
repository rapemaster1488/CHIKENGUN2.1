#pragma once
// ============================================================================
// LitePhysics.h — Лёгкое ядро коллизий и рейкастов без зависимости от Bullet.
// Используется напрямую как в Urho3D-клиенте (src/MyApp.cpp), так и в
// автономных C++17 модульных тестах (tests/test_core.cpp).
// ============================================================================

#include <cmath>
#include <cstdint>

namespace Lite {

template <typename Vec3>
struct ColliderT {
    Vec3 center;
    Vec3 half;        // полуразмеры AABB (или радиус в x_ для сферы)
    bool sphere = false;
    int  tag    = 0;  // 0 static, 1 target, 2 enemy, 4 moving target, 7 removed
    void* ptr   = nullptr;
};

template <typename Vec3>
inline float Dot3(const Vec3& a, const Vec3& b) {
    return a.x_ * b.x_ + a.y_ * b.y_ + a.z_ * b.z_;
}

template <typename Vec3>
inline float RaySphereT(const Vec3& o, const Vec3& d, const Vec3& c, float r) {
    // Полная квадратичная форма: работает и для ненормализованного луча,
    // и для старта внутри сферы (нужен дальний корень выхода).
    const float a = Dot3(d, d);
    if (a < 1e-12f || r < 0.0f) return -1.0f;
    Vec3 oc(o.x_ - c.x_, o.y_ - c.y_, o.z_ - c.z_);
    const float b = Dot3(oc, d);
    const float cc = Dot3(oc, oc) - r * r;
    const float disc = b * b - a * cc;
    if (disc < 0.0f) return -1.0f;
    const float root = std::sqrt(disc);
    const float nearT = (-b - root) / a;
    if (nearT >= 0.0f) return nearT;
    const float farT = (-b + root) / a;
    return farT >= 0.0f ? farT : -1.0f;
}

template <typename Vec3>
inline float RayBoxT(const Vec3& o, const Vec3& d, const Vec3& c, const Vec3& h) {
    // Полный slab-тест. Не обнуляем tmin заранее: если луч начинается
    // внутри AABB, возвращаем дальнюю грань (tmax), как RaySphereT делает
    // с дальним корнем. Это не создаёт ложного попадания в точке старта.
    // Явное обращение к x_/y_/z_ важно: у Urho3D::Vector3 возможно выравнивание.
    struct Axis { float ox, dx, cx, hx; };
    const Axis axes[3] = {
        { o.x_, d.x_, c.x_, h.x_ },
        { o.y_, d.y_, c.y_, h.y_ },
        { o.z_, d.z_, c.z_, h.z_ },
    };
    if (std::fabs(d.x_) < 1e-7f && std::fabs(d.y_) < 1e-7f && std::fabs(d.z_) < 1e-7f)
        return -1.0f;

    float tmin = -1e9f, tmax = 1e9f;
    for (int a = 0; a < 3; ++a) {
        const Axis& A = axes[a];
        if (A.hx < 0.0f) return -1.0f;
        if (std::fabs(A.dx) < 1e-7f) {
            if (A.ox < A.cx - A.hx || A.ox > A.cx + A.hx) return -1.0f;
        } else {
            float inv = 1.0f / A.dx;
            float t1 = (A.cx - A.hx - A.ox) * inv;
            float t2 = (A.cx + A.hx - A.ox) * inv;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return -1.0f;
        }
    }
    if (tmax < 0.0f) return -1.0f;
    return tmin >= 0.0f ? tmin : tmax;
}

template <typename Vec3>
inline Vec3 HitNormalBoxT(const Vec3& point, const Vec3& center, const Vec3& half) {
    float dx = point.x_ - center.x_;
    float dy = point.y_ - center.y_;
    float dz = point.z_ - center.z_;
    float hx = half.x_ > 0.001f ? half.x_ : 0.001f;
    float hy = half.y_ > 0.001f ? half.y_ : 0.001f;
    float hz = half.z_ > 0.001f ? half.z_ : 0.001f;
    float sx = std::fabs(dx) / hx;
    float sy = std::fabs(dy) / hy;
    float sz = std::fabs(dz) / hz;
    if (sx >= sy && sx >= sz) return Vec3(dx > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
    if (sy >= sx && sy >= sz) return Vec3(0.0f, dy > 0.0f ? 1.0f : -1.0f, 0.0f);
    return Vec3(0.0f, 0.0f, dz > 0.0f ? 1.0f : -1.0f);
}

// Кинематика энергетического грэппл-крюка: возвращает вектор скорости притяжения к якорю
// и выставляет shouldRelease = true при достижении дистанции отцепления.
template <typename Vec3>
inline Vec3 ComputeGrapplePullT(const Vec3& playerPos, const Vec3& anchorPos,
                                float pullSpeed, float releaseDist, bool& shouldRelease) {
    float dx = anchorPos.x_ - playerPos.x_;
    float dy = anchorPos.y_ - playerPos.y_;
    float dz = anchorPos.z_ - playerPos.z_;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist <= releaseDist || dist < 1e-4f) {
        shouldRelease = true;
        return Vec3(0.0f, 6.2f, 0.0f); // мягкий вертикальный подброс на уступ при отцеплении
    }
    shouldRelease = false;
    float inv = pullSpeed / dist;
    return Vec3(dx * inv, dy * inv + 1.8f, dz * inv);
}

// FNV-1a 64-bit хеш для кэширования процедурных материалов без утечек памяти
inline uint64_t HashMixBytes(uint64_t h, const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(p[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

inline uint64_t HashFloatQ(uint64_t h, float v) {
    int32_t q = static_cast<int32_t>(std::lround(v * 1000.0f));
    return HashMixBytes(h, &q, sizeof(q));
}

#ifdef URHO3D_API
using Collider = ColliderT<Urho3D::Vector3>;
inline float RaySphere(const Urho3D::Vector3& o, const Urho3D::Vector3& d, const Urho3D::Vector3& c, float r) {
    return RaySphereT(o, d, c, r);
}
inline float RayBox(const Urho3D::Vector3& o, const Urho3D::Vector3& d, const Urho3D::Vector3& c, const Urho3D::Vector3& h) {
    return RayBoxT(o, d, c, h);
}
#endif

} // namespace Lite

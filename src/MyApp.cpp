// ============================================================================
// MyApp.cpp - Полноценный FPS шутер на Urho3D
// ============================================================================
// Исправления:
// 1. Исправлено движение камеры (теперь работает корректно)
// 2. Добавлено оружие с моделью и анимацией
// 3. Добавлены интерактивные мишени с физикой
// 4. Улучшено освещение и материалы
// 5. Добавлены частицы, звуки и эффекты
// 6. Добавлен интерфейс (прицел, здоровье, патроны)
// 7. Добавлена система стрельбы с рейкастами
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <unistd.h> // readlink для поиска CoreData рядом с бинарником
#include <sys/stat.h> // stat()/S_ISDIR для проверки папок ресурсов
#include <cstring>    // strlen для программной техники

// ВАЖНО: Urho3D/Math/Vector3.h подключается ДО всего остального, иначе
// заголовки Graphics/Input/UI (через Variant->Color->Vector4->Vector3)
// могут быть раскрыты раньше и Vector3 не будет виден в глобальной области.
#include <Urho3D/Math/Vector3.h>

#include <Urho3D/Engine/Application.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/Core/Timer.h>
#include <Urho3D/Core/Variant.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/GraphicsEvents.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Viewport.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Technique.h> // класс Pass уже определён внутри Technique.h — отдельного Pass.h в этом Urho3D нет
// Texture2D — реальный ресурсный тип (Texture — абстрактная база, в ResourceCache не регистрируется).
// Путь зависит от версии: в 1.8 это Graphics/, в master/fork'ах — GraphicsAPI/.
#if defined(__has_include)
#  if __has_include(<Urho3D/Graphics/Texture2D.h>)
#    include <Urho3D/Graphics/Texture2D.h>
#  elif __has_include(<Urho3D/GraphicsAPI/Texture2D.h>)
#    include <Urho3D/GraphicsAPI/Texture2D.h>
#  endif
#endif
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/ParticleEmitter.h>
#include <Urho3D/Graphics/ParticleEffect.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/DebugRenderer.h>
#include <Urho3D/Graphics/Skybox.h>
#include <Urho3D/Graphics/RenderPath.h>

#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Math/BoundingBox.h>
#include <Urho3D/Math/Vector2.h>
#include <Urho3D/Math/Vector4.h>
#include <Urho3D/Scene/SceneEvents.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Scene/Component.h>
#include <Urho3D/Scene/LogicComponent.h>

#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/ResourceEvents.h>

#include <Urho3D/Input/Input.h>
#include <Urho3D/Input/InputEvents.h>
#include <Urho3D/Input/InputConstants.h>

#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIElement.h>
#include <Urho3D/UI/BorderImage.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/Font.h>

#include <Urho3D/Audio/Audio.h>
#include <Urho3D/Audio/Sound.h>
#include <Urho3D/Audio/SoundSource.h>
#include <Urho3D/Audio/SoundSource3D.h>

// =========================================================================
// ЛЁГКИЙ ФИЗИЧЕСКИЙ ДВИЖОК (без Bullet). Заголовки Urho3D/Physics/* тянут
// Bullet и ломают компиляцию при несовпадении версий ("isVisible ... marked
// override"). Вся физика игры — собственный рейкаст по AABB-коллайдерам.
// =========================================================================
namespace Lite {

using Urho3D::Vector3; // Vector3 живёт в namespace Urho3D (проверено по заголовкам этой сборки)

struct Collider {
    Vector3 center;
    Vector3 half;      // box half-extents (или radius в x_ для sphere)
    bool sphere = false;
    int tag = 0;       // 0 static, 1 target, 2 enemy, 4 moving target, 7 removed
    void* ptr = nullptr;
};

inline float RaySphere(const Vector3& o, const Vector3& d, const Vector3& c, float r) {
    Vector3 oc = o - c;
    float b = oc.DotProduct(d);
    float cc = oc.DotProduct(oc) - r * r;
    float disc = b * b - cc;
    if (disc < 0.0f) return -1.0f;
    float t = -b - sqrtf(disc);
    return t > 0.0f ? t : -1.0f;
}

inline float RayBox(const Vector3& o, const Vector3& d, const Vector3& c, const Vector3& h) {
    // ВАЖНО: у Urho3D::Vector3 члены НЕ лежат подряд (выравнивание/padding),
    // поэтому обращаемся к x_/y_/z_ явно, а не через указатель+индекс.
    struct Axis { float ox, dx, cx, hx; };
    const Axis axes[3] = {
        { o.x_, d.x_, c.x_, h.x_ },
        { o.y_, d.y_, c.y_, h.y_ },
        { o.z_, d.z_, c.z_, h.z_ },
    };
    float tmin = 0.0f, tmax = 1e9f;
    for (int a = 0; a < 3; ++a) {
        const Axis& A = axes[a];
        if (fabsf(A.dx) < 1e-7f) {
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
    return tmin;
}

} // namespace Lite

#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Math/MathDefs.h>
#include <Urho3D/Math/Vector3.h>
#include <Urho3D/Math/Quaternion.h>
#include <Urho3D/Math/Color.h>

#include <Urho3D/Graphics/CustomGeometry.h>
// Technique/Texture2D не инклюдим отдельными заголовками (их нет в этой сборке Urho3D) —
// GetResource<T>() использует только имя ресурса, достаточно ResourceCache.h + Graphics.h
#include <Urho3D/UI/Button.h>

using namespace Urho3D;

// ============================================================================
// Константы и настройки игры
// ============================================================================

namespace GameConstants {
    // Настройки игрока
    constexpr float PLAYER_HEIGHT = 1.7f;         // стоя
    constexpr float PLAYER_HEIGHT_CROUCH = 1.0f;  // присев
    constexpr float CROUCH_TRANSITION_SPEED = 9.0f; // скорость плавного приседания
    constexpr float PLAYER_SPEED = 8.0f;
    constexpr float PLAYER_SPRINT_MULTIPLIER = 1.8f;
    constexpr float PLAYER_CROUCH_MULTIPLIER = 0.5f;   // скорость ползком
    constexpr float MOUSE_SENSITIVITY = 0.15f;
    constexpr float MAX_PITCH = 89.0f;

    // Прыжки / физика игрока
    constexpr float JUMP_IMPULSE = 6.4f;          // начальная вертикальная скорость
    constexpr float GRAVITY = 22.0f;              // "игровая" гравитация (быстрее 9.81 — приятнее в шутере)
    constexpr float COYOTE_TIME = 0.12f;          // сек после схода с края, когда ещё можно прыгнуть
    constexpr float JUMP_BUFFER_TIME = 0.15f;     // сек до приземления, когда нажатие Space засчитывается

    // Динамическая камера от 1-го лица
    constexpr float HEAD_BOB_FREQ = 9.0f;         // частота покачивания головы при ходьбе
    constexpr float HEAD_BOB_AMP = 0.035f;        // амплитуда покачивания (м)
    constexpr float SPRINT_FOV_ADD = 12.0f;       // добавка FOV при беге
    constexpr float ADS_FOV_SUB = 25.0f;          // уменьшение FOV при прицеливании (ПКМ)
    constexpr float LAND_DUCK_AMOUNT = 0.14f;     // просадка камеры при приземлении (м)
    constexpr float LAND_DUCK_RECOVERY = 7.0f;    // скорость восстановления после просадки
    constexpr float FALL_CAM_TILT = 3.0f;         // лёгкий наклон камеры при падении (град/сек скорости)
    constexpr float CAMERA_SHAKE_DECAY = 6.0f;    // затухание тряски от взрывов
    
    // Настройки оружия
    constexpr int MAX_AMMO = 30;
    constexpr int MAX_RESERVE_AMMO = 120;
    constexpr float FIRE_RATE = 0.1f;  // секунды между выстрелами
    constexpr float RELOAD_TIME = 2.0f;
    constexpr float WEAPON_BOB_FREQUENCY = 10.0f;
    constexpr float WEAPON_BOB_AMOUNT = 0.05f;
    constexpr float RECOIL_AMOUNT = 2.0f;
    constexpr float RECOIL_RECOVERY = 5.0f;
    
    // Настройки пуль
    constexpr float BULLET_DAMAGE = 25.0f;
    constexpr float BULLET_RANGE = 1000.0f;
    constexpr float BULLET_SPREAD = 0.02f;
    
    // Настройки мишеней
    constexpr float TARGET_HEALTH = 100.0f;
    constexpr int NUM_TARGETS = 10;
    
    // Настройки уровня
    constexpr int ARENA_SIZE = 48;      // тайлов на сторону (было 50)
    constexpr float TILE_SIZE = 4.0f;   // крупнее тайл -> арена 192x192 м вместо 100x100
    constexpr float FLOOR_TILE_UV_SCALE = 6.0f; // сколько раз тайл-текстура ложится на плиту пола

    // Пол арены — аналитическая плоскость в Lite-физике (см. RaycastLite).
    constexpr float FLOOR_Y = 0.0f;
    constexpr float FLOOR_UV_TILES = 48.0f; // 192 м / 4 м = 48 повторов текстуры на сторону

    // Пикапы: время респавна после подбора (сек) и радиус подбора (м)
    constexpr float PICKUP_RESPAWN = 18.0f;
    constexpr float PICKUP_RADIUS = 1.4f;
    constexpr float PICKUP_HEAL = 25.0f;
}

// ============================================================================
// Структуры данных
// ============================================================================

struct TargetData {
    WeakPtr<Node> node_;
    int colliderIdx_ = -1;   // Lite-коллайдер мишени (tag 1, ptr = Node*)
    float health_;
    bool isActive_;
    int scoreValue_;
    
    TargetData() : health_(GameConstants::TARGET_HEALTH), isActive_(true), scoreValue_(100) {}
};

enum GameMode {
    MODE_RANGE = 0,   // тир с мишенями
    MODE_ARENA,       // бесконечные волны роботов
    MODE_COUNT
};

enum WeaponType {
    W_PISTOL = 0,
    W_SMG,
    W_RIFLE,
    W_SHOTGUN,
    W_SNIPER,
    W_MINIGUN,
    W_LAUNCHER,
    WEAPON_COUNT
};

struct WeaponDef {
    const char* name;
    float damage;
    float fireRate;      // сек между выстрелами
    int   magSize;
    int   reserveMax;
    float spread;        // радианы
    float reloadTime;
    int   pellets;       // дробь
    Color tint;          // базовый цвет модели
};

static const WeaponDef WEAPON_DEFS[WEAPON_COUNT] = {
    { "PISTOL",  34.0f, 0.22f, 12,  96, 0.012f, 1.4f, 1, Color(120,120,130) },
    { "SMG",     18.0f, 0.075f, 30, 180, 0.030f, 2.0f, 1, Color( 90, 95,105) },
    { "RIFLE",   26.0f, 0.10f, 30, 150, 0.018f, 2.4f, 1, Color( 70, 80, 70) },
    // ДРОБОВИК: урон 12->16 за дробину (8*16=128 в упор), разброс 0.060->0.045 — живёт дольше на дистанции
    { "SHOTGUN", 16.0f, 0.85f,  6,  48, 0.045f, 2.8f, 8, Color(100, 70, 40) },
    { "SNIPER", 110.0f,1.20f,   5,  30, 0.002f, 3.2f, 1, Color( 60, 60, 66) },
    { "MINIGUN", 14.0f, 0.045f, 120, 400, 0.045f, 4.2f, 1, Color(110, 85, 60) },
    { "LAUNCHER",150.0f,1.50f,   4,  20, 0.020f, 3.6f, 1, Color( 70, 95, 70) },
};

struct SkinDef {
    const char* name;
    Color color;
    bool  emissive;
};

static const SkinDef SKIN_DEFS[] = {
    { "DEFAULT",   Color(150,150,160), false },
    { "GOLD",      Color(255,190, 60), false },
    { "NEON",      Color( 40,255,160), true  },
    { "CRIMSON",   Color(200, 30, 40), false },
    { "ARCTIC",    Color(210,235,255), false },
    { "OCEAN",     Color( 40,140,255), true  },
    { "VOID",      Color(120, 40,200), true  },
    { "TOXIC",     Color(160,255, 40), true  },
};
constexpr int SKIN_COUNT = 8;

struct WeaponState {
    int currentAmmo_;
    int weaponType_;
    int reserveAmmo_;
    bool isReloading_;
    float reloadTimer_;
    float fireTimer_;
    float recoilX_;
    float recoilY_;
    float weaponBobPhase_;
    
    WeaponState() 
        : currentAmmo_(GameConstants::MAX_AMMO)
        , reserveAmmo_(GameConstants::MAX_RESERVE_AMMO)
        , isReloading_(false)
        , reloadTimer_(0.0f)
        , fireTimer_(0.0f)
        , recoilX_(0.0f)
        , recoilY_(0.0f)
        , weaponBobPhase_(0.0f)
        , weaponType_(W_RIFLE)
    {}
};

struct PendingRemove {
    WeakPtr<Node> node_;
    float ttl_;
};

struct PlayerStats {
    float health_;
    float maxHealth_;
    float armor_;
    int score_;
    int kills_;
    int shotsFired_;
    int shotsHit_;
    
    PlayerStats() 
        : health_(100.0f)
        , maxHealth_(100.0f)
        , armor_(0.0f)
        , score_(0)
        , kills_(0)
        , shotsFired_(0)
        , shotsHit_(0)
    {}
    
    float GetAccuracy() const {
        if (shotsFired_ == 0) return 0.0f;
        return (float)shotsHit_ / (float)shotsFired_ * 100.0f;
    }
};

// ============================================================================
// Класс приложения
// ============================================================================

class MyApp : public Application {
    URHO3D_OBJECT(MyApp, Application);

private:
    // Подсистемы
    SharedPtr<Scene> scene_;
    Node* cameraNode_ = nullptr;
    Node* weaponNode_ = nullptr;
    Node* wingHandL_ = nullptr;   // куриные «руки» в виде крыльев на вьюмодели
    Node* wingHandR_ = nullptr;
    Node* weaponModelNode_ = nullptr;
    Light* muzzleLight_ = nullptr;
    Node* muzzleLightNode_ = nullptr;
    
    // Компоненты
    // === Лёгкая физика: реестр коллайдеров уровня (см. namespace Lite) ===
    Vector<Lite::Collider> colliders_;
    int AddBoxCollider(const Vector3& c, const Vector3& half, int tag = 0, void* ptr = nullptr) {
        Lite::Collider col; col.center = c; col.half = half; col.sphere = false; col.tag = tag; col.ptr = ptr;
        colliders_.Push(col);
        return (int)colliders_.Size() - 1;
    }
    int AddSphereCollider(const Vector3& c, float r, int tag = 0, void* ptr = nullptr) {
        Lite::Collider col; col.center = c; col.half = Vector3(r, r, r); col.sphere = true; col.tag = tag; col.ptr = ptr;
        colliders_.Push(col);
        return (int)colliders_.Size() - 1;
    }
    int colliderGarbage_ = 0;   // сколько «удалённых» записей ждёт компакции

    void RemoveColliderLite(int idx) {
        if (idx >= 0 && idx < (int)colliders_.Size() && colliders_[(unsigned)idx].tag != 7) {
            colliders_[(unsigned)idx].tag = 7;
            ++colliderGarbage_;
        }
    }

    // Реестр коллайдеров раньше только рос: удалённые записи (tag 7) оставались в массиве
    // и перебирались в каждом рейкасте. Компакция пересобирает массив и чинит все индексы.
    void CompactColliders() {
        if (colliderGarbage_ <= 0) return;
        Vector<Lite::Collider> compacted;
        compacted.Reserve(colliders_.Size());
        Vector<int> remap;
        remap.Resize(colliders_.Size());
        for (unsigned i = 0; i < colliders_.Size(); ++i) {
            if (colliders_[i].tag == 7) { remap[i] = -1; continue; }
            remap[i] = (int)compacted.Size();
            compacted.Push(colliders_[i]);
        }
        auto fix = [&remap](int& idx) {
            if (idx >= 0 && idx < (int)remap.Size()) idx = remap[(unsigned)idx];
            else idx = -1;
        };
        for (unsigned i = 0; i < targets_.Size(); ++i) fix(targets_[i].colliderIdx_);
        for (unsigned i = 0; i < enemies_.Size(); ++i) fix(enemies_[i].colliderIdx_);
        for (unsigned i = 0; i < movingTargetsLite_.Size(); ++i) fix(movingTargetsLite_[i].colliderIdx_);
        colliders_ = compacted;
        colliderGarbage_ = 0;
    }
    void MoveColliderLite(int idx, const Vector3& center) {
        if (idx >= 0 && idx < (int)colliders_.Size()) colliders_[(unsigned)idx].center = center;
    }
    Vector3 HitNormalLite(const Vector3& point, const Vector3& center, const Vector3& half) const {
        Vector3 d = point - center;
        float ax = fabsf(d.x_), ay = fabsf(d.y_), az = fabsf(d.z_);
        float sx = ax / Max(half.x_, 0.001f), sy = ay / Max(half.y_, 0.001f), sz = az / Max(half.z_, 0.001f);
        if (sx >= sy && sx >= sz) return Vector3(d.x_ > 0 ? 1 : -1, 0, 0);
        if (sy >= sx && sy >= sz) return Vector3(0, d.y_ > 0 ? 1 : -1, 0);
        return Vector3(0, 0, d.z_ > 0 ? 1 : -1);
    }
    bool RaycastLite(const Vector3& origin, const Vector3& dir, float maxDist, int tagMask,
                     float& outDist, Vector3& outPoint, int* outTag = nullptr, void** outPtr = nullptr,
                     Vector3* outNormal = nullptr) const {
        float best = maxDist; int bestTag = -1; void* bestPtr = nullptr; bool hit = false;
        Vector3 bestCenter = Vector3::ZERO, bestHalf = Vector3::ONE; bool bestSphere = false;
        bool bestFloor = false;

        // Пол: аналитическая плоскость y = FLOOR_Y, тег 0 (статика).
        // Раньше пола не было в реестре вообще: пули уходили «сквозь» него, а ProbeGround
        // возвращал мусор и HandleMovement подставлял высоту вручную.
        if ((tagMask & 1) && dir.y_ < -1e-6f) {
            float tFloor = (GameConstants::FLOOR_Y - origin.y_) / dir.y_;
            if (tFloor >= 0.0f && tFloor < best) {
                best = tFloor; bestTag = 0; bestPtr = nullptr; hit = true; bestFloor = true;
            }
        }

        for (unsigned i = 0; i < colliders_.Size(); ++i) {
            const Lite::Collider& c = colliders_[i];
            if (c.tag == 7 || !(tagMask & (1 << c.tag))) continue; // 7 = удалён
            float t = c.sphere ? Lite::RaySphere(origin, dir, c.center, c.half.x_)
                               : Lite::RayBox(origin, dir, c.center, c.half);
            if (t >= 0.0f && t < best) {
                best = t; bestTag = c.tag; bestPtr = c.ptr; hit = true; bestFloor = false;
                bestCenter = c.center; bestHalf = c.half; bestSphere = c.sphere;
            }
        }
        if (hit) {
            outDist = best; outPoint = origin + dir * best;
            if (outTag) *outTag = bestTag;
            if (outPtr) *outPtr = bestPtr;
            if (outNormal) {
                if (bestFloor) *outNormal = Vector3::UP;
                else if (bestSphere) *outNormal = (outPoint - bestCenter).Normalized();
                else *outNormal = HitNormalLite(outPoint, bestCenter, bestHalf);
            }
        }
        return hit;
    }
    // Свободна ли точка для спавна (пересечение с AABB/сферами статики)
    bool IsPointFree(const Vector3& p, float radius) const {
        for (unsigned i = 0; i < colliders_.Size(); ++i) {
            const Lite::Collider& c = colliders_[i];
            if (c.tag != 0) continue; // только статика: стены, колонны, ящики, бочки, платформы
            float dx = p.x_ - c.center.x_, dy = p.y_ - c.center.y_, dz = p.z_ - c.center.z_;
            if (c.sphere) {
                float r = c.half.x_ + radius;
                if (dx * dx + dy * dy + dz * dz < r * r) return false;
            } else if (fabsf(dx) < c.half.x_ + radius &&
                       fabsf(dy) < c.half.y_ + radius &&
                       fabsf(dz) < c.half.z_ + radius) {
                return false;
            }
        }
        return true;
    }

    // Выталкивание из статики по горизонтали (4 коротких луча).
    // Раньше роботы двигались «напрямую» и проходили сквозь стены, ящики и платформу.
    Vector3 PushOutOfObstacles(const Vector3& pos, float radius, float probeHeight) {
        Vector3 test(pos.x_, pos.y_ + probeHeight, pos.z_);
        const Vector3 dirs[4] = { Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 0, 1), Vector3(0, 0, -1) };
        Vector3 push = Vector3::ZERO;
        for (int i = 0; i < 4; ++i) {
            float dist = 0.0f; Vector3 hitPoint;
            if (RaycastLite(test, dirs[i], radius + 0.1f, 1, dist, hitPoint) && dist < radius + 0.1f)
                push -= dirs[i] * ((radius + 0.1f) - dist);
        }
        return pos + push;
    }

    SharedPtr<DebugRenderer> debugRenderer_;
    
    // UI элементы
    SharedPtr<UIElement> crosshairCenter_;
    SharedPtr<UIElement> crosshairTop_;
    SharedPtr<UIElement> crosshairBottom_;
    SharedPtr<UIElement> crosshairLeft_;
    SharedPtr<UIElement> crosshairRight_;
    SharedPtr<Text> ammoText_;
    SharedPtr<Text> healthText_;
    SharedPtr<Text> scoreText_;
    SharedPtr<Text> accuracyText_;
    SharedPtr<Text> messageText_;
    SharedPtr<BorderImage> damageOverlay_;
    SharedPtr<BorderImage> reloadOverlay_;
    
    // Игровые данные
    Vector<TargetData> targets_;
    WeaponState weapon_;
    PlayerStats player_;

    // Патроны по каждому оружию: переключение 1..7 больше не является бесконечным боезапасом.
    // magAmmo_/reserveAmmo_ хранят остаток для слотов, ammoInit_ — инициализирован ли слот.
    int magAmmo_[WEAPON_COUNT] = {};
    int reserveAmmo_[WEAPON_COUNT] = {};
    bool ammoInit_[WEAPON_COUNT] = {};

    // Таймер вспышки дула (свет включается на 1-2 кадра, а не «включается и сразу гаснет»)
    float muzzleFlashTimer_ = 0.0f;

    // Пауза (Esc в игре) и отложенная пересборка UI при ресайзе окна
    bool paused_ = false;
    SharedPtr<UIElement> pausePanel_;
    bool pendingUiRebuild_ = false;
    bool rebuildingUI_ = false;
    float lastCrosshairGap_ = -1.0f;
    int uiWidth_ = 0;
    int uiHeight_ = 0;

    // Кэш материалов эффектов: раньше MakeTinted() вызывался на каждый выстрел/снаряд
    // и навсегда оседал в tintedMatStore_ (утечка + churn ресурсов).
    SharedPtr<Material> tracerMat_;
    SharedPtr<Material> enemyShotMat_;
    // Кэш материалов глаз роботов и «железа» оружия: раньше новые Material создавались
    // на каждого заспавненного врага и на каждое переключение оружия.
    SharedPtr<Material> eyeMatMelee_, eyeMatCharger_, eyeMatShooter_, eyeMatBoss_;
    SharedPtr<Material> weaponDarkMat_, weaponAccentMat_;
    
    // Состояние ввода
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    bool isSprinting_ = false;
    bool isCrouching_ = false;      // удерживаемый Ctrl/C — присед
    bool isAiming_ = false;         // прицеливание (удержание ПКМ)
    bool isMouseVisible_ = false;

    // Физика игрока: прыжки, гравитация, рейкаст-пол
    Vector3 playerVelocity_ = Vector3::ZERO;  // горизонтальная скорость (сглаживание)
    float verticalVelocity_ = 0.0f;           // вертикальная скорость (прыжок/падение)
    float eyeHeight_ = GameConstants::PLAYER_HEIGHT; // текущая высота глаз (приседание)
    float groundY_ = 0.0f;                    // высота пола под игроком
    bool onGround_ = true;
    float coyoteTimer_ = 0.0f;                // «коёот-таймер»: запрыгивание на край после схода
    float jumpBufferTimer_ = 0.0f;            // буфер нажатия Space до приземления
    float fallSpeedOnLand_ = 0.0f;            // скорость в момент касания земли

    // Динамическая камера от 1-го лица
    float headBobPhase_ = 0.0f;
    float headBobIntensity_ = 0.0f;           // 0..1, плавное нарастание покачки при разгоне
    float landDuckOffset_ = 0.0f;             // просадка камеры при приземлении
    float camShakeAmount_ = 0.0f;             // тряска от взрывов/отдачи
    float currentFov_ = 75.0f;                // плавно интерполируемый FOV
    
    // Временные переменные
    float messageTimer_ = 0.0f;
    float damageFlashTimer_ = 0.0f;
    float timeAcc_ = 0.0f;
    Vector<PendingRemove> pendingRemoves_;

    // === GAME OVER / смерть игрока ===
    bool gameOver_ = false;
    SharedPtr<UIElement> gameOverPanel_;
    SharedPtr<Text> gameOverStatsText_;

    // === Game feel: hitmarker, низкое HP, трейсеры, снаряды врагов ===
    float hitMarkerTimer_ = 0.0f;       // >0 — показывать крестик попадания
    float hitKillTimer_ = 0.0f;         // >0 — красная версия (смертельный выстрел)
    Vector<SharedPtr<UIElement>> hitMarkerBars_;
    BorderImage* lowHealthVignette_ = nullptr; // пульсирующая красная рамка при HP<30
    Vector<SharedPtr<BorderImage>> vignetteEdges_; // 4 полосы виньетки низкого HP
    struct TracerData {
        SharedPtr<Node> node_;
        float ttl_ = 0.0f;
    };
    Vector<TracerData> tracers_;        // видимые трассеры пуль
    struct ProjectileData {
        SharedPtr<Node> node_;
        Vector3 vel_;
        float ttl_ = 0.0f;
        float damage_ = 10.0f;
    };
    Vector<ProjectileData> enemyProjectiles_; // снаряды роботов-стрелков
    bool minigunSpinupActive_ = false;    // миниган стреляет → игрок замедлен

    // Новый контент: режимы, оружие, скины, арена
    GameMode gameMode_ = MODE_RANGE;

    // ===== Главное меню / настройки / арена с волнами =====
    bool inMenu_ = true;            // игра стартует в главном меню
    int menuSel_ = 0;               // 0 тир, 1 арена, 2 настройки, 3 выход
    bool inSettings_ = false;
    int settingsSel_ = 0;           // 0 тени, 1 bloom, 2 FPS, 3 фильтрация, 4 мышь, 5 SFX, 6 музыка, 7 назад
    int gfxShadowIdx_ = 2;          // 0=1K 1=2K 2=4K
    int gfxGlowIdx_ = 2;            // 0=выкл 1=слабый 2=полный
    int gfxFpsIdx_ = 1;             // 0=60 1=120 2=без лимита (раньше тут была мёртвая «разрешение»)
    int gfxAnisoIdx_ = 2;           // 0=билинейная 1=трилинейная 2=анизотропная
    // Текстуры, к которым применяется настройка фильтрации
    Vector<Texture*> anisoTextures_;
    float sfxVolume_ = 0.8f;
    float musicVolume_ = 0.35f;
    SharedPtr<UIElement> mainMenuPanel_;
    Vector<SharedPtr<Text>> mainMenuItems_;
    // Элементы меню для анимации (заголовок, подзаголовок, плашка, подсказка, частицы)
    SharedPtr<Text> menuTitle_;
    SharedPtr<Text> menuSub_;
    SharedPtr<Text> menuHint_;
    SharedPtr<BorderImage> menuPlate_;
    SharedPtr<BorderImage> menuAccentBar_;
    Vector<SharedPtr<BorderImage>> menuParticles_;
    Vector<float> menuParticlePhase_;
    Vector<float> menuParticleX_;
    Vector<float> menuParticleSpeed_;
    SharedPtr<Text> gameOverTitle_;
    SharedPtr<Text> gameOverHint_;
    float gameOverPulse_ = 0.0f;
    IntVector2 goBasePos_;   bool goBasePosSet_ = false;   // база для пульсации GAME OVER
    IntVector2 menuTitleBase_, menuPlateBase_, menuBarBase_;
    bool menuBasesSet_ = false;                              // базы параллакса меню
    Vector<UIElement*> menuRows_;                            // подложки строк меню (MenuRow0..3)
    SharedPtr<UIElement> settingsPanel_;
    Vector<SharedPtr<Text>> settingsItems_;
    // Арена: волны роботов-курят
    enum EnemyType { E_MELEE = 0, E_CHARGER, E_SHOOTER, E_BOSS, E_TYPE_COUNT };
    struct EnemyData {
        WeakPtr<Node> node_;
        int colliderIdx_ = -1;           // индекс в Lite-реестре коллайдеров
        float health_ = 100.0f;
        float maxHealth_ = 100.0f;
        bool alive_ = true;
        Vector3 patrolCenter_;
        float patrolRadius_ = 4.0f;
        float phase_ = 0.0f;
        float speed_ = 1.6f;
        int scoreValue_ = 150;
        // === ИИ нового поколения ===
        EnemyType type_ = E_MELEE;
        float attackTimer_ = 0.0f;      // кулдаун контактной атаки / стрельбы
        float strafePhase_ = 0.0f;      // петляние при сближении
        float burstShots_ = 0.0f;       // очередь выстрелов стрелка
        float burstTimer_ = 0.0f;       // таймер между одиночными в очереди
        float recoilKick_ = 0.0f;       // визуальный «откат» стрелка
        // === Куриная анимация (ходьба, клевки, взмахи) ===
        WeakPtr<Node> headNode_, wingLNode_, wingRNode_, legLNode_, legRNode_, tailNode_;
        float animPhase_ = 0.0f;
        float cluckTimer_ = 5.0f;        // случайное кудахтанье в простое
    };
    Vector<EnemyData> enemies_;
    // enemyNodes_ убран: он держал SharedPtr на уже удалённых роботов (утечка узла на каждое убийство).
    // Живыми нодами владеет сцена, EnemyData хранит WeakPtr.
    // === Lite-мишени и Lite-осколки (без Bullet) ===
    struct MovingTargetLite {
        SharedPtr<Node> node_;
        Vector3 vel_ = Vector3::ZERO;
        int colliderIdx_ = -1;
    };
    Vector<MovingTargetLite> movingTargetsLite_;
    struct DebrisData {
        SharedPtr<Node> node_;
        Vector3 pos_ = Vector3::ZERO;
        Vector3 vel_ = Vector3::ZERO;
        Vector3 spin_ = Vector3::ZERO;
        float ttl_ = 4.0f;
    };
    Vector<DebrisData> debrisLite_;

    // === Пикапы (аптечки и патроны): единственный источник лечения в игре ===
    struct PickupData {
        SharedPtr<Node> node_;
        Vector3 home_ = Vector3::ZERO;
        int type_ = 0;             // 0 = аптечка, 1 = патроны
        float respawn_ = 0.0f;     // >0 — подобран, ждёт респавна
    };
    Vector<PickupData> pickups_;
    SharedPtr<Material> pickupHealMat_;
    SharedPtr<Material> pickupAmmoMat_;

    int waveNum_ = 0;
    float waveRespawnTimer_ = 0.0f;
    // Комбо и рекорды
    int comboCount_ = 0;
    float comboTimer_ = 0.0f;
    int bestCombo_ = 0;
    int highScore_ = 0;
    float rangeRespawnTimer_ = 0.0f;
    SharedPtr<Material> robotBodyMat_;

    // === Куриные материалы (перья, гребень, клюв, лапы, яйца) ===
    SharedPtr<Material> chickWhite_, chickBrown_, chickBlack_, chickBoss_;
    SharedPtr<Material> combMat_, beakMat_, legMat_, eggMat_, featherMat_;
    SharedPtr<Material> decalMat_;     // пулевые отверстия
    SharedPtr<Material> shockMat_;     // ударная волна взрыва
    SharedPtr<Material> shellMat_;     // латунные гильзы
    SharedPtr<Material> spawnRingMat_;  // круг появления курицы

    struct ShockwaveData { SharedPtr<Node> node_; float ttl_ = 0.6f; float max_ = 0.6f; };
    Vector<ShockwaveData> shockwaves_;
    Vector<SharedPtr<Node>> decals_;   // пулевые отверстия (не копим больше 60)
    // (robotEyeMat_ убран: глаза роботов кэшируются в EnsureEyeMaterials)
    int currentWeapon_ = W_RIFLE;
    int currentSkin_ = 0;
    Vector<SharedPtr<Node>> decoNodes_;

    // Анимированный декор: голограмма над ареной, вращающиеся кольца, качающиеся фонари
    struct AnimatedDeco {
        SharedPtr<Node> node_;
        Vector3 pos_;
        float spin_ = 0.0f;      // градусов в секунду вокруг Y
        float bobAmp_ = 0.0f;    // амплитуда вертикального покачивания
        float bobSpeed_ = 1.0f;
        float phase_ = 0.0f;
    };
    Vector<AnimatedDeco> animatedDeco_;
    Light* neonLightA_ = nullptr;   // пульсирующая подсветка арены
    Light* neonLightB_ = nullptr;
    SharedPtr<Text> weaponNameText_;
    bool armoryOpen_ = false;
    SharedPtr<UIElement> armoryPanel_;
    Vector<SharedPtr<Text>> armoryWeaponRows_;
    Vector<SharedPtr<Text>> armorySkinRows_;
    SharedPtr<UIElement> hudPanelBg_;
    SharedPtr<BorderImage> hudHpBarBg_, hudHpBarFill_, hudAmmoBarBg_, hudAmmoBarFill_;
    static constexpr float HUD_BAR_W = 260.0f;
    SharedPtr<Text> helpText_;
    // === Новые элементы HUD: баннер волны, полоса босса, лента событий ===
    SharedPtr<Text> waveBanner_;
    float waveBannerTimer_ = 0.0f;
    Color waveBannerColor_ = Color(1.0f, 0.85f, 0.25f, 1.0f);  // UIElement::GetColor() в 1.8 требует Corner
    SharedPtr<BorderImage> bossBarBg_, bossBarFill_;
    SharedPtr<Text> bossBarText_;
    SharedPtr<Text> killFeedText_;
    struct FeedEntry { String text_; float ttl_ = 0.0f; };
    Vector<FeedEntry> killFeed_;
    float arenaTime_ = 0.0f;      // время, прожитое в арене (статистика)
    int mouseSensIdx_ = 1;        // 0 — низкая, 1 — обычная, 2 — высокая

    // Справка по управлению (F1). При первом старте показывается автоматически.
    SharedPtr<UIElement> helpPanel_;
    bool helpOpen_ = false;
    bool helpShownOnce_ = false;   // подсказка по управлению: прячется вместе с HUD
    
    // Ресурсы
    Model* boxModel_ = nullptr;
    Model* sphereModel_ = nullptr;
    Model* planeModel_ = nullptr;
    Model* coneModel_ = nullptr;      // клювы, гребни, хвостовые перья
    Model* cylModel_ = nullptr;       // шеи, лапы, трубы
    Material* stoneMaterial_ = nullptr;
    Material* metalMaterial_ = nullptr;
    Material* targetMaterial_ = nullptr;
    Material* targetHitMaterial_ = nullptr;
    Material* floorMaterialLight_ = nullptr;
    Material* hdStone_ = nullptr;
    Material* hdFloor_ = nullptr;
    Material* hdPanel_ = nullptr;
    Material* hdMetal_ = nullptr;
    Material* hdCrate_ = nullptr;
    Material* skyMaterial_ = nullptr;
    Material* fallbackMat_ = nullptr; // гарантия непустого материала, если ни один xml не найден
    Texture* skyTexture_ = nullptr;
    Vector<SharedPtr<Material>> weaponTintMats_;
    ParticleEffect* sparkEffect_ = nullptr;
    ParticleEffect* smokeEffect_ = nullptr;
    bool haveParticles_ = false; // Spark.pfx/Smoke.pfx реально загружены (пустые .pfx -> багы рендера)
    Font* font_ = nullptr;
    
    // Звуки (процедурно-синтезированные WAV в Data/Sounds)
    Sound* shootSounds_[WEAPON_COUNT] = {};
    Sound* reloadSound_ = nullptr;
    Sound* emptyClickSound_ = nullptr;
    Sound* hitSound_ = nullptr;
    Sound* destroySound_ = nullptr;
    Sound* explosionSound_ = nullptr;
    Sound* jumpSound_ = nullptr;
    Sound* landSound_ = nullptr;
    Sound* uiSelectSound_ = nullptr;
    SoundSource* soundSource_ = nullptr;   // 2D-микшер на камере
    SharedPtr<SoundSource> musicSource_;       // фоновая музыка
    SoundSource3D* explosionSource3D_ = nullptr; // взрывы в мировом пространстве

    // === Куриные звуки (см. tools/gen_sounds.py — синтезируются без внешних ассетов) ===
    Sound* cluckSounds_[2] = {};
    Sound* squawkSound_ = nullptr;
    Sound* eggSplatSound_ = nullptr;
    Sound* bossRoarSound_ = nullptr;
    Sound* waveStartSound_ = nullptr;
    Vector<SharedPtr<SoundSource3D>> sfx3DPool_;   // пул позиционных источников
    int sfx3DNext_ = 0;

    // Загрузочный экран
    bool loadingScreenActive_ = false;
    float loadProgress_ = 0.0f;
    SharedPtr<UIElement> loadingPanel_;
    SharedPtr<Text> loadingTitleText_;
    SharedPtr<Text> loadingTipText_;
    SharedPtr<BorderImage> loadingBarFill_;
    Vector<String> loadSteps_;
    int loadStepIndex_ = 0;

public:
    MyApp(Context* context) : Application(context) {}

    // Реализации всех методов встроены ниже в тело класса.
    // (предварительные объявления удалены — они вызывали ошибки переопределения)

    void Start() override {
        // Инициализация случайных чисел
        srand((unsigned int)time(nullptr));

        // Фикс "Failed to add resource path 'Data'/'CoreData'".
        // Application уже добавил абсолютный путь Data/ (от exeDir или cwd).
        // Если он не существует — удаляем и пересоздаём в правильной директории,
        // затем добавляем CoreData как отдельный префикс.
        {
            ResourceCache* cache = GetSubsystem<ResourceCache>();
            // Директория исполняемого файла
            String exeDir;
            {
                char link[4096];
                ssize_t n = readlink("/proc/self/exe", link, sizeof(link) - 1);
                if (n > 0) {
                    link[n] = '\0';
                    String exePath(link);
                    exeDir = exePath.Substring(0, exePath.FindLast('/'));
                }
            }
            auto dirOk = [](const String& p) -> bool {
                struct stat st;
                return !p.Empty() && stat(p.CString(), &st) == 0 && S_ISDIR(st.st_mode);
            };

            // 1) Корректный абсолютный путь к Data/
            String goodData;
            if (dirOk(exeDir + "/Data"))               goodData = exeDir + "/Data";        // build/Data
            else if (dirOk(exeDir + "/../Data"))       goodData = exeDir + "/../Data";     // ../Data от build/
            else if (dirOk("./Data"))                  goodData = "./Data";               // запуск из корня
            else if (dirOk("../Data"))                 goodData = "../Data";              // запуск из build/

            // 2) Ставим правильный Data ПЕРВЫМ префиксом.
            //    Методы Get/SetResourcePrefixPaths в этой сборке private —
            //    но AddResourceDir(path, priority=0) вставляет путь перед
            //    стандартным PRIORITY_LAST-записями Application, т.е. выше битого Data/.
            if (!goodData.Empty()) {
                cache->AddResourceDir(goodData, 0);
                URHO3D_LOGINFO("Папка ресурсов добавлена (priority 0): " + goodData);

            // УДАЛЕНО: на старте игра рекурсивно удаляла через system("find ... -delete")
            // все .pfx не только у себя, но и в установленном Urho3D (включая bin/Data движка).
            // Это необратимо ломало чужую установку движка ради подавления строчки в логе;
            // пустые/битые .pfx теперь просто игнорируются (см. CreateParticleEffects).
            } else {
                URHO3D_LOGWARNING("Папка Data/ не найдена ни рядом с бинарником, ни в cwd! Скопируй: cp -r ~/urho3d/bin/Data ./Data");
            }

            // 3) CoreData: ищем рядом с Data и добавляем отдельным префиксом
            Vector<String> coreCandidates;
            if (!goodData.Empty())   coreCandidates.Push(goodData + "/CoreData");
            if (!exeDir.Empty()) {
                coreCandidates.Push(exeDir + "/CoreData");
                coreCandidates.Push(exeDir + "/../CoreData");
                coreCandidates.Push(exeDir + "/../../bin/CoreData");
            }
            coreCandidates.Push("./Data/CoreData");
            coreCandidates.Push("../Data/CoreData");
            coreCandidates.Push("./CoreData");
            bool coreAdded = false;
            for (unsigned i = 0; i < coreCandidates.Size(); ++i) {
                if (dirOk(coreCandidates[i])) {
                    cache->AddResourceDir(coreCandidates[i]);
                    URHO3D_LOGINFO("CoreData найден и добавлен: " + coreCandidates[i]);
                    coreAdded = true;
                    break;
                }
            }
            if (!coreAdded)
                URHO3D_LOGWARNING("CoreData не найден! Положи папку: mkdir -p Data && cp -r ~/urho3d/bin/CoreData Data/CoreData (из корня проекта)");

            // 4) Копируем CoreData/ и прочие стандартные ресурсы движка в папку
            //    бинарника (build/Data), если там их нет. После этого Application
            //    при следующем запуске сам найдёт корректный абсолютный путь
            //    build/Data/ — ошибка 'Failed to add resource path' исчезнет совсем.
            if (!exeDir.Empty() && !goodData.Empty()) {
                String dst = exeDir + "/Data";
                auto copyTree = [](const char* cmd) { int rc = system(cmd); (void)rc; };
                String srcRoot = goodData; // источник ресурсов (проекта или urho3d)
                // CoreData
                if (!dirOk(exeDir + "/Data/CoreData")) {
                    for (unsigned i = 0; i < coreCandidates.Size(); ++i) {
                        if (dirOk(coreCandidates[i])) {
                            String cmd = "cp -r '" + coreCandidates[i] + "' '" + dst + "/' 2>/dev/null";
                            copyTree(cmd.CString());
                            URHO3D_LOGINFO("CoreData скопирован в " + dst + "/CoreData");
                            break;
                        }
                    }
                }
                // докидываем отсутствующие стандартные подпапки (Fonts, Models, Textures...)
                const char* stdDirs[] = {"Fonts", "Models", "Textures", "Sounds", "Scripts", "Matrices", "Shaders", "Techniques", "Particle", "Music", "Materials"};
                for (const char* sd : stdDirs) {
                    String dSrc = srcRoot + "/" + sd;
                    String dDst = dst + "/" + sd;
                    if (dirOk(dSrc) && !dirOk(dDst)) {
                        String cmd = "cp -r '" + dSrc + "' '" + dDst + "' 2>/dev/null";
                        copyTree(cmd.CString());
                    }
                }
                // Игровые HD-ресурсы: Materials/Game/*.xml из bin/Data проекта
                // (без них материалы пола/стен/ящиков не грузятся -> серая сцена).
                {
                    String gameMatSrc = srcRoot + "/Materials/Game";
                    if (dirOk(gameMatSrc) && !dirOk(dst + "/Materials/Game")) {
                        String cmd = "mkdir -p '" + dst + "/Materials' && cp -r '" + gameMatSrc + "' '" + dst + "/Materials/' 2>/dev/null";
                        copyTree(cmd.CString());
                        URHO3D_LOGINFO("Materials/Game скопирован в " + dst + "/Materials/Game");
                    }
                }
            }

            // Текстуры HD подключаются отдельным текстурным путём, если лежат в
            // bin/Data/TexturesHD (генерируются tools/gen_textures.py).
            {
                Vector<String> texHdCandidates;
                if (!goodData.Empty())   texHdCandidates.Push(goodData + "/TexturesHD");
                if (!exeDir.Empty())     texHdCandidates.Push(exeDir + "/../bin/Data/TexturesHD");
                texHdCandidates.Push("./bin/Data/TexturesHD");
                texHdCandidates.Push("../bin/Data/TexturesHD");
                auto fileOk = [](const String& p) -> bool {
                    struct stat st; return !p.Empty() && stat(p.CString(), &st) == 0;
                };
                for (unsigned i = 0; i < texHdCandidates.Size(); ++i) {
                    if (fileOk(texHdCandidates[i] + "/StoneHDDiffuse.png")) {
                        // AddTextureDir не существует — кладём папку в общий реестр ресурсов
                        cache->AddResourceDir(texHdCandidates[i], 1);
                        URHO3D_LOGINFO("TexturesHD подключён: " + texHdCandidates[i]);
                        break;
                    }
                }
            }
        }

        // Настройка движка
        SetupEngineSettings();

        // Пользовательские настройки графики/звука (settings.ini)
        LoadSettingsFile();
        ApplyGraphicsSettings();

        // Загрузочный экран показываем ДО долгой загрузки ресурсов
        CreateLoadingScreen();
        loadingScreenActive_ = true;
        
        // Загрузка ресурсов
        if (!LoadResources()) {
            URHO3D_LOGERROR("Failed to load resources!");
            engine_->Exit();
            return;
        }
        
        // Создание сцены
        CreateScene();
        AdvanceLoadingStep();
        
        // Создание игрока и камеры
        CreatePlayer();
        AdvanceLoadingStep();
        
        // Создание оружия
        CreateWeapon();
        AdvanceLoadingStep();
        
        // Создание UI
        CreateUI();
        AdvanceLoadingStep();
        
        // Настройка ввода
        SetupInput();
        
        // Подписка на события
        SubscribeToEvents();
        
        // Скрываем загрузочный экран перед первым игровым кадром
        HideLoadingScreen();

        // Стартовое главное меню (игрок сам выбирает режим)
        CreateMainMenu();
        ShowMainMenu(true);

        // Применение выбранного оружия/скина
        EquipWeapon(currentWeapon_, true);

        // Показ сообщения о начале игры
        ShowMessage("CHIKENGUN 2.1 | WASD - движение, SHIFT - бег, SPACE - прыжок, CTRL - присед\nЛКМ - огонь, ПКМ - прицел, 1-7 оружие, TAB - арсенал, F - режим, Esc - пауза\nЗелёные сферы - аптечки, жёлтые - патроны");
    }

private:
    // =========================================================================
    // Инициализация
    // =========================================================================
    
    void SetupEngineSettings() {
        // Получаем подсистемы
        auto* graphics = GetSubsystem<Graphics>();
        auto* renderer = GetSubsystem<Renderer>();
        
        // Настройки графики
        graphics->SetSRGB(true);
        
        // Настройки рендерера
        renderer->SetShadowMapSize(4096);           // карта теней 4K: чёткие края на большой арене
        renderer->SetSpecularLighting(true);
        // Единое условие с ApplyGraphicsSettings: HDR только на «полном» bloom (>=2),
        // иначе «слабый» вёл себя как «выкл», а на старте HDR был включён.
        renderer->SetHDRRendering(gfxGlowIdx_ >= 2);
        ConfigureGlow(renderer, gfxGlowIdx_ >= 2);
        
        // Настройка окна
    }
    
    bool LoadResources() {
        auto* cache = GetSubsystem<ResourceCache>();
        
        // Директории ресурсов уже настроены в Start() через SetResourcePrefixPaths
        // (поиск Data/ и CoreData/ относительно папки запуска). Старые вызовы
        // AddResourceDir("CoreData") удаляем — они и давали ошибку "Failed to add resource path".
        
        // Загружаем базовые модели
        boxModel_ = cache->GetResource<Model>("Models/Box.mdl");
        sphereModel_ = cache->GetResource<Model>("Models/Sphere.mdl");
        coneModel_ = cache->GetResource<Model>("Models/Cone.mdl");
        cylModel_ = cache->GetResource<Model>("Models/Cylinder.mdl");
        planeModel_ = cache->GetResource<Model>("Models/Plane.mdl");
        
        // Создаем материалы программно (на случай если файлы не найдены)
        CreateMaterials();
        
        // Загружаем или создаем эффекты частиц
        CreateParticleEffects();
        
        // Шрифт для UI
        font_ = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
        if (!font_) {
            URHO3D_LOGWARNING("Font not found, using default");
            font_ = cache->GetResource<Font>("Fonts/DejaVuSans.ttf");
        }
        
        // Проверяем критические ресурсы
        if (!boxModel_) {
            URHO3D_LOGWARNING("Box model not found, will use procedural geometry");
        }
        
        // Звуки выстрелов — по одному на каждый тип оружия
        static const char* shotFiles[WEAPON_COUNT] = {
            "Sounds/Shot_Pistol.wav", "Sounds/Shot_SMG.wav", "Sounds/Shot_Rifle.wav",
            "Sounds/Shot_Shotgun.wav", "Sounds/Shot_Sniper.wav", "Sounds/Shot_Minigun.wav",
            "Sounds/Shot_Grenade.wav"
        };
        for (int i2 = 0; i2 < WEAPON_COUNT; ++i2) {
            shootSounds_[i2] = cache->GetResource<Sound>(shotFiles[i2]);
            if (shootSounds_[i2]) shootSounds_[i2]->SetLooped(false);
        }

        reloadSound_     = cache->GetResource<Sound>("Sounds/Reload.wav");
        emptyClickSound_ = cache->GetResource<Sound>("Sounds/EmptyClick.wav");
        hitSound_        = cache->GetResource<Sound>("Sounds/HitMarker.wav");
        destroySound_    = cache->GetResource<Sound>("Sounds/TargetDestroy.wav");
        explosionSound_  = cache->GetResource<Sound>("Sounds/Explosion.wav");
        jumpSound_       = cache->GetResource<Sound>("Sounds/Jump.wav");
        landSound_       = cache->GetResource<Sound>("Sounds/Land.wav");
        uiSelectSound_   = cache->GetResource<Sound>("Sounds/UI_Select.wav");

        // Куриные звуки: если файлов нет (старый Data) — просто останутся nullptr,
        // все вызовы защищены проверками, игра работает без них.
        cluckSounds_[0]  = cache->GetResource<Sound>("Sounds/Chicken_Cluck.wav");
        cluckSounds_[1]  = cache->GetResource<Sound>("Sounds/Chicken_Cluck2.wav");
        squawkSound_     = cache->GetResource<Sound>("Sounds/Chicken_Squawk.wav");
        eggSplatSound_   = cache->GetResource<Sound>("Sounds/Chicken_Egg.wav");
        bossRoarSound_   = cache->GetResource<Sound>("Sounds/Chicken_Boss.wav");
        waveStartSound_  = cache->GetResource<Sound>("Sounds/Wave_Start.wav");

        // Фоновая музыка (синтезированный трансовый луп)
        Sound* music = cache->GetResource<Sound>("Music/Game_Theme_Loop.ogg");
        if (music) {
            music->SetLooped(true);
            // GetOutput() нет в этой сборке — музыка через отдельный неатenuированный источник на камере
            musicSource_ = cameraNode_ ? cameraNode_->CreateComponent<SoundSource>() : nullptr;
            if (musicSource_) {
// //                 musicSource_->SetAuto3DSound(false);
                musicSource_->SetGain(musicVolume_);
                musicSource_->Play(music);
            }
        }

        return true;
    }
    
    // Хранилище для unlit-материалов, созданных MakeTinted (владеем указателями, чтобы они не удалились)
    Vector<SharedPtr<Material>> tintedMatStore_;
    SharedPtr<Material> baseMat_; // кэш: валидный материал-база для тинтов (или null, если искали и не нашли)

    SharedPtr<Material> MakeTinted(const SharedPtr<Texture>& diff, const SharedPtr<Texture>& norm, const Color& c) {
        auto* cache = GetSubsystem<ResourceCache>();
        // Кэш: ищем ПЕРВЫЙ валидный материал-базу один раз. Если Data неполный и xml не грузятся —
        // строим технику программно (DiffUnlit.xml из CoreData). Пустой Material без техники = segfault рендера.
        if (!baseMat_) {
            static const char* kBaseMats[] = {
                "Materials/Game/TintUnlit.xml", "Materials/VColored.xml",
                "Materials/StoneTiled.xml", "Materials/DefaultGrey.xml"
            };
            for (const char* cand : kBaseMats) {
                Material* t = cache->GetResource<Material>(String(cand)); // сырой указатель!
                if (t && t->GetTechniques().Size()) { baseMat_ = t; break; }
            }
            if (!baseMat_) {
                // Финальный fallback: берём ЛЮБОЙ валидный материал из CoreData
                // (их техники уже описаны в xml — достаточно просто скопировать материал).
                static const char* kCoreMats[] = {
                    "Materials/Stone.xml", "Materials/RoughStoneBump1.xml",
                    "Materials/Water.xml", "Materials/BlurY.xml",
                    "Materials/VColUnlit.xml", "Materials/DefaultDiffuse.xml"
                };
                for (const char* cand : kCoreMats) {
                    Material* t = cache->GetResource<Material>(String(cand), true);
                    if (t && t->GetTechniques().Size()) { baseMat_ = t; break; }
                }
                if (!baseMat_) {
                    baseMat_ = new Material(context_);
                    URHO3D_LOGERROR("MakeTinted: ни один материал не найден даже в CoreData — сцена будет без шейдеров");
                } else {
                    URHO3D_LOGINFO("MakeTinted: база для тинтов взята из CoreData");
                }
            }
        }
        SharedPtr<Material> m = baseMat_->Clone();
        if (diff) { m->SetTexture(TU_DIFFUSE, diff); }
        // SetDiffuseColor нет — цвет через шейдерный параметр MatDiffColor (0..1)
        m->SetShaderParameter("MatDiffColor", Variant(Color(c.r_ / 255.0f, c.g_ / 255.0f, c.b_ / 255.0f, 1.0f)));
        tintedMatStore_.Push(m);   // держим ссылку: указатели остаются живыми
        return m;
    }

 void CreateMaterials() {
     auto* cache = GetSubsystem<ResourceCache>();
     if (!fallbackMat_) { fallbackMat_ = new Material(context_); tintedMatStore_.Push(SharedPtr<Material>(fallbackMat_)); }
     // --- HD текстуры (сгенерированы tools/gen_textures.py) ---
     hdStone_  = cache->GetResource<Material>("Materials/Game/StoneHD.xml");
     hdFloor_  = cache->GetResource<Material>("Materials/Game/FloorTile.xml");
     hdPanel_  = cache->GetResource<Material>("Materials/Game/MetalPanel.xml");
     hdMetal_  = cache->GetResource<Material>("Materials/Game/BrushedMetal.xml");
     hdCrate_  = cache->GetResource<Material>("Materials/Game/CrateWood.xml");
     // ВАЖНО: Texture — абстрактная база и НЕ зарегистрирована в ResourceCache,
     // поэтому GetResource<Texture>() давал «Could not load unknown resource type» на каждую
     // текстуру (12 ошибок в логе) и HD-материалы молча оставались без своих текстур.
     skyTexture_ = cache->GetResource<Texture2D>("Textures/SkyDay.png");

     // --- Качество текстур: mipmaps убирают «снег» на дальних плоскостях,
     // анизотропная фильтрация делает плитку пола чёткой под острым углом ---
     {
         const char* hdTexNames[] = {
             "Textures/StoneHDDiffuse.png", "Textures/StoneHDNormal.png",
             "Textures/FloorTileDiffuse.png", "Textures/FloorTileNormal.png",
             "Textures/MetalPanelDiffuse.png", "Textures/MetalPanelNormal.png",
             "Textures/BrushedMetalDiffuse.png", "Textures/BrushedMetalNormal.png",
             "Textures/CrateWoodDiffuse.png", "Textures/CrateWoodNormal.png",
             "Textures/SkyDay.png"
         };
         anisoTextures_.Clear();
         for (const char* tn : hdTexNames) {
            // ResourceCache::GetResource<T>() возвращает голый T* (не SharedPtr)
            Texture2D* t = cache->GetResource<Texture2D>(String(tn));
            if (t && !anisoTextures_.Contains(t)) anisoTextures_.Push(t);
        }
        // Фильтрация теперь реально зависит от настройки (было жёстко FILTER_ANISOTROPIC)
        ApplyTextureFiltering();
     }

     stoneMaterial_ = hdStone_ ? hdStone_ : cache->GetResource<Material>("Materials/Stone.xml");
     if (!stoneMaterial_) stoneMaterial_ = new Material(context_);
     metalMaterial_ = hdMetal_ ? hdMetal_ : stoneMaterial_;

     // --- Тайлинг HD-материалов: у процедурных текстур нет UV-тайлинга в модели,
     // поэтому ставим UOffset/VOffset (как в стоковом Materials/StoneTiled.xml).
     // Без этого одна текстура 1024px растягивается на весь пол — «мыло» без деталей.
     {
         auto tileMat = [](Material* m, float tiles) {
             if (!m) return;
             m->SetShaderParameter("UOffset", Variant(Vector4(tiles, 0, 0, 0)));
             m->SetShaderParameter("VOffset", Variant(Vector4(0, tiles, 0, 0)));
         };
         tileMat(hdStone_, 8.0f);    // стены/платформы
         tileMat(hdFloor_, 32.0f);   // пол арены — мелкая плитка
         tileMat(hdPanel_, 6.0f);    // панели
         tileMat(hdMetal_, 4.0f);    // металл
         tileMat(hdCrate_, 1.0f);    // ящики: текстура = вся грань
         URHO3D_LOGINFO(String("HD-материалы: ") + (hdStone_ ? "камень OK " : "камень NULL ")
             + (hdFloor_ ? "пол OK " : "пол NULL ") + (hdCrate_ ? "ящики OK" : "ящики NULL"));
     }
     // Мишени: яркие процедурные unlit-материалы, чтобы были видны всегда
     {
         SharedPtr<Texture> white;
         targetMaterial_ = MakeTinted(white, nullptr, Color(230, 60, 60));
         targetHitMaterial_ = MakeTinted(white, nullptr, Color(255, 220, 120));
     }
     // --- Куриные материалы: плоские unlit-тинты, как у остальных процедурных объектов ---
     chickWhite_ = MakeTinted(nullptr, nullptr, Color(250, 248, 240));   // клуша
     chickBrown_ = MakeTinted(nullptr, nullptr, Color(205, 115, 45));    // петух-чарджер
     chickBlack_ = MakeTinted(nullptr, nullptr, Color(58, 54, 70));      // чёрный стрелок
     chickBoss_  = MakeTinted(nullptr, nullptr, Color(235, 240, 255));   // босс-индюк
     combMat_    = MakeTinted(nullptr, nullptr, Color(225, 35, 45));     // гребень и бородка
     beakMat_    = MakeTinted(nullptr, nullptr, Color(255, 185, 45));    // клюв
     legMat_     = MakeTinted(nullptr, nullptr, Color(240, 175, 60));    // лапы
     eggMat_     = MakeTinted(nullptr, nullptr, Color(255, 250, 232));   // снаряд-яйцо
     featherMat_ = MakeTinted(nullptr, nullptr, Color(252, 250, 245));   // перья при смерти

     floorMaterialLight_ = hdFloor_ ? hdFloor_ : stoneMaterial_;
     // Анизотропная фильтрация: глобальная настройка движка (per-material MAT_ANISOTROPY в этой сборке недоступен)
     // Глобальная анизотропия недоступна в этой сборке — фильтрация по умолчанию
     // Материалы оружия под текущий скин
     RefreshWeaponMaterials();
     URHO3D_LOGINFO(stoneMaterial_ ? "Base material OK (HD)" : "Base material NULL");
 }

    void RefreshWeaponMaterials() {
        weaponTintMats_.Clear();
        weaponAccentMat_ = nullptr;   // акцент оружия зависит от скина — пересоздадим
        const SkinDef& skin = SKIN_DEFS[currentSkin_];
        for (int w = 0; w < WEAPON_COUNT; ++w) {
            Color base = WEAPON_DEFS[w].tint;
            Color c(base.r_ * 0.45f + skin.color.r_ * 0.55f,
                    base.g_ * 0.45f + skin.color.g_ * 0.55f,
                    base.b_ * 0.45f + skin.color.b_ * 0.55f);
            c = ClampColor(c);
            weaponTintMats_.Push(MakeTinted(nullptr, nullptr, c));
        }
    }

    static Color ClampColor(Color c) {
        if (c.r_ > 1) c.r_ = 1; if (c.g_ > 1) c.g_ = 1; if (c.b_ > 1) c.b_ = 1;
        return c;
    }
    
    void CreateParticleEffects() {
        auto* cache = GetSubsystem<ResourceCache>();

        // Ищем НАСТОЯЩИЙ валидный pfx: сначала Spark.pfx из Data проекта,
        // затем стандартные эффекты движка (Urho3D bin/Data/Particle/*).
        // Пустые/битые файлы дают "unknown resource type" и опасны для эмиттера.
        static const char* kSparkCandidates[] = {
            "Particle/Spark.pfx", "Particle/SparkBig.pfx",
            "Particle/Fireworks.pfx", "Particle/ExplosionBig.pfx",
            "Particle/Torch.pfx", "Particle/Flame.pfx"
        };
        for (const char* p : kSparkCandidates) {
            ParticleEffect* e = cache->GetResource<ParticleEffect>(String(p));
            // Валидность проверяем по числу техник (GetTechnique у ParticleEffect отсутствует)
            if (e && !e->GetName().Empty()) { sparkEffect_ = e; break; }
        }
        static const char* kSmokeCandidates[] = {
            "Particle/Smoke.pfx", "Particle/Mist.pfx", "Particle/Dust.pfx"
        };
        for (const char* p : kSmokeCandidates) {
            ParticleEffect* e = cache->GetResource<ParticleEffect>(String(p));
            if (e && !e->GetName().Empty()) { smokeEffect_ = e; break; }
        }

        // Эмиттер без SetEffect не рендерит ничего и НЕ крашится — это наш
        // безопасный путь. haveParticles_ включаем только при реальном эффекте.
        haveParticles_ = (sparkEffect_ != nullptr) || (smokeEffect_ != nullptr);
        if (haveParticles_)
            URHO3D_LOGINFO(String("Частицы активны: ") + (sparkEffect_ ? "искры " : "") + (smokeEffect_ ? "дым" : ""));
        else
            URHO3D_LOGINFO("Частицы выключены (нет валидных .pfx) — игра работает стабильно без них.");

        // Если ни один .pfx не загрузился (Data неполный / битые файлы), строим
        // эффекты ПРЯМО В ПАМЯТИ из XML движка: <material><technique name=.../></material>.
        // Technique грузится по абсолютному пути CoreData, поэтому материал всегда валиден.
        // Fallback: если ни один .pfx не загрузился — строим эффекты прямо в памяти
        if (!sparkEffect_) {
            sparkEffectStore_ = MakeProceduralParticles("Materials/ParticleAdditive.xml",
                                                        120, 60.0f, 0.05f, 0.25f, 0.8f, true);
            sparkEffect_ = sparkEffectStore_;
        }
        if (!smokeEffect_) {
            smokeEffectStore_ = MakeProceduralParticles("Materials/ParticleAlphaBlend.xml",
                                                        80, 30.0f, 0.3f, 0.9f, 1.6f, false);
            smokeEffect_ = smokeEffectStore_;
        }
        haveParticles_ = (sparkEffect_ != nullptr) || (smokeEffect_ != nullptr);
    }

    SharedPtr<ParticleEffect> sparkEffectStore_, smokeEffectStore_; // владение fallback-эффектами

    // Программный ParticleEffect из стоковых материалов CoreData.
    // Функция отключена: текущая сборка Urho3D не имеет нужного API частиц.
    // haveParticles_ остаётся false — вместо частиц работают декали/осколки (это штатный путь).
    SharedPtr<ParticleEffect> MakeProceduralParticles(const char*, int, float, float, float, float, bool) {
        return SharedPtr<ParticleEffect>();
    }



    // =========================================================================
    // Создание сцены
    // =========================================================================
    
    void CreateScene() {
        scene_ = new Scene(context_);
        
        // Физика — лёгкий движок (Lite), компонент PhysicsWorld/Bullet не создаётся
        colliders_.Clear(); // на случай повторного создания сцены (рестарт)
        
        // Отладочный рендерер
        debugRenderer_ = scene_->CreateComponent<DebugRenderer>();
        
        // Создаем октодерево
        scene_->CreateComponent<Octree>();
        
        // Создаем уровень
        CreateFloor();
        CreateWalls();
        // Потолок убран: арена открыта небу — прыжки и динамическая камера смотрят на skybox
        CreateObstacles();
        CreateStructures();
        CreateTargets();
        
        // Небо и декорации новой арены
        CreateSky();
        CreateArenaDecor();
        CreatePickups();

        // Создаем освещение
        CreateLighting();

        // Зона рендеринга: ambient + дымка по дальности (ощущение масштаба арены)
        CreateZone();

        // Позиционные звуки: пул источников создаётся один раз на сцену
        InitSfx3DPool();
    }

    void CreateZone() {
        Node* zoneNode = scene_->CreateChild("Zone");
        Zone* zone = zoneNode->CreateComponent<Zone>();
        zone->SetBoundingBox(BoundingBox(-700.0f, 700.0f));
        // Мягкий холодный ambient — нет чёрных провалов, но контраст теней сохраняется
        // Сумерки: тёплый горизонт + холодная тень — арена перестаёт быть «серой коробкой»
        zone->SetAmbientColor(Color(0.26f, 0.30f, 0.40f));
        zone->SetFogColor(Color(0.46f, 0.50f, 0.66f));
        zone->SetFogStart(55.0f);
        zone->SetFogEnd(230.0f);
        // SetFarDistance нет в этой сборке — дальность тумана уже задана SetFogEnd
    }

    void CreateSky() {
        // Небо через Skybox: рисуется до сцены на бесконечности — не мерцает с геометрией
        Node* skyNode = scene_->CreateChild("Sky");
        // Техника берётся из xml-материала; если TintUnlit.xml нет — MakeTinted построит материал сам
        {
            SharedPtr<Material> skyM = MakeTinted(nullptr, nullptr, Color(150, 185, 235));
            skyMaterial_ = skyM.Get();
        }
        if (!skyMaterial_) { skyMaterial_ = new Material(context_); tintedMatStore_.Push(SharedPtr<Material>(skyMaterial_)); }
        if (skyTexture_) skyMaterial_->SetTexture(TU_DIFFUSE, skyTexture_);
        skyMaterial_->SetShaderParameter("MatDiffColor", Variant(Color(150.0f / 255.0f, 185.0f / 255.0f, 235.0f / 255.0f, 1.0f)));
        skyMaterial_->SetCullMode(CULL_NONE);
        // ВАЖНО: Skybox с процедурным unlit-материалом без текстуры в этой сборке
        // рендерится БЕЛЫМ (весь экран белый). Если HD-текстуры небы — ставим
        // огромный тёмный фон через StaticModel вместо скайбокса.
        if (skyTexture_) {
            Skybox* sky = skyNode->CreateComponent<Skybox>();
            sky->SetModel(boxModel_);
            sky->SetMaterial(skyMaterial_);
        } else {
            skyMaterial_->SetShaderParameter("MatDiffColor", Variant(Color(0.02f, 0.035f, 0.07f, 1.0f)));
            StaticModel* skyGeo = skyNode->CreateComponent<StaticModel>();
            skyGeo->SetModel(boxModel_);
            skyGeo->SetMaterial(skyMaterial_);
            skyNode->SetScale(Vector3(400.0f, 400.0f, 400.0f));
            skyNode->SetPosition(Vector3(0.0f, 60.0f, 0.0f));
        }
        // SetDrawDistance нет в этой сборке — скайбокс рисуется всегда
    }

    Model* cache_GetSphere() {
        return sphereModel_ ? sphereModel_ : boxModel_;
    }

    void CreateArenaDecor() {
        auto* cache = GetSubsystem<ResourceCache>();
        SharedPtr<Material> glowRed   = MakeTinted(nullptr, nullptr, Color(255, 40, 40));
        SharedPtr<Material> glowCyan  = MakeTinted(nullptr, nullptr, Color(40, 220, 255));
        SharedPtr<Material> glowGreen = MakeTinted(nullptr, nullptr, Color(60, 255, 120));
        const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;

        // Светящиеся неоновые полосы вдоль стен
        for (int side = 0; side < 4; ++side) {
            for (int seg = 0; seg < 6; ++seg) {
                Node* strip = scene_->CreateChild("NeonStrip");
                float t = -half + (half * 2.0f) * (seg / 5.0f);
                if (side == 0) { strip->SetPosition(Vector3(t, 0.15f, -half + 0.6f)); strip->SetScale(Vector3(half/3.2f, 0.06f, 0.12f)); }
                else if (side == 1) { strip->SetPosition(Vector3(t, 0.15f, half - 0.6f)); strip->SetScale(Vector3(half/3.2f, 0.06f, 0.12f)); }
                else if (side == 2) { strip->SetPosition(Vector3(-half + 0.6f, 0.15f, t)); strip->SetScale(Vector3(0.12f, 0.06f, half/3.2f)); }
                else { strip->SetPosition(Vector3(half - 0.6f, 0.15f, t)); strip->SetScale(Vector3(0.12f, 0.06f, half/3.2f)); }
                StaticModel* sm = strip->CreateComponent<StaticModel>();
                sm->SetModel(boxModel_);
                sm->SetMaterial((seg % 2) ? glowCyan : glowGreen);
                decoNodes_.Push(SharedPtr<Node>(strip));
            }
        }

        // Ящики-укрытия с деревянной HD текстурой
        const Vector3 cratePos[] = {
            Vector3(14, 1, 14), Vector3(-16, 1, 10), Vector3(12, 1, -18),
            Vector3(-14, 1, -14), Vector3(22, 1, 4), Vector3(-30, 1, 7)
        };
        for (const auto& pos : cratePos) {
            Node* crate = scene_->CreateChild("Crate");
            crate->SetPosition(pos);
            crate->SetScale(Vector3(2.0f, 2.0f, 2.0f));
            crate->SetRotation(Quaternion(0, (float)(rand() % 90 - 45), 0));
            StaticModel* cm = crate->CreateComponent<StaticModel>();
            cm->SetModel(boxModel_);
            SharedPtr<Material> tintedWood = MakeTinted(nullptr, nullptr,
                Color(180 + (rand()%50), 140 + (rand()%40), 90 + (rand()%30)));
            cm->SetMaterial(hdCrate_ ? hdCrate_ : tintedWood.Get());
            AddBoxCollider(pos, Vector3(1.0f, 1.0f, 1.0f)); // Lite-коллайдер вместо Bullet
        }

        // Бочки с металлической HD-текстурой (укрытия в центре)
        const Vector3 barrelPos[] = {
            Vector3(5, 0.9f, -6), Vector3(-7, 0.9f, 5), Vector3(9, 0.9f, 9),
            Vector3(-4, 0.9f, -11), Vector3(14, 0.9f, -6), Vector3(-12, 0.9f, -8)
        };
        for (const auto& bp : barrelPos) {
            Node* barrel = scene_->CreateChild("Barrel");
            barrel->SetPosition(bp);
            barrel->SetScale(Vector3(1.1f, 1.8f, 1.1f));
            StaticModel* bm2 = barrel->CreateComponent<StaticModel>();
            Model* cylM = cache->GetResource<Model>("Models/Cylinder.mdl");
            bm2->SetModel(cylM ? cylM : boxModel_);
            SharedPtr<Material> barrelTint = MakeTinted(nullptr, nullptr,
                Color(60 + (rand()%40), 90 + (rand()%60), 110 + (rand()%50)));
            bm2->SetMaterial(hdMetal_ ? hdMetal_ : barrelTint.Get());
            AddBoxCollider(bp, Vector3(0.55f, 0.9f, 0.55f)); // Lite-коллайдер вместо Bullet
        }

        // Фонари вдоль дорожек — тёплый свет
        const Vector3 lampPos[] = {
            Vector3(18, 0, 0), Vector3(-18, 0, 0), Vector3(0, 0, 18), Vector3(0, 0, -18)
        };
        for (const auto& lp : lampPos) {
            Node* pole = scene_->CreateChild("LampPole");
            pole->SetPosition(lp + Vector3(0, 2.5f, 0));
            pole->SetScale(Vector3(0.2f, 5.0f, 0.2f));
            StaticModel* pm = pole->CreateComponent<StaticModel>();
            pm->SetModel(boxModel_);
            pm->SetMaterial(metalMaterial_);
            Node* head = scene_->CreateChild("LampHead");
            head->SetPosition(lp + Vector3(0, 5.1f, 0));
            head->SetScale(0.6f);
            StaticModel* hm = head->CreateComponent<StaticModel>();
            hm->SetModel(sphereModel_ ? sphereModel_ : boxModel_);
            { SharedPtr<Material> hmM = MakeTinted(nullptr, nullptr, Color(255, 230, 160)); if (hmM) hm->SetMaterial(hmM.Get()); }
            Node* ln2 = scene_->CreateChild("LampLight");
            ln2->SetPosition(lp + Vector3(0, 5.0f, 0));
            Light* l2 = ln2->CreateComponent<Light>();
            l2->SetLightType(LIGHT_POINT);
            l2->SetColor(Color(1.0f, 0.85f, 0.55f));
            l2->SetRange(14.0f);
            l2->SetBrightness(0.9f);
            decoNodes_.Push(SharedPtr<Node>(pole)); decoNodes_.Push(SharedPtr<Node>(head)); decoNodes_.Push(SharedPtr<Node>(ln2));
        }

        // Дополнительные ящики-пирамидки
        const Vector3 stackPos[] = { Vector3(20, 1, 20), Vector3(-20, 1, -20), Vector3(20, 3, 20) };
        for (const auto& sp : stackPos) {
            Node* crate2 = scene_->CreateChild("CrateStack");
            crate2->SetPosition(sp);
            crate2->SetScale(Vector3(1.4f, 1.4f, 1.4f));
            crate2->SetRotation(Quaternion(0, (float)(rand() % 90), 0));
            StaticModel* cm2 = crate2->CreateComponent<StaticModel>();
            cm2->SetModel(boxModel_);
            cm2->SetMaterial(hdCrate_ ? hdCrate_ : stoneMaterial_);
            AddBoxCollider(sp, Vector3(0.7f, 0.7f, 0.7f)); // Lite-коллайдер вместо Bullet
        }

        // Антенны-вышки по углам с красными маяками
        const float off = half - 4.0f;
        const Vector3 towers[] = { Vector3(off,0,off), Vector3(-off,0,off), Vector3(off,0,-off), Vector3(-off,0,-off) };
        for (const auto& tp : towers) {
            Node* mast = scene_->CreateChild("Mast");
            mast->SetPosition(tp + Vector3(0, 6, 0));
            mast->SetScale(Vector3(0.35f, 12.0f, 0.35f));
            StaticModel* mm = mast->CreateComponent<StaticModel>();
            mm->SetModel(boxModel_);
            mm->SetMaterial(metalMaterial_);
            Node* beacon = scene_->CreateChild("Beacon");
            beacon->SetPosition(tp + Vector3(0, 12.4f, 0));
            beacon->SetScale(0.5f);
            StaticModel* bm = beacon->CreateComponent<StaticModel>();
            bm->SetModel(sphereModel_ ? sphereModel_ : boxModel_);
            bm->SetMaterial(glowRed);
            // Маяки светятся материалом (unlit + bloom), а не отдельным Light:
            // 4 лишних динамических источника в forward-рендере = лишние проходы на каждом объекте.
            decoNodes_.Push(SharedPtr<Node>(mast)); decoNodes_.Push(SharedPtr<Node>(beacon));
        }
    }
    
    void CreateFloor() {
        // БЫЛО: 48x48 = 2304 отдельные ноды с StaticModel (2304 draw call'а и 2304 обхода
        // октодерева на каждый кадр). СТАЛО: один меш на всю арену с UV-тайлингом 48x48.
        const float size = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE; // 192 м
        const float uv = GameConstants::FLOOR_UV_TILES;

        Node* floorNode = scene_->CreateChild("Floor");
        floorNode->SetPosition(Vector3(0.0f, GameConstants::FLOOR_Y, 0.0f));

        CustomGeometry* floorGeom = floorNode->CreateComponent<CustomGeometry>();
        floorGeom->BeginGeometry(0, TRIANGLE_LIST);
        auto addVertex = [floorGeom](const Vector3& pos, float u, float v) {
            floorGeom->DefineVertex(pos);
            floorGeom->DefineNormal(Vector3::UP);
            floorGeom->DefineTexCoord(Vector2(u, v));
            floorGeom->DefineTangent(Vector4(1.0f, 0.0f, 0.0f, 1.0f)); // для normal-map техник
            floorGeom->DefineColor(Color::WHITE);
        };
        const float h = size * 0.5f;
        // Обход против часовой стрелки (вид сверху) — нормаль смотрит вверх
        addVertex(Vector3(-h, 0.0f, -h), 0.0f, 0.0f);
        addVertex(Vector3(-h, 0.0f,  h), 0.0f, uv);
        addVertex(Vector3( h, 0.0f,  h), uv,   uv);
        addVertex(Vector3(-h, 0.0f, -h), 0.0f, 0.0f);
        addVertex(Vector3( h, 0.0f,  h), uv,   uv);
        addVertex(Vector3( h, 0.0f, -h), uv,   0.0f);
        floorGeom->Commit();
        if (floorMaterialLight_) floorGeom->SetMaterial(floorMaterialLight_);
        floorGeom->SetCastShadows(false);

        // Коллайдер полу не нужен: пол — аналитическая плоскость y = FLOOR_Y внутри RaycastLite.
    }
    
    void CreateWalls() {
        const float wallHeight = 8.0f;    // выше: арена 192x192 м выглядит «стадионом»
        const float wallThickness = 2.0f;
        const float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        
        // ВАЖНО: куб 1x1 растягивается на ВСЮ длину стороны (2 * arenaSize),
        // иначе каждая стена закрывала только половину периметра и в углах были дыры.
        const float wallLength = arenaSize * 2.0f;
        // Северная стена
        CreateWall(Vector3(0, wallHeight/2, -arenaSize), Vector3(wallLength, wallHeight, wallThickness));
        // Южная стена
        CreateWall(Vector3(0, wallHeight/2, arenaSize), Vector3(wallLength, wallHeight, wallThickness));
        // Западная стена
        CreateWall(Vector3(-arenaSize, wallHeight/2, 0), Vector3(wallThickness, wallHeight, wallLength));
        // Восточная стена
        CreateWall(Vector3(arenaSize, wallHeight/2, 0), Vector3(wallThickness, wallHeight, wallLength));

        // Верхний светящийся пояс по периметру (без физики — только красота в тумане)
        SharedPtr<Material> neonStrip = MakeTinted(nullptr, nullptr, Color(60, 220, 255));
        const float topY = wallHeight - 0.4f;
        Node* sN = scene_->CreateChild("TrimN"); sN->SetPosition(Vector3(0, topY, -arenaSize + 0.6f)); sN->SetScale(Vector3(arenaSize*2, 0.25f, 0.15f));
        Node* sS = scene_->CreateChild("TrimS"); sS->SetPosition(Vector3(0, topY,  arenaSize - 0.6f)); sS->SetScale(Vector3(arenaSize*2, 0.25f, 0.15f));
        Node* sW = scene_->CreateChild("TrimW"); sW->SetPosition(Vector3(-arenaSize + 0.6f, topY, 0)); sW->SetScale(Vector3(0.15f, 0.25f, arenaSize*2));
        Node* sE = scene_->CreateChild("TrimE"); sE->SetPosition(Vector3( arenaSize - 0.6f, topY, 0)); sE->SetScale(Vector3(0.15f, 0.25f, arenaSize*2));
        Node* trims[4] = { sN, sS, sW, sE };
        for (Node* t : trims) {
            StaticModel* sm = t->CreateComponent<StaticModel>();
            sm->SetModel(boxModel_);
            sm->SetMaterial(neonStrip);
            sm->SetCastShadows(false);
            decoNodes_.Push(SharedPtr<Node>(t));
        }
    }
    
    void CreateWall(const Vector3& position, const Vector3& scale) {
        Node* wallNode = scene_->CreateChild("Wall");
        wallNode->SetPosition(position);
        wallNode->SetScale(scale);
        
        StaticModel* model = wallNode->CreateComponent<StaticModel>();
        model->SetModel(boxModel_);
        model->SetMaterial(stoneMaterial_);
        
        // Физика: Lite-коллайдер (половина scale = half-extents)
        AddBoxCollider(position, scale * 0.5f);
    }
    
    void CreateCeiling() {
        // Потолок намеренно не создаётся: открытый небо-купол для прыжков
        // и обзора динамической камеры. Функция оставлена как заглушка.
    }
    
    void CreateObstacles() {
        // =====================================================================
        // Карта арены: центральный подиум со ступенями, кольцо укрытий,
        // бетонные блоки, бочки, ящики и угловые бункеры.
        // Всё, во что можно упереться, получает Lite-коллайдер.
        // =====================================================================
        const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;

        SharedPtr<Material> neonCyan = MakeTinted(nullptr, nullptr, Color(40, 220, 255));
        SharedPtr<Material> neonPink = MakeTinted(nullptr, nullptr, Color(255, 70, 140));

        // Универсальный «блок»: визуал + коллайдер
        auto block = [this](const Vector3& pos, const Vector3& size, Material* mat, bool collide = true) {
            Node* n = scene_->CreateChild("Block");
            n->SetPosition(pos);
            n->SetScale(size);
            StaticModel* sm = n->CreateComponent<StaticModel>();
            sm->SetModel(boxModel_);
            sm->SetMaterial(mat ? mat : fallbackMat_);
            sm->SetCastShadows(true);
            if (collide) AddBoxCollider(pos, size * 0.5f);
            return n;
        };

        // ------------------------- центральная площадь -------------------------
        // Намеренно БЕЗ высокой платформы: враги не умеют запрыгивать (у них фиксированная
        // высота и выталкивание), поэтому подиум стал бы «безопасной точкой», откуда
        // игрок безнаказанно расстреливает арену. Вместо него — открытая площадь
        // с голограммой и четырьмя колоннами-укрытиями.
        Node* plaza = scene_->CreateChild("Plaza");
        for (int ring = 0; ring < 2; ++ring) {
            Node* disc = plaza->CreateChild("Ring");
            disc->SetPosition(Vector3(0, 0.03f + (float)ring * 0.002f, 0));
            disc->SetScale(Vector3(9.0f - (float)ring * 3.0f, 0.03f, 9.0f - (float)ring * 3.0f));
            StaticModel* dm = disc->CreateComponent<StaticModel>();
            dm->SetModel(cylModel_ ? cylModel_ : boxModel_);
            dm->SetMaterial(ring == 0 ? neonCyan.Get() : neonPink.Get());
            dm->SetCastShadows(false);
        }
        // Колонны-укрытия: 4 штуки по диагоналям, с неоновым поясом
        for (int i = 0; i < 4; ++i) {
            float ang = 45.0f + (float)i * 90.0f;
            float rad = ang * 0.0174533f;
            Vector3 cp(cosf(rad) * 11.0f, 3.5f, sinf(rad) * 11.0f);
            block(cp, Vector3(1.6f, 7.0f, 1.6f), hdPanel_ ? hdPanel_ : metalMaterial_);
            block(cp + Vector3(0, 2.4f, 0), Vector3(1.9f, 0.14f, 1.9f),
                  (i % 2) ? neonPink.Get() : neonCyan.Get(), false);
            block(cp + Vector3(0, -2.4f, 0), Vector3(1.9f, 0.14f, 1.9f), neonCyan.Get(), false);
        }

        // ------------------------- кольцо укрытий -----------------------------
        // Четыре длинных бетонных блока в 24 м от центра (разрывают линии огня)
        block(Vector3(0, 1.2f,  24.0f), Vector3(16.0f, 2.4f, 1.4f), metalMaterial_);
        block(Vector3(0, 1.2f, -24.0f), Vector3(16.0f, 2.4f, 1.4f), metalMaterial_);
        block(Vector3( 24.0f, 1.2f, 0), Vector3(1.4f, 2.4f, 16.0f), metalMaterial_);
        block(Vector3(-24.0f, 1.2f, 0), Vector3(1.4f, 2.4f, 16.0f), metalMaterial_);

        // Угловые бункеры (Г-образные стены) — «точки» для перестрелок
        const Vector3 bunkers[] = { Vector3(46, 0, 46), Vector3(-46, 0, 46),
                                    Vector3(46, 0, -46), Vector3(-46, 0, -46) };
        for (const auto& bpos : bunkers) {
            float sx = (bpos.x_ > 0) ? 1.0f : -1.0f;
            float sz = (bpos.z_ > 0) ? 1.0f : -1.0f;
            block(Vector3(bpos.x_, 1.4f, bpos.z_), Vector3(10.0f, 2.8f, 1.2f), stoneMaterial_);
            block(Vector3(bpos.x_ + sx * 4.4f, 1.4f, bpos.z_ + sz * 4.4f), Vector3(1.2f, 2.8f, 10.0f), stoneMaterial_);
        }

        // Бочки по кольцу r=32 (между укрытиями) — металл + светящийся обод
        for (int i = 0; i < 8; ++i) {
            float ang = (float)i * 45.0f + 22.5f;
            float rad = ang * 0.0174533f;
            Vector3 bp(cosf(rad) * 32.0f, 0.8f, sinf(rad) * 32.0f);
            Node* barrel = scene_->CreateChild("Barrel");
            barrel->SetPosition(bp);
            barrel->SetScale(Vector3(1.0f, 1.6f, 1.0f));
            StaticModel* bm = barrel->CreateComponent<StaticModel>();
            bm->SetModel(cylModel_ ? cylModel_ : boxModel_);
            bm->SetMaterial(metalMaterial_);
            bm->SetCastShadows(true);
            block(bp + Vector3(0, 0.78f, 0), Vector3(0.86f, 0.10f, 0.86f),
                  (i % 2) ? neonPink.Get() : neonCyan.Get(), false);
            AddSphereCollider(bp, 0.55f);
        }

        // Ящики-штабеля между укрытиями
        const Vector3 crates[] = {
            Vector3(38, 0, 14), Vector3(38, 0, -14), Vector3(-38, 0, 14), Vector3(-38, 0, -14),
            Vector3(14, 0, 38), Vector3(14, 0, -38), Vector3(-14, 0, 38), Vector3(-14, 0, -38)
        };
        int crateIdx = 0;
        for (const auto& cpos : crates) {
            block(cpos + Vector3(0, 0.8f, 0), Vector3(1.6f, 1.6f, 1.6f), hdCrate_ ? hdCrate_ : stoneMaterial_);
            if (crateIdx % 2 == 0) // второй ящик сверху — «пирамидка»
                block(cpos + Vector3(0.3f, 2.3f, 0.2f), Vector3(1.2f, 1.2f, 1.2f), hdCrate_ ? hdCrate_ : stoneMaterial_);
            ++crateIdx;
        }

        (void)half;
    }

    // Структуры декора: технический балкон на стене, трубы, голограмма, площадки спавна
    void CreateStructures() {
        SharedPtr<Material> neonCyan = MakeTinted(nullptr, nullptr, Color(40, 220, 255));
        SharedPtr<Material> neonPink = MakeTinted(nullptr, nullptr, Color(255, 70, 140));

        auto box = [this](Node* parent, const Vector3& pos, const Vector3& size, Material* mat) {
            Node* n = parent ? parent->CreateChild("Part") : scene_->CreateChild("Part");
            n->SetPosition(pos);
            n->SetScale(size);
            StaticModel* sm = n->CreateComponent<StaticModel>();
            sm->SetModel(boxModel_);
            sm->SetMaterial(mat ? mat : fallbackMat_);
            sm->SetCastShadows(false);
            return n;
        };

        // --- Технический балкон вдоль северной стены (высота 4.5 м) ---
        Node* catwalk = scene_->CreateChild("Catwalk");
        catwalk->SetPosition(Vector3(0, 0, -90.0f));
        for (int i = 0; i < 10; ++i) {
            box(catwalk, Vector3(-81.0f + (float)i * 18.0f, 4.5f, 0), Vector3(17.0f, 0.3f, 6.0f), hdPanel_ ? hdPanel_ : metalMaterial_);
            // перила
            box(catwalk, Vector3(-81.0f + (float)i * 18.0f, 5.5f, 2.9f), Vector3(17.0f, 0.1f, 0.1f), metalMaterial_);
            box(catwalk, Vector3(-81.0f + (float)i * 18.0f, 5.5f, -2.9f), Vector3(17.0f, 0.1f, 0.1f), metalMaterial_);
            // стойки перил
            for (int p = -1; p <= 1; ++p)
                box(catwalk, Vector3(-81.0f + (float)i * 18.0f + (float)p * 8.0f, 5.0f, 2.9f), Vector3(0.1f, 1.0f, 0.1f), metalMaterial_);
            // подсветка под балконом
            box(catwalk, Vector3(-81.0f + (float)i * 18.0f, 4.3f, 2.9f), Vector3(14.0f, 0.05f, 0.12f),
                (i % 2) ? neonCyan.Get() : neonPink.Get());
        }

        // --- Трубы по стенам: три уровня, два цвета ---
        const float wallX[] = { -94.0f, 94.0f };
        for (float wx : wallX) {
            for (int lvl = 0; lvl < 3; ++lvl) {
                Node* pipe = scene_->CreateChild("Pipe");
                pipe->SetPosition(Vector3(wx, 2.0f + (float)lvl * 1.2f, 0));
                pipe->SetScale(Vector3(0.35f, 180.0f, 0.35f));
                // цилиндр по умолчанию вертикальный: кладём на бок
                pipe->SetRotation(Quaternion(90.0f, Vector3(1, 0, 0)));
                StaticModel* pm = pipe->CreateComponent<StaticModel>();
                pm->SetModel(cylModel_ ? cylModel_ : boxModel_);
                pm->SetMaterial((lvl % 2) ? metalMaterial_ : (hdPanel_ ? hdPanel_ : metalMaterial_));
                pm->SetCastShadows(false);
            }
        }

        // --- Голограмма над центром: ядро + 3 вращающихся кольца ---
        Node* holo = scene_->CreateChild("Holo");
        holo->SetPosition(Vector3(0, 7.5f, 0));
        {
            Node* core = holo->CreateChild("Core");
            core->SetScale(Vector3::ONE * 1.6f);
            StaticModel* cm = core->CreateComponent<StaticModel>();
            cm->SetModel(sphereModel_ ? sphereModel_ : boxModel_);
            cm->SetMaterial(neonCyan.Get());
            cm->SetCastShadows(false);
            AnimatedDeco d; d.node_ = SharedPtr<Node>(core); d.pos_ = Vector3(0, 7.5f, 0);
            d.spin_ = 35.0f; d.bobAmp_ = 0.25f; d.bobSpeed_ = 1.2f; d.phase_ = 0.0f;
            animatedDeco_.Push(d);
        }
        for (int ring = 0; ring < 3; ++ring) {
            Node* rn = holo->CreateChild("Ring");
            float radius = 2.6f + (float)ring * 1.1f;
            rn->SetRotation(Quaternion((float)ring * 35.0f, Vector3(1, 0, 0)));
            for (int seg = 0; seg < 8; ++seg) {
                float ang = (float)seg * 45.0f;
                float rad = ang * 0.0174533f;
                box(rn, Vector3(cosf(rad) * radius, 0, sinf(rad) * radius), Vector3(0.8f, 0.10f, 0.10f),
                    (ring % 2) ? neonPink.Get() : neonCyan.Get());
            }
            AnimatedDeco d; d.node_ = SharedPtr<Node>(rn); d.pos_ = Vector3(0, 7.5f, 0);
            d.spin_ = (ring % 2) ? -22.0f : 28.0f; d.bobAmp_ = 0.0f; d.bobSpeed_ = 1.0f; d.phase_ = (float)ring;
            animatedDeco_.Push(d);
        }

        // --- Опоры под мишени тира (иначе они висят в воздухе после сноса старых колонн) ---
        const float tOff = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f * 0.6f;
        const Vector3 towers[4] = { Vector3(tOff, 0, tOff), Vector3(-tOff, 0, tOff),
                                    Vector3(tOff, 0, -tOff), Vector3(-tOff, 0, -tOff) };
        for (int i = 0; i < 4; ++i) {
            box(nullptr, towers[i] + Vector3(0, 1.5f, 0), Vector3(1.4f, 3.0f, 1.4f),
                hdPanel_ ? hdPanel_ : metalMaterial_);
            box(nullptr, towers[i] + Vector3(0, 3.05f, 0), Vector3(1.8f, 0.12f, 1.8f), metalMaterial_);
            AddBoxCollider(towers[i] + Vector3(0, 1.5f, 0), Vector3(0.7f, 1.5f, 0.7f));
        }
        // Постамент под центральную мишень
        box(nullptr, Vector3(0, 1.5f, -15.0f), Vector3(0.9f, 3.0f, 0.9f), metalMaterial_);
        AddBoxCollider(Vector3(0, 1.5f, -15.0f), Vector3(0.45f, 1.5f, 0.45f));

        // --- Площадки спавна: светящиеся диски по диагоналям ---
        for (int i = 0; i < 4; ++i) {
            float ang = 45.0f + (float)i * 90.0f;
            float rad = ang * 0.0174533f;
            Vector3 p(cosf(rad) * 62.0f, 0.06f, sinf(rad) * 62.0f);
            Node* pad = scene_->CreateChild("SpawnPad");
            pad->SetPosition(p);
            pad->SetScale(Vector3(4.0f, 0.06f, 4.0f));
            StaticModel* pm = pad->CreateComponent<StaticModel>();
            pm->SetModel(cylModel_ ? cylModel_ : boxModel_);
            pm->SetMaterial((i % 2) ? neonPink.Get() : neonCyan.Get());
            pm->SetCastShadows(false);
            AnimatedDeco d; d.node_ = SharedPtr<Node>(pad); d.pos_ = p;
            d.spin_ = 12.0f; d.bobAmp_ = 0.0f; d.bobSpeed_ = 1.0f; d.phase_ = (float)i;
            animatedDeco_.Push(d);
        }
    }

    // Вращение/покачивание декора + пульсация неонового света
    void UpdateAnimatedDecor(float dt) {
        for (unsigned i = 0; i < animatedDeco_.Size(); ++i) {
            AnimatedDeco& d = animatedDeco_[i];
            Node* n = d.node_.Get();
            if (!n) continue;
            n->SetRotation(n->GetRotation() * Quaternion(d.spin_ * dt, Vector3(0, 1, 0)));
            if (d.bobAmp_ > 0.0f)
                n->SetPosition(Vector3(d.pos_.x_, d.pos_.y_ + sinf(timeAcc_ * d.bobSpeed_ + d.phase_) * d.bobAmp_, d.pos_.z_));
        }
        // Пульсация двух неоновых источников — арена «дышит»
        float pulse = 0.55f + 0.45f * (0.5f + 0.5f * sinf(timeAcc_ * 1.6f));
        if (neonLightA_) neonLightA_->SetBrightness(1.4f * pulse);
        if (neonLightB_) neonLightB_->SetBrightness(1.4f * (1.0f - 0.6f * pulse));
    }

    void CreateRamp(const Vector3& position, const Vector3& direction) {
        Node* rampNode = scene_->CreateChild("Ramp");
        rampNode->SetPosition(position);
        rampNode->SetScale(Vector3(4.0f, 1.0f, 8.0f));
        rampNode->SetRotation(Quaternion(direction.z_ * 30.0f, 0.0f, 0.0f));
        
        StaticModel* model = rampNode->CreateComponent<StaticModel>();
        model->SetModel(boxModel_);
        model->SetMaterial(metalMaterial_);
        
        AddBoxCollider(position + Vector3(0, 1.2f, direction.z_*1.2f), Vector3(2.0f, 1.8f, 4.0f)); // Lite-аппроксимация пандуса
    }
    
    void CreateTargets() {
        // Создаем мишени в различных местах арены
        
        float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        
        // Мишени на стенах
        Vector3 wallPositions[] = {
            Vector3(0, 2.0f, -arenaSize + 1.0f),      // Север
            Vector3(0, 2.0f, arenaSize - 1.0f),       // Юг
            Vector3(-arenaSize + 1.0f, 2.0f, 0),      // Запад
            Vector3(arenaSize - 1.0f, 2.0f, 0),       // Восток
        };
        
        for (int i = 0; i < 4; ++i) {
            CreateTarget(wallPositions[i], Vector3(0, 0, i % 2 == 0 ? 1 : -1));
        }
        
        // Мишени на колоннах
        float offset = arenaSize * 0.6f;
        Vector3 columnPositions[] = {
            Vector3(offset, 3.0f, offset),
            Vector3(-offset, 3.0f, offset),
            Vector3(offset, 3.0f, -offset),
            Vector3(-offset, 3.0f, -offset)
        };
        
        for (int i = 0; i < 4; ++i) {
            CreateTarget(columnPositions[i], Vector3(0, -1, 0));
        }
        
        // Движущиеся мишени
        Vector3 movingPositions[] = {
            Vector3(5.0f, 1.5f, 5.0f),
            Vector3(-5.0f, 1.5f, -5.0f),
            Vector3(10.0f, 1.5f, -10.0f),
            Vector3(-10.0f, 1.5f, 10.0f)
        };
        
        for (int i = 0; i < 4; ++i) {
            CreateMovingTarget(movingPositions[i]);
        }
        
        // Главная тестовая мишень в центре
        Node* bossTargetNode = scene_->CreateChild("BossTarget");
        bossTargetNode->SetPosition(Vector3(0, 3.0f, -15.0f));
        bossTargetNode->SetScale(Vector3(2.0f, 2.0f, 0.2f));
        
        StaticModel* bossModel = bossTargetNode->CreateComponent<StaticModel>();
        bossModel->SetModel(boxModel_);
        bossModel->SetMaterial(targetMaterial_);
        
        // Физика для босс-мишени: Lite-коллайдер (tag 1, ptr = узел)
        TargetData bossData;
        bossData.node_ = bossTargetNode;
        bossData.colliderIdx_ = AddBoxCollider(bossTargetNode->GetPosition(), Vector3(2.0f, 2.0f, 0.2f), 1, bossTargetNode);
        bossData.health_ = GameConstants::TARGET_HEALTH * 3.0f;
        bossData.isActive_ = true;
        bossData.scoreValue_ = 500;
        targets_.Push(bossData);
        
        URHO3D_LOGINFO("Created " + String(targets_.Size()) + " targets");
    }
    
    void CreateTarget(const Vector3& position, const Vector3& normal) {
        Node* targetNode = scene_->CreateChild("Target");
        targetNode->SetPosition(position);
        targetNode->SetScale(Vector3(1.0f, 1.0f, 0.1f));
        
        // Поворачиваем мишень в направлении нормали
        if (normal.x_ != 0 || normal.z_ != 0) {
            targetNode->LookAt(position + normal);
        }
        
        StaticModel* model = targetNode->CreateComponent<StaticModel>();
        model->SetModel(boxModel_);
        model->SetMaterial(targetMaterial_);
        
        // Физика: Lite-коллайдер мишени
        TargetData data;
        data.node_ = targetNode;
        data.colliderIdx_ = AddBoxCollider(position, Vector3(0.6f, 0.6f, 0.2f), 1, targetNode);
        data.health_ = GameConstants::TARGET_HEALTH;
        data.isActive_ = true;
        data.scoreValue_ = 100;
        targets_.Push(data);
    }
    
    void CreateMovingTarget(const Vector3& startPosition) {
        Node* targetNode = scene_->CreateChild("MovingTarget");
        targetNode->SetPosition(startPosition);
        targetNode->SetScale(Vector3(0.8f, 0.8f, 0.8f));
        
        StaticModel* model = targetNode->CreateComponent<StaticModel>();
        model->SetModel(sphereModel_ ? sphereModel_ : boxModel_);
        model->SetMaterial(targetMaterial_);
        
        // Физика для движущейся мишени: Lite-сфера + интеграция в UpdateMovingTargetsLite
        MovingTargetLite mt;
        mt.node_ = targetNode;
        mt.vel_ = Vector3((float)(rand() % 5 - 2), 0, (float)(rand() % 5 - 2));
        mt.colliderIdx_ = AddSphereCollider(startPosition, 0.5f, 4, targetNode);
        movingTargetsLite_.Push(mt);

        TargetData data;
        data.node_ = targetNode;
        data.colliderIdx_ = mt.colliderIdx_;
        data.health_ = GameConstants::TARGET_HEALTH * 1.5f;
        data.isActive_ = true;
        data.scoreValue_ = 150;
        targets_.Push(data);
    }
    
    void CreateLighting() {
        const float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;

        // --- Ключевой свет: «золотой час», длинные тени по арене ---
        Node* sunNode = scene_->CreateChild("Sun");
        sunNode->SetPosition(Vector3(0, 30, 20));
        sunNode->SetDirection(Vector3(-0.58f, -0.50f, -0.52f).Normalized());
        Light* sunLight = sunNode->CreateComponent<Light>();
        sunLight->SetLightType(LIGHT_DIRECTIONAL);
        sunLight->SetColor(Color(1.0f, 0.87f, 0.66f, 1.0f));
        sunLight->SetBrightness(1.05f);
        sunLight->SetCastShadows(true);
        sunLight->SetShadowDistance(150.0f);
        // SetNumCascadeShadows/SetShadowFocus/GetShadowBias в этой сборке отсутствуют

        // --- Заполняющий «лунный» свет с противоположной стороны: убирает чёрные провалы ---
        Node* moonNode = scene_->CreateChild("MoonFill");
        moonNode->SetDirection(Vector3(0.6f, -0.35f, 0.55f).Normalized());
        Light* moon = moonNode->CreateComponent<Light>();
        moon->SetLightType(LIGHT_DIRECTIONAL);
        moon->SetColor(Color(0.45f, 0.60f, 1.00f, 1.0f));
        moon->SetBrightness(0.38f);
        moon->SetCastShadows(false);

        // --- Прожектор над центральным подиумом ---
        Node* spotNode = scene_->CreateChild("PlatformSpot");
        spotNode->SetPosition(Vector3(0, 22, 0));
        spotNode->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
        Light* spot = spotNode->CreateComponent<Light>();
        spot->SetLightType(LIGHT_SPOT);
        spot->SetColor(Color(0.75f, 0.92f, 1.0f, 1.0f));
        spot->SetBrightness(1.6f);
        spot->SetRange(40.0f);
        spot->SetFov(52.0f);
        spot->SetCastShadows(false);

        // --- Четыре заливающих источника по углам арены ---
        const Vector3 floodPos[] = {
            Vector3( arenaSize * 0.55f, 9.0f,  arenaSize * 0.55f),
            Vector3(-arenaSize * 0.55f, 9.0f,  arenaSize * 0.55f),
            Vector3( arenaSize * 0.55f, 9.0f, -arenaSize * 0.55f),
            Vector3(-arenaSize * 0.55f, 9.0f, -arenaSize * 0.55f)
        };
        for (int i = 0; i < 4; ++i) {
            Node* ln = scene_->CreateChild("Flood");
            ln->SetPosition(floodPos[i]);
            Light* l = ln->CreateComponent<Light>();
            l->SetLightType(LIGHT_POINT);
            l->SetColor(Color(1.0f, 0.88f, 0.72f, 1.0f));
            l->SetBrightness(1.0f);
            l->SetRange(26.0f);
            l->SetCastShadows(false);
        }

        // --- Пульсирующий неон: холодный и тёплый, по бокам от подиума ---
        Node* neonA = scene_->CreateChild("NeonA");
        neonA->SetPosition(Vector3(-12.0f, 3.0f, 0.0f));
        neonLightA_ = neonA->CreateComponent<Light>();
        neonLightA_->SetLightType(LIGHT_POINT);
        neonLightA_->SetColor(Color(0.25f, 0.85f, 1.0f, 1.0f));
        neonLightA_->SetRange(20.0f);
        neonLightA_->SetBrightness(1.4f);
        neonLightA_->SetCastShadows(false);

        Node* neonB = scene_->CreateChild("NeonB");
        neonB->SetPosition(Vector3(12.0f, 3.0f, 0.0f));
        neonLightB_ = neonB->CreateComponent<Light>();
        neonLightB_->SetLightType(LIGHT_POINT);
        neonLightB_->SetColor(Color(1.0f, 0.35f, 0.65f, 1.0f));
        neonLightB_->SetRange(20.0f);
        neonLightB_->SetBrightness(1.0f);
        neonLightB_->SetCastShadows(false);
    }

    // =========================================================================
    // Создание игрока и оружия
    // =========================================================================
    
    void CreatePlayer() {
        // Создаем узел камеры
        cameraNode_ = scene_->CreateChild("Camera");

        // Звуковой источник игрока: 2D-эффекты (выстрелы, UI) «приклеены» к камере
        soundSource_ = cameraNode_->CreateComponent<SoundSource>();

        // 3D-источник для взрывов в мировом пространстве
        explosionSource3D_ = scene_->CreateChild("ExplosionSfx")->CreateComponent<SoundSource3D>();
// //         explosionSource3D_->SetAuto3DSound(true, 4.0f, 120.0f);
        cameraNode_->SetPosition(Vector3(0.0f, GameConstants::PLAYER_HEIGHT, 0.0f));
        
        // Добавляем компонент камеры
        Camera* camera = cameraNode_->CreateComponent<Camera>();
        camera->SetFov(75.0f);
        camera->SetNearClip(0.1f);
        camera->SetFarClip(1000.0f);
        
        // Настраиваем Viewport
        auto* renderer = GetSubsystem<Renderer>();
        Viewport* vp = new Viewport(context_, scene_, camera);
        renderer->SetViewport(0, vp);
    }
    
    void CreateWeapon() {
        // Создаем узел оружия (привязан к камере)
        weaponNode_ = cameraNode_->CreateChild("Weapon");
        weaponNode_->SetPosition(Vector3(0.3f, -0.25f, 0.5f));
        weaponNode_->SetRotation(Quaternion(0, 0, 0));
        
        // Модель оружия (используем коробку как временную модель)
        weaponModelNode_ = weaponNode_->CreateChild("WeaponModel");
        BuildWeaponModel(currentWeapon_);

        // Куриные «руки»: оружие держат крылья (игра-то про кур)
        if (!featherMat_) featherMat_ = MakeTinted(nullptr, nullptr, Color(252, 250, 245));
        Material* wingMat = featherMat_ ? featherMat_.Get() : fallbackMat_;
        Model* sph = sphereModel_ ? sphereModel_ : boxModel_;
        wingHandL_ = AddPart(weaponNode_, Vector3(-0.10f, -0.13f, 0.02f), Vector3(0.16f, 0.34f, 0.42f), wingMat, sph);
        wingHandR_ = AddPart(weaponNode_, Vector3( 0.10f, -0.13f, 0.02f), Vector3(0.16f, 0.34f, 0.42f), wingMat, sph);
        // Перья на крыле: три «пальца», которыми курица обхватывает рукоять
        for (int i = 0; i < 3; ++i) {
            float z = 0.06f + (float)i * 0.09f;
            AddPart(wingHandL_, Vector3(0.35f, 0.10f, z), Vector3(0.30f, 0.16f, 0.16f), wingMat, sph);
            AddPart(wingHandR_, Vector3(-0.35f, 0.10f, z), Vector3(0.30f, 0.16f, 0.16f), wingMat, sph);
        }
        
        // Дуло оружия (для визуализации выстрела)
        Node* muzzleNode = weaponNode_->CreateChild("Muzzle");
        muzzleNode->SetPosition(Vector3(0, 0.05f, -0.25f));
        
        // Свет от выстрела
        Node* muzzleLightNode = weaponNode_->CreateChild("MuzzleLight");
        muzzleLightNode->SetPosition(Vector3(0, 0.05f, -0.25f));
        
        Light* muzzleLight = muzzleLightNode->CreateComponent<Light>();
        muzzleLight->SetLightType(LIGHT_POINT);
        muzzleLight->SetColor(Color(1.0f, 0.8f, 0.3f, 1.0f));
        muzzleLight->SetRange(5.0f);
        muzzleLightNode->SetEnabled(false);
        
        muzzleLight_ = muzzleLight;
        muzzleLightNode_ = muzzleLightNode;
    }

    // =========================================================================
    // Процедурные модели оружия и экипировка
    // =========================================================================

    Node* AddPart(Node* parent, const Vector3& pos, const Vector3& scale, Material* mat,
                  Model* model = nullptr, bool castShadows = false) {
        Node* n = parent->CreateChild("Part");
        n->SetPosition(pos);
        n->SetScale(scale);
        StaticModel* sm = n->CreateComponent<StaticModel>();
        sm->SetModel(model ? model : boxModel_);
        sm->SetMaterial(mat ? mat : fallbackMat_);
        sm->SetCastShadows(castShadows);
        return n;
    }

    // =========================================================================
    // Куры: единственный «персонаж» игры. Собирается из примитивов движка,
    // поэтому не требует внешних ассетов. Возвращает узел головы (анимация).
    // =========================================================================
    static constexpr float CH_HEAD_Y = 1.62f;   // базовая высота головы (локально)
    static constexpr float CH_HEAD_Z = 0.58f;

    Node* BuildChicken(Node* root, EnemyType type) {
        Model* sph = sphereModel_ ? sphereModel_ : boxModel_;
        Model* cone = coneModel_ ? coneModel_ : boxModel_;
        Model* cyl = cylModel_ ? cylModel_ : boxModel_;

        Material* feathers = chickWhite_.Get();
        Material* comb = combMat_.Get();
        Material* beak = beakMat_.Get();
        Material* leg = legMat_.Get();
        float combScale = 1.0f;
        switch (type) {
            case E_CHARGER: feathers = chickBrown_.Get(); combScale = 1.45f; break;
            case E_SHOOTER: feathers = chickBlack_.Get(); combScale = 0.8f; break;
            case E_BOSS:    feathers = chickBoss_.Get();  combScale = 2.0f; break;
            default: break;
        }
        if (!feathers) feathers = fallbackMat_;

        // --- туловище: яйцевидное, чуть наклонено вперёд
        Node* body = root->CreateChild("Body");
        body->SetPosition(Vector3(0, 0.86f, 0.02f));
        body->SetScale(Vector3(1.15f, 1.0f, 1.42f));
        body->SetRotation(Quaternion(-8.0f, Vector3(1, 0, 0)));
        StaticModel* bsm = body->CreateComponent<StaticModel>();
        bsm->SetModel(sph); bsm->SetMaterial(feathers); bsm->SetCastShadows(true);

        // --- ноги: голень + три пальца, анимируются в UpdateEnemies
        for (int i = 0; i < 2; ++i) {
            float sx = (i == 0) ? -0.24f : 0.24f;
            Node* hip = root->CreateChild(i == 0 ? "HipL" : "HipR");
            hip->SetPosition(Vector3(sx, 0.44f, 0.02f));
            AddPart(hip, Vector3(0, -0.20f, 0), Vector3(0.11f, 0.42f, 0.11f), leg, cyl, true);
            for (int t = 0; t < 3; ++t) {
                float toeAng = (float)(t - 1) * 26.0f;
                Node* toe = AddPart(hip, Vector3(0, -0.40f, 0.08f), Vector3(0.05f, 0.035f, 0.24f), leg);
                toe->SetRotation(Quaternion(toeAng, Vector3(0, 1, 0)));
            }
        }

        // --- крылья: сплюснутые сферы по бокам
        for (int i = 0; i < 2; ++i) {
            float sx = (i == 0) ? -0.60f : 0.60f;
            Node* wing = AddPart(root, Vector3(sx, 0.92f, 0.02f), Vector3(0.30f, 0.78f, 1.05f), feathers, sph, true);
            wing->SetName(i == 0 ? "WingL" : "WingR");
            wing->SetRotation(Quaternion((i == 0 ? 18.0f : -18.0f), Vector3(0, 0, 1)));
        }

        // --- хвост: веер из трёх конусов
        Node* tail = root->CreateChild("Tail");
        tail->SetPosition(Vector3(0, 1.05f, -0.68f));
        for (int t = 0; t < 3; ++t) {
            Node* featherNode = AddPart(tail, Vector3((float)(t - 1) * 0.11f, 0, 0),
                                        Vector3(0.13f, 0.5f, 0.13f), feathers, cone);
            // конус «смотрит» вверх: поворачиваем назад и разводим веером
            featherNode->SetRotation(Quaternion(-118.0f + (float)(t - 1) * 14.0f, Vector3(1, 0, 0)) *
                                     Quaternion((float)(t - 1) * 16.0f, Vector3(0, 0, 1)));
        }
        if (type == E_BOSS) { // у индюка хвост пышнее
            for (int t = 0; t < 4; ++t) {
                Node* extra = AddPart(tail, Vector3((float)(t - 2) * 0.16f + 0.08f, 0.05f, 0.02f),
                                      Vector3(0.14f, 0.62f, 0.14f), feathers, cone);
                extra->SetRotation(Quaternion(-112.0f + (float)(t - 2) * 10.0f, Vector3(1, 0, 0)) *
                                   Quaternion((float)(t - 2) * 20.0f, Vector3(0, 0, 1)));
            }
        }

        // --- шея
        Node* neck = AddPart(root, Vector3(0, 1.28f, 0.34f), Vector3(0.24f, 0.40f, 0.24f), feathers, cyl, true);
        neck->SetRotation(Quaternion(-22.0f, Vector3(1, 0, 0)));

        // --- голова + гребень + бородка + клюв
        Node* head = root->CreateChild("Head");
        head->SetPosition(Vector3(0, CH_HEAD_Y, CH_HEAD_Z));
        Node* skull = AddPart(head, Vector3(0, 0, 0), Vector3(0.56f, 0.55f, 0.60f), feathers, sph, true);

        for (int cIdx = 0; cIdx < 3; ++cIdx) {  // гребень «короной»
            Node* crest = AddPart(head, Vector3(0, 0.28f * combScale - 0.03f, -0.10f + (float)cIdx * 0.13f),
                                  Vector3(0.11f * combScale, 0.20f * combScale, 0.11f * combScale), comb, cone);
            crest->SetRotation(Quaternion((float)(cIdx - 1) * 12.0f, Vector3(1, 0, 0)));
        }
        AddPart(head, Vector3(0, -0.16f, 0.22f), Vector3(0.13f, 0.17f, 0.11f), comb, sph); // бородка

        Node* beakUpper = AddPart(head, Vector3(0, 0.02f, 0.34f), Vector3(0.17f, 0.26f, 0.17f), beak, cone);
        beakUpper->SetRotation(Quaternion(90.0f, Vector3(1, 0, 0)));  // конус вперёд (+Z)

        // --- глаза: те же glow-материалы, что были у роботов
        Material* em = (type == E_CHARGER) ? eyeMatCharger_.Get()
                      : (type == E_SHOOTER) ? eyeMatShooter_.Get()
                      : (type == E_BOSS) ? eyeMatBoss_.Get() : eyeMatMelee_.Get();
        if (!em) em = comb;
        for (int i = 0; i < 2; ++i) {
            float sx = (i == 0) ? -0.17f : 0.17f;
            Node* eye = head->CreateChild(i == 0 ? "EyeL" : "EyeR");
            eye->SetPosition(Vector3(sx, 0.08f, 0.24f));
            eye->SetScale(Vector3(0.14f, 0.14f, 0.10f));
            StaticModel* esm = eye->CreateComponent<StaticModel>();
            esm->SetModel(sph); esm->SetMaterial(em); esm->SetCastShadows(false);
            // зрачок: тёмная точка, из-за неё взгляд становится «злым»
            AddPart(eye, Vector3(0, 0, 0.36f), Vector3(0.42f, 0.42f, 0.30f),
                    weaponDarkMat_ ? weaponDarkMat_.Get() : comb, sph);
        }
        (void)skull;
        return head;
    }

    void BuildWeaponModel(int type) {
        if (!weaponModelNode_) return;
        weaponModelNode_->RemoveAllChildren();
        weaponModelNode_->SetPosition(Vector3::ZERO);
        weaponModelNode_->SetRotation(Quaternion::IDENTITY);
        weaponModelNode_->SetScale(Vector3::ONE);

        Material* gunMat = weaponTintMats_.Size() ? weaponTintMats_[type] : metalMaterial_;
        // Тёмный/акцентный материал кэшируются: раньше на КАЖДОЕ переключение оружия
        // создавалось два новых Material, и они навсегда оставались в tintedMatStore_.
        if (!weaponDarkMat_) weaponDarkMat_ = MakeTinted(nullptr, nullptr, Color(35, 35, 40));
        if (!weaponAccentMat_) weaponAccentMat_ = SKIN_DEFS[currentSkin_].emissive
            ? MakeTinted(nullptr, nullptr, Color(60, 255, 170))
            : MakeTinted(nullptr, nullptr, Color(25, 25, 28));
        Material* dark = weaponDarkMat_;
        Material* accent = weaponAccentMat_;
        Model* cyl = GetSubsystem<ResourceCache>()->GetResource<Model>("Models/Cylinder.mdl");
        if (!cyl) cyl = boxModel_;

        switch (type) {
        case W_PISTOL:
            AddPart(weaponModelNode_, Vector3(0, 0, 0.10f), Vector3(0.045f, 0.05f, 0.22f), gunMat);
            AddPart(weaponModelNode_, Vector3(0, -0.075f, 0.02f), Vector3(0.04f, 0.11f, 0.055f), dark);
            AddPart(weaponModelNode_, Vector3(0, 0.012f, 0.225f), Vector3(0.018f, 0.018f, 0.06f), accent);
            break;
        case W_SMG:
            AddPart(weaponModelNode_, Vector3(0, 0, 0.16f), Vector3(0.05f, 0.055f, 0.34f), gunMat);
            AddPart(weaponModelNode_, Vector3(0, -0.09f, 0.04f), Vector3(0.04f, 0.12f, 0.05f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.085f, 0.18f), Vector3(0.03f, 0.10f, 0.045f), dark);
            AddPart(weaponModelNode_, Vector3(0, 0.045f, 0.10f), Vector3(0.02f, 0.02f, 0.16f), accent);
            AddPart(weaponModelNode_, Vector3(0, 0.0f, 0.36f), Vector3(0.02f, 0.02f, 0.08f), gunMat, cyl);
            break;
        case W_RIFLE:
            AddPart(weaponModelNode_, Vector3(0, 0, 0.24f), Vector3(0.05f, 0.06f, 0.55f), gunMat);
            AddPart(weaponModelNode_, Vector3(0, -0.10f, 0.06f), Vector3(0.04f, 0.13f, 0.05f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.09f, 0.24f), Vector3(0.032f, 0.12f, 0.05f), dark);
            AddPart(weaponModelNode_, Vector3(0, 0.055f, 0.16f), Vector3(0.024f, 0.028f, 0.20f), accent);
            AddPart(weaponModelNode_, Vector3(0, 0.055f, 0.28f), Vector3(0.03f, 0.03f, 0.02f), gunMat, cyl);
            AddPart(weaponModelNode_, Vector3(0, 0.0f, 0.55f), Vector3(0.018f, 0.018f, 0.14f), gunMat, cyl);
            AddPart(weaponModelNode_, Vector3(0, -0.02f, -0.06f), Vector3(0.045f, 0.06f, 0.14f), dark);
            break;
        case W_SHOTGUN:
            AddPart(weaponModelNode_, Vector3(0, 0.012f, 0.28f), Vector3(0.028f, 0.028f, 0.55f), gunMat, cyl);
            AddPart(weaponModelNode_, Vector3(0, -0.02f, 0.26f), Vector3(0.03f, 0.03f, 0.5f), gunMat, cyl);
            AddPart(weaponModelNode_, Vector3(0, -0.015f, 0.10f), Vector3(0.06f, 0.05f, 0.22f), gunMat);
            AddPart(weaponModelNode_, Vector3(0, -0.07f, 0.02f), Vector3(0.04f, 0.11f, 0.05f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.045f, 0.30f), Vector3(0.05f, 0.035f, 0.12f), accent);
            AddPart(weaponModelNode_, Vector3(0, -0.02f, -0.10f), Vector3(0.05f, 0.07f, 0.16f), dark);
            break;
        case W_SNIPER:
            AddPart(weaponModelNode_, Vector3(0, 0, 0.30f), Vector3(0.04f, 0.05f, 0.7f), gunMat);
            AddPart(weaponModelNode_, Vector3(0, 0.0f, 0.72f), Vector3(0.015f, 0.015f, 0.2f), gunMat, cyl);
            AddPart(weaponModelNode_, Vector3(0, 0.07f, 0.26f), Vector3(0.028f, 0.028f, 0.26f), dark, cyl);
            AddPart(weaponModelNode_, Vector3(0, 0.07f, 0.40f), Vector3(0.036f, 0.036f, 0.02f), accent, cyl);
            AddPart(weaponModelNode_, Vector3(0, -0.10f, 0.08f), Vector3(0.04f, 0.12f, 0.05f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.02f, -0.12f), Vector3(0.05f, 0.07f, 0.2f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.05f, 0.5f), Vector3(0.02f, 0.06f, 0.02f), dark);
            break;
        case W_MINIGUN: {
            // Корпус и барабан из 6 стволов
            AddPart(weaponModelNode_, Vector3(0, -0.01f, 0.14f), Vector3(0.09f, 0.10f, 0.34f), gunMat);
            for (int i = 0; i < 6; ++i) {
                float ang = i * 60.0f;
                Quaternion rot(ang, Vector3(1, 0, 0));
                Vector3 off = rot * Vector3(0, 0.045f, 0);
                Node* barrel = AddPart(weaponModelNode_, Vector3(off.x_, off.y_, 0.48f), Vector3(0.02f, 0.02f, 0.34f), accent, cyl);
                barrel->SetRotation(rot);
                barrel->SetPosition(Vector3(off.x_, off.y_, 0.48f));
            }
            AddPart(weaponModelNode_, Vector3(0, -0.10f, 0.02f), Vector3(0.05f, 0.12f, 0.07f), dark);      // рукоять
            AddPart(weaponModelNode_, Vector3(0, -0.09f, 0.20f), Vector3(0.06f, 0.10f, 0.10f), dark);      // магазин-короб
            AddPart(weaponModelNode_, Vector3(0, 0.06f, 0.14f), Vector3(0.05f, 0.05f, 0.22f), dark);       // верхний кожух
            break;
        }
        case W_LAUNCHER: {
            // Труба гранатомёта + прицел + рукояти
            AddPart(weaponModelNode_, Vector3(0, 0.02f, 0.30f), Vector3(0.075f, 0.075f, 0.62f), gunMat, cyl);
            AddPart(weaponModelNode_, Vector3(0, 0.02f, 0.63f), Vector3(0.09f, 0.09f, 0.06f), accent, cyl);  // раструб
            AddPart(weaponModelNode_, Vector3(0, 0.11f, 0.22f), Vector3(0.03f, 0.05f, 0.16f), dark);          // кронштейн прицела
            AddPart(weaponModelNode_, Vector3(0, 0.15f, 0.22f), Vector3(0.045f, 0.03f, 0.12f), accent);       // оптика
            AddPart(weaponModelNode_, Vector3(0, -0.09f, 0.06f), Vector3(0.04f, 0.12f, 0.05f), dark);         // пистолетная рукоять
            AddPart(weaponModelNode_, Vector3(0, -0.06f, 0.34f), Vector3(0.04f, 0.06f, 0.10f), dark);         // передняя рукоять
            AddPart(weaponModelNode_, Vector3(0, -0.01f, -0.05f), Vector3(0.07f, 0.09f, 0.14f), dark);        // затыльник
            break;
        }
        }
    }

    void EquipWeapon(int type, bool full) {
        if (type < 0 || type >= WEAPON_COUNT) return;

        // Сохраняем остаток патронов текущего оружия. Без этого каждое переключение
        // 1..7 выдавало полный магазин — бесконечный боезапас.
        if (weapon_.weaponType_ >= 0 && weapon_.weaponType_ < WEAPON_COUNT && ammoInit_[weapon_.weaponType_]) {
            magAmmo_[weapon_.weaponType_] = weapon_.currentAmmo_;
            reserveAmmo_[weapon_.weaponType_] = weapon_.reserveAmmo_;
        }

        currentWeapon_ = type;
        const WeaponDef& def = WEAPON_DEFS[type];
        weapon_.weaponType_ = type;
        weapon_.isReloading_ = false;
        weapon_.reloadTimer_ = 0;
        weapon_.fireTimer_ = 0;

        // Первый доступ к слоту (или принудительный рефилл, full=true) — полный боезапас,
        // иначе восстанавливаем сохранённый остаток.
        if (!ammoInit_[type] || full) {
            magAmmo_[type] = def.magSize;
            reserveAmmo_[type] = def.reserveMax;
            ammoInit_[type] = true;
        }
        weapon_.currentAmmo_ = magAmmo_[type];
        weapon_.reserveAmmo_ = reserveAmmo_[type];
        BuildWeaponModel(type);
        UpdateAmmoDisplay();
        if (weaponNameText_) weaponNameText_->SetText(String(def.name) + " | " + String(SKIN_DEFS[currentSkin_].name));
    }

    // =========================================================================
    // Создание UI
    // =========================================================================
    
    void CreateUI() {
        auto* ui = GetSubsystem<UI>();
        
        // Создаем прицел
        CreateCrosshair();
        
        // Создаем текстовые элементы HUD
        CreateHUD();
        
        // Оверлей получения урона
        damageOverlay_ = ui->GetRoot()->CreateChild<BorderImage>();
        damageOverlay_->SetSize(GetSubsystem<Graphics>()->GetWidth(), GetSubsystem<Graphics>()->GetHeight());
        damageOverlay_->SetColor(Color(1.0f, 0.0f, 0.0f, 0.0f));
        damageOverlay_->SetBlendMode(BLEND_ADD);
        
        // Оверлей перезарядки
        reloadOverlay_ = ui->GetRoot()->CreateChild<BorderImage>();
        reloadOverlay_->SetSize(GetSubsystem<Graphics>()->GetWidth(), GetSubsystem<Graphics>()->GetHeight());
        reloadOverlay_->SetColor(Color(1.0f, 1.0f, 0.0f, 0.0f));
        reloadOverlay_->SetBlendMode(BLEND_ADD);

        // === Game feel: хитмаркер (крестик X при попадании) ===
        CreateHitMarker();

        // === Виньетка низкого здоровья (4 красные полосы по краям экрана) ===
        {
            int sw = GetSubsystem<Graphics>()->GetWidth();
            int sh = GetSubsystem<Graphics>()->GetHeight();
            const int th = 90; // толщина полосы
            BorderImage* t = ui->GetRoot()->CreateChild<BorderImage>();
            t->SetSize(sw, th); t->SetPosition(0, 0);   t->SetBlendMode(BLEND_ADD); t->SetColor(Color(1,0,0,0));
            BorderImage* b = ui->GetRoot()->CreateChild<BorderImage>();
            b->SetSize(sw, th); b->SetPosition(0, sh-th); b->SetBlendMode(BLEND_ADD); b->SetColor(Color(1,0,0,0));
            BorderImage* l = ui->GetRoot()->CreateChild<BorderImage>();
            l->SetSize(th, sh); l->SetPosition(0, 0);   l->SetBlendMode(BLEND_ADD); l->SetColor(Color(1,0,0,0));
            BorderImage* r = ui->GetRoot()->CreateChild<BorderImage>();
            r->SetSize(th, sh); r->SetPosition(sw-th,0); r->SetBlendMode(BLEND_ADD); r->SetColor(Color(1,0,0,0));
            // Храним в pendingRemoves? Нет — живут всё время игры; скрываем через SetVisible
            lowHealthVignette_ = t; // ссылка на верхнюю для цвета; остальные ищем через children root
            vignetteEdges_.Push(SharedPtr<BorderImage>(t));
            vignetteEdges_.Push(SharedPtr<BorderImage>(b));
            vignetteEdges_.Push(SharedPtr<BorderImage>(l));
            vignetteEdges_.Push(SharedPtr<BorderImage>(r));
        }

        // === Экран GAME OVER ===
        CreateGameOverPanel();

        // Запоминаем размер, под который свёрстан UI: по нему ловим ресайз окна
        auto* gfx = GetSubsystem<Graphics>();
        if (gfx) { uiWidth_ = gfx->GetWidth(); uiHeight_ = gfx->GetHeight(); }
        lastCrosshairGap_ = -1.0f;
    }

    void CreateHitMarker() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int cx = GetSubsystem<Graphics>()->GetWidth() / 2;
        int cy = GetSubsystem<Graphics>()->GetHeight() / 2;
        // 4 диагональные штриха, образующие «X» вокруг прицела
        struct Seg { int x, y, w, h; };
        const Seg segs[4] = {
            { cx - 18, cy - 18, 10, 3 },  // верх-левый
            { cx + 8,  cy - 18, 10, 3 },  // верх-правый
            { cx - 18, cy + 15, 10, 3 },  // низ-левый
            { cx + 8,  cy + 15, 10, 3 },  // низ-правый
        };
        for (int i = 0; i < 4; ++i) {
            BorderImage* bar = root->CreateChild<BorderImage>();
            bar->SetSize(segs[i].w, segs[i].h);
            bar->SetPosition(segs[i].x, segs[i].y);
            bar->SetColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
            bar->SetBlendMode(BLEND_ADD);
            hitMarkerBars_.Push(SharedPtr<UIElement>(bar));
        }
    }

    void ShowHitMarker(bool kill) {
        hitMarkerTimer_ = kill ? 0.28f : 0.16f;
        if (kill) hitKillTimer_ = 0.28f;
    }

    void UpdateHitMarker(float dt) {
        Color c(0.0f, 0.0f, 0.0f, 0.0f);
        if (hitMarkerTimer_ > 0.0f) {
            hitMarkerTimer_ -= dt;
            float a = Clamp(hitMarkerTimer_ / 0.16f, 0.0f, 1.0f);
            bool kill = hitKillTimer_ > 0.0f;
            if (kill) hitKillTimer_ -= dt;
            c = kill ? Color(1.0f, 0.25f, 0.2f, a) : Color(1.0f, 1.0f, 1.0f, a * 0.9f);
        }
        for (unsigned i = 0; i < hitMarkerBars_.Size(); ++i)
            hitMarkerBars_[i]->SetColor(c);
    }

    void UpdateLowHealthVignette(float dt) {
        float ratio = player_.maxHealth_ > 0.0f ? player_.health_ / player_.maxHealth_ : 1.0f;
        float a = 0.0f;
        if (!gameOver_ && gameMode_ == MODE_ARENA && ratio < 0.3f && ratio > 0.0f) {
            // Пульсация: быстрее, когда меньше HP
            float pulseFreq = 3.0f + (0.3f - ratio) * 20.0f;
            a = (0.25f + 0.2f * sinf(timeAcc_ * pulseFreq)) * (1.0f - ratio / 0.3f);
        }
        for (unsigned i = 0; i < vignetteEdges_.Size(); ++i)
            vignetteEdges_[i]->SetColor(Color(1.0f, 0.05f, 0.05f, a));
    }

    void CreateGameOverPanel() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        gameOverPanel_ = root->CreateChild<UIElement>("GameOverPanel");
        gameOverPanel_->SetSize(sw, sh);

        BorderImage* bg = gameOverPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.05f, 0.0f, 0.0f, 0.78f));

        Text* title = gameOverPanel_->CreateChild<Text>();
        title->SetFont(font_, 72);
        title->SetText("GAME OVER");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - 400, sh / 2 - 150);
        title->SetWidth(800);
        title->SetColor(Color(1.0f, 0.15f, 0.1f, 1.0f));
        gameOverTitle_ = SharedPtr<Text>(title);

        // Красная акцентная черта под заголовком
        BorderImage* goBar = gameOverPanel_->CreateChild<BorderImage>();
        goBar->SetPosition(sw / 2 - 200, sh / 2 - 66);
        goBar->SetSize(400, 2);
        goBar->SetColor(Color(1.0f, 0.2f, 0.15f, 0.8f));

        gameOverStatsText_ = gameOverPanel_->CreateChild<Text>();
        gameOverStatsText_->SetFont(font_, 24);
        gameOverStatsText_->SetTextAlignment(HA_CENTER);
        gameOverStatsText_->SetPosition(sw / 2 - 400, sh / 2 - 40);
        gameOverStatsText_->SetWidth(800);
        gameOverStatsText_->SetColor(Color(0.95f, 0.9f, 0.85f, 1.0f));

        Text* hint = gameOverPanel_->CreateChild<Text>();
        hint->SetFont(font_, 20);
        hint->SetText("ENTER — заново (арена)   |   ESC — главное меню");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - 400, sh / 2 + 120);
        hint->SetWidth(800);
        hint->SetColor(Color(1.0f, 0.85f, 0.3f, 1.0f));
        gameOverHint_ = SharedPtr<Text>(hint);

        gameOverPanel_->SetVisible(false);
    }

    // Пульсация экрана смерти: заголовок «бьётся» как сердце, подсказка дышит
    void AnimateGameOver(float dt) {
        if (!gameOverPanel_ || !gameOverPanel_->IsVisible()) return;
        auto* gfx = GetSubsystem<Graphics>();
        int sw = gfx->GetWidth(), sh = gfx->GetHeight();
        gameOverPulse_ += dt;
        if (gameOverTitle_) {
            float beat = powf(0.5f + 0.5f * sinf(gameOverPulse_ * 3.2f), 2.0f);
            gameOverTitle_->SetColor(Color(0.75f + 0.25f * beat, 0.1f + 0.08f * beat, 0.08f, 1.0f));
            // SetX/SetY нет в UIElement — только SetPosition (абсолютный пиксельный сдвиг)
            if (!goBasePosSet_) { goBasePos_ = gameOverTitle_->GetPosition(); goBasePosSet_ = true; }
            gameOverTitle_->SetPosition(goBasePos_.x_ + int(sinf(gameOverPulse_ * 9.0f) * beat * 3.0f),
                                        goBasePos_.y_ + int(beat * 4.0f));
        }
        if (gameOverHint_) {
            gameOverHint_->SetColor(Color(1.0f, 0.85f, 0.3f, 0.55f + 0.45f * sinf(gameOverPulse_ * 2.4f)));
        }
    }
    
    void CreateCrosshair() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        
        int screenWidth = GetSubsystem<Graphics>()->GetWidth();
        int screenHeight = GetSubsystem<Graphics>()->GetHeight();
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2;
        
        // Центральный элемент
        crosshairCenter_ = root->CreateChild<BorderImage>();
        crosshairCenter_->SetSize(4, 4);
        crosshairCenter_->SetPosition(centerX - 2, centerY - 2);
        crosshairCenter_->SetColor(Color(0.0f, 1.0f, 0.0f, 1.0f));
        
        // Верхняя линия
        crosshairTop_ = root->CreateChild<BorderImage>();
        crosshairTop_->SetSize(2, 10);
        crosshairTop_->SetPosition(centerX - 1, centerY - 15);
        crosshairTop_->SetColor(Color(0.0f, 1.0f, 0.0f, 0.8f));
        
        // Нижняя линия
        crosshairBottom_ = root->CreateChild<BorderImage>();
        crosshairBottom_->SetSize(2, 10);
        crosshairBottom_->SetPosition(centerX - 1, centerY + 5);
        crosshairBottom_->SetColor(Color(0.0f, 1.0f, 0.0f, 0.8f));
        
        // Левая линия
        crosshairLeft_ = root->CreateChild<BorderImage>();
        crosshairLeft_->SetSize(10, 2);
        crosshairLeft_->SetPosition(centerX - 15, centerY - 1);
        crosshairLeft_->SetColor(Color(0.0f, 1.0f, 0.0f, 0.8f));
        
        // Правая линия
        crosshairRight_ = root->CreateChild<BorderImage>();
        crosshairRight_->SetSize(10, 2);
        crosshairRight_->SetPosition(centerX + 5, centerY - 1);
        crosshairRight_->SetColor(Color(0.0f, 1.0f, 0.0f, 0.8f));
    }
    
    void CreateHUD() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        
        int screenWidth = GetSubsystem<Graphics>()->GetWidth();
        int screenHeight = GetSubsystem<Graphics>()->GetHeight();
        
        // Текст патронов (правый нижний угол)
        ammoText_ = root->CreateChild<Text>();
        ammoText_->SetFont(font_, 24);
        ammoText_->SetTextAlignment(HA_RIGHT);
        ammoText_->SetPosition(screenWidth - 20, screenHeight - 60);
        ammoText_->SetColor(Color(1.0f, 1.0f, 0.0f, 1.0f));
        UpdateAmmoDisplay();
        
        // Текст здоровья (левый нижний угол)
        healthText_ = root->CreateChild<Text>();
        healthText_->SetFont(font_, 24);
        healthText_->SetPosition(20, screenHeight - 60);
        healthText_->SetColor(Color(0.0f, 1.0f, 0.0f, 1.0f));
        UpdateHealthDisplay();
        
        // Текст счета (правый верхний угол)
        scoreText_ = root->CreateChild<Text>();
        scoreText_->SetFont(font_, 24);
        scoreText_->SetTextAlignment(HA_RIGHT);
        scoreText_->SetPosition(screenWidth - 20, 20);
        scoreText_->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        UpdateScoreDisplay();
        
        // Текст точности (под счетом)
        accuracyText_ = root->CreateChild<Text>();
        accuracyText_->SetFont(font_, 18);
        accuracyText_->SetTextAlignment(HA_RIGHT);
        accuracyText_->SetPosition(screenWidth - 20, 50);
        accuracyText_->SetColor(Color(0.8f, 0.8f, 0.8f, 1.0f));
        UpdateAccuracyDisplay();
        
        // Текст сообщений (центр экрана)
        messageText_ = root->CreateChild<Text>();
        messageText_->SetFont(font_, 20);
        messageText_->SetTextAlignment(HA_CENTER);
        // Без ширины/переносов текст с HA_CENTER всё равно рисовался от центра вправо
        // (и многострочные сообщения уезжали за экран).
        messageText_->SetPosition(screenWidth / 2 - 460, screenHeight / 2);
        messageText_->SetWidth(920);
        messageText_->SetWordwrap(true);
        messageText_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
        messageText_->SetEnabled(false);

        // Панель текущего оружия (низ центра)
        hudPanelBg_ = root->CreateChild<BorderImage>("HudWeaponPanel");
        hudPanelBg_->SetSize(260, 46);
        hudPanelBg_->SetPosition(screenWidth / 2 - 130, screenHeight - 70);
        hudPanelBg_->SetColor(Color(0.05f, 0.07f, 0.1f, 0.55f));

        weaponNameText_ = root->CreateChild<Text>();
        weaponNameText_->SetFont(font_, 18);
        weaponNameText_->SetTextAlignment(HA_CENTER);
        weaponNameText_->SetPosition(screenWidth / 2 - 125, screenHeight - 62);
        weaponNameText_->SetWidth(250);
        weaponNameText_->SetColor(Color(1.0f, 0.85f, 0.3f, 1.0f));
        weaponNameText_->SetText(String(WEAPON_DEFS[currentWeapon_].name) + " | " + String(SKIN_DEFS[currentSkin_].name));

        // --- Баннер волны: крупный текст по центру, появляется с масштабом ---
        waveBanner_ = root->CreateChild<Text>();
        waveBanner_->SetFont(font_, 52);
        waveBanner_->SetTextAlignment(HA_CENTER);
        waveBanner_->SetPosition(screenWidth / 2 - 400, screenHeight * 0.22f);
        waveBanner_->SetWidth(800);
        waveBanner_->SetColor(Color(1.0f, 0.85f, 0.25f, 0.0f));
        waveBanner_->SetEnabled(false);

        // --- Полоса здоровья босса (появляется, когда на арене есть индюк) ---
        bossBarBg_ = root->CreateChild<BorderImage>("BossBarBg");
        bossBarBg_->SetSize(620, 20);
        bossBarBg_->SetPosition(screenWidth / 2 - 310, 26);
        bossBarBg_->SetColor(Color(0.05f, 0.03f, 0.03f, 0.75f));

        bossBarFill_ = root->CreateChild<BorderImage>("BossBarFill");
        bossBarFill_->SetSize(620, 20);
        bossBarFill_->SetPosition(screenWidth / 2 - 310, 26);
        bossBarFill_->SetColor(Color(0.95f, 0.25f, 0.25f, 1.0f));

        bossBarText_ = root->CreateChild<Text>();
        bossBarText_->SetFont(font_, 16);
        bossBarText_->SetTextAlignment(HA_CENTER);
        bossBarText_->SetPosition(screenWidth / 2 - 310, 6);
        bossBarText_->SetWidth(620);
        bossBarText_->SetText("ИНДЮК-БОСС");
        bossBarText_->SetColor(Color(1.0f, 0.9f, 0.9f, 1.0f));

        // --- Лента событий (кто кого): левый верх, 5 последних строк ---
        killFeedText_ = root->CreateChild<Text>();
        killFeedText_->SetFont(font_, 16);
        killFeedText_->SetPosition(20, 120);
        killFeedText_->SetWidth(520);
        killFeedText_->SetWordwrap(false);
        killFeedText_->SetColor(Color(0.95f, 0.95f, 0.8f, 0.85f));

        // --- Полоски HP и патронов: читаются быстрее, чем цифры ---
        hudHpBarBg_ = root->CreateChild<BorderImage>("HpHpBarBg");
        hudHpBarBg_->SetSize(HUD_BAR_W, 10);
        hudHpBarBg_->SetPosition(20, screenHeight - 32);
        hudHpBarBg_->SetColor(Color(0.05f, 0.07f, 0.1f, 0.65f));

        hudHpBarFill_ = root->CreateChild<BorderImage>("HpBarFill");
        hudHpBarFill_->SetSize(HUD_BAR_W, 10);
        hudHpBarFill_->SetPosition(20, screenHeight - 32);
        hudHpBarFill_->SetColor(Color(0.25f, 0.95f, 0.35f, 1.0f));

        hudAmmoBarBg_ = root->CreateChild<BorderImage>("AmmoBarBg");
        hudAmmoBarBg_->SetSize(HUD_BAR_W, 10);
        hudAmmoBarBg_->SetPosition(screenWidth - 20 - HUD_BAR_W, screenHeight - 32);
        hudAmmoBarBg_->SetColor(Color(0.05f, 0.07f, 0.1f, 0.65f));

        hudAmmoBarFill_ = root->CreateChild<BorderImage>("AmmoBarFill");
        hudAmmoBarFill_->SetSize(HUD_BAR_W, 10);
        hudAmmoBarFill_->SetPosition(screenWidth - 20 - HUD_BAR_W, screenHeight - 32);
        hudAmmoBarFill_->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));

        // Подсказка управления (левый верх) — храним в helpText_, чтобы прятать вместе с HUD
        helpText_ = root->CreateChild<Text>();
        helpText_->SetFont(font_, 14);
        helpText_->SetText("WASD — движение | SHIFT — бег | SPACE — прыжок | CTRL/C — присед | ЛКМ — огонь | ПКМ — прицел | R — перезарядка | 1-7 — оружие | Q/E — скин | TAB — арсенал | ESC — пауза");
        helpText_->SetPosition(20, 90);
        helpText_->SetColor(Color(0.8f, 0.85f, 0.9f, 0.75f));
    }
    
    // =========================================================================
    // HUD-фичи: лента событий, баннер волны, полоса босса
    // =========================================================================

    // 95 -> "1:35"
    String FormatTime(float seconds) {
        int total = (int)Max(0.0f, seconds);
        int m = total / 60;
        int sec = total % 60;
        return String(m) + ":" + (sec < 10 ? "0" : "") + String(sec);
    }

    void RefreshKillFeedText() {
        if (!killFeedText_) return;
        String txt;
        for (unsigned i = 0; i < killFeed_.Size(); ++i)
            txt += killFeed_[i].text_ + "\n";
        killFeedText_->SetText(txt);
    }

    // Новые события идут сверху, список держим коротким (5 строк)
    void PushKillFeed(const String& text) {
        FeedEntry e; e.text_ = text; e.ttl_ = 4.5f;
        killFeed_.Push(e);
        for (unsigned i = killFeed_.Size() - 1; i > 0; --i)
            killFeed_[i] = killFeed_[i - 1];
        killFeed_[0] = e;
        if (killFeed_.Size() > 5) killFeed_.Resize(5);
        RefreshKillFeedText();
    }

    void UpdateKillFeed(float dt) {
        if (killFeed_.Empty()) return;
        bool changed = false;
        for (unsigned i = 0; i < killFeed_.Size(); ++i) {
            killFeed_[i].ttl_ -= dt;
            if (killFeed_[i].ttl_ <= 0.0f) changed = true;
        }
        if (changed) {
            Vector<FeedEntry> keep;
            float minTtl = 99.0f;
            for (unsigned i = 0; i < killFeed_.Size(); ++i) {
                if (killFeed_[i].ttl_ > 0.0f) { keep.Push(killFeed_[i]); minTtl = Min(minTtl, killFeed_[i].ttl_); }
            }
            killFeed_ = keep;
            RefreshKillFeedText();
            if (killFeedText_) {
                float a = Clamp(minTtl / 1.2f, 0.0f, 1.0f);
                killFeedText_->SetColor(Color(0.95f, 0.95f, 0.8f, 0.85f * (killFeed_.Empty() ? 0.0f : a)));
            }
        }
    }

    void ShowWaveBanner(const String& text, const Color& color) {
        waveBannerTimer_ = 2.4f;
        if (!waveBanner_) return;
        waveBanner_->SetText(text);
        waveBannerColor_ = Color(color.r_, color.g_, color.b_, 1.0f);
        waveBanner_->SetColor(waveBannerColor_);
        waveBanner_->SetEnabled(true);
    }

    void UpdateWaveBanner(float dt) {
        if (waveBannerTimer_ <= 0.0f) return;
        waveBannerTimer_ -= dt;
        if (!waveBanner_) return;
        float t = Clamp(waveBannerTimer_ / 2.4f, 0.0f, 1.0f);       // 1 -> 0
        float alpha = Min(1.0f, t * 2.5f) * Min(1.0f, (1.0f - t) * 6.0f + 0.15f);
        waveBanner_->SetColor(Color(waveBannerColor_.r_, waveBannerColor_.g_, waveBannerColor_.b_,
                                    Clamp(alpha, 0.0f, 1.0f)));
        if (waveBannerTimer_ <= 0.0f) waveBanner_->SetEnabled(false);
    }

    // Полоса здоровья босса: показывается, пока на арене жив хотя бы один индюк
    void UpdateBossBar() {
        if (!bossBarBg_ || !bossBarFill_) return;
        EnemyData* boss = nullptr;
        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            if (enemies_[i].alive_ && enemies_[i].type_ == E_BOSS) { boss = &enemies_[i]; break; }
        }
        bool show = (boss != nullptr) && !inMenu_ && !gameOver_;
        bossBarBg_->SetVisible(show);
        bossBarFill_->SetVisible(show);
        if (bossBarText_) bossBarText_->SetVisible(show);
        if (!show) return;
        float ratio = boss->maxHealth_ > 0.0f ? Clamp(boss->health_ / boss->maxHealth_, 0.0f, 1.0f) : 0.0f;
        bossBarFill_->SetSize(620.0f * ratio, 20);
        bossBarFill_->SetColor(ratio > 0.5f ? Color(0.95f, 0.30f, 0.25f, 1.0f)
                              : ratio > 0.25f ? Color(1.0f, 0.60f, 0.15f, 1.0f)
                                             : Color(1.0f, 0.20f, 0.20f, 1.0f));
        if (bossBarText_) bossBarText_->SetText("ИНДЮК-БОСС   " + String((int)boss->health_) + " / " + String((int)boss->maxHealth_));
    }

    void UpdateAmmoDisplay() {
        if (ammoText_) {
            String ammoStr = "PATRONS: " + String(weapon_.currentAmmo_) + " / " + String(weapon_.reserveAmmo_);
            if (weapon_.isReloading_) {
                ammoStr += " (RELOADING...)";
            }
            ammoText_->SetText(ammoStr);
            
            // Красный цвет если мало патронов
            if (weapon_.currentAmmo_ <= 5) {
                ammoText_->SetColor(Color(1.0f, 0.0f, 0.0f, 1.0f));
            } else {
                ammoText_->SetColor(Color(1.0f, 1.0f, 0.0f, 1.0f));
            }
        }
        if (hudAmmoBarFill_) {
            float ratio = WEAPON_DEFS[currentWeapon_].magSize > 0
                        ? Clamp((float)weapon_.currentAmmo_ / (float)WEAPON_DEFS[currentWeapon_].magSize, 0.0f, 1.0f)
                        : 0.0f;
            hudAmmoBarFill_->SetSize(HUD_BAR_W * ratio, 10);
            hudAmmoBarFill_->SetColor(ratio < 0.25f ? Color(1.0f, 0.25f, 0.2f, 1.0f)
                                                    : Color(1.0f, 0.85f, 0.25f, 1.0f));
        }
    }
    
    void UpdateHealthDisplay() {
        if (healthText_) {
            String healthStr = "HEALTH: " + String((int)player_.health_) + " / " + String((int)player_.maxHealth_);
            healthText_->SetText(healthStr);
            
            // Цвет зависит от здоровья
            float healthRatio = player_.health_ / player_.maxHealth_;
            if (healthRatio > 0.6f) {
                healthText_->SetColor(Color(0.0f, 1.0f, 0.0f, 1.0f));
            } else if (healthRatio > 0.3f) {
                healthText_->SetColor(Color(1.0f, 1.0f, 0.0f, 1.0f));
            } else {
                healthText_->SetColor(Color(1.0f, 0.0f, 0.0f, 1.0f));
            }
        }
        if (hudHpBarFill_) {
            float ratio = player_.maxHealth_ > 0.0f ? Clamp(player_.health_ / player_.maxHealth_, 0.0f, 1.0f) : 0.0f;
            hudHpBarFill_->SetSize(HUD_BAR_W * ratio, 10);
            hudHpBarFill_->SetColor(ratio > 0.6f ? Color(0.25f, 0.95f, 0.35f, 1.0f)
                                   : ratio > 0.3f ? Color(1.0f, 0.85f, 0.2f, 1.0f)
                                                  : Color(1.0f, 0.25f, 0.2f, 1.0f));
        }
    }
    
    void UpdateScoreDisplay() {
        if (scoreText_) {
            String s = "SCORE: " + String(player_.score_) + " | KILLS: " + String(player_.kills_);
            if (comboCount_ > 1) s += " | COMBO x" + String(comboCount_);
            s += "\nRECORD: " + String(highScore_);
            if (gameMode_ == MODE_ARENA) s += " | WAVE " + String(waveNum_);
            scoreText_->SetText(s);
            scoreText_->SetColor(comboCount_ > 1 ? Color(1.0f, 0.6f, 0.1f, 1.0f) : Color(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    
    void UpdateAccuracyDisplay() {
        if (accuracyText_) {
            accuracyText_->SetText("ACCURACY: " + String((int)player_.GetAccuracy()) + "%");
        }
    }
    
    void ShowMessage(const String& message) {
        if (messageText_) {
            messageText_->SetText(message);
            messageText_->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
            messageText_->SetEnabled(true);
            messageTimer_ = 5.0f;  // Показывать 5 секунд
        }
    }

    // Прицел «дышит» вместе с реальным разбросом оружия (раньше был статичный крестик)
    void UpdateCrosshairSpread() {
        if (!crosshairCenter_ || !crosshairTop_ || !crosshairBottom_ || !crosshairLeft_ || !crosshairRight_) return;
        auto* gfx = GetSubsystem<Graphics>();
        if (!gfx) return;
        const int cx = gfx->GetWidth() / 2;
        const int cy = gfx->GetHeight() / 2;

        float spread = WEAPON_DEFS[currentWeapon_].spread;
        spread *= (1.0f - player_.GetAccuracy() / 200.0f);
        if (isAiming_) spread *= 0.35f;
        if (isCrouching_) spread *= 0.7f;
        if (!onGround_) spread *= 1.6f;

        float gap = 10.0f + spread * 780.0f;   // ~12..46 px
        if (fabsf(gap - lastCrosshairGap_) < 0.5f) return;
        lastCrosshairGap_ = gap;
        int g = (int)gap;
        crosshairTop_->SetPosition(cx - 1, cy - g - 10);
        crosshairBottom_->SetPosition(cx - 1, cy + g);
        crosshairLeft_->SetPosition(cx - g - 10, cy - 1);
        crosshairRight_->SetPosition(cx + g, cy - 1);
    }

    // =========================================================================
    // ПАУЗА (Esc в игре)
    // =========================================================================

    // Полноэкранная справка: F1 — открыть/закрыть, Esc — закрыть
    void CreateHelpPanel() {
        if (!helpPanel_) {
            auto* ui = GetSubsystem<UI>();
            auto* root = ui->GetRoot();
            int sw = GetSubsystem<Graphics>()->GetWidth();
            int sh = GetSubsystem<Graphics>()->GetHeight();

            helpPanel_ = root->CreateChild<UIElement>("HelpPanel");
            helpPanel_->SetSize(sw, sh);

            BorderImage* bg = helpPanel_->CreateChild<BorderImage>();
            bg->SetSize(sw, sh);
            bg->SetColor(Color(0.01f, 0.02f, 0.04f, 0.86f));

            Text* title = helpPanel_->CreateChild<Text>();
            title->SetFont(font_, 40);
            title->SetText("CHIKENGUN 2.1 — СПРАВКА");
            title->SetTextAlignment(HA_CENTER);
            title->SetPosition(sw / 2 - 500, sh / 2 - 250);
            title->SetWidth(1000);
            title->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));

            Text* body = helpPanel_->CreateChild<Text>();
            body->SetFont(font_, 20);
            body->SetPosition(sw / 2 - 480, sh / 2 - 190);
            body->SetWidth(960);
            body->SetWordwrap(false);
            body->SetColor(Color(0.9f, 0.93f, 0.97f, 1.0f));
            body->SetText(
                "УПРАВЛЕНИЕ\n"
                "  WASD — движение      SHIFT — бег        SPACE — прыжок      CTRL / C — присед\n"
                "  ЛКМ — огонь          ПКМ — прицел       R — перезарядка     1..7 — оружие\n"
                "  Q / E — скин оружия  TAB — арсенал      F — тир / арена      ESC — пауза\n"
                "  F1 — эта справка     M — меню (пауза)  Q — выход (пауза)\n"
                "\n"
                "АРЕНА\n"
                "  Волны кур: белая клуша, рыжий петух (быстрый), чёрный стрелок (яйца),\n"
                "  индюк-босс каждые 5 волн — полоса его здоровья сверху экрана.\n"
                "  Яйцо с крестом — аптечка +25 HP.  Ящик с лентой — патроны +2 магазина.\n"
                "  Убийства подряд без пауз дают комбо-множитель очков (до x5 и больше).\n"
                "\n"
                "ТИР\n"
                "  Мишени на стенах, на опорах и движущиеся. Зачистил все — они вернутся\n"
                "  через несколько секунд, счёт и точность продолжают копиться.\n"
                "\n"
                "СЕЙВ\n"
                "  settings.ini рядом с бинарником: рекорд, громкость, графика, чувствительность.");

            Text* hint = helpPanel_->CreateChild<Text>();
            hint->SetFont(font_, 18);
            hint->SetText("F1 или ESC — закрыть");
            hint->SetTextAlignment(HA_CENTER);
            hint->SetPosition(sw / 2 - 300, sh / 2 + 230);
            hint->SetWidth(600);
            hint->SetColor(Color(1.0f, 0.85f, 0.3f, 1.0f));
        }
        helpPanel_->SetVisible(true);
        helpOpen_ = true;
    }

    void ToggleHelp() {
        if (helpOpen_) {
            if (helpPanel_) helpPanel_->SetVisible(false);
            helpOpen_ = false;
        } else {
            CreateHelpPanel();
        }
    }

    void CreatePausePanel() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        pausePanel_ = root->CreateChild<UIElement>("PausePanel");
        pausePanel_->SetSize(sw, sh);

        BorderImage* bg = pausePanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.01f, 0.02f, 0.035f, 0.72f));

        Text* title = pausePanel_->CreateChild<Text>();
        title->SetFont(font_, 64);
        title->SetText("ПАУЗА");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - 400, sh / 2 - 150);
        title->SetWidth(800);
        title->SetColor(Color(0.55f, 0.95f, 1.0f, 1.0f));

        BorderImage* bar = pausePanel_->CreateChild<BorderImage>();
        bar->SetPosition(sw / 2 - 180, sh / 2 - 60);
        bar->SetSize(360, 2);
        bar->SetColor(Color(0.3f, 0.85f, 1.0f, 0.8f));

        Text* stats = pausePanel_->CreateChild<Text>();
        stats->SetFont(font_, 22);
        stats->SetTextAlignment(HA_CENTER);
        stats->SetPosition(sw / 2 - 400, sh / 2 - 20);
        stats->SetWidth(800);
        stats->SetColor(Color(0.9f, 0.92f, 0.95f, 1.0f));
        stats->SetText("ВРЕМЯ: " + FormatTime(arenaTime_) + "\n" +
                       "СЧЁТ: " + String(player_.score_) + "   |   УБИЙСТВ: " + String(player_.kills_) +
                       "   |   ТОЧНОСТЬ: " + String((int)player_.GetAccuracy()) + "%\n" +
                       "РЕЖИМ: " + String(gameMode_ == MODE_ARENA ? "АРЕНА" : "ТИР") +
                       (gameMode_ == MODE_ARENA ? ("   |   ВОЛНА: " + String(waveNum_)) : String()));

        Text* hint = pausePanel_->CreateChild<Text>();
        hint->SetFont(font_, 20);
        hint->SetText("ESC / ENTER — продолжить   |   M — в главное меню   |   Q — выход");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - 460, sh / 2 + 90);
        hint->SetWidth(920);
        hint->SetColor(Color(1.0f, 0.85f, 0.3f, 1.0f));

        pausePanel_->SetVisible(false);
    }

    void TogglePause() {
        if (gameOver_ || inMenu_) return;
        paused_ = !paused_;
        auto* input = GetSubsystem<Input>();
        if (paused_) {
            // Строим панель заново: в ней отображаются актуальные очки/волна/точность
            if (pausePanel_) { pausePanel_->Remove(); pausePanel_ = nullptr; }
            CreatePausePanel();
            if (pausePanel_) pausePanel_->SetVisible(true);
            SetGameHudVisible(false);
            if (input) input->SetMouseVisible(false);
        } else {
            if (pausePanel_) pausePanel_->SetVisible(false);
            SetGameHudVisible(true);
        }
    }

    // =========================================================================
    // ПЕРЕСБОРКА UI ПРИ РЕСАЙЗЕ ОКНА
    // =========================================================================

    void RebuildUI() {
        if (rebuildingUI_) return;
        rebuildingUI_ = true;

        auto* ui = GetSubsystem<UI>();
        if (!ui) { rebuildingUI_ = false; return; }
        UIElement* root = ui->GetRoot();
        if (root) root->RemoveAllChildren();

        // Сбрасываем все ссылки на удалённые элементы, иначе create-функции ничего не создадут
        crosshairCenter_ = nullptr; crosshairTop_ = nullptr; crosshairBottom_ = nullptr;
        crosshairLeft_ = nullptr; crosshairRight_ = nullptr;
        ammoText_ = nullptr; healthText_ = nullptr; scoreText_ = nullptr;
        accuracyText_ = nullptr; messageText_ = nullptr;
        damageOverlay_ = nullptr; reloadOverlay_ = nullptr;
        hitMarkerBars_.Clear();
        vignetteEdges_.Clear(); lowHealthVignette_ = nullptr;
        gameOverPanel_ = nullptr; gameOverStatsText_ = nullptr;
        gameOverTitle_ = nullptr; gameOverHint_ = nullptr;
        goBasePosSet_ = false;
        hudPanelBg_ = nullptr; helpText_ = nullptr; weaponNameText_ = nullptr;
        hudHpBarBg_ = nullptr; hudHpBarFill_ = nullptr; hudAmmoBarBg_ = nullptr; hudAmmoBarFill_ = nullptr;
        waveBanner_ = nullptr; bossBarBg_ = nullptr; bossBarFill_ = nullptr; bossBarText_ = nullptr;
        killFeedText_ = nullptr;
        helpPanel_ = nullptr; helpOpen_ = false;
        mainMenuPanel_ = nullptr; mainMenuItems_.Clear();
        menuTitle_ = nullptr; menuSub_ = nullptr; menuHint_ = nullptr;
        menuPlate_ = nullptr; menuAccentBar_ = nullptr;
        menuParticles_.Clear(); menuParticlePhase_.Clear(); menuParticleX_.Clear(); menuParticleSpeed_.Clear();
        menuRows_.Clear(); menuBasesSet_ = false;
        settingsPanel_ = nullptr; settingsItems_.Clear();
        armoryPanel_ = nullptr; armoryWeaponRows_.Clear(); armorySkinRows_.Clear();
        pausePanel_ = nullptr;

        CreateUI();
        CreateMainMenu();

        SetGameHudVisible(!inMenu_ && !gameOver_ && !paused_);
        if (mainMenuPanel_) mainMenuPanel_->SetVisible(inMenu_);
        if (inSettings_) { CreateSettingsPanel(); if (settingsPanel_) settingsPanel_->SetVisible(true); }
        if (armoryOpen_) { CreateArmoryMenu(); if (armoryPanel_) armoryPanel_->SetVisible(true); }
        if (gameOverPanel_) gameOverPanel_->SetVisible(gameOver_);
        if (paused_) { CreatePausePanel(); if (pausePanel_) pausePanel_->SetVisible(true); }
        UpdateMainMenuSelection();

        rebuildingUI_ = false;
    }

    // =========================================================================
    // Настройка ввода
    // =========================================================================
    
    void SetupInput() {
        auto* input = GetSubsystem<Input>();
        auto* graphics = GetSubsystem<Graphics>();
        
        // Скрываем курсор
        input->SetMouseVisible(false);
        isMouseVisible_ = false;
        
        // Центрируем мышь
        input->SetMousePosition(IntVector2(graphics->GetWidth() / 2, graphics->GetHeight() / 2));
    }
    
    void SubscribeToEvents() {
        SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(MyApp, HandleUpdate));
        SubscribeToEvent(E_POSTUPDATE, URHO3D_HANDLER(MyApp, HandlePostUpdate));
        SubscribeToEvent(E_MOUSEMOVE, URHO3D_HANDLER(MyApp, HandleMouseMove));
        SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(MyApp, HandleKeyDown));
        SubscribeToEvent(E_KEYUP, URHO3D_HANDLER(MyApp, HandleKeyUp));
        SubscribeToEvent(E_MOUSEBUTTONDOWN, URHO3D_HANDLER(MyApp, HandleMouseButtonDown));
        SubscribeToEvent(E_MOUSEBUTTONUP, URHO3D_HANDLER(MyApp, HandleMouseButtonUp));
    }

    // =========================================================================
    // Обработчики событий
    // =========================================================================
    
    void HandleUpdate(StringHash eventType, VariantMap& eventData) {
        using namespace Update;
        
        float dt = eventData[P_TIMESTEP].GetFloat();
        // Страховка: большой шаг (окно после сворачивания) не должен «teleport'ить» ИИ/снаряды
        if (dt > 0.1f) dt = 0.1f;
        timeAcc_ += dt;

        // Ресайз окна: раньше UI строился один раз под стартовое разрешение и «уезжал».
        // Событие не нужно — сравниваем текущий размер с тем, под который свёрстан UI.
        {
            auto* gfx = GetSubsystem<Graphics>();
            if (gfx && (gfx->GetWidth() != uiWidth_ || gfx->GetHeight() != uiHeight_)) {
                uiWidth_ = gfx->GetWidth();
                uiHeight_ = gfx->GetHeight();
                pendingUiRebuild_ = true;
            }
        }
        if (pendingUiRebuild_) {
            pendingUiRebuild_ = false;
            RebuildUI();
        }

        // Пауза замораживает игровой мир (UI при этом остаётся живым)
        if (paused_ || helpOpen_) return;   // пауза и справка замораживают мир

        // Анимации интерфейса (главное меню, экран смерти)
        AnimateMainMenu(dt);
        AnimateGameOver(dt);
        Vector<PendingRemove> alive;
        for (unsigned i = 0; i < pendingRemoves_.Size(); ++i) {
            PendingRemove pr = pendingRemoves_[i];
            pr.ttl_ -= dt;
            if (pr.ttl_ <= 0.0f) { if (pr.node_) pr.node_->Remove(); }
            else alive.Push(pr);
        }
        pendingRemoves_ = alive;

        // Уплотняем реестр коллайдеров, когда накопилось достаточно «могил»
        if (colliderGarbage_ > 64) CompactColliders();

        auto* input = GetSubsystem<Input>();
        
        // Обновляем таймеры
        if (messageTimer_ > 0) {
            messageTimer_ -= dt;
            if (messageTimer_ <= 0) {
                messageText_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
                messageText_->SetEnabled(false);
            }
        }
        
        if (damageFlashTimer_ > 0) {
            damageFlashTimer_ -= dt;
            float alpha = damageFlashTimer_ / 0.3f;
            damageOverlay_->SetColor(Color(1.0f, 0.0f, 0.0f, alpha * 0.5f));
        } else {
            damageOverlay_->SetColor(Color(1.0f, 0.0f, 0.0f, 0.0f));
        }
        
        // Обновляем перезарядку
        if (weapon_.isReloading_) {
            weapon_.reloadTimer_ -= dt;
            
            // Анимация прозрачности для оверлея перезарядки
            float reloadAlpha = 0.3f + 0.2f * sin(timeAcc_ * 10.0f);
            reloadOverlay_->SetColor(Color(1.0f, 1.0f, 0.0f, reloadAlpha));
            
            if (weapon_.reloadTimer_ <= 0) {
                CompleteReload();
            }
        }
        
        // Обновляем таймер стрельбы
        if (weapon_.fireTimer_ > 0) {
            weapon_.fireTimer_ -= dt;
        }

        // Автоогонь: удержание ЛКМ (не для дробовика/снайперки/гранатомёта)
        if (!inMenu_ && !gameOver_ && !paused_ && !helpOpen_ && !armoryOpen_ && !isMouseVisible_ && input->GetMouseButtonDown(MOUSEB_LEFT)) {
            int wt = currentWeapon_;
            if (wt != W_SHOTGUN && wt != W_SNIPER && wt != W_LAUNCHER) {
                TryFire();
            }
        }

        // === Замедление движения при стрельбе из минигана (баланс долгой перезарядки) ===
        minigunSpinupActive_ = (currentWeapon_ == W_MINIGUN && weapon_.fireTimer_ > 0.0f);
        
        // Восстанавливаем отдачу
        weapon_.recoilX_ = Lerp(weapon_.recoilX_, 0.0f, GameConstants::RECOIL_RECOVERY * dt);
        weapon_.recoilY_ = Lerp(weapon_.recoilY_, 0.0f, GameConstants::RECOIL_RECOVERY * dt);
        
        // === ОБРАБОТКА ВВОДА ДЛЯ ДВИЖЕНИЯ ===
        // В меню/GAME OVER игрок не должен ходить: раньше WASD двигал камеру за панелью меню
        if (!gameOver_ && !inMenu_) HandleMovement(dt);

        // Сначала покачивание, ПОТОМ отдача: ApplyRecoilToWeapon() перезаписывает позицию
        // weaponNode_, поэтому при обратном порядке отдача визуально съедалась бобом.
        UpdateWeaponBob(dt);
        ApplyRecoilToWeapon();

        // Гасим вспышку дула, когда её таймер истёк
        if (muzzleFlashTimer_ > 0.0f) {
            muzzleFlashTimer_ -= dt;
            if (muzzleFlashTimer_ <= 0.0f && muzzleLightNode_) muzzleLightNode_->SetEnabled(false);
        }

        // === Game feel: хитмаркер, виньетка низкого HP, динамический прицел ===
        UpdateCrosshairSpread();
        UpdateHitMarker(dt);
        UpdateLowHealthVignette(dt);
        UpdateTracers(dt);
        UpdateEnemyProjectiles(dt);
        UpdateMovingTargetsLite(dt);
        UpdateDebrisLite(dt);
        UpdateAnimatedDecor(dt);
        UpdateShockwaves(dt);
        UpdateKillFeed(dt);
        UpdateWaveBanner(dt);
        UpdateBossBar();
        if (!inMenu_ && !gameOver_ && gameMode_ == MODE_ARENA) arenaTime_ += dt;
        UpdatePickups(dt);

        // (Раньше здесь камера «крутилась» в меню — из-за этого игра стартовала
        //  со случайно повёрнутым на пару градусов взглядом.)

        // Комбо-таймер: серия убийств живёт 4 секунды
        if (comboTimer_ > 0.0f) {
            comboTimer_ -= dt;
            if (comboTimer_ <= 0.0f && comboCount_ > 0) {
                comboCount_ = 0;
                UpdateScoreDisplay();
            }
        }

        if (!inMenu_) {
            if (gameMode_ == MODE_ARENA) {
                UpdateEnemies(dt);
                int aliveCount = 0;
                for (unsigned i = 0; i < enemies_.Size(); ++i) if (enemies_[i].alive_) aliveCount++;
                if (aliveCount == 0) {
                    waveRespawnTimer_ += dt;
                    if (waveRespawnTimer_ > 3.0f) { waveRespawnTimer_ = 0.0f; SpawnWave(); }
                }
            } else if (gameMode_ == MODE_RANGE) {
                // Чистим уничтоженные мишени: иначе targets_ и реестр коллайдеров
                // растут без предела (каждая новая мишень — ещё один BoxCollider в рейкастах).
                for (unsigned i = targets_.Size(); i-- > 0; ) {
                    if (!targets_[i].isActive_) {
                        RemoveColliderLite(targets_[i].colliderIdx_);
                        targets_.Erase(i);
                    }
                }
                int activeCount = 0;
                for (unsigned i = 0; i < targets_.Size(); ++i) if (targets_[i].isActive_) activeCount++;
                if (activeCount < 3) {
                    rangeRespawnTimer_ += dt;
                    if (rangeRespawnTimer_ > 8.0f) {
                        rangeRespawnTimer_ = 0.0f;
                        float rx = (float)(rand() % 30 - 15);
                        float rz = (float)(rand() % 30 - 15);
                        CreateTarget(Vector3(rx, 1.5f, rz), Vector3(0, 0, 1));
                    }
                }
            }
        }
    }
    
    void HandlePostUpdate(StringHash eventType, VariantMap& eventData) {
        // Отладочная информация
        #ifdef DEBUG
        auto* debugRenderer = scene_->GetComponent<DebugRenderer>();
        if (debugRenderer) {
            // Рисуем лучи от пуль для отладки
        }
        #endif
    }
    
    void HandleMouseMove(StringHash eventType, VariantMap& eventData) {
        using namespace MouseMove;
        
        // Если мышь видима (меню) или игра на паузе, не обрабатываем вращение
        if (isMouseVisible_ || paused_ || helpOpen_) {
            return;
        }
        
        auto* input = GetSubsystem<Input>();
        
        // Получаем смещение мыши
        IntVector2 mouseMove = input->GetMouseMove();
        
        // Обновляем углы Эйлера
        // Чувствительность из настроек (низкая/обычная/высокая)
        static const float kSensMult[3] = { 0.55f, 1.0f, 1.65f };
        float sens = GameConstants::MOUSE_SENSITIVITY * kSensMult[Clamp(mouseSensIdx_, 0, 2)];
        if (isAiming_) sens *= 0.45f; // точное прицеливание мышью (ПКМ)
        if (isCrouching_) sens *= 0.8f;
        yaw_ += (float)mouseMove.x_ * sens;
        pitch_ += (float)mouseMove.y_ * sens;
        
        // Ограничиваем вертикальный угол
        pitch_ = Clamp(pitch_, -GameConstants::MAX_PITCH, GameConstants::MAX_PITCH);
        
        // Позицию и вращение камеры применяет динамический контроллер
    }
    
    void HandleKeyDown(StringHash eventType, VariantMap& eventData) {
        using namespace KeyDown;
        
        int key = eventData[P_KEY].GetI32();

        // ===== Экран GAME OVER перехватывает весь ввод =====
        if (gameOver_) {
            if (key == KEY_RETURN || key == KEY_SPACE) { PlayUISound(); RestartArena(); }
            else if (key == KEY_ESCAPE) { PlayUISound(); ShowGameOver(false); ShowMainMenu(true); }
            return;
        }

        // ===== Главное меню перехватывает весь ввод =====
        if (inMenu_) {
            if (inSettings_) {
                if (key == KEY_UP || key == KEY_W) { settingsSel_ = (settingsSel_ + 7) % 8; PlayUISound(); UpdateSettingsDisplay(); }
                else if (key == KEY_DOWN || key == KEY_S) { settingsSel_ = (settingsSel_ + 1) % 8; PlayUISound(); UpdateSettingsDisplay(); }
                else if (key == KEY_LEFT || key == KEY_A) { ChangeSetting(-1); }
                else if (key == KEY_RIGHT || key == KEY_D) { ChangeSetting(1); }
                else if (key == KEY_RETURN) { if (settingsSel_ == 7) ShowSettings(false); else ChangeSetting(1); }
                else if (key == KEY_ESCAPE) { ShowSettings(false); }
                return;
            }
            if (key == KEY_UP || key == KEY_W) { menuSel_ = (menuSel_ + 3) % 4; PlayUISound(); UpdateMainMenuSelection(); }
            else if (key == KEY_DOWN || key == KEY_S) { menuSel_ = (menuSel_ + 1) % 4; PlayUISound(); UpdateMainMenuSelection(); }
            else if (key == KEY_RETURN || key == KEY_SPACE) {
                if (menuSel_ == 0) StartGameMode(MODE_RANGE);
                else if (menuSel_ == 1) StartGameMode(MODE_ARENA);
                else if (menuSel_ == 2) ShowSettings(true);
                else engine_->Exit();
            }
            else if (key == KEY_ESCAPE) engine_->Exit();
            return;
        }

        // ===== Пауза перехватывает ввод =====
        if (paused_) {
            if (key == KEY_F1) { PlayUISound(); ToggleHelp(); return; }
            if (key == KEY_ESCAPE || key == KEY_RETURN) TogglePause();
            else if (key == KEY_M) { TogglePause(); ShowMainMenu(true); }
            else if (key == KEY_Q) engine_->Exit();
            return;
        }

        // F1 — справка (работает и в игре, и в паузе)
        if (key == KEY_F1) { PlayUISound(); ToggleHelp(); return; }

        // F — быстрая смена режима (тир <-> арена)
        if (key == KEY_F) {
            StartGameMode(gameMode_ == MODE_RANGE ? MODE_ARENA : MODE_RANGE);
            return;
        }
        
        // Бег
        if (key == KEY_LSHIFT || key == KEY_RSHIFT) {
            isSprinting_ = !isCrouching_; // из приседа не разбегаемся
        }
        
        // Перезарядка
        if (key == KEY_R && !inMenu_ && !weapon_.isReloading_) {
            StartReload();
        }
        
        // Скрыть/показать мышь (для отладки)
        if (key == KEY_TAB) { ToggleArmory(); return; }

        // Esc: закрыть справку -> арсенал -> пауза
        if (key == KEY_ESCAPE) {
            if (helpOpen_) { ToggleHelp(); }
            else if (armoryOpen_) { ToggleArmory(); }
            else { TogglePause(); }
            return;
        }

        // 1..7 — быстрое переключение оружия
        if (key >= KEY_1 && key <= KEY_7) {
            int idx = key - KEY_1;
            if (idx < WEAPON_COUNT) { EquipWeapon(idx, false); ShowMessage(String("Оружие: ") + WEAPON_DEFS[idx].name); }
            return;
        }

        // Q — смена скина
        if (key == KEY_Q) { CycleSkin(1); return; }
        if (key == KEY_E) { CycleSkin(-1); return; }
    }
    
    void HandleKeyUp(StringHash eventType, VariantMap& eventData) {
        using namespace KeyUp;
        
        int key = eventData[P_KEY].GetI32();
        
        if (key == KEY_LSHIFT || key == KEY_RSHIFT) {
            isSprinting_ = false;
        }
    }
    
    void HandleMouseButtonDown(StringHash eventType, VariantMap& eventData) {
        using namespace MouseButtonDown;
        
        int button = eventData[P_BUTTON].GetI32();
        
        // Меню арсенала перехватывает клики
        if (armoryOpen_) {
            HandleArmoryClick(button == MOUSEB_LEFT);
            return;
        }

        // Левая кнопка мыши — стрельба. Только в игре: раньше клик в главном меню
        // или на экране GAME OVER стрелял (звук, расход патронов, тряска).
        if (button == MOUSEB_LEFT && !inMenu_ && !gameOver_ && !paused_ && !helpOpen_ && !isMouseVisible_) {
            TryFire();
        }
        
        // Правая кнопка мыши - прицеливание (ADS): FOV сужается, мышь точнее, разброс меньше
        if (button == MOUSEB_RIGHT) {
            isAiming_ = true;
        }
    }
    
    void HandleMouseButtonUp(StringHash eventType, VariantMap& eventData) {
        using namespace MouseButtonUp;
        
        int button = eventData[P_BUTTON].GetI32();
        
        if (button == MOUSEB_RIGHT) {
            isAiming_ = false;
        }
    }

    // =========================================================================
    // Игровая логика
    // =========================================================================
    
    // =========================================================================
    // Контроллер игрока: движение + прыжки (Space) + гравитация + присед (Ctrl/C)
    // Пол определяется рейкастом вниз — можно запрыгнуть на платформу/ящик.
    // =========================================================================

    float ProbeGround(const Vector3& feetPos) {
        // Ищем верхнюю поверхность под ногами (4 луча по краям «стопы») — Lite-рейкаст
        float bestY = -1e6f;
        const float probeTop = feetPos.y_ + eyeHeight_ * 0.5f;
        const float probeLen = eyeHeight_ + 3.0f;
        for (int sx = -1; sx <= 1; sx += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                Vector3 offset(sx * 0.22f, 0.0f, sz * 0.22f);
                float dist; Vector3 hitPoint;
                if (RaycastLite(Vector3(feetPos.x_ + offset.x_, probeTop, feetPos.z_ + offset.z_),
                                Vector3(0, -1, 0), probeLen, 1 | 8 | 16, dist, hitPoint)) {
                    bestY = Max(bestY, hitPoint.y_);
                }
            }
        }
        return bestY;
    }

    bool HeadBlocked(const Vector3& feetPos) {
        // Нет ли препятствия над головой (нельзя встать из приседа под низкие объекты)
        float dist; Vector3 hitPoint;
        if (RaycastLite(Vector3(feetPos.x_, feetPos.y_ + 0.1f, feetPos.z_),
                        Vector3(0, 1, 0), GameConstants::PLAYER_HEIGHT + 0.1f, 1 | 8 | 16, dist, hitPoint)) {
            return dist > 0.05f && dist < GameConstants::PLAYER_HEIGHT;
        }
        return false;
    }

    void HandleMovement(float dt) {
        auto* input = GetSubsystem<Input>();

        Vector3 camPos = cameraNode_->GetPosition();
        Vector3 feetPos(camPos.x_, camPos.y_ - eyeHeight_, camPos.z_);

        // --- Направление ввода (горизонтальная плоскость взгляда) ---
        Quaternion yawRot(0.0f, yaw_, 0.0f);
        Vector3 forward = yawRot * Vector3(0.0f, 0.0f, 1.0f);
        Vector3 right = yawRot * Vector3(1.0f, 0.0f, 0.0f);

        Vector3 wishDir = Vector3::ZERO;
        if (input->GetKeyDown(KEY_W)) wishDir += forward;
        if (input->GetKeyDown(KEY_S)) wishDir -= forward;
        if (input->GetKeyDown(KEY_D)) wishDir += right;
        if (input->GetKeyDown(KEY_A)) wishDir -= right;
        if (wishDir.LengthSquared() > 0.0f) wishDir.Normalize();

        // --- Приседание (Ctrl или C): плавная высота глаз + замедление ---
        bool wantCrouch = input->GetKeyDown(KEY_LCTRL) || input->GetKeyDown(KEY_RCTRL) || input->GetKeyDown(KEY_C);
        if (!wantCrouch && isCrouching_ && HeadBlocked(feetPos)) {
            wantCrouch = true; // под низким препятствием не даём встать
        }
        isCrouching_ = wantCrouch;
        float targetEye = isCrouching_ ? GameConstants::PLAYER_HEIGHT_CROUCH : GameConstants::PLAYER_HEIGHT;
        eyeHeight_ = Lerp(eyeHeight_, targetEye, Min(1.0f, GameConstants::CROUCH_TRANSITION_SPEED * dt));

        // --- Целевая скорость (бег Shift быстрее, присед медленнее, прицел тише) ---
        float speed = GameConstants::PLAYER_SPEED;
        if (isCrouching_) speed *= GameConstants::PLAYER_CROUCH_MULTIPLIER;
        else if (isSprinting_ && wishDir.DotProduct(forward) > 0.0f && !isAiming_)
            speed *= GameConstants::PLAYER_SPRINT_MULTIPLIER;
        if (isAiming_) speed *= 0.6f;
        // Миниган: стрелять на бегу нельзя — серьёзное замедление, оправдывает долгую перезарядку
        if (minigunSpinupActive_) speed *= 0.35f;

        Vector3 targetVel = wishDir * speed;
        // Плавный разгон/торможение (ускорение и инерция)
        float accel = onGround_ ? 14.0f : 4.0f; // в воздухе управление слабее
        playerVelocity_ = Lerp(playerVelocity_, targetVel, Min(1.0f, accel * dt));

        // --- Гравитация и прыжок ---
        verticalVelocity_ -= GameConstants::GRAVITY * dt;

        if (onGround_) coyoteTimer_ = GameConstants::COYOTE_TIME;
        else coyoteTimer_ -= dt;
        if (jumpBufferTimer_ > 0.0f) jumpBufferTimer_ -= dt;

        bool wantJump = input->GetKeyDown(KEY_SPACE);
        if (wantJump && jumpBufferTimer_ <= 0.0f) jumpBufferTimer_ = GameConstants::JUMP_BUFFER_TIME;

        if (jumpBufferTimer_ > 0.0f && coyoteTimer_ > 0.0f) {
            verticalVelocity_ = GameConstants::JUMP_IMPULSE;
            jumpBufferTimer_ = 0.0f;
            coyoteTimer_ = 0.0f;
            onGround_ = false;
            landDuckOffset_ = -0.03f; // лёгкая «подседжка» перед отрывом
            PlayJumpSound();
        }

        // --- Интегрирование позиции ---
        Vector3 newPos = feetPos + playerVelocity_ * dt;

        // Коллизии со стенами/колоннами: горизонтальные рейкасты по 4 сторонам
        const float radius = 0.35f;
        Vector3 testFeet(newPos.x_, feetPos.y_ + 0.4f, newPos.z_);
        const Vector3 dirs[4] = { Vector3(1,0,0), Vector3(-1,0,0), Vector3(0,0,1), Vector3(0,0,-1) };
        Vector3 push = Vector3::ZERO;
        for (int i = 0; i < 4; ++i) {
            float dist; Vector3 hitPoint;
            if (RaycastLite(testFeet, dirs[i], radius + 0.15f, 1 | 8 | 16, dist, hitPoint)
                && dist > 0.0f && dist < radius + 0.15f) {
                float penetration = (radius + 0.15f) - dist;
                push -= dirs[i] * penetration;
            }
        }
        newPos += push;

        // Границы арены
        float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        newPos.x_ = Clamp(newPos.x_, -arenaSize + 2.0f, arenaSize - 2.0f);
        newPos.z_ = Clamp(newPos.z_, -arenaSize + 2.0f, arenaSize - 2.0f);

        // Вертикальное движение + определение земли
        newPos.y_ += verticalVelocity_ * dt;
        float groundY = ProbeGround(newPos);
        if (groundY < -1e5f) groundY = 0.0f; // страховка: базовый пол арены

        bool wasOnGround = onGround_;
        if (verticalVelocity_ <= 0.0f && newPos.y_ <= groundY + 0.02f) {
            // Приземление
            fallSpeedOnLand_ = -verticalVelocity_;
            newPos.y_ = groundY;
            verticalVelocity_ = 0.0f;
            onGround_ = true;
            if (!wasOnGround) {
                // Просадка камеры пропорционально скорости падения + тряска
                float impact = Clamp(fallSpeedOnLand_ / 12.0f, 0.0f, 1.0f);
                landDuckOffset_ = -GameConstants::LAND_DUCK_AMOUNT * impact;
                camShakeAmount_ = Max(camShakeAmount_, impact * 0.06f);
                if (impact > 0.15f) PlayLandSound(impact);
            }
        } else if (newPos.y_ > groundY + 0.05f) {
            onGround_ = false;
        }
        groundY_ = groundY;

        // Камера на высоте глаз поверх позиции ног
        cameraNode_->SetPosition(Vector3(newPos.x_, newPos.y_ + eyeHeight_, newPos.z_));

        // --- Динамика камеры (bob, FOV, наклон) ---
        UpdateDynamicCamera(dt, playerVelocity_.Length());
    }

    // =========================================================================
    // Динамическая камера от 1-го лица: покачка головы, FOV-эффекты, просадки
    // =========================================================================

    void UpdateDynamicCamera(float dt, float horizSpeed) {
        Camera* camera = cameraNode_->GetComponent<Camera>();

        // 1) Покачка головы (head bob): фаза зависит от скорости бега
        float speedRatio = Clamp(horizSpeed / GameConstants::PLAYER_SPEED, 0.0f, 2.0f);
        float targetIntensity = (onGround_ && horizSpeed > 0.5f && !isAiming_) ? Min(speedRatio, 1.5f) : 0.0f;
        headBobIntensity_ = Lerp(headBobIntensity_, targetIntensity, Min(1.0f, 8.0f * dt));
        headBobPhase_ += GameConstants::HEAD_BOB_FREQ * dt * (0.6f + speedRatio * 0.7f);

        float amp = GameConstants::HEAD_BOB_AMP * headBobIntensity_;
        float bobY = fabs(sin(headBobPhase_)) * amp;
        float bobX = cos(headBobPhase_ * 0.5f) * amp * 0.7f; // боковая составляющая -> roll

        // 2) Просадка после приземления — пружинное восстановление
        landDuckOffset_ = Lerp(landDuckOffset_, 0.0f, Min(1.0f, GameConstants::LAND_DUCK_RECOVERY * dt));

        // 3) Тряска от взрывов/приземлений
        camShakeAmount_ = Lerp(camShakeAmount_, 0.0f, Min(1.0f, GameConstants::CAMERA_SHAKE_DECAY * dt));
        float shakeX = ((float)(rand() % 200) - 100.0f) / 100.0f * camShakeAmount_;
        float shakeY = ((float)(rand() % 200) - 100.0f) / 100.0f * camShakeAmount_;

        // Смещаем камеру относительно «логической» позиции игрока
        Vector3 pos = cameraNode_->GetPosition();
        pos.y_ += bobY + landDuckOffset_ + shakeY;
        pos.x_ += shakeX;
        cameraNode_->SetPosition(pos);

        // 4) Наклон камеры (roll): боковая покачка + крен при падении
        float roll = bobX * 60.0f; // градусы
        if (!onGround_ && verticalVelocity_ < -2.0f) {
            roll += Clamp(verticalVelocity_, -15.0f, 0.0f) * GameConstants::FALL_CAM_TILT * 0.15f;
        }
        cameraNode_->SetRotation(Quaternion(pitch_, yaw_, roll));

        // 5) Динамический FOV: разбег при беге, сужение при прицеливании, вытягивание в прыжке
        float targetFov = 75.0f;
        if (isAiming_) targetFov -= GameConstants::ADS_FOV_SUB;
        else if (isSprinting_ && horizSpeed > GameConstants::PLAYER_SPEED * 1.1f)
            targetFov += GameConstants::SPRINT_FOV_ADD;
        if (isCrouching_) targetFov -= 4.0f;
        if (!onGround_) targetFov += verticalVelocity_ * 0.35f;
        currentFov_ = Lerp(currentFov_, targetFov, Min(1.0f, 6.0f * dt));
        if (camera) camera->SetFov(Clamp(currentFov_, 30.0f, 110.0f));
    }

    void AddCameraShake(float amount) {
        camShakeAmount_ = Min(camShakeAmount_ + amount, 0.25f);
    }
    
    void UpdateWeaponBob(float dt) {
        // Покачка оружия синхронизирована с динамикой камеры и состояниями игрока:
        // прицел (ПКМ) — оружие по центру, присед — ниже, бег — сильнее, в прыжке — подтянуто
        bool moving = playerVelocity_.LengthSquared() > 0.5f;

        if (moving && !weapon_.isReloading_ && !isAiming_) {
            weapon_.weaponBobPhase_ += GameConstants::WEAPON_BOB_FREQUENCY * dt * (0.7f + headBobIntensity_ * 0.6f);

            float bobAmount = GameConstants::WEAPON_BOB_AMOUNT * (0.5f + headBobIntensity_);
            if (isSprinting_ && !isCrouching_) bobAmount *= 2.0f;
            if (isCrouching_) bobAmount *= 0.5f;

            float bobX = cos(weapon_.weaponBobPhase_) * bobAmount;
            float bobY = fabs(sin(weapon_.weaponBobPhase_ * 2.0f)) * bobAmount;

            Vector3 base = WeaponBasePose();
            weaponNode_->SetPosition(Vector3(base.x_ + bobX, base.y_ - bobY, base.z_));
        } else {
            Vector3 currentPos = weaponNode_->GetPosition();
            weaponNode_->SetPosition(Lerp(currentPos, WeaponBasePose(), Min(1.0f, 10.0f * dt)));
        }

        // Крылья: при перезарядке «подхватывают» магазин, иначе мелко подрагивают
        if (wingHandL_ && wingHandR_) {
            float breathe = 8.0f * sinf(timeAcc_ * 2.2f);
            float reload = weapon_.isReloading_ ? 1.0f : 0.0f;
            wingHandL_->SetRotation(Quaternion( breathe + 30.0f * reload, Vector3(0, 0, 1)));
            wingHandR_->SetRotation(Quaternion(-breathe - 30.0f * reload, Vector3(0, 0, 1)));
        }
    }

    Vector3 WeaponBasePose() {
        Vector3 base(0.3f, -0.25f, 0.5f);
        if (isAiming_) return Vector3(0.0f, -0.16f, 0.45f);      // ADS: оружие у центра экрана
        if (isCrouching_) base.y_ -= 0.05f;                       // от приседа смотрит чуть ниже
        if (!onGround_) base.y_ += 0.04f;                         // в прыжке подтягиваем вверх
        return base;
    }
    
    void ApplyRecoilToWeapon() {
        // Применяем отдачу к позиции оружия (база зависит от стойки: ADS/присед/прыжок)
        Vector3 basePos = WeaponBasePose();
        Vector3 recoilOffset(-weapon_.recoilY_ * 0.01f, -weapon_.recoilX_ * 0.01f, -weapon_.recoilX_ * 0.02f);
        weaponNode_->SetPosition(basePos + recoilOffset);
        
        // Вращение оружия от отдачи
        Quaternion recoilRot(weapon_.recoilX_ * 0.5f, -weapon_.recoilY_ * 0.3f, 0.0f);
        weaponNode_->SetRotation(recoilRot);
    }
    
    void TryFire() {
        // Проверки перед выстрелом
        if (weapon_.isReloading_) {
            return;
        }
        
        if (weapon_.fireTimer_ > 0) {
            return;
        }
        
        if (weapon_.currentAmmo_ <= 0) {
            // Пустой магазин
            PlayEmptyClickSound();
            ShowMessage("Патроны закончились! Нажмите R для перезарядки");
            return;
        }
        
        // Производим выстрел
        FireWeapon();
    }
    
    void FireWeapon() {
        // Уменьшаем патроны
        weapon_.currentAmmo_--;
        player_.shotsFired_++;
        
        // Сбрасываем таймер стрельбы
        weapon_.fireTimer_ = WEAPON_DEFS[currentWeapon_].fireRate;
        
        // Добавляем отдачу
        float randomRecoil = (float)(rand() % 100) / 100.0f * GameConstants::BULLET_SPREAD;
        weapon_.recoilX_ += GameConstants::RECOIL_AMOUNT * (0.8f + randomRecoil);
        weapon_.recoilY_ += (float)(rand() % 40 - 20) / 10.0f;
        
        // Визуальные эффекты выстрела
        PlayMuzzleFlash();
        PlayShootSound();
        SpawnShellCasing();   // латунная гильза вылетает из оружия
        
        // Дробины дробовика — отдельными лучами
        const WeaponDef& wdef = WEAPON_DEFS[currentWeapon_];
        bool anyPelletHit = false;
        for (int p = 0; p < wdef.pellets; ++p) {
            if (PerformRaycast(wdef.damage, wdef.spread)) anyPelletHit = true;
        }
        // Точность считаем по ВЫСТРЕЛАМ, а не по дробинам: 8 дробин одного выстрела != 8 выстрелов
        // (раньше точность дробовика могла показывать >100%).
        if (anyPelletHit) player_.shotsHit_++;

        // Обновляем UI
        UpdateAmmoDisplay();
        UpdateAccuracyDisplay();
    }
    
    bool PerformRaycast(float damage, float spread) {
        // Получаем направление из центра экрана
        auto* camera = cameraNode_->GetComponent<Camera>();
        if (!camera) return false;
        
        // Добавляем разброс
        float spreadAngle = spread * (1.0f - player_.GetAccuracy() / 200.0f);
        if (isAiming_) spreadAngle *= 0.35f;      // ADS: точный огонь
        if (isCrouching_) spreadAngle *= 0.7f;    // присед: стабильнее
        if (!onGround_) spreadAngle *= 1.6f;      // в прыжке: хуже точность
        float randomX = (float)(rand() % 1000 - 500) / 1000.0f * spreadAngle;
        float randomY = (float)(rand() % 1000 - 500) / 1000.0f * spreadAngle;
        
        // Направление с учетом отдачи и разброса
        Quaternion spreadRot(pitch_ + randomY * 57.3f, yaw_ + randomX * 57.3f, 0.0f);
        Vector3 shootDirection = spreadRot * Vector3(0.0f, 0.0f, 1.0f);
        
        // Начало луча - позиция оружия
        Vector3 startPos = cameraNode_->GetPosition();
        
        // Конец луча
        Vector3 endPos = startPos + shootDirection * GameConstants::BULLET_RANGE;
        
        // Выполняем рейкаст (Lite-физика: статика + мишени + роботы)
        float hitDist = 0.0f; Vector3 hitPosition; Vector3 hitNormal; int hitTag = -1; void* hitPtr = nullptr;
        bool anyHit = RaycastLite(startPos, shootDirection, GameConstants::BULLET_RANGE,
                                  1 | 2 | 4 | 8 | 16, hitDist, hitPosition, &hitTag, &hitPtr, &hitNormal);
        if (anyHit) {
            // === ТРЕЙСЕР: светящаяся трасса пули от дула до точки попадания ===
            SpawnTracer(startPos + shootDirection * 0.6f, hitPosition);

            // Создаем эффект попадания
            CreateImpactEffect(hitPosition, hitNormal);
            
            // Проверяем, попала ли пуля в мишень
            // Гранатомёт: взрыв по площади
            if (currentWeapon_ == W_LAUNCHER) {
                CreateExplosionEffect(hitPosition);
                ApplyExplosionDamage(hitPosition, damage);
                PlayExplosionAt(hitPosition);
                AddCameraShake(0.12f); // тряска камеры от взрыва
                ShowHitMarker(false);
            } else {
                CheckTargetHit(hitTag, hitPtr, hitPosition, damage);
            }
            
            // Отладочная визуализация
            #ifdef DEBUG
            auto* debugRenderer = scene_->GetComponent<DebugRenderer>();
            if (debugRenderer) {
                debugRenderer->AddLine(startPos, hitPosition, Color(1.0f, 0.0f, 0.0f, 1.0f));
            }
            #endif
        } else {
            // Пуля ушла в никуда - создаем эффект на максимальной дистанции
            CreateImpactEffect(endPos, Vector3(0, 1, 0));
        }
        return anyHit;
    }

    // === Трассеры пуль: тонкий светящийся «бола» от дула до цели, живёт ~70 мс ===
    void SpawnTracer(const Vector3& from, const Vector3& to) {
        float len = (to - from).Length();
        if (len < 0.5f || len > 120.0f) return;
        Node* tr = scene_->CreateChild("Tracer", LOCAL);
        tr->SetPosition((from + to) * 0.5f);
        tr->LookAt(to);
        StaticModel* sm = tr->CreateComponent<StaticModel>();
        sm->SetModel(boxModel_);
        if (!tracerMat_) tracerMat_ = MakeTinted(nullptr, nullptr, Color(255, 230, 150));
        sm->SetMaterial(tracerMat_.Get());
        sm->SetCastShadows(false);
        tr->SetScale(Vector3(0.02f, 0.02f, len));
        tracers_.Push(TracerData{ SharedPtr<Node>(tr), 0.07f });
    }

    void UpdateTracers(float dt) {
        for (unsigned i = tracers_.Size(); i-- > 0; ) {
            tracers_[i].ttl_ -= dt;
            if (tracers_[i].ttl_ <= 0.0f) {
                if (tracers_[i].node_) tracers_[i].node_->Remove();
                tracers_.Erase(i);
            }
        }
    }
    
    void CheckTargetHit(int hitTag, void* hitPtr, const Vector3& hitPosition, float damage) {
        Node* hitNode = nullptr;
        if ((hitTag == 1 || hitTag == 2 || hitTag == 4) && hitPtr)
            hitNode = static_cast<Node*>(hitPtr);   // 1/4 — мишень тира, 2 — робот
        if (!hitNode) return;

        if (hitTag == 2) {
            for (unsigned ei = 0; ei < enemies_.Size(); ++ei) {
                if (enemies_[ei].alive_ && enemies_[ei].node_ == hitNode) { DamageEnemy(enemies_[ei], damage); break; }
            }
            return;
        }

        // Ищем мишень, соответствующую попавшему узлу
        for (unsigned ti = 0; ti < targets_.Size(); ++ti) {
            TargetData& target = targets_[ti];
            if (target.isActive_ && target.node_ == hitNode) {
                bool killed = (target.health_ - damage <= 0.0f);
                ShowHitMarker(killed);
                target.health_ -= damage;
                CreateHitSpark(hitNode->GetPosition());
                StaticModel* model = hitNode->GetComponent<StaticModel>();
                if (model) model->SetMaterial(targetHitMaterial_);
                PlayHitSound();
                if (target.health_ <= 0) DestroyTarget(target);
                break;
            }
        }
    }

    void DestroyTarget(TargetData& target) {
        target.isActive_ = false;
        RemoveColliderLite(target.colliderIdx_); // мишень больше не участвует в рейкастах
        target.colliderIdx_ = -1;
        
        // Комбо-очки (серия убийств подряд)
        AddScoreWithCombo(target.scoreValue_);
        player_.kills_++;
        
        // Удаляем мишень
        Node* targetNode = target.node_.Get();
        if (targetNode) {
            // Эффект разрушения
            CreateExplosionEffect(targetNode->GetPosition());
            CreateDebrisBurst(targetNode->GetPosition());
            PlayDestroySound();
            
            // Удаляем из сцены
            targetNode->Remove();
        }
        
        // Обновляем UI
        UpdateScoreDisplay();
        
        // Сообщение
        ShowMessage("МИШЕНЬ УНИЧТОЖЕНА! +" + String(target.scoreValue_) + " очков");
        
        // Проверяем, остались ли мишени
        int activeTargets = 0;
        for (const auto& t : targets_) {
            if (t.isActive_) activeTargets++;
        }
        
        if (activeTargets == 0) {
            ShowMessage("ПОЗДРАВЛЯЕМ! ВСЕ МИШЕНИ УНИЧТОЖЕНЫ!\nВаш счет: " + String(player_.score_));
        }
    }
    
    // Пулевое отверстие: тёмный плоский «пятак», развёрнутый по нормали поверхности.
    // DecalSet в этой сборке нет, поэтому это тонкий бокс с TTL и лимитом количества.
    void CreateBulletDecal(const Vector3& position, const Vector3& normal) {
        if (!decalMat_) decalMat_ = MakeTinted(nullptr, nullptr, Color(22, 20, 18));
        Node* d = scene_->CreateChild("BulletHole");
        d->SetPosition(position + normal * 0.012f);
        d->SetScale(Vector3(0.20f, 0.20f, 0.02f));
        Vector3 n = normal.LengthSquared() > 0.001f ? normal.Normalized() : Vector3::UP;
        d->LookAt(position + n * 2.0f);   // тот же приём, что у эффекта попадания
        StaticModel* sm = d->CreateComponent<StaticModel>();
        sm->SetModel(boxModel_);
        sm->SetMaterial(decalMat_.Get());
        sm->SetCastShadows(false);
        decals_.Push(SharedPtr<Node>(d));
        if (decals_.Size() > 60) {
            Node* oldest = decals_[0];
            if (oldest) oldest->Remove();
            decals_.Erase(0);
        }
        PendingRemove pr; pr.node_ = d; pr.ttl_ = 12.0f; pendingRemoves_.Push(pr);
    }

    // Ударная волна: кольцо, которое расширяется и исчезает (см. UpdateShockwaves)
    void CreateShockwave(const Vector3& position, Material* overrideMat = nullptr, float ttl = 0.55f) {
        if (!shockMat_) shockMat_ = MakeTinted(nullptr, nullptr, Color(255, 205, 130));
        Material* mat = overrideMat ? overrideMat : shockMat_.Get();
        Node* root = scene_->CreateChild("Shockwave");
        root->SetPosition(Vector3(position.x_, 0.15f, position.z_));
        for (int i = 0; i < 16; ++i) {
            float ang = (float)i * 22.5f;
            float rad = ang * 0.0174533f;
            Node* seg = AddPart(root, Vector3(cosf(rad), 0.0f, sinf(rad)), Vector3(0.42f, 0.05f, 0.16f), mat);
            seg->SetRotation(Quaternion(90.0f - ang, Vector3(0, 1, 0)));
        }
        ShockwaveData sw; sw.node_ = SharedPtr<Node>(root); sw.ttl_ = ttl; sw.max_ = ttl;
        shockwaves_.Push(sw);
    }

    void UpdateShockwaves(float dt) {
        for (unsigned i = shockwaves_.Size(); i-- > 0; ) {
            ShockwaveData& sw = shockwaves_[i];
            Node* n = sw.node_.Get();
            if (!n) { shockwaves_.Erase(i); continue; }
            sw.ttl_ -= dt;
            if (sw.ttl_ <= 0.0f) { n->Remove(); shockwaves_.Erase(i); continue; }
            float t = 1.0f - sw.ttl_ / sw.max_;     // 0..1
            float radius = 0.6f + t * 9.0f;          // волна разбегается до 9 м
            n->SetScale(Vector3(radius, 1.0f, radius));
        }
    }

    void CreateImpactEffect(const Vector3& position, const Vector3& normal) {
        // Создаем узел для эффекта
        Node* effectNode = scene_->CreateChild("ImpactEffect");
        effectNode->SetPosition(position);
        effectNode->LookAt(position + normal);

        // Эмиттер искр
        ParticleEmitter* emitter = effectNode->CreateComponent<ParticleEmitter>();
        if (sparkEffect_) emitter->SetEffect(sparkEffect_);

        // Отверстие от пули на поверхности (видно, куда именно попал)
        CreateBulletDecal(position, normal);

        // Автоудаление через 1 секунду
        { PendingRemove pr; pr.node_ = effectNode; pr.ttl_ = 1.0f; pendingRemoves_.Push(pr); }
    }
    
    void CreateExplosionEffect(const Vector3& position) {
        Node* effectNode = scene_->CreateChild("ExplosionEffect");
        effectNode->SetPosition(position);
        
        // Эмиттер дыма
        ParticleEmitter* smokeEmitter = effectNode->CreateComponent<ParticleEmitter>();
        if (smokeEffect_) smokeEmitter->SetEffect(smokeEffect_);
        
        // Точечный свет для вспышки
        Node* lightNode = effectNode->CreateChild("ExplosionLight");
        Light* light = lightNode->CreateComponent<Light>();
        light->SetLightType(LIGHT_POINT);
        light->SetColor(Color(1.0f, 0.5f, 0.0f, 1.0f));
        light->SetRange(10.0f);
        
        // Ударная волна по полу (свет удаляется вместе с узлом эффекта)
        CreateShockwave(position);

        { PendingRemove pr; pr.node_ = effectNode; pr.ttl_ = 2.0f; pendingRemoves_.Push(pr); }
    }
    
    void PlayMuzzleFlash() {
        // Раньше свет включался и в ТОМ ЖЕ кадре выключался — вспышки не было видно вообще.
        if (muzzleLight_ && muzzleLightNode_) {
            muzzleLightNode_->SetEnabled(true);
            muzzleFlashTimer_ = 0.05f;
        }
    }
    
    void StartReload() {
        // Размер магазина и время перезарядки берём из оружия, а не из глобальных MAX_AMMO/RELOAD_TIME
        // (иначе пистолет с магазином 12 «перезаряжался» до 30 и все стволы перезаряжались за 2 с).
        const WeaponDef& def = WEAPON_DEFS[currentWeapon_];
        if (weapon_.currentAmmo_ >= def.magSize) {
            ShowMessage("Магазин полон!");
            return;
        }
        
        if (weapon_.reserveAmmo_ <= 0) {
            ShowMessage("Нет запасных патронов!");
            return;
        }
        
        weapon_.isReloading_ = true;
        weapon_.reloadTimer_ = def.reloadTime;
        
        PlayReloadSound();
        ShowMessage("Перезарядка...");
    }
    
    void CompleteReload() {
        int needed = WEAPON_DEFS[currentWeapon_].magSize - weapon_.currentAmmo_;
        int available = weapon_.reserveAmmo_;
        int toReload = Min(needed, available);
        
        weapon_.currentAmmo_ += toReload;
        weapon_.reserveAmmo_ -= toReload;
        weapon_.isReloading_ = false;
        weapon_.reloadTimer_ = 0.0f;
        
        reloadOverlay_->SetColor(Color(1.0f, 1.0f, 0.0f, 0.0f));
        
        ShowMessage("Перезарядка завершена!");
        UpdateAmmoDisplay();
    }
    
    // =========================================================================
    // Звуковые эффекты (заглушки)
    // =========================================================================
    
    float SfxGain(float base) const { return Clamp(base * sfxVolume_, 0.0f, 1.0f); }

    // Пул позиционных источников: 6 штук по кругу хватает, чтобы кудахтанье
    // и шлепки яиц не обрывали друг друга.
    void InitSfx3DPool() {
        if (!sfx3DPool_.Empty()) return;
        for (int i = 0; i < 6; ++i) {
            Node* n = scene_->CreateChild("Sfx3D" + String(i));
            SoundSource3D* src = n->CreateComponent<SoundSource3D>();
            if (!src) continue;
            src->SetGain(0.0f);
            sfx3DPool_.Push(SharedPtr<SoundSource3D>(src));
        }
    }

    void PlaySfx3D(const Vector3& pos, Sound* s, float gain) {
        if (!s || sfx3DPool_.Empty()) return;
        SoundSource3D* src = sfx3DPool_[(unsigned)sfx3DNext_ % sfx3DPool_.Size()];
        ++sfx3DNext_;
        if (!src) return;
        src->GetNode()->SetWorldPosition(pos);
        src->SetGain(SfxGain(gain));
        src->Play(s);
    }

    void PlayCluck(const Vector3& pos, float gain = 0.45f) {
        Sound* s = cluckSounds_[rand() % 2];
        PlaySfx3D(pos, s, gain);
    }

    // Гильза: летит вправо-вверх от оружия, живёт 1.6 с (физика — UpdateDebrisLite)
    void SpawnShellCasing() {
        if (!weaponNode_) return;
        if (!shellMat_) shellMat_ = MakeTinted(nullptr, nullptr, Color(215, 175, 80));
        Vector3 muzzle = weaponNode_->GetWorldPosition() + weaponNode_->GetWorldDirection() * 0.4f;
        Node* c = scene_->CreateChild("Shell", LOCAL);
        c->SetPosition(muzzle);
        c->SetScale(Vector3(0.035f, 0.035f, 0.09f));
        StaticModel* sm = c->CreateComponent<StaticModel>();
        sm->SetModel(cylModel_ ? cylModel_ : boxModel_);
        sm->SetMaterial(shellMat_.Get());
        sm->SetCastShadows(false);
        Vector3 right = weaponNode_->GetWorldRight();
        DebrisData d;
        d.node_ = c;
        d.pos_ = muzzle;
        d.vel_ = right * (2.0f + (float)(rand() % 20) / 10.0f) + Vector3(0, 2.6f, 0);
        d.spin_ = Vector3(12.0f, 8.0f, 16.0f);
        d.ttl_ = 1.6f;
        debrisLite_.Push(d);
    }

    void PlayShootSound() {
        if (!soundSource_) return;
        Sound* s = shootSounds_[currentWeapon_];
        if (!s) return;
        // Вариация высоты/громкости — выстрелы не звучат клонами
        float gainVar = Max(0.4f, Min(1.0f, 0.85f + (rand() % 100 - 50) / 300.0f)) * SfxGain(1.0f);
        soundSource_->SetGain(gainVar);
        soundSource_->Play(s);
    }
    
    void PlayReloadSound() {
        if (soundSource_ && reloadSound_) { soundSource_->SetGain(SfxGain(0.8f)); soundSource_->Play(reloadSound_); }
    }
    
    void PlayEmptyClickSound() {
        if (soundSource_ && emptyClickSound_) { soundSource_->SetGain(SfxGain(0.6f)); soundSource_->Play(emptyClickSound_); }
    }
    
    void PlayHitSound() {
        if (soundSource_ && hitSound_) { soundSource_->SetGain(SfxGain(0.7f)); soundSource_->Play(hitSound_); }
    }
    
    void PlayDestroySound() {
        if (soundSource_ && destroySound_) { soundSource_->SetGain(SfxGain(0.9f)); soundSource_->Play(destroySound_); }
    }

    void PlayUISound() {
        if (soundSource_ && uiSelectSound_) { soundSource_->SetGain(SfxGain(0.5f)); soundSource_->Play(uiSelectSound_); }
    }

    void PlayJumpSound() {
        if (soundSource_ && jumpSound_) { soundSource_->SetGain(SfxGain(0.45f)); soundSource_->Play(jumpSound_); }
    }

    void PlayLandSound(float impact) {
        if (soundSource_ && landSound_) { soundSource_->SetGain(Clamp(impact * 0.9f, 0.15f, 0.9f)); soundSource_->Play(landSound_); }
    }

    void PlayExplosionAt(const Vector3& pos) {
        if (!explosionSource3D_ || !explosionSound_) return;
        explosionSource3D_->GetNode()->SetWorldPosition(pos);
        explosionSource3D_->Play(explosionSound_);
    }
    
    // =========================================================================
    // ЗАГРУЗОЧНЫЙ ЭКРАН: затемнение, логотип, прогресс-бар, советы
    // =========================================================================

    void CreateLoadingScreen() {
        // Шрифт для загрузочного экрана грузим напрямую (font_ ещё не инициализирован)
        if (!font_) {
            font_ = GetSubsystem<ResourceCache>()->GetResource<Font>("Fonts/Anonymous Pro.ttf");
            if (!font_) font_ = GetSubsystem<ResourceCache>()->GetResource<Font>("Fonts/DejaVuSans.ttf");
        }
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        loadSteps_ = Vector<String>{
            "Загрузка ресурсов и HD-текстур",
            "Построение арены 192x192",
            "Сборка оружия и камеры",
            "Интерфейс и арсенал",
            "Готово"
        };

        loadingPanel_ = root->CreateChild<UIElement>("LoadingPanel");
        loadingPanel_->SetSize(sw, sh);

        BorderImage* bg = loadingPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.01f, 0.02f, 0.04f, 1.0f));

        // «Неоновая» линия-акцент над заголовком
        BorderImage* accent = loadingPanel_->CreateChild<BorderImage>();
        accent->SetSize(420, 3);
        accent->SetPosition(sw / 2 - 210, sh / 2 - 120);
        accent->SetColor(Color(0.1f, 1.0f, 0.6f, 0.9f));

        loadingTitleText_ = loadingPanel_->CreateChild<Text>();
        loadingTitleText_->SetFont(font_ ? font_ : cache_font(), 56);
        loadingTitleText_->SetText("CHIKENGUN 2.1");
        loadingTitleText_->SetTextAlignment(HA_CENTER);
        loadingTitleText_->SetPosition(sw / 2 - 300, sh / 2 - 105);
        loadingTitleText_->SetWidth(600);
        loadingTitleText_->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));

        Text* sub = loadingPanel_->CreateChild<Text>();
        sub->SetFont(font_ ? font_ : cache_font(), 18);
        sub->SetText("ARENA SHOOTER | URHO3D");
        sub->SetTextAlignment(HA_CENTER);
        sub->SetPosition(sw / 2 - 300, sh / 2 - 40);
        sub->SetWidth(600);
        sub->SetColor(Color(0.5f, 0.8f, 1.0f, 0.9f));

        // Трек прогресс-бара
        BorderImage* track = loadingPanel_->CreateChild<BorderImage>();
        track->SetSize(520, 18);
        track->SetPosition(sw / 2 - 260, sh / 2 + 30);
        track->SetColor(Color(0.12f, 0.15f, 0.2f, 1.0f));

        loadingBarFill_ = loadingPanel_->CreateChild<BorderImage>();
        loadingBarFill_->SetSize(0, 18);
        loadingBarFill_->SetPosition(sw / 2 - 260, sh / 2 + 30);
        loadingBarFill_->SetColor(Color(0.15f, 1.0f, 0.55f, 1.0f));

        loadingTipText_ = loadingPanel_->CreateChild<Text>();
        loadingTipText_->SetFont(font_ ? font_ : cache_font(), 16);
        loadingTipText_->SetText("Совет: ПКМ — прицел (ADS), удерживай ЛКМ для автоогня");
        loadingTipText_->SetTextAlignment(HA_CENTER);
        loadingTipText_->SetPosition(sw / 2 - 350, sh / 2 + 70);
        loadingTipText_->SetWidth(700);
        loadingTipText_->SetColor(Color(0.7f, 0.75f, 0.8f, 0.9f));

        // Рендер кадра загрузки до запуска основного цикла событий
        UpdateLoadingProgress(0.05f);
    }

    static Font* cache_font() { return nullptr; } // fallback: текст без шрифта не критичен

    void UpdateLoadingProgress(float p) {
        if (!loadingPanel_) return;
        loadProgress_ = Clamp(p, 0.0f, 1.0f);
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();
        if (loadingBarFill_) loadingBarFill_->SetSize((int)(520 * loadProgress_), 18);
        if (loadingTipText_ && loadStepIndex_ < (int)loadSteps_.Size()) {
            loadingTipText_->SetText(String("Загрузка: ") + loadSteps_[loadStepIndex_] +
                                     String(" ... ") + String((int)(loadProgress_ * 100)) + "%");
        }
        // Принудительно рендерим кадр, чтобы прогресс-бар обновился сразу
    }

    void AdvanceLoadingStep() {
        ++loadStepIndex_;
        float p = (float)loadStepIndex_ / (float)Max(1, (int)loadSteps_.Size());
        UpdateLoadingProgress(p);
    }

    void HideLoadingScreen() {
        if (loadingPanel_) {
            loadingPanel_->Remove();
            loadingPanel_ = nullptr;
        }
        loadingScreenActive_ = false;
    }

    // =========================================================================
    // Утилиты
    // =========================================================================
    
    void ApplyExplosionDamage(const Vector3& center, float damage) {
        const float radius = 5.0f;
        for (auto& target : targets_) {
            if (!target.isActive_ || !target.node_) continue;
            float dist = (target.node_->GetPosition() - center).Length();
            if (dist <= radius) {
                float falloff = 1.0f - dist / radius;
                target.health_ -= damage * falloff;
                StaticModel* model = target.node_->GetComponent<StaticModel>();
                if (model) model->SetMaterial(targetHitMaterial_);
                if (target.health_ <= 0) DestroyTarget(target);
            }
        }
        // Взрыв задевает и роботов арены
        for (auto& e : enemies_) {
            if (!e.alive_ || !e.node_) continue;
            float dist = (e.node_->GetPosition() - center).Length();
            if (dist <= radius) DamageEnemy(e, damage * (1.0f - dist / radius));
        }
        // === Self-damage: граната, кинутая вплотную к себе, обидит и игрока (как в нормальных шутерах) ===
        if (cameraNode_) {
            Vector3 camPos = cameraNode_->GetPosition();
            float pdist = (camPos - center).Length();
            // Самоподрыв работает в обоих режимах (раньше в тире граната в упор была бесплатной)
            if (pdist < radius && !gameOver_) {
                float falloff = 1.0f - pdist / radius;
                // Игрок получает только 40% урона — наказывает «самоубийственные» кампы в упор
                DamagePlayer(damage * falloff * 0.4f);
            }
        }
    }

    // =========================================================================
    // УРОН ПО ИГРОКУ + GAME OVER
    // =========================================================================

    void DamagePlayer(float amount) {
        if (gameOver_ || amount <= 0.0f) return;
        player_.health_ -= amount;
        damageFlashTimer_ = 0.3f;              // красный флеш всего экрана
        AddCameraShake(Clamp(amount / 60.0f, 0.05f, 0.25f)); // тряска пропорционально урону
        UpdateHealthDisplay();
        if (player_.health_ <= 0.0f) {
            player_.health_ = 0.0f;
            TriggerGameOver();
        }
    }

    void TriggerGameOver() {
        gameOver_ = true;
        player_.health_ = 0.0f;
        UpdateHealthDisplay();
        // Финальный взрыв на позиции игрока + громкая тряски
        if (cameraNode_) {
            CreateExplosionEffect(cameraNode_->GetPosition());
            PlayExplosionAt(cameraNode_->GetPosition());
        }
        AddCameraShake(0.5f);
        if (player_.score_ > highScore_) {
            highScore_ = player_.score_;
            SaveSettingsFile();
        }
        if (gameOverStatsText_) {
            gameOverStatsText_->SetText(
                String("СЧЁТ: ") + String(player_.score_) + "   |   ВОЛНА: " + String(waveNum_) + "\n" +
                String("УБИЙСТВ: ") + String(player_.kills_) + "   |   ЛУЧШЕЕ КОМБО: x" + String(bestCombo_) + "\n" +
                String("ТОЧНОСТЬ: ") + String((int)player_.GetAccuracy()) + "%   |   РЕКОРД: " + String(highScore_) + "\n" +
                String("ВРЕМЯ: ") + FormatTime(arenaTime_) + "   |   УБИЙСТВ В МИНУТУ: " +
                String(arenaTime_ > 5.0f ? (int)(player_.kills_ / (arenaTime_ / 60.0f)) : 0));
        }
        ShowGameOver(true);
    }

    void ShowGameOver(bool show) {
        gameOver_ = show;
        if (show) gameOverPulse_ = 0.0f;   // рестарт пульсации каждый раз
        if (gameOverPanel_) gameOverPanel_->SetVisible(show);
        SetGameHudVisible(!show);
        auto* input = GetSubsystem<Input>();
        if (show) {
            input->SetMouseVisible(false);
        }
    }

    void RestartArena() {
        ShowGameOver(false);
        StartGameMode(MODE_ARENA);
    }

    void CycleSkin(int dir) {
        currentSkin_ = ((currentSkin_ + dir) % SKIN_COUNT + SKIN_COUNT) % SKIN_COUNT;
        PlayUISound();
        RefreshWeaponMaterials();
        BuildWeaponModel(currentWeapon_);
        if (weaponNameText_) weaponNameText_->SetText(String(WEAPON_DEFS[currentWeapon_].name) + " | " + String(SKIN_DEFS[currentSkin_].name));
        ShowMessage(String("Скин: ") + SKIN_DEFS[currentSkin_].name);
        if (armoryOpen_) UpdateArmoryHighlight();
    }

    // ================= МЕНЮ АРСЕНАЛА (TAB) =================
// NOTE: methods below are part of class MyApp (do not remove this brace pair)
    void ToggleArmory() {
        armoryOpen_ = !armoryOpen_;
        auto* input = GetSubsystem<Input>();
        auto* graphics = GetSubsystem<Graphics>();
        if (armoryOpen_) {
            if (!armoryPanel_) CreateArmoryMenu();
            armoryPanel_->SetVisible(true);
            input->SetMouseVisible(true);
            ShowMessage("Арсенал: клик по оружию/скину, TAB — закрыть");
            PlayUISound();
        } else {
            if (armoryPanel_) armoryPanel_->SetVisible(false);
            input->SetMouseVisible(false);
            input->SetMousePosition(IntVector2(graphics->GetWidth() / 2, graphics->GetHeight() / 2));
        }
        UpdateArmoryHighlight();
    }

    void CreateArmoryMenu() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        armoryPanel_ = root->CreateChild<UIElement>("ArmoryPanel");
        armoryPanel_->SetSize(sw, sh);

        BorderImage* bg = armoryPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.02f, 0.03f, 0.05f, 0.82f));

        Text* title = armoryPanel_->CreateChild<Text>();
        title->SetFont(font_, 36);
        title->SetText("АРСЕНАЛ CHIKENGUN 2.1");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - 200, 30);
        title->SetWidth(400);
        title->SetColor(Color(1.0f, 0.85f, 0.3f, 1.0f));

        Text* wHead = armoryPanel_->CreateChild<Text>();
        wHead->SetFont(font_, 22);
        wHead->SetText("ОРУЖИЕ (1-" + String((int)WEAPON_COUNT) + ")");
        wHead->SetPosition(sw / 2 - 380, 100);
        wHead->SetColor(Color(0.7f, 0.9f, 1.0f, 1.0f));

        for (int i = 0; i < WEAPON_COUNT; ++i) {
            Text* row = armoryPanel_->CreateChild<Text>();
            row->SetFont(font_, 20);
            const WeaponDef& d = WEAPON_DEFS[i];
            row->SetText(String(i + 1) + ". " + String(d.name) + "  DMG:" + String(d.damage, 0) +
                         "  ROF:" + String((int)(1.0f / d.fireRate)) + "/s  MAG:" + String(d.magSize));
            row->SetPosition(sw / 2 - 380, 135 + i * 34);
            row->SetVar("index", i);
            armoryWeaponRows_.Push(SharedPtr<Text>(row));
        }

        Text* sHead = armoryPanel_->CreateChild<Text>();
        sHead->SetFont(font_, 22);
        sHead->SetText("СКИНЫ (Q/E)");
        sHead->SetPosition(sw / 2 + 80, 100);
        sHead->SetColor(Color(0.7f, 1.0f, 0.8f, 1.0f));

        for (int i = 0; i < SKIN_COUNT; ++i) {
            Text* row = armoryPanel_->CreateChild<Text>();
            row->SetFont(font_, 20);
            const SkinDef& sk = SKIN_DEFS[i];
            row->SetText(String(i + 1) + ". " + String(sk.name) + (sk.emissive ? " (glow)" : ""));
            row->SetPosition(sw / 2 + 80, 135 + i * 34);
            row->SetVar("index", i);
            armorySkinRows_.Push(SharedPtr<Text>(row));
        }

        Text* hint = armoryPanel_->CreateChild<Text>();
        hint->SetFont(font_, 16);
        hint->SetText("TAB - close | ESC - close/exit | LMB - select");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - 250, sh - 60);
        hint->SetWidth(500);
        hint->SetColor(Color(0.8f, 0.8f, 0.8f, 1.0f));

        armoryPanel_->SetVisible(false);
    }

    void UpdateArmoryHighlight() {
        for (unsigned i = 0; i < armoryWeaponRows_.Size(); ++i) {
            bool sel = ((int)i == currentWeapon_);
            armoryWeaponRows_[i]->SetColor(sel ? Color(1.0f, 0.85f, 0.2f, 1.0f) : Color(0.85f, 0.85f, 0.9f, 1.0f));
        }
        for (unsigned i = 0; i < armorySkinRows_.Size(); ++i) {
            bool sel = ((int)i == currentSkin_);
            const SkinDef& sk = SKIN_DEFS[i];
            if (sel) armorySkinRows_[i]->SetColor(Color(1.0f, 0.85f, 0.2f, 1.0f));
            else armorySkinRows_[i]->SetColor(Color(sk.color.r_ * 0.9f + 0.1f, sk.color.g_ * 0.9f + 0.1f, sk.color.b_ * 0.9f + 0.1f, 1.0f));
        }
    }

    void HandleArmoryClick(bool left) {
        if (!left || !armoryPanel_) return;
        auto* input = GetSubsystem<Input>();
        IntVector2 mp = input->GetMousePosition();
        // Хит-тест строк арсенала: GetScreenRect нет — считаем экранную позицию проходом по предкам
        auto screenPos = [](UIElement* el) -> IntVector2 {
            IntVector2 p;
            for (UIElement* e = el; e; e = e->GetParent()) { p.x_ += e->GetPosition().x_; p.y_ += e->GetPosition().y_; }
            return p;
        };
        for (unsigned i = 0; i < armoryWeaponRows_.Size(); ++i) {
            UIElement* row = armoryWeaponRows_[i];
            IntVector2 rp = screenPos(row);
            if (rp.x_ <= mp.x_ && mp.x_ < rp.x_ + (int)row->GetWidth() && rp.y_ <= mp.y_ && mp.y_ < rp.y_ + (int)row->GetHeight()) { EquipWeapon((int)i, false); UpdateArmoryHighlight(); return; }
        }
        for (unsigned i = 0; i < armorySkinRows_.Size(); ++i) {
            UIElement* row = armorySkinRows_[i];
            IntVector2 rp = screenPos(row);
            if (rp.x_ <= mp.x_ && mp.x_ < rp.x_ + (int)row->GetWidth() && rp.y_ <= mp.y_ && mp.y_ < rp.y_ + (int)row->GetHeight()) { CycleSkin((int)i - currentSkin_); return; }
        }
    }

    void ToggleMouseVisibility() {
        auto* input = GetSubsystem<Input>();
        isMouseVisible_ = !isMouseVisible_;
        input->SetMouseVisible(isMouseVisible_);
        
        if (isMouseVisible_) {
            ShowMessage("Курсор видим. Нажмите TAB чтобы скрыть.");
        } else {
            // Центрируем мышь при скрытии
            auto* graphics = GetSubsystem<Graphics>();
            input->SetMousePosition(IntVector2(graphics->GetWidth() / 2, graphics->GetHeight() / 2));
        }
    }

    // =========================================================================
    // НАСТРОЙКИ: загрузка/сохранение settings.ini, применение на лету
    // =========================================================================

    // В этой сборке Urho3D у XMLElement нет GetInt/SetInt — читаем/пишем строковые атрибуты
    static int XmlGetInt(const XMLElement& el, const char* name, int def) {
        if (!el.HasAttribute(name)) return def;
        //return String(el.GetAttribute(name)).ToInt();
        return atoi(el.GetAttribute(name).CString());
    }
    static void XmlSetInt(XMLElement& el, const char* name, int v) {
        el.SetString(name, String(v));
    }

    String SettingsPath() const {
        return GetSubsystem<FileSystem>()->GetProgramDir() + "settings.ini";
    }

    void LoadSettingsFile() {
        auto* fs = GetSubsystem<FileSystem>();
        if (!fs->FileExists(SettingsPath())) return;
        SharedPtr<XMLFile> xml(new XMLFile(context_));
        if (!xml->LoadFile(SettingsPath())) return;
        XMLElement rootEl = xml->GetRoot();
        XMLElement gfx = rootEl.GetChild("graphics");
        if (gfx) {
            gfxShadowIdx_ = Clamp(XmlGetInt(gfx, "shadows", gfxShadowIdx_), 0, 2);
            gfxGlowIdx_   = Clamp(XmlGetInt(gfx, "bloom", gfxGlowIdx_), 0, 2);
            gfxFpsIdx_    = Clamp(XmlGetInt(gfx, "fps", gfxFpsIdx_), 0, 2);
            gfxAnisoIdx_  = Clamp(XmlGetInt(gfx, "anisotropic", gfxAnisoIdx_), 0, 2);
            mouseSensIdx_ = Clamp(XmlGetInt(gfx, "mouse", mouseSensIdx_), 0, 2);
        }
        XMLElement snd = rootEl.GetChild("sound");
        if (snd) {
            // atof("") == 0, т.е. отсутствующий/битый атрибут раньше ГЛУШИЛ звук.
            // Читаем только реально существующие атрибуты, остальные оставляем как есть.
            if (snd.HasAttribute("sfx"))
                sfxVolume_ = Clamp((float)atof(snd.GetAttribute("sfx").CString()), 0.0f, 1.0f);
            if (snd.HasAttribute("music"))
                musicVolume_ = Clamp((float)atof(snd.GetAttribute("music").CString()), 0.0f, 1.0f);
        }
        XMLElement prog = rootEl.GetChild("progress");
        if (prog) highScore_ = Max(0, XmlGetInt(prog, "highscore", highScore_));
    }

    void SaveSettingsFile() {
        SharedPtr<XMLFile> xml(new XMLFile(context_));
        XMLElement rootEl = xml->CreateRoot("chikengun");
        XMLElement gfx = rootEl.CreateChild("graphics");
        XmlSetInt(gfx, "shadows", gfxShadowIdx_);
        XmlSetInt(gfx, "bloom", gfxGlowIdx_);
        XmlSetInt(gfx, "fps", gfxFpsIdx_);
        XmlSetInt(gfx, "anisotropic", gfxAnisoIdx_);
        XmlSetInt(gfx, "mouse", mouseSensIdx_);
        XMLElement snd = rootEl.CreateChild("sound");
        snd.SetString("sfx", String(sfxVolume_));
        snd.SetString("music", String(musicVolume_));
        XMLElement prog = rootEl.CreateChild("progress");
        XmlSetInt(prog, "highscore", highScore_);
        xml->SaveFile(SettingsPath());
    }

    // Bloom: сеттеров Glow*/ValueCollection в этой сборке нет — управляем только флагом HDR
    void ConfigureGlow(Renderer* renderer, bool strong) {
        if (renderer) renderer->SetHDRRendering(strong);
    }

    // Настройка «Фильтрация текстур»: реально меняет фильтр у HD-текстур.
    void ApplyTextureFiltering() {
        TextureFilterMode fm = FILTER_ANISOTROPIC;
        if (gfxAnisoIdx_ == 0) fm = FILTER_BILINEAR;
        else if (gfxAnisoIdx_ == 1) fm = FILTER_TRILINEAR;
        for (unsigned i = 0; i < anisoTextures_.Size(); ++i)
            if (anisoTextures_[i]) anisoTextures_[i]->SetFilterMode(fm);
    }

    void ApplyGraphicsSettings() {
        auto* renderer = GetSubsystem<Renderer>();
        if (!renderer) return;

        static const int shadowSizes[3] = { 1024, 2048, 4096 };
        renderer->SetShadowMapSize((unsigned)shadowSizes[Clamp(gfxShadowIdx_, 0, 2)]);

        // Свечение (HDR) включаем ТОЛЬКО по явному выбору пользователя:
        // на этой сборке HDR часто даёт пересвет/белый кадр.
        renderer->SetHDRRendering(gfxGlowIdx_ >= 2);

        // Лимит FPS (раньше в этом слоте была настройка «Разрешение», которая нигде не применялась)
        static const int fpsValues[3] = { 60, 120, 0 };
        if (engine_) engine_->SetMaxFps(fpsValues[Clamp(gfxFpsIdx_, 0, 2)]);

        // Фильтрация текстур и звук
        ApplyTextureFiltering();
        if (soundSource_) soundSource_->SetGain(sfxVolume_);
        if (musicSource_) musicSource_->SetGain(musicVolume_);
    }

    // =========================================================================
    // ГЛАВНОЕ МЕНЮ
    // =========================================================================

    // =========================================================================
    // ГЛАВНОЕ МЕНЮ — анимированный кибер-экран входа
    // Градиентный фон, неоновая плашка, «дыхание» заголовка, летящие частицы,
    // параллакс от мыши, золотая полоска под пунктами.
    // =========================================================================

    void CreateMainMenu() {
        if (mainMenuPanel_) return;
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        mainMenuPanel_ = root->CreateChild<UIElement>("MainMenuPanel");
        mainMenuPanel_->SetSize(sw, sh);

        // --- Фон: тёмно-синий + вертикальные градиентные полосы = мягкий виньеточный градиент ---
        BorderImage* bg = mainMenuPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.012f, 0.02f, 0.045f, 0.80f));
        for (int i = 0; i < 16; ++i) {
            BorderImage* band = mainMenuPanel_->CreateChild<BorderImage>();
            float t = i / 15.0f;
            // Затемнение к краям экрана (горизонтальная виньетка)
            float edge = fabs(t - 0.5f) * 2.0f;
            band->SetPosition(int(t * sw), 0);
            band->SetSize(sw / 16 + 1, sh);
            band->SetColor(Color(0.0f, 0.005f, 0.02f, 0.10f + edge * edge * 0.30f));
        }
        // Верхняя и нижняя «затухающие» полосы — кинематографичные letterbox-тени
        BorderImage* topShade = mainMenuPanel_->CreateChild<BorderImage>();
        topShade->SetPosition(0, 0); topShade->SetSize(sw, sh / 6);
        topShade->SetColor(Color(0.0f, 0.0f, 0.01f, 0.35f));
        BorderImage* botShade = mainMenuPanel_->CreateChild<BorderImage>();
        botShade->SetPosition(0, sh - sh / 6); botShade->SetSize(sw, sh / 6);
        botShade->SetColor(Color(0.0f, 0.0f, 0.01f, 0.45f));

        // --- Летящие вверх неоновые частицы («искры арены») ---
        for (int i = 0; i < 26; ++i) {
            BorderImage* p = mainMenuPanel_->CreateChild<BorderImage>();
            int sz = 2 + rand() % 3;
            p->SetSize(sz, sz * 3);
            float hue = (rand() % 100) / 100.0f;
            Color pc = (hue < 0.5f)
                ? Color(0.35f + hue * 0.6f, 0.75f, 1.0f, 0.5f)          // циан
                : Color(1.0f, 0.8f - (hue - 0.5f) * 0.5f, 0.25f, 0.5f); // золото
            p->SetColor(pc);
            menuParticles_.Push(SharedPtr<BorderImage>(p));
            menuParticlePhase_.Push(rand() % 1000 / 1000.0f * 6.283f);
            menuParticleX_.Push(float(rand() % sw));
            menuParticleSpeed_.Push(18.0f + rand() % 42);   // px/c
        }

        // --- Неоновая рамка-плашка вокруг блока меню ---
        menuPlate_ = mainMenuPanel_->CreateChild<BorderImage>();
        menuPlate_->SetPosition(sw / 2 - 320, sh / 2 - 250);
        menuPlate_->SetSize(640, 470);
        menuPlate_->SetColor(Color(0.02f, 0.05f, 0.10f, 0.55f));
        // Тонкая светящаяся окантовка из 4 полосок
        struct Edge { int x, y, w, h; };
        Edge edges[4] = {
            { sw/2 - 322, sh/2 - 252, 644, 2 }, { sw/2 - 322, sh/2 + 218, 644, 2 },
            { sw/2 - 322, sh/2 - 252, 2, 472 }, { sw/2 + 320, sh/2 - 252, 2, 472 }
        };
        for (int i = 0; i < 4; ++i) {
            BorderImage* e = mainMenuPanel_->CreateChild<BorderImage>();
            e->SetPosition(edges[i].x, edges[i].y);
            e->SetSize(edges[i].w, edges[i].h);
            e->SetColor(Color(0.25f, 0.75f, 1.0f, 0.35f));
        }

        // --- Заголовок с «золотым дыханием» ---
        Text* title = mainMenuPanel_->CreateChild<Text>();
        title->SetFont(font_, 68);
        title->SetText("CHIKENGUN 2.1");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - 400, sh / 2 - 220);
        title->SetWidth(800);
        title->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));
        menuTitle_ = SharedPtr<Text>(title);

        // Декоративная черта под заголовком
        menuAccentBar_ = mainMenuPanel_->CreateChild<BorderImage>();
        menuAccentBar_->SetPosition(sw / 2 - 180, sh / 2 - 140);
        menuAccentBar_->SetSize(360, 2);
        menuAccentBar_->SetColor(Color(1.0f, 0.8f, 0.2f, 0.9f));

        Text* sub = mainMenuPanel_->CreateChild<Text>();
        sub->SetFont(font_, 18);
        sub->SetText("[ ARENA SHOOTER ]  \xc2\xb7  [ URHO3D ENGINE ]  \xc2\xb7  [ v2.1 ]");
        sub->SetTextAlignment(HA_CENTER);
        sub->SetPosition(sw / 2 - 400, sh / 2 - 128);
        sub->SetWidth(800);
        sub->SetColor(Color(0.5f, 0.8f, 1.0f, 0.9f));
        menuSub_ = SharedPtr<Text>(sub);

        const char* items[4] = { "ТИР (мишени)", "АРЕНА (волны роботов)", "НАСТРОЙКИ", "ВЫХОД" };
        for (int i = 0; i < 4; ++i) {
            // Полупрозрачная подложка строки — «строка меню» как в киберпанк-интерфейсах
            BorderImage* rowBg = mainMenuPanel_->CreateChild<BorderImage>();
            rowBg->SetPosition(sw / 2 - 260, sh / 2 - 70 + i * 48);
            rowBg->SetSize(520, 40);
            rowBg->SetColor(Color(0.05f, 0.12f, 0.2f, i == menuSel_ ? 0.45f : 0.0f));
            rowBg->SetName(String("MenuRow") + String(i));
            menuRows_.Push(rowBg);   // кэшируем: FindChild по имени недоступен в этой сборке

            Text* it = mainMenuPanel_->CreateChild<Text>();
            it->SetFont(font_, 28);
            it->SetText(items[i]);
            it->SetPosition(sw / 2 - 260, sh / 2 - 66 + i * 48);
            it->SetWidth(520);
            mainMenuItems_.Push(SharedPtr<Text>(it));
        }

        Text* hint = mainMenuPanel_->CreateChild<Text>();
        hint->SetFont(font_, 16);
        hint->SetText("W/S или стрелки — выбор   |   ENTER — запуск   |   двигай мышью — почувствуй параллакс");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - 350, sh / 2 + 150);
        hint->SetWidth(700);
        hint->SetColor(Color(0.7f, 0.75f, 0.8f, 0.9f));
        menuHint_ = SharedPtr<Text>(hint);

        UpdateMainMenuSelection();
    }

    // Анимация главного меню: вызывается каждый кадр когда панель видима
    void AnimateMainMenu(float dt) {
        if (!mainMenuPanel_ || !mainMenuPanel_->IsVisible()) return;
        auto* gfx = GetSubsystem<Graphics>();
        int sw = gfx->GetWidth(), sh = gfx->GetHeight();

        // Золотое «дыхание» заголовка + лёгкий дрейф (SetX/SetY нет — только SetPosition)
        if (!menuBasesSet_) {
            if (menuTitle_)      menuTitleBase_  = menuTitle_->GetPosition();
            if (menuPlate_)      menuPlateBase_  = menuPlate_->GetPosition();
            if (menuAccentBar_)  menuBarBase_    = menuAccentBar_->GetPosition();
            menuBasesSet_ = true;
        }
        if (menuTitle_) {
            float pulse = 0.75f + 0.25f * sinf(timeAcc_ * 1.6f);
            menuTitle_->SetColor(Color(pulse, pulse * 0.82f, pulse * 0.28f, 1.0f));
        }
        // Пульс акцентной черты (ширина туда-сюда)
        if (menuAccentBar_) {
            int w = 300 + int(120.0f * (0.5f + 0.5f * sinf(timeAcc_ * 1.2f)));
            menuAccentBar_->SetSize(w, 2);
            menuAccentBar_->SetColor(Color(1.0f, 0.8f, 0.2f, 0.55f + 0.45f * sinf(timeAcc_ * 2.4f)));
        }
        // Мерцание подзаголовка (редкое «неоновое» мигание)
        if (menuSub_) {
            float flick = (sinf(timeAcc_ * 7.0f) > 0.985f) ? 0.35f : 0.9f;
            menuSub_->SetColor(Color(0.5f, 0.8f, 1.0f, flick));
        }
        // Подсказка дышит альфой
        if (menuHint_) {
            menuHint_->SetColor(Color(0.7f, 0.75f, 0.8f, 0.55f + 0.35f * sinf(timeAcc_ * 2.0f)));
        }
        // Частицы летят вверх и респаунятся снизу
        for (unsigned i = 0; i < menuParticles_.Size(); ++i) {
            BorderImage* p = menuParticles_[i].Get();
            float speed = menuParticleSpeed_[i];
            float phase = menuParticlePhase_[i];
            int py = sh - int(fmodf(timeAcc_ * speed + phase * sh, float(sh + 40))) + 20;
            int px = int(menuParticleX_[i]) + int(sinf(timeAcc_ * 0.7f + phase * 6.283f) * 14.0f);
            p->SetPosition(px, py);
            // Затухание у верхнего края
            float a = 0.55f * (float(py) / float(sh)) + 0.1f;
            Color c = p->GetColor(C_TOPLEFT); c.a_ = a; p->SetColor(c);
        }
        // Параллакс от мыши: панель чуть сдвигается за курсором (глубина)
        auto* input = GetSubsystem<Input>();
        IntVector2 mp = input->GetMousePosition();
        float nx = (float(mp.x_) / sw - 0.5f);
        float ny = (float(mp.y_) / sh - 0.5f);
        if (menuPlate_) {
            menuPlate_->SetPosition(menuPlateBase_.x_ + int(nx * 8.0f),
                                    menuPlateBase_.y_ + int(ny * 5.0f));
        }
        if (menuTitle_) {
            menuTitle_->SetPosition(menuTitleBase_.x_ - int(nx * 6.0f),
                                    menuTitleBase_.y_ + int(sinf(timeAcc_ * 0.8f) * 3.0f));
        }
        if (menuAccentBar_) {
            int w = menuAccentBar_->GetWidth();
            menuAccentBar_->SetPosition(sw / 2 - w / 2 - int(nx * 4.0f), menuBarBase_.y_);
        }
    }

    void ShowMainMenu(bool show) {
        inMenu_ = show;
        if (show) {
            if (!mainMenuPanel_) CreateMainMenu();
            mainMenuPanel_->SetVisible(true);
            SetGameHudVisible(false);
            auto* input = GetSubsystem<Input>();
            input->SetMouseVisible(false);
        } else {
            if (mainMenuPanel_) mainMenuPanel_->SetVisible(false);
            SetGameHudVisible(true);
        }
    }

    void SetGameHudVisible(bool vis) {
        if (crosshairCenter_) crosshairCenter_->SetVisible(vis);
        if (crosshairTop_) crosshairTop_->SetVisible(vis);
        if (crosshairBottom_) crosshairBottom_->SetVisible(vis);
        if (crosshairLeft_) crosshairLeft_->SetVisible(vis);
        if (crosshairRight_) crosshairRight_->SetVisible(vis);
        if (ammoText_) ammoText_->SetVisible(vis);
        if (healthText_) healthText_->SetVisible(vis);
        if (scoreText_) scoreText_->SetVisible(vis);
        if (accuracyText_) accuracyText_->SetVisible(vis);
        if (messageText_) messageText_->SetVisible(vis);
        if (weaponNameText_) weaponNameText_->SetVisible(vis);
        if (helpText_) helpText_->SetVisible(vis);
        if (hudPanelBg_) hudPanelBg_->SetVisible(vis);
        if (killFeedText_) killFeedText_->SetVisible(vis);
        if (waveBanner_) waveBanner_->SetVisible(vis);
        if (hudHpBarBg_) hudHpBarBg_->SetVisible(vis);
        if (hudHpBarFill_) hudHpBarFill_->SetVisible(vis);
        if (hudAmmoBarBg_) hudAmmoBarBg_->SetVisible(vis);
        if (hudAmmoBarFill_) hudAmmoBarFill_->SetVisible(vis);
        if (damageOverlay_) damageOverlay_->SetVisible(vis);
        if (reloadOverlay_) reloadOverlay_->SetVisible(vis);
        // SetOriginEnabled нет — видимость оружия управляется через SetVisible(node)
    }

    void UpdateMainMenuSelection() {
        static const char* labels[4] = { "ТИР (мишени)", "АРЕНА (волны роботов)", "НАСТРОЙКИ", "ВЫХОД" };
        auto* gfx = GetSubsystem<Graphics>();
        int sw = gfx->GetWidth(), sh = gfx->GetHeight();
        for (unsigned i = 0; i < mainMenuItems_.Size(); ++i) {
            bool sel = ((int)i == menuSel_);
            mainMenuItems_[i]->SetText((sel ? "\xe2\x96\xb6  " : "   ") + String(labels[i]) + (sel ? "  \xe2\x97\x80" : ""));
            // Выбранный — золотой с лёгким свечением, остальные приглушённые
            mainMenuItems_[i]->SetColor(sel ? Color(1.0f, 0.87f, 0.3f, 1.0f) : Color(0.55f, 0.6f, 0.7f, 1.0f));
            // Подсветка строки-подложки под выбранным пунктом (кэш menuRows_, FindChild по имени нет)
            if ((int)menuRows_.Size() > (int)i && i < mainMenuItems_.Size()) {
                BorderImage* bi = static_cast<BorderImage*>(menuRows_[i]);
                if (bi) {
                    bi->SetColor(Color(0.05f, 0.14f, 0.22f, sel ? 0.5f : 0.0f));
                    // Золотая «засечка»: у выбранной строки сдвигаем подложку на 4px вправо
                    IntVector2 base = bi->GetPosition();
                    bi->SetPosition(sw / 2 - 260 + (sel ? 4 : 0), base.y_);
                }
            }
        }
    }

    // =========================================================================
    // ПАНЕЛЬ НАСТРОЕК
    // =========================================================================

    void CreateSettingsPanel() {
        if (settingsPanel_) return;
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        settingsPanel_ = root->CreateChild<UIElement>("SettingsPanel");
        settingsPanel_->SetSize(sw, sh);

        BorderImage* bg = settingsPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.01f, 0.02f, 0.04f, 0.88f));

        Text* title = settingsPanel_->CreateChild<Text>();
        title->SetFont(font_, 40);
        title->SetText("НАСТРОЙКИ");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - 300, sh / 2 - 220);
        title->SetWidth(600);
        title->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));

        for (int i = 0; i < 8; ++i) {
            Text* row = settingsPanel_->CreateChild<Text>();
            row->SetFont(font_, 24);
            row->SetPosition(sw / 2 - 320, sh / 2 - 140 + i * 44);
            row->SetWidth(640);
            settingsItems_.Push(SharedPtr<Text>(row));
        }

        Text* hint = settingsPanel_->CreateChild<Text>();
        hint->SetFont(font_, 16);
        hint->SetText("A/D или стрелки - менять, Enter - применить/назад, Esc - назад (применяет автоматически)");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - 450, sh / 2 + 190);
        hint->SetWidth(900);
        hint->SetColor(Color(0.7f, 0.75f, 0.8f, 0.9f));

        UpdateSettingsDisplay();
    }

    void ShowSettings(bool show) {
        inSettings_ = show;
        if (show) {
            if (!settingsPanel_) CreateSettingsPanel();
            settingsPanel_->SetVisible(true);
        } else {
            if (settingsPanel_) settingsPanel_->SetVisible(false);
            SaveSettingsFile();
            PlayUISound();
        }
    }

    void UpdateSettingsDisplay() {
        static const char* shadowNames[3] = { "1K", "2K", "4K" };
        static const char* bloomNames[3]  = { "выкл", "слабый", "полный" };
        static const char* fpsNames[3]    = { "60", "120", "без лимита" };
        static const char* filterNames[3] = { "билинейная", "трилинейная", "анизотропная" };
        static const char* sensNames[3] = { "низкая", "обычная", "высокая" };
        const char* names[8] = { "Тени", "HDR Bloom", "Лимит FPS", "Фильтрация текстур",
                                 "Чувствительность мыши", "Громкость эффектов", "Громкость музыки", "Назад" };
        String vals[8];
        vals[0] = shadowNames[Clamp(gfxShadowIdx_, 0, 2)];
        vals[1] = bloomNames[Clamp(gfxGlowIdx_, 0, 2)];
        vals[2] = fpsNames[Clamp(gfxFpsIdx_, 0, 2)];
        vals[3] = filterNames[Clamp(gfxAnisoIdx_, 0, 2)];
        vals[4] = sensNames[Clamp(mouseSensIdx_, 0, 2)];
        vals[5] = String((int)(sfxVolume_ * 100.0f)) + "%";
        vals[6] = String((int)(musicVolume_ * 100.0f)) + "%";
        vals[7] = "<- в меню";
        for (unsigned i = 0; i < settingsItems_.Size(); ++i) {
            bool sel = ((int)i == settingsSel_);
            settingsItems_[i]->SetText((sel ? ">  " : "   ") + String(names[i]) + ":  " + vals[i]);
            settingsItems_[i]->SetColor(sel ? Color(1.0f, 0.85f, 0.2f, 1.0f) : Color(0.85f, 0.85f, 0.9f, 1.0f));
        }
    }

    void ChangeSetting(int delta) {
        switch (settingsSel_) {
        case 0: gfxShadowIdx_ = Clamp(gfxShadowIdx_ + delta, 0, 2); break;
        case 1: gfxGlowIdx_   = Clamp(gfxGlowIdx_ + delta, 0, 2); break;
        case 2: gfxFpsIdx_    = Clamp(gfxFpsIdx_ + delta, 0, 2); break;
        case 3: gfxAnisoIdx_  = Clamp(gfxAnisoIdx_ + delta, 0, 2); break;
        case 4: mouseSensIdx_ = Clamp(mouseSensIdx_ + delta, 0, 2); break;
        case 5: sfxVolume_   = Clamp(sfxVolume_ + delta * 0.1f, 0.0f, 1.0f); break;
        case 6: musicVolume_ = Clamp(musicVolume_ + delta * 0.1f, 0.0f, 1.0f); break;
        default: return;
        }
        PlayUISound();
        ApplyGraphicsSettings();
        UpdateSettingsDisplay();
    }

    // =========================================================================
    // РЕЖИМЫ ИГРЫ
    // =========================================================================

    void ClearEnemies() {
        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            RemoveColliderLite(enemies_[i].colliderIdx_); // чистим Lite-реестр роботов
            if (enemies_[i].node_) enemies_[i].node_->Remove();
        }
        enemies_.Clear();
        // Снаряды роботов тоже чистим — иначе они «доживают» до новой волны
        for (unsigned i = 0; i < enemyProjectiles_.Size(); ++i)
            if (enemyProjectiles_[i].node_) enemyProjectiles_[i].node_->Remove();
        enemyProjectiles_.Clear();
    }

    void StartGameMode(GameMode mode) {
        gameMode_ = mode;
        player_.score_ = 0;
        player_.kills_ = 0;
        player_.shotsFired_ = 0;
        player_.shotsHit_ = 0;
        player_.health_ = player_.maxHealth_; // полный HP при старте режима / рестарте арены
        bestCombo_ = 0;
        comboCount_ = 0;
        comboTimer_ = 0.0f;
        waveRespawnTimer_ = 0.0f;
        rangeRespawnTimer_ = 0.0f;
        damageFlashTimer_ = 0.0f;
        hitMarkerTimer_ = 0.0f;
        hitKillTimer_ = 0.0f;
        gameOver_ = false;
        arenaTime_ = 0.0f;
        killFeed_.Clear();
        if (killFeedText_) killFeedText_->SetText("");
        waveBannerTimer_ = 0.0f;
        if (waveBanner_) waveBanner_->SetEnabled(false);
        ClearEnemies();

        // Полный боезапас на старте/рестарте режима (патроны теперь хранятся по оружиям)
        for (int i = 0; i < WEAPON_COUNT; ++i) ammoInit_[i] = false;
        EquipWeapon(currentWeapon_, true);
        ResetPickups();

        UpdateHealthDisplay();

        if (mode == MODE_ARENA) {
            waveNum_ = 0;
            SpawnWave();
            ShowMessage("РЕЖИМ: АРЕНА - отбивай волны роботов! F - сменить режим");
        } else {
            waveNum_ = 0;
            // Тир перезапускается с целыми мишенями: раньше после зачистки они не появлялись
            // (кроме «доапсывания» по одной раз в 8 секунд).
            ResetRangeTargets();
            ShowMessage("РЕЖИМ: ТИР - уничтожь все мишени! F - арена");
        }
        UpdateScoreDisplay();
        ShowMainMenu(false);

        // Первый запуск: сразу показать справку, чтобы не гадать про управление
        if (!helpShownOnce_) {
            helpShownOnce_ = true;
            CreateHelpPanel();
        }
    }

    // Полная пересборка мишеней тира (без накопления узлов и коллайдеров)
    void ResetRangeTargets() {
        for (unsigned i = 0; i < targets_.Size(); ++i) {
            RemoveColliderLite(targets_[i].colliderIdx_);
            if (targets_[i].node_) targets_[i].node_->Remove();
        }
        targets_.Clear();
        movingTargetsLite_.Clear();
        CreateTargets();
    }

    // =========================================================================
    // ВРАГИ-РОБОТЫ: спавн, ИИ, урон
    // =========================================================================

    void EnsureRobotMaterials() {
        if (robotBodyMat_) return;
        robotBodyMat_ = MakeTinted(nullptr, nullptr, Color(150, 155, 165));
        // Глаза роботов кэшируются отдельно по типу (см. EnsureEyeMaterials ниже)
    }

    // Материалы глаз кэшируются по типу робота (4 штуки на всю сессию)
    void EnsureEyeMaterials() {
        if (eyeMatMelee_) return;
        auto make = [this](const Color& glow) -> SharedPtr<Material> {
            SharedPtr<Material> m = MakeTinted(nullptr, nullptr, Color(255, 255, 255));
            if (m) m->SetShaderParameter("MatDiffColor", Variant(glow));
            return m;
        };
        eyeMatMelee_   = make(Color(1.0f, 0.25f, 0.15f));
        eyeMatCharger_ = make(Color(1.0f, 0.55f, 0.05f));
        eyeMatShooter_ = make(Color(0.85f, 0.3f, 1.0f));
        eyeMatBoss_    = make(Color(1.0f, 0.8f, 0.15f));
    }

    void SpawnEnemyRobot(const Vector3& pos, EnemyType type) {
        EnsureRobotMaterials();
        EnsureEyeMaterials();
        Node* rob = scene_->CreateChild("Enemy", LOCAL);
        rob->SetPosition(pos);
        rob->SetVar("enemy", true); rob->AddTag("Enemy");

        // Курица вместо робота: тело/голова/крылья/хвост/лапы из примитивов.
        // Материалы перьев выбираются по типу внутри BuildChicken.
        BuildChicken(rob, type);

        // Масштаб модели: босс крупнее, стрелок чуть «суше»
        float scale = (type == E_BOSS) ? 1.75f : (type == E_SHOOTER ? 0.92f : 1.0f);
        rob->SetScale(Vector3::ONE * scale);

        // Появление: «плюх» портала + кудахтанье (раньше куры появлялись бесшумно из воздуха)
        if (!spawnRingMat_) spawnRingMat_ = MakeTinted(nullptr, nullptr, Color(120, 235, 255));
        CreateShockwave(pos, spawnRingMat_.Get(), 0.45f);
        PlayCluck(pos, 0.5f);

        // Lite-коллайдер робота: tag 2, ptr = узел (рейкасты пуль и LOS проходят через него)
        // Коллайдер под габариты курицы (тело + хвост), а не «человеческого» бокса:
        // раньше пули пролетали мимо видимого тела и не попадали по хвосту/крыльям.
        int colIdx = AddBoxCollider(pos + Vector3(0, 0.85f, 0),
                                    Vector3(0.58f, 0.85f, 0.62f) * scale, 2, rob);

        EnemyData e;
        e.node_ = rob;
        e.colliderIdx_ = colIdx;
        e.type_ = type;
        switch (type) {
            case E_CHARGER:
                e.health_ = 80.0f;  e.speed_ = 3.4f;  e.scoreValue_ = 200;
                e.patrolRadius_ = 3.0f; break;
            case E_SHOOTER:
                e.health_ = 70.0f;  e.speed_ = 1.5f;  e.scoreValue_ = 220;
                e.patrolRadius_ = 7.0f; break;
            case E_BOSS:
                e.health_ = 400.0f; e.speed_ = 2.2f;  e.scoreValue_ = 800;
                e.patrolRadius_ = 6.0f; break;
            default: // E_MELEE — «штурмовик», идёт в плотную
                e.health_ = 100.0f + (float)waveNum_ * 8.0f; // волны постепенно жирнее
                e.speed_  = 1.9f; e.scoreValue_ = 150;
                e.patrolRadius_ = 4.0f; break;
        }
        e.maxHealth_ = e.health_;
        e.alive_ = true;
        e.patrolCenter_ = pos;
        e.phase_ = (float)(rand() % 628) / 100.0f;
        e.attackTimer_ = 0.5f + (float)(rand() % 100) / 100.0f; // разносим атаки по фазе
        e.strafePhase_ = (float)(rand() % 628) / 100.0f;
        e.burstShots_ = 0.0f;
        e.burstTimer_ = 0.0f;
        e.recoilKick_ = 0.0f;
        e.headNode_ = rob->GetChild("Head", true);
        e.wingLNode_ = rob->GetChild("WingL", true);
        e.wingRNode_ = rob->GetChild("WingR", true);
        e.legLNode_ = rob->GetChild("HipL", true);
        e.legRNode_ = rob->GetChild("HipR", true);
        e.tailNode_ = rob->GetChild("Tail", true);
        e.animPhase_ = (float)(rand() % 628) / 100.0f;   // рассинхронизируем походку
        e.cluckTimer_ = 2.0f + (float)(rand() % 600) / 100.0f;
        enemies_.Push(e);
    }

    // Выбор типа робота для спавна с учётом номера волны
    EnemyType PickEnemyType(int slot, int total) {
        if (waveNum_ >= 5 && slot == 0 && (waveNum_ % 5 == 0)) return E_BOSS;      // босс каждые 5 волн
        // Ветка "charagers с 3-й волны" была недостижима: её перекрывал waveNum_ >= 2 выше.
        if (waveNum_ >= 2) {                                                        // стрелки/чанжеры со 2-й волны
            int roll = rand() % 100;
            if (roll < 25) return E_SHOOTER;
            if (roll < 50) return E_CHARGER;
        }
        return E_MELEE;
    }

    void SpawnWave() {
        ++waveNum_;

        // Новая волна = снабжение: без этого, как только кончаются патроны,
        // арена становится непроходимой (переключение оружия больше не читерит).
        for (int w = 0; w < WEAPON_COUNT; ++w) {
            if (!ammoInit_[w]) continue;
            reserveAmmo_[w] = WEAPON_DEFS[w].reserveMax;
        }
        reserveAmmo_[currentWeapon_] = WEAPON_DEFS[currentWeapon_].reserveMax;
        weapon_.reserveAmmo_ = reserveAmmo_[currentWeapon_];
        UpdateAmmoDisplay();

        const bool bossWave = (waveNum_ % 5 == 0) && waveNum_ >= 5;
        int count = Min(3 + waveNum_, 10);
        const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f - 6.0f;
        Vector3 playerPos = cameraNode_ ? cameraNode_->GetPosition() : Vector3::ZERO;
        int spawned = 0;
        for (int i = 0; i < count; ++i) {
            // Ищем свободную точку: раньше робот мог появиться внутри колонны или ящика
            // и застрять там навсегда (волна не могла закончиться).
            Vector3 pos;
            bool ok = false;
            for (int attempt = 0; attempt < 24 && !ok; ++attempt) {
                float ang = (float)(rand() % 360) * 0.0174533f;
                float r = 10.0f + (float)(rand() % 30);
                pos = Vector3(cosf(ang) * r, 0.06f, sinf(ang) * r);
                pos.x_ = Clamp(pos.x_, -half, half);
                pos.z_ = Clamp(pos.z_, -half, half);
                Vector3 flat = pos - playerPos; flat.y_ = 0.0f;
                ok = flat.Length() > 7.0f && IsPointFree(pos, 0.8f);
            }
            if (!ok) continue;
            EnemyType t = PickEnemyType(i, count);
            SpawnEnemyRobot(pos, t);
            ++spawned;
        }
        String msg = "ВОЛНА " + String(waveNum_) + "! Кур: " + String(spawned);
        if (bossWave) msg += "  [БОСС!]";
        ShowMessage(msg);

        // Баннер + звук старта волны (босс-волна — отдельный цвет и рёв)
        if (waveStartSound_ && soundSource_) {
            soundSource_->SetGain(SfxGain(bossWave ? 0.9f : 0.65f));
            soundSource_->Play(waveStartSound_);
        }
        if (bossWave && bossRoarSound_) PlaySfx3D(Vector3(0, 2, 0), bossRoarSound_, 1.0f);
        ShowWaveBanner(bossWave ? ("ВОЛНА " + String(waveNum_) + " — ИНДЮК!")
                                : ("ВОЛНА " + String(waveNum_)),
                       bossWave ? Color(1.0f, 0.35f, 0.25f) : Color(1.0f, 0.85f, 0.25f));
        PushKillFeed("ВОЛНА " + String(waveNum_) + " — кур: " + String(spawned) + (bossWave ? " + БОСС" : ""));
        UpdateScoreDisplay();
    }

    // Проверка прямой видимости между двумя точками (препятствия: слой 1 — статика, 2 — роботы)
    bool HasLineOfSight(const Vector3& from, const Vector3& to) {
        Vector3 dir = to - from;
        float dist = dir.Length();
        if (dist < 0.01f) return true;
        dir /= dist;
        float outDist; Vector3 outPoint;
        // Битовая маска: (1<<0) статика | (1<<4) движущиеся мишени — роботы не перекрывают LOS друг другу
        return !RaycastLite(from, dir, dist - 0.3f, (1 << 0) | (1 << 4), outDist, outPoint);
    }

    void SpawnEnemyProjectile(const Vector3& origin, const Vector3& targetCamPos, float damage) {
        Vector3 dir = targetCamPos - origin;
        float len = dir.Length();
        if (len < 0.5f) return;
        dir /= len;
        // Разброс тем сильнее, чем дальше цель
        float spread = 0.02f + len * 0.004f;
        dir.x_ += ((float)(rand() % 1000 - 500) / 500.0f) * spread;
        dir.y_ += ((float)(rand() % 1000 - 500) / 500.0f) * spread * 0.5f;
        dir.Normalize();

        Node* pr = scene_->CreateChild("EnemyShot", LOCAL);
        pr->SetPosition(origin);
        StaticModel* sm = pr->CreateComponent<StaticModel>();
        sm->SetModel(sphereModel_ ? sphereModel_ : boxModel_);
        if (!enemyShotMat_) enemyShotMat_ = MakeTinted(nullptr, nullptr, Color(255, 80, 255));
        sm->SetMaterial(enemyShotMat_.Get());
        sm->SetCastShadows(false);
        if (eggMat_) sm->SetMaterial(eggMat_.Get());          // стрелок плюётся яйцом
        pr->SetScale(Vector3(0.16f, 0.21f, 0.16f));
        pr->SetRotation(Quaternion((float)(rand() % 360), Vector3(1, 0, 0)));

        ProjectileData pd;
        pd.node_ = pr;
        pd.vel_ = dir * 26.0f; // скорость заметно ниже пули — игрок успевает увернуться
        pd.ttl_ = 3.0f;
        pd.damage_ = damage;
        enemyProjectiles_.Push(pd);

        // Звук выстрела робота: играем 3D-источником. Раньше здесь вызывался PlayHitSound(),
        // т.е. игрок слышал «попадание», хотя в него только что выстрелили.
        if (explosionSource3D_ && shootSounds_[W_PISTOL]) {
            explosionSource3D_->GetNode()->SetWorldPosition(origin);
            explosionSource3D_->SetGain(SfxGain(0.55f));
            explosionSource3D_->Play(shootSounds_[W_PISTOL]);
        }
    }

    // Полёт снарядов роботов: движение, попадание по игроку (дистанционная проверка), пролёты мимо
    void UpdateEnemyProjectiles(float dt) {
        if (!cameraNode_) return;
        Vector3 camPos = cameraNode_->GetPosition();
        for (unsigned i = enemyProjectiles_.Size(); i-- > 0; ) {
            ProjectileData& p = enemyProjectiles_[i];
            p.ttl_ -= dt;
            bool dead = (p.ttl_ <= 0.0f);
            if (!dead && p.node_) {
                Vector3 pos = p.node_->GetPosition();
                Vector3 step = p.vel_ * dt;
                Vector3 next = pos + step;
                // Попадание по игроку: сфера радиуса ~0.7 вокруг камеры
                Vector3 toCam = camPos - next;
                if (toCam.LengthSquared() < 0.5f) {
                    if (!gameOver_) DamagePlayer(p.damage_);
                    CreateHitSpark(next);
                    dead = true;
                } else {
                    // Столкновение со статикой/мишенями: короткий Lite-рейкаст по направлению полёта
                    float stepLen = step.Length();
                    if (stepLen > 0.001f) {
                        float outDist; Vector3 outPoint;
                        if (RaycastLite(pos, step / stepLen, stepLen, (1 << 0) | (1 << 1) | (1 << 4), outDist, outPoint)) {
                            CreateHitSpark(outPoint);
                            dead = true;
                        }
                    }
                    if (!dead) p.node_->SetPosition(next);
                }
            }
            if (dead) {
                if (p.node_) {
                    PlaySfx3D(p.node_->GetPosition(), eggSplatSound_, 0.5f);   // «шлёп» разбитого яйца
                    p.node_->Remove();
                }
                enemyProjectiles_.Erase(i);
            }
        }
    }

    // =========================================================================
    // ПИКАПЫ: аптечки (+25 HP) и патроны (+2 магазина в резерв), респавн 18 с
    // =========================================================================

    void CreatePickups() {
        if (!pickupHealMat_) pickupHealMat_ = MakeTinted(nullptr, nullptr, Color(80, 255, 120));
        if (!pickupAmmoMat_) pickupAmmoMat_ = MakeTinted(nullptr, nullptr, Color(255, 210, 90));

        // Точки по арене: 4 у центра (по краям платформы), 4 в углах
        const Vector3 spots[] = {
            Vector3(13.0f, 1.0f,  0.0f), Vector3(-13.0f, 1.0f,  0.0f),
            Vector3( 0.0f, 1.0f, 13.0f), Vector3( 0.0f, 1.0f, -13.0f),
            Vector3( 34.0f, 1.0f,  34.0f), Vector3(-34.0f, 1.0f,  34.0f),
            Vector3( 34.0f, 1.0f, -34.0f), Vector3(-34.0f, 1.0f, -34.0f)
        };
        SharedPtr<Material> crossMat = MakeTinted(nullptr, nullptr, Color(235, 45, 55));
        SharedPtr<Material> caseMat = MakeTinted(nullptr, nullptr, Color(90, 70, 55));
        Model* sph = sphereModel_ ? sphereModel_ : boxModel_;

        for (unsigned i = 0; i < sizeof(spots) / sizeof(spots[0]); ++i) {
            Node* n = scene_->CreateChild("Pickup", LOCAL);
            n->SetPosition(spots[i]);
            n->SetScale(Vector3::ONE * 0.4f);

            if (i % 2 == 0) {
                // Аптечка-яйцо с красным крестом (курица же)
                AddPart(n, Vector3(0, 0, 0), Vector3(0.88f, 1.05f, 0.88f), pickupHealMat_.Get(), sph);
                AddPart(n, Vector3(0, 0.05f, 0.42f), Vector3(0.48f, 0.16f, 0.12f), crossMat.Get());
                AddPart(n, Vector3(0, 0.05f, 0.43f), Vector3(0.16f, 0.48f, 0.12f), crossMat.Get());
            } else {
                // Ящик с патронами: дерево + жёлтая эмиссивная лента
                AddPart(n, Vector3(0, 0, 0), Vector3(0.8f, 0.6f, 0.8f), caseMat.Get());
                AddPart(n, Vector3(0, 0.05f, 0), Vector3(0.86f, 0.14f, 0.86f), pickupAmmoMat_.Get());
                for (int b = 0; b < 3; ++b)
                    AddPart(n, Vector3((float)(b - 1) * 0.2f, 0.36f, 0.0f), Vector3(0.14f, 0.30f, 0.14f),
                            pickupAmmoMat_.Get(), cylModel_);
            }
            // Световой «постамент» под пикапом, чтобы его было видно издалека
            Node* halo = AddPart(n, Vector3(0, -0.55f, 0), Vector3(1.6f, 0.06f, 1.6f),
                                 (i % 2 == 0) ? pickupHealMat_.Get() : pickupAmmoMat_.Get(), cylModel_);
            if (halo) halo->SetScale(Vector3(1.6f, 0.06f, 1.6f));

            PickupData p;
            p.node_ = n;
            p.home_ = spots[i];
            p.type_ = (int)(i % 2);
            p.respawn_ = 0.0f;
            pickups_.Push(p);
        }
    }

    void ResetPickups() {
        for (unsigned i = 0; i < pickups_.Size(); ++i) {
            PickupData& p = pickups_[i];
            p.respawn_ = 0.0f;
            if (p.node_) {
                p.node_->SetEnabled(true);
                p.node_->SetPosition(p.home_);
                p.node_->SetScale(Vector3::ONE * 0.4f);
            }
        }
    }

    void UpdatePickups(float dt) {
        if (!cameraNode_) return;
        Vector3 camPos = cameraNode_->GetPosition();
        const float r2 = GameConstants::PICKUP_RADIUS * GameConstants::PICKUP_RADIUS;

        for (unsigned i = 0; i < pickups_.Size(); ++i) {
            PickupData& p = pickups_[i];
            Node* n = p.node_.Get();
            if (!n) continue;

            if (p.respawn_ > 0.0f) {
                p.respawn_ -= dt;
                if (p.respawn_ <= 0.0f) {
                    n->SetEnabled(true);
                    n->SetScale(Vector3::ONE * 0.4f);
                }
                continue;
            }

            // Медленное вращение + парение, чтобы пикап читался на фоне арены
            n->SetRotation(Quaternion(timeAcc_ * 70.0f + (float)i * 37.0f, Vector3::UP));
            n->SetPosition(p.home_ + Vector3(0.0f, sinf(timeAcc_ * 2.0f + (float)i) * 0.15f, 0.0f));

            if ((camPos - n->GetPosition()).LengthSquared() > r2) continue;

            if (p.type_ == 0) {
                if (player_.health_ >= player_.maxHealth_) continue;  // полному HP аптечка не нужна
                player_.health_ = Min(player_.maxHealth_, player_.health_ + GameConstants::PICKUP_HEAL);
                UpdateHealthDisplay();
                ShowMessage("АПТЕЧКА +" + String((int)GameConstants::PICKUP_HEAL) + " HP");
                PushKillFeed("аптечка  +" + String((int)GameConstants::PICKUP_HEAL) + " HP");
            } else {
                const WeaponDef& def = WEAPON_DEFS[currentWeapon_];
                int cur = reserveAmmo_[currentWeapon_];
                if (cur >= def.reserveMax) continue;                  // резерв и так полный
                int next = Min(def.reserveMax, cur + def.magSize * 2);
                reserveAmmo_[currentWeapon_] = next;
                weapon_.reserveAmmo_ = next;
                UpdateAmmoDisplay();
                ShowMessage("ПАТРОНЫ +" + String(next - cur));
                PushKillFeed("патроны  +" + String(next - cur));
            }
            PlayUISound();
            p.respawn_ = GameConstants::PICKUP_RESPAWN;
            n->SetEnabled(false);
        }
    }

    // Движение «летучих» мишеней тира без Bullet: простая интеграция + отскок от границ
    void UpdateMovingTargetsLite(float dt) {
        const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f - 3.0f;
        for (unsigned i = 0; i < movingTargetsLite_.Size(); ++i) {
            MovingTargetLite& mt = movingTargetsLite_[i];
            // Уничтоженная мишень остаётся жить как отсоединённый узел (SharedPtr держит её),
            // поэтому проверяем ещё и сцену — иначе запись висит вечно.
            if (!mt.node_ || !mt.node_->GetScene()) continue;
            Vector3 p = mt.node_->GetPosition();
            p += mt.vel_ * dt;
            if (fabsf(p.x_) > half) { mt.vel_.x_ = -mt.vel_.x_; p.x_ = Clamp(p.x_, -half, half); }
            if (fabsf(p.z_) > half) { mt.vel_.z_ = -mt.vel_.z_; p.z_ = Clamp(p.z_, -half, half); }
            mt.node_->SetPosition(p);
            MoveColliderLite(mt.colliderIdx_, p);
        }
    }

    // Мини-физика осколков: гравитация, отскок от пола y=0.05, затухание
    void UpdateDebrisLite(float dt) {
        for (unsigned i = debrisLite_.Size(); i-- > 0; ) {
            DebrisData& d = debrisLite_[i];
            if (!d.node_ || !d.node_->GetScene()) { debrisLite_.Erase(i); continue; }
            d.ttl_ -= dt;
            if (d.ttl_ <= 0.0f) { d.node_->Remove(); debrisLite_.Erase(i); continue; }
            d.vel_.y_ -= GameConstants::GRAVITY * dt;
            d.pos_ += d.vel_ * dt;
            if (d.pos_.y_ < 0.05f) {
                d.pos_.y_ = 0.05f;
                d.vel_.y_ = -d.vel_.y_ * 0.35f;       // restitution
                d.vel_.x_ *= 0.7f; d.vel_.z_ *= 0.7f; // трение о пол
            }
            d.node_->SetPosition(d.pos_);
            d.node_->SetRotation(Quaternion(d.spin_.x_ * dt * 57.3f, Vector3(1,0,0)) *
                                 Quaternion(d.spin_.y_ * dt * 57.3f, Vector3(0,1,0)));
        }
    }

    void UpdateEnemies(float dt) {
        Vector3 camPos = cameraNode_ ? cameraNode_->GetPosition() : Vector3::ZERO;
        // Сначала убираем «мёртвые» записи из массива (чистка памяти)
        for (unsigned i = enemies_.Size(); i-- > 0; ) {
            if (!enemies_[i].alive_) { RemoveColliderLite(enemies_[i].colliderIdx_); enemies_.Erase(i); }
        }
        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            EnemyData& e = enemies_[i];
            if (!e.alive_) continue;
            Node* n = e.node_.Get();
            if (!n) { e.alive_ = false; continue; }

            Vector3 p = n->GetPosition();
            Vector3 toCam = camPos - p; toCam.y_ = 0.0f;
            float dist = toCam.Length();
            Vector3 toDir = dist > 0.01f ? toCam / dist : Vector3::FORWARD;

            Vector3 move = Vector3::ZERO;
            e.attackTimer_ -= dt;
            e.strafePhase_ += dt * 2.2f;

            switch (e.type_) {
            case E_SHOOTER: {
                // Держит дистанцию ~10-14 м, стреляет очередями только с прямой видимостью
                bool los = HasLineOfSight(p + Vector3(0, 0.9f, 0), camPos);
                if (dist > 13.0f)      move = toDir * e.speed_;
                else if (dist < 8.0f)  move = -toDir * e.speed_;
                else                   move = Vector3(-toDir.z_, 0, toDir.x_) * e.speed_ * 0.6f * sinf(e.strafePhase_);
                // Очередь из 3 выстрелов каждые ~2.5 сек
                if (e.burstShots_ > 0.0f) {
                    e.burstTimer_ -= dt;
                    if (e.burstTimer_ <= 0.0f) {
                        e.burstShots_ -= 1.0f;
                        e.burstTimer_ = 0.18f;
                        e.recoilKick_ = 0.18f;
                        SpawnEnemyProjectile(p + Vector3(0, 1.1f, 0) + toDir * 0.6f, camPos, 8.0f);
                    }
                } else if (los && e.attackTimer_ <= 0.0f && dist < 22.0f) {
                    e.attackTimer_ = 2.2f + (float)(rand() % 80) / 100.0f;
                    e.burstShots_ = 3.0f;
                    e.burstTimer_ = 0.0f;
                }
                break;
            }
            case E_CHARGER: {
                // Быстро рвётся в упор, петляет; контактный удар 20 урона, кулдаун 1.5 сек
                Vector3 strafe(-toDir.z_, 0, toDir.x_);
                move = (toDir + strafe * 0.35f * sinf(e.strafePhase_)) * e.speed_;
                if (dist < 1.6f && e.attackTimer_ <= 0.0f) {
                    e.attackTimer_ = 1.5f;
                    e.recoilKick_ = 0.25f;
                    DamagePlayer(20.0f);
                    AddCameraShake(0.2f);
                    CreateHitSpark((p + camPos) * 0.5f);
                }
                break;
            }
            case E_BOSS: {
                // Босс: давит скоростью и контактными ударами, иногда «плюётся» залпом
                move = toDir * e.speed_;
                if (dist < 2.2f && e.attackTimer_ <= 0.0f) {
                    e.attackTimer_ = 1.2f;
                    e.recoilKick_ = 0.3f;
                    DamagePlayer(30.0f);
                    AddCameraShake(0.3f);
                    CreateHitSpark((p + camPos) * 0.5f);
                } else if (e.attackTimer_ <= 0.0f && dist > 6.0f && (rand() % 100) < 30) {
                    e.attackTimer_ = 2.5f;
                    SpawnEnemyProjectile(p + Vector3(0, 1.6f, 0) + toDir, camPos, 12.0f);
                }
                break;
            }
            default: { // E_MELEE — идёт к игроку, обходя препятствия рейкастом
                Vector3 probePos = p + Vector3(0, 0.6f, 0);
                bool blocked = false;
                if (dist > 1.5f) {
                    float outDist; Vector3 outPoint; Vector3 outNormal;
                    if (RaycastLite(probePos, toDir, 2.2f, (1 << 0) | (1 << 4), outDist, outPoint, nullptr, nullptr, &outNormal)) {
                        blocked = true;
                        // Скольжение вдоль нормали препятствия + лёгкий «инстинкт стаи»
                        Vector3 slide = outNormal.CrossProduct(Vector3::UP);
                        slide.y_ = 0;
                        if (slide.LengthSquared() > 0.001f)
                            move = slide.Normalized() * e.speed_ * (sinf(e.strafePhase_) > 0 ? 1.0f : -1.0f);
                    }
                }
                if (!blocked) move = toDir * e.speed_;
                if (dist < 1.4f && e.attackTimer_ <= 0.0f) {
                    e.attackTimer_ = 1.0f;
                    e.recoilKick_ = 0.2f;
                    DamagePlayer(10.0f);
                    AddCameraShake(0.12f);
                }
                break;
            }
            }

            // Расталкивание роботов, чтобы они не слипались в одну точку
            for (unsigned j = 0; j < enemies_.Size(); ++j) {
                if (j == i || !enemies_[j].alive_) continue;
                Node* other = enemies_[j].node_.Get();
                if (!other) continue;
                Vector3 d = p - other->GetPosition();
                d.y_ = 0.0f;
                float dd = d.Length();
                if (dd > 0.01f && dd < 1.5f) move += d / dd * (1.5f - dd) * 2.5f;
            }

            // Интеграция движения с ограничением по арене.
            // PushOutOfObstacles не даёт роботам проходить сквозь стены/ящики/платформу.
            Vector3 np = PushOutOfObstacles(p + move * dt, 0.55f, 0.55f);
            const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f - 2.0f;
            np.x_ = Clamp(np.x_, -half, half);
            np.z_ = Clamp(np.z_, -half, half);
            float bob = 0.04f * sinf(timeAcc_ * 3.0f + e.phase_);
            // Визуальный «откат» при выстреле/ударе: приседание модели
            if (e.recoilKick_ > 0.0f) e.recoilKick_ = Max(0.0f, e.recoilKick_ - dt);
            float kick = e.recoilKick_ * 0.6f;
            Vector3 newPos(np.x_, 0.06f + bob - kick, np.z_);   // лапы курицы на полу
            n->SetPosition(newPos);
            MoveColliderLite(e.colliderIdx_, newPos + Vector3(0, 0.85f, 0)); // центр тела курицы

            if (dist > 0.5f) {
                float yawDeg = atan2f(toCam.x_, toCam.z_) * 57.29578f;
                n->SetRotation(Quaternion(yawDeg, Vector3::UP));
            }

            // Курицы кудахчут в простое — арена не «мёртвая»
            e.cluckTimer_ -= dt;
            if (e.cluckTimer_ <= 0.0f) {
                e.cluckTimer_ = 6.0f + (float)(rand() % 600) / 100.0f;
                PlayCluck(p, 0.30f);
            }

            // === Куриная анимация: походка, клевки головой, взмахи крыльями ===
            {
                float speedNow = Vector3(move.x_, 0.0f, move.z_).Length();
                e.animPhase_ += dt * (3.2f + speedNow * 3.4f);
                float walk = sinf(e.animPhase_);
                if (Node* head = e.headNode_.Get()) {
                    head->SetPosition(Vector3(0.0f, CH_HEAD_Y + 0.05f * walk, CH_HEAD_Z + 0.05f * fabsf(walk)));
                    head->SetRotation(Quaternion(7.0f * walk, Vector3(1, 0, 0)));
                }
                if (Node* l = e.legLNode_.Get()) l->SetRotation(Quaternion( 30.0f * walk, Vector3(1, 0, 0)));
                if (Node* r = e.legRNode_.Get()) r->SetRotation(Quaternion(-30.0f * walk, Vector3(1, 0, 0)));
                // Крылья: мелкая дрожь на месте, широкий взмах на бегу/рывке
                float flap = 6.0f + 30.0f * Max(0.0f, walk) * (speedNow > 0.8f ? 1.0f : 0.3f);
                if (Node* wl = e.wingLNode_.Get()) wl->SetRotation(Quaternion( 18.0f + flap, Vector3(0, 0, 1)));
                if (Node* wr = e.wingRNode_.Get()) wr->SetRotation(Quaternion(-18.0f - flap, Vector3(0, 0, 1)));
                if (Node* tail = e.tailNode_.Get())
                    tail->SetRotation(Quaternion(8.0f * sinf(e.animPhase_ * 0.45f), Vector3(0, 1, 0)));
            }
        }
    }

    void DamageEnemy(EnemyData& e, float damage) {
        if (!e.alive_) return;
        e.health_ -= damage;
        Node* n = e.node_.Get();
        if (n) {
            CreateHitSpark(n->GetPosition() + Vector3(0, 0.5f, 0));
            // Пух: 2 пёрышка из точки попадания — сразу видно, что попал по курице
            if (featherMat_) {
                for (int k = 0; k < 2; ++k) {
                    Node* f = scene_->CreateChild("HitFeather", LOCAL);
                    f->SetPosition(n->GetPosition() + Vector3(0, 0.9f, 0));
                    f->SetScale(Vector3(0.05f, 0.02f, 0.18f));
                    StaticModel* fsm = f->CreateComponent<StaticModel>();
                    fsm->SetModel(boxModel_);
                    fsm->SetMaterial(featherMat_.Get());
                    fsm->SetCastShadows(false);
                    float a = (float)(rand() % 628) / 100.0f;
                    DebrisData fd;
                    fd.node_ = f;
                    fd.pos_ = n->GetPosition() + Vector3(0, 0.9f, 0);
                    fd.vel_ = Vector3(cosf(a) * 1.6f, 1.6f + (float)(rand() % 100) / 100.0f, sinf(a) * 1.6f);
                    fd.spin_ = Vector3((float)(rand() % 400 - 200), (float)(rand() % 400 - 200), 0.0f) * 0.02f;
                    fd.ttl_ = 1.8f;
                    debrisLite_.Push(fd);
                }
            }
            PlayHitSound();
            // Визуальный «откид» без физики: короткий наклон модели робота
            e.recoilKick_ = 0.35f;
        }
        if (e.health_ <= 0.0f) {
            e.alive_ = false;
            if (n) {
                CreateExplosionEffect(n->GetPosition());
                CreateFeatherBurst(n->GetPosition(), e.type_);   // курица «взрывается» перьями
                // Куриный вопль (у индюка — низкое «гло-гло»)
                PlaySfx3D(n->GetPosition(), e.type_ == E_BOSS ? bossRoarSound_ : squawkSound_, 0.85f);
                CreateDebrisBurst(n->GetPosition());
                PlayExplosionAt(n->GetPosition());
                n->Remove();
            }
            AddScoreWithCombo(e.scoreValue_);
            player_.kills_++;
            UpdateScoreDisplay();
            ShowMessage((e.type_ == E_BOSS ? "БОСС-ИНДЮК ПАЛ! +" : "КУРИЦА ОЩИПАНА! +") +
                        String(e.scoreValue_) +
                        (comboCount_ > 1 ? (" (COMBO x" + String(comboCount_) + ")") : ""));
            PushKillFeed((e.type_ == E_BOSS ? "ИНДЮК ПАЛ +" : "курица ощипана +") +
                         String(e.scoreValue_) +
                         (comboCount_ > 1 ? ("  x" + String(comboCount_)) : ""));
        }
    }

    // =========================================================================
    // ЧАСТИЦЫ: перья, осколки и искры
    // =========================================================================

    // Взрыв перьев: 12 плоских «пёрышек» на лёгкой физике осколков (UpdateDebrisLite).
    void CreateFeatherBurst(const Vector3& pos, EnemyType type) {
        Material* mat = featherMat_ ? featherMat_.Get() : fallbackMat_;
        for (int i = 0; i < 12; ++i) {
            Node* f = scene_->CreateChild("Feather", LOCAL);
            f->SetPosition(pos + Vector3(0, 0.9f, 0));
            f->SetScale(Vector3(0.05f, 0.02f, 0.20f));   // тонкое плоское пёрышко
            StaticModel* sm = f->CreateComponent<StaticModel>();
            sm->SetModel(boxModel_);
            switch (type) { // перья в цвет породы
                case E_CHARGER: sm->SetMaterial(chickBrown_ ? chickBrown_.Get() : mat); break;
                case E_SHOOTER: sm->SetMaterial(chickBlack_ ? chickBlack_.Get() : mat); break;
                case E_BOSS:    sm->SetMaterial(chickBoss_ ? chickBoss_.Get() : mat); break;
                default:        sm->SetMaterial(mat); break;
            }
            sm->SetCastShadows(false);
            float a = (float)(rand() % 628) / 100.0f;
            float up = 2.0f + (float)(rand() % 35) / 10.0f;
            DebrisData d;
            d.node_ = f;
            d.pos_ = pos + Vector3(0, 0.9f, 0);
            d.vel_ = Vector3(cosf(a) * 2.6f, up, sinf(a) * 2.6f);
            d.spin_ = Vector3((float)(rand() % 600 - 300), (float)(rand() % 600 - 300),
                              (float)(rand() % 600 - 300)) * 0.03f;
            d.ttl_ = 3.5f;
            debrisLite_.Push(d);
        }
    }

    void CreateDebrisBurst(const Vector3& pos) {
        // Lite-осколки: интеграция положения/скорости в UpdateDebrisLite, без Bullet
        for (int i = 0; i < 8; ++i) {
            Node* frag = scene_->CreateChild("Frag", LOCAL);
            frag->SetPosition(pos);
            frag->SetScale(Vector3(0.08f, 0.08f, 0.08f));
            StaticModel* sm = frag->CreateComponent<StaticModel>();
            sm->SetModel(boxModel_);
            sm->SetMaterial(robotBodyMat_ ? robotBodyMat_.Get() : metalMaterial_);
            sm->SetCastShadows(false);
            float a = (float)(rand() % 628) / 100.0f;
            float up = 3.0f + (float)(rand() % 40) / 10.0f;
            DebrisData d;
            d.node_ = frag;
            d.pos_ = pos;
            d.vel_ = Vector3(cosf(a) * 4.0f, up, sinf(a) * 4.0f);
            d.spin_ = Vector3((float)(rand() % 400 - 200), (float)(rand() % 400 - 200), (float)(rand() % 400 - 200)) * 0.02f;
            d.ttl_ = 4.0f;
            debrisLite_.Push(d);
        }

        if (haveParticles_ && sparkEffect_) {
            Node* sparks = scene_->CreateChild("DebrisSparks");
            sparks->SetPosition(pos);
            ParticleEmitter* pe = sparks->CreateComponent<ParticleEmitter>();
            pe->SetEffect(sparkEffect_);
            pe->SetNumParticles(200);
            pe->SetEmitting(true);
            PendingRemove pr2; pr2.node_ = sparks; pr2.ttl_ = 1.5f; pendingRemoves_.Push(pr2);
        }
    }

    void CreateHitSpark(const Vector3& pos) {
        if (!haveParticles_ || !sparkEffect_) return;
        Node* sparkNode = scene_->CreateChild("HitSpark");
        sparkNode->SetPosition(pos);
        ParticleEmitter* emitter = sparkNode->CreateComponent<ParticleEmitter>();
        emitter->SetEffect(sparkEffect_);
        emitter->SetNumParticles(100);
        emitter->SetEmitting(true);
        PendingRemove pr; pr.node_ = sparkNode; pr.ttl_ = 1.0f; pendingRemoves_.Push(pr);
    }

    // =========================================================================
    // КОМБО / ОЧКИ
    // =========================================================================

    void AddScoreWithCombo(int base) {
        comboCount_++;
        comboTimer_ = 4.0f;
        bestCombo_ = Max(bestCombo_, comboCount_);
        int gained = (int)(base * (1.0f + 0.5f * (float)(comboCount_ - 1)));
        player_.score_ += gained;
        // Рекорд обновляем в памяти; на диск пишем только в GAME OVER/настройках.
        // Раньше settings.ini перезаписывался на КАЖДОМ убийстве (I/O в игровом цикле).
        if (player_.score_ > highScore_) highScore_ = player_.score_;
    }
};

// ============================================================================
// Макрос регистрации приложения
// ============================================================================

URHO3D_DEFINE_APPLICATION_MAIN(MyApp)

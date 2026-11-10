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
#  if __has_include(<Urho3D/Graphics/TextureCube.h>)
#    include <Urho3D/Graphics/TextureCube.h>
#  elif __has_include(<Urho3D/GraphicsAPI/TextureCube.h>)
#    include <Urho3D/GraphicsAPI/TextureCube.h>
#  endif
#endif
#include <Urho3D/Resource/Image.h>
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

#include "LitePhysics.h"
#include "CyberSystems.h"
#include "BanyaSim.h"
#include "CasinoSim.h"

#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Container/HashMap.h>
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
    constexpr int ARENA_SIZE = 64;      // тайлов на сторону: большая арена 256x256 м
    constexpr float TILE_SIZE = 4.0f;   // крупный тайл для читаемого дальнего пространства
    constexpr float FLOOR_TILE_UV_SCALE = 6.0f; // сколько раз тайл-текстура ложится на плиту пола

    // Пол арены — аналитическая плоскость в Lite-физике (см. RaycastLite).
    constexpr float FLOOR_Y = 0.0f;
    constexpr float FLOOR_UV_TILES = 64.0f; // 256 м / 4 м = 64 повторов текстуры на сторону

    // Пикапы: время респавна после подбора (сек) и радиус подбора (м)
    constexpr float PICKUP_RESPAWN = 18.0f;
    constexpr float PICKUP_RADIUS = 1.4f;
    constexpr float PICKUP_HEAL = 25.0f;
}

// Слои Lite-физики: единая таблица вместо разрозненных «магических» чисел.
// Битовая маска слоя совпадает с tag, поэтому старые рейкасты остаются совместимыми.
namespace Collision {
    enum Tag : int {
        STATIC = 0,
        TARGET = 1,
        ENEMY = 2,
        BARREL = 3,
        MOVING_TARGET = 4,
        REMOVED = 7,
    };
    constexpr int Mask(Tag tag) { return 1 << static_cast<int>(tag); }
    constexpr int STATIC_MASK = Mask(STATIC);
    constexpr int TARGET_MASK = Mask(TARGET);
    constexpr int ENEMY_MASK = Mask(ENEMY);
    constexpr int BARREL_MASK = Mask(BARREL);
    constexpr int MOVING_TARGET_MASK = Mask(MOVING_TARGET);
}

// ============================================================================
// Структуры данных
// ============================================================================

struct TargetData {
    WeakPtr<Node> node_;
    int colliderIdx_ = -1;   // Lite-коллайдер мишени (Collision::TARGET, ptr = Node*)
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
    W_RAILGUN,
    W_PLASMA,
    W_ARC,
    W_CRYO,
    W_SAW,
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
    { "RAILGUN",185.0f, 1.32f,   4,  24, 0.0008f,2.9f, 1, Color( 55,195,255) },
    { "PLASMA",  56.0f, 0.19f,  20, 120, 0.013f, 2.1f, 1, Color( 45,240,155) },
    // Тяжёлые стволы: ARC — редкий crowd-control, CRYO — контроль темпа,
    // ION SAW — ближняя высокая скорострельность с падением урона на дистанции.
    { "ARC CANNON",200.0f, 1.75f,  3,  18, 0.004f,3.6f, 1, Color(255,190, 45) },
    { "CRYO LANCE", 72.0f, 0.62f,  8,  48, 0.010f,2.8f, 1, Color( 80,215,255) },
    { "ION SAW",    86.0f, 0.30f,  24, 144, 0.022f,2.4f, 1, Color(255,105, 35) },
};

struct SkinDef {
    const char* name;
    Color color;
    bool  emissive;
    float roughness;
    float metallic;
    Color accent;
};

static const SkinDef SKIN_DEFS[] = {
    { "GUNMETAL",     Color(135,142,156), false, 0.28f, 0.88f, Color( 70,190,255) },
    { "GOLD 24K",     Color(255,198, 62), false, 0.14f, 0.98f, Color(255,240,170) },
    { "CYBER NEON",   Color( 35,245,165), true,  0.22f, 0.72f, Color( 40,255,190) },
    { "CRIMSON",      Color(210, 28, 42), false, 0.16f, 0.90f, Color(255, 90, 70) },
    { "ARCTIC",       Color(225,238,252), false, 0.30f, 0.20f, Color( 90,205,255) },
    { "OCEAN PLASMA", Color( 38,135,255), true,  0.18f, 0.84f, Color( 65,220,255) },
    { "VOID QUANTUM", Color(128, 42,215), true,  0.20f, 0.78f, Color(235, 75,255) },
    { "TOXIC BIO",    Color(155,255, 38), true,  0.24f, 0.68f, Color(195,255, 65) },
    { "OBSIDIAN 4K",  Color( 28, 30, 38), true,  0.10f, 0.82f, Color(255,195, 55) },
    { "SOLAR FLARE",  Color(255,108, 24), true,  0.19f, 0.86f, Color(255,215, 60) },
};
constexpr int SKIN_COUNT = 10;

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
        , weaponType_(W_RIFLE)
        , reserveAmmo_(GameConstants::MAX_RESERVE_AMMO)
        , isReloading_(false)
        , reloadTimer_(0.0f)
        , fireTimer_(0.0f)
        , recoilX_(0.0f)
        , recoilY_(0.0f)
        , weaponBobPhase_(0.0f)
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
    float maxArmor_;
    int score_;
    int kills_;
    int headshots_;
    int shotsFired_;
    int shotsHit_;
    
    PlayerStats() 
        : health_(100.0f)
        , maxHealth_(100.0f)
        , armor_(50.0f)
        , maxArmor_(100.0f)
        , score_(0)
        , kills_(0)
        , headshots_(0)
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
    Node* weaponSpinNode_ = nullptr;       // барабан/диск: постоянное вращение и spin-up
    Node* weaponPulseNode_ = nullptr;      // энергетическое ядро: заряд/пульсация
    Node* weaponSlideNode_ = nullptr;      // затвор: короткий откат при выстреле
    Vector3 weaponPulseBaseScale_ = Vector3::ONE;
    Vector3 weaponSlideBasePosition_ = Vector3::ZERO;
    Quaternion weaponSlideBaseRotation_ = Quaternion::IDENTITY;
    float weaponAnimTime_ = 0.0f;
    Light* muzzleLight_ = nullptr;
    Node* muzzleLightNode_ = nullptr;
    
    // Компоненты
    // === Лёгкая физика: реестр коллайдеров уровня (см. namespace Lite) ===
    Vector<Lite::Collider> colliders_;
    int AddBoxCollider(const Vector3& c, const Vector3& half, int tag = Collision::STATIC, void* ptr = nullptr) {
        Lite::Collider col; col.center = c; col.half = half; col.sphere = false; col.tag = tag; col.ptr = ptr;
        colliders_.Push(col);
        return (int)colliders_.Size() - 1;
    }
    int AddSphereCollider(const Vector3& c, float r, int tag = Collision::STATIC, void* ptr = nullptr) {
        Lite::Collider col; col.center = c; col.half = Vector3(r, r, r); col.sphere = true; col.tag = tag; col.ptr = ptr;
        colliders_.Push(col);
        return (int)colliders_.Size() - 1;
    }
    int colliderGarbage_ = 0;   // сколько «удалённых» записей ждёт компакции

    void RemoveColliderLite(int idx) {
        if (idx >= 0 && idx < (int)colliders_.Size() && colliders_[(unsigned)idx].tag != Collision::REMOVED) {
            colliders_[(unsigned)idx].tag = Collision::REMOVED;
            ++colliderGarbage_;
        }
    }

    // Реестр коллайдеров раньше только рос: удалённые записи оставались в массиве
    // и перебирались в каждом рейкасте. Компакция пересобирает массив и чинит все индексы.
    void CompactColliders() {
        if (colliderGarbage_ <= 0) return;
        Vector<Lite::Collider> compacted;
        compacted.Reserve(colliders_.Size());
        Vector<int> remap;
        remap.Resize(colliders_.Size());
        for (unsigned i = 0; i < colliders_.Size(); ++i) {
            if (colliders_[i].tag == Collision::REMOVED) { remap[i] = -1; continue; }
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
        for (unsigned i = 0; i < barrels_.Size(); ++i) fix(barrels_[i].colliderIdx_);
        colliders_ = compacted;
        colliderGarbage_ = 0;
    }
    void MoveColliderLite(int idx, const Vector3& center) {
        if (idx >= 0 && idx < (int)colliders_.Size() && colliders_[(unsigned)idx].tag != Collision::REMOVED)
            colliders_[(unsigned)idx].center = center;
    }
    void ResizeColliderLite(int idx, const Vector3& half) {
        if (idx >= 0 && idx < (int)colliders_.Size() && colliders_[(unsigned)idx].tag != Collision::REMOVED)
            colliders_[(unsigned)idx].half = half;
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
        if ((tagMask & Collision::STATIC_MASK) && dir.y_ < -1e-6f) {
            float tFloor = (GameConstants::FLOOR_Y - origin.y_) / dir.y_;
            if (tFloor >= 0.0f && tFloor < best) {
                best = tFloor; bestTag = Collision::STATIC; bestPtr = nullptr; hit = true; bestFloor = true;
            }
        }

        for (unsigned i = 0; i < colliders_.Size(); ++i) {
            const Lite::Collider& c = colliders_[i];
            if (c.tag == Collision::REMOVED || !(tagMask & Collision::Mask(static_cast<Collision::Tag>(c.tag)))) continue; // удалён
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
            if (c.tag != Collision::STATIC) continue; // только статика: стены, колонны, ящики, платформы
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
            if (RaycastLite(test, dirs[i], radius + 0.1f, Collision::STATIC_MASK, dist, hitPoint) && dist < radius + 0.1f)
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
    SharedPtr<Material> railBeamMat_;
    SharedPtr<Material> plasmaOrbMat_;
    SharedPtr<Material> turretLaserMat_;
    SharedPtr<Material> enemyShotMat_;
    // Кэш материалов глаз роботов и «железа» оружия: раньше новые Material создавались
    // на каждого заспавненного врага и на каждое переключение оружия.
    SharedPtr<Material> eyeMatMelee_, eyeMatCharger_, eyeMatShooter_, eyeMatBoss_, eyeMatKamikaze_, eyeMatDrone_;
    SharedPtr<Material> weaponDarkMat_, weaponAccentMat_;
    SharedPtr<Material> arcCoreMat_, cryoCoreMat_, sawGlowMat_;
    
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

    // ===== Главное меню / 4K настройки / арена с волнами =====
    bool inMenu_ = true;            // игра стартует в главном меню (после 4K загрузочного экрана)
    int menuSel_ = 0;               // 0 тир, 1 арена, 2 баня, 3 рулетка, 4 настройки, 5 выход
    bool inSettings_ = false;
    int settingsSel_ = 0;
    static constexpr int SETTINGS_ROW_COUNT = 14;
    int gfxQualityIdx_ = 3;         // 0=НИЗКИЙ 1=СРЕДНИЙ 2=ВЫСОКИЙ 3=4K УЛЬТРА
    int gfxResIdx_ = 0;             // 0=АВТО(МОНИТОР) 1=3840x2160(4K) 2=2560x1440(2K) 3=1920x1080 4=1280x720
    int gfxDisplayMode_ = 1;        // 0=ОКНО 1=БЕЗ РАМКИ 2=ПОЛНЫЙ ЭКРАН
    int gfxMsaaIdx_ = 2;            // 0=ВЫКЛ 1=2x 2=4x 3=8x
    int gfxShadowIdx_ = 2;          // 0=ВЫКЛ 1=2K PCF 2=4K ULTRA 3=8K EXTREME
    int gfxGlowIdx_ = 2;            // 0=ВЫКЛ 1=МЯГКИЙ 2=КИНО HDR
    bool gfxSsao_ = true;           // SSAO4K экранное затенение
    bool gfxHdr_ = true;            // ACES Filmic HDR тонмаппинг
    bool gfxFxaa_ = true;           // FXAA 3.11 Ultra (Preset 39)
    bool gfxSharpen_ = true;        // Contrast-Adaptive 4K Sharpening (CAS)
    bool gfxVsync_ = false;         // вертикальная синхронизация
    int gfxFpsIdx_ = 2;             // 0=60 FPS 1=120 FPS 2=БЕЗ ЛИМИТА
    int gfxAnisoIdx_ = 2;           // 0=Билинейная 1=Трилинейная 2=Анизотропная 16x
    float fovBase_ = 90.0f;         // базовый горизонтально-комфортный FOV 4K (75..115)
    float uiScale_ = 1.0f;          // авто-масштаб интерфейса под 1080p / 1440p / 4K UHD (3840x2160 -> 2.0x)
    int S(float px) const { return Max(1, (int)(px * uiScale_ + 0.5f)); }
    int SF(int pt) const  { return Max(9, (int)(pt * uiScale_ + 0.5f)); }
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
    // Арена: волны роботов-курят (6 пород в 2.2: + Камикадзе с C4 и Реактивная Валькирия-Дрон!)
    enum EnemyType { E_MELEE = 0, E_CHARGER, E_SHOOTER, E_BOSS, E_KAMIKAZE, E_DRONE, E_TYPE_COUNT };
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
        float cryoSlowTimer_ = 0.0f;    // короткое замедление от CRYO LANCE
        float cryoSlowMul_ = 1.0f;
        // === Куриная анимация (ходьба, клевки, взмахи) ===
        WeakPtr<Node> headNode_, wingLNode_, wingRNode_, legLNode_, legRNode_, tailNode_;
        float animPhase_ = 0.0f;
        float cluckTimer_ = 5.0f;        // случайное кудахтанье в простое
        bool  enraged_ = false;          // фаза ярости Мега-Индюка (<50% HP)
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

    // === Пикапы (аптечки, патроны, титановая броня, кибер-овердрайв) ===
    struct PickupData {
        SharedPtr<Node> node_;
        Vector3 home_ = Vector3::ZERO;
        int type_ = 0;             // 0 = аптечка, 1 = патроны, 2 = броня +50, 3 = овердрайв x2
        float respawn_ = 0.0f;     // >0 — подобран, ждёт респавна
    };
    Vector<PickupData> pickups_;
    SharedPtr<Material> pickupHealMat_;
    SharedPtr<Material> pickupAmmoMat_;
    SharedPtr<Material> pickupArmorMat_;
    SharedPtr<Material> pickupOverdriveMat_;

    // === Взрывоопасные бочки (Hazard Barrels, Collision::BARREL) ===
    struct ExplosiveBarrel {
        SharedPtr<Node> node_;
        SharedPtr<Node> beacon_;
        Vector3 home_ = Vector3::ZERO;
        int colliderIdx_ = -1;
        float hp_ = 45.0f;
        bool exploded_ = false;
        float respawn_ = 0.0f;
    };
    Vector<ExplosiveBarrel> barrels_;

    // === Всплывающие 4K цифры урона и КРИТ ХЭДШОТОВ (WorldToScreenPoint) ===
    struct FloatingDmg {
        SharedPtr<Text> label_;
        Vector3 worldPos_;
        Vector3 vel_;
        float life_ = 0.85f;
        float maxLife_ = 0.85f;
        bool crit_ = false;
    };
    Vector<FloatingDmg> floatingDmgs_;

    // === Новые способности игрока: тактический рывок (Dash), удар крылом (Melee), Овердрайв, Лазер, Осмотр ===
    float dashCooldown_ = 0.0f;
    float dashTimer_ = 0.0f;
    Vector3 dashDir_ = Vector3::ZERO;
    float meleeCooldown_ = 0.0f;
    float meleeAnimTimer_ = 0.0f;
    float overdriveTimer_ = 0.0f;
    float inspectTimer_ = 0.0f;
    bool laserOn_ = true;
    SharedPtr<Node> laserDotNode_;
    SharedPtr<Material> laserDotMat_;
    bool photoMode_ = false;
    bool showTelemetry_ = false;
    SharedPtr<Text> telemetryText_;
    SharedPtr<BorderImage> telemetryBg_;

    // === Тактический 4K радар / миникарта (правый верх под счётом или левый верх) ===
    SharedPtr<BorderImage> radarBg_;
    SharedPtr<BorderImage> radarSweep_;
    Vector<SharedPtr<BorderImage>> radarBlips_;
    SharedPtr<BorderImage> hudArmorBarBg_, hudArmorBarFill_;
    SharedPtr<BorderImage> hudDashBarBg_, hudDashBarFill_;
    SharedPtr<Text> statusBuffText_;

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
    SharedPtr<Material> chickWhite_, chickBrown_, chickBlack_, chickBoss_, chickKamikaze_, chickDrone_;
    SharedPtr<Material> combMat_, beakMat_, legMat_, eggMat_, featherMat_;

    // === 2.2 CYBER OVERLOAD: Джамп-пады, Плазмомет, Кластерные гранаты [G], Авто-турели [T], Перки [B], Погода [F8], Достижения ===
    struct JumpPadData {
        SharedPtr<Node> node_;
        SharedPtr<Node> ringNode_;
        Vector3 pos_ = Vector3::ZERO;
        float phase_ = 0.0f;
    };
    Vector<JumpPadData> jumpPads_;
    float jumpPadCooldown_ = 0.0f;
    bool airborneFromPad_ = false;

    struct PlasmaBoltData {
        SharedPtr<Node> node_;
        Vector3 vel_ = Vector3::ZERO;
        float damage_ = 56.0f;
        float ttl_ = 2.6f;
        int bouncesLeft_ = 1;
    };
    Vector<PlasmaBoltData> playerPlasmaBolts_;

    struct ThrownGrenade {
        SharedPtr<Node> node_;
        Vector3 pos_ = Vector3::ZERO;
        Vector3 vel_ = Vector3::ZERO;
        float fuse_ = 1.45f;
    };
    Vector<ThrownGrenade> thrownGrenades_;
    float grenadeCooldown_ = 0.0f;

    struct TurretData {
        SharedPtr<Node> node_;
        WeakPtr<Node> headNode_;
        Vector3 pos_ = Vector3::ZERO;
        float ttl_ = 22.0f;
        float fireTimer_ = 0.25f;
    };
    Vector<TurretData> turrets_;
    float turretCooldown_ = 0.0f;

    // Кибер-кредиты и перки прокачки в забеге [B]
    int credits_ = 0;
    int perkHpLvl_  = 0;
    int perkRofLvl_ = 0;
    int perkDmgLvl_ = 0;
    int perkSpdLvl_ = 0;
    bool perkShopOpen_ = false;
    SharedPtr<UIElement> perkShopPanel_;
    SharedPtr<Text> perkShopBody_;

    // Серии быстрых убийств (Multi-Kill) и кинематографический Bullet-Time
    int multiKillCount_ = 0;
    float multiKillTimer_ = 0.0f;
    float slowMoTimer_ = 0.0f;
    int barrelsDetonatedRun_ = 0;

    // 4 атмосферных пресета освещения/тумана арены [F8]
    int atmosphereIdx_ = 0;

    // === 2.3 ULTRA CYBER EVOLUTION: Грэппл-крюк [Z], Орбитальный удар [X], Мутаторы волн, Погода 3D, Музыка [M] ===
    bool grappleActive_ = false;
    float grappleCooldown_ = 0.0f;
    float grappleTime_ = 0.0f;
    Vector3 grappleAnchor_ = Vector3::ZERO;
    SharedPtr<Node> grappleBeamNode_;

    float orbitalCharge_ = 0.0f;
    struct OrbitalStrikeFX {
        SharedPtr<Node> beamNode_;
        SharedPtr<Node> ringNode_;
        Vector3 pos_ = Vector3::ZERO;
        float ttl_ = 1.15f;
        float maxTtl_ = 1.15f;
    };
    Vector<OrbitalStrikeFX> orbitalStrikes_;

    int activeMutator_ = Cyber::MUT_STANDARD;
    int musicTrackIdx_ = 0;

    struct WeatherMote {
        SharedPtr<Node> node_;
        Vector3 offset_ = Vector3::ZERO;
        float speed_ = 3.0f;
        float phase_ = 0.0f;
    };
    Vector<WeatherMote> weatherMotes_;
    int weatherAtmBuilt_ = -1;

    // 14 постоянных достижений (битовая маска в settings.ini, вкл. 2 банных)
    int achievementsMask_ = 0;
    SharedPtr<BorderImage> achievToastBg_;
    SharedPtr<Text> achievToastText_;
    float achievToastTimer_ = 0.0f;

    // === 2.4 СИМУЛЯТОР ПАРНОЙ «КОЧЕТ В БАНЬКЕ» (src/BanyaSim.h: 14 концовок, 57 загадок, баффы Арены) ===
    Banya::Simulator banyaSim_{0xC0C4E7u};
    bool  banyaOpen_          = false;
    bool  banyaFromMenu_      = false;
    bool  banyaRewardApplied_ = false;
    int   banyaBuffTier_      = 0;     // 0=нет, 1=Банный закал (+25 HP/Armor), 2=Ковш Смотрящего (+50 HP/Armor, +15% DMG, +35% Орбита)
    int   banyaBonusCredits_  = 0;     // заработанные в бане кредиты (переносятся в бой)
    float banyaVenikAnim_     = 0.0f;
    float banyaCaptionTimer_  = 0.0f;
    float banyaLastWarnTime_  = -12.0f;
    float banyaSteamAcc_      = 0.0f;
    Vector3 banyaDoorPos_{-28.0f, 0.0f, -30.2f};
    float banyaPromptCooldown_ = 0.0f;

    struct BanyaUIParticle {
        SharedPtr<BorderImage> el_;
        float x_ = 0.0f, y_ = 0.0f, vx_ = 0.0f, vy_ = 0.0f;
        float life_ = 0.0f, ttl_ = 1.5f, sz_ = 6.0f;
        int   kind_ = 0; // 0=steam, 1=smoke, 2=drop, 3=leaf, 4=snow, 5=ember, 6=spark
    };
    Vector<BanyaUIParticle> banyaParticles_;

    SharedPtr<UIElement>   banyaPanel_, banyaSceneBox_, banyaRiddleBox_, banyaEndBox_, banyaStatsBox_;
    SharedPtr<BorderImage> banyaHeatWash_, banyaAirWash_, banyaColdWash_, banyaEndTint_;
    SharedPtr<BorderImage> banyaStoveFire_, banyaStonesGlow_, banyaVenikVis_, banyaRoosterBody_, banyaRoosterComb_, banyaRoosterCrown_;
    SharedPtr<Text>        banyaRoosterEyesText_;
    SharedPtr<Text>        banyaTempText_, banyaMoodText_, banyaClockText_, banyaCaptionText_, banyaBadgeText_;
    SharedPtr<BorderImage> banyaBarSteam_, banyaBarFire_, banyaBarAir_, banyaBarLife_, banyaBarTime_;
    SharedPtr<Text>        banyaRiddleQ_, banyaRiddleTime_, banyaRiddleFb_;
    SharedPtr<BorderImage> banyaRiddleTimerBar_;
    SharedPtr<BorderImage> banyaRiddleOptBg_[4];
    SharedPtr<Text>        banyaRiddleOptTxt_[4];
    SharedPtr<BorderImage> banyaActBtnBg_[5];
    SharedPtr<BorderImage> banyaStatsBtnBg_;
    SharedPtr<Text>        banyaEndTitle_, banyaEndText_, banyaEndHint_, banyaEndStats_;
    SharedPtr<BorderImage> banyaEndBtnRestart_, banyaEndBtnStats_, banyaEndBtnBack_;
    SharedPtr<Text>        banyaStatsRunText_, banyaStatsAllText_;
    SharedPtr<BorderImage> banyaStatsCloseBtn_;

    // === Нативная «Золотая рулетка» (только виртуальные фишки, без реальных денег) ===
    bool casinoOpen_ = false;
    Casino::Bet casinoBet_{};
    int casinoBalance_ = 1000;
    int casinoLastResult_ = -1;
    int casinoPendingResult_ = -1;
    int casinoWins_ = 0;
    int casinoLosses_ = 0;
    float casinoSpinTimer_ = 0.0f;
    SharedPtr<UIElement> casinoPanel_;
    SharedPtr<BorderImage> casinoWheelCard_, casinoSpinButton_, casinoBetButton_, casinoChipButton_, casinoNumberButton_, casinoTopupButton_, casinoCloseButton_;
    SharedPtr<Text> casinoTitleText_, casinoBalanceText_, casinoWheelText_, casinoBetText_, casinoResultText_, casinoStatsText_, casinoHelpText_;

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
    Material* hdFeather_ = nullptr;
    Material* hdHazard_ = nullptr;
    SharedPtr<Material> pbrBaseMat_;
    SharedPtr<Material> glowBaseMat_;
    SharedPtr<Material> critTracerMat_;
    Material* skyMaterial_ = nullptr;
    Material* fallbackMat_ = nullptr; // гарантия непустого материала, если ни один xml не найден
    Texture* skyTexture_ = nullptr;
    SharedPtr<TextureCube> skyCube_;
    SharedPtr<RenderPath> renderPath_;
    Light* sunLight_ = nullptr;
    Light* moonLight_ = nullptr;
    Light* spotLight_ = nullptr;
    Vector<Light*> floodLights_;
    Zone* sceneZone_ = nullptr;
    Vector<SharedPtr<Material>> weaponTintMats_;
    ParticleEffect* sparkEffect_ = nullptr;
    ParticleEffect* smokeEffect_ = nullptr;
    bool haveParticles_ = false; // .pfx или безопасный процедурный ParticleEffect доступны
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
    Sound* uiHoverSound_ = nullptr;
    Sound* dashSound_ = nullptr;
    Sound* headshotSound_ = nullptr;
    Sound* armorPickupSound_ = nullptr;
    Sound* overdriveSound_ = nullptr;
    Sound* wingSlapSound_ = nullptr;
    Sound* barrelBoomSound_ = nullptr;
    Sound* comboSound_ = nullptr;
    Sound* jumpPadSound_ = nullptr;
    Sound* turretDeploySound_ = nullptr;
    Sound* turretShotSound_ = nullptr;
    Sound* perkBuySound_ = nullptr;
    Sound* achievementSound_ = nullptr;
    Sound* kamikazeBeepSound_ = nullptr;
    Sound* grappleSound_ = nullptr;
    Sound* orbitalSound_ = nullptr;
    Sound* banyaSteamSound_ = nullptr;
    Sound* banyaVenikSound_ = nullptr;
    Sound* banyaSplashSound_ = nullptr;
    Sound* banyaDoorSound_ = nullptr;
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

    // === Полноценный многокадровый 4K ЗАГРУЗОЧНЫЙ ЭКРАН + экран высадки на миссию ===
    bool loadingScreenActive_ = false;
    bool loadingFadingOut_ = false;
    float loadingFadeAlpha_ = 1.0f;
    float loadProgress_ = 0.0f;
    float loadTargetProgress_ = 0.0f;
    float bootStageTimer_ = 0.0f;
    int loadStepIndex_ = 0;
    int loadingTipIdx_ = 0;
    float loadingTipTimer_ = 0.0f;
    SharedPtr<UIElement> loadingPanel_;
    SharedPtr<BorderImage> loadingBgImg_;
    SharedPtr<BorderImage> loadingCurtain_;
    SharedPtr<Text> loadingTitleText_;
    SharedPtr<Text> loadingSubText_;
    SharedPtr<Text> loadingStageText_;
    SharedPtr<Text> loadingPctText_;
    SharedPtr<Text> loadingTipText_;
    SharedPtr<Text> loadingSpecText_;
    SharedPtr<BorderImage> loadingBarTrack_;
    SharedPtr<BorderImage> loadingBarFill_;
    SharedPtr<BorderImage> loadingBarShimmer_;
    Vector<SharedPtr<BorderImage>> loadingEmblemParts_;
    Vector<SharedPtr<Text>> loadingCheckRows_;
    Vector<String> loadSteps_;

    // === 2.5.5 SPLASH INTRO: koch.studios → powered by fall.os → CHICHKENCUM 2.1 ===
    bool splashActive_ = false;
    int splashStage_ = 0;
    float splashTimer_ = 0.0f;
    SharedPtr<UIElement> splashPanel_;
    SharedPtr<Text> splashText_;
    SharedPtr<BorderImage> splashLine_;
    SharedPtr<Scene> splashScene_;
    SharedPtr<Node> kochMusicNode_;
    SoundSource* kochThemeSrc_ = nullptr;   // компонент временной сцены — владеется нодой
    SoundSource* splashSfxSrc_ = nullptr;
    SharedPtr<BorderImage> splashGlow_;
    SharedPtr<BorderImage> splashSheen_;
    String splashFullText_;              // полный текст стадии (посимвольное открытие)

    // Быстрый загрузочный оверлей десантирования при входе в режим (АРЕНА / ТИР)
    bool deployActive_ = false;
    float deployTimer_ = 0.0f;
    float deployDuration_ = 1.05f;
    GameMode pendingDeployMode_ = MODE_ARENA;
    bool deployExecuted_ = false;
    SharedPtr<UIElement> deployPanel_;
    SharedPtr<BorderImage> deployBarFill_;
    SharedPtr<Text> deployTitleText_;
    SharedPtr<Text> deployTipText_;

public:
    MyApp(Context* context) : Application(context) {}

    // Ограничители временных FX. Взрывная волна или длинная очередь могут
    // породить сотни узлов за секунду; старые эффекты удаляем сразу, чтобы
    // сцена и SharedPtr-реестры не росли быстрее TTL.
    static constexpr unsigned MAX_PENDING_REMOVES = 512;
    static constexpr unsigned MAX_TRACERS = 256;
    static constexpr unsigned MAX_SHOCKWAVES = 96;
    static constexpr unsigned MAX_DEBRIS = 384;
    static constexpr unsigned MAX_ENEMY_PROJECTILES = 128;
    static constexpr unsigned MAX_PLASMA_BOLTS = 32;
    static constexpr unsigned MAX_THROWN_GRENADES = 8;
    static constexpr unsigned MAX_TURRETS = 8;
    static constexpr unsigned MAX_ORBITAL_STRIKES = 8;

    void QueuePendingRemove(Node* node, float ttl) {
        if (!node) return;
        if (pendingRemoves_.Size() >= MAX_PENDING_REMOVES) {
            if (pendingRemoves_[0].node_) pendingRemoves_[0].node_->Remove();
            pendingRemoves_.Erase(0);
        }
        PendingRemove pr;
        pr.node_ = node;
        pr.ttl_ = Max(0.02f, ttl);
        pendingRemoves_.Push(pr);
    }

    void AddTracer(Node* node, float ttl) {
        if (!node) return;
        if (tracers_.Size() >= MAX_TRACERS) {
            if (tracers_[0].node_) tracers_[0].node_->Remove();
            tracers_.Erase(0);
        }
        TracerData tr;
        tr.node_ = node;
        tr.ttl_ = Max(0.01f, ttl);
        tracers_.Push(tr);
    }

    void AddShockwave(const ShockwaveData& wave) {
        if (!wave.node_) return;
        if (shockwaves_.Size() >= MAX_SHOCKWAVES) {
            if (shockwaves_[0].node_) shockwaves_[0].node_->Remove();
            shockwaves_.Erase(0);
        }
        shockwaves_.Push(wave);
    }

    void AddDebris(const DebrisData& debris) {
        if (!debris.node_) return;
        if (debrisLite_.Size() >= MAX_DEBRIS) {
            if (debrisLite_[0].node_) debrisLite_[0].node_->Remove();
            debrisLite_.Erase(0);
        }
        debrisLite_.Push(debris);
    }

    // Удаляет все FX, которые относятся к предыдущему забегу. Это важно при
    // частом переключении «тир/арена»: TTL-очередь сама по себе ограничена,
    // но не должна держать старые SharedPtr до их естественного истечения.
    void ClearTransientEffects() {
        for (unsigned i = 0; i < pendingRemoves_.Size(); ++i)
            if (pendingRemoves_[i].node_) pendingRemoves_[i].node_->Remove();
        pendingRemoves_.Clear();

        for (unsigned i = 0; i < tracers_.Size(); ++i)
            if (tracers_[i].node_) tracers_[i].node_->Remove();
        tracers_.Clear();

        for (unsigned i = 0; i < shockwaves_.Size(); ++i)
            if (shockwaves_[i].node_) shockwaves_[i].node_->Remove();
        shockwaves_.Clear();

        for (unsigned i = 0; i < debrisLite_.Size(); ++i)
            if (debrisLite_[i].node_) debrisLite_[i].node_->Remove();
        debrisLite_.Clear();

        for (unsigned i = 0; i < decals_.Size(); ++i)
            if (decals_[i]) decals_[i]->Remove();
        decals_.Clear();

        for (unsigned i = 0; i < floatingDmgs_.Size(); ++i)
            if (floatingDmgs_[i].label_) floatingDmgs_[i].label_->Remove();
        floatingDmgs_.Clear();
    }

    // После обновления сцены удаляем записи, чьи узлы были сняты другими
    // системами в этом же кадре. Так SharedPtr/WeakPtr-реестры не копят
    // «призраков», а компакция Lite-коллайдеров не попадает в середину рейкаста.
    void PruneTransientEffectsAfterSceneUpdate() {
        for (unsigned i = pendingRemoves_.Size(); i-- > 0; ) {
            Node* n = pendingRemoves_[i].node_.Get();
            if (!n || !n->GetScene()) pendingRemoves_.Erase(i);
        }
        for (unsigned i = tracers_.Size(); i-- > 0; ) {
            Node* n = tracers_[i].node_.Get();
            if (!n || !n->GetScene()) tracers_.Erase(i);
        }
        for (unsigned i = shockwaves_.Size(); i-- > 0; ) {
            Node* n = shockwaves_[i].node_.Get();
            if (!n || !n->GetScene()) shockwaves_.Erase(i);
        }
        for (unsigned i = debrisLite_.Size(); i-- > 0; ) {
            Node* n = debrisLite_[i].node_.Get();
            if (!n || !n->GetScene()) debrisLite_.Erase(i);
        }
    }

    // =========================================================================
    // Setup() — вызывается ДО создания окна OpenGL: выставляет 4K UHD / монитор,
    // MSAA, HighDPI, TripleBuffer, Anisotropy 16x и максимальное качество PBR!
    // =========================================================================
    void Setup() override {
        LoadSettingsFile();

        if (!engineParameters_.Contains(EP_RESOURCE_PREFIX_PATHS))
            engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ".;..;bin;../bin";

        engineParameters_[EP_WINDOW_TITLE] = "CHIKENGUN 2.5 — ПЕРО СУДЬБЫ (4K ULTRA)";
        engineParameters_[EP_HIGH_DPI] = true;
        engineParameters_[EP_WINDOW_RESIZABLE] = true;
        engineParameters_[EP_TRIPLE_BUFFER] = true;
        engineParameters_[EP_VSYNC] = gfxVsync_;
        engineParameters_[EP_SOUND] = true;

        static const int kMsaaTable[4] = { 1, 2, 4, 8 };
        static const int kAnisoTable[3] = { 1, 4, 16 };
        engineParameters_[EP_MULTI_SAMPLE] = kMsaaTable[Clamp(gfxMsaaIdx_, 0, 3)];
        engineParameters_[EP_TEXTURE_ANISOTROPY] = kAnisoTable[Clamp(gfxAnisoIdx_, 0, 2)];
        engineParameters_[EP_TEXTURE_FILTER_MODE] = (gfxAnisoIdx_ >= 2) ? FILTER_ANISOTROPIC
                                                  : (gfxAnisoIdx_ == 1 ? FILTER_TRILINEAR : FILTER_BILINEAR);
        engineParameters_[EP_TEXTURE_QUALITY] = QUALITY_MAX;
        engineParameters_[EP_MATERIAL_QUALITY] = (gfxQualityIdx_ >= 2) ? QUALITY_HIGH
                                               : (gfxQualityIdx_ == 1 ? QUALITY_MEDIUM : QUALITY_LOW);

        static const int kResW[5] = { 0, 3840, 2560, 1920, 1600 };
        static const int kResH[5] = { 0, 2160, 1440, 1080,  900 };
        int rIdx = Clamp(gfxResIdx_, 0, 4);
        int w = kResW[rIdx];
        int h = kResH[rIdx];

        if (gfxDisplayMode_ == 2) {
            engineParameters_[EP_FULL_SCREEN] = true;
            engineParameters_[EP_BORDERLESS] = false;
            if (w > 0) { engineParameters_[EP_WINDOW_WIDTH] = w; engineParameters_[EP_WINDOW_HEIGHT] = h; }
        } else if (gfxDisplayMode_ == 1) {
            engineParameters_[EP_FULL_SCREEN] = false;
            engineParameters_[EP_BORDERLESS] = true;
            if (w > 0) { engineParameters_[EP_WINDOW_WIDTH] = w; engineParameters_[EP_WINDOW_HEIGHT] = h; }
        } else {
            engineParameters_[EP_FULL_SCREEN] = false;
            engineParameters_[EP_BORDERLESS] = false;
            engineParameters_[EP_WINDOW_WIDTH] = (w > 0) ? w : 1920;
            engineParameters_[EP_WINDOW_HEIGHT] = (h > 0) ? h : 1080;
        }
    }

    void RefreshUIScale() {
        auto* gfx = GetSubsystem<Graphics>();
        if (!gfx) { uiScale_ = 1.0f; return; }
        float sx = (float)Max(640, gfx->GetWidth()) / 1920.0f;
        float sy = (float)Max(480, gfx->GetHeight()) / 1080.0f;
        uiScale_ = Clamp(Min(sx, sy), 0.72f, 2.50f);
    }

    void StyleCrispText(Text* t, bool stroke = false) {
        if (!t) return;
        t->SetTextEffect(stroke ? TE_STROKE : TE_SHADOW);
        t->SetEffectColor(Color(0.0f, 0.01f, 0.03f, 0.88f));
        if (stroke) t->SetEffectStrokeThickness(Max(1, S(1.4f)));
        else t->SetEffectShadowOffset(IntVector2(Max(1, S(2.0f)), Max(1, S(2.0f))));
    }

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

            // 4) Дополнительно регистрируем bin/Data без вызова внешних shell-команд (system),
            //    чтобы ресурсы находились при запуске из корня, build/ или bin/.
            const char* extraDataDirs[] = { "./bin/Data", "../bin/Data" };
            for (const char* ed : extraDataDirs) {
                if (dirOk(ed)) {
                    cache->AddResourceDir(ed);
                    break;
                }
            }
            if (!exeDir.Empty() && dirOk(exeDir + "/../bin/Data"))
                cache->AddResourceDir(exeDir + "/../bin/Data");

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

        // Пользовательские настройки графики/звука (settings.ini) + 4K движок
        LoadSettingsFile();
        SetupEngineSettings();
        RefreshUIScale();

        // Подключаем обработчики событий СРАЗУ, чтобы многокадровый 4K загрузочный экран
        // рендерился на GPU в реальном времени при 60+ FPS, а не уничтожался до 1-го кадра!
        SubscribeToEvents();

        // Показываем полноценный 4K UHD загрузочный экран и запускаем поэтапную загрузку
        CreateLoadingScreen();
        loadingScreenActive_ = true;
        loadingFadingOut_ = false;
        loadingFadeAlpha_ = 1.0f;
        loadStepIndex_ = 0;
        loadProgress_ = 0.02f;
        loadTargetProgress_ = 0.12f;
        bootStageTimer_ = 0.0f;

        // 2.5.5: эпичная интро-заставка koch.studios → powered by fall.os → CHICHKENCUM 2.1
        // (рисует поверх загрузочного экрана, музыка перетекает в фазу загрузки)
        CreateSplashIntro();
    }

private:
    // =========================================================================
    // Инициализация 4K HDR PBR конвейера
    // =========================================================================

    static Color NormByteColor(const Color& c) {
        if (c.r_ > 1.5f || c.g_ > 1.5f || c.b_ > 1.5f)
            return Color(Clamp(c.r_ / 255.0f, 0.0f, 1.0f),
                         Clamp(c.g_ / 255.0f, 0.0f, 1.0f),
                         Clamp(c.b_ / 255.0f, 0.0f, 1.0f), 1.0f);
        return Color(Clamp(c.r_, 0.0f, 1.0f), Clamp(c.g_, 0.0f, 1.0f), Clamp(c.b_, 0.0f, 1.0f), c.a_);
    }

    void SetupEngineSettings() {
        auto* graphics = GetSubsystem<Graphics>();
        auto* renderer = GetSubsystem<Renderer>();
        if (!graphics || !renderer) return;

        // ВАЖНО: отключаем аппаратный GL_FRAMEBUFFER_SRGB на бэкбафере!
        // Раньше SetSRGB(true) высветлял все тёмные плашки UI в «молоко» и давал белый экран в HDR.
        // Теперь тональная компрессия ACES Filmic и цветокоррекция выполняются шейдером Cinema4K.glsl!
        graphics->SetSRGB(false);
        graphics->SetDefaultTextureAnisotropy((gfxAnisoIdx_ >= 3) ? 16 : (gfxAnisoIdx_ == 2 ? 8 : 2));
        graphics->SetDefaultTextureFilterMode((gfxAnisoIdx_ >= 2) ? FILTER_ANISOTROPIC
                                            : (gfxAnisoIdx_ == 1 ? FILTER_TRILINEAR : FILTER_BILINEAR));

        renderer->SetTextureQuality(QUALITY_MAX);
        renderer->SetMaterialQuality((gfxQualityIdx_ >= 2) ? QUALITY_MAX
                                   : (gfxQualityIdx_ == 1 ? QUALITY_MEDIUM : QUALITY_LOW));
        renderer->SetSpecularLighting(true);
        renderer->SetDynamicInstancing(true);
        renderer->SetDrawShadows(gfxShadowIdx_ > 0);
        renderer->SetShadowMapSize(gfxShadowIdx_ >= 3 ? 8192 : (gfxShadowIdx_ == 2 ? 4096 : 2048));
        renderer->SetShadowQuality(SHADOWQUALITY_PCF_24BIT);
        renderer->SetShadowSoftness(1.25f);
        renderer->SetHDRRendering(gfxHdr_ || gfxGlowIdx_ >= 1);
    }

    void LoadCoreModelsAndFont() {
        auto* cache = GetSubsystem<ResourceCache>();
        boxModel_    = cache->GetResource<Model>("Models/Box.mdl");
        sphereModel_ = cache->GetResource<Model>("Models/Sphere.mdl");
        coneModel_   = cache->GetResource<Model>("Models/Cone.mdl");
        cylModel_    = cache->GetResource<Model>("Models/Cylinder.mdl");
        planeModel_  = cache->GetResource<Model>("Models/Plane.mdl");
        if (!font_) {
            font_ = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
            if (!font_) font_ = cache->GetResource<Font>("Fonts/BlueHighway.ttf");
            if (!font_) font_ = cache->GetResource<Font>("Fonts/DejaVuSans.ttf");
        }
    }

    void LoadAudioAssets() {
        auto* cache = GetSubsystem<ResourceCache>();
        static const char* shotFiles[WEAPON_COUNT] = {
            "Sounds/Shot_Pistol.wav", "Sounds/Shot_SMG.wav", "Sounds/Shot_Rifle.wav",
            "Sounds/Shot_Shotgun.wav", "Sounds/Shot_Sniper.wav", "Sounds/Shot_Minigun.wav",
            "Sounds/Shot_Grenade.wav", "Sounds/Shot_Railgun.wav", "Sounds/Shot_Plasma.wav",
            "Sounds/Shot_Railgun.wav", "Sounds/Shot_Plasma.wav", "Sounds/Shot_Shotgun.wav"
        };
        for (int i2 = 0; i2 < WEAPON_COUNT; ++i2) {
            shootSounds_[i2] = cache->GetResource<Sound>(shotFiles[i2]);
            if (shootSounds_[i2]) shootSounds_[i2]->SetLooped(false);
        }

        reloadSound_      = cache->GetResource<Sound>("Sounds/Reload.wav");
        emptyClickSound_  = cache->GetResource<Sound>("Sounds/EmptyClick.wav");
        hitSound_         = cache->GetResource<Sound>("Sounds/HitMarker.wav");
        destroySound_     = cache->GetResource<Sound>("Sounds/TargetDestroy.wav");
        explosionSound_   = cache->GetResource<Sound>("Sounds/Explosion.wav");
        jumpSound_        = cache->GetResource<Sound>("Sounds/Jump.wav");
        landSound_        = cache->GetResource<Sound>("Sounds/Land.wav");
        uiSelectSound_    = cache->GetResource<Sound>("Sounds/UI_Select.wav");
        uiHoverSound_     = cache->GetResource<Sound>("Sounds/UI_Hover.wav");
        dashSound_        = cache->GetResource<Sound>("Sounds/Dash.wav");
        headshotSound_    = cache->GetResource<Sound>("Sounds/Headshot.wav");
        armorPickupSound_ = cache->GetResource<Sound>("Sounds/ArmorPickup.wav");
        overdriveSound_   = cache->GetResource<Sound>("Sounds/Overdrive.wav");
        wingSlapSound_    = cache->GetResource<Sound>("Sounds/WingSlap.wav");
        barrelBoomSound_  = cache->GetResource<Sound>("Sounds/BarrelBoom.wav");
        comboSound_       = cache->GetResource<Sound>("Sounds/Combo.wav");
        jumpPadSound_     = cache->GetResource<Sound>("Sounds/JumpPad.wav");
        turretDeploySound_= cache->GetResource<Sound>("Sounds/TurretDeploy.wav");
        turretShotSound_  = cache->GetResource<Sound>("Sounds/TurretShot.wav");
        perkBuySound_     = cache->GetResource<Sound>("Sounds/PerkBuy.wav");
        achievementSound_ = cache->GetResource<Sound>("Sounds/Achievement.wav");
        kamikazeBeepSound_= cache->GetResource<Sound>("Sounds/KamikazeBeep.wav");
        grappleSound_     = cache->GetResource<Sound>("Sounds/Grapple.wav");
        orbitalSound_     = cache->GetResource<Sound>("Sounds/OrbitalStrike.wav");
        banyaSteamSound_  = cache->GetResource<Sound>("Sounds/BanyaSteam.wav");
        banyaVenikSound_  = cache->GetResource<Sound>("Sounds/BanyaVenik.wav");
        banyaSplashSound_ = cache->GetResource<Sound>("Sounds/BanyaSplash.wav");
        banyaDoorSound_   = cache->GetResource<Sound>("Sounds/BanyaDoor.wav");

        cluckSounds_[0] = cache->GetResource<Sound>("Sounds/Chicken_Cluck.wav");
        if (!cluckSounds_[0]) cluckSounds_[0] = cache->GetResource<Sound>("Sounds/ChickenCluck.wav");
        cluckSounds_[1] = cache->GetResource<Sound>("Sounds/Chicken_Cluck2.wav");
        if (!cluckSounds_[1]) cluckSounds_[1] = cluckSounds_[0];
        squawkSound_    = cache->GetResource<Sound>("Sounds/Chicken_Squawk.wav");
        if (!squawkSound_) squawkSound_ = cache->GetResource<Sound>("Sounds/ChickenDie.wav");
        eggSplatSound_  = cache->GetResource<Sound>("Sounds/Chicken_Egg.wav");
        bossRoarSound_  = cache->GetResource<Sound>("Sounds/Chicken_Boss.wav");
        if (!bossRoarSound_) bossRoarSound_ = cache->GetResource<Sound>("Sounds/RoosterBoss.wav");
        waveStartSound_ = cache->GetResource<Sound>("Sounds/Wave_Start.wav");
        if (!waveStartSound_) waveStartSound_ = cache->GetResource<Sound>("Sounds/WaveStart.wav");

        Sound* music = cache->GetResource<Sound>("Music/Game_Theme_Loop.ogg");
        if (music && cameraNode_ && !musicSource_ && !kochThemeSrc_) {   // интро-тема ещё играет — основная петля вступит после неё
            music->SetLooped(true);
            musicSource_ = cameraNode_->CreateComponent<SoundSource>();
            if (musicSource_) {
                musicSource_->SetGain(musicVolume_);
                musicSource_->Play(music);
            }
        }
    }

    void Stop() override {
        // Сохраняем прогресс даже при обычном закрытии окна, а не только
        // после GAME OVER/изменения меню. SaveSettingsFile() сам выбирает
        // пользовательскую директорию и atomic fallback.
        SaveSettingsFile();
        if (musicSource_) musicSource_->Stop();
        Application::Stop();
    }

    bool LoadResources() {
        LoadCoreModelsAndFont();
        CreateMaterials();
        CreateParticleEffects();
        LoadAudioAssets();
        return true;
    }

    // Хранилище и O(1)-кэш дедупликации процедурно клонированных материалов (фикс утечки при долгой игре)
    Vector<SharedPtr<Material>> tintedMatStore_;
    HashMap<unsigned, SharedPtr<Material>> matCache_;
    SharedPtr<Material> baseMat_;

    void CycleMusicTrack() {
        auto* cache = GetSubsystem<ResourceCache>();
        if (!cache || !cameraNode_) return;
        musicTrackIdx_ = (musicTrackIdx_ + 1) % 3;
        if (!musicSource_) musicSource_ = cameraNode_->CreateComponent<SoundSource>();
        if (!musicSource_) return;
        if (musicTrackIdx_ == 2) {
            musicSource_->Stop();
            ShowMessage("Музыка: ВЫКЛЮЧЕНА [M]");
            return;
        }
        const char* tracks[2] = { "Music/Game_Theme_Loop.ogg", "Music/Ninja Gods.ogg" };
        const char* names[2]  = { "CYBER ARENA LOOP", "NINJA GODS OVERDRIVE" };
        Sound* mus = cache->GetResource<Sound>(tracks[musicTrackIdx_]);
        if (mus) {
            mus->SetLooped(true);
            musicSource_->SetGain(musicVolume_);
            musicSource_->Play(mus);
            ShowMessage(String("Музыка [M]: ") + names[musicTrackIdx_]);
        }
    }

    SharedPtr<Material> MakeTinted(const SharedPtr<Texture>& diff, const SharedPtr<Texture>& norm, const Color& c) {
        Color nc = NormByteColor(c);
        uint64_t h = 1469598103934665603ULL;
        const void* dp = diff.Get(); const void* np = norm.Get();
        h = Lite::HashMixBytes(h, &dp, sizeof(dp));
        h = Lite::HashMixBytes(h, &np, sizeof(np));
        h = Lite::HashFloatQ(h, nc.r_); h = Lite::HashFloatQ(h, nc.g_);
        h = Lite::HashFloatQ(h, nc.b_); h = Lite::HashFloatQ(h, nc.a_);
        unsigned key = (unsigned)(h ^ (h >> 32)) ^ 0xA11CE001u;
        auto it = matCache_.Find(key);
        if (it != matCache_.End() && it->second_) return it->second_;

        auto* cache = GetSubsystem<ResourceCache>();
        if (!baseMat_) {
            static const char* kBaseMats[] = {
                "Materials/Game/GlowUnlit.xml", "Materials/Game/TintUnlit.xml",
                "Materials/VColUnlit.xml", "Materials/DefaultGrey.xml"
            };
            for (const char* cand : kBaseMats) {
                Material* t = cache->GetResource<Material>(String(cand));
                if (t && t->GetTechniques().Size()) { baseMat_ = t; break; }
            }
            if (!baseMat_) baseMat_ = new Material(context_);
        }
        SharedPtr<Material> m = baseMat_->Clone();
        if (diff) m->SetTexture(TU_DIFFUSE, diff);
        if (norm) m->SetTexture(TU_NORMAL, norm);
        m->SetShaderParameter("MatDiffColor", Variant(nc));
        tintedMatStore_.Push(m);
        matCache_[key] = m;
        return m;
    }

    // HDR Emissive Unlit — значения R/G/B > 1.0 дают настоящий физический ореол в BloomHDR!
    SharedPtr<Material> MakeHDRGlow(const Color& hdrLinearColor, float intensity = 1.0f) {
        Color scaled(hdrLinearColor.r_ * intensity, hdrLinearColor.g_ * intensity, hdrLinearColor.b_ * intensity, hdrLinearColor.a_);
        uint64_t h = 1469598103934665603ULL;
        h = Lite::HashFloatQ(h, scaled.r_); h = Lite::HashFloatQ(h, scaled.g_);
        h = Lite::HashFloatQ(h, scaled.b_); h = Lite::HashFloatQ(h, scaled.a_);
        unsigned key = (unsigned)(h ^ (h >> 32)) ^ 0xB22DF002u;
        auto it = matCache_.Find(key);
        if (it != matCache_.End() && it->second_) return it->second_;

        auto* cache = GetSubsystem<ResourceCache>();
        if (!glowBaseMat_) {
            Material* g = cache->GetResource<Material>("Materials/Game/GlowUnlit.xml");
            if (!g) g = cache->GetResource<Material>("Materials/Game/TintUnlit.xml");
            glowBaseMat_ = g ? SharedPtr<Material>(g) : MakeTinted(nullptr, nullptr, Color(255, 255, 255));
        }
        SharedPtr<Material> m = glowBaseMat_->Clone();
        m->SetShaderParameter("MatDiffColor", Variant(scaled));
        tintedMatStore_.Push(m);
        matCache_[key] = m;
        return m;
    }

    // Полноценный микрофасетный GGX PBR материал (металл, золото 24K, оружейная сталь, пластик, клювы)
    SharedPtr<Material> MakePBR(const Color& albedo, float roughness, float metallic,
                                const Color& emissive = Color::BLACK) {
        Color col = NormByteColor(albedo);
        uint64_t h = 1469598103934665603ULL;
        h = Lite::HashFloatQ(h, col.r_); h = Lite::HashFloatQ(h, col.g_); h = Lite::HashFloatQ(h, col.b_);
        h = Lite::HashFloatQ(h, roughness); h = Lite::HashFloatQ(h, metallic);
        h = Lite::HashFloatQ(h, emissive.r_); h = Lite::HashFloatQ(h, emissive.g_); h = Lite::HashFloatQ(h, emissive.b_);
        unsigned key = (unsigned)(h ^ (h >> 32)) ^ 0xC33EF003u;
        auto it = matCache_.Find(key);
        if (it != matCache_.End() && it->second_) return it->second_;

        auto* cache = GetSubsystem<ResourceCache>();
        if (!pbrBaseMat_) {
            Material* pm = cache->GetResource<Material>("Materials/Game/PBRSolid.xml");
            if (pm && pm->GetTechniques().Size()) pbrBaseMat_ = pm;
            else {
                pbrBaseMat_ = new Material(context_);
                Technique* tPbr = cache->GetResource<Technique>("Techniques/PBR/PBRNoTexture.xml");
                Technique* tLow = cache->GetResource<Technique>("Techniques/NoTexture.xml");
                if (tPbr) pbrBaseMat_->SetTechnique(0, tPbr, QUALITY_HIGH);
                if (tLow) pbrBaseMat_->SetTechnique(tPbr ? 1 : 0, tLow, QUALITY_LOW);
            }
        }
        SharedPtr<Material> m = pbrBaseMat_->Clone();
        float specPow = Lerp(16.0f, 160.0f, 1.0f - Clamp(roughness, 0.05f, 1.0f));
        float specStr = Lerp(0.35f, 0.95f, Clamp(metallic, 0.0f, 1.0f));
        m->SetShaderParameter("MatDiffColor", Variant(col));
        m->SetShaderParameter("MatSpecColor", Variant(Color(specStr, specStr, specStr, specPow)));
        m->SetShaderParameter("MatEmissiveColor", Variant(emissive));
        m->SetShaderParameter("Roughness", Variant(Clamp(roughness, 0.06f, 0.98f)));
        m->SetShaderParameter("Metallic", Variant(Clamp(metallic, 0.0f, 1.0f)));
        tintedMatStore_.Push(m);
        matCache_[key] = m;
        return m;
    }

    // Перьевой 4K PBR материал с рельефной картой бородок пера (FeatherPBRNormal) + тинтом породы
    SharedPtr<Material> MakeFeatheredPBR(const Color& tint, float extraRough = 0.0f) {
        if (hdFeather_) {
            SharedPtr<Material> m = hdFeather_->Clone();
            Color col = NormByteColor(tint);
            m->SetShaderParameter("MatDiffColor", Variant(col));
            m->SetShaderParameter("Roughness", Variant(extraRough));
            m->SetShaderParameter("Metallic", Variant(0.0f));
            m->SetShaderParameter("UOffset", Variant(Vector4(3.0f, 0, 0, 0)));
            m->SetShaderParameter("VOffset", Variant(Vector4(0, 3.0f, 0, 0)));
            tintedMatStore_.Push(m);
            return m;
        }
        return MakePBR(tint, Clamp(0.58f + extraRough, 0.15f, 0.95f), 0.02f);
    }

    // Загрузка или процедурный синтез бесшовного 6-гранного 360° Skybox + IBL Cubemap прямо в ОЗУ
    void EnsureSkyCubemap() {
        if (skyCube_) return;
        auto* cache = GetSubsystem<ResourceCache>();
        TextureCube* loaded = cache->GetResource<TextureCube>("Textures/Skybox4K.xml");
        if (loaded && loaded->GetWidth() > 0) {
            skyCube_ = SharedPtr<TextureCube>(loaded);
            skyCube_->SetFilterMode(FILTER_TRILINEAR);
            return;
        }

        // Автономный fallback в RAM: строим бесшовный 6-гранный 360° TextureCube с закатом и мегаполисом
        SharedPtr<TextureCube> cube(new TextureCube(context_));
        cube->SetName("Textures/ProceduralSkybox4K");
        cube->SetNumLevels(0); // автогенерация полной цепочки mip-уровней для IBL.glsl!
        cube->SetFilterMode(FILTER_TRILINEAR);
        const int CS = 256;
        const Vector3 sunDir = Vector3(0.58f, 0.50f, 0.52f).Normalized();
        for (int f = 0; f < MAX_CUBEMAP_FACES; ++f) {
            SharedPtr<Image> img(new Image(context_));
            img->SetSize(CS, CS, 4);
            for (int y = 0; y < CS; ++y) {
                float t = 2.0f * (y + 0.5f) / float(CS) - 1.0f;
                for (int x = 0; x < CS; ++x) {
                    float s = 2.0f * (x + 0.5f) / float(CS) - 1.0f;
                    Vector3 d;
                    switch (f) {
                        case FACE_POSITIVE_X: d = Vector3( 1.0f, -t, -s); break;
                        case FACE_NEGATIVE_X: d = Vector3(-1.0f, -t,  s); break;
                        case FACE_POSITIVE_Y: d = Vector3( s,  1.0f,  t); break;
                        case FACE_NEGATIVE_Y: d = Vector3( s, -1.0f, -t); break;
                        case FACE_POSITIVE_Z: d = Vector3( s, -t,  1.0f); break;
                        default:              d = Vector3(-s, -t, -1.0f); break;
                    }
                    d.Normalize();
                    float sunDot = Max(0.0f, d.DotProduct(sunDir));
                    float up = Clamp(d.y_, 0.0f, 1.0f);
                    float hor = expf(-fabsf(d.y_) * 5.0f);
                    float r = Lerp(0.28f, 0.035f, powf(up, 0.55f)) + powf(sunDot, 5.0f) * 0.65f + powf(sunDot, 80.0f);
                    float g = Lerp(0.20f, 0.055f, powf(up, 0.62f)) + powf(sunDot, 5.0f) * 0.35f + powf(sunDot, 80.0f) * 0.8f;
                    float b = Lerp(0.38f, 0.145f, powf(up, 0.48f)) + hor * 0.08f + powf(sunDot, 80.0f) * 0.5f;
                    if (d.y_ < 0.02f) { float k = Clamp(-d.y_ * 4.0f, 0.0f, 1.0f); r = Lerp(r, 0.03f, k); g = Lerp(g, 0.04f, k); b = Lerp(b, 0.07f, k); }
                    img->SetPixel(x, y, Color(Clamp(r, 0.0f, 1.0f), Clamp(g, 0.0f, 1.0f), Clamp(b, 0.0f, 1.0f), 1.0f));
                }
            }
            cube->SetData(static_cast<CubeMapFace>(f), img);
        }
        cache->AddManualResource(cube);
        skyCube_ = cube;
    }

    void CreateMaterials() {
        auto* cache = GetSubsystem<ResourceCache>();
        if (!fallbackMat_) {
            SharedPtr<Material> fb = MakePBR(Color(150, 155, 165), 0.5f, 0.1f);
            fallbackMat_ = fb.Get();
        }

        // --- 4K PBR материалы поверхности арены ---
        // Material.xml can still be returned even when one of its image
        // resources failed to decode.  Do not keep such a half-loaded material:
        // older Urho3D builds may dereference its missing texture during the
        // first render pass instead of failing gracefully.
        auto textureSetOk = [cache](const char* diffuse, const char* normal, const char* roughMetal) {
            const char* names[] = { diffuse, normal, roughMetal };
            for (const char* name : names) {
                if (!cache->GetResource<Texture2D>(String(name))) return false;
            }
            return true;
        };
        auto loadPbrMaterial = [&](const char* materialName, const char* diffuse,
                                   const char* normal, const char* roughMetal) -> Material* {
            Material* material = cache->GetResource<Material>(String(materialName));
            if (!material || !textureSetOk(diffuse, normal, roughMetal)) {
                URHO3D_LOGWARNING(String("PBR material disabled (missing/invalid texture): ") + materialName);
                return nullptr;
            }
            return material;
        };

        hdStone_ = loadPbrMaterial("Materials/Game/StoneHD.xml",
                                   "Textures/StoneHDDiffuse.png", "Textures/StoneHDNormal.png",
                                   "Textures/StoneHDRoughMetal.png");
        hdFloor_ = loadPbrMaterial("Materials/Game/FloorTile.xml",
                                   "Textures/FloorTileDiffuse.png", "Textures/FloorTileNormal.png",
                                   "Textures/FloorTileRoughMetal.png");
        hdPanel_ = loadPbrMaterial("Materials/Game/MetalPanel.xml",
                                   "Textures/MetalPanelDiffuse.png", "Textures/MetalPanelNormal.png",
                                   "Textures/MetalPanelRoughMetal.png");
        hdMetal_ = loadPbrMaterial("Materials/Game/BrushedMetal.xml",
                                   "Textures/BrushedMetalDiffuse.png", "Textures/BrushedMetalNormal.png",
                                   "Textures/BrushedMetalRoughMetal.png");
        hdCrate_ = loadPbrMaterial("Materials/Game/CrateWood.xml",
                                   "Textures/CrateWoodDiffuse.png", "Textures/CrateWoodNormal.png",
                                   "Textures/CrateWoodRoughMetal.png");
        hdFeather_ = loadPbrMaterial("Materials/Game/FeatherPBR.xml",
                                     "Textures/FeatherPBRDiffuse.png", "Textures/FeatherPBRNormal.png",
                                     "Textures/FeatherPBRRoughMetal.png");
        hdHazard_ = loadPbrMaterial("Materials/Game/HazardMetal.xml",
                                    "Textures/HazardMetalDiffuse.png", "Textures/HazardMetalNormal.png",
                                    "Textures/HazardMetalRoughMetal.png");
        skyTexture_ = cache->GetResource<Texture2D>("Textures/SkyDay.png");
        EnsureSkyCubemap();

        {
            const char* hdTexNames[] = {
                "Textures/StoneHDDiffuse.png",      "Textures/StoneHDNormal.png",      "Textures/StoneHDRoughMetal.png",
                "Textures/FloorTileDiffuse.png",    "Textures/FloorTileNormal.png",    "Textures/FloorTileRoughMetal.png",
                "Textures/MetalPanelDiffuse.png",   "Textures/MetalPanelNormal.png",   "Textures/MetalPanelRoughMetal.png",
                "Textures/BrushedMetalDiffuse.png", "Textures/BrushedMetalNormal.png", "Textures/BrushedMetalRoughMetal.png",
                "Textures/CrateWoodDiffuse.png",    "Textures/CrateWoodNormal.png",    "Textures/CrateWoodRoughMetal.png",
                "Textures/FeatherPBRDiffuse.png",   "Textures/FeatherPBRNormal.png",   "Textures/FeatherPBRRoughMetal.png",
                "Textures/HazardMetalDiffuse.png",  "Textures/HazardMetalNormal.png",  "Textures/HazardMetalRoughMetal.png",
                "Textures/SkyDay.png"
            };
            anisoTextures_.Clear();
            for (const char* tn : hdTexNames) {
                Texture2D* t = cache->GetResource<Texture2D>(String(tn));
                if (t && !anisoTextures_.Contains(t)) anisoTextures_.Push(t);
            }
            ApplyTextureFiltering();
        }

        stoneMaterial_ = hdStone_ ? hdStone_ : cache->GetResource<Material>("Materials/Stone.xml");
        if (!stoneMaterial_) stoneMaterial_ = fallbackMat_;
        metalMaterial_ = hdMetal_ ? hdMetal_ : stoneMaterial_;

        {
            auto tileMat = [](Material* m, float tiles) {
                if (!m) return;
                m->SetShaderParameter("UOffset", Variant(Vector4(tiles, 0, 0, 0)));
                m->SetShaderParameter("VOffset", Variant(Vector4(0, tiles, 0, 0)));
            };
            tileMat(hdStone_, 10.0f);   // 4K бетон/базальт
            tileMat(hdFloor_, 1.25f);   // у пола уже UV 48x48 в CustomGeometry -> плитка ~35 см в 4K!
            tileMat(hdPanel_, 6.0f);    // 4K титановые бронепанели
            tileMat(hdMetal_, 4.0f);    // 4K шлифованная сталь
            tileMat(hdCrate_, 1.0f);    // 4K дубовые ящики со стальной оковкой
            tileMat(hdHazard_, 1.0f);   // 4K взрывоопасные бочки
        }

        // Мишени тира: насыщенный PBR с лёгким HDR-само-свечением, чтобы читались на любой дистанции
        {
            SharedPtr<Material> tMat = MakePBR(Color(235, 45, 55), 0.25f, 0.35f, Color(0.35f, 0.04f, 0.05f));
            SharedPtr<Material> tHit = MakeHDRGlow(Color(2.2f, 1.85f, 0.55f, 1.0f));
            targetMaterial_ = tMat.Get();
            targetHitMaterial_ = tHit.Get();
        }

        // --- Перьевые и анатомические 4K PBR-материалы кур (теперь с рельефом перьев, тенями и бликами!) ---
        chickWhite_    = MakeFeatheredPBR(Color(248, 246, 238),  0.00f);  // белая клуша-штурмовик
        chickBrown_    = MakeFeatheredPBR(Color(220, 110,  38), -0.06f);  // огненный петух-чарджер
        chickBlack_    = MakeFeatheredPBR(Color( 46,  48,  64), -0.10f);  // спецназ-стрелок (вороной блеск)
        chickBoss_     = MakeFeatheredPBR(Color(238, 242, 255), -0.14f);  // кибер-индюк босс (перламутр)
        chickKamikaze_ = MakeFeatheredPBR(Color(255,  72,  24), -0.08f);  // камикадзе-цыплёнок с C4
        chickDrone_    = MakeFeatheredPBR(Color( 75, 215, 255), -0.12f);  // реактивная куро-валькирия (дрон)
        combMat_    = MakePBR(Color(228,  28,  42), 0.32f, 0.04f, Color(0.18f, 0.01f, 0.02f));
        beakMat_    = MakePBR(Color(255, 182,  35), 0.20f, 0.08f);     // глянцевый роговой клюв
        legMat_     = MakePBR(Color(238, 168,  48), 0.42f, 0.04f);
        eggMat_     = MakePBR(Color(255, 248, 230), 0.26f, 0.02f, Color(0.18f, 0.16f, 0.10f));
        featherMat_ = MakeFeatheredPBR(Color(252, 250, 245),  0.04f);

        floorMaterialLight_ = hdFloor_ ? hdFloor_ : stoneMaterial_;
        RefreshWeaponMaterials();
    }

    void RefreshWeaponMaterials() {
        weaponTintMats_.Clear();
        weaponAccentMat_ = nullptr;
        weaponDarkMat_ = MakePBR(Color(30, 33, 40), 0.34f, 0.78f); // вороненая оружейная сталь
        const SkinDef& skin = SKIN_DEFS[Clamp(currentSkin_, 0, SKIN_COUNT - 1)];
        Color skinCol = NormByteColor(skin.color);
        Color accCol  = NormByteColor(skin.accent);
        weaponAccentMat_ = skin.emissive
            ? MakeHDRGlow(Color(accCol.r_ * 2.4f, accCol.g_ * 2.4f, accCol.b_ * 2.4f, 1.0f))
            : MakePBR(accCol, 0.18f, 0.94f);

        for (int w = 0; w < WEAPON_COUNT; ++w) {
            Color base = NormByteColor(WEAPON_DEFS[w].tint);
            Color c(Lerp(base.r_, skinCol.r_, 0.72f),
                    Lerp(base.g_, skinCol.g_, 0.72f),
                    Lerp(base.b_, skinCol.b_, 0.72f), 1.0f);
            Color emi = skin.emissive ? Color(c.r_ * 0.22f, c.g_ * 0.22f, c.b_ * 0.22f) : Color::BLACK;
            SharedPtr<Material> m = MakePBR(c, skin.roughness, skin.metallic, emi);
            // Накладываем 4K карту микрошлифовки оружейного металла (normal) при наличии
            auto* cache = GetSubsystem<ResourceCache>();
            if (Texture2D* bn = cache->GetResource<Texture2D>("Textures/BrushedMetalNormal.png")) {
                m->SetTexture(TU_NORMAL, bn);
            }
            weaponTintMats_.Push(m);
        }
    }

    static Color ClampColor(Color c) {
        return NormByteColor(c);
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

        // Если ни один .pfx не загрузился (Data неполный / битые файлы), строим
        // эффекты прямо в памяти на базе процедурных ParticleEffect и локальных
        // материалов ParticleAdditive/ParticleAlphaBlend.
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
        if (haveParticles_)
            URHO3D_LOGINFO(String("Частицы активны: ") + (sparkEffect_ ? "искры " : "") + (smokeEffect_ ? "дым" : ""));
        else
            URHO3D_LOGINFO("Частицы недоступны: нет .pfx и материалов — включены Lite-декали/осколки.");
    }

    SharedPtr<ParticleEffect> sparkEffectStore_, smokeEffectStore_; // владение fallback-эффектами

    // Программный ParticleEffect из стоковых материалов проекта.
    // Нужен как fallback, если .pfx отсутствует в неполной portable-копии Data/.
    SharedPtr<ParticleEffect> MakeProceduralParticles(const char* materialName, int maxParticles,
                                                      float emissionRate, float minSize, float maxSize,
                                                      float ttl, bool additive) {
        auto* cache = GetSubsystem<ResourceCache>();
        if (!cache) return SharedPtr<ParticleEffect>();
        Material* material = cache->GetResource<Material>(String(materialName));
        if (!material)
            material = cache->GetResource<Material>(additive ? "Materials/ParticleAdditive.xml"
                                                              : "Materials/ParticleAlphaBlend.xml");
        if (!material) material = cache->GetResource<Material>("Materials/Particle.xml");
        if (!material) return SharedPtr<ParticleEffect>();

        SharedPtr<ParticleEffect> effect(new ParticleEffect(context_));
        effect->SetMaterial(material);
        effect->SetNumParticles((unsigned)Max(1, maxParticles));
        effect->SetEmitterType(EMITTER_BOX);
        effect->SetEmitterSize(Vector3(0.25f, 0.25f, 0.25f));
        effect->SetMinDirection(Vector3(-20.0f, 45.0f, -20.0f));
        effect->SetMaxDirection(Vector3(20.0f, 90.0f, 20.0f));
        effect->SetConstantForce(additive ? Vector3(0.0f, -42.0f, 0.0f)
                                          : Vector3(0.0f, -18.0f, 0.0f));
        effect->SetDampingForce(0.25f);
        effect->SetMinEmissionRate(Max(1.0f, emissionRate * 0.70f));
        effect->SetMaxEmissionRate(Max(1.0f, emissionRate * 1.30f));
        effect->SetMinParticleSize(Vector2(minSize, minSize));
        effect->SetMaxParticleSize(Vector2(maxSize, maxSize));
        effect->SetMinTimeToLive(Max(0.05f, ttl * 0.55f));
        effect->SetMaxTimeToLive(Max(0.10f, ttl));
        effect->SetMinVelocity(additive ? 2.0f : 0.8f);
        effect->SetMaxVelocity(additive ? 6.0f : 3.0f);
        effect->SetMinRotation(-180.0f);
        effect->SetMaxRotation(180.0f);
        effect->SetMinRotationSpeed(-90.0f);
        effect->SetMaxRotationSpeed(90.0f);
        effect->SetSizeMul(0.72f);
        effect->SetNumColorFrames(2);
        effect->SetColorFrame(0, ColorFrame(additive ? Color(1.0f, 0.82f, 0.40f, 1.0f)
                                                       : Color(0.82f, 0.90f, 1.0f, 0.55f), 0.0f));
        effect->SetColorFrame(1, ColorFrame(additive ? Color(1.0f, 0.18f, 0.04f, 0.0f)
                                                       : Color(0.35f, 0.45f, 0.62f, 0.0f), 1.0f));
        return effect;
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
        CreateExplosiveBarrels();
        CreateJumpPads();
        CreatePickups();

        // Создаем освещение
        CreateLighting();

        // Зона рендеринга: IBL Cubemap отражения + атмосферная дымка
        CreateZone();

        // Позиционные звуки: пул источников создаётся один раз на сцену
        InitSfx3DPool();
    }

    void CreateZone() {
        Node* zoneNode = scene_->CreateChild("Zone");
        Zone* zone = zoneNode->CreateComponent<Zone>();
        sceneZone_ = zone;
        zone->SetBoundingBox(BoundingBox(-850.0f, 850.0f));
        // Альфа-канал AmbientColor (0.68) задаёт силу IBL отражений в шейдере IBL.glsl!
        zone->SetAmbientColor(Color(0.24f, 0.28f, 0.38f, 0.68f));
        zone->SetAmbientGradient(true);
        zone->SetFogColor(Color(0.14f, 0.12f, 0.22f));
        zone->SetFogStart(75.0f);
        zone->SetFogEnd(265.0f);
        zone->SetHeightFog(true);
        zone->SetFogHeight(-4.5f);
        zone->SetFogHeightScale(0.055f);
        EnsureSkyCubemap();
        if (skyCube_) zone->SetZoneTexture(skyCube_);
    }

    void CreateSky() {
        auto* cache = GetSubsystem<ResourceCache>();
        EnsureSkyCubemap();
        Node* skyNode = scene_->CreateChild("Sky");

        // Истинный 360° бесшовный Cubemap Skybox через Techniques/DiffSkybox.xml
        Material* baseSky = cache->GetResource<Material>("Materials/Skybox.xml");
        if (baseSky && skyCube_) {
            SharedPtr<Material> sm = baseSky->Clone();
            sm->SetTexture(TU_DIFFUSE, skyCube_);
            sm->SetShaderParameter("MatDiffColor", Variant(Color(1.05f, 1.02f, 1.08f, 1.0f)));
            tintedMatStore_.Push(sm);
            skyMaterial_ = sm.Get();
            Skybox* sky = skyNode->CreateComponent<Skybox>();
            sky->SetModel(boxModel_);
            sky->SetMaterial(skyMaterial_);
            return;
        }

        // Резервный 2D скайбокс
        SharedPtr<Material> skyM = MakeTinted(nullptr, nullptr, Color(150, 185, 235));
        skyMaterial_ = skyM.Get();
        if (skyTexture_) {
            skyMaterial_->SetTexture(TU_DIFFUSE, skyTexture_);
            skyMaterial_->SetCullMode(CULL_NONE);
            Skybox* sky = skyNode->CreateComponent<Skybox>();
            sky->SetModel(boxModel_);
            sky->SetMaterial(skyMaterial_);
        }
    }

    void CreateJumpPads() {
        jumpPads_.Clear();
        const Vector3 padCoords[4] = {
            Vector3( 19.0f, 0.04f,  19.0f),
            Vector3(-19.0f, 0.04f,  19.0f),
            Vector3( 19.0f, 0.04f, -19.0f),
            Vector3(-19.0f, 0.04f, -19.0f)
        };
        SharedPtr<Material> padGlow = MakeHDRGlow(Color(0.20f, 2.4f, 2.8f, 1.0f));
        SharedPtr<Material> padGold = MakeHDRGlow(Color(2.6f, 1.8f, 0.25f, 1.0f));
        Material* baseMetal = hdPanel_ ? hdPanel_ : metalMaterial_;
        Model* cyl = cylModel_ ? cylModel_ : boxModel_;

        for (int i = 0; i < 4; ++i) {
            Node* pad = scene_->CreateChild("JumpPad", LOCAL);
            pad->SetPosition(padCoords[i]);
            AddPart(pad, Vector3(0, 0.05f, 0), Vector3(2.8f, 0.12f, 2.8f), baseMetal, cyl, true);
            AddPart(pad, Vector3(0, 0.12f, 0), Vector3(2.25f, 0.05f, 2.25f), padGlow.Get(), cyl, false);
            Node* hoverRing = AddPart(pad, Vector3(0, 0.42f, 0), Vector3(1.55f, 0.06f, 1.55f), padGold.Get(), cyl, false);

            JumpPadData jp;
            jp.node_ = pad;
            jp.ringNode_ = hoverRing;
            jp.pos_ = padCoords[i];
            jp.phase_ = (float)i * 1.57f;
            jumpPads_.Push(jp);
        }
    }

    void UpdateJumpPads(float dt) {
        if (jumpPadCooldown_ > 0.0f) jumpPadCooldown_ = Max(0.0f, jumpPadCooldown_ - dt);
        if (!cameraNode_) return;
        Vector3 camPos = cameraNode_->GetPosition();
        Vector3 feet(camPos.x_, camPos.y_ - eyeHeight_, camPos.z_);

        for (unsigned i = 0; i < jumpPads_.Size(); ++i) {
            JumpPadData& jp = jumpPads_[i];
            if (jp.ringNode_) {
                float h = 0.36f + 0.18f * (0.5f + 0.5f * sinf(timeAcc_ * 4.5f + jp.phase_));
                float sc = 1.40f + 0.25f * sinf(timeAcc_ * 4.5f + jp.phase_);
                jp.ringNode_->SetPosition(Vector3(0, h, 0));
                jp.ringNode_->SetScale(Vector3(sc, 0.05f, sc));
            }
            if (jumpPadCooldown_ <= 0.0f && !inMenu_ && !gameOver_) {
                float dx = feet.x_ - jp.pos_.x_;
                float dz = feet.z_ - jp.pos_.z_;
                if (dx * dx + dz * dz < 2.65f && fabsf(feet.y_ - jp.pos_.y_) < 1.25f) {
                    jumpPadCooldown_ = 0.65f;
                    verticalVelocity_ = 14.6f;
                    onGround_ = false;
                    airborneFromPad_ = true;
                    playerVelocity_ *= 1.28f;
                    AddCameraShake(0.07f);
                    if (soundSource_ && jumpPadSound_) {
                        soundSource_->SetGain(SfxGain(0.85f));
                        soundSource_->Play(jumpPadSound_);
                    }
                    CreateShockwave(jp.pos_ + Vector3(0, 0.2f, 0), nullptr, 0.45f);
                }
            }
        }
    }

    void CreateExplosiveBarrels() {
        barrels_.Clear();
        const Vector3 posList[] = {
            Vector3( 16.0f, 0.85f,   0.0f), Vector3(-16.0f, 0.85f,   0.0f),
            Vector3(  0.0f, 0.85f,  16.0f), Vector3(  0.0f, 0.85f, -16.0f),
            Vector3( 26.0f, 0.85f,  18.0f), Vector3(-26.0f, 0.85f,  18.0f),
            Vector3( 26.0f, 0.85f, -18.0f), Vector3(-26.0f, 0.85f, -18.0f),
            Vector3( 18.0f, 0.85f,  28.0f), Vector3(-18.0f, 0.85f, -28.0f),
            Vector3( 40.0f, 0.85f,   0.0f), Vector3(-40.0f, 0.85f,   0.0f),
            Vector3(  0.0f, 0.85f,  40.0f), Vector3(  0.0f, 0.85f, -40.0f)
        };
        SharedPtr<Material> ledRed = MakeHDRGlow(Color(3.2f, 0.25f, 0.18f, 1.0f));
        SharedPtr<Material> fallbackHaz = MakePBR(Color(215, 35, 30), 0.30f, 0.55f);
        Material* bMat = hdHazard_ ? hdHazard_ : fallbackHaz.Get();
        Model* cyl = cylModel_ ? cylModel_ : boxModel_;
        Model* sph = sphereModel_ ? sphereModel_ : boxModel_;

        for (const auto& bp : posList) {
            Node* bn = scene_->CreateChild("HazardBarrel", LOCAL);
            bn->SetPosition(bp);
            Node* body = AddPart(bn, Vector3::ZERO, Vector3(1.05f, 1.70f, 1.05f), bMat, cyl, true);
            (void)body;
            Node* led = AddPart(bn, Vector3(0.0f, 0.92f, 0.0f), Vector3(0.22f, 0.12f, 0.22f), ledRed.Get(), sph, false);

            ExplosiveBarrel eb;
            eb.node_ = bn;
            eb.beacon_ = led;
            eb.home_ = bp;
            eb.hp_ = 42.0f;
            eb.exploded_ = false;
            eb.respawn_ = 0.0f;
            // tag = 3 (битовая маска 1<<3 = 8): попадают пули и взрывы + игрок не проходит сквозь бочку!
            eb.colliderIdx_ = AddBoxCollider(bp, Vector3(0.56f, 0.88f, 0.56f), Collision::BARREL, bn);
            barrels_.Push(eb);
        }
    }

    void DetonateBarrel(ExplosiveBarrel& eb) {
        if (eb.exploded_) return;
        eb.exploded_ = true;
        eb.respawn_ = 22.0f;
        Vector3 center = eb.home_;
        RemoveColliderLite(eb.colliderIdx_);
        eb.colliderIdx_ = -1;
        if (eb.node_) eb.node_->SetEnabled(false);

        CreateExplosionEffect(center);
        CreateShockwave(center, nullptr, 0.72f);
        CreateDebrisBurst(center);
        if (barrelBoomSound_) PlaySfx3D(center, barrelBoomSound_, 1.0f);
        else PlayExplosionAt(center);
        AddCameraShake(0.24f);
        PushKillFeed("ДЕТОНАЦИЯ ТОПЛИВНОЙ БОЧКИ!");
        ++barrelsDetonatedRun_;
        if (barrelsDetonatedRun_ >= 5) UnlockAchievement(2, "ПИРОМАН КУРЯТНИКА (5 бочек за забег)");

        // Мощный взрыв по площади (радиус 9.5 м, урон до 185 + цепная реакция соседних бочек!)
        const float rad = 9.5f;
        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            if (!enemies_[i].alive_ || !enemies_[i].node_) continue;
            float d = (enemies_[i].node_->GetPosition() - center).Length();
            if (d < rad) {
                float dmg = 185.0f * (1.0f - d / rad * 0.75f);
                SpawnFloatingDamage(enemies_[i].node_->GetPosition() + Vector3(0, 1.5f, 0), (int)dmg, true);
                DamageEnemy(enemies_[i], dmg);
            }
        }
        for (unsigned i = 0; i < targets_.Size(); ++i) {
            if (!targets_[i].isActive_ || !targets_[i].node_) continue;
            float d = (targets_[i].node_->GetPosition() - center).Length();
            if (d < rad) {
                targets_[i].health_ -= 180.0f * (1.0f - d / rad);
                if (targets_[i].health_ <= 0.0f) DestroyTarget(targets_[i]);
            }
        }
        if (cameraNode_ && !gameOver_) {
            float pd = (cameraNode_->GetPosition() - center).Length();
            if (pd < rad * 0.75f) DamagePlayer(55.0f * (1.0f - pd / (rad * 0.75f)));
        }
        // Цепная реакция по соседним бочкам
        for (unsigned i = 0; i < barrels_.Size(); ++i) {
            if (barrels_[i].exploded_) continue;
            float d = (barrels_[i].home_ - center).Length();
            if (d < rad * 0.85f) {
                barrels_[i].hp_ -= 120.0f * (1.0f - d / rad);
                if (barrels_[i].hp_ <= 0.0f) DetonateBarrel(barrels_[i]);
            }
        }
    }

    void UpdateExplosiveBarrels(float dt) {
        for (unsigned i = 0; i < barrels_.Size(); ++i) {
            ExplosiveBarrel& eb = barrels_[i];
            if (eb.exploded_) {
                eb.respawn_ -= dt;
                if (eb.respawn_ <= 0.0f) {
                    eb.exploded_ = false;
                    eb.hp_ = 42.0f;
                    if (eb.node_) eb.node_->SetEnabled(true);
                    eb.colliderIdx_ = AddBoxCollider(eb.home_, Vector3(0.56f, 0.88f, 0.56f), Collision::BARREL, eb.node_.Get());
                }
            } else if (eb.beacon_) {
                float p = 0.75f + 0.45f * sinf(timeAcc_ * 6.5f + (float)i * 1.3f);
                eb.beacon_->SetScale(Vector3(0.20f * p, 0.12f * p, 0.20f * p));
            }
        }
    }

    void CreateArenaDecor() {
        auto* cache = GetSubsystem<ResourceCache>();
        SharedPtr<Material> glowRed   = MakeHDRGlow(Color(2.8f, 0.22f, 0.20f, 1.0f));
        SharedPtr<Material> glowCyan  = MakeHDRGlow(Color(0.20f, 2.15f, 2.95f, 1.0f));
        SharedPtr<Material> glowGreen = MakeHDRGlow(Color(0.30f, 2.55f, 0.95f, 1.0f));
        SharedPtr<Material> glowPink  = MakeHDRGlow(Color(2.8f, 0.22f, 1.35f, 1.0f));
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

            // Вращающийся маячный венец делает дальние углы заметными даже в тумане.
            Node* ring = scene_->CreateChild("PerimeterBeaconRing");
            ring->SetPosition(tp + Vector3(0, 11.5f, 0));
            for (int r = 0; r < 8; ++r) {
                float a = (float)r * 45.0f * 0.0174533f;
                Node* marker = ring->CreateChild("Marker");
                marker->SetPosition(Vector3(cosf(a) * 1.0f, 0.0f, sinf(a) * 1.0f));
                marker->SetScale(Vector3(0.26f, 0.08f, 0.08f));
                marker->SetRotation(Quaternion(-a * 57.29578f, Vector3::UP));
                StaticModel* rsm = marker->CreateComponent<StaticModel>();
                rsm->SetModel(boxModel_);
                rsm->SetMaterial((r % 2) ? glowCyan.Get() : glowRed.Get());
                rsm->SetCastShadows(false);
            }
            AnimatedDeco beaconAnim;
            beaconAnim.node_ = SharedPtr<Node>(ring);
            beaconAnim.pos_ = ring->GetPosition();
            beaconAnim.spin_ = (tp.x_ * tp.z_ > 0.0f) ? 34.0f : -34.0f;
            beaconAnim.bobAmp_ = 0.12f;
            beaconAnim.bobSpeed_ = 2.0f;
            beaconAnim.phase_ = tp.x_ + tp.z_;
            animatedDeco_.Push(beaconAnim);
        }

        // Четыре дальних «сектора» с арками и цветовой маркировкой. Они создают
        // читаемые входы на карту и разбивают длинные 256-метровые линии обзора.
        const Vector3 gatePos[] = {
            Vector3(0, 0, -half * 0.76f), Vector3(0, 0, half * 0.76f),
            Vector3(-half * 0.76f, 0, 0), Vector3(half * 0.76f, 0, 0)
        };
        for (int i = 0; i < 4; ++i) {
            bool horizontal = (i < 2);
            Material* glow = (i % 2) ? glowPink.Get() : glowCyan.Get();
            Vector3 gp = gatePos[i];
            Vector3 side = horizontal ? Vector3(1, 0, 0) : Vector3(0, 0, 1);
            Vector3 inward = horizontal ? Vector3(0, 0, (gp.z_ < 0 ? 1 : -1))
                                       : Vector3((gp.x_ < 0 ? 1 : -1), 0, 0);
            Node* gate = scene_->CreateChild("SectorGate");
            gate->SetPosition(gp);
            Vector3 postL = gp + side * -3.6f + Vector3(0, 2.6f, 0);
            Vector3 postR = gp + side *  3.6f + Vector3(0, 2.6f, 0);
            AddPart(gate, side * -3.6f + Vector3(0, 2.6f, 0), Vector3(1.2f, 5.2f, 1.2f), hdPanel_ ? hdPanel_ : metalMaterial_, boxModel_, true);
            AddPart(gate, side *  3.6f + Vector3(0, 2.6f, 0), Vector3(1.2f, 5.2f, 1.2f), hdPanel_ ? hdPanel_ : metalMaterial_, boxModel_, true);
            AddBoxCollider(postL, Vector3(0.6f, 2.6f, 0.6f));
            AddBoxCollider(postR, Vector3(0.6f, 2.6f, 0.6f));
            AddPart(gate, Vector3(0, 5.1f, 0), horizontal ? Vector3(8.4f, 0.8f, 1.2f) : Vector3(1.2f, 0.8f, 8.4f), glow, boxModel_, false);
            Node* sign = AddPart(gate, Vector3(0, 4.3f, 0), Vector3(3.4f, 0.12f, 0.12f), glow, boxModel_, false);
            sign->SetRotation(Quaternion(horizontal ? 0.0f : 90.0f, Vector3::UP));
            (void)inward;
        }
    }
    
    void CreateFloor() {
        // Один меш на всю большую арену с UV-тайлингом: вместо тысяч плиток — один draw call.
        const float size = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE; // 256 м
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
        const float wallHeight = 9.5f;    // высокая кибер-стена: большая арена читается как стадион
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

        // Угловые бункеры (Г-образные стены) — дальние «точки» для перестрелок.
        // Позиция масштабируется вместе с большой ареной, чтобы внешний пояс не пустовал.
        const float bunkerOffset = half * 0.58f;
        const Vector3 bunkers[] = { Vector3(bunkerOffset, 0, bunkerOffset), Vector3(-bunkerOffset, 0, bunkerOffset),
                                    Vector3(bunkerOffset, 0, -bunkerOffset), Vector3(-bunkerOffset, 0, -bunkerOffset) };
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

        const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;

        // --- Технический балкон вдоль северной стены (высота 4.5 м) ---
        // Балкон теперь действительно проходит по дальней стене 256-метровой карты.
        Node* catwalk = scene_->CreateChild("Catwalk");
        catwalk->SetPosition(Vector3(0, 0, -half + 6.0f));
        const int catSegments = 14;
        const float catStep = (half * 2.0f - 14.0f) / (float)(catSegments - 1);
        for (int i = 0; i < catSegments; ++i) {
            float x = -half + 7.0f + (float)i * catStep;
            box(catwalk, Vector3(x, 4.5f, 0), Vector3(catStep + 0.4f, 0.3f, 6.0f), hdPanel_ ? hdPanel_ : metalMaterial_);
            // перила
            box(catwalk, Vector3(x, 5.5f, 2.9f), Vector3(catStep + 0.4f, 0.1f, 0.1f), metalMaterial_);
            box(catwalk, Vector3(x, 5.5f, -2.9f), Vector3(catStep + 0.4f, 0.1f, 0.1f), metalMaterial_);
            // стойки перил
            for (int p = -1; p <= 1; ++p)
                box(catwalk, Vector3(x + (float)p * (catStep * 0.42f), 5.0f, 2.9f), Vector3(0.1f, 1.0f, 0.1f), metalMaterial_);
            // подсветка под балконом
            box(catwalk, Vector3(x, 4.3f, 2.9f), Vector3(catStep - 2.0f, 0.05f, 0.12f),
                (i % 2) ? neonCyan.Get() : neonPink.Get());
        }

        // --- Трубы по стенам: три уровня, два цвета ---
        const float wallX[] = { -half + 2.0f, half - 2.0f };
        for (float wx : wallX) {
            for (int lvl = 0; lvl < 3; ++lvl) {
                Node* pipe = scene_->CreateChild("Pipe");
                pipe->SetPosition(Vector3(wx, 2.0f + (float)lvl * 1.2f, 0));
                pipe->SetScale(Vector3(0.35f, half - 4.0f, 0.35f));
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
            Vector3 p(cosf(rad) * (half * 0.72f), 0.06f, sinf(rad) * (half * 0.72f));
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

        // --- 3D Деревянная «БАНЬКА СМОТРЯЩЕГО» на арене ([P] — вход в симулятор парной!) ---
        {
            SharedPtr<Material> warmGlow = MakeHDRGlow(Color(2.5f, 1.45f, 0.38f, 1.0f));
            SharedPtr<Material> frostWin = MakeHDRGlow(Color(0.85f, 1.45f, 1.95f, 1.0f));
            Material* woodMat = hdCrate_ ? hdCrate_ : fallbackMat_;
            Material* pipeMat = hdMetal_ ? hdMetal_ : metalMaterial_;
            Vector3 bp(-28.0f, 0.0f, -34.0f);
            Node* banya = scene_->CreateChild("BanyaCabin");
            banya->SetPosition(bp);
            // Бревенчатый сруб + двускатная крыша + крыльцо + печная труба
            box(banya, Vector3(0.0f, 1.75f, 0.0f),  Vector3(6.8f, 3.5f, 5.8f), woodMat);
            box(banya, Vector3(0.0f, 3.85f, 0.0f),  Vector3(7.4f, 0.65f, 6.4f), woodMat);
            box(banya, Vector3(0.0f, 4.35f, 0.0f),  Vector3(5.2f, 0.55f, 6.2f), woodMat);
            box(banya, Vector3(0.0f, 0.12f, 3.55f), Vector3(3.4f, 0.24f, 1.5f), woodMat);
            // Светящийся тёплый дверной проём (+Z к центру арены) и запотевшее окошко
            box(banya, Vector3(0.0f, 1.25f, 2.93f), Vector3(1.45f, 2.35f, 0.12f), warmGlow.Get());
            box(banya, Vector3(2.05f, 1.95f, 2.93f), Vector3(1.15f, 0.95f, 0.12f), frostWin.Get());
            // Стальная труба печи-каменки
            Node* chim = banya->CreateChild("BanyaChimney");
            chim->SetPosition(Vector3(-2.1f, 5.2f, -1.2f));
            chim->SetScale(Vector3(0.55f, 2.4f, 0.55f));
            StaticModel* csm = chim->CreateComponent<StaticModel>();
            csm->SetModel(cylModel_ ? cylModel_ : boxModel_);
            csm->SetMaterial(pipeMat);
            // Тёплый оранжевый свет из предбанника
            Node* lNode = banya->CreateChild("BanyaWarmLight");
            lNode->SetPosition(Vector3(0.0f, 2.2f, 3.9f));
            Light* bLight = lNode->CreateComponent<Light>();
            bLight->SetLightType(LIGHT_POINT);
            bLight->SetColor(Color(1.0f, 0.62f, 0.22f));
            bLight->SetRange(14.0f);
            bLight->SetBrightness((gfxQualityIdx_ >= 2) ? 4.5f : 1.2f);
            // Коллайдер сруба для физики и грэппл-крюка
            AddBoxCollider(bp + Vector3(0.0f, 1.9f, 0.0f), Vector3(3.4f, 1.9f, 2.9f));
            banyaDoorPos_ = bp + Vector3(0.0f, 0.0f, 3.6f);
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
        // Пульсация двух неоновых источников — арена «дышит» (с учётом масштаба PBR)
        float pbrMult = (gfxQualityIdx_ >= 2) ? 6.8f : 1.0f;
        float pulse = 0.55f + 0.45f * (0.5f + 0.5f * sinf(timeAcc_ * 1.6f));
        if (neonLightA_) neonLightA_->SetBrightness(1.35f * pbrMult * pulse);
        if (neonLightB_) neonLightB_->SetBrightness(1.25f * pbrMult * (1.0f - 0.55f * pulse));
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
        
        // Физика для босс-мишени: Lite-коллайдер (Collision::TARGET, ptr = узел)
        TargetData bossData;
        bossData.node_ = bossTargetNode;
        bossData.colliderIdx_ = AddBoxCollider(bossTargetNode->GetPosition(), Vector3(2.0f, 2.0f, 0.2f), Collision::TARGET, bossTargetNode);
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
        data.colliderIdx_ = AddBoxCollider(position, Vector3(0.6f, 0.6f, 0.2f), Collision::TARGET, targetNode);
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
        mt.colliderIdx_ = AddSphereCollider(startPosition, 0.5f, Collision::MOVING_TARGET, targetNode);
        movingTargetsLite_.Push(mt);

        TargetData data;
        data.node_ = targetNode;
        data.colliderIdx_ = mt.colliderIdx_;
        data.health_ = GameConstants::TARGET_HEALTH * 1.5f;
        data.isActive_ = true;
        data.scoreValue_ = 150;
        targets_.Push(data);
    }
    
    void SyncPbrLightScale() {
        // Шейдер PBRLitSolid.glsl делит прямой свет на M_PI*M_PI (~9.87),
        // тогда как классический Blinn-Phong на низком/среднем пресете не делит.
        // Плюс 4 атмосферных пресета [F8]: Закат / Кибер-Полночь / Кровавое Затмение / Арктика!
        static const Color kSunCol[4]  = { Color(1.00f,0.86f,0.68f), Color(0.32f,0.58f,1.00f), Color(1.00f,0.28f,0.20f), Color(0.82f,0.94f,1.00f) };
        static const Color kMoonCol[4] = { Color(0.42f,0.56f,0.92f), Color(0.88f,0.22f,0.95f), Color(0.55f,0.12f,0.32f), Color(0.45f,0.78f,1.00f) };
        static const Color kAmbCol[4]  = { Color(0.24f,0.28f,0.38f), Color(0.11f,0.16f,0.30f), Color(0.26f,0.10f,0.14f), Color(0.28f,0.34f,0.44f) };
        static const Color kFogCol[4]  = { Color(0.14f,0.12f,0.22f), Color(0.04f,0.06f,0.15f), Color(0.20f,0.05f,0.08f), Color(0.20f,0.28f,0.38f) };
        static const float kSunMul[4]  = { 1.12f, 0.62f, 0.96f, 1.25f };

        int atm = Clamp(atmosphereIdx_, 0, 3);
        float pbr = (gfxQualityIdx_ >= 2) ? 7.6f : 1.0f;
        if (sunLight_) {
            sunLight_->SetColor(kSunCol[atm]);
            sunLight_->SetBrightness(kSunMul[atm] * pbr);
            sunLight_->SetCastShadows(gfxShadowIdx_ > 0);
        }
        if (moonLight_) {
            moonLight_->SetColor(kMoonCol[atm]);
            moonLight_->SetBrightness(0.40f * pbr);
        }
        if (spotLight_) spotLight_->SetBrightness(1.45f * pbr);
        for (unsigned i = 0; i < floodLights_.Size(); ++i)
            if (floodLights_[i]) floodLights_[i]->SetBrightness(0.95f * pbr);
        if (sceneZone_) {
            float iblAlpha = (gfxQualityIdx_ >= 2) ? 0.68f : 1.0f;
            Color ac = kAmbCol[atm];
            ac.a_ = iblAlpha;
            sceneZone_->SetAmbientColor(ac);
            sceneZone_->SetFogColor(kFogCol[atm]);
        }
    }

    void CycleAtmosphere() {
        static const char* kAtmNames[4] = {
            "ЗОЛОТОЙ ЗАКАТ 4K (Golden Hour)",
            "КИБЕРПАНК ПОЛНОЧЬ (Neon Midnight)",
            "КРОВАВОЕ ЗАТМЕНИЕ (Crimson Eclipse)",
            "АРКТИЧЕСКИЙ РАССВЕТ (Cryo Dawn)"
        };
        atmosphereIdx_ = (atmosphereIdx_ + 1) % 4;
        SyncPbrLightScale();
        PlayUISound();
        ShowMessage(String("Атмосфера 4K [F8]: ") + kAtmNames[atmosphereIdx_]);
        SaveSettingsFile();
    }

    void CreateLighting() {
        const float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        floodLights_.Clear();

        // --- Ключевой 4K солнечный свет: «золотой час» (4 каскада теней 4K/8K PCF!) ---
        Node* sunNode = scene_->CreateChild("Sun");
        sunNode->SetPosition(Vector3(0, 30, 20));
        sunNode->SetDirection(Vector3(-0.58f, -0.50f, -0.52f).Normalized());
        sunLight_ = sunNode->CreateComponent<Light>();
        sunLight_->SetLightType(LIGHT_DIRECTIONAL);
        sunLight_->SetColor(Color(1.0f, 0.88f, 0.68f, 1.0f));
        sunLight_->SetSpecularIntensity(1.25f);
        sunLight_->SetCastShadows(gfxShadowIdx_ > 0);
        sunLight_->SetShadowCascade(CascadeParameters(14.0f, 38.0f, 88.0f, 165.0f, 0.82f));
        // FIX(2.5.4): bias 0.00018/0.42 давал shadow acne (рябь и чёрные полосы на поверхностях).
        // Подняли константное смещение + slope-scaled под 165м каскад — рябь уходит, утечек нет.
        sunLight_->SetShadowBias(BiasParameters(0.00042f, 0.60f));
        sunLight_->SetShadowIntensity(0.18f);

        // --- Заполняющий небесно-сумеречный контровой свет ---
        Node* moonNode = scene_->CreateChild("MoonFill");
        moonNode->SetDirection(Vector3(0.6f, -0.35f, 0.55f).Normalized());
        moonLight_ = moonNode->CreateComponent<Light>();
        moonLight_->SetLightType(LIGHT_DIRECTIONAL);
        moonLight_->SetColor(Color(0.46f, 0.62f, 1.00f, 1.0f));
        moonLight_->SetSpecularIntensity(0.55f);
        moonLight_->SetCastShadows(false);

        // --- Кибер-прожектор над центральной голограммой ---
        Node* spotNode = scene_->CreateChild("PlatformSpot");
        spotNode->SetPosition(Vector3(0, 22, 0));
        spotNode->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
        spotLight_ = spotNode->CreateComponent<Light>();
        spotLight_->SetLightType(LIGHT_SPOT);
        spotLight_->SetColor(Color(0.62f, 0.90f, 1.0f, 1.0f));
        spotLight_->SetRange(42.0f);
        spotLight_->SetFov(56.0f);
        spotLight_->SetRadius(1.2f);
        spotLight_->SetCastShadows(false);

        // --- Четыре угловых PBR Area-источника (SphereLight блики на обсидиановом полу!) ---
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
            l->SetColor(Color(1.0f, 0.86f, 0.68f, 1.0f));
            l->SetRange(30.0f);
            l->SetRadius(1.1f); // PBR SphereLight!
            l->SetCastShadows(false);
            floodLights_.Push(l);
        }

        // --- Две неоновые PBR TubeLight лампы по бокам площади ---
        Node* neonA = scene_->CreateChild("NeonA");
        neonA->SetPosition(Vector3(-12.0f, 3.0f, 0.0f));
        neonLightA_ = neonA->CreateComponent<Light>();
        neonLightA_->SetLightType(LIGHT_POINT);
        neonLightA_->SetColor(Color(0.22f, 0.88f, 1.0f, 1.0f));
        neonLightA_->SetRange(24.0f);
        neonLightA_->SetRadius(0.45f);
        neonLightA_->SetLength(3.5f);
        neonLightA_->SetCastShadows(false);

        Node* neonB = scene_->CreateChild("NeonB");
        neonB->SetPosition(Vector3(12.0f, 3.0f, 0.0f));
        neonLightB_ = neonB->CreateComponent<Light>();
        neonLightB_->SetLightType(LIGHT_POINT);
        neonLightB_->SetColor(Color(1.0f, 0.32f, 0.68f, 1.0f));
        neonLightB_->SetRange(24.0f);
        neonLightB_->SetRadius(0.45f);
        neonLightB_->SetLength(3.5f);
        neonLightB_->SetCastShadows(false);

        SyncPbrLightScale();
    }

    // =========================================================================
    // Конфигурация 4K HDR Post-Process RenderPath:
    // ForwardHWDepth -> SSAO4K -> BloomHDR -> Cinema4K (ACES + CAS 4K) -> FXAA3
    // =========================================================================
    void ConfigureRenderPipeline(Viewport* vp) {
        if (!vp) return;
        auto* cache = GetSubsystem<ResourceCache>();

        if (XMLFile* hwDepth = cache->GetResource<XMLFile>("RenderPaths/ForwardHWDepth.xml")) {
            SharedPtr<RenderPath> rp(new RenderPath());
            if (rp->Load(hwDepth)) {
                vp->SetRenderPath(rp);
            }
        }
        renderPath_ = vp->GetRenderPath();
        if (!renderPath_) return;

        if (XMLFile* ssaoXml = cache->GetResource<XMLFile>("PostProcess/SSAO4K.xml"))
            renderPath_->Append(ssaoXml);
        if (XMLFile* bloomXml = cache->GetResource<XMLFile>("PostProcess/BloomHDR.xml"))
            renderPath_->Append(bloomXml);
        if (XMLFile* cinemaXml = cache->GetResource<XMLFile>("PostProcess/Cinema4K.xml"))
            renderPath_->Append(cinemaXml);
        else if (XMLFile* tmXml = cache->GetResource<XMLFile>("PostProcess/Tonemap.xml"))
            renderPath_->Append(tmXml);
        if (XMLFile* fxaaXml = cache->GetResource<XMLFile>("PostProcess/FXAA3.xml"))
            renderPath_->Append(fxaaXml);

        SyncPostProcessParams();
    }

    void SyncPostProcessParams() {
        if (!renderPath_) return;
        bool useSsao   = gfxSsao_ && (gfxQualityIdx_ >= 1);
        bool useBloom  = (gfxGlowIdx_ > 0);
        bool useCinema = true; // ACES tonemap + CAS 4K + виньетка всегда держат идеальный контраст!
        bool useFxaa   = gfxFxaa_;

        renderPath_->SetEnabled("SSAO4K", useSsao);
        renderPath_->SetEnabled("BloomHDR", useBloom);
        renderPath_->SetEnabled("Cinema4K", useCinema);
        renderPath_->SetEnabled("Tonemap", !renderPath_->IsAdded("Cinema4K") && gfxHdr_);
        renderPath_->SetEnabled("FXAA3", useFxaa);

        renderPath_->SetShaderParameter("SSAORadius", Variant(0.85f));
        renderPath_->SetShaderParameter("SSAOStrength", Variant(gfxQualityIdx_ >= 3 ? 0.78f : 0.60f));

        float bloomMix = (gfxGlowIdx_ >= 2) ? 0.34f : 0.16f;
        renderPath_->SetShaderParameter("BloomHDRThreshold", Variant(0.92f));
        renderPath_->SetShaderParameter("BloomHDRMix", Variant(Vector2(1.0f, bloomMix)));

        renderPath_->SetShaderParameter("CinemaExposure", Variant(1.06f));
        renderPath_->SetShaderParameter("CinemaSaturation", Variant(1.10f));
        renderPath_->SetShaderParameter("ChromaStrength", Variant(gfxQualityIdx_ >= 2 ? 0.0011f : 0.0f));
        renderPath_->SetShaderParameter("SharpenStrength", Variant(gfxSharpen_ ? 0.48f : 0.0f));
        renderPath_->SetShaderParameter("VignetteStrength", Variant(0.20f));
    }

    // =========================================================================
    // Создание игрока и оружия
    // =========================================================================
    
    void CreatePlayer() {
        cameraNode_ = scene_->CreateChild("Camera");
        soundSource_ = cameraNode_->CreateComponent<SoundSource>();
        explosionSource3D_ = scene_->CreateChild("ExplosionSfx")->CreateComponent<SoundSource3D>();
        cameraNode_->SetPosition(Vector3(0.0f, GameConstants::PLAYER_HEIGHT, 0.0f));
        
        Camera* camera = cameraNode_->CreateComponent<Camera>();
        currentFov_ = fovBase_;
        camera->SetFov(fovBase_);
        camera->SetNearClip(0.08f);
        camera->SetFarClip(1000.0f);

        // 3D точка тактического лазерного целеуказателя (клавиша L)
        laserDotNode_ = scene_->CreateChild("LaserDot", LOCAL);
        laserDotNode_->SetScale(Vector3::ONE * 0.06f);
        StaticModel* lsm = laserDotNode_->CreateComponent<StaticModel>();
        lsm->SetModel(sphereModel_ ? sphereModel_ : boxModel_);
        laserDotMat_ = MakeHDRGlow(Color(3.4f, 0.25f, 0.35f, 1.0f));
        lsm->SetMaterial(laserDotMat_.Get());
        lsm->SetCastShadows(false);
        laserDotNode_->SetEnabled(false);

        auto* renderer = GetSubsystem<Renderer>();
        SharedPtr<Viewport> vp(new Viewport(context_, scene_, camera));
        renderer->SetViewport(0, vp);
        ConfigureRenderPipeline(vp);

        // The update loop starts as soon as the loading overlay is hidden.
        // Create the first-person weapon before that happens; otherwise
        // UpdateWeaponBob/ApplyRecoilToWeapon dereference a null weaponNode_.
        CreateWeapon();
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
            case E_CHARGER:  feathers = chickBrown_.Get();    combScale = 1.45f; break;
            case E_SHOOTER:  feathers = chickBlack_.Get();    combScale = 0.8f;  break;
            case E_BOSS:     feathers = chickBoss_.Get();     combScale = 2.0f;  break;
            case E_KAMIKAZE: feathers = chickKamikaze_.Get(); combScale = 1.25f; break;
            case E_DRONE:    feathers = chickDrone_.Get();    combScale = 0.9f;  break;
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

        // --- Кибер-бронедетали в зависимости от класса врага (4K PBR металл + HDR ядро)
        if (type == E_BOSS) {
            Material* armorMat = hdPanel_ ? hdPanel_ : metalMaterial_;
            AddPart(root, Vector3(0, 0.92f, 0.58f), Vector3(0.62f, 0.44f, 0.24f), armorMat, sph, true);
            AddPart(root, Vector3(0, 0.92f, 0.70f), Vector3(0.26f, 0.26f, 0.12f),
                    eyeMatBoss_ ? eyeMatBoss_.Get() : comb, sph, false); // светящийся реактор на груди босса
        } else if (type == E_SHOOTER) {
            Material* tacMat = hdMetal_ ? hdMetal_ : metalMaterial_;
            AddPart(root, Vector3(0, 0.95f, 0.52f), Vector3(0.50f, 0.36f, 0.18f), tacMat); // бронежилет спецназа
        } else if (type == E_CHARGER) {
            Material* spikeMat = hdMetal_ ? hdMetal_ : metalMaterial_;
            AddPart(root, Vector3(-0.56f, 1.08f, 0.22f), Vector3(0.22f, 0.22f, 0.32f), spikeMat, cone);
            AddPart(root, Vector3( 0.56f, 1.08f, 0.22f), Vector3(0.22f, 0.22f, 0.32f), spikeMat, cone);
        } else if (type == E_KAMIKAZE) {
            // Цыплёнок-камикадзе несёт мини-бочку C4 HazardMetal на спине + мигающий детонатор!
            Material* c4Mat = hdHazard_ ? hdHazard_ : comb;
            Node* c4 = AddPart(root, Vector3(0, 1.32f, -0.10f), Vector3(0.42f, 0.58f, 0.42f), c4Mat, cyl, true);
            c4->SetRotation(Quaternion(90.0f, Vector3(0, 0, 1)));
            AddPart(root, Vector3(0, 1.60f, -0.10f), Vector3(0.20f, 0.14f, 0.20f),
                    eyeMatKamikaze_ ? eyeMatKamikaze_.Get() : comb, sph, false);
        } else if (type == E_DRONE) {
            // Реактивная куро-валькирия: два турбореактивных двигателя под крыльями с плазменным соплом!
            Material* jetMat = hdPanel_ ? hdPanel_ : metalMaterial_;
            Material* nozMat = eyeMatDrone_ ? eyeMatDrone_.Get() : comb;
            for (int j = 0; j < 2; ++j) {
                float jx = (j == 0) ? -0.66f : 0.66f;
                AddPart(root, Vector3(jx, 0.72f, -0.05f), Vector3(0.22f, 0.42f, 0.22f), jetMat, cyl, true);
                AddPart(root, Vector3(jx, 0.46f, -0.05f), Vector3(0.18f, 0.16f, 0.18f), nozMat, cone, false);
            }
        }

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
        Material* em = (type == E_CHARGER)  ? eyeMatCharger_.Get()
                     : (type == E_SHOOTER)  ? eyeMatShooter_.Get()
                     : (type == E_BOSS)     ? eyeMatBoss_.Get()
                     : (type == E_KAMIKAZE) ? eyeMatKamikaze_.Get()
                     : (type == E_DRONE)    ? eyeMatDrone_.Get()
                                            : eyeMatMelee_.Get();
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
        Model* sph = sphereModel_ ? sphereModel_ : boxModel_;
        Model* cone = GetSubsystem<ResourceCache>()->GetResource<Model>("Models/Cone.mdl");
        if (!cone) cone = cyl;

        // Animation state belongs to the newly rebuilt model, not the old weapon.
        weaponSpinNode_ = nullptr;
        weaponPulseNode_ = nullptr;
        weaponSlideNode_ = nullptr;
        weaponPulseBaseScale_ = Vector3::ONE;
        weaponSlideBasePosition_ = Vector3::ZERO;
        weaponSlideBaseRotation_ = Quaternion::IDENTITY;

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
        case W_RAILGUN: {
            // Квантовый рельсотрон: две параллельные магнитные рельсы + 3 светящихся конденсаторных кольца + голоприцел
            if (!railBeamMat_) railBeamMat_ = MakeHDRGlow(Color(0.25f, 1.95f, 3.2f, 1.0f));
            AddPart(weaponModelNode_, Vector3( 0.00f, -0.015f, 0.12f), Vector3(0.065f, 0.065f, 0.32f), gunMat);
            AddPart(weaponModelNode_, Vector3(-0.026f, 0.012f, 0.44f), Vector3(0.016f, 0.030f, 0.62f), gunMat);
            AddPart(weaponModelNode_, Vector3( 0.026f, 0.012f, 0.44f), Vector3(0.016f, 0.030f, 0.62f), gunMat);
            AddPart(weaponModelNode_, Vector3( 0.00f,  0.012f, 0.42f), Vector3(0.018f, 0.018f, 0.56f), railBeamMat_.Get(), cyl);
            for (int r = 0; r < 3; ++r)
                AddPart(weaponModelNode_, Vector3(0, 0.012f, 0.30f + r * 0.15f), Vector3(0.075f, 0.055f, 0.018f), accent);
            AddPart(weaponModelNode_, Vector3(0,  0.072f, 0.18f), Vector3(0.032f, 0.032f, 0.16f), dark);
            AddPart(weaponModelNode_, Vector3(0,  0.092f, 0.18f), Vector3(0.026f, 0.018f, 0.08f), railBeamMat_.Get());
            AddPart(weaponModelNode_, Vector3(0, -0.095f, 0.05f), Vector3(0.04f,  0.12f,  0.05f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.02f, -0.09f), Vector3(0.055f, 0.075f, 0.16f), dark);
            break;
        }
        case W_PLASMA: {
            // Плазменный яйцемёт: сферическая камера удержания биоплазмы + 3 излучающих зубца
            if (!plasmaOrbMat_) plasmaOrbMat_ = MakeHDRGlow(Color(0.20f, 2.8f, 1.45f, 1.0f));
            Model* sph = sphereModel_ ? sphereModel_ : boxModel_;
            AddPart(weaponModelNode_, Vector3(0, -0.01f, 0.18f), Vector3(0.075f, 0.07f, 0.38f), gunMat);
            weaponPulseNode_ = AddPart(weaponModelNode_, Vector3(0,  0.03f, 0.28f), Vector3(0.085f, 0.085f, 0.12f), plasmaOrbMat_.Get(), sph);
            for (int k = 0; k < 3; ++k) {
                float a = k * 2.09439f;
                AddPart(weaponModelNode_, Vector3(cosf(a) * 0.038f, sinf(a) * 0.038f + 0.01f, 0.46f),
                        Vector3(0.016f, 0.016f, 0.24f), accent, cyl);
            }
            AddPart(weaponModelNode_, Vector3(0, -0.095f, 0.06f), Vector3(0.04f, 0.12f, 0.05f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.065f, 0.28f), Vector3(0.038f, 0.07f, 0.07f), dark);
            break;
        }
        case W_ARC: {
            // ARC CANNON: тяжёлый корпус, две дуговые шины и золотое конденсаторное ядро.
            if (!arcCoreMat_) arcCoreMat_ = MakeHDRGlow(Color(2.8f, 1.55f, 0.16f, 1.0f));
            AddPart(weaponModelNode_, Vector3(0, -0.015f, 0.14f), Vector3(0.10f, 0.11f, 0.28f), gunMat);
            AddPart(weaponModelNode_, Vector3(-0.075f, 0.025f, 0.38f), Vector3(0.025f, 0.035f, 0.44f), gunMat, cyl);
            AddPart(weaponModelNode_, Vector3( 0.075f, 0.025f, 0.38f), Vector3(0.025f, 0.035f, 0.44f), gunMat, cyl);
            weaponPulseNode_ = AddPart(weaponModelNode_, Vector3(0, 0.01f, 0.27f), Vector3(0.12f, 0.12f, 0.12f), arcCoreMat_.Get(), sph);
            for (int i = 0; i < 3; ++i)
                AddPart(weaponModelNode_, Vector3(0, 0.01f, 0.30f + (float)i * 0.13f), Vector3(0.14f, 0.018f, 0.018f), accent);
            weaponSlideNode_ = weaponModelNode_->CreateChild("ArcCapacitorSlide");
            weaponSlideNode_->SetPosition(Vector3(0, 0.04f, 0.18f));
            AddPart(weaponSlideNode_, Vector3::ZERO, Vector3(0.075f, 0.035f, 0.12f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.11f, 0.02f), Vector3(0.055f, 0.13f, 0.08f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.04f, -0.08f), Vector3(0.07f, 0.08f, 0.16f), dark);
            break;
        }
        case W_CRYO: {
            // CRYO LANCE: длинный крио-ствол, вращающиеся охлаждающие кольца и ледяное ядро.
            if (!cryoCoreMat_) cryoCoreMat_ = MakeHDRGlow(Color(0.24f, 1.65f, 3.0f, 1.0f));
            Material* cryoMat = cryoCoreMat_.Get();
            AddPart(weaponModelNode_, Vector3(0, -0.01f, 0.16f), Vector3(0.075f, 0.075f, 0.34f), gunMat);
            weaponSpinNode_ = weaponModelNode_->CreateChild("CryoCoils");
            weaponSpinNode_->SetPosition(Vector3(0, 0.02f, 0.42f));
            for (int i = 0; i < 4; ++i) {
                float a = (float)i * 1.5707963f;
                Node* coil = AddPart(weaponSpinNode_, Vector3(cosf(a) * 0.055f, sinf(a) * 0.055f, 0),
                                     Vector3(0.018f, 0.018f, 0.18f), cryoMat, cyl);
                coil->SetRotation(Quaternion((float)i * 45.0f, Vector3(0, 0, 1)));
            }
            weaponPulseNode_ = AddPart(weaponModelNode_, Vector3(0, 0.025f, 0.34f), Vector3(0.09f, 0.09f, 0.11f), cryoMat, sph);
            weaponSlideNode_ = weaponModelNode_->CreateChild("CryoBreechSlide");
            weaponSlideNode_->SetPosition(Vector3(0, 0.01f, 0.19f));
            AddPart(weaponSlideNode_, Vector3::ZERO, Vector3(0.06f, 0.045f, 0.13f), dark);
            AddPart(weaponModelNode_, Vector3(0, 0.03f, 0.58f), Vector3(0.025f, 0.025f, 0.28f), cryoMat, cyl);
            AddPart(weaponModelNode_, Vector3(0, -0.10f, 0.04f), Vector3(0.045f, 0.12f, 0.06f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.06f, 0.23f), Vector3(0.045f, 0.09f, 0.08f), dark);
            break;
        }
        case W_SAW: {
            // ION SAW: вращающийся энергетический диск с четырьмя зубьями.
            Model* torus = GetSubsystem<ResourceCache>()->GetResource<Model>("Models/Torus.mdl");
            if (!sawGlowMat_) sawGlowMat_ = MakeHDRGlow(Color(3.0f, 0.42f, 0.08f, 1.0f));
            weaponSpinNode_ = weaponModelNode_->CreateChild("IonSawRotor");
            weaponSpinNode_->SetPosition(Vector3(0, 0.025f, 0.35f));
            Node* blade = AddPart(weaponSpinNode_, Vector3::ZERO, Vector3(0.18f, 0.035f, 0.18f), sawGlowMat_.Get(), torus ? torus : cyl);
            weaponPulseNode_ = blade;
            for (int i = 0; i < 4; ++i) {
                float a = (float)i * 1.5707963f;
                Node* tooth = AddPart(weaponSpinNode_, Vector3(cosf(a) * 0.16f, 0.0f, sinf(a) * 0.16f),
                                      Vector3(0.045f, 0.045f, 0.11f), sawGlowMat_.Get(), cone);
                tooth->SetRotation(Quaternion(-90.0f, Vector3(1, 0, 0)) * Quaternion((float)i * 90.0f, Vector3(0, 1, 0)));
            }
            weaponSlideNode_ = weaponModelNode_->CreateChild("IonSawBreechSlide");
            weaponSlideNode_->SetPosition(Vector3(0, 0.0f, 0.18f));
            AddPart(weaponSlideNode_, Vector3::ZERO, Vector3(0.065f, 0.04f, 0.12f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.02f, 0.14f), Vector3(0.09f, 0.09f, 0.30f), gunMat);
            AddPart(weaponModelNode_, Vector3(0, -0.10f, 0.02f), Vector3(0.05f, 0.13f, 0.08f), dark);
            AddPart(weaponModelNode_, Vector3(0, -0.08f, 0.24f), Vector3(0.05f, 0.09f, 0.08f), dark);
            break;
        }
        if (weaponPulseNode_) weaponPulseBaseScale_ = weaponPulseNode_->GetScale();
        if (weaponSlideNode_) {
            weaponSlideBasePosition_ = weaponSlideNode_->GetPosition();
            weaponSlideBaseRotation_ = weaponSlideNode_->GetRotation();
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
    // Создание 4K UHD UI (авто-масштаб uiScale_ + радар + телеметрия + урон)
    // =========================================================================
    
    void CreateUI() {
        RefreshUIScale();
        auto* ui = GetSubsystem<UI>();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();
        
        CreateCrosshair();
        CreateHUD();
        CreateRadar();
        CreateTelemetryHUD();
        CreateAchievementToastHUD();
        
        damageOverlay_ = ui->GetRoot()->CreateChild<BorderImage>();
        damageOverlay_->SetSize(sw, sh);
        damageOverlay_->SetColor(Color(1.0f, 0.0f, 0.0f, 0.0f));
        damageOverlay_->SetBlendMode(BLEND_ADD);
        

        CreateHitMarker();

        {
            const int th = S(90.0f);
            BorderImage* t = ui->GetRoot()->CreateChild<BorderImage>();
            t->SetSize(sw, th); t->SetPosition(0, 0);      t->SetBlendMode(BLEND_ADD); t->SetColor(Color(1,0,0,0));
            BorderImage* b = ui->GetRoot()->CreateChild<BorderImage>();
            b->SetSize(sw, th); b->SetPosition(0, sh-th);  b->SetBlendMode(BLEND_ADD); b->SetColor(Color(1,0,0,0));
            BorderImage* l = ui->GetRoot()->CreateChild<BorderImage>();
            l->SetSize(th, sh); l->SetPosition(0, 0);      l->SetBlendMode(BLEND_ADD); l->SetColor(Color(1,0,0,0));
            BorderImage* r = ui->GetRoot()->CreateChild<BorderImage>();
            r->SetSize(th, sh); r->SetPosition(sw-th, 0);  r->SetBlendMode(BLEND_ADD); r->SetColor(Color(1,0,0,0));
            lowHealthVignette_ = t;
            vignetteEdges_.Push(SharedPtr<BorderImage>(t));
            vignetteEdges_.Push(SharedPtr<BorderImage>(b));
            vignetteEdges_.Push(SharedPtr<BorderImage>(l));
            vignetteEdges_.Push(SharedPtr<BorderImage>(r));
        }

        CreateGameOverPanel();

        auto* gfx = GetSubsystem<Graphics>();
        if (gfx) { uiWidth_ = gfx->GetWidth(); uiHeight_ = gfx->GetHeight(); }
        lastCrosshairGap_ = -1.0f;
    }

    void CreateHitMarker() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int cx = GetSubsystem<Graphics>()->GetWidth() / 2;
        int cy = GetSubsystem<Graphics>()->GetHeight() / 2;
        int w = S(11.0f), h = S(3.0f), d1 = S(19.0f), d2 = S(8.0f), d3 = S(16.0f);
        struct Seg { int x, y, w, h; };
        const Seg segs[4] = {
            { cx - d1, cy - d1, w, h },
            { cx + d2, cy - d1, w, h },
            { cx - d1, cy + d3, w, h },
            { cx + d2, cy + d3, w, h },
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

    bool hitWasHeadshot_ = false;
    void ShowHitMarker(bool kill, bool headshot = false) {
        hitMarkerTimer_ = (kill || headshot) ? 0.30f : 0.16f;
        if (kill) hitKillTimer_ = 0.30f;
        hitWasHeadshot_ = headshot;
    }

    void UpdateHitMarker(float dt) {
        Color c(0.0f, 0.0f, 0.0f, 0.0f);
        if (hitMarkerTimer_ > 0.0f) {
            hitMarkerTimer_ -= dt;
            float a = Clamp(hitMarkerTimer_ / 0.16f, 0.0f, 1.0f);
            bool kill = hitKillTimer_ > 0.0f;
            if (kill) hitKillTimer_ -= dt;
            if (hitWasHeadshot_) c = Color(1.0f, 0.86f, 0.18f, a); // золотой маркер критического хэдшота!
            else c = kill ? Color(1.0f, 0.25f, 0.2f, a) : Color(1.0f, 1.0f, 1.0f, a * 0.92f);
            if (hitMarkerTimer_ <= 0.0f) hitWasHeadshot_ = false;
        }
        for (unsigned i = 0; i < hitMarkerBars_.Size(); ++i)
            hitMarkerBars_[i]->SetColor(c);
    }

    void UpdateLowHealthVignette(float dt) {
        (void)dt;
        float ratio = player_.maxHealth_ > 0.0f ? player_.health_ / player_.maxHealth_ : 1.0f;
        float a = 0.0f;
        if (!gameOver_ && gameMode_ == MODE_ARENA && ratio < 0.3f && ratio > 0.0f) {
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
        bg->SetColor(Color(0.03f, 0.005f, 0.01f, 0.86f));

        Text* title = gameOverPanel_->CreateChild<Text>();
        title->SetFont(font_, SF(72));
        title->SetText("МИССИЯ ПРОВАЛЕНА");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - S(460), sh / 2 - S(165));
        title->SetWidth(S(920));
        title->SetColor(Color(1.0f, 0.18f, 0.14f, 1.0f));
        StyleCrispText(title, true);
        gameOverTitle_ = SharedPtr<Text>(title);

        BorderImage* goBar = gameOverPanel_->CreateChild<BorderImage>();
        goBar->SetPosition(sw / 2 - S(240), sh / 2 - S(72));
        goBar->SetSize(S(480), S(3));
        goBar->SetColor(Color(1.0f, 0.25f, 0.18f, 0.88f));

        gameOverStatsText_ = gameOverPanel_->CreateChild<Text>();
        gameOverStatsText_->SetFont(font_, SF(23));
        gameOverStatsText_->SetTextAlignment(HA_CENTER);
        gameOverStatsText_->SetPosition(sw / 2 - S(450), sh / 2 - S(46));
        gameOverStatsText_->SetWidth(S(900));
        gameOverStatsText_->SetColor(Color(0.95f, 0.92f, 0.88f, 1.0f));
        StyleCrispText(gameOverStatsText_);

        Text* hint = gameOverPanel_->CreateChild<Text>();
        hint->SetFont(font_, SF(20));
        hint->SetText("ENTER / ПРОБЕЛ — СНОВА В БОЙ   |   ESC — ГЛАВНОЕ МЕНЮ");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - S(450), sh / 2 + S(135));
        hint->SetWidth(S(900));
        hint->SetColor(Color(1.0f, 0.85f, 0.3f, 1.0f));
        StyleCrispText(hint);
        gameOverHint_ = SharedPtr<Text>(hint);

        gameOverPanel_->SetVisible(false);
    }

    void AnimateGameOver(float dt) {
        if (!gameOverPanel_ || !gameOverPanel_->IsVisible()) return;
        gameOverPulse_ += dt;
        if (gameOverTitle_) {
            float beat = powf(0.5f + 0.5f * sinf(gameOverPulse_ * 3.2f), 2.0f);
            gameOverTitle_->SetColor(Color(0.78f + 0.22f * beat, 0.12f + 0.08f * beat, 0.10f, 1.0f));
            if (!goBasePosSet_) { goBasePos_ = gameOverTitle_->GetPosition(); goBasePosSet_ = true; }
            gameOverTitle_->SetPosition(goBasePos_.x_ + int(sinf(gameOverPulse_ * 9.0f) * beat * S(3.0f)),
                                        goBasePos_.y_ + int(beat * S(4.0f)));
        }
        if (gameOverHint_) {
            gameOverHint_->SetColor(Color(1.0f, 0.85f, 0.3f, 0.55f + 0.45f * sinf(gameOverPulse_ * 2.4f)));
        }
    }
    
    void CreateCrosshair() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        
        int cx = GetSubsystem<Graphics>()->GetWidth() / 2;
        int cy = GetSubsystem<Graphics>()->GetHeight() / 2;
        int dot = Max(2, S(4.0f));
        int th  = Max(2, S(2.0f));
        int len = S(10.0f);
        
        crosshairCenter_ = root->CreateChild<BorderImage>();
        crosshairCenter_->SetSize(dot, dot);
        crosshairCenter_->SetPosition(cx - dot / 2, cy - dot / 2);
        crosshairCenter_->SetColor(Color(0.25f, 1.0f, 0.72f, 0.95f));
        
        crosshairTop_ = root->CreateChild<BorderImage>();
        crosshairTop_->SetSize(th, len);
        crosshairTop_->SetPosition(cx - th / 2, cy - S(15.0f));
        crosshairTop_->SetColor(Color(0.25f, 1.0f, 0.72f, 0.85f));
        
        crosshairBottom_ = root->CreateChild<BorderImage>();
        crosshairBottom_->SetSize(th, len);
        crosshairBottom_->SetPosition(cx - th / 2, cy + S(5.0f));
        crosshairBottom_->SetColor(Color(0.25f, 1.0f, 0.72f, 0.85f));
        
        crosshairLeft_ = root->CreateChild<BorderImage>();
        crosshairLeft_->SetSize(len, th);
        crosshairLeft_->SetPosition(cx - S(15.0f), cy - th / 2);
        crosshairLeft_->SetColor(Color(0.25f, 1.0f, 0.72f, 0.85f));
        
        crosshairRight_ = root->CreateChild<BorderImage>();
        crosshairRight_->SetSize(len, th);
        crosshairRight_->SetPosition(cx + S(5.0f), cy - th / 2);
        crosshairRight_->SetColor(Color(0.25f, 1.0f, 0.72f, 0.85f));
    }

    void CreateRadar() {
        auto* root = GetSubsystem<UI>()->GetRoot();
        radarBlips_.Clear();
        int rSize = S(154.0f);
        int rx = S(22.0f), ry = S(18.0f);

        radarBg_ = root->CreateChild<BorderImage>("RadarBg");
        radarBg_->SetPosition(rx, ry);
        radarBg_->SetSize(rSize, rSize);
        radarBg_->SetColor(Color(0.015f, 0.035f, 0.065f, 0.78f));

        int b = Max(1, S(2.0f));
        BorderImage* topE = radarBg_->CreateChild<BorderImage>(); topE->SetPosition(0, 0); topE->SetSize(rSize, b); topE->SetColor(Color(0.22f, 0.85f, 1.0f, 0.65f));
        BorderImage* botE = radarBg_->CreateChild<BorderImage>(); botE->SetPosition(0, rSize - b); botE->SetSize(rSize, b); botE->SetColor(Color(0.22f, 0.85f, 1.0f, 0.65f));
        BorderImage* lftE = radarBg_->CreateChild<BorderImage>(); lftE->SetPosition(0, 0); lftE->SetSize(b, rSize); lftE->SetColor(Color(0.22f, 0.85f, 1.0f, 0.65f));
        BorderImage* rgtE = radarBg_->CreateChild<BorderImage>(); rgtE->SetPosition(rSize - b, 0); rgtE->SetSize(b, rSize); rgtE->SetColor(Color(0.22f, 0.85f, 1.0f, 0.65f));

        BorderImage* chH = radarBg_->CreateChild<BorderImage>(); chH->SetPosition(S(8), rSize / 2); chH->SetSize(rSize - S(16), 1); chH->SetColor(Color(0.2f, 0.7f, 0.9f, 0.22f));
        BorderImage* chV = radarBg_->CreateChild<BorderImage>(); chV->SetPosition(rSize / 2, S(8)); chV->SetSize(1, rSize - S(16)); chV->SetColor(Color(0.2f, 0.7f, 0.9f, 0.22f));

        radarSweep_ = radarBg_->CreateChild<BorderImage>();
        radarSweep_->SetSize(Max(2, S(2.0f)), rSize / 2 - S(8));
        radarSweep_->SetColor(Color(0.25f, 1.0f, 0.80f, 0.35f));

        BorderImage* selfDot = radarBg_->CreateChild<BorderImage>();
        int sd = S(6.0f);
        selfDot->SetPosition(rSize / 2 - sd / 2, rSize / 2 - sd / 2);
        selfDot->SetSize(sd, sd);
        selfDot->SetColor(Color(0.35f, 1.0f, 0.95f, 1.0f));

        Text* rTitle = radarBg_->CreateChild<Text>();
        rTitle->SetFont(font_, SF(11));
        rTitle->SetText("РАДАР 110М");
        rTitle->SetPosition(S(6), rSize - S(16));
        rTitle->SetColor(Color(0.45f, 0.88f, 1.0f, 0.85f));

        for (int i = 0; i < 36; ++i) {
            BorderImage* blip = radarBg_->CreateChild<BorderImage>();
            blip->SetSize(S(5.0f), S(5.0f));
            blip->SetVisible(false);
            radarBlips_.Push(SharedPtr<BorderImage>(blip));
        }
    }

    void UpdateRadar() {
        if (!radarBg_ || !radarBg_->IsVisible() || !cameraNode_) return;
        int rSize = radarBg_->GetWidth();
        float halfR = rSize * 0.5f - S(8.0f);
        const float maxWorldR = 110.0f;
        Vector3 camPos = cameraNode_->GetPosition();
        float yawRad = -yaw_ * 0.01745329f;
        float cs = cosf(yawRad), sn = sinf(yawRad);

        if (radarSweep_) {
            float swAng = timeAcc_ * 2.6f;
            int sx = (int)(rSize * 0.5f + cosf(swAng) * halfR * 0.5f);
            int sy = (int)(rSize * 0.5f + sinf(swAng) * halfR * 0.5f);
            radarSweep_->SetPosition(sx, sy);
            radarSweep_->SetSize(S(5.0f), S(5.0f));
        }

        unsigned used = 0;
        auto placeBlip = [&](const Vector3& wp, const Color& col, int sz) {
            if (used >= radarBlips_.Size()) return;
            Vector3 d = wp - camPos;
            float rx =  d.x_ * cs - d.z_ * sn;
            float rz =  d.x_ * sn + d.z_ * cs;
            float dist = sqrtf(rx * rx + rz * rz);
            if (dist > maxWorldR) { rx = rx / dist * maxWorldR; rz = rz / dist * maxWorldR; }
            int px = (int)(rSize * 0.5f + (rx / maxWorldR) * halfR) - sz / 2;
            int py = (int)(rSize * 0.5f - (rz / maxWorldR) * halfR) - sz / 2;
            BorderImage* b = radarBlips_[used++];
            b->SetPosition(px, py);
            b->SetSize(sz, sz);
            b->SetColor(col);
            b->SetVisible(true);
        };

        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            if (!enemies_[i].alive_ || !enemies_[i].node_) continue;
            EnemyType et = enemies_[i].type_;
            Color ec = (et == E_BOSS)     ? Color(1.0f, 0.85f, 0.15f, 1.0f)
                     : (et == E_KAMIKAZE) ? Color(1.0f, 0.50f, 0.10f, 1.0f)
                     : (et == E_DRONE)    ? Color(0.35f, 0.90f, 1.0f, 1.0f)
                                          : Color(1.0f, 0.22f, 0.22f, 0.95f);
            placeBlip(enemies_[i].node_->GetPosition(), ec, (et == E_BOSS) ? S(9.0f) : S(5.5f));
        }
        for (unsigned i = 0; i < turrets_.Size(); ++i) {
            if (turrets_[i].ttl_ > 0.0f)
                placeBlip(turrets_[i].pos_, Color(0.15f, 1.0f, 0.72f, 1.0f), S(6.5f));
        }
        for (unsigned i = 0; i < pickups_.Size(); ++i) {
            if (pickups_[i].respawn_ > 0.0f) continue;
            Color pc = (pickups_[i].type_ == 0) ? Color(0.25f, 1.0f, 0.45f, 0.85f)
                     : (pickups_[i].type_ == 2) ? Color(0.25f, 0.85f, 1.0f, 0.85f)
                     : (pickups_[i].type_ == 3) ? Color(1.0f, 0.35f, 0.95f, 0.95f)
                                                : Color(1.0f, 0.85f, 0.25f, 0.80f);
            placeBlip(pickups_[i].home_, pc, S(4.5f));
        }
        for (unsigned i = used; i < radarBlips_.Size(); ++i)
            radarBlips_[i]->SetVisible(false);
    }

    int CountUnlockedAchievements() const {
        return Cyber::CountUnlockedAchievements(achievementsMask_);
    }

    void CreateAchievementToastHUD() {
        auto* root = GetSubsystem<UI>()->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int w = S(460), h = S(62);
        achievToastBg_ = root->CreateChild<BorderImage>("AchievToastBg");
        achievToastBg_->SetPosition(sw / 2 - w / 2, S(20));
        achievToastBg_->SetSize(w, h);
        achievToastBg_->SetColor(Color(0.08f, 0.06f, 0.01f, 0.92f));
        achievToastBg_->SetPriority(850);
        achievToastBg_->SetVisible(false);

        BorderImage* topLine = achievToastBg_->CreateChild<BorderImage>();
        topLine->SetPosition(0, 0);
        topLine->SetSize(w, Max(2, S(2.5f)));
        topLine->SetColor(Color(1.0f, 0.86f, 0.22f, 1.0f));

        achievToastText_ = achievToastBg_->CreateChild<Text>();
        achievToastText_->SetFont(font_, SF(15));
        achievToastText_->SetTextAlignment(HA_CENTER);
        achievToastText_->SetPosition(S(10), S(9));
        achievToastText_->SetWidth(w - S(20));
        achievToastText_->SetColor(Color(1.0f, 0.90f, 0.32f, 1.0f));
        StyleCrispText(achievToastText_, true);
    }

    void UnlockAchievement(int bitIdx, const String& title) {
        if (bitIdx < 0 || bitIdx >= Cyber::TOTAL_ACHIEVEMENTS) return;
        if (achievementsMask_ & (1 << bitIdx)) return;
        achievementsMask_ |= (1 << bitIdx);
        player_.score_ += 250;
        credits_ += 50;
        SaveSettingsFile();
        UpdateScoreDisplay();
        PushKillFeed("🏆 ДОСТИЖЕНИЕ: " + title);
        if (soundSource_ && achievementSound_) {
            soundSource_->SetGain(SfxGain(0.92f));
            soundSource_->Play(achievementSound_);
        }
        if (achievToastBg_ && achievToastText_) {
            achievToastText_->SetText("🏆 ДОСТИЖЕНИЕ ОТКРЫТО (" + String(CountUnlockedAchievements()) + "/" +
                                      String(Cyber::TOTAL_ACHIEVEMENTS) + ")\n" + title + "  [+250 ОЧК / +50 КР]");
            achievToastBg_->SetVisible(true);
            achievToastTimer_ = 4.2f;
        }
    }

    void UpdateAchievementToast(float dt) {
        if (achievToastTimer_ <= 0.0f) return;
        achievToastTimer_ -= dt;
        if (achievToastBg_) {
            if (achievToastTimer_ <= 0.0f) {
                achievToastBg_->SetVisible(false);
            } else {
                float a = Clamp(achievToastTimer_ / 0.6f, 0.0f, 1.0f);
                achievToastBg_->SetOpacity(a);
            }
        }
    }

    void CreateTelemetryHUD() {
        auto* root = GetSubsystem<UI>()->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        telemetryBg_ = root->CreateChild<BorderImage>("TelemetryBg");
        telemetryBg_->SetPosition(sw - S(350), S(92));
        telemetryBg_->SetSize(S(330), S(142));
        telemetryBg_->SetColor(Color(0.01f, 0.025f, 0.045f, 0.80f));
        telemetryBg_->SetVisible(showTelemetry_);

        telemetryText_ = telemetryBg_->CreateChild<Text>();
        telemetryText_->SetFont(font_, SF(13));
        telemetryText_->SetPosition(S(10), S(8));
        telemetryText_->SetColor(Color(0.55f, 0.95f, 1.0f, 0.95f));
        StyleCrispText(telemetryText_);
    }

    void UpdateTelemetry(float dt) {
        if (!telemetryBg_ || !telemetryText_) return;
        bool vis = showTelemetry_ && !loadingScreenActive_;
        telemetryBg_->SetVisible(vis);
        if (!vis) return;
        auto* gfx = GetSubsystem<Graphics>();
        auto* ren = GetSubsystem<Renderer>();
        int fps = dt > 1e-5f ? (int)(1.0f / dt + 0.5f) : 0;
        float ms = dt * 1000.0f;
        static const char* qNames[4] = { "LOW", "MED", "HIGH PBR", "4K ULTRA PBR" };
        String info =
            "4K TELEMETRY [F3]\n"
            "FPS: " + String(fps) + "  (" + String(ms, 1) + " ms)\n"
            "RES: " + String(gfx->GetWidth()) + "x" + String(gfx->GetHeight()) +
            "  MSAA: " + String(gfx->GetMultiSample()) + "x\n"
            "PRESET: " + String(qNames[Clamp(gfxQualityIdx_, 0, 3)]) + "\n"
            "POST: " + String(gfxSsao_ ? "SSAO4K " : "") + String(gfxGlowIdx_ > 0 ? "BLOOM " : "") +
            String("ACES ") + String(gfxSharpen_ ? "CAS4K " : "") + String(gfxFxaa_ ? "FXAA" : "") + "\n"
            "TRIS: " + String(ren ? ren->GetNumPrimitives() : 0) +
            "  BATCHES: " + String(ren ? ren->GetNumBatches() : 0);
        telemetryText_->SetText(info);
    }

    void SpawnFloatingDamage(const Vector3& worldPos, int dmg, bool crit) {
        if (dmg <= 0 || !font_) return;
        auto* root = GetSubsystem<UI>()->GetRoot();
        if (floatingDmgs_.Size() >= 24) {
            if (floatingDmgs_[0].label_) floatingDmgs_[0].label_->Remove();
            floatingDmgs_.Erase(0);
        }
        Text* t = root->CreateChild<Text>();
        t->SetFont(font_, crit ? SF(26) : SF(19));
        t->SetText(crit ? ("★ " + String(dmg) + " КРИТ!") : ("-" + String(dmg)));
        t->SetColor(crit ? Color(1.0f, 0.88f, 0.18f, 1.0f) : Color(1.0f, 0.96f, 0.92f, 0.95f));
        StyleCrispText(t, crit);

        FloatingDmg fd;
        fd.label_ = SharedPtr<Text>(t);
        fd.worldPos_ = worldPos;
        fd.vel_ = Vector3(((rand() % 100) - 50) * 0.012f, 1.55f + (crit ? 0.5f : 0.0f), 0.0f);
        fd.life_ = crit ? 1.05f : 0.75f;
        fd.maxLife_ = fd.life_;
        fd.crit_ = crit;
        floatingDmgs_.Push(fd);
    }

    void UpdateFloatingDamages(float dt) {
        Camera* cam = cameraNode_ ? cameraNode_->GetComponent<Camera>() : nullptr;
        auto* gfx = GetSubsystem<Graphics>();
        for (unsigned i = floatingDmgs_.Size(); i-- > 0; ) {
            FloatingDmg& fd = floatingDmgs_[i];
            fd.life_ -= dt;
            if (fd.life_ <= 0.0f || !fd.label_ || !cam || !gfx) {
                if (fd.label_) fd.label_->Remove();
                floatingDmgs_.Erase(i);
                continue;
            }
            fd.worldPos_ += fd.vel_ * dt;
            Vector3 toPt = fd.worldPos_ - cameraNode_->GetWorldPosition();
            if (toPt.DotProduct(cameraNode_->GetWorldDirection()) <= 0.1f) {
                fd.label_->SetVisible(false);
                continue;
            }
            Vector2 sp = cam->WorldToScreenPoint(fd.worldPos_);
            int px = (int)(sp.x_ * gfx->GetWidth()) - fd.label_->GetWidth() / 2;
            int py = (int)(sp.y_ * gfx->GetHeight()) - fd.label_->GetHeight() / 2;
            fd.label_->SetPosition(px, py);
            fd.label_->SetVisible(!inMenu_ && !paused_ && !photoMode_);
            float a = Clamp(fd.life_ / (fd.maxLife_ * 0.45f), 0.0f, 1.0f);
            Color c = fd.crit_ ? Color(1.0f, 0.88f, 0.18f, a) : Color(1.0f, 0.96f, 0.92f, a);
            fd.label_->SetColor(c);
        }
    }
    
    void CreateHUD() {
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();
        int barW = S(HUD_BAR_W);
        int barH = S(10.0f);
        
        ammoText_ = root->CreateChild<Text>();
        ammoText_->SetFont(font_, SF(24));
        ammoText_->SetTextAlignment(HA_RIGHT);
        ammoText_->SetPosition(sw - S(24), sh - S(74));
        ammoText_->SetColor(Color(1.0f, 0.90f, 0.25f, 1.0f));
        StyleCrispText(ammoText_);
        
        healthText_ = root->CreateChild<Text>();
        healthText_->SetFont(font_, SF(23));
        healthText_->SetPosition(S(24), sh - S(86));
        healthText_->SetColor(Color(0.25f, 1.0f, 0.45f, 1.0f));
        StyleCrispText(healthText_);
        
        scoreText_ = root->CreateChild<Text>();
        scoreText_->SetFont(font_, SF(23));
        scoreText_->SetTextAlignment(HA_RIGHT);
        scoreText_->SetPosition(sw - S(24), S(18));
        scoreText_->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        StyleCrispText(scoreText_);
        
        accuracyText_ = root->CreateChild<Text>();
        accuracyText_->SetFont(font_, SF(16));
        accuracyText_->SetTextAlignment(HA_RIGHT);
        accuracyText_->SetPosition(sw - S(24), S(68));
        accuracyText_->SetColor(Color(0.68f, 0.88f, 1.0f, 0.95f));
        StyleCrispText(accuracyText_);
        
        messageText_ = root->CreateChild<Text>();
        messageText_->SetFont(font_, SF(20));
        messageText_->SetTextAlignment(HA_CENTER);
        messageText_->SetPosition(sw / 2 - S(480), sh / 2 + S(65));
        messageText_->SetWidth(S(960));
        messageText_->SetWordwrap(true);
        messageText_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
        StyleCrispText(messageText_);
        messageText_->SetEnabled(false);

        hudPanelBg_ = root->CreateChild<BorderImage>("HudWeaponPanel");
        hudPanelBg_->SetSize(S(320), S(50));
        hudPanelBg_->SetPosition(sw / 2 - S(160), sh - S(72));
        hudPanelBg_->SetColor(Color(0.02f, 0.045f, 0.08f, 0.72f));

        weaponNameText_ = root->CreateChild<Text>();
        weaponNameText_->SetFont(font_, SF(17));
        weaponNameText_->SetTextAlignment(HA_CENTER);
        weaponNameText_->SetPosition(sw / 2 - S(155), sh - S(64));
        weaponNameText_->SetWidth(S(310));
        weaponNameText_->SetColor(Color(1.0f, 0.86f, 0.30f, 1.0f));
        StyleCrispText(weaponNameText_);
        weaponNameText_->SetText(String(WEAPON_DEFS[currentWeapon_].name) + "  •  " + String(SKIN_DEFS[currentSkin_].name));

        hudDashBarBg_ = root->CreateChild<BorderImage>("DashBarBg");
        hudDashBarBg_->SetSize(S(240), S(5));
        hudDashBarBg_->SetPosition(sw / 2 - S(120), sh - S(18));
        hudDashBarBg_->SetColor(Color(0.05f, 0.08f, 0.12f, 0.70f));

        hudDashBarFill_ = root->CreateChild<BorderImage>("DashBarFill");
        hudDashBarFill_->SetSize(S(240), S(5));
        hudDashBarFill_->SetPosition(sw / 2 - S(120), sh - S(18));
        hudDashBarFill_->SetColor(Color(0.25f, 0.92f, 1.0f, 0.95f));

        statusBuffText_ = root->CreateChild<Text>();
        statusBuffText_->SetFont(font_, SF(16));
        statusBuffText_->SetTextAlignment(HA_CENTER);
        statusBuffText_->SetPosition(sw / 2 - S(260), sh - S(100));
        statusBuffText_->SetWidth(S(520));
        statusBuffText_->SetColor(Color(1.0f, 0.45f, 0.95f, 1.0f));
        StyleCrispText(statusBuffText_, true);

        waveBanner_ = root->CreateChild<Text>();
        waveBanner_->SetFont(font_, SF(52));
        waveBanner_->SetTextAlignment(HA_CENTER);
        waveBanner_->SetPosition(sw / 2 - S(450), (int)(sh * 0.20f));
        waveBanner_->SetWidth(S(900));
        waveBanner_->SetColor(Color(1.0f, 0.85f, 0.25f, 0.0f));
        StyleCrispText(waveBanner_, true);
        waveBanner_->SetEnabled(false);

        bossBarBg_ = root->CreateChild<BorderImage>("BossBarBg");
        bossBarBg_->SetSize(S(640), S(20));
        bossBarBg_->SetPosition(sw / 2 - S(320), S(28));
        bossBarBg_->SetColor(Color(0.05f, 0.02f, 0.03f, 0.82f));

        bossBarFill_ = root->CreateChild<BorderImage>("BossBarFill");
        bossBarFill_->SetSize(S(640), S(20));
        bossBarFill_->SetPosition(sw / 2 - S(320), S(28));
        bossBarFill_->SetColor(Color(0.95f, 0.22f, 0.25f, 1.0f));

        bossBarText_ = root->CreateChild<Text>();
        bossBarText_->SetFont(font_, SF(16));
        bossBarText_->SetTextAlignment(HA_CENTER);
        bossBarText_->SetPosition(sw / 2 - S(320), S(6));
        bossBarText_->SetWidth(S(640));
        bossBarText_->SetText("КИБЕР-ИНДЮК БОСС");
        bossBarText_->SetColor(Color(1.0f, 0.92f, 0.92f, 1.0f));
        StyleCrispText(bossBarText_);

        killFeedText_ = root->CreateChild<Text>();
        killFeedText_->SetFont(font_, SF(15));
        killFeedText_->SetPosition(S(24), S(210));
        killFeedText_->SetWidth(S(540));
        killFeedText_->SetWordwrap(false);
        killFeedText_->SetColor(Color(0.95f, 0.95f, 0.82f, 0.90f));
        StyleCrispText(killFeedText_);

        hudHpBarBg_ = root->CreateChild<BorderImage>("HpBarBg");
        hudHpBarBg_->SetSize(barW, barH);
        hudHpBarBg_->SetPosition(S(24), sh - S(30));
        hudHpBarBg_->SetColor(Color(0.04f, 0.06f, 0.10f, 0.78f));

        hudHpBarFill_ = root->CreateChild<BorderImage>("HpBarFill");
        hudHpBarFill_->SetSize(barW, barH);
        hudHpBarFill_->SetPosition(S(24), sh - S(30));
        hudHpBarFill_->SetColor(Color(0.22f, 0.96f, 0.42f, 1.0f));

        hudArmorBarBg_ = root->CreateChild<BorderImage>("ArmorBarBg");
        hudArmorBarBg_->SetSize(barW, S(7.0f));
        hudArmorBarBg_->SetPosition(S(24), sh - S(44));
        hudArmorBarBg_->SetColor(Color(0.04f, 0.06f, 0.12f, 0.78f));

        hudArmorBarFill_ = root->CreateChild<BorderImage>("ArmorBarFill");
        hudArmorBarFill_->SetSize(barW / 2, S(7.0f));
        hudArmorBarFill_->SetPosition(S(24), sh - S(44));
        hudArmorBarFill_->SetColor(Color(0.24f, 0.82f, 1.0f, 1.0f));

        hudAmmoBarBg_ = root->CreateChild<BorderImage>("AmmoBarBg");
        hudAmmoBarBg_->SetSize(barW, barH);
        hudAmmoBarBg_->SetPosition(sw - S(24) - barW, sh - S(30));
        hudAmmoBarBg_->SetColor(Color(0.04f, 0.06f, 0.10f, 0.78f));

        hudAmmoBarFill_ = root->CreateChild<BorderImage>("AmmoBarFill");
        hudAmmoBarFill_->SetSize(barW, barH);
        hudAmmoBarFill_->SetPosition(sw - S(24) - barW, sh - S(30));
        hudAmmoBarFill_->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));

        helpText_ = root->CreateChild<Text>();
        helpText_->SetFont(font_, SF(13));
        helpText_->SetText("[ALT/X] Рывок  •  [V] Удар крылом  •  [L] Лазер  •  [G] Осмотр  •  [TAB] Арсенал  •  [F2] Фото 4K  •  [F3] GPU  •  [F1] Справка");
        helpText_->SetPosition(S(24), S(182));
        helpText_->SetColor(Color(0.75f, 0.88f, 0.98f, 0.82f));
        StyleCrispText(helpText_);

        UpdateAmmoDisplay();
        UpdateHealthDisplay();
        UpdateScoreDisplay();
        UpdateAccuracyDisplay();
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
        bool show = (boss != nullptr) && !inMenu_ && !gameOver_ && !photoMode_;
        bossBarBg_->SetVisible(show);
        bossBarFill_->SetVisible(show);
        if (bossBarText_) bossBarText_->SetVisible(show);
        if (!show) return;
        float ratio = boss->maxHealth_ > 0.0f ? Clamp(boss->health_ / boss->maxHealth_, 0.0f, 1.0f) : 0.0f;
        bossBarFill_->SetSize((int)(S(640.0f) * ratio), S(20.0f));
        bossBarFill_->SetColor(ratio > 0.5f ? Color(0.95f, 0.30f, 0.25f, 1.0f)
                              : ratio > 0.25f ? Color(1.0f, 0.60f, 0.15f, 1.0f)
                                             : Color(1.0f, 0.20f, 0.20f, 1.0f));
        if (bossBarText_) bossBarText_->SetText("КИБЕР-ИНДЮК БОСС   " + String((int)boss->health_) + " / " + String((int)boss->maxHealth_));
    }

    void UpdateAmmoDisplay() {
        if (ammoText_) {
            String ammoStr = "ПАТРОНЫ: " + String(weapon_.currentAmmo_) + " / " + String(weapon_.reserveAmmo_);
            if (weapon_.isReloading_) ammoStr += "  [ПЕРЕЗАРЯДКА]";
            ammoText_->SetText(ammoStr);
            ammoText_->SetColor(weapon_.currentAmmo_ <= 5 ? Color(1.0f, 0.22f, 0.20f, 1.0f)
                                                          : Color(1.0f, 0.90f, 0.25f, 1.0f));
        }
        if (hudAmmoBarFill_) {
            float ratio = WEAPON_DEFS[currentWeapon_].magSize > 0
                        ? Clamp((float)weapon_.currentAmmo_ / (float)WEAPON_DEFS[currentWeapon_].magSize, 0.0f, 1.0f)
                        : 0.0f;
            hudAmmoBarFill_->SetSize((int)(S(HUD_BAR_W) * ratio), S(10.0f));
            hudAmmoBarFill_->SetColor(ratio < 0.25f ? Color(1.0f, 0.25f, 0.2f, 1.0f)
                                                    : Color(1.0f, 0.85f, 0.25f, 1.0f));
        }
    }
    
    void UpdateHealthDisplay() {
        if (healthText_) {
            String healthStr = "HP: " + String((int)player_.health_) + "  |  БРОНЯ: " + String((int)player_.armor_);
            healthText_->SetText(healthStr);
            
            float healthRatio = player_.maxHealth_ > 0.0f ? player_.health_ / player_.maxHealth_ : 0.0f;
            if (healthRatio > 0.6f) {
                healthText_->SetColor(Color(0.25f, 1.0f, 0.45f, 1.0f));
            } else if (healthRatio > 0.3f) {
                healthText_->SetColor(Color(1.0f, 0.90f, 0.22f, 1.0f));
            } else {
                healthText_->SetColor(Color(1.0f, 0.22f, 0.20f, 1.0f));
            }
        }
        if (hudHpBarFill_) {
            float ratio = player_.maxHealth_ > 0.0f ? Clamp(player_.health_ / player_.maxHealth_, 0.0f, 1.0f) : 0.0f;
            hudHpBarFill_->SetSize((int)(S(HUD_BAR_W) * ratio), S(10.0f));
            hudHpBarFill_->SetColor(ratio > 0.6f ? Color(0.22f, 0.96f, 0.42f, 1.0f)
                                   : ratio > 0.3f ? Color(1.0f, 0.85f, 0.2f, 1.0f)
                                                  : Color(1.0f, 0.25f, 0.2f, 1.0f));
        }
        if (hudArmorBarFill_) {
            float ar = player_.maxArmor_ > 0.0f ? Clamp(player_.armor_ / player_.maxArmor_, 0.0f, 1.0f) : 0.0f;
            hudArmorBarFill_->SetSize((int)(S(HUD_BAR_W) * ar), S(7.0f));
        }
    }

    void UpdateDashAndBuffHUD() {
        float dashMaxCd = 2.2f * powf(0.86f, (float)perkSpdLvl_);
        if (hudDashBarFill_) {
            float ready = dashCooldown_ <= 0.0f ? 1.0f : Clamp(1.0f - dashCooldown_ / Max(0.4f, dashMaxCd), 0.0f, 1.0f);
            hudDashBarFill_->SetSize((int)(S(240.0f) * ready), S(5.0f));
            hudDashBarFill_->SetColor(ready >= 0.999f ? Color(0.25f, 0.95f, 1.0f, 0.95f)
                                                      : Color(0.35f, 0.55f, 0.72f, 0.75f));
        }
        if (statusBuffText_) {
            String b;
            if (banyaBuffTier_ >= 2)      b += "👑 СМОТРЯЩИЙ (+50HP/+15%УР)  ";
            else if (banyaBuffTier_ == 1) b += "♨ БАННЫЙ ЗАКАЛ (+25HP)  ";
            if (overdriveTimer_ > 0.0f)
                b += "⚡ ОВЕРДРАЙВ 2X (" + String(overdriveTimer_, 1) + "с)  ";
            b += (grappleCooldown_ <= 0.0f) ? "[Z] КРЮК  " : ("[Z] " + String(grappleCooldown_, 1) + "с  ");
            if (orbitalCharge_ >= 99.9f)
                b += "☢ [X] ЯЙЦЕГЕДДОН: ГОТОВ!  ";
            else
                b += "[X] ОРБИТА " + String((int)orbitalCharge_) + "%  ";
            b += (grenadeCooldown_ <= 0.0f) ? "[G] ГРАНАТА  " : ("[G] " + String(grenadeCooldown_, 1) + "с  ");
            b += (turretCooldown_ <= 0.0f)  ? "[T] ТУРЕЛЬ (100₵)  " : ("[T] " + String((int)turretCooldown_) + "с  ");
            b += "[B] ПЕРКИ (" + String(credits_) + "₵)  [P] БАНЯ";
            statusBuffText_->SetText(b);
        }
    }
    
    void UpdateScoreDisplay() {
        if (scoreText_) {
            String s = "СЧЁТ: " + String(player_.score_) + "  |  ₵" + String(credits_) + "  |  УБИЙСТВ: " + String(player_.kills_);
            if (player_.headshots_ > 0) s += " (★" + String(player_.headshots_) + ")";
            if (comboCount_ > 1) s += "  |  КОМБО x" + String(comboCount_);
            s += "\nРЕКОРД: " + String(highScore_) + "  |  🏆 " + String(CountUnlockedAchievements()) + "/" + String(Cyber::TOTAL_ACHIEVEMENTS);
            if (gameMode_ == MODE_ARENA) {
                s += "  |  ВОЛНА " + String(waveNum_);
                if (activeMutator_ != Cyber::MUT_STANDARD)
                    s += " [" + String(Cyber::GetMutatorInfo(activeMutator_).name) + "]";
            }
            scoreText_->SetText(s);
            scoreText_->SetColor(comboCount_ > 1 ? Color(1.0f, 0.65f, 0.15f, 1.0f) : Color(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    
    void UpdateAccuracyDisplay() {
        if (accuracyText_) {
            accuracyText_->SetText("ТОЧНОСТЬ: " + String((int)player_.GetAccuracy()) + "%");
        }
    }
    
    void ShowMessage(const String& message) {
        if (messageText_) {
            messageText_->SetText(message);
            messageText_->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
            messageText_->SetEnabled(true);
            messageTimer_ = 4.5f;
        }
    }

    void UpdateCrosshairSpread() {
        if (!crosshairCenter_ || !crosshairTop_ || !crosshairBottom_ || !crosshairLeft_ || !crosshairRight_) return;
        auto* gfx = GetSubsystem<Graphics>();
        if (!gfx) return;
        const int cx = gfx->GetWidth() / 2;
        const int cy = gfx->GetHeight() / 2;
        const int th = Max(2, S(2.0f));
        const int len = S(10.0f);

        float spread = WEAPON_DEFS[currentWeapon_].spread;
        spread *= (1.0f - player_.GetAccuracy() / 200.0f);
        if (isAiming_) spread *= 0.35f;
        if (isCrouching_) spread *= 0.7f;
        if (laserOn_) spread *= 0.78f;
        if (!onGround_) spread *= 1.6f;

        float gap = (10.0f + spread * 780.0f) * uiScale_;
        if (fabsf(gap - lastCrosshairGap_) < 0.5f) return;
        lastCrosshairGap_ = gap;
        int g = (int)gap;
        crosshairTop_->SetPosition(cx - th / 2, cy - g - len);
        crosshairBottom_->SetPosition(cx - th / 2, cy + g);
        crosshairLeft_->SetPosition(cx - g - len, cy - th / 2);
        crosshairRight_->SetPosition(cx + g, cy - th / 2);
    }

    // =========================================================================
    // ПАУЗА И СПРАВКА (DPI-масштаб 4K UHD)
    // =========================================================================

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
            bg->SetColor(Color(0.01f, 0.02f, 0.04f, 0.90f));

            Text* title = helpPanel_->CreateChild<Text>();
            title->SetFont(font_, SF(40));
            title->SetText("CHIKENGUN 2.5 — ПЕРО СУДЬБЫ: СПРАВКА");
            title->SetTextAlignment(HA_CENTER);
            title->SetPosition(sw / 2 - S(540), sh / 2 - S(280));
            title->SetWidth(S(1080));
            title->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));
            StyleCrispText(title, true);

            Text* body = helpPanel_->CreateChild<Text>();
            body->SetFont(font_, SF(18));
            body->SetPosition(sw / 2 - S(520), sh / 2 - S(215));
            body->SetWidth(S(1040));
            body->SetWordwrap(false);
            body->SetColor(Color(0.90f, 0.94f, 0.98f, 1.0f));
            StyleCrispText(body);
            body->SetText(
                "БОЕВОЕ УПРАВЛЕНИЕ 2.5 (ПЕРО СУДЬБЫ + КОЧЕТ В БАНЬКЕ)\n"
                "  WASD — движение      SHIFT — бег        SPACE — прыжок       CTRL / C — присед\n"
                "  ALT — рывок          Z — грэппл-крюк    X — орбита ЯЙЦЕГЕДДОН P — БАНЬКА СМОТРЯЩЕГО\n"
                "  V — удар крылом      G — яйце-граната   T — авто-турель      B — магазин перков\n"
                "  ЛКМ / ПКМ — огонь    R — перезарядка    1..0 — 12 стволов   [ / ] — листать\n"
                "  Q / E — PBR-скин     L — 3D лазер       I — осмотр оружия    TAB — арсенал\n"
                "  F — Тир/Арена        F2 — 4K фоторежим  F3 — телеметрия      F8 — атмосфера 4K\n"
                "\n"
                "ТАКТИКА, БАНЬКА СМОТРЯЩЕГО [P] И ПЕРКИ\n"
                "  • [P] «КОЧЕТ В БАНЬКЕ»: продержись 3 минуты и разгадай загадки смотрящего (14 концовок, 57 загадок)!\n"
                "    Выход с зачётом даёт кредиты (₵) и бафф «Банный закал» (+25 HP/Брони) или «Ковш Смотрящего» (+50 HP, +15% урона)!\n"
                "  • [8] RAILGUN прошивает всех! [9] PLASMA рикошетит. [0] ARC CANNON цепляет стаю.\n"
                "  • CRYO LANCE вращает охлаждающие кольца, ION SAW крутит энергетический диск.\n"
                "  • Встань на ДЖАМП-ПЛАТФОРМУ или зацепись крюком [Z] для аэро-фрагов с высоты!\n"
                "  • Копи кредиты (₵) за убийства и парение в баньке — качай перки в лаборатории [B]!");

            Text* hint = helpPanel_->CreateChild<Text>();
            hint->SetFont(font_, SF(18));
            hint->SetText("F1 или ESC — закрыть справку");
            hint->SetTextAlignment(HA_CENTER);
            hint->SetPosition(sw / 2 - S(340), sh / 2 + S(255));
            hint->SetWidth(S(680));
            hint->SetColor(Color(1.0f, 0.85f, 0.3f, 1.0f));
            StyleCrispText(hint);
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

    int GetPerkCost(int lvl) const {
        return 75 + lvl * 65;
    }

    void UpdatePerkShopDisplay() {
        if (!perkShopBody_) return;
        auto fmtRow = [&](int keyNum, const char* name, int lvl, const char* desc) -> String {
            String costStr = (lvl >= 5) ? "[МАКС. УР. 5/5]" : ("Цена: " + String(GetPerkCost(lvl)) + "₵  (Ур. " + String(lvl) + "/5)");
            return "  [" + String(keyNum) + "]  " + String(name) + "   —   " + costStr + "\n        " + String(desc) + "\n\n";
        };
        String txt = "ДОСТУПНО КИБЕР-КРЕДИТОВ:  " + String(credits_) + " ₵\n\n";
        txt += fmtRow(1, "ТИТАНОВОЕ ОПЕРЕНИЕ", perkHpLvl_,  "+25 к макс. HP и мгновенное лечение +40 HP");
        txt += fmtRow(2, "УСКОРЕННЫЙ ЗАТВОР",  perkRofLvl_, "+12% скорострельности и скорости перезарядки");
        txt += fmtRow(3, "КАЛИБР ВОЛЬФРАМ",    perkDmgLvl_, "+14% урона всему оружию, гранатам и авто-турелям");
        txt += fmtRow(4, "ТУРБО-ПРИВОДЫ",      perkSpdLvl_, "+8% скорости бега и -14% кулдауна рывка/гранаты");
        perkShopBody_->SetText(txt);
    }

    void CreatePerkShop() {
        auto* root = GetSubsystem<UI>()->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        perkShopPanel_ = root->CreateChild<UIElement>("PerkShopPanel");
        perkShopPanel_->SetSize(sw, sh);
        perkShopPanel_->SetPriority(820);

        BorderImage* bg = perkShopPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.008f, 0.020f, 0.042f, 0.88f));

        BorderImage* card = perkShopPanel_->CreateChild<BorderImage>();
        int cw = S(880), ch = S(470);
        card->SetPosition(sw / 2 - cw / 2, sh / 2 - ch / 2);
        card->SetSize(cw, ch);
        card->SetColor(Color(0.015f, 0.032f, 0.065f, 0.94f));

        BorderImage* topLine = perkShopPanel_->CreateChild<BorderImage>();
        topLine->SetPosition(sw / 2 - cw / 2, sh / 2 - ch / 2);
        topLine->SetSize(cw, Max(2, S(3.0f)));
        topLine->SetColor(Color(0.20f, 0.95f, 0.82f, 1.0f));

        Text* title = perkShopPanel_->CreateChild<Text>();
        title->SetFont(font_, SF(34));
        title->SetText("ЛАБОРАТОРИЯ КИБЕР-ПЕРКОВ [B]");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - S(420), sh / 2 - ch / 2 + S(22));
        title->SetWidth(S(840));
        title->SetColor(Color(1.0f, 0.86f, 0.25f, 1.0f));
        StyleCrispText(title, true);

        perkShopBody_ = perkShopPanel_->CreateChild<Text>();
        perkShopBody_->SetFont(font_, SF(18));
        perkShopBody_->SetPosition(sw / 2 - cw / 2 + S(44), sh / 2 - ch / 2 + S(88));
        perkShopBody_->SetWidth(cw - S(88));
        perkShopBody_->SetColor(Color(0.88f, 0.95f, 1.0f, 1.0f));
        StyleCrispText(perkShopBody_);

        Text* hint = perkShopPanel_->CreateChild<Text>();
        hint->SetFont(font_, SF(17));
        hint->SetText("Нажмите 1..4 для покупки улучшения   |   B или ESC — вернуться в бой");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - S(420), sh / 2 + ch / 2 - S(46));
        hint->SetWidth(S(840));
        hint->SetColor(Color(0.42f, 0.95f, 0.85f, 1.0f));
        StyleCrispText(hint);

        UpdatePerkShopDisplay();
        perkShopPanel_->SetVisible(false);
    }

    void TogglePerkShop() {
        if (inMenu_ || gameOver_ || loadingScreenActive_) return;
        perkShopOpen_ = !perkShopOpen_;
        if (perkShopOpen_) {
            if (!perkShopPanel_) CreatePerkShop();
            UpdatePerkShopDisplay();
            perkShopPanel_->SetVisible(true);
            SetGameHudVisible(false);
            PlayUISound();
        } else {
            if (perkShopPanel_) perkShopPanel_->SetVisible(false);
            SetGameHudVisible(!photoMode_);
            PlayUISound();
        }
    }

    void BuyPerk(int idx) {
        int* lvlPtr = (idx == 0) ? &perkHpLvl_
                    : (idx == 1) ? &perkRofLvl_
                    : (idx == 2) ? &perkDmgLvl_
                                 : &perkSpdLvl_;
        if (*lvlPtr >= 5) {
            PlayEmptyClickSound();
            return;
        }
        int cost = GetPerkCost(*lvlPtr);
        if (credits_ < cost) {
            PlayEmptyClickSound();
            return;
        }
        credits_ -= cost;
        (*lvlPtr)++;
        if (idx == 0) {
            player_.maxHealth_ += 25.0f;
            player_.health_ = Min(player_.maxHealth_, player_.health_ + 40.0f);
            UpdateHealthDisplay();
        }
        if (soundSource_ && perkBuySound_) {
            soundSource_->SetGain(SfxGain(0.88f));
            soundSource_->Play(perkBuySound_);
        }
        UpdatePerkShopDisplay();
        UpdateScoreDisplay();
        UpdateDashAndBuffHUD();
        if (perkHpLvl_ + perkRofLvl_ + perkDmgLvl_ + perkSpdLvl_ >= 5)
            UnlockAchievement(7, "КИБЕР-МУТАНТ (5 улучшений перков)");
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
        bg->SetColor(Color(0.01f, 0.02f, 0.04f, 0.78f));

        Text* title = pausePanel_->CreateChild<Text>();
        title->SetFont(font_, SF(64));
        title->SetText("ТАКТИЧЕСКАЯ ПАУЗА");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - S(460), sh / 2 - S(160));
        title->SetWidth(S(920));
        title->SetColor(Color(0.55f, 0.95f, 1.0f, 1.0f));
        StyleCrispText(title, true);

        BorderImage* bar = pausePanel_->CreateChild<BorderImage>();
        bar->SetPosition(sw / 2 - S(220), sh / 2 - S(65));
        bar->SetSize(S(440), S(3));
        bar->SetColor(Color(0.3f, 0.85f, 1.0f, 0.85f));

        Text* stats = pausePanel_->CreateChild<Text>();
        stats->SetFont(font_, SF(22));
        stats->SetTextAlignment(HA_CENTER);
        stats->SetPosition(sw / 2 - S(460), sh / 2 - S(30));
        stats->SetWidth(S(920));
        stats->SetColor(Color(0.92f, 0.95f, 0.98f, 1.0f));
        StyleCrispText(stats);
        stats->SetText("ВРЕМЯ: " + FormatTime(arenaTime_) + "\n" +
                       "СЧЁТ: " + String(player_.score_) + "   |   УБИЙСТВ: " + String(player_.kills_) +
                       " (★" + String(player_.headshots_) + ")   |   ТОЧНОСТЬ: " + String((int)player_.GetAccuracy()) + "%\n" +
                       "РЕЖИМ: " + String(gameMode_ == MODE_ARENA ? "АРЕНА" : "ТИР") +
                       (gameMode_ == MODE_ARENA ? ("   |   ВОЛНА: " + String(waveNum_)) : String()));

        Text* hint = pausePanel_->CreateChild<Text>();
        hint->SetFont(font_, SF(20));
        hint->SetText("ESC / ENTER — продолжить   |   P — БАНЬКА   |   M — в меню   |   Q — выход");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - S(480), sh / 2 + S(105));
        hint->SetWidth(S(960));
        hint->SetColor(Color(1.0f, 0.85f, 0.3f, 1.0f));
        StyleCrispText(hint);

        pausePanel_->SetVisible(false);
    }

    void TogglePause() {
        if (gameOver_ || inMenu_ || loadingScreenActive_) return;
        paused_ = !paused_;
        auto* input = GetSubsystem<Input>();
        if (paused_) {
            if (pausePanel_) { pausePanel_->Remove(); pausePanel_ = nullptr; }
            CreatePausePanel();
            if (pausePanel_) pausePanel_->SetVisible(true);
            SetGameHudVisible(false);
            if (input) input->SetMouseVisible(false);
        } else {
            if (pausePanel_) pausePanel_->SetVisible(false);
            SetGameHudVisible(!photoMode_);
        }
    }

    // =========================================================================
    // ПЕРЕСБОРКА UI ПРИ РЕСАЙЗЕ ОКНА ИЛИ СМЕНЕ 4K РАЗРЕШЕНИЯ
    // =========================================================================

    void RebuildUI() {
        if (rebuildingUI_ || loadingScreenActive_) return;
        rebuildingUI_ = true;

        RefreshUIScale();
        auto* ui = GetSubsystem<UI>();
        if (!ui) { rebuildingUI_ = false; return; }
        UIElement* root = ui->GetRoot();
        if (root) root->RemoveAllChildren();

        crosshairCenter_ = nullptr; crosshairTop_ = nullptr; crosshairBottom_ = nullptr;
        crosshairLeft_ = nullptr; crosshairRight_ = nullptr;
        ammoText_ = nullptr; healthText_ = nullptr; scoreText_ = nullptr;
        accuracyText_ = nullptr; messageText_ = nullptr;
        damageOverlay_ = nullptr;
        hitMarkerBars_.Clear();
        vignetteEdges_.Clear(); lowHealthVignette_ = nullptr;
        gameOverPanel_ = nullptr; gameOverStatsText_ = nullptr;
        gameOverTitle_ = nullptr; gameOverHint_ = nullptr;
        goBasePosSet_ = false;
        hudPanelBg_ = nullptr; helpText_ = nullptr; weaponNameText_ = nullptr;
        hudHpBarBg_ = nullptr; hudHpBarFill_ = nullptr;
        hudArmorBarBg_ = nullptr; hudArmorBarFill_ = nullptr;
        hudDashBarBg_ = nullptr; hudDashBarFill_ = nullptr;
        statusBuffText_ = nullptr;
        hudAmmoBarBg_ = nullptr; hudAmmoBarFill_ = nullptr;
        radarBg_ = nullptr; radarSweep_ = nullptr; radarBlips_.Clear();
        telemetryBg_ = nullptr; telemetryText_ = nullptr;
        floatingDmgs_.Clear();
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
        deployPanel_ = nullptr; deployTitleText_ = nullptr; deployTipText_ = nullptr;
        deployBarFill_ = nullptr; deployTimer_ = 0.0f;
        achievToastBg_ = nullptr; achievToastText_ = nullptr;
        perkShopPanel_ = nullptr; perkShopBody_ = nullptr;
        casinoPanel_ = nullptr; casinoWheelCard_ = nullptr; casinoSpinButton_ = nullptr;
        casinoBetButton_ = nullptr; casinoChipButton_ = nullptr; casinoNumberButton_ = nullptr;
        casinoTopupButton_ = nullptr; casinoCloseButton_ = nullptr;
        casinoTitleText_ = nullptr; casinoBalanceText_ = nullptr; casinoWheelText_ = nullptr;
        casinoBetText_ = nullptr; casinoResultText_ = nullptr; casinoStatsText_ = nullptr; casinoHelpText_ = nullptr;
        ResetBanyaUIPointers();

        CreateUI();
        CreateMainMenu();

        SetGameHudVisible(!inMenu_ && !gameOver_ && !paused_ && !perkShopOpen_ && !banyaOpen_ && !photoMode_);
        if (mainMenuPanel_) mainMenuPanel_->SetVisible(inMenu_ && !banyaOpen_ && !casinoOpen_);
        if (inSettings_) { CreateSettingsPanel(); if (settingsPanel_) settingsPanel_->SetVisible(true); }
        if (armoryOpen_) { CreateArmoryMenu(); if (armoryPanel_) armoryPanel_->SetVisible(true); }
        if (perkShopOpen_) { CreatePerkShop(); if (perkShopPanel_) perkShopPanel_->SetVisible(true); }
        if (banyaOpen_) { CreateBanyaPanel(); if (banyaPanel_) banyaPanel_->SetVisible(true); SyncBanyaVisuals(0.0f); }
        if (casinoOpen_) { CreateCasinoPanel(); if (casinoPanel_) casinoPanel_->SetVisible(true); UpdateCasinoVisuals(); }
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
        if (dt > 0.1f) dt = 0.1f;
        timeAcc_ += dt;

        // Пошаговая 4K-загрузка игры с живой отрисовкой прогресс-бара на GPU
        if (loadingScreenActive_) {
            if (splashActive_) UpdateSplashIntro(dt);
            else               UpdateBootLoading(dt);
            return;
        }

        // Ресайз окна или переключение разрешения (1080p <-> 1440p <-> 4K UHD)
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

        UpdateTelemetry(dt);
        UpdateDeployOverlay(dt);
        UpdateAchievementToast(dt);

        if (banyaOpen_) {
            UpdateBanyaSimulator(dt);
            return;
        }
        if (casinoOpen_) {
            UpdateCasino(dt);
            return;
        }

        if (paused_ || helpOpen_ || perkShopOpen_) return;

        float rawDt = dt;
        if (slowMoTimer_ > 0.0f) {
            slowMoTimer_ = Max(0.0f, slowMoTimer_ - rawDt);
            dt = rawDt * 0.40f;
        }

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

        auto* input = GetSubsystem<Input>();

        // Таймеры способностей 4K Ultra & Cyber Overload
        if (dashCooldown_ > 0.0f) dashCooldown_ = Max(0.0f, dashCooldown_ - dt);
        if (dashTimer_ > 0.0f) dashTimer_ = Max(0.0f, dashTimer_ - dt);
        if (meleeAnimTimer_ > 0.0f) meleeAnimTimer_ = Max(0.0f, meleeAnimTimer_ - dt);
        if (meleeCooldown_ > 0.0f) meleeCooldown_ = Max(0.0f, meleeCooldown_ - dt);
        if (grenadeCooldown_ > 0.0f) grenadeCooldown_ = Max(0.0f, grenadeCooldown_ - dt);
        if (turretCooldown_ > 0.0f) turretCooldown_ = Max(0.0f, turretCooldown_ - dt);
        if (grappleCooldown_ > 0.0f) grappleCooldown_ = Max(0.0f, grappleCooldown_ - dt);
        if (multiKillTimer_ > 0.0f) {
            multiKillTimer_ -= dt;
            if (multiKillTimer_ <= 0.0f) multiKillCount_ = 0;
        }
        if (inspectTimer_ > 0.0f) inspectTimer_ = Max(0.0f, inspectTimer_ - dt);
        if (overdriveTimer_ > 0.0f) {
            overdriveTimer_ = Max(0.0f, overdriveTimer_ - dt);
            if (overdriveTimer_ <= 0.0f) ShowMessage("Кибер-овердрайв завершён");
        }
        UpdateDashAndBuffHUD();
        
        if (messageTimer_ > 0) {
            messageTimer_ -= dt;
            if (messageTimer_ <= 0 && messageText_) {
                messageText_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
                messageText_->SetEnabled(false);
            }
        }
        
        if (damageOverlay_) {
            if (damageFlashTimer_ > 0) {
                damageFlashTimer_ -= dt;
                float alpha = damageFlashTimer_ / 0.3f;
                damageOverlay_->SetColor(Color(1.0f, 0.0f, 0.0f, alpha * 0.5f));
            } else {
                damageOverlay_->SetColor(Color(1.0f, 0.0f, 0.0f, 0.0f));
            }
        }
        
        if (weapon_.isReloading_) {
            float rofMul = (1.0f + 0.12f * (float)perkRofLvl_) * (overdriveTimer_ > 0.0f ? 1.6f : 1.0f);
            weapon_.reloadTimer_ -= dt * rofMul;
            if (weapon_.reloadTimer_ <= 0) CompleteReload();
        }
        
        if (weapon_.fireTimer_ > 0) weapon_.fireTimer_ -= dt;

        if (!inMenu_ && !gameOver_ && !paused_ && !helpOpen_ && !armoryOpen_ && !perkShopOpen_ && !photoMode_ &&
            !isMouseVisible_ && input->GetMouseButtonDown(MOUSEB_LEFT)) {
            int wt = currentWeapon_;
            if (wt != W_SHOTGUN && wt != W_SNIPER && wt != W_LAUNCHER && wt != W_RAILGUN && wt != W_ARC) {
                TryFire();
            }
        }

        minigunSpinupActive_ = (currentWeapon_ == W_MINIGUN && weapon_.fireTimer_ > 0.0f);
        
        weapon_.recoilX_ = Lerp(weapon_.recoilX_, 0.0f, GameConstants::RECOIL_RECOVERY * dt);
        weapon_.recoilY_ = Lerp(weapon_.recoilY_, 0.0f, GameConstants::RECOIL_RECOVERY * dt);
        
        if (!gameOver_ && !inMenu_) HandleMovement(dt);

        UpdateWeaponBob(dt);
        ApplyRecoilToWeapon();
        UpdateWeaponAnimation(dt);
        UpdateLaserSight();

        if (muzzleFlashTimer_ > 0.0f) {
            muzzleFlashTimer_ -= dt;
            if (muzzleFlashTimer_ <= 0.0f && muzzleLightNode_) muzzleLightNode_->SetEnabled(false);
        }

        UpdateCrosshairSpread();
        UpdateHitMarker(dt);
        UpdateLowHealthVignette(dt);
        UpdateTracers(dt);
        UpdatePlasmaBolts(dt);
        UpdateThrownGrenades(dt);
        UpdateTurrets(dt);
        UpdateJumpPads(dt);
        UpdateGrapple(dt);
        UpdateOrbitalStrikes(dt);
        UpdateWeatherParticles(dt);
        UpdateEnemyProjectiles(dt);
        UpdateMovingTargetsLite(dt);
        UpdateDebrisLite(dt);
        UpdateAnimatedDecor(dt);
        UpdateExplosiveBarrels(dt);
        UpdateShockwaves(dt);
        UpdateKillFeed(dt);
        UpdateWaveBanner(dt);
        UpdateBossBar();
        UpdateRadar();
        UpdateFloatingDamages(dt);
        if (!inMenu_ && !gameOver_ && gameMode_ == MODE_ARENA) arenaTime_ += dt;
        UpdatePickups(dt);

        // Подсказка у входа в 3D «Баньку Смотрящего»
        if (!inMenu_ && !gameOver_ && cameraNode_) {
            if (banyaPromptCooldown_ > 0.0f) banyaPromptCooldown_ -= dt;
            Vector3 ppos = cameraNode_->GetPosition();
            float dBanya = Vector3(ppos.x_ - banyaDoorPos_.x_, 0.0f, ppos.z_ - banyaDoorPos_.z_).Length();
            if (dBanya < 5.4f && banyaPromptCooldown_ <= 0.0f) {
                banyaPromptCooldown_ = 6.5f;
                ShowMessage("♨ БАНЬКА СМОТРЯЩЕГО — нажмите [P], чтобы попариться (бафф + ₵)!");
            }
        }

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
        using namespace PostUpdate;
        (void)eventType;
        (void)eventData;
        // Scene/Octree уже закончили кадр: безопасно чистим узлы, удалённые
        // эффектами, и только теперь компактифицируем Lite-коллайдеры.
        PruneTransientEffectsAfterSceneUpdate();
        if (colliderGarbage_ > 64 ||
            (colliderGarbage_ > 0 && colliderGarbage_ * 2 >= (int)colliders_.Size()))
            CompactColliders();
    }
    
    void HandleMouseMove(StringHash eventType, VariantMap& eventData) {
        using namespace MouseMove;
        (void)eventType; (void)eventData;
        if (loadingScreenActive_ || isMouseVisible_ || banyaOpen_ || paused_ || helpOpen_ || perkShopOpen_) return;
        
        auto* input = GetSubsystem<Input>();
        IntVector2 mouseMove = input->GetMouseMove();
        
        static const float kSensMult[3] = { 0.55f, 1.0f, 1.65f };
        float sens = GameConstants::MOUSE_SENSITIVITY * kSensMult[Clamp(mouseSensIdx_, 0, 2)];
        if (isAiming_) sens *= 0.45f;
        if (isCrouching_) sens *= 0.8f;
        yaw_ += (float)mouseMove.x_ * sens;
        pitch_ += (float)mouseMove.y_ * sens;
        pitch_ = Clamp(pitch_, -GameConstants::MAX_PITCH, GameConstants::MAX_PITCH);
    }

    void Save4KScreenshot() {
        auto* gfx = GetSubsystem<Graphics>();
        auto* fs  = GetSubsystem<FileSystem>();
        if (!gfx || !fs) return;
        String dir = fs->GetProgramDir() + "Screenshots/";
        if (!fs->DirExists(dir)) fs->CreateDir(dir);
        Image img(context_);
        if (gfx->TakeScreenShot(img)) {
            String path = dir + "chikengun_4k_" + String((int)(timeAcc_ * 100.0f)) + ".png";
            if (img.SavePNG(path))
                ShowMessage("4K скриншот сохранён: Screenshots/chikengun_4k_" + String((int)(timeAcc_ * 100.0f)) + ".png");
        }
    }
    
    void HandleKeyDown(StringHash eventType, VariantMap& eventData) {
        using namespace KeyDown;
        (void)eventType;
        if (loadingScreenActive_) return;
        
        int key = eventData[P_KEY].GetI32();

        // Глобальные горячие клавиши 4K Ultra & Cyber Overload
        if (key == KEY_F3) {
            showTelemetry_ = !showTelemetry_;
            PlayUISound();
            return;
        }
        if (key == KEY_F8) {
            CycleAtmosphere();
            return;
        }
        if (key == KEY_F9) {
            static const char* qNames[4] = { "НИЗКИЙ", "СРЕДНИЙ", "ВЫСОКИЙ", "4K УЛЬТРА" };
            gfxQualityIdx_ = (gfxQualityIdx_ + 1) % 4;
            ApplyQualityPresetDefaults();
            ApplyGraphicsSettings();
            SaveSettingsFile();
            PlayUISound();
            ShowMessage(String("Пресет графики [F9]: ") + qNames[gfxQualityIdx_]);
            return;
        }
        if (key == KEY_F11) {
            gfxDisplayMode_ = (gfxDisplayMode_ == 2) ? 0 : 2;
            ApplyDisplayMode();
            SaveSettingsFile();
            PlayUISound();
            return;
        }
        if (key == KEY_F12) {
            Save4KScreenshot();
            return;
        }

        // ===== Нативная «Золотая рулетка» (только виртуальные фишки) =====
        if (casinoOpen_) {
            HandleCasinoKey(key);
            return;
        }

        // ===== Симулятор парной «КОЧЕТ В БАНЬКЕ» =====
        if (banyaOpen_) {
            HandleBanyaKey(key);
            return;
        }

        // ===== Магазин перков [B] =====
        if (perkShopOpen_) {
            if (key >= KEY_1 && key <= KEY_4) { BuyPerk(key - KEY_1); return; }
            if (key == KEY_B || key == KEY_ESCAPE) { TogglePerkShop(); return; }
            return;
        }

        // ===== Экран GAME OVER =====
        if (gameOver_) {
            if (key == KEY_RETURN || key == KEY_SPACE) { PlayUISound(); RestartArena(); }
            else if (key == KEY_P) { PlayUISound(); ShowGameOver(false); OpenBanyaSimulator(true); }
            else if (key == KEY_ESCAPE) { PlayUISound(); ShowGameOver(false); ShowMainMenu(true); }
            return;
        }

        // ===== Главное меню (5 пунктов: вкл. «КОЧЕТ В БАНЬКЕ») и 14-строчные 4K настройки =====
        if (inMenu_) {
            if (inSettings_) {
                const int kRows = 14;
                if (key == KEY_UP || key == KEY_W) { settingsSel_ = (settingsSel_ + kRows - 1) % kRows; PlayUISound(); UpdateSettingsDisplay(); }
                else if (key == KEY_DOWN || key == KEY_S) { settingsSel_ = (settingsSel_ + 1) % kRows; PlayUISound(); UpdateSettingsDisplay(); }
                else if (key == KEY_LEFT || key == KEY_A) { ChangeSetting(-1); }
                else if (key == KEY_RIGHT || key == KEY_D) { ChangeSetting(1); }
                else if (key == KEY_RETURN || key == KEY_SPACE) { if (settingsSel_ == kRows - 1) ShowSettings(false); else ChangeSetting(1); }
                else if (key == KEY_ESCAPE) { ShowSettings(false); }
                return;
            }
            if (key == KEY_P) { PlayUISound(); OpenBanyaSimulator(true); return; }
            if (key == KEY_UP || key == KEY_W) { menuSel_ = (menuSel_ + 5) % 6; PlayUISound(); UpdateMainMenuSelection(); }
            else if (key == KEY_DOWN || key == KEY_S) { menuSel_ = (menuSel_ + 1) % 6; PlayUISound(); UpdateMainMenuSelection(); }
            else if (key == KEY_RETURN || key == KEY_SPACE) {
                if (menuSel_ == 0) StartGameMode(MODE_RANGE);
                else if (menuSel_ == 1) StartGameMode(MODE_ARENA);
                else if (menuSel_ == 2) OpenBanyaSimulator(true);
                else if (menuSel_ == 3) OpenCasino();
                else if (menuSel_ == 4) ShowSettings(true);
                else engine_->Exit();
            }
            else if (key == KEY_ESCAPE) engine_->Exit();
            return;
        }

        // ===== Пауза =====
        if (paused_) {
            if (key == KEY_F1) { PlayUISound(); ToggleHelp(); return; }
            if (key == KEY_P)  { TogglePause(); OpenBanyaSimulator(false); return; }
            if (key == KEY_ESCAPE || key == KEY_RETURN) TogglePause();
            else if (key == KEY_M) { TogglePause(); ShowMainMenu(true); }
            else if (key == KEY_Q) engine_->Exit();
            return;
        }

        // P — вход в парную «КОЧЕТ В БАНЬКЕ» прямо из боя
        if (key == KEY_P) {
            OpenBanyaSimulator(false);
            return;
        }

        if (key == KEY_F1) { PlayUISound(); ToggleHelp(); return; }

        // F2 — 4K Фоторежим (скрывает HUD и оружие для чистых 4K кадров, F12 — сохранить PNG)
        if (key == KEY_F2) {
            photoMode_ = !photoMode_;
            SetGameHudVisible(!photoMode_);
            if (weaponNode_) weaponNode_->SetEnabled(!photoMode_);
            if (laserDotNode_ && photoMode_) laserDotNode_->SetEnabled(false);
            PlayUISound();
            if (!photoMode_) ShowMessage("Фоторежим выключен (F12 — сохранить 4K PNG)");
            return;
        }

        if (key == KEY_F) {
            StartGameMode(gameMode_ == MODE_RANGE ? MODE_ARENA : MODE_RANGE);
            return;
        }
        
        if (key == KEY_LSHIFT || key == KEY_RSHIFT) {
            isSprinting_ = !isCrouching_;
        }

        // ALT — реактивный рывок (Jet-Dash)
        if (key == KEY_LALT || key == KEY_RALT) {
            TriggerDash();
            return;
        }

        // Z — энергетический грэппл-крюк (Cyber Grapple Hook)
        if (key == KEY_Z) {
            TriggerGrapple();
            return;
        }

        // X — орбитальный лазерный удар «ЯЙЦЕГЕДДОН» (Orbital Ion Strike)
        if (key == KEY_X) {
            TriggerOrbitalStrike();
            return;
        }

        // M — смена фонового кибер-саундтрека
        if (key == KEY_M) {
            CycleMusicTrack();
            return;
        }

        // V — ближний удар кибер-крылом (Wing-Slap)
        if (key == KEY_V) {
            TriggerWingSlap();
            return;
        }

        // G — бросок кластерной яйце-гранаты
        if (key == KEY_G) {
            ThrowClusterGrenade();
            return;
        }

        // T — установка авто-турели «КУРОРЕЗ»
        if (key == KEY_T) {
            DeployCyberTurret();
            return;
        }

        // B — лаборатория кибер-перков
        if (key == KEY_B) {
            TogglePerkShop();
            return;
        }

        // I — осмотр 4K PBR-скина оружия
        if (key == KEY_I && !weapon_.isReloading_ && !isAiming_) {
            inspectTimer_ = 2.1f;
            PlayUISound();
            return;
        }

        // L — тактический 3D лазерный целеуказатель
        if (key == KEY_L) {
            laserOn_ = !laserOn_;
            PlayUISound();
            ShowMessage(laserOn_ ? "Лазерный прицел: ВКЛ" : "Лазерный прицел: ВЫКЛ");
            return;
        }
        
        if (key == KEY_R && !inMenu_ && !weapon_.isReloading_) {
            StartReload();
        }
        
        if (key == KEY_TAB) { ToggleArmory(); return; }

        if (key == KEY_ESCAPE) {
            if (photoMode_) {
                photoMode_ = false;
                SetGameHudVisible(true);
                if (weaponNode_) weaponNode_->SetEnabled(true);
            } else if (helpOpen_) { ToggleHelp(); }
            else if (armoryOpen_) { ToggleArmory(); }
            else if (perkShopOpen_) { TogglePerkShop(); }
            else { TogglePause(); }
            return;
        }

        if (key >= KEY_1 && key <= KEY_9) {
            int idx = key - KEY_1;
            if (idx < WEAPON_COUNT) { EquipWeapon(idx, false); ShowMessage(String("Оружие: ") + WEAPON_DEFS[idx].name); }
            return;
        }
        if (key == KEY_0) {
            EquipWeapon(9, false);
            ShowMessage(String("Оружие: ") + WEAPON_DEFS[9].name);
            return;
        }
        if (key == KEY_LEFTBRACKET) { CycleWeapon(-1); return; }
        if (key == KEY_RIGHTBRACKET) { CycleWeapon(1); return; }

        if (key == KEY_Q) { CycleSkin(1); return; }
        if (key == KEY_E) { CycleSkin(-1); return; }
    }
    
    void HandleKeyUp(StringHash eventType, VariantMap& eventData) {
        using namespace KeyUp;
        (void)eventType;
        int key = eventData[P_KEY].GetI32();
        if (key == KEY_LSHIFT || key == KEY_RSHIFT) {
            isSprinting_ = false;
        }
    }
    
    void HandleMouseButtonDown(StringHash eventType, VariantMap& eventData) {
        using namespace MouseButtonDown;
        (void)eventType;
        if (loadingScreenActive_) return;
        int button = eventData[P_BUTTON].GetI32();
        
        if (casinoOpen_) {
            if (button == MOUSEB_LEFT) HandleCasinoClick();
            return;
        }

        if (banyaOpen_) {
            if (button == MOUSEB_LEFT) HandleBanyaClick();
            return;
        }

        if (armoryOpen_) {
            HandleArmoryClick(button == MOUSEB_LEFT);
            return;
        }

        if (button == MOUSEB_LEFT && !inMenu_ && !gameOver_ && !paused_ && !helpOpen_ && !perkShopOpen_ && !photoMode_ && !isMouseVisible_) {
            inspectTimer_ = 0.0f;
            TryFire();
        }
        
        if (button == MOUSEB_RIGHT) {
            inspectTimer_ = 0.0f;
            isAiming_ = true;
        }
    }
    
    void HandleMouseButtonUp(StringHash eventType, VariantMap& eventData) {
        using namespace MouseButtonUp;
        (void)eventType;
        int button = eventData[P_BUTTON].GetI32();
        if (button == MOUSEB_RIGHT) isAiming_ = false;
    }

    // =========================================================================
    // Новые боевые способности: Jet-Dash (ALT/X), Wing-Slap (V), 3D Лазер (L)
    // =========================================================================

    void TriggerDash() {
        if (dashCooldown_ > 0.0f || isCrouching_ || !cameraNode_) return;
        auto* input = GetSubsystem<Input>();
        Quaternion yawRot(0.0f, yaw_, 0.0f);
        Vector3 forward = yawRot * Vector3(0.0f, 0.0f, 1.0f);
        Vector3 right   = yawRot * Vector3(1.0f, 0.0f, 0.0f);
        Vector3 dir = Vector3::ZERO;
        if (input->GetKeyDown(KEY_W)) dir += forward;
        if (input->GetKeyDown(KEY_S)) dir -= forward;
        if (input->GetKeyDown(KEY_D)) dir += right;
        if (input->GetKeyDown(KEY_A)) dir -= right;
        if (dir.LengthSquared() < 0.01f) dir = forward;
        dir.Normalize();

        dashDir_ = dir;
        dashTimer_ = 0.20f;
        dashCooldown_ = 2.2f * powf(0.86f, (float)perkSpdLvl_);
        playerVelocity_ = dir * (21.0f * (1.0f + 0.08f * (float)perkSpdLvl_));
        if (!onGround_) verticalVelocity_ = Max(verticalVelocity_, 2.0f);
        AddCameraShake(0.045f);
        if (soundSource_ && dashSound_) {
            soundSource_->SetGain(SfxGain(0.8f));
            soundSource_->Play(dashSound_);
        }
    }

    // === Кластерная яйце-граната [G] с баллистикой и отскоком ===
    void ThrowClusterGrenade() {
        if (grenadeCooldown_ > 0.0f || inMenu_ || gameOver_ || !cameraNode_) return;
        grenadeCooldown_ = 7.2f * powf(0.86f, (float)perkSpdLvl_);

        Vector3 start = cameraNode_->GetPosition() + cameraNode_->GetWorldDirection() * 0.65f + Vector3(0, -0.1f, 0);
        Vector3 dir   = cameraNode_->GetWorldDirection();
        Node* gn = scene_->CreateChild("ClusterEggGrenade", LOCAL);
        gn->SetPosition(start);
        gn->SetScale(Vector3(0.24f, 0.30f, 0.24f));
        StaticModel* sm = gn->CreateComponent<StaticModel>();
        sm->SetModel(sphereModel_ ? sphereModel_ : boxModel_);
        sm->SetMaterial(hdHazard_ ? hdHazard_ : (combMat_ ? combMat_.Get() : fallbackMat_));
        sm->SetCastShadows(true);

        if (soundSource_ && wingSlapSound_) {
            soundSource_->SetGain(SfxGain(0.65f));
            soundSource_->Play(wingSlapSound_);
        }

        ThrownGrenade tg;
        tg.node_ = gn;
        tg.pos_ = start;
        tg.vel_ = dir * 19.5f + Vector3(0.0f, 4.4f, 0.0f);
        tg.fuse_ = 1.40f;
        if (thrownGrenades_.Size() >= MAX_THROWN_GRENADES) {
            if (thrownGrenades_[0].node_) thrownGrenades_[0].node_->Remove();
            thrownGrenades_.Erase(0);
        }
        thrownGrenades_.Push(tg);
    }

    void UpdateThrownGrenades(float dt) {
        float dmgMul = (1.0f + 0.14f * (float)perkDmgLvl_) * (overdriveTimer_ > 0.0f ? 2.0f : 1.0f);
        for (unsigned i = thrownGrenades_.Size(); i-- > 0; ) {
            ThrownGrenade& g = thrownGrenades_[i];
            g.fuse_ -= dt;
            g.vel_.y_ -= 19.0f * dt;
            Vector3 step = g.vel_ * dt;
            float stepLen = step.Length();
            bool hitEnemy = false;

            if (stepLen > 0.001f) {
                float hd = 0.0f; Vector3 hp, hn; int htag = -1;
                if (RaycastLite(g.pos_, step / stepLen, stepLen + 0.18f, Collision::STATIC_MASK | Collision::TARGET_MASK | Collision::ENEMY_MASK | Collision::BARREL_MASK, hd, hp, &htag, nullptr, &hn)) {
                    if (htag == Collision::ENEMY || htag == Collision::BARREL) {
                        g.pos_ = hp;
                        hitEnemy = true;
                    } else {
                        g.pos_ = hp + hn * 0.16f;
                        float vn = g.vel_.DotProduct(hn);
                        if (vn < 0.0f) g.vel_ = (g.vel_ - hn * (1.58f * vn)) * 0.68f;
                    }
                } else {
                    g.pos_ += step;
                }
            }
            if (g.pos_.y_ < 0.18f) {
                g.pos_.y_ = 0.18f;
                if (g.vel_.y_ < 0.0f) g.vel_.y_ = -g.vel_.y_ * 0.52f;
                g.vel_.x_ *= 0.86f;
                g.vel_.z_ *= 0.86f;
            }
            if (g.node_) {
                g.node_->SetPosition(g.pos_);
                g.node_->Rotate(Quaternion(320.0f * dt, Vector3(1, 0.6f, 0.2f)));
            }

            if (g.fuse_ <= 0.0f || hitEnemy) {
                Vector3 boomPos = g.pos_;
                CreateExplosionEffect(boomPos);
                CreateShockwave(boomPos, nullptr, 0.68f);
                ApplyExplosionDamage(boomPos, 155.0f * dmgMul);
                // 3 мини-кластерных микро-разрыва вокруг эпицентра
                for (int c = 0; c < 3; ++c) {
                    float a = c * 2.09439f + timeAcc_;
                    Vector3 sub = boomPos + Vector3(cosf(a) * 2.3f, 0.2f, sinf(a) * 2.3f);
                    CreateShockwave(sub, nullptr, 0.36f);
                    ApplyExplosionDamage(sub, 62.0f * dmgMul);
                }
                if (barrelBoomSound_) PlaySfx3D(boomPos, barrelBoomSound_, 0.90f);
                else PlayExplosionAt(boomPos);
                AddCameraShake(0.18f);
                if (g.node_) g.node_->Remove();
                thrownGrenades_.Erase(i);
            }
        }
    }

    // === Авто-Турель «КУРОРЕЗ» [T]: союзный боевой дрон-петух ===
    void DeployCyberTurret() {
        if (inMenu_ || gameOver_ || !cameraNode_) return;
        if (turretCooldown_ > 0.0f) {
            ShowMessage("Турель перезаряжается (" + String((int)turretCooldown_ + 1) + "с)");
            PlayEmptyClickSound();
            return;
        }
        int cost = (gameMode_ == MODE_ARENA) ? 100 : 0;
        if (credits_ < cost) {
            ShowMessage("Нужно 100₵ кибер-кредитов для установки авто-турели!");
            PlayEmptyClickSound();
            return;
        }
        credits_ -= cost;
        turretCooldown_ = 16.0f;

        Quaternion flatRot(0.0f, yaw_, 0.0f);
        Vector3 fwd = flatRot * Vector3::FORWARD;
        Vector3 pos = PushOutOfObstacles(
            Vector3(cameraNode_->GetPosition().x_, 0.0f, cameraNode_->GetPosition().z_) + fwd * 2.4f, 0.55f, 0.55f);
        pos.y_ = 0.0f;

        Node* root = scene_->CreateChild("CyberTurret", LOCAL);
        root->SetPosition(pos);
        Material* baseMat = hdPanel_ ? hdPanel_ : metalMaterial_;
        if (!turretLaserMat_) turretLaserMat_ = MakeHDRGlow(Color(0.15f, 2.5f, 1.95f, 1.0f));
        Model* cyl = cylModel_ ? cylModel_ : boxModel_;
        Model* cone = coneModel_ ? coneModel_ : boxModel_;

        // Тренога и бронированная стойка
        AddPart(root, Vector3(0, 0.12f, 0), Vector3(0.95f, 0.24f, 0.95f), baseMat, cyl, true);
        AddPart(root, Vector3(0, 0.55f, 0), Vector3(0.26f, 0.72f, 0.26f), baseMat, cyl, true);
        AddPart(root, Vector3(0, 0.26f, 0), Vector3(0.62f, 0.08f, 0.62f), turretLaserMat_.Get(), cyl, false);

        // Поворотная башня «Петушиный гребень» + сдвоенные плазма-стволы
        Node* head = root->CreateChild("TurretHead");
        head->SetPosition(Vector3(0, 1.02f, 0));
        AddPart(head, Vector3(0, 0, 0), Vector3(0.46f, 0.34f, 0.54f), baseMat, nullptr, true);
        AddPart(head, Vector3(0, 0.25f, 0), Vector3(0.10f, 0.22f, 0.32f), combMat_ ? combMat_.Get() : baseMat, cone, false);
        Node* bL = AddPart(head, Vector3(-0.15f, 0.0f, 0.42f), Vector3(0.08f, 0.08f, 0.52f), turretLaserMat_.Get(), cyl, false);
        Node* bR = AddPart(head, Vector3( 0.15f, 0.0f, 0.42f), Vector3(0.08f, 0.08f, 0.52f), turretLaserMat_.Get(), cyl, false);
        bL->SetRotation(Quaternion(90.0f, Vector3(1, 0, 0)));
        bR->SetRotation(Quaternion(90.0f, Vector3(1, 0, 0)));

        TurretData td;
        td.node_ = root;
        td.headNode_ = head;
        td.pos_ = pos;
        td.ttl_ = 24.0f;
        td.fireTimer_ = 0.20f;
        if (turrets_.Size() >= MAX_TURRETS) {
            if (turrets_[0].node_) turrets_[0].node_->Remove();
            turrets_.Erase(0);
        }
        turrets_.Push(td);

        if (turretDeploySound_) PlaySfx3D(pos, turretDeploySound_, 0.95f);
        CreateShockwave(pos + Vector3(0, 0.15f, 0), turretLaserMat_.Get(), 0.45f);
        ShowMessage("Авто-турель «КУРОРЕЗ» развёрнута на 24 сек!");
        UpdateScoreDisplay();
        UnlockAchievement(8, "ИНЖЕНЕР ПТИЦЕФАБРИКИ (развернуть авто-турель)");
    }

    void UpdateTurrets(float dt) {
        float dmg = 24.0f * (1.0f + 0.14f * (float)perkDmgLvl_);
        for (unsigned i = turrets_.Size(); i-- > 0; ) {
            TurretData& t = turrets_[i];
            t.ttl_ -= dt;
            if (t.ttl_ <= 0.0f || !t.node_) {
                if (t.node_) t.node_->Remove();
                turrets_.Erase(i);
                continue;
            }
            t.fireTimer_ -= dt;
            Vector3 muzzleOrig = t.pos_ + Vector3(0, 1.04f, 0);

            // Поиск ближайшей живой курицы в радиусе 36 м с прямой видимостью
            int bestEnemy = -1;
            float bestDist = 36.0f;
            for (unsigned ei = 0; ei < enemies_.Size(); ++ei) {
                if (!enemies_[ei].alive_ || !enemies_[ei].node_) continue;
                Vector3 ep = enemies_[ei].node_->GetPosition() + Vector3(0, 0.9f, 0);
                float d = (ep - muzzleOrig).Length();
                if (d < bestDist && HasLineOfSight(muzzleOrig, ep)) {
                    bestDist = d;
                    bestEnemy = (int)ei;
                }
            }

            if (bestEnemy >= 0) {
                Vector3 targetPos = enemies_[bestEnemy].node_->GetPosition() + Vector3(0, 0.92f, 0);
                if (Node* hn = t.headNode_.Get()) hn->LookAt(targetPos);
                if (t.fireTimer_ <= 0.0f) {
                    t.fireTimer_ = 0.27f;
                    // Бирюзовый луч турели
                    Node* tr = scene_->CreateChild("TurretBeam", LOCAL);
                    tr->SetPosition((muzzleOrig + targetPos) * 0.5f);
                    tr->LookAt(targetPos);
                    StaticModel* sm = tr->CreateComponent<StaticModel>();
                    sm->SetModel(boxModel_);
                    if (!turretLaserMat_) turretLaserMat_ = MakeHDRGlow(Color(0.15f, 2.5f, 1.95f, 1.0f));
                    sm->SetMaterial(turretLaserMat_.Get());
                    sm->SetCastShadows(false);
                    tr->SetScale(Vector3(0.025f, 0.025f, (targetPos - muzzleOrig).Length()));
                    AddTracer(tr, 0.06f);

                    if (turretShotSound_) PlaySfx3D(muzzleOrig, turretShotSound_, 0.52f);
                    SpawnFloatingDamage(targetPos, (int)dmg, false);
                    DamageEnemy(enemies_[bestEnemy], dmg);
                }
            } else if (Node* hn = t.headNode_.Get()) {
                hn->Rotate(Quaternion(48.0f * dt, Vector3::UP));
            }
        }
    }

    void TriggerWingSlap() {
        if (meleeCooldown_ > 0.0f || !cameraNode_) return;
        meleeCooldown_ = 0.60f;
        meleeAnimTimer_ = 0.28f;
        inspectTimer_ = 0.0f;
        AddCameraShake(0.06f);
        if (soundSource_ && wingSlapSound_) {
            soundSource_->SetGain(SfxGain(0.85f));
            soundSource_->Play(wingSlapSound_);
        }

        Vector3 camPos = cameraNode_->GetPosition();
        Vector3 camDir = cameraNode_->GetWorldDirection();
        float dmg = 65.0f * (overdriveTimer_ > 0.0f ? 2.0f : 1.0f);
        bool hitAny = false;

        // Удар крылом по курам в конусе 3.0 м перед игроком + сильный отброс
        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            EnemyData& e = enemies_[i];
            if (!e.alive_ || !e.node_) continue;
            Vector3 ep = e.node_->GetPosition() + Vector3(0, 0.9f, 0);
            Vector3 toE = ep - camPos;
            float dist = toE.Length();
            if (dist < 3.1f && (dist < 1.1f || (toE / dist).DotProduct(camDir) > 0.42f)) {
                Vector3 knock = Vector3(toE.x_, 0.0f, toE.z_);
                if (knock.LengthSquared() > 0.001f) knock.Normalize();
                Vector3 pushed = PushOutOfObstacles(e.node_->GetPosition() + knock * 2.2f, 0.55f, 0.55f);
                e.node_->SetPosition(pushed);
                MoveColliderLite(e.colliderIdx_, pushed + Vector3(0, 0.85f, 0));
                SpawnFloatingDamage(ep, (int)dmg, false);
                ShowHitMarker(e.health_ - dmg <= 0.0f, false);
                DamageEnemy(e, dmg);
                hitAny = true;
            }
        }
        // Также можно ударить мишень или взрывную бочку!
        float rDist = 0.0f; Vector3 rHit, rNorm; int rTag = -1; void* rPtr = nullptr;
        if (!hitAny && RaycastLite(camPos, camDir, 2.9f, Collision::ENEMY_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, rDist, rHit, &rTag, &rPtr, &rNorm)) {
            CheckTargetHit(rTag, rPtr, rHit, dmg);
        }
    }

    void UpdateLaserSight() {
        if (!laserDotNode_) return;
        if (!laserOn_ || inMenu_ || gameOver_ || paused_ || photoMode_ || !cameraNode_) {
            laserDotNode_->SetEnabled(false);
            return;
        }
        Vector3 start = cameraNode_->GetPosition();
        Vector3 dir = cameraNode_->GetWorldDirection();
        float hitDist = 0.0f; Vector3 hitPos, hitNorm;
        if (RaycastLite(start, dir, 95.0f, Collision::STATIC_MASK | Collision::TARGET_MASK | Collision::ENEMY_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, hitDist, hitPos, nullptr, nullptr, &hitNorm) && hitDist > 0.4f) {
            laserDotNode_->SetPosition(hitPos + hitNorm * 0.03f);
            float s = Clamp(0.030f + hitDist * 0.0016f, 0.03f, 0.14f);
            laserDotNode_->SetScale(Vector3::ONE * s);
            laserDotNode_->SetEnabled(true);
        } else {
            laserDotNode_->SetEnabled(false);
        }
    }

    // =========================================================================
    // 2.3 ГРЭППЛ-КРЮК [Z], ОРБИТАЛЬНЫЙ УДАР «ЯЙЦЕГЕДДОН» [X] И 3D-ПОГОДА [F8]
    // =========================================================================

    void TriggerGrapple() {
        if (!cameraNode_ || !scene_) return;
        if (grappleActive_) {
            grappleActive_ = false;
            if (grappleBeamNode_) grappleBeamNode_->SetEnabled(false);
            return;
        }
        if (grappleCooldown_ > 0.0f) return;

        Vector3 origin = cameraNode_->GetPosition();
        Vector3 dir    = cameraNode_->GetWorldDirection();
        float hitDist  = 0.0f;
        Vector3 hitPos, hitNorm;
        if (!RaycastLite(origin, dir, 58.0f, Collision::STATIC_MASK | Collision::TARGET_MASK | Collision::ENEMY_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, hitDist, hitPos, nullptr, nullptr, &hitNorm) || hitDist < 2.2f) {
            ShowMessage("Грэппл-крюк [Z]: нет точки зацепа в пределах 58 м");
            PlayEmptyClickSound();
            return;
        }

        grappleActive_   = true;
        grappleTime_     = 1.45f;
        grappleCooldown_ = 3.2f * powf(0.88f, (float)perkSpdLvl_);
        grappleAnchor_   = hitPos;
        onGround_        = false;
        airborneFromPad_ = true; // фраги на лету с крюка тоже дают аэро-бонус!

        if (!grappleBeamNode_) {
            grappleBeamNode_ = scene_->CreateChild("GrappleBeam", LOCAL);
            StaticModel* bm = grappleBeamNode_->CreateComponent<StaticModel>();
            bm->SetModel(cylModel_ ? cylModel_ : boxModel_);
            bm->SetMaterial(MakeHDRGlow(Color(0.18f, 0.96f, 1.0f, 0.95f), 2.4f).Get());
            bm->SetCastShadows(false);
        }
        grappleBeamNode_->SetEnabled(true);
        if (soundSource_ && grappleSound_) {
            soundSource_->SetGain(SfxGain(0.88f));
            soundSource_->Play(grappleSound_);
        }
        UnlockAchievement(10, "НЕБЕСНЫЙ АБОРДАЖ (зацеп энергетическим крюком [Z])");
        UpdateDashAndBuffHUD();
    }

    void UpdateGrapple(float dt) {
        if (!grappleActive_ || !cameraNode_) {
            if (grappleBeamNode_) grappleBeamNode_->SetEnabled(false);
            return;
        }
        grappleTime_ -= dt;
        Vector3 handPos = cameraNode_->GetPosition() + cameraNode_->GetWorldRight() * 0.28f - Vector3(0.0f, 0.22f, 0.0f);
        Vector3 seg = grappleAnchor_ - handPos;
        float len = seg.Length();
        if (grappleTime_ <= 0.0f || len < 1.8f) {
            grappleActive_ = false;
            if (grappleBeamNode_) grappleBeamNode_->SetEnabled(false);
            return;
        }
        if (grappleBeamNode_) {
            grappleBeamNode_->SetPosition((handPos + grappleAnchor_) * 0.5f);
            Quaternion q;
            q.FromRotationTo(Vector3::UP, seg / Max(0.001f, len));
            grappleBeamNode_->SetRotation(q);
            float pulse = 0.038f + 0.014f * sinf(timeAcc_ * 34.0f);
            grappleBeamNode_->SetScale(Vector3(pulse, len, pulse));
            grappleBeamNode_->SetEnabled(true);
        }
    }

    void TriggerOrbitalStrike() {
        if (!cameraNode_ || !scene_) return;
        if (orbitalCharge_ < 99.9f) {
            if (credits_ >= Cyber::ORBITAL_EMERGENCY_CREDITS) {
                credits_ -= Cyber::ORBITAL_EMERGENCY_CREDITS;
                PushKillFeed("🛰 СРОЧНЫЙ ВЫЗОВ ОРБИТЫ ЗА " + String(Cyber::ORBITAL_EMERGENCY_CREDITS) + "₵!");
            } else {
                ShowMessage("Орбитальный удар [X]: заряд " + String((int)orbitalCharge_) + "% (или " +
                            String(Cyber::ORBITAL_EMERGENCY_CREDITS) + "₵ для мгновенного залпа)");
                PlayEmptyClickSound();
                return;
            }
        } else {
            orbitalCharge_ = 0.0f;
        }

        Vector3 origin = cameraNode_->GetPosition();
        Vector3 dir    = cameraNode_->GetWorldDirection();
        float hitDist  = 0.0f;
        Vector3 strikePos = origin + Vector3(dir.x_, 0.0f, dir.z_).Normalized() * 18.0f;
        strikePos.y_ = GameConstants::FLOOR_Y;
        if (RaycastLite(origin, dir, 120.0f, Collision::STATIC_MASK | Collision::TARGET_MASK | Collision::ENEMY_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, hitDist, strikePos)) {
            strikePos.y_ = Max(GameConstants::FLOOR_Y, strikePos.y_);
        }

        // Создаём исполинский 65-метровый ионный столб с небес + ударное кольцо
        OrbitalStrikeFX fx;
        fx.pos_    = strikePos;
        fx.ttl_    = 1.25f;
        fx.maxTtl_ = 1.25f;
        fx.beamNode_ = scene_->CreateChild("OrbitalBeam", LOCAL);
        fx.beamNode_->SetPosition(strikePos + Vector3(0.0f, 32.0f, 0.0f));
        fx.beamNode_->SetScale(Vector3(2.8f, 64.0f, 2.8f));
        StaticModel* bm = fx.beamNode_->CreateComponent<StaticModel>();
        bm->SetModel(cylModel_ ? cylModel_ : boxModel_);
        bm->SetMaterial(MakeHDRGlow(Color(0.32f, 0.96f, 1.0f, 0.92f), 3.4f).Get());
        bm->SetCastShadows(false);

        Node* coreLightN = fx.beamNode_->CreateChild("OrbitalLight", LOCAL);
        coreLightN->SetPosition(Vector3(0.0f, -0.45f, 0.0f));
        Light* ol = coreLightN->CreateComponent<Light>();
        ol->SetLightType(LIGHT_POINT);
        ol->SetColor(Color(0.45f, 0.95f, 1.0f));
        ol->SetBrightness(4.8f);
        ol->SetRange(28.0f);

        fx.ringNode_ = scene_->CreateChild("OrbitalRing", LOCAL);
        fx.ringNode_->SetPosition(Vector3(strikePos.x_, GameConstants::FLOOR_Y + 0.12f, strikePos.z_));
        fx.ringNode_->SetScale(Vector3(2.0f, 0.08f, 2.0f));
        StaticModel* rm = fx.ringNode_->CreateComponent<StaticModel>();
        rm->SetModel(cylModel_ ? cylModel_ : sphereModel_);
        rm->SetMaterial(MakeHDRGlow(Color(1.0f, 0.84f, 0.25f, 0.9f), 2.8f).Get());
        rm->SetCastShadows(false);
        if (orbitalStrikes_.Size() >= MAX_ORBITAL_STRIKES) {
            if (orbitalStrikes_[0].beamNode_) orbitalStrikes_[0].beamNode_->Remove();
            if (orbitalStrikes_[0].ringNode_) orbitalStrikes_[0].ringNode_->Remove();
            orbitalStrikes_.Erase(0);
        }
        orbitalStrikes_.Push(fx);

        if (soundSource_ && orbitalSound_) {
            soundSource_->SetGain(SfxGain(0.96f));
            soundSource_->Play(orbitalSound_);
        }
        AddCameraShake(0.24f);
        CreateExplosionEffect(strikePos + Vector3(0, 0.6f, 0));

        float maxDmg = Cyber::ORBITAL_BASE_DAMAGE * (1.0f + 0.15f * (float)perkDmgLvl_) * (overdriveTimer_ > 0.0f ? 1.5f : 1.0f);
        int strikeKills = 0;
        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            EnemyData& e = enemies_[i];
            if (!e.alive_ || !e.node_) continue;
            float d = (e.node_->GetPosition() - strikePos).Length();
            if (d <= Cyber::ORBITAL_RADIUS) {
                float dmg = maxDmg * (1.0f - 0.55f * (d / Cyber::ORBITAL_RADIUS));
                bool wasAlive = e.alive_;
                SpawnFloatingDamage(e.node_->GetPosition() + Vector3(0, 1.4f, 0), (int)dmg, true);
                DamageEnemy(e, dmg);
                if (wasAlive && !e.alive_) ++strikeKills;
            }
        }
        for (unsigned bi = 0; bi < barrels_.Size(); ++bi) {
            if (!barrels_[bi].exploded_ && (barrels_[bi].home_ - strikePos).Length() <= Cyber::ORBITAL_RADIUS)
                DetonateBarrel(barrels_[bi]);
        }

        if (strikeKills >= 3) {
            slowMoTimer_ = Max(slowMoTimer_, 0.42f);
            UnlockAchievement(11, "ЯЙЦЕГЕДДОН С ОРБИТЫ (3+ кур одним орбитальным залпом)");
        }
        ShowWaveBanner("☢ ОРБИТАЛЬНЫЙ УДАР «ЯЙЦЕГЕДДОН»!", Color(0.32f, 0.96f, 1.0f));
        UpdateDashAndBuffHUD();
        UpdateScoreDisplay();
    }

    void UpdateOrbitalStrikes(float dt) {
        for (unsigned i = orbitalStrikes_.Size(); i-- > 0; ) {
            OrbitalStrikeFX& fx = orbitalStrikes_[i];
            fx.ttl_ -= dt;
            if (fx.ttl_ <= 0.0f) {
                if (fx.beamNode_) fx.beamNode_->Remove();
                if (fx.ringNode_) fx.ringNode_->Remove();
                orbitalStrikes_.Erase(i);
                continue;
            }
            float u = 1.0f - Clamp(fx.ttl_ / fx.maxTtl_, 0.0f, 1.0f);
            if (fx.beamNode_) {
                float w = Max(0.05f, 3.2f * (1.0f - u * u));
                fx.beamNode_->SetScale(Vector3(w, 64.0f, w));
            }
            if (fx.ringNode_) {
                float r = 2.0f + u * Cyber::ORBITAL_RADIUS * 2.0f;
                fx.ringNode_->SetScale(Vector3(r, Max(0.02f, 0.10f * (1.0f - u)), r));
            }
        }
    }

    void UpdateWeatherParticles(float dt) {
        if (!scene_ || !cameraNode_ || gfxQualityIdx_ <= 0) return;
        if (weatherAtmBuilt_ != atmosphereIdx_ || weatherMotes_.Empty()) {
            for (unsigned i = 0; i < weatherMotes_.Size(); ++i)
                if (weatherMotes_[i].node_) weatherMotes_[i].node_->Remove();
            weatherMotes_.Clear();
            weatherAtmBuilt_ = atmosphereIdx_;

            static const Color kMoteCol[4] = {
                Color(1.0f, 0.72f, 0.26f, 0.85f), // Закат: золотые искры
                Color(0.20f, 0.92f, 1.0f, 0.85f), // Кибер-полночь: неоновый дождь
                Color(1.0f, 0.22f, 0.28f, 0.85f), // Затмение: восходящий багровый пепел
                Color(0.86f, 0.95f, 1.0f, 0.90f), // Арктика: снежная метель
            };
            SharedPtr<Material> wMat = MakeHDRGlow(kMoteCol[Clamp(atmosphereIdx_, 0, 3)], 1.55f);
            const int kCount = 36;
            for (int i = 0; i < kCount; ++i) {
                WeatherMote m;
                m.node_ = scene_->CreateChild("WeatherMote", LOCAL);
                m.offset_ = Vector3((float)(rand() % 320 - 160) * 0.1f,
                                    (float)(rand() % 140) * 0.1f - 2.0f,
                                    (float)(rand() % 320 - 160) * 0.1f);
                m.speed_ = 2.2f + (float)(rand() % 30) * 0.1f;
                m.phase_ = (float)(rand() % 628) * 0.01f;
                if (atmosphereIdx_ == 1) m.node_->SetScale(Vector3(0.025f, 0.42f, 0.025f));
                else                     m.node_->SetScale(Vector3(0.055f, 0.055f, 0.055f));
                StaticModel* sm = m.node_->CreateComponent<StaticModel>();
                sm->SetModel(boxModel_);
                sm->SetMaterial(wMat.Get());
                sm->SetCastShadows(false);
                weatherMotes_.Push(m);
            }
        }

        Vector3 camPos = cameraNode_->GetPosition();
        for (unsigned i = 0; i < weatherMotes_.Size(); ++i) {
            WeatherMote& m = weatherMotes_[i];
            if (!m.node_) continue;
            m.phase_ += dt * 1.8f;
            if (atmosphereIdx_ == 1) {
                m.offset_.y_ -= m.speed_ * 3.6f * dt; // неоновый ливень вниз
                if (m.offset_.y_ < -3.0f) m.offset_.y_ = 11.0f;
            } else if (atmosphereIdx_ == 2) {
                m.offset_.y_ += m.speed_ * 0.9f * dt; // восходящий пепел затмения
                m.offset_.x_ += sinf(m.phase_) * 0.7f * dt;
                if (m.offset_.y_ > 11.0f) m.offset_.y_ = -2.5f;
            } else if (atmosphereIdx_ == 3) {
                m.offset_.y_ -= m.speed_ * 1.15f * dt; // арктический снегопад
                m.offset_.x_ += cosf(m.phase_) * 1.4f * dt;
                if (m.offset_.y_ < -2.5f) m.offset_.y_ = 10.5f;
            } else {
                m.offset_.y_ += sinf(m.phase_) * 0.45f * dt; // парящие золотые искры заката
                m.offset_.x_ += cosf(m.phase_ * 0.7f) * 0.45f * dt;
            }
            if (m.offset_.x_ > 16.0f) m.offset_.x_ -= 32.0f;
            if (m.offset_.x_ < -16.0f) m.offset_.x_ += 32.0f;
            m.node_->SetPosition(camPos + m.offset_);
        }
    }

    // =========================================================================
    // Контроллер игрока: движение + прыжки + присед + реактивный рывок
    // =========================================================================

    float ProbeGround(const Vector3& feetPos) {
        float bestY = -1e6f;
        const float probeTop = feetPos.y_ + eyeHeight_ * 0.5f;
        const float probeLen = eyeHeight_ + 3.0f;
        for (int sx = -1; sx <= 1; sx += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                Vector3 offset(sx * 0.22f, 0.0f, sz * 0.22f);
                float dist; Vector3 hitPoint;
                if (RaycastLite(Vector3(feetPos.x_ + offset.x_, probeTop, feetPos.z_ + offset.z_),
                                Vector3(0, -1, 0), probeLen, Collision::STATIC_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, dist, hitPoint)) {
                    bestY = Max(bestY, hitPoint.y_);
                }
            }
        }
        return bestY;
    }

    bool HeadBlocked(const Vector3& feetPos) {
        float dist; Vector3 hitPoint;
        if (RaycastLite(Vector3(feetPos.x_, feetPos.y_ + 0.1f, feetPos.z_),
                        Vector3(0, 1, 0), GameConstants::PLAYER_HEIGHT + 0.1f, Collision::STATIC_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, dist, hitPoint)) {
            return dist > 0.05f && dist < GameConstants::PLAYER_HEIGHT;
        }
        return false;
    }

    void HandleMovement(float dt) {
        auto* input = GetSubsystem<Input>();

        Vector3 camPos = cameraNode_->GetPosition();
        Vector3 feetPos(camPos.x_, camPos.y_ - eyeHeight_, camPos.z_);

        Quaternion yawRot(0.0f, yaw_, 0.0f);
        Vector3 forward = yawRot * Vector3(0.0f, 0.0f, 1.0f);
        Vector3 right = yawRot * Vector3(1.0f, 0.0f, 0.0f);

        Vector3 wishDir = Vector3::ZERO;
        if (input->GetKeyDown(KEY_W)) wishDir += forward;
        if (input->GetKeyDown(KEY_S)) wishDir -= forward;
        if (input->GetKeyDown(KEY_D)) wishDir += right;
        if (input->GetKeyDown(KEY_A)) wishDir -= right;
        if (wishDir.LengthSquared() > 0.0f) wishDir.Normalize();

        bool wantCrouch = input->GetKeyDown(KEY_LCTRL) || input->GetKeyDown(KEY_RCTRL) || input->GetKeyDown(KEY_C);
        if (!wantCrouch && isCrouching_ && HeadBlocked(feetPos)) {
            wantCrouch = true;
        }
        isCrouching_ = wantCrouch;
        float targetEye = isCrouching_ ? GameConstants::PLAYER_HEIGHT_CROUCH : GameConstants::PLAYER_HEIGHT;
        eyeHeight_ = Lerp(eyeHeight_, targetEye, Min(1.0f, GameConstants::CROUCH_TRANSITION_SPEED * dt));

        if (grappleActive_) {
            bool releaseNow = false;
            Vector3 pull = Lite::ComputeGrapplePullT(camPos, grappleAnchor_,
                                                     28.0f * (1.0f + 0.06f * (float)perkSpdLvl_),
                                                     2.0f, releaseNow);
            if (releaseNow) {
                grappleActive_ = false;
                verticalVelocity_ = Max(verticalVelocity_, pull.y_);
                if (grappleBeamNode_) grappleBeamNode_->SetEnabled(false);
            } else {
                playerVelocity_ = Vector3(pull.x_, 0.0f, pull.z_);
                verticalVelocity_ = pull.y_;
                onGround_ = false;
            }
        } else if (dashTimer_ > 0.0f) {
            playerVelocity_ = dashDir_ * (20.5f * (1.0f + 0.08f * (float)perkSpdLvl_));
        } else {
            float speed = GameConstants::PLAYER_SPEED * (1.0f + 0.08f * (float)perkSpdLvl_);
            if (overdriveTimer_ > 0.0f) speed *= 1.18f;
            if (isCrouching_) speed *= GameConstants::PLAYER_CROUCH_MULTIPLIER;
            else if (isSprinting_ && wishDir.DotProduct(forward) > 0.0f && !isAiming_)
                speed *= GameConstants::PLAYER_SPRINT_MULTIPLIER;
            if (isAiming_) speed *= 0.6f;
            if (minigunSpinupActive_) speed *= 0.35f;

            Vector3 targetVel = wishDir * speed;
            float accel = onGround_ ? 14.0f : 4.0f;
            playerVelocity_ = Lerp(playerVelocity_, targetVel, Min(1.0f, accel * dt));
        }

        float gravMul = (gameMode_ == MODE_ARENA) ? Cyber::GetMutatorInfo(activeMutator_).gravityMul : 1.0f;
        if (!grappleActive_)
            verticalVelocity_ -= GameConstants::GRAVITY * gravMul * dt;

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
            landDuckOffset_ = -0.03f;
            PlayJumpSound();
        }

        Vector3 newPos = feetPos + playerVelocity_ * dt;

        const float radius = 0.35f;
        Vector3 testFeet(newPos.x_, feetPos.y_ + 0.4f, newPos.z_);
        const Vector3 dirs[4] = { Vector3(1,0,0), Vector3(-1,0,0), Vector3(0,0,1), Vector3(0,0,-1) };
        Vector3 push = Vector3::ZERO;
        for (int i = 0; i < 4; ++i) {
            float dist; Vector3 hitPoint;
            if (RaycastLite(testFeet, dirs[i], radius + 0.15f, Collision::STATIC_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, dist, hitPoint)
                && dist > 0.0f && dist < radius + 0.15f) {
                float penetration = (radius + 0.15f) - dist;
                push -= dirs[i] * penetration;
            }
        }
        newPos += push;

        float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        newPos.x_ = Clamp(newPos.x_, -arenaSize + 2.0f, arenaSize - 2.0f);
        newPos.z_ = Clamp(newPos.z_, -arenaSize + 2.0f, arenaSize - 2.0f);

        newPos.y_ += verticalVelocity_ * dt;
        float groundY = ProbeGround(newPos);
        if (groundY < -1e5f) groundY = 0.0f;

        bool wasOnGround = onGround_;
        if (verticalVelocity_ <= 0.0f && newPos.y_ <= groundY + 0.02f) {
            fallSpeedOnLand_ = -verticalVelocity_;
            newPos.y_ = groundY;
            verticalVelocity_ = 0.0f;
            onGround_ = true;
            airborneFromPad_ = false;
            if (!wasOnGround) {
                float impact = Clamp(fallSpeedOnLand_ / 12.0f, 0.0f, 1.0f);
                landDuckOffset_ = -GameConstants::LAND_DUCK_AMOUNT * impact;
                camShakeAmount_ = Max(camShakeAmount_, impact * 0.06f);
                if (impact > 0.15f) PlayLandSound(impact);
            }
        } else if (newPos.y_ > groundY + 0.05f) {
            onGround_ = false;
        }
        groundY_ = groundY;

        cameraNode_->SetPosition(Vector3(newPos.x_, newPos.y_ + eyeHeight_, newPos.z_));
        UpdateDynamicCamera(dt, playerVelocity_.Length());
    }

    // =========================================================================
    // Динамическая 4K камера от 1-го лица (настраиваемый FOV + эффекты рывка)
    // =========================================================================

    void UpdateDynamicCamera(float dt, float horizSpeed) {
        Camera* camera = cameraNode_->GetComponent<Camera>();

        float speedRatio = Clamp(horizSpeed / GameConstants::PLAYER_SPEED, 0.0f, 2.0f);
        float targetIntensity = (onGround_ && horizSpeed > 0.5f && !isAiming_) ? Min(speedRatio, 1.5f) : 0.0f;
        headBobIntensity_ = Lerp(headBobIntensity_, targetIntensity, Min(1.0f, 8.0f * dt));
        headBobPhase_ += GameConstants::HEAD_BOB_FREQ * dt * (0.6f + speedRatio * 0.7f);

        float amp = GameConstants::HEAD_BOB_AMP * headBobIntensity_;
        float bobY = fabsf(sinf(headBobPhase_)) * amp;
        float bobX = cosf(headBobPhase_ * 0.5f) * amp * 0.7f;

        landDuckOffset_ = Lerp(landDuckOffset_, 0.0f, Min(1.0f, GameConstants::LAND_DUCK_RECOVERY * dt));

        camShakeAmount_ = Lerp(camShakeAmount_, 0.0f, Min(1.0f, GameConstants::CAMERA_SHAKE_DECAY * dt));
        float shakeX = ((float)(rand() % 200) - 100.0f) / 100.0f * camShakeAmount_;
        float shakeY = ((float)(rand() % 200) - 100.0f) / 100.0f * camShakeAmount_;

        Vector3 pos = cameraNode_->GetPosition();
        pos.y_ += bobY + landDuckOffset_ + shakeY;
        pos.x_ += shakeX;
        cameraNode_->SetPosition(pos);

        float roll = bobX * 60.0f;
        if (!onGround_ && verticalVelocity_ < -2.0f) {
            roll += Clamp(verticalVelocity_, -15.0f, 0.0f) * GameConstants::FALL_CAM_TILT * 0.15f;
        }
        cameraNode_->SetRotation(Quaternion(pitch_, yaw_, roll));

        float targetFov = fovBase_;
        if (isAiming_) targetFov -= (currentWeapon_ == W_SNIPER ? 38.0f : GameConstants::ADS_FOV_SUB);
        else if (dashTimer_ > 0.0f) targetFov += 11.0f;
        else if (isSprinting_ && horizSpeed > GameConstants::PLAYER_SPEED * 1.1f)
            targetFov += GameConstants::SPRINT_FOV_ADD;
        if (isCrouching_) targetFov -= 4.0f;
        if (!onGround_) targetFov += verticalVelocity_ * 0.35f;
        currentFov_ = Lerp(currentFov_, targetFov, Min(1.0f, 8.0f * dt));
        if (camera) camera->SetFov(Clamp(currentFov_, 35.0f, 120.0f));
    }

    void AddCameraShake(float amount) {
        camShakeAmount_ = Min(camShakeAmount_ + amount, 0.25f);
    }
    
    void UpdateWeaponBob(float dt) {
        if (!weaponNode_) return;
        bool moving = playerVelocity_.LengthSquared() > 0.5f;

        if (moving && !weapon_.isReloading_ && !isAiming_ && inspectTimer_ <= 0.0f) {
            weapon_.weaponBobPhase_ += GameConstants::WEAPON_BOB_FREQUENCY * dt * (0.7f + headBobIntensity_ * 0.6f);

            float bobAmount = GameConstants::WEAPON_BOB_AMOUNT * (0.5f + headBobIntensity_);
            if (isSprinting_ && !isCrouching_) bobAmount *= 2.0f;
            if (isCrouching_) bobAmount *= 0.5f;

            float bobX = cosf(weapon_.weaponBobPhase_) * bobAmount;
            float bobY = fabsf(sinf(weapon_.weaponBobPhase_ * 2.0f)) * bobAmount;

            Vector3 base = WeaponBasePose();
            weaponNode_->SetPosition(Vector3(base.x_ + bobX, base.y_ - bobY, base.z_));
        } else {
            Vector3 currentPos = weaponNode_->GetPosition();
            weaponNode_->SetPosition(Lerp(currentPos, WeaponBasePose(), Min(1.0f, 10.0f * dt)));
        }

        if (wingHandL_ && wingHandR_) {
            float breathe = 8.0f * sinf(timeAcc_ * 2.2f);
            float reload = weapon_.isReloading_ ? 1.0f : 0.0f;
            float slap = meleeAnimTimer_ > 0.0f ? sinf((1.0f - meleeAnimTimer_ / 0.28f) * 3.14159f) * 65.0f : 0.0f;
            wingHandL_->SetRotation(Quaternion( breathe + 30.0f * reload + slap, Vector3(0, 0, 1)));
            wingHandR_->SetRotation(Quaternion(-breathe - 30.0f * reload - slap, Vector3(0, 0, 1)));
        }
    }

    void UpdateWeaponAnimation(float dt) {
        if (!weaponModelNode_) return;
        weaponAnimTime_ += dt;

        // Механические узлы не «замирают» в руках: миниган раскручивается,
        // крио-кольца охлаждаются, а ионная пила продолжает опасно вращаться.
        if (weaponSpinNode_) {
            float speed = 80.0f;
            if (currentWeapon_ == W_MINIGUN)
                speed = minigunSpinupActive_ ? 980.0f : 110.0f;
            else if (currentWeapon_ == W_SAW)
                speed = (weapon_.fireTimer_ > 0.0f) ? 820.0f : 420.0f;
            else if (currentWeapon_ == W_CRYO)
                speed = 120.0f + 24.0f * sinf(weaponAnimTime_ * 2.0f);
            Vector3 axis = (currentWeapon_ == W_SAW) ? Vector3::UP : Vector3::FORWARD;
            weaponSpinNode_->SetRotation(Quaternion(weaponAnimTime_ * speed, axis));
        }

        // Светящиеся ядра дышат постоянно и расширяются во вспышке выстрела.
        if (weaponPulseNode_) {
            float pulse = 1.0f + 0.10f * sinf(weaponAnimTime_ * 5.0f);
            if (muzzleFlashTimer_ > 0.0f) pulse += 0.32f;
            if (currentWeapon_ == W_ARC && weapon_.fireTimer_ > 0.0f) pulse += 0.16f;
            weaponPulseNode_->SetScale(weaponPulseBaseScale_ * pulse);
        }

        // Универсальный визуальный «удар затвора» для энергетических стволов:
        // модель чуть подаётся вперёд на первых кадрах после спина и возвращается.
        if (weaponModelNode_ && weapon_.fireTimer_ > 0.0f) {
            const float rate = Max(0.05f, WEAPON_DEFS[currentWeapon_].fireRate);
            float kick = Clamp(weapon_.fireTimer_ / rate, 0.0f, 1.0f);
            if (currentWeapon_ == W_ARC || currentWeapon_ == W_CRYO || currentWeapon_ == W_SAW)
                weaponModelNode_->SetPosition(Vector3(0.0f, 0.0f, -0.018f * kick));
            else
                weaponModelNode_->SetPosition(Vector3::ZERO);
        } else if (weaponModelNode_) {
            weaponModelNode_->SetPosition(Vector3::ZERO);
        }

        // Новый тяжёлый арсенал имеет отдельный подвижный затвор/кожух.
        // На перезарядке он уходит назад и возвращается с лёгким поворотом;
        // при выстреле получает короткий механический импульс.
        if (weaponSlideNode_) {
            float motion = 0.0f;
            if (weapon_.isReloading_) {
                float duration = Max(0.05f, WEAPON_DEFS[currentWeapon_].reloadTime);
                float progress = Clamp(1.0f - weapon_.reloadTimer_ / duration, 0.0f, 1.0f);
                motion = sinf(progress * 3.1415926f);
            } else if (weapon_.fireTimer_ > 0.0f) {
                float rate = Max(0.05f, WEAPON_DEFS[currentWeapon_].fireRate);
                motion = Clamp(weapon_.fireTimer_ / rate, 0.0f, 1.0f);
            }
            weaponSlideNode_->SetPosition(weaponSlideBasePosition_ + Vector3(0.0f, 0.0f, -0.085f * motion));
            weaponSlideNode_->SetRotation(weaponSlideBaseRotation_ * Quaternion(-8.0f * motion, Vector3::RIGHT));
        }
    }

    Vector3 WeaponBasePose() {
        Vector3 base(0.3f, -0.25f, 0.5f);
        if (isAiming_) return Vector3(0.0f, -0.16f, 0.45f);
        if (meleeAnimTimer_ > 0.0f) {
            float s = sinf((1.0f - meleeAnimTimer_ / 0.28f) * 3.14159f);
            return Vector3(0.18f - 0.14f * s, -0.20f + 0.06f * s, 0.50f + 0.22f * s);
        }
        if (inspectTimer_ > 0.0f) {
            float s = sinf(Clamp((2.1f - inspectTimer_) / 2.1f, 0.0f, 1.0f) * 3.14159f);
            return Vector3(0.14f * (1.0f - 0.6f * s), -0.19f + 0.05f * s, 0.52f);
        }
        if (isCrouching_) base.y_ -= 0.05f;
        if (!onGround_) base.y_ += 0.04f;
        return base;
    }
    
    void ApplyRecoilToWeapon() {
        if (!weaponNode_) return;
        Vector3 basePos = weaponNode_->GetPosition();
        Vector3 recoilOffset(-weapon_.recoilY_ * 0.01f, -weapon_.recoilX_ * 0.01f, -weapon_.recoilX_ * 0.02f);
        weaponNode_->SetPosition(basePos + recoilOffset);
        
        Quaternion recoilRot(weapon_.recoilX_ * 0.5f, -weapon_.recoilY_ * 0.3f, 0.0f);
        if (inspectTimer_ > 0.0f) {
            float t = (2.1f - inspectTimer_) / 2.1f;
            float env = sinf(Clamp(t, 0.0f, 1.0f) * 3.14159f);
            Quaternion inspRot(-14.0f * env, -38.0f * sinf(t * 6.28318f) * env, 24.0f * env);
            recoilRot = recoilRot * inspRot;
        }
        weaponNode_->SetRotation(recoilRot);
    }
    
    void TryFire() {
        if (weapon_.isReloading_ || weapon_.fireTimer_ > 0) return;
        
        if (weapon_.currentAmmo_ <= 0) {
            PlayEmptyClickSound();
            ShowMessage("Магазин пуст! Нажмите R для перезарядки");
            return;
        }
        
        FireWeapon();
    }
    
    void FireWeapon() {
        weapon_.currentAmmo_--;
        player_.shotsFired_++;
        
        float rofDiv = (1.0f + 0.12f * (float)perkRofLvl_);
        weapon_.fireTimer_ = (WEAPON_DEFS[currentWeapon_].fireRate / rofDiv) * (overdriveTimer_ > 0.0f ? 0.82f : 1.0f);
        
        float randomRecoil = (float)(rand() % 100) / 100.0f * GameConstants::BULLET_SPREAD;
        float recoilMul = (currentWeapon_ == W_RAILGUN) ? 1.85f : 1.0f;
        weapon_.recoilX_ += GameConstants::RECOIL_AMOUNT * (0.8f + randomRecoil) * recoilMul;
        weapon_.recoilY_ += (float)(rand() % 40 - 20) / 10.0f;
        
        PlayMuzzleFlash();
        PlayShootSound();
        const bool isEnergyWeapon = currentWeapon_ == W_RAILGUN || currentWeapon_ == W_PLASMA ||
                                     currentWeapon_ == W_ARC || currentWeapon_ == W_CRYO || currentWeapon_ == W_SAW;
        if (!isEnergyWeapon) SpawnShellCasing();
        
        const WeaponDef& wdef = WEAPON_DEFS[currentWeapon_];
        float banyaDmgMul = (banyaBuffTier_ >= 2) ? 1.15f : 1.0f;
        float dmg = wdef.damage * (1.0f + 0.14f * (float)perkDmgLvl_) * banyaDmgMul * (overdriveTimer_ > 0.0f ? 2.0f : 1.0f);
        bool anyPelletHit = false;
        if (currentWeapon_ == W_RAILGUN) {
            anyPelletHit = PerformRailgunPierce(dmg, wdef.spread);
        } else if (currentWeapon_ == W_PLASMA) {
            SpawnPlasmaBolt(dmg, wdef.spread);
            anyPelletHit = true;
        } else if (currentWeapon_ == W_ARC) {
            anyPelletHit = PerformArcCannon(dmg, wdef.spread);
        } else {
            for (int p = 0; p < wdef.pellets; ++p) {
                if (PerformRaycast(dmg, wdef.spread)) anyPelletHit = true;
            }
            if (currentWeapon_ == W_CRYO && cameraNode_) {
                // Крио-импульс оставляет холодный след прямо у дульного среза.
                if (!cryoCoreMat_) cryoCoreMat_ = MakeHDRGlow(Color(0.24f, 1.65f, 3.0f, 1.0f));
                CreateShockwave(cameraNode_->GetPosition() + cameraNode_->GetWorldDirection() * 1.25f,
                                cryoCoreMat_.Get(), 0.22f);
            }
        }
        if (anyPelletHit && currentWeapon_ != W_PLASMA) player_.shotsHit_++;

        UpdateAmmoDisplay();
        UpdateAccuracyDisplay();
    }

    // === ARC CANNON [10]: дуговой выстрел с цепным поражением ближайшей стаи ===
    bool PerformArcCannon(float damage, float spread) {
        bool primaryHit = PerformRaycast(damage, spread);
        if (!cameraNode_) return primaryHit;

        Vector3 start = cameraNode_->GetPosition();
        Vector3 dir = (Quaternion(pitch_, yaw_, 0.0f) * Vector3::FORWARD).Normalized();
        float hitDist = 0.0f; Vector3 hitPoint, hitNormal; int hitTag = -1; void* hitPtr = nullptr;
        if (!RaycastLite(start, dir, 120.0f, Collision::STATIC_MASK | Collision::TARGET_MASK | Collision::ENEMY_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK,
                         hitDist, hitPoint, &hitTag, &hitPtr, &hitNormal))
            return primaryHit;

        if (!arcCoreMat_) arcCoreMat_ = MakeHDRGlow(Color(2.8f, 1.55f, 0.16f, 1.0f));
        CreateShockwave(hitPoint, arcCoreMat_.Get(), 0.34f);
        int chained = 0;
        Vector<unsigned> chainedIndices;
        // Не порядок массива, а ближайшие цели: цепь должна быть предсказуемой
        // при разных порядках спавна волн и не «пролетать» мимо ближайшей курицы.
        while (chained < 3) {
            int bestIndex = -1;
            float bestDist = 7.5f;
            for (unsigned i = 0; i < enemies_.Size(); ++i) {
                EnemyData& candidate = enemies_[i];
                if (!candidate.alive_ || !candidate.node_ ||
                    (hitTag == Collision::ENEMY && candidate.node_ == static_cast<Node*>(hitPtr))) continue;
                bool alreadyChained = false;
                for (unsigned ci = 0; ci < chainedIndices.Size(); ++ci)
                    if (chainedIndices[ci] == i) { alreadyChained = true; break; }
                if (alreadyChained) continue;
                float candidateDist = (candidate.node_->GetPosition() + Vector3(0, 0.85f, 0) - hitPoint).Length();
                if (candidateDist <= bestDist) {
                    bestDist = candidateDist;
                    bestIndex = (int)i;
                }
            }
            if (bestIndex < 0) break;
            EnemyData& enemy = enemies_[(unsigned)bestIndex];
            Vector3 enemyPoint = enemy.node_->GetPosition() + Vector3(0, 0.85f, 0);
            float chainDamage = damage * (0.54f - 0.09f * (float)chained);
            SpawnTracer(hitPoint, enemyPoint);
            DamageEnemy(enemy, chainDamage);
            SpawnFloatingDamage(enemyPoint, (int)chainDamage, false);
            chainedIndices.Push((unsigned)bestIndex);
            ++chained;
        }
        if (chained > 0) {
            AddCameraShake(0.055f + 0.018f * chained);
            PushKillFeed("⚡ ДУГОВОЙ РАЗРЯД: цепь x" + String(chained));
        }
        return primaryHit || chained > 0;
    }

    // === Квантовый Рельсотрон [8]: прошивает ВСЕХ врагов, мишени и бочки насквозь! ===
    bool PerformRailgunPierce(float damage, float spread) {
        Quaternion spreadRot(pitch_ + ((float)(rand() % 200 - 100) / 1000.0f) * spread * 57.3f,
                             yaw_   + ((float)(rand() % 200 - 100) / 1000.0f) * spread * 57.3f, 0.0f);
        Vector3 dir = (spreadRot * Vector3::FORWARD).Normalized();
        Vector3 startPos = cameraNode_->GetPosition();

        // Находим первую твёрдую стену/пол (Collision::STATIC)
        float maxDist = 180.0f;
        float wallDist = maxDist;
        Vector3 wallHit = startPos + dir * maxDist;
        Vector3 wallNorm = Vector3::UP;
        if (RaycastLite(startPos, dir, maxDist, Collision::STATIC_MASK, wallDist, wallHit, nullptr, nullptr, &wallNorm)) {
            maxDist = wallDist;
            CreateImpactEffect(wallHit, wallNorm);
        }

        // Яркий гиперзвуковой рельсовый луч + ударные кольца вдоль траектории
        if (!railBeamMat_) railBeamMat_ = MakeHDRGlow(Color(0.25f, 1.95f, 3.2f, 1.0f));
        Vector3 beamFrom = startPos + dir * 0.55f;
        float beamLen = (wallHit - beamFrom).Length();
        if (beamLen > 0.4f) {
            Node* bm = scene_->CreateChild("RailBeam", LOCAL);
            bm->SetPosition((beamFrom + wallHit) * 0.5f);
            bm->LookAt(wallHit);
            StaticModel* sm = bm->CreateComponent<StaticModel>();
            sm->SetModel(cylModel_ ? cylModel_ : boxModel_);
            sm->SetMaterial(railBeamMat_.Get());
            sm->SetCastShadows(false);
            bm->SetScale(Vector3(0.085f, 0.085f, beamLen));
            AddTracer(bm, 0.24f);

            for (float d = 5.0f; d < Min(beamLen, 55.0f); d += 7.0f) {
                CreateShockwave(startPos + dir * d, railBeamMat_.Get(), 0.28f);
            }
        }
        AddCameraShake(0.14f);

        // Проверяем ВСЕ динамические коллайдеры (враги, бочки и мишени), пересекаемые лучом до maxDist!
        struct HitEntry { int tag; void* ptr; Vector3 pt; float dist; };
        Vector<HitEntry> hits;
        for (unsigned ci = 0; ci < colliders_.Size(); ++ci) {
            const Lite::Collider& c = colliders_[ci];
            if (c.tag == Collision::STATIC || c.tag == Collision::REMOVED || !c.ptr) continue;
            float t = c.sphere ? Lite::RaySphere(startPos, dir, c.center, c.half.x_)
                               : Lite::RayBox(startPos, dir, c.center, c.half);
            if (t >= 0.1f && t <= maxDist + 0.25f) {
                hits.Push(HitEntry{ c.tag, c.ptr, startPos + dir * t, t });
            }
        }

        int killsBefore = player_.kills_;
        for (unsigned hi = 0; hi < hits.Size(); ++hi) {
            CheckTargetHit(hits[hi].tag, hits[hi].ptr, hits[hi].pt, damage);
        }
        int killsMade = player_.kills_ - killsBefore;
        if (killsMade >= 2) {
            slowMoTimer_ = Max(slowMoTimer_, 0.34f);
            PushKillFeed("⚡ СКВОЗНОЙ РЕЛЬС x" + String(killsMade) + "!");
            UnlockAchievement(5, "СКВОЗНОЙ РЕЛЬС (2+ врага одним выстрелом Railgun)");
        }
        return !hits.Empty();
    }

    // === Плазменный Яйцемёт [9]: рикошетные сгустки биоплазмы с AoE взрывом ===
    void SpawnPlasmaBolt(float damage, float spread) {
        if (!plasmaOrbMat_) plasmaOrbMat_ = MakeHDRGlow(Color(0.20f, 2.8f, 1.45f, 1.0f));
        float rx = ((float)(rand() % 1000 - 500) / 1000.0f) * spread * (isAiming_ ? 0.35f : 1.0f);
        float ry = ((float)(rand() % 1000 - 500) / 1000.0f) * spread * (isAiming_ ? 0.35f : 1.0f);
        Quaternion rot(pitch_ + ry * 57.3f, yaw_ + rx * 57.3f, 0.0f);
        Vector3 dir = (rot * Vector3::FORWARD).Normalized();
        Vector3 start = cameraNode_->GetPosition() + dir * 0.65f + Vector3(0, -0.08f, 0);

        Node* orb = scene_->CreateChild("PlasmaBolt", LOCAL);
        orb->SetPosition(start);
        orb->SetScale(Vector3(0.22f, 0.22f, 0.32f));
        orb->LookAt(start + dir);
        StaticModel* sm = orb->CreateComponent<StaticModel>();
        sm->SetModel(sphereModel_ ? sphereModel_ : boxModel_);
        sm->SetMaterial(plasmaOrbMat_.Get());
        sm->SetCastShadows(false);

        PlasmaBoltData pb;
        pb.node_ = orb;
        pb.vel_ = dir * 46.0f;
        pb.damage_ = damage;
        pb.ttl_ = 2.4f;
        pb.bouncesLeft_ = 1;
        if (playerPlasmaBolts_.Size() >= MAX_PLASMA_BOLTS) {
            if (playerPlasmaBolts_[0].node_) playerPlasmaBolts_[0].node_->Remove();
            playerPlasmaBolts_.Erase(0);
        }
        playerPlasmaBolts_.Push(pb);
    }

    void UpdatePlasmaBolts(float dt) {
        for (unsigned i = playerPlasmaBolts_.Size(); i-- > 0; ) {
            PlasmaBoltData& b = playerPlasmaBolts_[i];
            b.ttl_ -= dt;
            if (b.ttl_ <= 0.0f || !b.node_) {
                if (b.node_) b.node_->Remove();
                playerPlasmaBolts_.Erase(i);
                continue;
            }
            Vector3 pos = b.node_->GetPosition();
            Vector3 step = b.vel_ * dt;
            float len = step.Length();
            if (len < 0.0005f) continue;

            float hd = 0.0f; Vector3 hp, hn; int htag = -1; void* hptr = nullptr;
            if (RaycastLite(pos, step / len, len + 0.14f, Collision::STATIC_MASK | Collision::TARGET_MASK | Collision::ENEMY_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, hd, hp, &htag, &hptr, &hn)) {
                if (htag == Collision::STATIC && b.bouncesLeft_ > 0) {
                    // 1 энергетический рикошет от стены/пола!
                    b.bouncesLeft_--;
                    float vn = b.vel_.DotProduct(hn);
                    if (vn < 0.0f) b.vel_ -= hn * (2.0f * vn);
                    Vector3 np = hp + hn * 0.12f;
                    b.node_->SetPosition(np);
                    b.node_->LookAt(np + b.vel_);
                    CreateHitSpark(hp);
                    continue;
                }
                player_.shotsHit_++;
                UpdateAccuracyDisplay();
                if (htag != Collision::STATIC) CheckTargetHit(htag, hptr, hp, b.damage_);
                // Изумрудный плазменный мини-всплеск по соседям в радиусе 2.8 м
                CreateShockwave(hp, plasmaOrbMat_.Get(), 0.30f);
                for (unsigned ei = 0; ei < enemies_.Size(); ++ei) {
                    if (!enemies_[ei].alive_ || !enemies_[ei].node_) continue;
                    float d = (enemies_[ei].node_->GetPosition() + Vector3(0, 0.8f, 0) - hp).Length();
                    if (d < 2.8f) DamageEnemy(enemies_[ei], b.damage_ * 0.42f * (1.0f - d / 2.8f));
                }
                b.node_->Remove();
                playerPlasmaBolts_.Erase(i);
            } else {
                b.node_->SetPosition(pos + step);
            }
        }
    }
    
    bool PerformRaycast(float damage, float spread) {
        auto* camera = cameraNode_->GetComponent<Camera>();
        if (!camera) return false;
        
        float spreadAngle = spread * (1.0f - player_.GetAccuracy() / 200.0f);
        if (isAiming_) spreadAngle *= 0.35f;
        if (isCrouching_) spreadAngle *= 0.7f;
        if (laserOn_) spreadAngle *= 0.78f;
        if (!onGround_) spreadAngle *= 1.6f;
        float randomX = (float)(rand() % 1000 - 500) / 1000.0f * spreadAngle;
        float randomY = (float)(rand() % 1000 - 500) / 1000.0f * spreadAngle;
        
        Quaternion spreadRot(pitch_ + randomY * 57.3f, yaw_ + randomX * 57.3f, 0.0f);
        Vector3 shootDirection = spreadRot * Vector3(0.0f, 0.0f, 1.0f);
        
        Vector3 startPos = cameraNode_->GetPosition();
        // ION SAW — ближний энергетический диск, а не универсальная винтовка:
        // после 36 м он затухает и больше не достаёт дальние цели.
        const float rayRange = (currentWeapon_ == W_SAW) ? 36.0f : GameConstants::BULLET_RANGE;
        Vector3 endPos = startPos + shootDirection * rayRange;
        
        float hitDist = 0.0f; Vector3 hitPosition; Vector3 hitNormal; int hitTag = -1; void* hitPtr = nullptr;
        bool anyHit = RaycastLite(startPos, shootDirection, rayRange,
                                  Collision::STATIC_MASK | Collision::TARGET_MASK | Collision::ENEMY_MASK | Collision::BARREL_MASK | Collision::MOVING_TARGET_MASK, hitDist, hitPosition, &hitTag, &hitPtr, &hitNormal);
        if (anyHit) {
            SpawnTracer(startPos + shootDirection * 0.6f, hitPosition);
            CreateImpactEffect(hitPosition, hitNormal);
            
            if (currentWeapon_ == W_LAUNCHER) {
                CreateExplosionEffect(hitPosition);
                ApplyExplosionDamage(hitPosition, damage);
                PlayExplosionAt(hitPosition);
                AddCameraShake(0.12f);
                ShowHitMarker(false);
            } else {
                float appliedDamage = damage;
                if (currentWeapon_ == W_SAW) {
                    float distanceT = Clamp(hitDist / rayRange, 0.0f, 1.0f);
                    appliedDamage *= Lerp(1.0f, 0.42f, distanceT);
                }
                CheckTargetHit(hitTag, hitPtr, hitPosition, appliedDamage);
            }
        } else {
            const float tracerRange = Min(rayRange, 55.0f);
            SpawnTracer(startPos + shootDirection * 0.6f, startPos + shootDirection * tracerRange);
            // Промах не должен оставлять пулевую дыру в воздухе.
            CreateImpactEffect(endPos, Vector3::ZERO);
        }
        return anyHit;
    }

    void SpawnTracer(const Vector3& from, const Vector3& to) {
        float len = (to - from).Length();
        if (len < 0.5f || len > 120.0f) return;
        Node* tr = scene_->CreateChild("Tracer", LOCAL);
        tr->SetPosition((from + to) * 0.5f);
        tr->LookAt(to);
        StaticModel* sm = tr->CreateComponent<StaticModel>();
        sm->SetModel(boxModel_);
        if (!tracerMat_) tracerMat_ = MakeHDRGlow(Color(1.0f, 0.88f, 0.45f), 2.4f);
        sm->SetMaterial(tracerMat_.Get());
        sm->SetCastShadows(false);
        tr->SetScale(Vector3(0.018f, 0.018f, len));
        AddTracer(tr, 0.065f);
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
        // Взрывоопасные бочки (Collision::BARREL, ptr == Node* бочки)
        if (hitTag == Collision::BARREL && hitPtr) {
            Node* bNode = static_cast<Node*>(hitPtr);
            for (unsigned bi = 0; bi < barrels_.Size(); ++bi) {
                ExplosiveBarrel& eb = barrels_[bi];
                if (!eb.exploded_ && eb.node_ == bNode) {
                    eb.hp_ -= damage;
                    CreateHitSpark(hitPosition);
                    PlayHitSound();
                    bool boom = (eb.hp_ <= 0.0f);
                    ShowHitMarker(boom, false);
                    SpawnFloatingDamage(hitPosition, (int)damage, false);
                    if (boom) DetonateBarrel(eb);
                    break;
                }
            }
            return;
        }

        Node* hitNode = nullptr;
        if ((hitTag == Collision::TARGET || hitTag == Collision::ENEMY || hitTag == Collision::MOVING_TARGET) && hitPtr)
            hitNode = static_cast<Node*>(hitPtr);
        if (!hitNode) return;

        // Роботы-курицы (Collision::ENEMY) + детектор ХЭДШОТОВ по высоте головы курицы!
        if (hitTag == Collision::ENEMY) {
            for (unsigned ei = 0; ei < enemies_.Size(); ++ei) {
                EnemyData& e = enemies_[ei];
                if (e.alive_ && e.node_ == hitNode) {
                    float sc = Max(0.1f, hitNode->GetScale().y_);
                    float headThreshY = hitNode->GetPosition().y_ + 1.04f * sc;
                    bool headshot = (hitPosition.y_ >= headThreshY);
                    float finalDmg = headshot ? (damage * 2.0f) : damage;
                    bool killed = (e.health_ - finalDmg <= 0.0f);

                    ShowHitMarker(killed, headshot);
                    SpawnFloatingDamage(hitPosition, (int)finalDmg, headshot);

                    if (headshot) {
                        player_.headshots_++;
                        if (player_.headshots_ >= 10)
                            UnlockAchievement(1, "КУРИНЫЙ СНАЙПЕР (10 критических хэдшотов)");
                        if (soundSource_ && headshotSound_) {
                            soundSource_->SetGain(SfxGain(0.85f));
                            soundSource_->Play(headshotSound_);
                        }
                        if (killed) PushKillFeed("★ КРИТИЧЕСКИЙ ХЭДШОТ!");
                    }
                    DamageEnemy(e, finalDmg);
                    if (currentWeapon_ == W_CRYO && e.alive_) {
                        // Попадание крио-лучом режет скорость, но не стакается
                        // бесконечно: свежий импульс только продлевает эффект.
                        e.cryoSlowTimer_ = Max(e.cryoSlowTimer_, 1.35f);
                        e.cryoSlowMul_ = 0.52f;
                    }
                    break;
                }
            }
            return;
        }

        // Мишени тира (Collision::TARGET или Collision::MOVING_TARGET)
        for (unsigned ti = 0; ti < targets_.Size(); ++ti) {
            TargetData& target = targets_[ti];
            if (target.isActive_ && target.node_ == hitNode) {
                bool bullseye = (hitPosition - hitNode->GetPosition()).Length() < 0.35f;
                float finalDmg = bullseye ? (damage * 1.5f) : damage;
                bool killed = (target.health_ - finalDmg <= 0.0f);
                ShowHitMarker(killed, bullseye);
                SpawnFloatingDamage(hitPosition, (int)finalDmg, bullseye);
                target.health_ -= finalDmg;
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
        const int removedCollider = target.colliderIdx_;
        RemoveColliderLite(removedCollider); // мишень больше не участвует в рейкастах
        target.colliderIdx_ = -1;
        // Движущаяся мишень держит SharedPtr в отдельном Lite-реестре.
        // Убираем запись сразу после уничтожения, а не оставляем «призрак»
        // до следующего полного ResetRangeTargets().
        for (unsigned mi = movingTargetsLite_.Size(); mi-- > 0; ) {
            if (movingTargetsLite_[mi].colliderIdx_ == removedCollider) {
                movingTargetsLite_.Erase(mi);
                break;
            }
        }
        
        // Комбо-очки (серия убийств подряд)
        AddScoreWithCombo(target.scoreValue_);
        player_.kills_++;
        credits_ += 15;
        if (player_.kills_ == 1) UnlockAchievement(0, "ПЕРВОЕ ПЕРО (первая цель уничтожена)");
        
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
        QueuePendingRemove(d, 12.0f);
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
        ShockwaveData sw; sw.node_ = SharedPtr<Node>(root); sw.ttl_ = ttl; sw.max_ = Max(0.02f, ttl);
        AddShockwave(sw);
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
        const bool hasSurface = normal.LengthSquared() > 0.001f;
        effectNode->LookAt(position + (hasSurface ? normal : Vector3::UP));

        // Эмиттер искр
        ParticleEmitter* emitter = effectNode->CreateComponent<ParticleEmitter>();
        if (sparkEffect_) emitter->SetEffect(sparkEffect_);

        // Отверстие от пули только на реальной поверхности, не в точке промаха.
        if (hasSurface) CreateBulletDecal(position, normal);

        // Автоудаление через 1 секунду
        QueuePendingRemove(effectNode, 1.0f);
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

        QueuePendingRemove(effectNode, 2.0f);
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
        AddDebris(d);
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
    // ПОЛНОЦЕННЫЙ 4K ЗАГРУЗОЧНЫЙ ЭКРАН (многокадровый GPU-конвейер + арт 4K)
    // =========================================================================

    void CreateLoadingScreen() {
        RefreshUIScale();
        auto* cache = GetSubsystem<ResourceCache>();
        if (!font_) {
            // Anonymous Pro is shipped with the project.  Prefer it before the
            // optional DejaVu fallback so a normal install does not emit a
            // misleading "DejaVuSans.ttf not found" error on every launch.
            font_ = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
            if (!font_) font_ = cache->GetResource<Font>("Fonts/BlueHighway.ttf");
            if (!font_) font_ = cache->GetResource<Font>("Fonts/DejaVuSans.ttf");
        }
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        loadSteps_ = Vector<String>{
            "Инициализация 4K HDR пайплайна, ACES Filmic и шейдера CAS",
            "Загрузка 4K PBR текстур, нормалей и шероховатости/металла",
            "Синтез кубической карты неба 4K и IBL-отражений арены",
            "Построение кибер-арены 256×256, джамп-падов и бочек «ОПАСНО»",
            "Каскадные тени 4K, перья PBR и арсенал (12 стволов)",
            "Перки [B], кластерные гранаты, волны 2.5 и 3D-аудио пул",
            "ПЕРО СУДЬБЫ готово. В бой!"
        };

        static const char* kTips[6] = {
            "СОВЕТ: Хедшот курицы — 2.0× КРИТИЧЕСКОГО урона (золотой маркер ★).",
            "СОВЕТ: ALT или X — реактивный рывок (Jet-Dash) сквозь стаю врагов!",
            "СОВЕТ: Красные бочки «ОПАСНО» — цепная детонация выносит целую волну.",
            "СОВЕТ: Фиолетовая сфера ОВЕРДРАЙВА удваивает урон всех стволов на 12 секунд.",
            "СОВЕТ: V — удар кибер-крылом вблизи, L — 3D лазер, G — граната-клёцка.",
            "СОВЕТ: F8 переключает атмосферу 4K — от золотого заката до кровавого затмения."
        };
        loadingTipIdx_ = rand() % 6;

        loadingPanel_ = root->CreateChild<UIElement>("LoadingPanel");
        loadingPanel_->SetSize(sw, sh);
        loadingPanel_->SetPriority(1000);

        // 1) 4K фоновый кибер-арт (Textures/LoadingBg4K.png)
        loadingBgImg_ = loadingPanel_->CreateChild<BorderImage>("LoadingArt4K");
        loadingBgImg_->SetSize(sw, sh);
        if (Texture2D* bgTex = cache->GetResource<Texture2D>("Textures/LoadingBg4K.png")) {
            bgTex->SetFilterMode(FILTER_TRILINEAR);
            loadingBgImg_->SetTexture(bgTex);
            loadingBgImg_->SetFullImageRect();
            loadingBgImg_->SetColor(Color(0.92f, 0.95f, 1.0f, 1.0f));
        } else {
            loadingBgImg_->SetColor(Color(0.015f, 0.025f, 0.055f, 1.0f));
        }

        // 2) Кинематографические letterbox-полосы сверху и снизу + центральная стеклянная карточка
        BorderImage* topBar = loadingPanel_->CreateChild<BorderImage>();
        topBar->SetPosition(0, 0);
        topBar->SetSize(sw, S(78));
        topBar->SetColor(Color(0.008f, 0.015f, 0.032f, 0.88f));

        BorderImage* botBar = loadingPanel_->CreateChild<BorderImage>();
        botBar->SetPosition(0, sh - S(165));
        botBar->SetSize(sw, S(165));
        botBar->SetColor(Color(0.008f, 0.015f, 0.032f, 0.90f));

        BorderImage* card = loadingPanel_->CreateChild<BorderImage>();
        int cardW = S(860), cardH = S(390);
        card->SetPosition(sw / 2 - cardW / 2, sh / 2 - S(235));
        card->SetSize(cardW, cardH);
        card->SetColor(Color(0.012f, 0.024f, 0.048f, 0.76f));

        // Неоновая рамка карточки
        BorderImage* cTop = loadingPanel_->CreateChild<BorderImage>();
        cTop->SetPosition(sw / 2 - cardW / 2, sh / 2 - S(235));
        cTop->SetSize(cardW, Max(2, S(2.5f)));
        cTop->SetColor(Color(0.20f, 0.92f, 1.0f, 0.85f));

        BorderImage* cBot = loadingPanel_->CreateChild<BorderImage>();
        cBot->SetPosition(sw / 2 - cardW / 2, sh / 2 - S(235) + cardH - Max(2, S(2.0f)));
        cBot->SetSize(cardW, Max(2, S(2.0f)));
        cBot->SetColor(Color(1.0f, 0.78f, 0.22f, 0.75f));

        // Верхний статус движка
        Text* sysTag = loadingPanel_->CreateChild<Text>();
        sysTag->SetFont(font_, SF(15));
        sysTag->SetText("URHO3D 4K PBR   •   ACES FILMIC HDR   •   SSAO4K   •   CAS 4K   •   ПЕРО СУДЬБЫ 2.5");
        sysTag->SetTextAlignment(HA_CENTER);
        sysTag->SetPosition(sw / 2 - S(500), S(28));
        sysTag->SetWidth(S(1000));
        sysTag->SetColor(Color(0.45f, 0.90f, 1.0f, 0.92f));
        StyleCrispText(sysTag);

        // Кибер-эмблема (геометрический значок куриного гребня/прицела)
        BorderImage* embOuter = loadingPanel_->CreateChild<BorderImage>();
        embOuter->SetPosition(sw / 2 - S(52), sh / 2 - S(215));
        embOuter->SetSize(S(104), S(6));
        embOuter->SetColor(Color(1.0f, 0.78f, 0.22f, 0.95f)); // золотое перо

        loadingTitleText_ = loadingPanel_->CreateChild<Text>();
        loadingTitleText_->SetFont(font_, SF(60));
        loadingTitleText_->SetText("CHIKENGUN 2.5");
        loadingTitleText_->SetTextAlignment(HA_CENTER);
        loadingTitleText_->SetPosition(sw / 2 - S(410), sh / 2 - S(198));
        loadingTitleText_->SetWidth(S(820));
        loadingTitleText_->SetColor(Color(1.0f, 0.86f, 0.25f, 1.0f));
        StyleCrispText(loadingTitleText_, true);

        loadingSubText_ = loadingPanel_->CreateChild<Text>();
        loadingSubText_->SetFont(font_, SF(19));
        loadingSubText_->SetText("П Е Р О   С У Д Ь Б Ы     |     C Y B E R   A R E N A");
        loadingSubText_->SetTextAlignment(HA_CENTER);
        loadingSubText_->SetPosition(sw / 2 - S(410), sh / 2 - S(124));
        loadingSubText_->SetWidth(S(820));
        loadingSubText_->SetColor(Color(0.42f, 0.88f, 1.0f, 0.95f));
        StyleCrispText(loadingSubText_);

        // Чек-лист инициализации подсистем
        loadingSpecText_ = loadingPanel_->CreateChild<Text>();
        loadingSpecText_->SetFont(font_, SF(15));
        loadingSpecText_->SetPosition(sw / 2 - S(360), sh / 2 - S(82));
        loadingSpecText_->SetWidth(S(720));
        loadingSpecText_->SetColor(Color(0.82f, 0.93f, 0.98f, 0.95f));
        StyleCrispText(loadingSpecText_);

        // Полоса загрузки в нижней консоли (ширина 760 масштабируемых пикселей)
        int barW = S(760), barH = S(16);
        int barX = sw / 2 - barW / 2;
        int barY = sh - S(108);

        loadingBarTrack_ = loadingPanel_->CreateChild<BorderImage>();
        loadingBarTrack_->SetPosition(barX, barY);
        loadingBarTrack_->SetSize(barW, barH);
        loadingBarTrack_->SetColor(Color(0.08f, 0.13f, 0.20f, 0.95f));

        loadingBarFill_ = loadingPanel_->CreateChild<BorderImage>();
        loadingBarFill_->SetPosition(barX, barY);
        loadingBarFill_->SetSize(S(12), barH);
        loadingBarFill_->SetColor(Color(0.18f, 0.95f, 0.68f, 1.0f));

        loadingBarShimmer_ = loadingPanel_->CreateChild<BorderImage>();
        loadingBarShimmer_->SetPosition(barX, barY - S(3));
        loadingBarShimmer_->SetSize(S(24), barH + S(6));
        loadingBarShimmer_->SetColor(Color(0.65f, 1.0f, 0.95f, 0.90f));
        loadingBarShimmer_->SetBlendMode(BLEND_ADD);

        loadingStageText_ = loadingPanel_->CreateChild<Text>();
        loadingStageText_->SetFont(font_, SF(16));
        loadingStageText_->SetPosition(barX, barY - S(28));
        loadingStageText_->SetColor(Color(0.88f, 0.95f, 1.0f, 0.96f));
        StyleCrispText(loadingStageText_);

        loadingPctText_ = loadingPanel_->CreateChild<Text>();
        loadingPctText_->SetFont(font_, SF(18));
        loadingPctText_->SetTextAlignment(HA_RIGHT);
        loadingPctText_->SetPosition(barX + barW - S(120), barY - S(30));
        loadingPctText_->SetWidth(S(120));
        loadingPctText_->SetColor(Color(1.0f, 0.88f, 0.28f, 1.0f));
        StyleCrispText(loadingPctText_);

        loadingTipText_ = loadingPanel_->CreateChild<Text>();
        loadingTipText_->SetFont(font_, SF(15));
        loadingTipText_->SetText(kTips[loadingTipIdx_]);
        loadingTipText_->SetTextAlignment(HA_CENTER);
        loadingTipText_->SetPosition(sw / 2 - S(480), sh - S(62));
        loadingTipText_->SetWidth(S(960));
        loadingTipText_->SetColor(Color(0.75f, 0.86f, 0.96f, 0.92f));
        StyleCrispText(loadingTipText_);

        UpdateLoadingVisuals(0.04f, 0);
    }

    void UpdateLoadingVisuals(float progress, int activeStage) {
        if (!loadingPanel_) return;
        loadProgress_ = Clamp(progress, 0.0f, 1.0f);
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();
        int barW = S(760), barH = S(16);
        int barX = sw / 2 - barW / 2;
        int barY = sh - S(108);
        int fillW = Max(S(8), (int)(barW * loadProgress_));

        if (loadingBarFill_) {
            loadingBarFill_->SetSize(fillW, barH);
            float pulse = 0.85f + 0.15f * sinf(timeAcc_ * 8.0f);
            loadingBarFill_->SetColor(Color(0.15f * pulse, 0.95f * pulse, 0.72f + 0.25f * loadProgress_, 1.0f));
        }
        if (loadingBarShimmer_) {
            loadingBarShimmer_->SetPosition(barX + Max(0, fillW - S(16)), barY - S(3));
        }
        if (loadingPctText_) {
            loadingPctText_->SetText(String((int)(loadProgress_ * 100.0f + 0.5f)) + "%");
        }
        if (loadingStageText_ && !loadSteps_.Empty()) {
            int idx = Clamp(activeStage, 0, (int)loadSteps_.Size() - 1);
            loadingStageText_->SetText("ЭТАП " + String(idx + 1) + "/" + String((int)loadSteps_.Size()) +
                                       ":  " + loadSteps_[idx]);
        }
        if (loadingSpecText_) {
            static const char* shortNames[6] = {
                "Шейдеры 4K HDR / SSAO4K / ACES / CAS",
                "Пакет 4K PBR (Diffuse + Normal + RoughMetal)",
                "Sky4K кубемапа и IBL-отражения арены",
                "Арена 256×256, джамп-пады, бочки «ОПАСНО»",
                "Тени 4K, перья PBR, арсенал (12 стволов)",
                "Волны 2.5 «ПЕРО СУДЬБЫ», перки и радар 110 м"
            };
            String list;
            for (int i = 0; i < 6; ++i) {
                if (i < activeStage)      list += "  [✓]  ";
                else if (i == activeStage) list += "  [▶]  ";
                else                      list += "  [·]  ";
                list += String(shortNames[i]) + "\n";
            }
            loadingSpecText_->SetText(list);
        }
    }

    // Выполняет ровно ОДИН тяжёлый шаг инициализации за отрендеренный кадр,
    // чтобы игрок видел настоящую плавную загрузку без зависания окна!
    void UpdateBootLoading(float dt) {
        bootStageTimer_ += dt;
        if (loadingTitleText_) {
            float p = 0.86f + 0.14f * sinf(timeAcc_ * 3.5f);
            loadingTitleText_->SetColor(Color(1.0f, 0.86f * p, 0.25f * p, 1.0f));
        }

        const float kMinStepHold = 0.11f;
        if (loadStepIndex_ < 7) {
            if (bootStageTimer_ < kMinStepHold && loadStepIndex_ > 0) return;
            bootStageTimer_ = 0.0f;

            switch (loadStepIndex_) {
            case 0:
                LoadCoreModelsAndFont();
                UpdateLoadingVisuals(0.16f, 1);
                break;
            case 1:
                CreateMaterials();
                UpdateLoadingVisuals(0.36f, 2);
                break;
            case 2:
                EnsureSkyCubemap();
                LoadAudioAssets();
                UpdateLoadingVisuals(0.54f, 3);
                break;
            case 3:
                CreateScene();
                UpdateLoadingVisuals(0.74f, 4);
                break;
            case 4:
                CreatePlayer();
                ApplyGraphicsSettings();
                UpdateLoadingVisuals(0.90f, 5);
                break;
            case 5:
                InitSfx3DPool();
                CreateUI();
                CreateMainMenu();
                SetupInput();
                ShowMainMenu(true);
                if (loadingPanel_) loadingPanel_->BringToFront();
                UpdateLoadingVisuals(1.0f, 6);
                loadingFadeAlpha_ = 0.42f;
                break;
            case 6:
                break;
            }
            ++loadStepIndex_;
            return;
        }

        if (loadingFadeAlpha_ > 0.0f) {
            loadingFadeAlpha_ -= dt;
            float a = Clamp(loadingFadeAlpha_ / 0.42f, 0.0f, 1.0f);
            if (kochThemeSrc_) kochThemeSrc_->SetGain(musicVolume_ * a * a);
            if (loadingPanel_) loadingPanel_->SetOpacity(a * a);
            if (loadingFadeAlpha_ <= 0.0f) {
                HideLoadingScreen();
            }
        } else {
            HideLoadingScreen();
        }
    }

    void HideLoadingScreen() {
        // Останавливаем интро-тему «koch.studios» и включаем основную петлю арены
        if (kochThemeSrc_) { kochThemeSrc_->Stop(); kochThemeSrc_ = nullptr; }
        splashSfxSrc_ = nullptr;
        kochMusicNode_ = nullptr;
        splashScene_ = nullptr;
        StartAmbientLoop();
        if (loadingPanel_) {
            loadingPanel_->Remove();
            loadingPanel_ = nullptr;
        }
        loadingBgImg_ = nullptr;
        loadingTitleText_ = nullptr;
        loadingSubText_ = nullptr;
        loadingStageText_ = nullptr;
        loadingPctText_ = nullptr;
        loadingSpecText_ = nullptr;
        loadingTipText_ = nullptr;
        loadingBarTrack_ = nullptr;
        loadingBarFill_ = nullptr;
        loadingBarShimmer_ = nullptr;
        loadingScreenActive_ = false;
    }

    // =========================================================================
    // 2.5.6 SPLASH INTRO — «koch.studios» → «powered by fall.os» → «CHICHKENCUM 2.1»
    // Надписи выплывают снизу (easeOutExpo), открываются посимвольно, светятся,
    // пробегает волна-блик; CHICHKENCUM наезжает размером с тряской.
    // Скип: клик / Space / Enter / Esc.
    // =========================================================================
    int SplashLineY(int stage) const {
        int sh = GetSubsystem<Graphics>()->GetHeight();
        return sh / 2 + (stage == 2 ? S(46) : (stage == 1 ? S(16) : S(20)));
    }
    int SplashTextY(int stage) const {
        int sh = GetSubsystem<Graphics>()->GetHeight();
        return sh / 2 - S(stage == 2 ? 62 : (stage == 1 ? 40 : 44));
    }
    static float EaseOutCubic(float t) {
        float f = 1.0f - Clamp(t, 0.0f, 1.0f);
        return 1.0f - f * f * f;
    }
    static float EaseOutExpo(float t) {
        t = Clamp(t, 0.0f, 1.0f);
        return t >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
    }

    void SplashEnterStage(int stage) {
        auto* cache = GetSubsystem<ResourceCache>();
        if (!cache || !splashSfxSrc_) return;
        // koch.studios — мягкий тик; fall.os — поверпап; CHICHKENCUM — петушиный клич
        const char* snd = stage == 0 ? "Sounds/UI_Select.wav"
                        : stage == 1 ? "Sounds/Powerup.wav"
                                     : "Sounds/Chicken_Boss.wav";
        if (Sound* s = cache->GetResource<Sound>(snd)) {
            splashSfxSrc_->Stop();
            splashSfxSrc_->SetGain(stage == 2 ? 0.9f : 0.7f);
            splashSfxSrc_->Play(s);
        }
    }

    void ConfigureSplashStage(int stage) {
        if (!splashText_ || !splashLine_) return;
        const int sw = GetSubsystem<Graphics>()->GetWidth();
        splashLine_->SetPosition(sw / 2, SplashLineY(stage));
        splashLine_->SetSize(0, Max(2, S(stage == 2 ? 4.0f : 3.0f)));
        splashText_->SetPosition(sw / 2 - S(720), SplashTextY(stage) + S(48));   // старт ниже — выплывем вверх
        if (stage == 0) {
            splashFullText_ = "k o c h . s t u d i o s";
            splashText_->SetFont(font_, SF(36));
            splashText_->SetText("");
            splashText_->SetColor(Color(0.90f, 0.96f, 1.0f, 0.0f));
        } else if (stage == 1) {
            splashFullText_ = "powered by fall.os";
            splashText_->SetFont(font_, SF(27));
            splashText_->SetText("");
            splashText_->SetColor(Color(0.70f, 0.76f, 0.88f, 0.0f));
        } else {
            splashFullText_ = "CHICHKENCUM 2.1";
            splashText_->SetFont(font_, SF(104));
            splashText_->SetText(splashFullText_);
            splashText_->SetPosition(sw / 2 - S(720), SplashTextY(2) + S(12));
            splashText_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
        }
    }

    void CreateSplashIntro() {
        auto* ui = GetSubsystem<UI>();
        auto* gfx = GetSubsystem<Graphics>();
        auto* cache = GetSubsystem<ResourceCache>();
        if (!ui || !gfx || !cache) return;
        if (!font_) {
            font_ = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
            if (!font_) font_ = cache->GetResource<Font>("Fonts/DejaVuSans.ttf");
        }
        const int sw = gfx->GetWidth(), sh = gfx->GetHeight();

        splashPanel_ = ui->GetRoot()->CreateChild<UIElement>("KochSplashIntro");
        splashPanel_->SetSize(sw, sh);
        splashPanel_->SetPriority(1200);   // поверх загрузочного экрана (1000)

        BorderImage* bg = splashPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.0f, 0.0f, 0.0f, 1.0f));

        // Неоновое свечение за надписью (под текстом и линией)
        splashGlow_ = splashPanel_->CreateChild<BorderImage>();
        if (Texture2D* glowTex = cache->GetResource<Texture2D>("Textures/SplashGlow.png")) {
            glowTex->SetFilterMode(FILTER_TRILINEAR);
            splashGlow_->SetTexture(glowTex);
        }
        splashGlow_->SetBlendMode(BLEND_ADD);
        splashGlow_->SetColor(Color(0.18f, 0.82f, 1.0f, 0.0f));
        {
            const int g0 = S(880);
            splashGlow_->SetSize(g0, g0);
            splashGlow_->SetPosition(sw / 2 - g0 / 2, sh / 2 - g0 / 2);
        }

        // Пробегающая световая волна (деликатный блик по тексту слева направо)
        splashSheen_ = splashPanel_->CreateChild<BorderImage>();
        splashSheen_->SetBlendMode(BLEND_ADD);
        splashSheen_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
        splashSheen_->SetSize(S(150), S(64));
        splashSheen_->SetPosition(-S(400), sh / 2 - S(32));

        splashText_ = splashPanel_->CreateChild<Text>();
        splashText_->SetFont(font_, SF(36));
        splashText_->SetTextAlignment(HA_CENTER);
        splashText_->SetPosition(sw / 2 - S(720), sh / 2 - S(44));
        splashText_->SetWidth(S(1440));
        splashText_->SetColor(Color(0.90f, 0.96f, 1.0f, 0.0f));
        StyleCrispText(splashText_, true);

        splashLine_ = splashPanel_->CreateChild<BorderImage>();
        splashLine_->SetPosition(sw / 2, sh / 2 + S(20));
        splashLine_->SetSize(0, Max(2, S(3.0f)));
        splashLine_->SetColor(Color(0.20f, 0.92f, 1.0f, 0.0f));

        // Временная сцена для музыки: главная сцена игры создаётся лишь на шаге 3 загрузки
        splashScene_ = SharedPtr<Scene>(new Scene(context_));
        kochMusicNode_ = SharedPtr<Node>(splashScene_->CreateChild("KochTheme"));
        if (kochMusicNode_) {
            kochThemeSrc_ = kochMusicNode_->CreateComponent<SoundSource>();
            splashSfxSrc_ = kochMusicNode_->CreateComponent<SoundSource>();
            if (kochThemeSrc_) {
                if (Sound* theme = cache->GetResource<Sound>("Music/KochSplashTheme.wav")) {
                    theme->SetLooped(false);
                    kochThemeSrc_->SetGain(musicVolume_);
                    kochThemeSrc_->Play(theme);
                } else {
                    kochThemeSrc_ = nullptr;   // нет файла — немой показ, основная музыка вступит как обычно
                }
            }
        }

        splashStage_ = 0;
        splashTimer_ = 0.0f;
        splashActive_ = true;
        ConfigureSplashStage(0);
        SplashEnterStage(0);
    }

    void EndSplashIntro() {
        if (splashPanel_) { splashPanel_->Remove(); splashPanel_ = nullptr; }
        splashText_ = nullptr;
        splashLine_ = nullptr;
        splashGlow_ = nullptr;
        splashSheen_ = nullptr;
        splashFullText_ = String();
        splashActive_ = false;
        splashTimer_ = 0.0f;
        splashStage_ = 0;
        // Музыка продолжает звучать в фазе загрузки и затухает вместе с экраном (fade в UpdateBootLoading)
    }

    void UpdateSplashIntro(float dt) {
        auto* input = GetSubsystem<Input>();
        if (input) {   // скип заставки
            if (input->GetMouseButtonDown(MOUSEB_LEFT) || input->GetMouseButtonDown(MOUSEB_RIGHT) ||
                input->GetKeyPress(KEY_SPACE) || input->GetKeyPress(KEY_RETURN) ||
                input->GetKeyPress(KEY_ESCAPE)) {
                EndSplashIntro();
                return;
            }
        }
        splashTimer_ += dt;
        const int sw = GetSubsystem<Graphics>()->GetWidth();
        const int sh = GetSubsystem<Graphics>()->GetHeight();

        static const float kDur[3]   = { 1.30f, 1.20f, 2.55f };
        static const float kIn[3]    = { 0.60f, 0.55f, 0.10f };
        static const float kOutAt[3] = { 1.02f, 0.94f, 2.18f };
        static const int   kLineW[3] = { 300, 200, 840 };

        const int stage = splashStage_;
        const float t = splashTimer_;
        const float inT  = Clamp(t / kIn[stage], 0.0f, 1.0f);
        const float outT = Clamp((t - kOutAt[stage]) / (kDur[stage] - kOutAt[stage]), 0.0f, 1.0f);
        const float alpha = Clamp(EaseOutCubic(inT) * (1.0f - EaseOutCubic(outT)), 0.0f, 1.0f);

        // «Выплывание»: текст поднимается на easeOutExpo + деликатное дыхание в покое
        const float floatAmp = (float)S(stage == 2 ? 12 : 48);
        const float floatOff = (1.0f - EaseOutExpo(Clamp(t / 1.05f, 0.0f, 1.0f))) * floatAmp;
        const float breathe  = sinf(t * 1.9f + stage * 1.7f) * (float)S(2.5f)
                             * Clamp((t - 0.9f) / 0.5f, 0.0f, 1.0f);
        int drawX = sw / 2 - S(720);
        int drawY = SplashTextY(stage) + (int)floatOff + (int)breathe;

        if (stage == 2) {
            // CHICHKENCUM: наезд размера 104→76 и затухающая тряска от удара
            const float punch = 1.0f - EaseOutExpo(Clamp(t / 0.26f, 0.0f, 1.0f));
            splashText_->SetFont(font_, SF((int)(76.0f + 28.0f * punch)));
            drawX += (int)(sinf(t * 88.0f) * expf(-t * 9.5f) * (float)S(6.0f));
        } else {
            // Посимвольное открытие надписи
            const float reveal = EaseOutCubic(Clamp(t / 0.80f, 0.0f, 1.0f));
            const int n = (int)(splashFullText_.Length() * reveal + 0.49f);
            splashText_->SetText(splashFullText_.Substring(0, n));
        }

        // Цвет текста. CHICHKENCUM вспыхивает белым и остывает в золото
        Color base = stage == 0 ? Color(0.90f, 0.96f, 1.0f, 1.0f)
                   : stage == 1 ? Color(0.70f, 0.76f, 0.88f, 1.0f)
                                : Color(1.0f, 0.78f, 0.16f, 1.0f);
        if (stage == 2) {
            const float flash = EaseOutCubic(Clamp(t / 0.55f, 0.0f, 1.0f));
            base = Color(1.0f, Lerp(1.0f, 0.78f, flash), Lerp(1.0f, 0.16f, flash), 1.0f);
        }
        base.a_ = alpha;
        if (splashText_) {
            splashText_->SetColor(base);
            splashText_->SetPosition(drawX, drawY);
        }

        // Неоновое свечение за текстом (тинт стадии + пульс)
        if (splashGlow_) {
            static const Color kGlow[3] = {
                Color(0.18f, 0.82f, 1.0f, 1.0f),
                Color(1.00f, 0.55f, 0.15f, 1.0f),
                Color(1.00f, 0.60f, 0.12f, 1.0f)
            };
            const float pulse = 0.80f + 0.20f * sinf(t * 2.2f + stage * 2.1f);
            const int gsize = S(stage == 2 ? 1180 : 860);
            Color gc = kGlow[stage];
            gc.a_ = alpha * 0.60f * pulse;
            splashGlow_->SetSize(gsize, gsize);
            splashGlow_->SetPosition(sw / 2 - gsize / 2, sh / 2 - gsize / 2 + (int)floatOff + (int)breathe);
            splashGlow_->SetColor(gc);
        }

        // Волна-блик проскакивает слева направо в середине каждой стадии
        if (splashSheen_) {
            const float sweep = t - 0.68f;
            if (sweep > 0.0f && sweep < 0.85f && alpha > 0.20f) {
                const float k = sweep / 0.85f;
                splashSheen_->SetPosition((int)Lerp(-S(240), sw + S(40), k), drawY - S(8));
                Color sc(1.0f, 1.0f, 1.0f, 0.40f * alpha * sinf(3.14159265f * k));
                splashSheen_->SetColor(sc);
            } else {
                splashSheen_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
            }
        }

        // Неоновая линия раскрывается от центра (easeOutQuad)
        float lineT = Clamp(t / 0.5f, 0.0f, 1.0f);
        lineT = 1.0f - (1.0f - lineT) * (1.0f - lineT);
        const int w = (int)(S(kLineW[stage]) * lineT);
        if (splashLine_) {
            splashLine_->SetPosition(sw / 2 - w / 2, SplashLineY(stage));
            splashLine_->SetSize(Max(0, w), Max(2, S(stage == 2 ? 4.0f : 3.0f)));
            Color lc = stage == 0 ? Color(0.20f, 0.92f, 1.0f, 1.0f)
                     : stage == 1 ? Color(1.0f, 0.56f, 0.16f, 1.0f)
                                  : Color(1.0f, 0.36f, 0.14f, 1.0f);
            lc.a_ = 0.95f * alpha;
            splashLine_->SetColor(lc);
        }

        if (t >= kDur[stage]) {
            splashTimer_ = 0.0f;
            ++splashStage_;
            if (splashStage_ > 2) { EndSplashIntro(); return; }
            ConfigureSplashStage(splashStage_);
            SplashEnterStage(splashStage_);
        }
    }

    void StartAmbientLoop() {
        if (musicSource_) return;
        auto* cache = GetSubsystem<ResourceCache>();
        if (!cache || !cameraNode_) return;
        Sound* mus = cache->GetResource<Sound>("Music/Game_Theme_Loop.ogg");
        if (!mus) return;
        mus->SetLooped(true);
        if (!musicSource_) musicSource_ = cameraNode_->CreateComponent<SoundSource>();
        if (musicSource_) {
            musicSource_->SetGain(musicVolume_);
            musicSource_->Play(mus);
        }
    }

    // Короткий тактический экран высадки при старте миссии («ТИР» / «АРЕНА»)
    void TriggerMissionDeploy(GameMode mode) {
        auto* root = GetSubsystem<UI>()->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();
        if (deployPanel_) { deployPanel_->Remove(); deployPanel_ = nullptr; }

        deployPanel_ = root->CreateChild<UIElement>("DeployOverlay");
        deployPanel_->SetSize(sw, sh);
        deployPanel_->SetPriority(900);

        BorderImage* dbg = deployPanel_->CreateChild<BorderImage>();
        dbg->SetSize(sw, sh);
        dbg->SetColor(Color(0.008f, 0.018f, 0.038f, 0.92f));

        deployTitleText_ = deployPanel_->CreateChild<Text>();
        deployTitleText_->SetFont(font_, SF(46));
        deployTitleText_->SetText(mode == MODE_ARENA ? "ВЫСАДКА НА КИБЕР-АРЕНУ" : "ПОЛИГОН ВЫСОКОЙ ТОЧНОСТИ 4K");
        deployTitleText_->SetTextAlignment(HA_CENTER);
        deployTitleText_->SetPosition(sw / 2 - S(460), sh / 2 - S(70));
        deployTitleText_->SetWidth(S(920));
        deployTitleText_->SetColor(Color(1.0f, 0.86f, 0.25f, 1.0f));
        StyleCrispText(deployTitleText_, true);

        deployTipText_ = deployPanel_->CreateChild<Text>();
        deployTipText_->SetFont(font_, SF(18));
        deployTipText_->SetText(mode == MODE_ARENA
            ? "ЗАДАЧА: ОТРАЗИТЬ ВОЛНЫ КИБЕР-КУР И БОССА-ИНДЮКА  •  ИСПОЛЬЗУЙ РЫВОК [ALT] И БОЧКИ"
            : "ЗАДАЧА: ПОРАЗИТЬ СТАТИЧНЫЕ И ПОДВИЖНЫЕ МИШЕНИ  •  ЦЕНТР МИШЕНИ ДАЁТ КРИТ 1.5X");
        deployTipText_->SetTextAlignment(HA_CENTER);
        deployTipText_->SetPosition(sw / 2 - S(500), sh / 2 - S(6));
        deployTipText_->SetWidth(S(1000));
        deployTipText_->SetColor(Color(0.48f, 0.90f, 1.0f, 0.95f));
        StyleCrispText(deployTipText_);

        BorderImage* tr = deployPanel_->CreateChild<BorderImage>();
        tr->SetPosition(sw / 2 - S(260), sh / 2 + S(44));
        tr->SetSize(S(520), S(8));
        tr->SetColor(Color(0.08f, 0.14f, 0.22f, 1.0f));

        deployBarFill_ = deployPanel_->CreateChild<BorderImage>();
        deployBarFill_->SetPosition(sw / 2 - S(260), sh / 2 + S(44));
        deployBarFill_->SetSize(S(20), S(8));
        deployBarFill_->SetColor(Color(0.20f, 0.95f, 0.72f, 1.0f));

        deployDuration_ = 0.70f;
        deployTimer_ = deployDuration_;
    }

    void UpdateDeployOverlay(float dt) {
        if (deployTimer_ <= 0.0f || !deployPanel_) return;
        deployTimer_ -= dt;
        float elapsed = 1.0f - Clamp(deployTimer_ / Max(0.01f, deployDuration_), 0.0f, 1.0f);
        if (deployBarFill_) {
            deployBarFill_->SetSize((int)(S(520.0f) * Min(1.0f, elapsed * 1.45f)), S(8));
        }
        float alpha = deployTimer_ < 0.28f ? Clamp(deployTimer_ / 0.28f, 0.0f, 1.0f) : 1.0f;
        deployPanel_->SetOpacity(alpha);
        if (deployTimer_ <= 0.0f) {
            deployPanel_->Remove();
            deployPanel_ = nullptr;
            deployTitleText_ = nullptr;
            deployTipText_ = nullptr;
            deployBarFill_ = nullptr;
        }
    }

    // =========================================================================
    // Утилиты взрывов и урона
    // =========================================================================
    
    void ApplyExplosionDamage(const Vector3& center, float damage) {
        const float radius = 5.5f;
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
        for (auto& e : enemies_) {
            if (!e.alive_ || !e.node_) continue;
            float dist = (e.node_->GetPosition() - center).Length();
            if (dist <= radius) {
                float d = damage * (1.0f - dist / radius);
                SpawnFloatingDamage(e.node_->GetPosition() + Vector3(0, 1.2f, 0), (int)d, false);
                DamageEnemy(e, d);
            }
        }
        // Гранатомёт подрывает и взрывные бочки в радиусе!
        for (unsigned bi = 0; bi < barrels_.Size(); ++bi) {
            if (barrels_[bi].exploded_) continue;
            float dist = (barrels_[bi].home_ - center).Length();
            if (dist <= radius) DetonateBarrel(barrels_[bi]);
        }
        if (cameraNode_) {
            Vector3 camPos = cameraNode_->GetPosition();
            float pdist = (camPos - center).Length();
            if (pdist < radius && !gameOver_) {
                float falloff = 1.0f - pdist / radius;
                DamagePlayer(damage * falloff * 0.4f);
            }
        }
    }

    // =========================================================================
    // УРОН ПО ИГРОКУ (с поглощением 65% урона ТИТАНОВОЙ БРОНЁЙ) + GAME OVER
    // =========================================================================

    void DamagePlayer(float amount) {
        if (gameOver_ || amount <= 0.0f) return;
        // Во время реактивного рывка — снижаем входящий урон вдвое
        if (dashTimer_ > 0.0f) amount *= 0.5f;

        if (player_.armor_ > 0.0f) {
            float absorbed = Min(player_.armor_, amount * 0.65f);
            player_.armor_ = Max(0.0f, player_.armor_ - absorbed);
            amount -= absorbed;
        }
        player_.health_ -= amount;
        damageFlashTimer_ = 0.3f;
        AddCameraShake(Clamp(amount / 60.0f, 0.05f, 0.25f));
        UpdateHealthDisplay();
        if (player_.health_ <= 0.0f) {
            player_.health_ = 0.0f;
            TriggerGameOver();
        }
    }

    void TriggerGameOver() {
        gameOver_ = true;
        photoMode_ = false;
        player_.health_ = 0.0f;
        UpdateHealthDisplay();
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
                String("УБИЙСТВ: ") + String(player_.kills_) + "  (ХЭДШОТОВ ★: " + String(player_.headshots_) + ")   |   ЛУЧШЕЕ КОМБО: x" + String(bestCombo_) + "\n" +
                String("ТОЧНОСТЬ: ") + String((int)player_.GetAccuracy()) + "%   |   РЕКОРД: " + String(highScore_) + "\n" +
                String("ВРЕМЯ: ") + FormatTime(arenaTime_) + "   |   ТЕМП: " +
                String(arenaTime_ > 5.0f ? (int)(player_.kills_ / (arenaTime_ / 60.0f)) : 0) + " уб/мин");
        }
        ShowGameOver(true);
    }

    void ShowGameOver(bool show) {
        gameOver_ = show;
        if (show) gameOverPulse_ = 0.0f;
        if (gameOverPanel_) gameOverPanel_->SetVisible(show);
        SetGameHudVisible(!show);
        auto* input = GetSubsystem<Input>();
        if (show && input) input->SetMouseVisible(false);
    }

    void RestartArena() {
        ShowGameOver(false);
        StartGameMode(MODE_ARENA);
    }

    void CycleWeapon(int dir) {
        int next = (currentWeapon_ + dir + WEAPON_COUNT) % WEAPON_COUNT;
        EquipWeapon(next, false);
        PlayUISound();
        ShowMessage(String("Оружие: ") + WEAPON_DEFS[next].name);
    }

    void CycleSkin(int dir) {
        currentSkin_ = ((currentSkin_ + dir) % SKIN_COUNT + SKIN_COUNT) % SKIN_COUNT;
        PlayUISound();
        RefreshWeaponMaterials();
        BuildWeaponModel(currentWeapon_);
        if (weaponNameText_) weaponNameText_->SetText(String(WEAPON_DEFS[currentWeapon_].name) + "  •  " + String(SKIN_DEFS[currentSkin_].name));
        ShowMessage(String("PBR-скин: ") + SKIN_DEFS[currentSkin_].name);
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
        bg->SetColor(Color(0.012f, 0.022f, 0.045f, 0.88f));

        Text* title = armoryPanel_->CreateChild<Text>();
        title->SetFont(font_, SF(36));
        title->SetText("АРСЕНАЛ 4K PBR — CHIKENGUN 2.5");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - S(360), S(32));
        title->SetWidth(S(720));
        title->SetColor(Color(1.0f, 0.86f, 0.30f, 1.0f));
        StyleCrispText(title, true);

        Text* wHead = armoryPanel_->CreateChild<Text>();
        wHead->SetFont(font_, SF(22));
        wHead->SetText("ВООРУЖЕНИЕ (1–" + String((int)WEAPON_COUNT) + ")");
        wHead->SetPosition(sw / 2 - S(430), S(104));
        wHead->SetColor(Color(0.55f, 0.90f, 1.0f, 1.0f));
        StyleCrispText(wHead);

        for (int i = 0; i < WEAPON_COUNT; ++i) {
            Text* row = armoryPanel_->CreateChild<Text>();
            row->SetFont(font_, SF(18));
            const WeaponDef& d = WEAPON_DEFS[i];
            row->SetText(String(i + 1) + ". " + String(d.name) + "   DMG:" + String(d.damage, 0) +
                         "   ROF:" + String((int)(1.0f / d.fireRate)) + "/с   MAG:" + String(d.magSize));
            row->SetPosition(sw / 2 - S(430), S(138) + i * S(31));
            row->SetVar("index", i);
            StyleCrispText(row);
            armoryWeaponRows_.Push(SharedPtr<Text>(row));
        }

        Text* sHead = armoryPanel_->CreateChild<Text>();
        sHead->SetFont(font_, SF(22));
        sHead->SetText("PBR-ПОКРЫТИЯ (Q / E)");
        sHead->SetPosition(sw / 2 + S(60), S(104));
        sHead->SetWidth(S(420));
        sHead->SetColor(Color(0.55f, 1.0f, 0.80f, 1.0f));
        StyleCrispText(sHead);

        for (int i = 0; i < SKIN_COUNT; ++i) {
            Text* row = armoryPanel_->CreateChild<Text>();
            row->SetFont(font_, SF(19));
            const SkinDef& sk = SKIN_DEFS[i];
            String tag = sk.emissive ? " [HDR GLOW]" : (sk.metallic > 0.65f ? " [PBR МЕТАЛЛ]" : " [PBR МАТ]");
            row->SetText(String(i + 1) + ". " + String(sk.name) + tag);
            row->SetPosition(sw / 2 + S(60), S(142) + i * S(34));
            row->SetVar("index", i);
            StyleCrispText(row);
            armorySkinRows_.Push(SharedPtr<Text>(row));
        }

        Text* hint = armoryPanel_->CreateChild<Text>();
        hint->SetFont(font_, SF(16));
        hint->SetText("ЛКМ — выбрать ствол или PBR-скин   |   I — осмотреть в руках   |   TAB / ESC — закрыть");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - S(420), sh - S(64));
        hint->SetWidth(S(840));
        hint->SetColor(Color(0.82f, 0.88f, 0.95f, 1.0f));
        StyleCrispText(hint);

        armoryPanel_->SetVisible(false);
    }

    void UpdateArmoryHighlight() {
        for (unsigned i = 0; i < armoryWeaponRows_.Size(); ++i) {
            bool sel = ((int)i == currentWeapon_);
            armoryWeaponRows_[i]->SetColor(sel ? Color(1.0f, 0.86f, 0.22f, 1.0f) : Color(0.85f, 0.88f, 0.94f, 1.0f));
        }
        for (unsigned i = 0; i < armorySkinRows_.Size(); ++i) {
            bool sel = ((int)i == currentSkin_);
            Color sc = NormByteColor(SKIN_DEFS[i].color);
            if (sel) armorySkinRows_[i]->SetColor(Color(1.0f, 0.86f, 0.22f, 1.0f));
            else armorySkinRows_[i]->SetColor(Color(sc.r_ * 0.75f + 0.25f, sc.g_ * 0.75f + 0.25f, sc.b_ * 0.75f + 0.25f, 1.0f));
        }
    }

    void HandleArmoryClick(bool left) {
        if (!left || !armoryPanel_) return;
        auto* input = GetSubsystem<Input>();
        IntVector2 mp = input->GetMousePosition();
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

    // =========================================================================
    // НАСТРОЙКИ 4K ULTRA: settings.ini + мгновенное применение на лету
    // =========================================================================

    static int XmlGetInt(const XMLElement& el, const char* name, int def) {
        if (!el || !el.HasAttribute(name)) return def;
        String s = el.GetAttribute(name).Trimmed().ToLower();
        if (s.Empty()) return def;
        if (s == "true" || s == "on" || s == "yes") return 1;
        if (s == "false" || s == "off" || s == "no") return 0;
        return atoi(s.CString());
    }
    static void XmlSetInt(XMLElement& el, const char* name, int v) {
        el.SetString(name, String(v));
    }
    static float XmlGetFloat(const XMLElement& el, const char* name, float def, float lo, float hi) {
        if (!el || !el.HasAttribute(name)) return def;
        const char* raw = el.GetAttribute(name).CString();
        char* end = nullptr;
        float value = strtof(raw, &end);
        if (end == raw || !std::isfinite(value)) return def;
        return Clamp(value, lo, hi);
    }

    String LegacySettingsPath() const {
        auto* fs = GetSubsystem<FileSystem>();
        return fs ? (fs->GetProgramDir() + "settings.ini") : String("settings.ini");
    }

    String SettingsPath() const {
        auto* fs = GetSubsystem<FileSystem>();
        if (!fs) return String("settings.ini");
        // Не пишем рядом с бинарником: /usr/bin и Program Files часто read-only.
        // Старый путь остаётся в LegacySettingsPath() для бесшовной миграции.
        String dir = fs->GetAppPreferencesDir("CHIKENGUN", "CHIKENGUN2.1");
        if (dir.Empty()) return LegacySettingsPath();
        return AddTrailingSlash(dir) + "settings.ini";
    }

    void LoadSettingsFile() {
        auto* fs = GetSubsystem<FileSystem>();
        if (!fs) return;
        const String preferredPath = SettingsPath();
        String path = preferredPath;
        bool migrateLegacy = false;
        // Один раз подхватываем прежний settings.ini рядом с executable.
        if (!fs->FileExists(path)) {
            path = LegacySettingsPath();
            migrateLegacy = path != preferredPath;
        }
        if (!fs->FileExists(path)) return;
        SharedPtr<XMLFile> xml(new XMLFile(context_));
        if (!xml->LoadFile(path)) {
            // Битый пользовательский файл не должен скрывать рабочий legacy-конфиг.
            const String legacyPath = LegacySettingsPath();
            if (path == legacyPath || !fs->FileExists(legacyPath) || !xml->LoadFile(legacyPath)) return;
            path = legacyPath;
            migrateLegacy = path != preferredPath;
        }
        XMLElement rootEl = xml->GetRoot();
        if (!rootEl) return;

        XMLElement gfx = rootEl.GetChild("graphics");
        if (!gfx) gfx = rootEl;

        gfxQualityIdx_  = Clamp(XmlGetInt(gfx, "qualityPreset", gfxQualityIdx_), 0, 3);
        gfxResIdx_      = Clamp(XmlGetInt(gfx, "resPreset",     gfxResIdx_),     0, 4);
        gfxDisplayMode_ = Clamp(XmlGetInt(gfx, "displayMode",   gfxDisplayMode_),0, 2);
        gfxMsaaIdx_     = Clamp(XmlGetInt(gfx, "msaaIdx",       gfxMsaaIdx_),    0, 3);
        if (!gfx.HasAttribute("msaaIdx") && gfx.HasAttribute("msaa")) {
            int m = XmlGetInt(gfx, "msaa", 4);
            gfxMsaaIdx_ = (m >= 8) ? 3 : (m >= 4 ? 2 : (m >= 2 ? 1 : 0));
        }
        gfxShadowIdx_   = Clamp(XmlGetInt(gfx, "shadows",     gfxShadowIdx_),  0, 2);
        gfxGlowIdx_     = Clamp(XmlGetInt(gfx, "bloom",       gfxGlowIdx_),    0, 2);
        gfxSsao_        = XmlGetInt(gfx, "ssao",    gfxSsao_    ? 1 : 0) != 0;
        gfxHdr_         = XmlGetInt(gfx, "hdr",     gfxHdr_     ? 1 : 0) != 0;
        gfxFxaa_        = XmlGetInt(gfx, "fxaa",    gfxFxaa_    ? 1 : 0) != 0;
        gfxSharpen_     = XmlGetInt(gfx, "sharpen", gfxSharpen_ ? 1 : 0) != 0;
        gfxVsync_       = XmlGetInt(gfx, "vsync",   gfxVsync_   ? 1 : 0) != 0;
        gfxFpsIdx_      = Clamp(XmlGetInt(gfx, "fps",         gfxFpsIdx_),     0, 2);
        gfxAnisoIdx_    = Clamp(XmlGetInt(gfx, "anisotropicIdx", gfxAnisoIdx_), 0, 2);
        if (!gfx.HasAttribute("anisotropicIdx") && gfx.HasAttribute("anisotropic")) {
            int a = XmlGetInt(gfx, "anisotropic", 16);
            gfxAnisoIdx_ = (a >= 8) ? 2 : (a >= 2 ? 1 : 0);
        }
        fovBase_        = (float)Clamp(XmlGetInt(gfx, "fov", (int)fovBase_), 75, 105);
        mouseSensIdx_   = Clamp(XmlGetInt(gfx, "mouse",       mouseSensIdx_),  0, 2);
        atmosphereIdx_  = Clamp(XmlGetInt(gfx, "atmosphere",  atmosphereIdx_), 0, 3);

        XMLElement snd = rootEl.GetChild("sound");
        if (!snd) snd = rootEl;
        sfxVolume_ = XmlGetFloat(snd, "sfx", sfxVolume_, 0.0f, 1.0f);
        musicVolume_ = XmlGetFloat(snd, "music", musicVolume_, 0.0f, 1.0f);

        XMLElement prog = rootEl.GetChild("progress");
        if (!prog) prog = rootEl;
        if (prog.HasAttribute("highscore"))
            highScore_ = Max(0, XmlGetInt(prog, "highscore", highScore_));
        if (prog.HasAttribute("achievements"))
            achievementsMask_ = Clamp(XmlGetInt(prog, "achievements", achievementsMask_), 0, Cyber::ALL_ACHIEVEMENTS_MASK);
        if (prog.HasAttribute("banyaSeen"))
            banyaSim_.lifetime.seenEndingsMask = Clamp(XmlGetInt(prog, "banyaSeen", 0), 0, Banya::ALL_ENDINGS_MASK);
        if (prog.HasAttribute("banyaRuns"))
            banyaSim_.lifetime.runs = Max(0, XmlGetInt(prog, "banyaRuns", 0));
        if (prog.HasAttribute("banyaWins"))
            banyaSim_.lifetime.wins = Max(0, XmlGetInt(prog, "banyaWins", 0));
        if (prog.HasAttribute("banyaBest"))
            banyaSim_.lifetime.bestScore = Max(0, XmlGetInt(prog, "banyaBest", 0));
        if (prog.HasAttribute("banyaBuff"))
            banyaBuffTier_ = Clamp(XmlGetInt(prog, "banyaBuff", 0), 0, 2);
        if (prog.HasAttribute("banyaCredits"))
            banyaBonusCredits_ = Clamp(XmlGetInt(prog, "banyaCredits", 0), 0, 50000);
        if (prog.HasAttribute("casinoBalance"))
            casinoBalance_ = Clamp(XmlGetInt(prog, "casinoBalance", casinoBalance_), 0, 100000000);
        if (prog.HasAttribute("casinoWins"))
            casinoWins_ = Max(0, XmlGetInt(prog, "casinoWins", casinoWins_));
        if (prog.HasAttribute("casinoLosses"))
            casinoLosses_ = Max(0, XmlGetInt(prog, "casinoLosses", casinoLosses_));

        if (migrateLegacy) SaveSettingsFile();
    }

    void SaveSettingsFile() {
        SharedPtr<XMLFile> xml(new XMLFile(context_));
        XMLElement rootEl = xml->CreateRoot("chikengun");
        XMLElement gfx = rootEl.CreateChild("graphics");
        static const int kMsaa[4] = { 1, 2, 4, 8 };
        static const int kAniso[3] = { 1, 4, 16 };
        XmlSetInt(gfx, "qualityPreset", gfxQualityIdx_);
        XmlSetInt(gfx, "resPreset",     gfxResIdx_);
        XmlSetInt(gfx, "displayMode",   gfxDisplayMode_);
        XmlSetInt(gfx, "msaa",          kMsaa[Clamp(gfxMsaaIdx_, 0, 3)]);
        XmlSetInt(gfx, "msaaIdx",       gfxMsaaIdx_);
        XmlSetInt(gfx, "shadows",       gfxShadowIdx_);
        XmlSetInt(gfx, "bloom",         gfxGlowIdx_);
        XmlSetInt(gfx, "ssao",          gfxSsao_    ? 1 : 0);
        XmlSetInt(gfx, "hdr",           gfxHdr_     ? 1 : 0);
        XmlSetInt(gfx, "fxaa",          gfxFxaa_    ? 1 : 0);
        XmlSetInt(gfx, "sharpen",       gfxSharpen_ ? 1 : 0);
        XmlSetInt(gfx, "vsync",         gfxVsync_   ? 1 : 0);
        XmlSetInt(gfx, "fps",           gfxFpsIdx_);
        XmlSetInt(gfx, "anisotropic",   kAniso[Clamp(gfxAnisoIdx_, 0, 2)]);
        XmlSetInt(gfx, "anisotropicIdx",gfxAnisoIdx_);
        XmlSetInt(gfx, "fov",           (int)fovBase_);
        XmlSetInt(gfx, "mouse",         mouseSensIdx_);
        XmlSetInt(gfx, "atmosphere",    atmosphereIdx_);
        XMLElement snd = rootEl.CreateChild("sound");
        snd.SetString("sfx", String(sfxVolume_));
        snd.SetString("music", String(musicVolume_));
        XMLElement prog = rootEl.CreateChild("progress");
        XmlSetInt(prog, "highscore",    highScore_);
        XmlSetInt(prog, "achievements", achievementsMask_);
        XmlSetInt(prog, "banyaSeen",    banyaSim_.lifetime.seenEndingsMask);
        XmlSetInt(prog, "banyaRuns",    banyaSim_.lifetime.runs);
        XmlSetInt(prog, "banyaWins",    banyaSim_.lifetime.wins);
        XmlSetInt(prog, "banyaBest",    banyaSim_.lifetime.bestScore);
        XmlSetInt(prog, "banyaBuff",     banyaBuffTier_);
        XmlSetInt(prog, "banyaCredits",  banyaBonusCredits_);
        XmlSetInt(prog, "casinoBalance", casinoBalance_);
        XmlSetInt(prog, "casinoWins",    casinoWins_);
        XmlSetInt(prog, "casinoLosses",  casinoLosses_);
        auto* fs = GetSubsystem<FileSystem>();
        if (!fs) return;

        auto saveTo = [&](const String& dst) -> bool {
            String dir = GetPath(dst);
            if (!dir.Empty() && !fs->DirExists(dir)) fs->CreateDir(dir);
            String tmp = dst + ".tmp";
            if (!xml->SaveFile(tmp)) return false;
            // На POSIX rename() заменяет dst атомарно. Не удаляем старый
            // файл заранее: при сбое записи пользователь должен сохранить
            // предыдущие рабочие настройки.
            if (fs->Rename(tmp, dst)) return true;
            // Windows MoveFile не заменяет существующий dst. Только после
            // неудачного атомарного пути используем совместимый fallback;
            // если удаление не удалось, старый файл остаётся нетронутым.
            if (fs->FileExists(dst) && fs->Delete(dst) && fs->Rename(tmp, dst)) return true;
            // Крайний fallback для файловых систем без rename-overwrite.
            if (xml->SaveFile(dst)) {
                if (fs->FileExists(tmp)) fs->Delete(tmp);
                return true;
            }
            if (fs->FileExists(tmp)) fs->Delete(tmp);
            return false;
        };

        // Предпочтительно пользовательская директория; при отказе доступа
        // оставляем совместимый fallback рядом с бинарником.
        if (!saveTo(SettingsPath())) saveTo(LegacySettingsPath());
    }

    void ApplyDisplayMode() {
        auto* gfx = GetSubsystem<Graphics>();
        if (!gfx) return;
        static const int kResW[5] = { 0, 3840, 2560, 1920, 1600 };
        static const int kResH[5] = { 0, 2160, 1440, 1080,  900 };
        static const int kMsaa[4] = { 1, 2, 4, 8 };

        int idx = Clamp(gfxResIdx_, 0, 4);
        int w = kResW[idx], h = kResH[idx];
        if (idx == 0) {
            IntVector2 desk = gfx->GetDesktopResolution(0);
            w = desk.x_ > 0 ? desk.x_ : 1920;
            h = desk.y_ > 0 ? desk.y_ : 1080;
            if (gfxDisplayMode_ == 0 && w > 1920) { w = 1920; h = 1080; }
        }
        bool full = (gfxDisplayMode_ == 2);
        bool borderless = (gfxDisplayMode_ == 1);
        int msaa = kMsaa[Clamp(gfxMsaaIdx_, 0, 3)];

        gfx->SetMode(w, h, full, borderless, true, true, gfxVsync_, true, msaa, 0, 0);
        pendingUiRebuild_ = true;
    }

    void ApplyTextureFiltering() {
        static const int kAniso[3] = { 1, 4, 16 };
        TextureFilterMode fm = FILTER_ANISOTROPIC;
        if (gfxAnisoIdx_ == 0) fm = FILTER_BILINEAR;
        else if (gfxAnisoIdx_ == 1) fm = FILTER_TRILINEAR;

        auto* renderer = GetSubsystem<Renderer>();
        auto* gfx = GetSubsystem<Graphics>();
        int aniso = kAniso[Clamp(gfxAnisoIdx_, 0, 2)];
        if (renderer) {
            renderer->SetTextureFilterMode(fm);
            renderer->SetTextureAnisotropy(aniso);
        }
        if (gfx) gfx->SetDefaultTextureAnisotropy((unsigned)aniso);

        for (unsigned i = 0; i < anisoTextures_.Size(); ++i) {
            if (anisoTextures_[i]) {
                anisoTextures_[i]->SetFilterMode(fm);
                anisoTextures_[i]->SetAnisotropy((unsigned)aniso);
            }
        }
    }

    void ApplyGraphicsSettings() {
        auto* renderer = GetSubsystem<Renderer>();
        if (!renderer) return;

        int q = Clamp(gfxQualityIdx_, 0, 3);
        renderer->SetMaterialQuality(q >= 2 ? QUALITY_HIGH : (q == 1 ? QUALITY_MEDIUM : QUALITY_LOW));
        renderer->SetTextureQuality(QUALITY_MAX);
        renderer->SetSpecularLighting(q >= 1);

        static const int shadowSizes[3] = { 1024, 2048, 4096 };
        renderer->SetDrawShadows(q > 0);
        renderer->SetShadowMapSize((unsigned)shadowSizes[Clamp(gfxShadowIdx_, 0, 2)]);
        renderer->SetShadowQuality(q >= 2 ? SHADOWQUALITY_PCF_24BIT : SHADOWQUALITY_PCF_16BIT);
        renderer->SetShadowSoftness(q >= 3 ? 1.15f : 1.0f);

        renderer->SetHDRRendering(gfxHdr_);

        static const int fpsValues[3] = { 60, 120, 0 };
        if (engine_) engine_->SetMaxFps(fpsValues[Clamp(gfxFpsIdx_, 0, 2)]);

        ApplyTextureFiltering();
        SyncPbrLightScale();
        SyncPostProcessParams();

        if (soundSource_) soundSource_->SetGain(sfxVolume_);
        if (musicSource_) musicSource_->SetGain(musicVolume_);
    }

    void ApplyQualityPresetDefaults() {
        switch (Clamp(gfxQualityIdx_, 0, 3)) {
        case 0:
            gfxShadowIdx_ = 0; gfxGlowIdx_ = 0; gfxSsao_ = false;
            gfxHdr_ = false;   gfxFxaa_ = false; gfxSharpen_ = false; gfxAnisoIdx_ = 0;
            break;
        case 1:
            gfxShadowIdx_ = 1; gfxGlowIdx_ = 1; gfxSsao_ = false;
            gfxHdr_ = true;    gfxFxaa_ = true;  gfxSharpen_ = false; gfxAnisoIdx_ = 1;
            break;
        case 2:
            gfxShadowIdx_ = 1; gfxGlowIdx_ = 2; gfxSsao_ = true;
            gfxHdr_ = true;    gfxFxaa_ = true;  gfxSharpen_ = true;  gfxAnisoIdx_ = 2;
            break;
        default:
            gfxShadowIdx_ = 2; gfxGlowIdx_ = 2; gfxSsao_ = true;
            gfxHdr_ = true;    gfxFxaa_ = true;  gfxSharpen_ = true;  gfxAnisoIdx_ = 2;
            break;
        }
    }

    // =========================================================================
    // ГЛАВНОЕ МЕНЮ (4K DPI-масштабируемый кибер-экран)
    // =========================================================================

    void CreateMainMenu() {
        if (mainMenuPanel_) return;
        RefreshUIScale();
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        mainMenuPanel_ = root->CreateChild<UIElement>("MainMenuPanel");
        mainMenuPanel_->SetSize(sw, sh);

        BorderImage* bg = mainMenuPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.010f, 0.018f, 0.042f, 0.76f));
        for (int i = 0; i < 16; ++i) {
            BorderImage* band = mainMenuPanel_->CreateChild<BorderImage>();
            float t = i / 15.0f;
            float edge = fabsf(t - 0.5f) * 2.0f;
            band->SetPosition(int(t * sw), 0);
            band->SetSize(sw / 16 + 2, sh);
            band->SetColor(Color(0.0f, 0.005f, 0.02f, 0.08f + edge * edge * 0.30f));
        }
        BorderImage* topShade = mainMenuPanel_->CreateChild<BorderImage>();
        topShade->SetPosition(0, 0); topShade->SetSize(sw, sh / 6);
        topShade->SetColor(Color(0.0f, 0.0f, 0.01f, 0.36f));
        BorderImage* botShade = mainMenuPanel_->CreateChild<BorderImage>();
        botShade->SetPosition(0, sh - sh / 6); botShade->SetSize(sw, sh / 6);
        botShade->SetColor(Color(0.0f, 0.0f, 0.01f, 0.46f));

        for (int i = 0; i < 28; ++i) {
            BorderImage* p = mainMenuPanel_->CreateChild<BorderImage>();
            int sz = Max(2, S(2.0f + rand() % 3));
            p->SetSize(sz, sz * 3);
            float hue = (rand() % 100) / 100.0f;
            Color pc = (hue < 0.5f)
                ? Color(0.35f + hue * 0.6f, 0.78f, 1.0f, 0.55f)
                : Color(1.0f, 0.82f - (hue - 0.5f) * 0.5f, 0.25f, 0.55f);
            p->SetColor(pc);
            menuParticles_.Push(SharedPtr<BorderImage>(p));
            menuParticlePhase_.Push(rand() % 1000 / 1000.0f * 6.283f);
            menuParticleX_.Push(float(rand() % Max(1, sw)));
            menuParticleSpeed_.Push((18.0f + rand() % 42) * uiScale_);
        }

        int plateW = S(720), plateH = S(620);
        menuPlate_ = mainMenuPanel_->CreateChild<BorderImage>();
        menuPlate_->SetPosition(sw / 2 - plateW / 2, sh / 2 - S(316));
        menuPlate_->SetSize(plateW, plateH);
        menuPlate_->SetColor(Color(0.018f, 0.042f, 0.088f, 0.64f));

        int bTh = Max(2, S(2.0f));
        struct Edge { int x, y, w, h; };
        Edge edges[4] = {
            { sw/2 - plateW/2 - bTh, sh/2 - S(316) - bTh, plateW + bTh*2, bTh },
            { sw/2 - plateW/2 - bTh, sh/2 - S(316) + plateH, plateW + bTh*2, bTh },
            { sw/2 - plateW/2 - bTh, sh/2 - S(316) - bTh, bTh, plateH + bTh*2 },
            { sw/2 + plateW/2,       sh/2 - S(316) - bTh, bTh, plateH + bTh*2 }
        };
        for (int i = 0; i < 4; ++i) {
            BorderImage* e = mainMenuPanel_->CreateChild<BorderImage>();
            e->SetPosition(edges[i].x, edges[i].y);
            e->SetSize(edges[i].w, edges[i].h);
            e->SetColor(Color(0.25f, 0.82f, 1.0f, 0.42f));
        }

        Text* title = mainMenuPanel_->CreateChild<Text>();
        title->SetFont(font_, SF(64));
        title->SetText("CHIKENGUN 2.5");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - S(420), sh / 2 - S(252));
        title->SetWidth(S(840));
        title->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));
        StyleCrispText(title, true);
        menuTitle_ = SharedPtr<Text>(title);

        menuAccentBar_ = mainMenuPanel_->CreateChild<BorderImage>();
        menuAccentBar_->SetPosition(sw / 2 - S(200), sh / 2 - S(170));
        menuAccentBar_->SetSize(S(400), Max(2, S(2.5f)));
        menuAccentBar_->SetColor(Color(1.0f, 0.8f, 0.2f, 0.9f));

        Text* sub = mainMenuPanel_->CreateChild<Text>();
        sub->SetFont(font_, SF(16));
        sub->SetText("[ 2.5 ПЕРО СУДЬБЫ ]   •   [ 4K ULTRA PBR ]   •   [ КОЧЕТ В БАНЬКЕ ]");
        sub->SetTextAlignment(HA_CENTER);
        sub->SetPosition(sw / 2 - S(440), sh / 2 - S(154));
        sub->SetWidth(S(880));
        sub->SetColor(Color(0.48f, 0.86f, 1.0f, 0.95f));
        StyleCrispText(sub);
        menuSub_ = SharedPtr<Text>(sub);

        const char* items[6] = {
            "ПОЛИГОН (тир 4K)",
            "КИБЕР-АРЕНА (волны + босс)",
            "КОЧЕТ В БАНЬКЕ (симулятор парной)",
            "ЗОЛОТАЯ РУЛЕТКА (фейковые фишки)",
            "НАСТРОЙКИ 4K ULTRA",
            "ВЫХОД"
        };
        for (int i = 0; i < 6; ++i) {
            BorderImage* rowBg = mainMenuPanel_->CreateChild<BorderImage>();
            rowBg->SetPosition(sw / 2 - S(300), sh / 2 - S(100) + i * S(49));
            rowBg->SetSize(S(600), S(42));
            rowBg->SetColor(Color(0.05f, 0.14f, 0.24f, i == menuSel_ ? 0.52f : 0.0f));
            rowBg->SetName(String("MenuRow") + String(i));
            menuRows_.Push(rowBg);

            Text* it = mainMenuPanel_->CreateChild<Text>();
            it->SetFont(font_, SF(25));
            it->SetText(items[i]);
            it->SetPosition(sw / 2 - S(290), sh / 2 - S(94) + i * S(49));
            it->SetWidth(S(580));
            StyleCrispText(it);
            mainMenuItems_.Push(SharedPtr<Text>(it));
        }

        Text* hint = mainMenuPanel_->CreateChild<Text>();
        hint->SetFont(font_, SF(15));
        hint->SetText("W/S — выбор   •   ENTER — запуск   •   [P] БАНЬКА   •   РУЛЕТКА — виртуальные фишки   •   F11 — полный экран");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - S(440), sh / 2 + S(236));
        hint->SetWidth(S(880));
        hint->SetColor(Color(0.75f, 0.82f, 0.90f, 0.92f));
        StyleCrispText(hint);
        menuHint_ = SharedPtr<Text>(hint);

        UpdateMainMenuSelection();
    }

    void AnimateMainMenu(float dt) {
        (void)dt;
        if (!mainMenuPanel_ || !mainMenuPanel_->IsVisible()) return;
        auto* gfx = GetSubsystem<Graphics>();
        int sw = gfx->GetWidth(), sh = gfx->GetHeight();

        if (!menuBasesSet_) {
            if (menuTitle_)      menuTitleBase_  = menuTitle_->GetPosition();
            if (menuPlate_)      menuPlateBase_  = menuPlate_->GetPosition();
            if (menuAccentBar_)  menuBarBase_    = menuAccentBar_->GetPosition();
            menuBasesSet_ = true;
        }
        if (menuTitle_) {
            float pulse = 0.78f + 0.22f * sinf(timeAcc_ * 1.6f);
            menuTitle_->SetColor(Color(pulse, pulse * 0.84f, pulse * 0.28f, 1.0f));
        }
        if (menuAccentBar_) {
            int w = S(310.0f + 130.0f * (0.5f + 0.5f * sinf(timeAcc_ * 1.2f)));
            menuAccentBar_->SetSize(w, Max(2, S(2.5f)));
            menuAccentBar_->SetColor(Color(1.0f, 0.8f, 0.2f, 0.55f + 0.45f * sinf(timeAcc_ * 2.4f)));
        }
        if (menuSub_) {
            float flick = (sinf(timeAcc_ * 7.0f) > 0.985f) ? 0.35f : 0.92f;
            menuSub_->SetColor(Color(0.48f, 0.86f, 1.0f, flick));
        }
        if (menuHint_) {
            menuHint_->SetColor(Color(0.75f, 0.82f, 0.90f, 0.58f + 0.36f * sinf(timeAcc_ * 2.0f)));
        }
        for (unsigned i = 0; i < menuParticles_.Size(); ++i) {
            BorderImage* p = menuParticles_[i].Get();
            float speed = menuParticleSpeed_[i];
            float phase = menuParticlePhase_[i];
            int py = sh - int(fmodf(timeAcc_ * speed + phase * sh, float(sh + 40))) + 20;
            int px = int(menuParticleX_[i]) + int(sinf(timeAcc_ * 0.7f + phase * 6.283f) * S(14.0f));
            p->SetPosition(px, py);
            float a = 0.55f * (float(py) / float(Max(1, sh))) + 0.1f;
            Color c = p->GetColor(C_TOPLEFT); c.a_ = a; p->SetColor(c);
        }
        auto* input = GetSubsystem<Input>();
        IntVector2 mp = input->GetMousePosition();
        float nx = (float(mp.x_) / Max(1, sw) - 0.5f);
        float ny = (float(mp.y_) / Max(1, sh) - 0.5f);
        if (menuPlate_) {
            menuPlate_->SetPosition(menuPlateBase_.x_ + int(nx * S(10.0f)),
                                    menuPlateBase_.y_ + int(ny * S(6.0f)));
        }
        if (menuTitle_) {
            menuTitle_->SetPosition(menuTitleBase_.x_ - int(nx * S(7.0f)),
                                    menuTitleBase_.y_ + int(sinf(timeAcc_ * 0.8f) * S(3.0f)));
        }
        if (menuAccentBar_) {
            int w = menuAccentBar_->GetWidth();
            menuAccentBar_->SetPosition(sw / 2 - w / 2 - int(nx * S(4.0f)), menuBarBase_.y_);
        }
    }

    void ShowMainMenu(bool show) {
        inMenu_ = show;
        if (show) {
            photoMode_ = false;
            if (weaponNode_) weaponNode_->SetEnabled(true);
            if (!mainMenuPanel_) CreateMainMenu();
            mainMenuPanel_->SetVisible(true);
            SetGameHudVisible(false);
            auto* input = GetSubsystem<Input>();
            if (input) input->SetMouseVisible(false);
        } else {
            if (mainMenuPanel_) mainMenuPanel_->SetVisible(false);
            SetGameHudVisible(!photoMode_);
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
        if (hudArmorBarBg_) hudArmorBarBg_->SetVisible(vis);
        if (hudArmorBarFill_) hudArmorBarFill_->SetVisible(vis);
        if (hudDashBarBg_) hudDashBarBg_->SetVisible(vis);
        if (hudDashBarFill_) hudDashBarFill_->SetVisible(vis);
        if (statusBuffText_) statusBuffText_->SetVisible(vis);
        if (hudAmmoBarBg_) hudAmmoBarBg_->SetVisible(vis);
        if (hudAmmoBarFill_) hudAmmoBarFill_->SetVisible(vis);
        if (radarBg_) radarBg_->SetVisible(vis);
        if (damageOverlay_) damageOverlay_->SetVisible(vis);
    }

    void UpdateMainMenuSelection() {
        static const char* labels[6] = {
            "ПОЛИГОН (тир 4K)",
            "КИБЕР-АРЕНА (волны + босс)",
            "КОЧЕТ В БАНЬКЕ (симулятор парной)",
            "ЗОЛОТАЯ РУЛЕТКА (фейковые фишки)",
            "НАСТРОЙКИ 4K ULTRA",
            "ВЫХОД"
        };
        auto* gfx = GetSubsystem<Graphics>();
        int sw = gfx->GetWidth();
        for (unsigned i = 0; i < mainMenuItems_.Size(); ++i) {
            bool sel = ((int)i == menuSel_);
            mainMenuItems_[i]->SetText((sel ? "\xe2\x96\xb6  " : "   ") + String(labels[i]) + (sel ? "  \xe2\x97\x80" : ""));
            Color unselCol = (i == 2 || i == 3) ? Color(0.96f, 0.76f, 0.48f, 1.0f) : Color(0.58f, 0.66f, 0.76f, 1.0f);
            mainMenuItems_[i]->SetColor(sel ? Color(1.0f, 0.87f, 0.30f, 1.0f) : unselCol);
            if ((int)menuRows_.Size() > (int)i && i < mainMenuItems_.Size()) {
                BorderImage* bi = static_cast<BorderImage*>(menuRows_[i]);
                if (bi) {
                    bi->SetColor((i == 2 || i == 3)
                        ? Color(0.28f, 0.14f, 0.05f, sel ? 0.68f : 0.22f)
                        : Color(0.06f, 0.16f, 0.28f, sel ? 0.56f : 0.0f));
                    IntVector2 base = bi->GetPosition();
                    bi->SetPosition(sw / 2 - S(300) + (sel ? S(5) : 0), base.y_);
                }
            }
        }
    }

    // =========================================================================
    // ПАНЕЛЬ НАСТРОЕК 4K ULTRA (14 интерактивных строк)
    // =========================================================================

    void CreateSettingsPanel() {
        if (settingsPanel_) return;
        RefreshUIScale();
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        settingsPanel_ = root->CreateChild<UIElement>("SettingsPanel");
        settingsPanel_->SetSize(sw, sh);

        BorderImage* bg = settingsPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.008f, 0.016f, 0.036f, 0.93f));

        BorderImage* card = settingsPanel_->CreateChild<BorderImage>();
        int cw = S(820), ch = S(610);
        card->SetPosition(sw / 2 - cw / 2, sh / 2 - ch / 2);
        card->SetSize(cw, ch);
        card->SetColor(Color(0.016f, 0.032f, 0.064f, 0.82f));

        Text* title = settingsPanel_->CreateChild<Text>();
        title->SetFont(font_, SF(34));
        title->SetText("НАСТРОЙКИ ГРАФИКИ 4K ULTRA И ЗВУКА");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - S(400), sh / 2 - S(288));
        title->SetWidth(S(800));
        title->SetColor(Color(1.0f, 0.86f, 0.25f, 1.0f));
        StyleCrispText(title, true);

        settingsItems_.Clear();
        for (int i = 0; i < 14; ++i) {
            Text* row = settingsPanel_->CreateChild<Text>();
            row->SetFont(font_, SF(19));
            row->SetPosition(sw / 2 - S(360), sh / 2 - S(226) + i * S(34));
            row->SetWidth(S(720));
            StyleCrispText(row);
            settingsItems_.Push(SharedPtr<Text>(row));
        }

        Text* hint = settingsPanel_->CreateChild<Text>();
        hint->SetFont(font_, SF(15));
        hint->SetText("W/S — строка   •   A/D или стрелки — изменить   •   ENTER / ESC — сохранить и вернуться");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - S(450), sh / 2 + S(264));
        hint->SetWidth(S(900));
        hint->SetColor(Color(0.68f, 0.84f, 0.96f, 0.92f));
        StyleCrispText(hint);

        UpdateSettingsDisplay();
    }

    void ShowSettings(bool show) {
        inSettings_ = show;
        if (show) {
            if (!settingsPanel_) CreateSettingsPanel();
            settingsPanel_->SetVisible(true);
            UpdateSettingsDisplay();
        } else {
            if (settingsPanel_) settingsPanel_->SetVisible(false);
            SaveSettingsFile();
            PlayUISound();
        }
    }

    void UpdateSettingsDisplay() {
        static const char* qualNames[4]   = { "НИЗКИЙ", "СРЕДНИЙ", "ВЫСОКИЙ PBR", "4K ULTRA PBR" };
        static const char* resNames[5]    = { "АВТО (монитор)", "3840×2160 (4K UHD)", "2560×1440 (2K QHD)", "1920×1080 (Full HD)", "1600×900 (HD+)" };
        static const char* modeNames[3]   = { "ОКОННЫЙ", "БЕЗ РАМКИ (Borderless)", "ПОЛНЫЙ ЭКРАН" };
        static const char* msaaNames[4]   = { "ВЫКЛ (1x)", "2x MSAA", "4x MSAA", "8x MSAA" };
        static const char* shadowNames[3] = { "1024 px", "2048 px", "4096 px (4K PCF)" };
        static const char* bloomNames[3]  = { "ВЫКЛ", "МЯГКИЙ HDR", "КИНЕМАТОГРАФИЧНЫЙ HDR" };
        static const char* filterNames[3] = { "Билинейная", "Трилинейная", "Анизотропная 16x" };
        static const char* fpsNames[3]    = { "60 FPS", "120 FPS", "БЕЗ ЛИМИТА" };
        static const char* sensNames[3]   = { "Низкая", "Обычная", "Высокая" };

        const char* names[14] = {
            "Пресет графики",
            "Разрешение рендера",
            "Режим экрана",
            "Сглаживание MSAA + FXAA",
            "Карты теней (4 каскада)",
            "SSAO4K (затенение углов)",
            "HDR Bloom + ACES Tonemap",
            "Резкость CAS 4K Contrast",
            "Фильтрация 4K текстур",
            "Лимит кадров / VSync",
            "Поле зрения камеры (FOV)",
            "Чувствительность мыши",
            "Громкость эффектов / музыки",
            "Готово"
        };

        String vals[14];
        vals[0]  = qualNames[Clamp(gfxQualityIdx_, 0, 3)];
        vals[1]  = resNames[Clamp(gfxResIdx_, 0, 4)];
        vals[2]  = modeNames[Clamp(gfxDisplayMode_, 0, 2)];
        vals[3]  = String(msaaNames[Clamp(gfxMsaaIdx_, 0, 3)]) + (gfxFxaa_ ? " + FXAA3" : "");
        vals[4]  = shadowNames[Clamp(gfxShadowIdx_, 0, 2)];
        vals[5]  = gfxSsao_ ? "ВКЛ (16 сэмплов)" : "ВЫКЛ";
        vals[6]  = String(bloomNames[Clamp(gfxGlowIdx_, 0, 2)]) + (gfxHdr_ ? " [HDR ON]" : " [SDR]");
        vals[7]  = gfxSharpen_ ? "ВКЛ (Чёткость 4K)" : "ВЫКЛ";
        vals[8]  = filterNames[Clamp(gfxAnisoIdx_, 0, 2)];
        vals[9]  = String(fpsNames[Clamp(gfxFpsIdx_, 0, 2)]) + (gfxVsync_ ? " | VSync ВКЛ" : " | VSync ВЫКЛ");
        vals[10] = String((int)fovBase_) + "°";
        vals[11] = sensNames[Clamp(mouseSensIdx_, 0, 2)];
        vals[12] = "SFX " + String((int)(sfxVolume_ * 100.0f + 0.5f)) + "%  /  МУЗ " + String((int)(musicVolume_ * 100.0f + 0.5f)) + "%";
        vals[13] = "◀ СОХРАНИТЬ И ВЫЙТИ В МЕНЮ";

        for (unsigned i = 0; i < settingsItems_.Size() && i < 14; ++i) {
            bool sel = ((int)i == settingsSel_);
            if (i == 13)
                settingsItems_[i]->SetText((sel ? "▶  " : "   ") + vals[i]);
            else
                settingsItems_[i]->SetText((sel ? "▶  " : "   ") + String(names[i]) + ":   " + vals[i]);
            settingsItems_[i]->SetColor(sel ? Color(1.0f, 0.88f, 0.24f, 1.0f) : Color(0.84f, 0.88f, 0.94f, 1.0f));
        }
    }

    void ChangeSetting(int delta) {
        bool displayChanged = false;
        switch (settingsSel_) {
        case 0:
            gfxQualityIdx_ = Clamp(gfxQualityIdx_ + delta, 0, 3);
            ApplyQualityPresetDefaults();
            break;
        case 1:
            gfxResIdx_ = (gfxResIdx_ + delta + 5) % 5;
            displayChanged = true;
            break;
        case 2:
            gfxDisplayMode_ = (gfxDisplayMode_ + delta + 3) % 3;
            displayChanged = true;
            break;
        case 3:
            gfxMsaaIdx_ = Clamp(gfxMsaaIdx_ + delta, 0, 3);
            gfxFxaa_ = (gfxMsaaIdx_ >= 1);
            displayChanged = true;
            break;
        case 4: gfxShadowIdx_ = Clamp(gfxShadowIdx_ + delta, 0, 2); break;
        case 5: gfxSsao_ = !gfxSsao_; break;
        case 6:
            gfxGlowIdx_ = Clamp(gfxGlowIdx_ + delta, 0, 2);
            gfxHdr_ = (gfxGlowIdx_ > 0);
            break;
        case 7: gfxSharpen_ = !gfxSharpen_; break;
        case 8: gfxAnisoIdx_ = Clamp(gfxAnisoIdx_ + delta, 0, 2); break;
        case 9:
            gfxFpsIdx_ = (gfxFpsIdx_ + delta + 3) % 3;
            if (delta < 0 && gfxFpsIdx_ == 2) gfxVsync_ = !gfxVsync_;
            break;
        case 10: fovBase_ = (float)Clamp((int)fovBase_ + delta * 5, 75, 105); break;
        case 11: mouseSensIdx_ = Clamp(mouseSensIdx_ + delta, 0, 2); break;
        case 12:
            sfxVolume_   = Clamp(sfxVolume_   + delta * 0.1f, 0.0f, 1.0f);
            musicVolume_ = Clamp(musicVolume_ + delta * 0.05f, 0.0f, 1.0f);
            break;
        default: return;
        }
        PlayUISound();
        if (displayChanged) ApplyDisplayMode();
        ApplyGraphicsSettings();
        UpdateSettingsDisplay();
    }

    // =========================================================================
    // РЕЖИМЫ ИГРЫ
    // =========================================================================

    void ClearEnemies() {
        ClearTransientEffects();
        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            RemoveColliderLite(enemies_[i].colliderIdx_);
            if (enemies_[i].node_) enemies_[i].node_->Remove();
        }
        enemies_.Clear();
        for (unsigned i = 0; i < enemyProjectiles_.Size(); ++i)
            if (enemyProjectiles_[i].node_) enemyProjectiles_[i].node_->Remove();
        enemyProjectiles_.Clear();
        for (unsigned i = 0; i < playerPlasmaBolts_.Size(); ++i)
            if (playerPlasmaBolts_[i].node_) playerPlasmaBolts_[i].node_->Remove();
        playerPlasmaBolts_.Clear();
        for (unsigned i = 0; i < thrownGrenades_.Size(); ++i)
            if (thrownGrenades_[i].node_) thrownGrenades_[i].node_->Remove();
        thrownGrenades_.Clear();
        for (unsigned i = 0; i < turrets_.Size(); ++i)
            if (turrets_[i].node_) turrets_[i].node_->Remove();
        turrets_.Clear();
        for (unsigned i = 0; i < orbitalStrikes_.Size(); ++i) {
            if (orbitalStrikes_[i].beamNode_) orbitalStrikes_[i].beamNode_->Remove();
            if (orbitalStrikes_[i].ringNode_) orbitalStrikes_[i].ringNode_->Remove();
        }
        orbitalStrikes_.Clear();
        grappleActive_ = false;
        if (grappleBeamNode_) grappleBeamNode_->SetEnabled(false);
    }

    void StartGameMode(GameMode mode) {
        gameMode_ = mode;
        photoMode_ = false;
        perkShopOpen_ = false;
        if (perkShopPanel_) perkShopPanel_->SetVisible(false);
        if (weaponNode_) weaponNode_->SetEnabled(true);
        player_.score_ = 0;
        player_.kills_ = 0;
        player_.headshots_ = 0;
        player_.shotsFired_ = 0;
        player_.shotsHit_ = 0;
        float bonusHp  = (banyaBuffTier_ >= 2) ? 50.0f : (banyaBuffTier_ == 1 ? 25.0f : 0.0f);
        float bonusArm = (banyaBuffTier_ >= 2) ? 50.0f : (banyaBuffTier_ == 1 ? 25.0f : 0.0f);
        player_.maxHealth_ = 100.0f + bonusHp;
        player_.health_ = player_.maxHealth_;
        player_.armor_ = Min(player_.maxArmor_, 50.0f + bonusArm);
        credits_ = 100 + banyaBonusCredits_;
        banyaBonusCredits_ = 0;
        perkHpLvl_ = 0;
        perkRofLvl_ = 0;
        perkDmgLvl_ = 0;
        perkSpdLvl_ = 0;
        overdriveTimer_ = 0.0f;
        dashCooldown_ = 0.0f;
        dashTimer_ = 0.0f;
        grenadeCooldown_ = 0.0f;
        turretCooldown_ = 0.0f;
        grappleCooldown_ = 0.0f;
        orbitalCharge_ = (banyaBuffTier_ >= 2) ? 70.0f : 35.0f; // Ковш Смотрящего даёт +35% заряда Орбиты!
        activeMutator_ = Cyber::MUT_STANDARD;
        multiKillCount_ = 0;
        multiKillTimer_ = 0.0f;
        slowMoTimer_ = 0.0f;
        barrelsDetonatedRun_ = 0;
        airborneFromPad_ = false;
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

        // Мгновенный респавн взрывных бочек при старте раунда
        for (unsigned bi = 0; bi < barrels_.Size(); ++bi) {
            ExplosiveBarrel& b = barrels_[bi];
            if (b.exploded_) {
                b.exploded_ = false;
                b.hp_ = 42.0f;
                b.respawn_ = 0.0f;
                if (b.node_) b.node_->SetEnabled(true);
                b.colliderIdx_ = AddBoxCollider(b.home_, Vector3(0.56f, 0.88f, 0.56f), Collision::BARREL, b.node_.Get());
            } else {
                b.hp_ = 42.0f;
            }
        }

        for (int i = 0; i < WEAPON_COUNT; ++i) ammoInit_[i] = false;
        EquipWeapon(currentWeapon_, true);
        ResetPickups();

        UpdateHealthDisplay();
        UpdateDashAndBuffHUD();

        if (mode == MODE_ARENA) {
            waveNum_ = 0;
            SpawnWave();
            ShowMessage("РЕЖИМ: КИБЕР-АРЕНА 4K  •  ALT — рывок, V — удар крылом, F — полигон");
        } else {
            waveNum_ = 0;
            ResetRangeTargets();
            ShowMessage("РЕЖИМ: ПОЛИГОН 4K  •  Стреляй в яблочко мишеней и бочки!  F — арена");
        }
        UpdateScoreDisplay();
        ShowMainMenu(false);
        TriggerMissionDeploy(mode);

        if (!helpShownOnce_) {
            helpShownOnce_ = true;
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
        robotBodyMat_ = MakePBR(Color(0.58f, 0.62f, 0.68f), 0.28f, 0.85f);
    }

    // Материалы глаз кэшируются по типу робота (HDR Glow для свечения в BloomHDR!)
    void EnsureEyeMaterials() {
        if (eyeMatMelee_) return;
        eyeMatMelee_    = MakeHDRGlow(Color(1.0f, 0.25f, 0.15f), 2.4f);
        eyeMatCharger_  = MakeHDRGlow(Color(1.0f, 0.55f, 0.08f), 2.6f);
        eyeMatShooter_  = MakeHDRGlow(Color(0.88f, 0.30f, 1.0f), 2.6f);
        eyeMatBoss_     = MakeHDRGlow(Color(1.0f, 0.85f, 0.15f), 3.2f);
        eyeMatKamikaze_ = MakeHDRGlow(Color(1.0f, 0.28f, 0.05f), 3.4f);
        eyeMatDrone_    = MakeHDRGlow(Color(0.20f, 0.95f, 1.0f), 3.0f);
    }

    void SpawnEnemyRobot(const Vector3& pos, EnemyType type) {
        EnsureRobotMaterials();
        EnsureEyeMaterials();
        Node* rob = scene_->CreateChild("Enemy", LOCAL);
        Vector3 initPos = (type == E_DRONE) ? Vector3(pos.x_, 3.2f, pos.z_) : pos;
        rob->SetPosition(initPos);
        rob->SetVar("enemy", true); rob->AddTag("Enemy");

        // Курица вместо робота: тело/голова/крылья/хвост/лапы из примитивов.
        // Материалы перьев выбираются по типу внутри BuildChicken.
        BuildChicken(rob, type);

        // Масштаб модели: босс крупнее, камикадзе юркий, дрон-валькирия поджарая
        float scale = (type == E_BOSS)     ? 1.75f
                    : (type == E_KAMIKAZE) ? 0.80f
                    : (type == E_DRONE)    ? 0.90f
                    : (type == E_SHOOTER)  ? 0.92f : 1.0f;
        rob->SetScale(Vector3::ONE * scale);

        // Появление: «плюх» портала + кудахтанье (раньше куры появлялись бесшумно из воздуха)
        if (!spawnRingMat_) spawnRingMat_ = MakeTinted(nullptr, nullptr, Color(120, 235, 255));
        CreateShockwave(pos, spawnRingMat_.Get(), 0.45f);
        PlayCluck(initPos, 0.5f);

        // Lite-коллайдер робота: Collision::ENEMY, ptr = узел (рейкасты пуль и LOS проходят через него)
        int colIdx = AddBoxCollider(initPos + Vector3(0, 0.85f * scale, 0),
                                    Vector3(0.58f, 0.85f, 0.62f) * scale, Collision::ENEMY, rob);

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
            case E_KAMIKAZE:
                e.health_ = 52.0f;  e.speed_ = 4.45f; e.scoreValue_ = 240;
                e.patrolRadius_ = 2.0f; break;
            case E_DRONE:
                e.health_ = 85.0f;  e.speed_ = 2.65f; e.scoreValue_ = 260;
                e.patrolRadius_ = 8.0f; break;
            case E_BOSS:
                e.health_ = 420.0f + (float)waveNum_ * 20.0f; e.speed_ = 2.25f; e.scoreValue_ = 900;
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
        (void)total;
        if (waveNum_ >= 5 && slot == 0 && (waveNum_ % 5 == 0)) return E_BOSS;      // босс каждые 5 волн
        if (waveNum_ >= 3) {
            int roll = rand() % 100;
            if (roll < 18) return E_DRONE;
            if (roll < 36) return E_KAMIKAZE;
            if (roll < 58) return E_SHOOTER;
            if (roll < 80) return E_CHARGER;
            return E_MELEE;
        }
        if (waveNum_ >= 2) {
            int roll = rand() % 100;
            if (roll < 18) return E_KAMIKAZE;
            if (roll < 42) return E_SHOOTER;
            if (roll < 68) return E_CHARGER;
        }
        return E_MELEE;
    }

    void SpawnWave() {
        ++waveNum_;
        activeMutator_ = Cyber::PickWaveMutator(waveNum_, rand() % 97);
        const Cyber::MutatorInfo& mut = Cyber::GetMutatorInfo(activeMutator_);
        if (waveNum_ > 1) {
            credits_ += 45;
            PushKillFeed("+45₵ БОНУС ЗА ЗАЧИСТКУ ВОЛНЫ!");
        }
        if (waveNum_ >= 5)  UnlockAchievement(4, "ШТУРМОВИК АРЕНЫ (достичь 5-й волны)");
        if (waveNum_ >= 10) UnlockAchievement(9, "ЛЕГЕНДА КУРЯТНИКА (выжить до 10-й волны)");

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
        const float outerSpawnRadius = Min(104.0f, half - 8.0f);
        const float innerSpawnRadius = Min(24.0f, outerSpawnRadius * 0.42f);
        Vector3 playerPos = cameraNode_ ? cameraNode_->GetPosition() : Vector3::ZERO;
        int spawned = 0;
        for (int i = 0; i < count; ++i) {
            // Ищем свободную точку: раньше робот мог появиться внутри колонны или ящика
            // и застрять там навсегда (волна не могла закончиться).
            Vector3 pos;
            bool ok = false;
            for (int attempt = 0; attempt < 24 && !ok; ++attempt) {
                float ang = (float)(rand() % 360) * 0.0174533f;
                float r = innerSpawnRadius + (float)(rand() % 1000) / 1000.0f * (outerSpawnRadius - innerSpawnRadius);
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
        ShowWaveBanner(bossWave ? ("ВОЛНА " + String(waveNum_) + " — МЕГА-ИНДЮК! [" + String(mut.name) + "]")
                                : ("ВОЛНА " + String(waveNum_) + "  •  " + String(mut.name)),
                       bossWave ? Color(1.0f, 0.35f, 0.25f) : Color(1.0f, 0.85f, 0.25f));
        PushKillFeed("ВОЛНА " + String(waveNum_) + " (" + String(mut.name) + ") — кур: " + String(spawned) + (bossWave ? " + БОСС" : ""));
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
        return !RaycastLite(from, dir, dist - 0.3f, Collision::STATIC_MASK | Collision::MOVING_TARGET_MASK, outDist, outPoint);
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
        if (enemyProjectiles_.Size() >= MAX_ENEMY_PROJECTILES) {
            if (enemyProjectiles_[0].node_) enemyProjectiles_[0].node_->Remove();
            enemyProjectiles_.Erase(0);
        }
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
                        if (RaycastLite(pos, step / stepLen, stepLen, Collision::STATIC_MASK | Collision::TARGET_MASK | Collision::MOVING_TARGET_MASK, outDist, outPoint)) {
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
    // ПИКАПЫ 4K ULTRA: Аптечка (+30 HP), Патроны (+2 маг.), Броня (+50), Овердрайв (2x)
    // =========================================================================

    void CreatePickups() {
        if (!pickupHealMat_)      pickupHealMat_      = MakeHDRGlow(Color(0.25f, 1.0f, 0.45f), 2.1f);
        if (!pickupAmmoMat_)      pickupAmmoMat_      = MakeHDRGlow(Color(1.0f, 0.84f, 0.28f), 2.1f);
        if (!pickupArmorMat_)     pickupArmorMat_     = MakeHDRGlow(Color(0.20f, 0.86f, 1.0f), 2.3f);
        if (!pickupOverdriveMat_) pickupOverdriveMat_ = MakeHDRGlow(Color(0.95f, 0.30f, 1.0f), 2.8f);

        struct SpotDef { Vector3 pos; int type; };
        const SpotDef spots[] = {
            // 4 у центральной платформы: 2 аптечки + 2 патроны
            { Vector3( 13.0f, 1.0f,   0.0f), 0 },
            { Vector3(-13.0f, 1.0f,   0.0f), 0 },
            { Vector3(  0.0f, 1.0f,  13.0f), 1 },
            { Vector3(  0.0f, 1.0f, -13.0f), 1 },
            // 4 в диагональных секторах: 2 титановые брони + 2 патроны/аптечка
            { Vector3( 34.0f, 1.0f,  34.0f), 2 },
            { Vector3(-34.0f, 1.0f, -34.0f), 2 },
            { Vector3(-34.0f, 1.0f,  34.0f), 0 },
            { Vector3( 34.0f, 1.0f, -34.0f), 1 },
            // 2 мощных сферы КИБЕР-ОВЕРДРАЙВА (2x урон) у северного и южного энергоузлов
            { Vector3(  0.0f, 1.25f,  29.0f), 3 },
            { Vector3(  0.0f, 1.25f, -29.0f), 3 }
        };

        SharedPtr<Material> crossMat = MakeHDRGlow(Color(1.0f, 0.22f, 0.26f), 2.0f);
        SharedPtr<Material> caseMat  = hdCrate_ ? SharedPtr<Material>(hdCrate_->Clone()) : MakePBR(Color(0.36f, 0.28f, 0.22f), 0.7f, 0.1f);
        SharedPtr<Material> plateMat = hdMetal_ ? SharedPtr<Material>(hdMetal_->Clone()) : MakePBR(Color(0.68f, 0.76f, 0.85f), 0.22f, 0.92f);
        Model* sph = sphereModel_ ? sphereModel_ : boxModel_;

        for (unsigned i = 0; i < sizeof(spots) / sizeof(spots[0]); ++i) {
            Node* n = scene_->CreateChild("Pickup", LOCAL);
            n->SetPosition(spots[i].pos);
            n->SetScale(Vector3::ONE * 0.42f);
            int tp = spots[i].type;
            Material* haloMat = pickupHealMat_.Get();

            if (tp == 0) {
                // Аптечка-яйцо с HDR крестом
                AddPart(n, Vector3(0, 0, 0), Vector3(0.88f, 1.05f, 0.88f), pickupHealMat_.Get(), sph);
                AddPart(n, Vector3(0, 0.05f, 0.42f), Vector3(0.48f, 0.16f, 0.12f), crossMat.Get());
                AddPart(n, Vector3(0, 0.05f, 0.43f), Vector3(0.16f, 0.48f, 0.12f), crossMat.Get());
                haloMat = pickupHealMat_.Get();
            } else if (tp == 1) {
                // Ящик с боеприпасами
                AddPart(n, Vector3(0, 0, 0), Vector3(0.82f, 0.62f, 0.82f), caseMat.Get());
                AddPart(n, Vector3(0, 0.05f, 0), Vector3(0.88f, 0.14f, 0.88f), pickupAmmoMat_.Get());
                for (int b = 0; b < 3; ++b)
                    AddPart(n, Vector3((float)(b - 1) * 0.2f, 0.38f, 0.0f), Vector3(0.14f, 0.32f, 0.14f),
                            pickupAmmoMat_.Get(), cylModel_);
                haloMat = pickupAmmoMat_.Get();
            } else if (tp == 2) {
                // Титановый бронежилет (+50 брони)
                AddPart(n, Vector3(0, 0.05f, 0), Vector3(0.86f, 0.96f, 0.34f), plateMat.Get());
                AddPart(n, Vector3(-0.34f, 0.48f, 0), Vector3(0.24f, 0.22f, 0.36f), plateMat.Get());
                AddPart(n, Vector3( 0.34f, 0.48f, 0), Vector3(0.24f, 0.22f, 0.36f), plateMat.Get());
                AddPart(n, Vector3(0, 0.08f, 0.18f), Vector3(0.36f, 0.36f, 0.10f), pickupArmorMat_.Get(), sph);
                haloMat = pickupArmorMat_.Get();
            } else {
                // Сфера КИБЕР-ОВЕРДРАЙВА (2x урон + ускоренная перезарядка)
                AddPart(n, Vector3(0, 0.10f, 0), Vector3(0.76f, 0.76f, 0.76f), pickupOverdriveMat_.Get(), sph);
                AddPart(n, Vector3(0, 0.10f, 0), Vector3(1.18f, 0.10f, 1.18f), pickupArmorMat_.Get(), cylModel_);
                AddPart(n, Vector3(0, 0.10f, 0), Vector3(0.10f, 1.18f, 1.18f), pickupOverdriveMat_.Get(), cylModel_);
                haloMat = pickupOverdriveMat_.Get();
            }

            Node* halo = AddPart(n, Vector3(0, -0.58f, 0), Vector3(1.65f, 0.06f, 1.65f), haloMat, cylModel_);
            if (halo) halo->SetScale(Vector3(1.65f, 0.06f, 1.65f));

            PickupData p;
            p.node_ = n;
            p.home_ = spots[i].pos;
            p.type_ = tp;
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
                p.node_->SetScale(Vector3::ONE * 0.42f);
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
                    n->SetScale(Vector3::ONE * 0.42f);
                }
                continue;
            }

            n->SetRotation(Quaternion(timeAcc_ * (p.type_ == 3 ? 115.0f : 72.0f) + (float)i * 37.0f, Vector3::UP));
            n->SetPosition(p.home_ + Vector3(0.0f, sinf(timeAcc_ * 2.2f + (float)i) * 0.16f, 0.0f));

            if ((camPos - n->GetPosition()).LengthSquared() > r2) continue;

            if (p.type_ == 0) {
                if (player_.health_ >= player_.maxHealth_) continue;
                float gain = 30.0f;
                player_.health_ = Min(player_.maxHealth_, player_.health_ + gain);
                UpdateHealthDisplay();
                ShowMessage("АПТЕЧКА +30 HP");
                PushKillFeed("аптечка  +30 HP");
                PlayUISound();
                p.respawn_ = GameConstants::PICKUP_RESPAWN;
            } else if (p.type_ == 1) {
                const WeaponDef& def = WEAPON_DEFS[currentWeapon_];
                int cur = reserveAmmo_[currentWeapon_];
                if (cur >= def.reserveMax) continue;
                int next = Min(def.reserveMax, cur + def.magSize * 2);
                reserveAmmo_[currentWeapon_] = next;
                weapon_.reserveAmmo_ = next;
                UpdateAmmoDisplay();
                ShowMessage("БОЕПРИПАСЫ +" + String(next - cur));
                PushKillFeed("патроны  +" + String(next - cur));
                PlayUISound();
                p.respawn_ = GameConstants::PICKUP_RESPAWN;
            } else if (p.type_ == 2) {
                if (player_.armor_ >= player_.maxArmor_) continue;
                player_.armor_ = Min(player_.maxArmor_, player_.armor_ + 50.0f);
                UpdateHealthDisplay();
                ShowMessage("ТИТАНОВАЯ БРОНЯ +50");
                PushKillFeed("броня  +50 SHIELD");
                if (soundSource_ && armorPickupSound_) { soundSource_->SetGain(SfxGain(0.85f)); soundSource_->Play(armorPickupSound_); }
                else PlayUISound();
                p.respawn_ = 22.0f;
            } else {
                overdriveTimer_ = 12.0f;
                UpdateDashAndBuffHUD();
                ShowMessage("⚡ КИБЕР-ОВЕРДРАЙВ: 2× УРОН И БЫСТРАЯ ПЕРЕЗАРЯДКА (12 сек)!");
                PushKillFeed("⚡ КИБЕР-ОВЕРДРАЙВ 2X АКТИВЕН!");
                if (soundSource_ && overdriveSound_) { soundSource_->SetGain(SfxGain(0.9f)); soundSource_->Play(overdriveSound_); }
                else PlayUISound();
                p.respawn_ = 32.0f;
            }
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
            if (e.cryoSlowTimer_ > 0.0f) e.cryoSlowTimer_ = Max(0.0f, e.cryoSlowTimer_ - dt);
            const float mutSpd = Cyber::GetMutatorInfo(activeMutator_).speedMul;
            const float cryoMul = e.cryoSlowTimer_ > 0.0f ? e.cryoSlowMul_ : 1.0f;
            const float effSpd = e.speed_ * mutSpd * cryoMul * (e.enraged_ ? 1.32f : 1.0f);

            switch (e.type_) {
            case E_SHOOTER: {
                // Держит дистанцию ~10-14 м, стреляет очередями только с прямой видимостью
                bool los = HasLineOfSight(p + Vector3(0, 0.9f, 0), camPos);
                if (dist > 13.0f)      move = toDir * effSpd;
                else if (dist < 8.0f)  move = -toDir * effSpd;
                else                   move = Vector3(-toDir.z_, 0, toDir.x_) * effSpd * 0.6f * sinf(e.strafePhase_);
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
                move = (toDir + strafe * 0.35f * sinf(e.strafePhase_)) * effSpd;
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
                // При падении HP ниже 50% Мега-Индюк впадает в ЯРОСТЬ (рёв, ударная волна, веерный залп!)
                if (!e.enraged_ && e.health_ <= e.maxHealth_ * 0.5f) {
                    e.enraged_ = true;
                    n->SetScale(n->GetScale() * 1.12f);
                    CreateShockwave(p + Vector3(0, 0.5f, 0), nullptr, 0.7f);
                    if (bossRoarSound_) PlaySfx3D(p, bossRoarSound_, 1.0f);
                    ShowWaveBanner("🔥 МЕГА-ИНДЮК ВПАЛ В ЯРОСТЬ!", Color(1.0f, 0.22f, 0.22f));
                    PushKillFeed("🔥 БОСС В ЯРОСТИ: СКОРОСТЬ И ТРОЙНОЙ ЗАЛП!");
                }
                move = toDir * effSpd;
                if (dist < 2.2f && e.attackTimer_ <= 0.0f) {
                    e.attackTimer_ = e.enraged_ ? 0.9f : 1.2f;
                    e.recoilKick_ = 0.3f;
                    DamagePlayer(e.enraged_ ? 36.0f : 30.0f);
                    AddCameraShake(0.3f);
                    CreateHitSpark((p + camPos) * 0.5f);
                } else if (e.attackTimer_ <= 0.0f && dist > 5.5f && (rand() % 100) < (e.enraged_ ? 55 : 30)) {
                    e.attackTimer_ = e.enraged_ ? 1.65f : 2.5f;
                    Vector3 side(-toDir.z_, 0.0f, toDir.x_);
                    SpawnEnemyProjectile(p + Vector3(0, 1.6f, 0) + toDir, camPos, 12.0f);
                    if (e.enraged_) {
                        SpawnEnemyProjectile(p + Vector3(0, 1.6f, 0) + toDir, camPos + side * 2.6f, 11.0f);
                        SpawnEnemyProjectile(p + Vector3(0, 1.6f, 0) + toDir, camPos - side * 2.6f, 11.0f);
                    }
                }
                break;
            }
            case E_KAMIKAZE: {
                // Цыплёнок-камикадзе: несётся зигзагом с тикающей C4; вблизи взрывается сам!
                Vector3 strafe(-toDir.z_, 0, toDir.x_);
                move = (toDir + strafe * 0.50f * sinf(e.strafePhase_ * 1.9f)) * effSpd;
                e.burstTimer_ -= dt;
                float beepPeriod = Clamp(dist * 0.045f, 0.11f, 0.55f);
                if (dist < 16.0f && e.burstTimer_ <= 0.0f) {
                    e.burstTimer_ = beepPeriod;
                    if (kamikazeBeepSound_) PlaySfx3D(p, kamikazeBeepSound_, 0.55f);
                }
                if (dist < 1.85f) {
                    e.alive_ = false;
                    Vector3 boomPos = p + Vector3(0, 0.7f, 0);
                    n->Remove();
                    CreateExplosionEffect(boomPos);
                    CreateShockwave(boomPos, nullptr, 0.65f);
                    if (barrelBoomSound_) PlaySfx3D(boomPos, barrelBoomSound_, 0.95f);
                    ApplyExplosionDamage(boomPos, 75.0f);
                    AddCameraShake(0.24f);
                    PushKillFeed("💥 ПОДРЫВ ЦЫПЛЁНКА-КАМИКАДЗЕ!");
                    continue;
                }
                break;
            }
            case E_DRONE: {
                // Реактивная куро-валькирия: парит над укрытиями (y ~ 3.2..4.1 м) и ведёт огонь сверху!
                Vector3 strafe(-toDir.z_, 0, toDir.x_);
                if (dist > 15.0f)      move = toDir * effSpd;
                else if (dist < 8.5f)  move = -toDir * effSpd * 0.8f;
                else                   move = strafe * effSpd * 0.9f * cosf(e.strafePhase_ * 0.7f);
                if (e.attackTimer_ <= 0.0f && dist < 26.0f) {
                    e.attackTimer_ = 1.95f + (float)(rand() % 70) / 100.0f;
                    e.recoilKick_ = 0.16f;
                    SpawnEnemyProjectile(p + Vector3(0, 0.9f, 0) + toDir * 0.5f, camPos, 10.0f);
                }
                break;
            }
            default: { // E_MELEE — идёт к игроку, обходя препятствия рейкастом
                Vector3 probePos = p + Vector3(0, 0.6f, 0);
                bool blocked = false;
                if (dist > 1.5f) {
                    float outDist; Vector3 outPoint; Vector3 outNormal;
                    if (RaycastLite(probePos, toDir, 2.2f, Collision::STATIC_MASK | Collision::MOVING_TARGET_MASK, outDist, outPoint, nullptr, nullptr, &outNormal)) {
                        blocked = true;
                        // Скольжение вдоль нормали препятствия + лёгкий «инстинкт стаи»
                        Vector3 slide = outNormal.CrossProduct(Vector3::UP);
                        slide.y_ = 0;
                        if (slide.LengthSquared() > 0.001f)
                            move = slide.Normalized() * effSpd * (sinf(e.strafePhase_) > 0 ? 1.0f : -1.0f);
                    }
                }
                if (!blocked) move = toDir * effSpd;
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
            // Для летающей E_DRONE обходим наземные ящики на высоте полёта!
            Vector3 np = (e.type_ == E_DRONE) ? (p + move * dt) : PushOutOfObstacles(p + move * dt, 0.55f, 0.55f);
            const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f - 2.0f;
            np.x_ = Clamp(np.x_, -half, half);
            np.z_ = Clamp(np.z_, -half, half);
            float bob = 0.04f * sinf(timeAcc_ * 3.0f + e.phase_);
            // Визуальный «откат» при выстреле/ударе: приседание модели
            if (e.recoilKick_ > 0.0f) e.recoilKick_ = Max(0.0f, e.recoilKick_ - dt);
            float kick = e.recoilKick_ * 0.6f;
            float baseY = (e.type_ == E_DRONE) ? (3.25f + 0.55f * sinf(timeAcc_ * 2.3f + e.phase_)) : (0.06f + bob);
            Vector3 newPos(np.x_, baseY - kick, np.z_);
            n->SetPosition(newPos);
            float bodyScale = Max(0.1f, n->GetScale().y_);
            ResizeColliderLite(e.colliderIdx_, Vector3(0.58f, 0.85f, 0.62f) * bodyScale);
            MoveColliderLite(e.colliderIdx_, newPos + Vector3(0, 0.85f * bodyScale, 0)); // центр тела курицы

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
                    AddDebris(fd);
                }
            }
            PlayHitSound();
            // Визуальный «откид» без физики: короткий наклон модели робота
            e.recoilKick_ = 0.35f;
        }
        if (e.health_ <= 0.0f) {
            e.alive_ = false;
            Vector3 deathPos = n ? n->GetPosition() : Vector3::ZERO;
            EnemyType killedType = e.type_;
            int scoreVal = e.scoreValue_;
            if (n) {
                CreateExplosionEffect(deathPos);
                CreateFeatherBurst(deathPos, killedType);   // курица «взрывается» перьями
                PlaySfx3D(deathPos, killedType == E_BOSS ? bossRoarSound_ : squawkSound_, 0.85f);
                CreateDebrisBurst(deathPos);
                PlayExplosionAt(deathPos);
                n->Remove();
            }

            const Cyber::MutatorInfo& mut = Cyber::GetMutatorInfo(activeMutator_);

            // Камикадзе или мутатор ПИРО-ШТОРМ запускают взрывную волну по соседним курам!
            if (killedType == E_KAMIKAZE || mut.volatileDeaths) {
                float blastR = (killedType == E_KAMIKAZE) ? 6.0f : 4.2f;
                float blastD = (killedType == E_KAMIKAZE) ? 115.0f : 55.0f;
                CreateShockwave(deathPos + Vector3(0, 0.5f, 0), nullptr, 0.62f);
                if (barrelBoomSound_ && killedType == E_KAMIKAZE) PlaySfx3D(deathPos, barrelBoomSound_, 0.88f);
                for (unsigned oi = 0; oi < enemies_.Size(); ++oi) {
                    if (!enemies_[oi].alive_ || !enemies_[oi].node_) continue;
                    float dd = (enemies_[oi].node_->GetPosition() - deathPos).Length();
                    if (dd < blastR) DamageEnemy(enemies_[oi], blastD * (1.0f - dd / blastR));
                }
            }

            int baseCred = (killedType == E_BOSS) ? 120 : (killedType == E_DRONE || killedType == E_KAMIKAZE ? 35 : 22);
            int creditGain = (int)lroundf((float)baseCred * mut.creditMul);
            credits_ += creditGain;
            orbitalCharge_ = Cyber::AddOrbitalCharge(orbitalCharge_, killedType == E_BOSS, false);

            // Бонус за убийство в воздухе с джамп-платформы!
            if (!onGround_ && airborneFromPad_) {
                player_.score_ += 100;
                credits_ += 20;
                PushKillFeed("🚀 АЭРО-ФРАГ С ДЖАМП-ПАДА! +100");
                UnlockAchievement(3, "НЕБЕСНЫЙ ПЕТУХ (убийство в полёте с джамп-пада)");
            }

            AddScoreWithCombo(scoreVal);
            player_.kills_++;
            if (player_.kills_ == 1) UnlockAchievement(0, "ПЕРВОЕ ПЕРО (первая цель уничтожена)");
            if (killedType == E_BOSS) {
                slowMoTimer_ = Max(slowMoTimer_, 0.38f);
                UnlockAchievement(6, "УБИЙЦА МЕГА-ИНДЮКА (одолеть босса арены)");
            }

            // Система быстрых серий (Multi-Kill Announcer)
            multiKillCount_ = (multiKillTimer_ > 0.0f) ? (multiKillCount_ + 1) : 1;
            multiKillTimer_ = 3.2f;
            if (multiKillCount_ == 2)      PushKillFeed("🔥 ДВОЙНОЙ ФРАГ!");
            else if (multiKillCount_ == 3) PushKillFeed("🔥🔥 ТРОЙНОЙ ШАШЛЫК!");
            else if (multiKillCount_ == 4) PushKillFeed("⚡ УЛЬТРА-ОЩИП x4!");
            else if (multiKillCount_ >= 5) {
                PushKillFeed("💀 КУРИНЫЙ АПОКАЛИПСИС x" + String(multiKillCount_) + "!");
                slowMoTimer_ = Max(slowMoTimer_, 0.30f);
            }

            UpdateScoreDisplay();
            ShowMessage((killedType == E_BOSS ? "БОСС-ИНДЮК ПАЛ! +" : "КУРИЦА ОЩИПАНА! +") +
                        String(scoreVal) + " (+" + String(creditGain) + "₵)" +
                        (comboCount_ > 1 ? (" [COMBO x" + String(comboCount_) + "]") : ""));
            PushKillFeed((killedType == E_BOSS ? "ИНДЮК ПАЛ +" : "курица ощипана +") +
                         String(scoreVal) +
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
                case E_CHARGER:  sm->SetMaterial(chickBrown_    ? chickBrown_.Get()    : mat); break;
                case E_SHOOTER:  sm->SetMaterial(chickBlack_    ? chickBlack_.Get()    : mat); break;
                case E_BOSS:     sm->SetMaterial(chickBoss_     ? chickBoss_.Get()     : mat); break;
                case E_KAMIKAZE: sm->SetMaterial(chickKamikaze_ ? chickKamikaze_.Get() : mat); break;
                case E_DRONE:    sm->SetMaterial(chickDrone_    ? chickDrone_.Get()    : mat); break;
                default:         sm->SetMaterial(mat); break;
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
            AddDebris(d);
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
            AddDebris(d);
        }

        if (haveParticles_ && sparkEffect_) {
            Node* sparks = scene_->CreateChild("DebrisSparks");
            sparks->SetPosition(pos);
            ParticleEmitter* pe = sparks->CreateComponent<ParticleEmitter>();
            pe->SetEffect(sparkEffect_);
            pe->SetNumParticles(200);
            pe->SetEmitting(true);
            QueuePendingRemove(sparks, 1.5f);
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
        QueuePendingRemove(sparkNode, 1.0f);
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

    // =========================================================================
    // НАТИВНАЯ «ЗОЛОТАЯ РУЛЕТКА» (только виртуальные фишки)
    // =========================================================================

    void UpdateCasinoVisuals() {
        if (!casinoPanel_) return;
        if (casinoBalanceText_)
            casinoBalanceText_->SetText("ФИШКИ: " + String(casinoBalance_) + "₵");
        if (casinoBetText_) {
            String bet = String(Casino::BetName(casinoBet_.type));
            if (casinoBet_.type >= Casino::BET_NUMBER)
                bet += " " + String(casinoBet_.number);
            casinoBetText_->SetText("СТАВКА: " + bet + "  •  " + String(casinoBet_.amount) + "₵");
        }
        if (casinoStatsText_)
            casinoStatsText_->SetText("ПОБЕДЫ: " + String(casinoWins_) + "   ПОРАЖЕНИЯ: " + String(casinoLosses_));
        if (casinoResultText_) {
            if (casinoSpinTimer_ > 0.0f) {
                casinoResultText_->SetText("Шарик в полёте… ставка уже на столе");
                casinoResultText_->SetColor(Color(1.0f, 0.84f, 0.35f, 1.0f));
            } else if (casinoLastResult_ >= 0) {
                casinoResultText_->SetText("Выпало: " + String(casinoLastResult_) + " • " + String(Casino::ColorName(casinoLastResult_)));
                casinoResultText_->SetColor(casinoLastResult_ == 0 ? Color(0.35f, 1.0f, 0.62f, 1.0f)
                    : Casino::IsRed(casinoLastResult_) ? Color(1.0f, 0.42f, 0.42f, 1.0f)
                                                        : Color(0.72f, 0.80f, 0.88f, 1.0f));
            } else {
                casinoResultText_->SetText("Сделайте виртуальную ставку и нажмите ПРОКРУТИТЬ");
                casinoResultText_->SetColor(Color(0.72f, 0.80f, 0.88f, 1.0f));
            }
        }

        if (casinoWheelText_) {
            static const int kWheelPreview[18] = { 0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23 };
            int offset = casinoSpinTimer_ > 0.0f ? ((int)(timeAcc_ * 18.0f) % 18) : 0;
            String wheel = "◉  ";
            for (int i = 0; i < 9; ++i) {
                if (i > 0) wheel += "  •  ";
                int n = kWheelPreview[(offset + i) % 18];
                wheel += String(n);
            }
            casinoWheelText_->SetText(wheel);
        }
        if (casinoSpinButton_)
            casinoSpinButton_->SetColor(casinoSpinTimer_ > 0.0f ? Color(0.22f, 0.22f, 0.24f, 0.96f)
                                                                    : Color(0.64f, 0.38f, 0.08f, 0.98f));
    }

    void CreateCasinoPanel() {
        if (casinoPanel_) return;
        RefreshUIScale();
        auto* ui = GetSubsystem<UI>();
        auto* gfx = GetSubsystem<Graphics>();
        if (!ui || !gfx) return;
        UIElement* root = ui->GetRoot();
        int sw = gfx->GetWidth(), sh = gfx->GetHeight();

        casinoPanel_ = root->CreateChild<UIElement>("CasinoPanel");
        casinoPanel_->SetSize(sw, sh);
        casinoPanel_->SetPriority(760);

        BorderImage* bg = casinoPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.012f, 0.036f, 0.026f, 0.98f));

        BorderImage* card = casinoPanel_->CreateChild<BorderImage>();
        int cw = Min(sw - S(48), S(1080));
        int ch = Min(sh - S(80), S(720));
        int cx = (sw - cw) / 2;
        int cy = (sh - ch) / 2;
        card->SetPosition(cx, cy);
        card->SetSize(cw, ch);
        card->SetColor(Color(0.025f, 0.095f, 0.060f, 0.98f));
        casinoWheelCard_ = SharedPtr<BorderImage>(card);

        auto makeButton = [&](int x, int y, int w, int h, const String& label, const Color& color) -> BorderImage* {
            BorderImage* b = casinoPanel_->CreateChild<BorderImage>();
            b->SetPosition(x, y); b->SetSize(w, h); b->SetColor(color);
            Text* t = b->CreateChild<Text>();
            t->SetFont(font_, SF(16)); t->SetText(label); t->SetTextAlignment(HA_CENTER);
            t->SetPosition(S(6), (h - SF(16)) / 2 - S(2)); t->SetWidth(w - S(12));
            t->SetColor(Color(1.0f, 0.94f, 0.76f, 1.0f)); StyleCrispText(t);
            return b;
        };

        casinoTitleText_ = casinoPanel_->CreateChild<Text>();
        casinoTitleText_->SetFont(font_, SF(38));
        casinoTitleText_->SetText("ЗОЛОТАЯ РУЛЕТКА");
        casinoTitleText_->SetTextAlignment(HA_CENTER);
        casinoTitleText_->SetPosition(cx + S(30), cy + S(28));
        casinoTitleText_->SetWidth(cw - S(60));
        casinoTitleText_->SetColor(Color(1.0f, 0.82f, 0.28f, 1.0f));
        StyleCrispText(casinoTitleText_.Get(), true);

        casinoBalanceText_ = casinoPanel_->CreateChild<Text>();
        casinoBalanceText_->SetFont(font_, SF(24));
        casinoBalanceText_->SetPosition(cx + S(34), cy + S(92));
        casinoBalanceText_->SetWidth(cw - S(68));
        casinoBalanceText_->SetTextAlignment(HA_CENTER);
        casinoBalanceText_->SetColor(Color(0.65f, 1.0f, 0.75f, 1.0f));
        StyleCrispText(casinoBalanceText_);

        casinoWheelText_ = casinoPanel_->CreateChild<Text>();
        casinoWheelText_->SetFont(font_, SF(27));
        casinoWheelText_->SetTextAlignment(HA_CENTER);
        casinoWheelText_->SetPosition(cx + S(50), cy + S(150));
        casinoWheelText_->SetWidth(cw - S(100));
        casinoWheelText_->SetColor(Color(1.0f, 0.90f, 0.48f, 1.0f));
        StyleCrispText(casinoWheelText_, true);

        casinoResultText_ = casinoPanel_->CreateChild<Text>();
        casinoResultText_->SetFont(font_, SF(18));
        casinoResultText_->SetTextAlignment(HA_CENTER);
        casinoResultText_->SetPosition(cx + S(55), cy + S(218));
        casinoResultText_->SetWidth(cw - S(110));
        casinoResultText_->SetColor(Color(0.72f, 0.80f, 0.88f, 1.0f));
        StyleCrispText(casinoResultText_);

        casinoBetText_ = casinoPanel_->CreateChild<Text>();
        casinoBetText_->SetFont(font_, SF(22));
        casinoBetText_->SetTextAlignment(HA_CENTER);
        casinoBetText_->SetPosition(cx + S(45), cy + S(270));
        casinoBetText_->SetWidth(cw - S(90));
        casinoBetText_->SetColor(Color(1.0f, 0.86f, 0.42f, 1.0f));
        StyleCrispText(casinoBetText_);

        int bw = S(260), bh = S(46), gap = S(16);
        int row1 = cy + S(320);
        casinoBetButton_ = SharedPtr<BorderImage>(makeButton(cx + S(42), row1, bw, bh, "ТИП СТАВКИ [Q/E]", Color(0.42f, 0.12f, 0.14f, 0.98f)));
        casinoChipButton_ = SharedPtr<BorderImage>(makeButton(cx + S(42) + bw + gap, row1, bw, bh, "РАЗМЕР ФИШКИ [A/D]", Color(0.10f, 0.20f, 0.30f, 0.98f)));
        casinoNumberButton_ = SharedPtr<BorderImage>(makeButton(cx + S(42) + (bw + gap) * 2, row1, bw, bh, "ЧИСЛО [W/S]", Color(0.16f, 0.25f, 0.14f, 0.98f)));

        casinoSpinButton_ = SharedPtr<BorderImage>(makeButton(cx + S(190), cy + S(390), cw - S(380), S(58), "ПРОКРУТИТЬ РУЛЕТКУ [ENTER / SPACE]", Color(0.64f, 0.38f, 0.08f, 0.98f)));
        casinoTopupButton_ = SharedPtr<BorderImage>(makeButton(cx + S(42), cy + S(470), bw, bh, "+5000 ФИШЕК", Color(0.12f, 0.32f, 0.22f, 0.98f)));
        casinoCloseButton_ = SharedPtr<BorderImage>(makeButton(cx + S(42) + bw + gap, cy + S(470), bw * 2 + gap, bh, "НАЗАД В ГЛАВНОЕ МЕНЮ [ESC]", Color(0.16f, 0.16f, 0.20f, 0.98f)));

        casinoStatsText_ = casinoPanel_->CreateChild<Text>();
        casinoStatsText_->SetFont(font_, SF(16));
        casinoStatsText_->SetTextAlignment(HA_CENTER);
        casinoStatsText_->SetPosition(cx + S(40), cy + S(535));
        casinoStatsText_->SetWidth(cw - S(80));
        casinoStatsText_->SetColor(Color(0.68f, 0.82f, 0.72f, 1.0f));
        StyleCrispText(casinoStatsText_);

        casinoHelpText_ = casinoPanel_->CreateChild<Text>();
        casinoHelpText_->SetFont(font_, SF(14));
        casinoHelpText_->SetText("Q/E — тип ставки   •   A/D — размер фишки   •   W/S — число 0..36   •   R — случайное число\n"
                                 "Прямые ставки: ×2   •   дюжина/колонка: ×3   •   сплит/стрит/корнер/сикс-лайн: ×18/×12/×8/×6   •   число: ×36");
        casinoHelpText_->SetTextAlignment(HA_CENTER);
        casinoHelpText_->SetPosition(cx + S(30), cy + S(570));
        casinoHelpText_->SetWidth(cw - S(60));
        casinoHelpText_->SetColor(Color(0.58f, 0.72f, 0.64f, 1.0f));
        StyleCrispText(casinoHelpText_);

        casinoPanel_->SetVisible(false);
        UpdateCasinoVisuals();
    }

    void OpenCasino() {
        if (casinoOpen_) return;
        casinoOpen_ = true;
        casinoSpinTimer_ = 0.0f;
        if (!casinoPanel_) CreateCasinoPanel();
        if (mainMenuPanel_) mainMenuPanel_->SetVisible(false);
        if (casinoPanel_) casinoPanel_->SetVisible(true);
        SetGameHudVisible(false);
        auto* input = GetSubsystem<Input>();
        if (input) { input->SetMouseVisible(true); isMouseVisible_ = true; }
        UpdateCasinoVisuals();
        PlayUISound();
    }

    void CloseCasino() {
        if (!casinoOpen_ || casinoSpinTimer_ > 0.0f) return;
        casinoOpen_ = false;
        if (casinoPanel_) casinoPanel_->SetVisible(false);
        SaveSettingsFile();
        auto* input = GetSubsystem<Input>();
        if (input) { input->SetMouseVisible(false); isMouseVisible_ = false; }
        ShowMainMenu(true);
    }

    void StartCasinoSpin() {
        if (casinoSpinTimer_ > 0.0f) return;
        casinoBet_.amount = std::max(1, casinoBet_.amount);
        if (casinoBalance_ < casinoBet_.amount) {
            casinoResultText_->SetText("Недостаточно фишек. Нажмите «+5000 ФИШЕК».");
            casinoResultText_->SetColor(Color(1.0f, 0.36f, 0.34f, 1.0f));
            PlayEmptyClickSound();
            return;
        }
        casinoBalance_ -= casinoBet_.amount;
        casinoPendingResult_ = rand() % 37;
        casinoSpinTimer_ = 2.8f;
        PlayUISound();
        UpdateCasinoVisuals();
    }

    void FinishCasinoSpin() {
        casinoLastResult_ = casinoPendingResult_;
        int payout = Casino::Payout(casinoBet_, casinoLastResult_);
        if (payout > 0) {
            casinoBalance_ += payout;
            ++casinoWins_;
            casinoResultText_->SetText("ВЫИГРЫШ: +" + String(payout) + "₵ • " + String(Casino::ColorName(casinoLastResult_)));
            casinoResultText_->SetColor(Color(0.35f, 1.0f, 0.62f, 1.0f));
        } else {
            ++casinoLosses_;
            casinoResultText_->SetText("Ставка сгорела: выпало " + String(casinoLastResult_) + " • всё понарошку");
            casinoResultText_->SetColor(Color(1.0f, 0.40f, 0.42f, 1.0f));
        }
        casinoPendingResult_ = -1;
        SaveSettingsFile();
        PlayUISound();
        UpdateCasinoVisuals();
    }

    void UpdateCasino(float dt) {
        if (!casinoOpen_) return;
        if (casinoSpinTimer_ > 0.0f) {
            casinoSpinTimer_ = Max(0.0f, casinoSpinTimer_ - dt);
            UpdateCasinoVisuals();
            if (casinoSpinTimer_ <= 0.0f) FinishCasinoSpin();
        }
    }

    void HandleCasinoKey(int key) {
        if (casinoSpinTimer_ > 0.0f) return;
        if (key == KEY_ESCAPE) { CloseCasino(); return; }
        if (key == KEY_RETURN || key == KEY_SPACE) { StartCasinoSpin(); return; }
        if (key == KEY_Q || key == KEY_LEFT) {
            int t = (static_cast<int>(casinoBet_.type) + Casino::BET_COUNT - 1) % Casino::BET_COUNT;
            casinoBet_.type = static_cast<Casino::BetType>(t); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (key == KEY_E || key == KEY_RIGHT) {
            int t = (static_cast<int>(casinoBet_.type) + 1) % Casino::BET_COUNT;
            casinoBet_.type = static_cast<Casino::BetType>(t); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (key == KEY_A || key == KEY_DOWN) {
            casinoBet_.amount = Casino::NextChipAmount(casinoBet_.amount, -1); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (key == KEY_D || key == KEY_UP) {
            casinoBet_.amount = Casino::NextChipAmount(casinoBet_.amount, 1); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (key == KEY_W) {
            casinoBet_.number = Casino::ClampNumber(casinoBet_.number + 1); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (key == KEY_S) {
            casinoBet_.number = Casino::ClampNumber(casinoBet_.number - 1); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (key == KEY_R) {
            casinoBet_.number = rand() % 37; UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (key == KEY_T) {
            casinoBalance_ += 5000; SaveSettingsFile(); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (key >= KEY_0 && key <= KEY_9) {
            casinoBet_.number = Clamp(static_cast<int>(key) - static_cast<int>(KEY_0), 0, 9); UpdateCasinoVisuals(); return;
        }
    }

    void HandleCasinoClick() {
        auto* input = GetSubsystem<Input>();
        if (!input || casinoSpinTimer_ > 0.0f) return;
        IntVector2 mp = input->GetMousePosition();
        if (HitUIElement(casinoSpinButton_.Get(), mp)) { StartCasinoSpin(); return; }
        if (HitUIElement(casinoBetButton_.Get(), mp)) {
            int t = (static_cast<int>(casinoBet_.type) + 1) % Casino::BET_COUNT;
            casinoBet_.type = static_cast<Casino::BetType>(t); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (HitUIElement(casinoChipButton_.Get(), mp)) {
            casinoBet_.amount = Casino::NextChipAmount(casinoBet_.amount, 1); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (HitUIElement(casinoNumberButton_.Get(), mp)) {
            casinoBet_.number = (casinoBet_.number + 1) % 37; UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (HitUIElement(casinoTopupButton_.Get(), mp)) {
            casinoBalance_ += 5000; SaveSettingsFile(); UpdateCasinoVisuals(); PlayUISound(); return;
        }
        if (HitUIElement(casinoCloseButton_.Get(), mp)) CloseCasino();
    }

    // =========================================================================
    // СИМУЛЯТОР ПАРНОЙ «КОЧЕТ В БАНЬКЕ» (14 концовок, 57 загадок, баффы Арены)
    // =========================================================================

    void ResetBanyaUIPointers() {
        banyaParticles_.Clear();
        banyaPanel_ = nullptr; banyaSceneBox_ = nullptr;
        banyaRiddleBox_ = nullptr; banyaEndBox_ = nullptr; banyaStatsBox_ = nullptr;
        banyaHeatWash_ = nullptr; banyaAirWash_ = nullptr; banyaColdWash_ = nullptr; banyaEndTint_ = nullptr;
        banyaStoveFire_ = nullptr; banyaStonesGlow_ = nullptr; banyaVenikVis_ = nullptr;
        banyaRoosterBody_ = nullptr; banyaRoosterComb_ = nullptr; banyaRoosterCrown_ = nullptr;
        banyaRoosterEyesText_ = nullptr;
        banyaTempText_ = nullptr; banyaMoodText_ = nullptr; banyaClockText_ = nullptr;
        banyaCaptionText_ = nullptr; banyaBadgeText_ = nullptr;
        banyaBarSteam_ = nullptr; banyaBarFire_ = nullptr; banyaBarAir_ = nullptr;
        banyaBarLife_ = nullptr; banyaBarTime_ = nullptr;
        banyaRiddleQ_ = nullptr; banyaRiddleTime_ = nullptr; banyaRiddleFb_ = nullptr;
        banyaRiddleTimerBar_ = nullptr;
        for (int i = 0; i < 4; ++i) { banyaRiddleOptBg_[i] = nullptr; banyaRiddleOptTxt_[i] = nullptr; }
        for (int i = 0; i < 5; ++i) { banyaActBtnBg_[i] = nullptr; }
        banyaStatsBtnBg_ = nullptr;
        banyaEndTitle_ = nullptr; banyaEndText_ = nullptr; banyaEndHint_ = nullptr; banyaEndStats_ = nullptr;
        banyaEndBtnRestart_ = nullptr; banyaEndBtnStats_ = nullptr; banyaEndBtnBack_ = nullptr;
        banyaStatsRunText_ = nullptr; banyaStatsAllText_ = nullptr; banyaStatsCloseBtn_ = nullptr;
    }

    static String FormatBanyaClock(float sec) {
        int s = Max(0, (int)ceilf(sec));
        int m = s / 60;
        int r = s % 60;
        return String(m) + ":" + (r < 10 ? "0" : "") + String(r);
    }

    void PlayBanyaSound(Sound* s, float gain = 0.85f) {
        if (soundSource_ && s) {
            soundSource_->SetGain(sfxVolume_ * gain);
            soundSource_->Play(s);
        }
    }

    void SetBanyaCaption(const String& txt, float holdSec = 4.0f) {
        if (banyaCaptionText_) {
            banyaCaptionText_->SetText(txt);
            banyaCaptionText_->SetOpacity(1.0f);
        }
        banyaCaptionTimer_ = holdSec;
        banyaLastWarnTime_ = timeAcc_;
    }

    void SpawnBanyaParticle(float nx, float ny, float vx, float vy, float ttl, float sz, int kind) {
        if (!banyaSceneBox_) return;
        if (banyaParticles_.Size() >= 96) {
            if (banyaParticles_[0].el_) banyaParticles_[0].el_->Remove();
            banyaParticles_.Erase(0);
        }
        BorderImage* p = banyaSceneBox_->CreateChild<BorderImage>();
        int pxSz = Max(2, S(sz));
        p->SetSize(pxSz, pxSz);
        Color col(1.0f, 1.0f, 1.0f, 0.35f);
        if (kind == 1)      col = Color(0.28f, 0.20f, 0.14f, 0.45f); // smoke
        else if (kind == 2) col = Color(0.72f, 0.90f, 0.98f, 0.85f); // water drop
        else if (kind == 3) col = Color(0.45f, 0.62f, 0.28f, 0.90f); // birch leaf
        else if (kind == 4) col = Color(0.95f, 0.98f, 1.00f, 0.88f); // snow
        else if (kind == 5) col = Color(1.00f, 0.62f, 0.18f, 0.95f); // ember
        else if (kind == 6) col = Color(1.00f, 0.90f, 0.55f, 0.95f); // spark
        p->SetColor(col);
        p->SetPosition((int)(nx * banyaSceneBox_->GetWidth()), (int)(ny * banyaSceneBox_->GetHeight()));
        BanyaUIParticle bp;
        bp.el_ = SharedPtr<BorderImage>(p);
        bp.x_ = nx; bp.y_ = ny; bp.vx_ = vx; bp.vy_ = vy;
        bp.life_ = 0.0f; bp.ttl_ = ttl; bp.sz_ = sz; bp.kind_ = kind;
        banyaParticles_.Push(bp);
    }

    void CreateBanyaPanel() {
        if (banyaPanel_) return;
        RefreshUIScale();
        auto* ui = GetSubsystem<UI>();
        auto* gfx = GetSubsystem<Graphics>();
        if (!ui || !gfx) return;
        UIElement* root = ui->GetRoot();
        int sw = gfx->GetWidth(), sh = gfx->GetHeight();

        banyaPanel_ = root->CreateChild<UIElement>("BanyaPanel");
        banyaPanel_->SetSize(sw, sh);
        banyaPanel_->SetPriority(700);

        // Тёплый хвойно-древесный фон предбанника
        BorderImage* outerBg = banyaPanel_->CreateChild<BorderImage>();
        outerBg->SetSize(sw, sh);
        outerBg->SetColor(Color(0.09f, 0.06f, 0.04f, 0.97f));

        // Верхняя плашка интеграции CHIKENGUN 2.5
        BorderImage* topStrip = banyaPanel_->CreateChild<BorderImage>();
        topStrip->SetPosition(0, 0);
        topStrip->SetSize(sw, S(40));
        topStrip->SetColor(Color(0.14f, 0.08f, 0.04f, 0.95f));

        banyaBadgeText_ = topStrip->CreateChild<Text>();
        banyaBadgeText_->SetFont(font_, SF(14));
        banyaBadgeText_->SetPosition(S(18), S(10));
        banyaBadgeText_->SetColor(Color(1.0f, 0.86f, 0.62f, 1.0f));
        StyleCrispText(banyaBadgeText_);

        // Сцена парной (пропорции 1000x600 по центру экрана)
        int scW = Min(sw - S(40), S(1120));
        int scH = Min(sh - S(150), (int)(scW * 0.60f));
        int scX = (sw - scW) / 2;
        int scY = S(48);

        banyaSceneBox_ = banyaPanel_->CreateChild<UIElement>("BanyaScene");
        banyaSceneBox_->SetPosition(scX, scY);
        banyaSceneBox_->SetSize(scW, scH);
        banyaSceneBox_->SetClipChildren(true);

        // Бревенчатая стена
        BorderImage* wall = banyaSceneBox_->CreateChild<BorderImage>();
        wall->SetSize(scW, scH);
        wall->SetColor(Color(0.36f, 0.22f, 0.12f, 1.0f));
        for (int i = 0; i < 12; ++i) {
            BorderImage* plank = banyaSceneBox_->CreateChild<BorderImage>();
            plank->SetPosition((scW * i) / 12, 0);
            plank->SetSize(Max(2, S(2)), scH);
            plank->SetColor(Color(0.16f, 0.09f, 0.04f, 0.52f));
        }
        // Потолочная балка и пол
        BorderImage* ceil = banyaSceneBox_->CreateChild<BorderImage>();
        ceil->SetPosition(0, 0); ceil->SetSize(scW, (int)(scH * 0.11f));
        ceil->SetColor(Color(0.22f, 0.13f, 0.07f, 1.0f));

        BorderImage* floorPlank = banyaSceneBox_->CreateChild<BorderImage>();
        floorPlank->SetPosition(0, (int)(scH * 0.81f));
        floorPlank->SetSize(scW, scH - (int)(scH * 0.81f));
        floorPlank->SetColor(Color(0.27f, 0.16f, 0.09f, 1.0f));

        // Заснеженное окно справа
        BorderImage* winFrame = banyaSceneBox_->CreateChild<BorderImage>();
        winFrame->SetPosition((int)(scW * 0.76f), (int)(scH * 0.16f));
        winFrame->SetSize((int)(scW * 0.19f), (int)(scH * 0.30f));
        winFrame->SetColor(Color(0.18f, 0.10f, 0.05f, 1.0f));

        BorderImage* winGlass = winFrame->CreateChild<BorderImage>();
        winGlass->SetPosition(S(6), S(6));
        winGlass->SetSize(winFrame->GetWidth() - S(12), winFrame->GetHeight() - S(12));
        winGlass->SetColor(Color(0.72f, 0.86f, 0.94f, 1.0f));

        // Каменная печь слева + труба + топка + камни
        BorderImage* pipe = banyaSceneBox_->CreateChild<BorderImage>();
        pipe->SetPosition((int)(scW * 0.15f), (int)(scH * 0.11f));
        pipe->SetSize((int)(scW * 0.08f), (int)(scH * 0.34f));
        pipe->SetColor(Color(0.36f, 0.34f, 0.31f, 1.0f));

        BorderImage* stove = banyaSceneBox_->CreateChild<BorderImage>();
        stove->SetPosition((int)(scW * 0.06f), (int)(scH * 0.44f));
        stove->SetSize((int)(scW * 0.28f), (int)(scH * 0.42f));
        stove->SetColor(Color(0.29f, 0.25f, 0.22f, 1.0f));

        banyaStoveFire_ = stove->CreateChild<BorderImage>();
        banyaStoveFire_->SetPosition((int)(stove->GetWidth() * 0.30f), (int)(stove->GetHeight() * 0.56f));
        banyaStoveFire_->SetSize((int)(stove->GetWidth() * 0.40f), (int)(stove->GetHeight() * 0.38f));
        banyaStoveFire_->SetColor(Color(1.0f, 0.58f, 0.16f, 0.95f));

        banyaStonesGlow_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaStonesGlow_->SetPosition((int)(scW * 0.09f), (int)(scH * 0.40f));
        banyaStonesGlow_->SetSize((int)(scW * 0.22f), (int)(scH * 0.06f));
        banyaStonesGlow_->SetColor(Color(0.45f, 0.41f, 0.38f, 1.0f));

        // Двухъярусный деревянный полок + шайка с водой
        BorderImage* bench1 = banyaSceneBox_->CreateChild<BorderImage>();
        bench1->SetPosition((int)(scW * 0.42f), (int)(scH * 0.54f));
        bench1->SetSize((int)(scW * 0.55f), (int)(scH * 0.06f));
        bench1->SetColor(Color(0.76f, 0.56f, 0.35f, 1.0f));

        BorderImage* bench2 = banyaSceneBox_->CreateChild<BorderImage>();
        bench2->SetPosition((int)(scW * 0.47f), (int)(scH * 0.71f));
        bench2->SetSize((int)(scW * 0.46f), (int)(scH * 0.05f));
        bench2->SetColor(Color(0.68f, 0.48f, 0.29f, 1.0f));

        BorderImage* bucket = banyaSceneBox_->CreateChild<BorderImage>();
        bucket->SetPosition((int)(scW * 0.74f), (int)(scH * 0.78f));
        bucket->SetSize((int)(scW * 0.10f), (int)(scH * 0.13f));
        bucket->SetColor(Color(0.54f, 0.35f, 0.19f, 1.0f));
        BorderImage* bucketWater = bucket->CreateChild<BorderImage>();
        bucketWater->SetPosition(S(4), S(3));
        bucketWater->SetSize(bucket->GetWidth() - S(8), S(8));
        bucketWater->SetColor(Color(0.32f, 0.62f, 0.72f, 0.95f));

        // Берёзовый веник на гвоздике (анимируется к кочету при [2])
        banyaVenikVis_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaVenikVis_->SetPosition((int)(scW * 0.41f), (int)(scH * 0.31f));
        banyaVenikVis_->SetSize((int)(scW * 0.05f), (int)(scH * 0.14f));
        banyaVenikVis_->SetColor(Color(0.38f, 0.56f, 0.26f, 1.0f));

        // Кочет в полотенце на полке
        banyaRoosterBody_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaRoosterBody_->SetPosition((int)(scW * 0.61f), (int)(scH * 0.36f));
        banyaRoosterBody_->SetSize((int)(scW * 0.11f), (int)(scH * 0.18f));
        banyaRoosterBody_->SetColor(Color(0.96f, 0.91f, 0.82f, 1.0f));

        BorderImage* towel = banyaRoosterBody_->CreateChild<BorderImage>();
        towel->SetPosition(0, (int)(banyaRoosterBody_->GetHeight() * 0.54f));
        towel->SetSize(banyaRoosterBody_->GetWidth(), (int)(banyaRoosterBody_->GetHeight() * 0.42f));
        towel->SetColor(Color(0.93f, 0.89f, 0.82f, 1.0f));
        BorderImage* towelStripe = towel->CreateChild<BorderImage>();
        towelStripe->SetPosition(0, towel->GetHeight() / 2);
        towelStripe->SetSize(towel->GetWidth(), Max(2, S(4)));
        towelStripe->SetColor(Color(0.76f, 0.29f, 0.21f, 0.9f));

        banyaRoosterComb_ = banyaRoosterBody_->CreateChild<BorderImage>();
        banyaRoosterComb_->SetPosition(S(8), -S(14));
        banyaRoosterComb_->SetSize((int)(banyaRoosterBody_->GetWidth() * 0.42f), S(15));
        banyaRoosterComb_->SetColor(Color(0.88f, 0.26f, 0.18f, 1.0f));

        banyaRoosterCrown_ = banyaRoosterBody_->CreateChild<BorderImage>();
        banyaRoosterCrown_->SetPosition(S(6), -S(24));
        banyaRoosterCrown_->SetSize((int)(banyaRoosterBody_->GetWidth() * 0.50f), S(12));
        banyaRoosterCrown_->SetColor(Color(1.0f, 0.84f, 0.26f, 1.0f));
        banyaRoosterCrown_->SetVisible(false);

        BorderImage* beak = banyaRoosterBody_->CreateChild<BorderImage>();
        beak->SetPosition(-S(14), S(14));
        beak->SetSize(S(15), S(10));
        beak->SetColor(Color(0.98f, 0.72f, 0.22f, 1.0f));

        banyaRoosterEyesText_ = banyaRoosterBody_->CreateChild<Text>();
        banyaRoosterEyesText_->SetFont(font_, SF(15));
        banyaRoosterEyesText_->SetPosition(S(10), S(8));
        banyaRoosterEyesText_->SetColor(Color(0.14f, 0.08f, 0.04f, 1.0f));
        banyaRoosterEyesText_->SetText("●");

        // Атмосферные фильтры опасности / концовок
        banyaColdWash_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaColdWash_->SetSize(scW, scH);
        banyaColdWash_->SetColor(Color(0.66f, 0.85f, 0.94f, 0.0f));

        banyaHeatWash_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaHeatWash_->SetSize(scW, scH);
        banyaHeatWash_->SetColor(Color(1.0f, 0.28f, 0.08f, 0.0f));

        banyaAirWash_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaAirWash_->SetSize(scW, scH);
        banyaAirWash_->SetColor(Color(0.04f, 0.08f, 0.04f, 0.0f));

        banyaEndTint_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaEndTint_->SetSize(scW, scH);
        banyaEndTint_->SetColor(Color(0.0f, 0.0f, 0.0f, 0.0f));

        // HUD термометра и 4 шкал слева сверху
        BorderImage* hudCard = banyaSceneBox_->CreateChild<BorderImage>();
        hudCard->SetPosition(S(16), S(16));
        hudCard->SetSize(S(216), S(178));
        hudCard->SetColor(Color(0.14f, 0.08f, 0.05f, 0.84f));

        banyaTempText_ = hudCard->CreateChild<Text>();
        banyaTempText_->SetFont(font_, SF(28));
        banyaTempText_->SetPosition(S(14), S(8));
        banyaTempText_->SetColor(Color(1.0f, 0.87f, 0.68f, 1.0f));
        StyleCrispText(banyaTempText_, true);

        banyaMoodText_ = hudCard->CreateChild<Text>();
        banyaMoodText_->SetFont(font_, SF(13));
        banyaMoodText_->SetPosition(S(14), S(46));
        banyaMoodText_->SetColor(Color(0.96f, 0.90f, 0.80f, 0.90f));
        StyleCrispText(banyaMoodText_);

        const char* barNames[4] = { "ПАР", "ПЕЧЬ", "ВОЗДУХ", "КОЧЕТ" };
        Color barCols[4] = {
            Color(1.0f, 0.74f, 0.45f, 1.0f),
            Color(1.0f, 0.38f, 0.16f, 1.0f),
            Color(0.42f, 0.80f, 0.94f, 1.0f),
            Color(0.45f, 0.82f, 0.42f, 1.0f)
        };
        SharedPtr<BorderImage>* barTargets[4] = { &banyaBarSteam_, &banyaBarFire_, &banyaBarAir_, &banyaBarLife_ };
        for (int i = 0; i < 4; ++i) {
            Text* lbl = hudCard->CreateChild<Text>();
            lbl->SetFont(font_, SF(11));
            lbl->SetText(barNames[i]);
            lbl->SetPosition(S(14), S(74 + i * 24));
            lbl->SetColor(Color(0.92f, 0.84f, 0.72f, 0.85f));
            StyleCrispText(lbl);

            BorderImage* trk = hudCard->CreateChild<BorderImage>();
            trk->SetPosition(S(74), S(79 + i * 24));
            trk->SetSize(S(126), S(8));
            trk->SetColor(Color(1.0f, 1.0f, 1.0f, 0.15f));

            BorderImage* fill = trk->CreateChild<BorderImage>();
            fill->SetSize(S(60), S(8));
            fill->SetColor(barCols[i]);
            *(barTargets[i]) = SharedPtr<BorderImage>(fill);
        }

        // Таймер 3:00 и счётчик загадок по центру сверху
        BorderImage* clockCard = banyaSceneBox_->CreateChild<BorderImage>();
        clockCard->SetPosition(scW / 2 - S(95), S(16));
        clockCard->SetSize(S(190), S(54));
        clockCard->SetColor(Color(0.14f, 0.08f, 0.05f, 0.84f));

        banyaClockText_ = clockCard->CreateChild<Text>();
        banyaClockText_->SetFont(font_, SF(16));
        banyaClockText_->SetTextAlignment(HA_CENTER);
        banyaClockText_->SetPosition(S(8), S(7));
        banyaClockText_->SetWidth(S(174));
        banyaClockText_->SetColor(Color(1.0f, 0.91f, 0.78f, 1.0f));
        StyleCrispText(banyaClockText_);

        BorderImage* timeTrk = clockCard->CreateChild<BorderImage>();
        timeTrk->SetPosition(S(18), S(36));
        timeTrk->SetSize(S(154), S(7));
        timeTrk->SetColor(Color(1.0f, 1.0f, 1.0f, 0.16f));
        banyaBarTime_ = timeTrk->CreateChild<BorderImage>();
        banyaBarTime_->SetSize(S(154), S(7));
        banyaBarTime_->SetColor(Color(1.0f, 0.78f, 0.42f, 1.0f));

        // Кнопка Статистика [6] справа сверху
        banyaStatsBtnBg_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaStatsBtnBg_->SetPosition(scW - S(168), S(16));
        banyaStatsBtnBg_->SetSize(S(152), S(38));
        banyaStatsBtnBg_->SetColor(Color(0.22f, 0.13f, 0.07f, 0.90f));
        Text* stLbl = banyaStatsBtnBg_->CreateChild<Text>();
        stLbl->SetFont(font_, SF(13));
        stLbl->SetText("[6] СТАТИСТИКА");
        stLbl->SetTextAlignment(HA_CENTER);
        stLbl->SetPosition(S(6), S(9));
        stLbl->SetWidth(S(140));
        stLbl->SetColor(Color(1.0f, 0.89f, 0.74f, 1.0f));
        StyleCrispText(stLbl);

        // Живая карточка загадки смотрящего (игра НЕ останавливается!)
        int rW = Min(scW - S(32), S(650));
        banyaRiddleBox_ = banyaSceneBox_->CreateChild<BorderImage>();
        banyaRiddleBox_->SetPosition((scW - rW) / 2, scH - S(238));
        banyaRiddleBox_->SetSize(rW, S(196));
        static_cast<BorderImage*>(banyaRiddleBox_.Get())->SetColor(Color(0.18f, 0.10f, 0.05f, 0.96f));
        banyaRiddleBox_->SetVisible(false);

        Text* rKick = banyaRiddleBox_->CreateChild<Text>();
        rKick->SetFont(font_, SF(12));
        rKick->SetText("ЗАГАДКА СМОТРЯЩЕГО (ответь мышью или Q / W / E / R)");
        rKick->SetPosition(S(14), S(8));
        rKick->SetColor(Color(0.96f, 0.82f, 0.62f, 0.85f));
        StyleCrispText(rKick);

        banyaRiddleTime_ = banyaRiddleBox_->CreateChild<Text>();
        banyaRiddleTime_->SetFont(font_, SF(13));
        banyaRiddleTime_->SetPosition(rW - S(64), S(8));
        banyaRiddleTime_->SetColor(Color(1.0f, 0.85f, 0.55f, 1.0f));
        StyleCrispText(banyaRiddleTime_);

        banyaRiddleQ_ = banyaRiddleBox_->CreateChild<Text>();
        banyaRiddleQ_->SetFont(font_, SF(15));
        banyaRiddleQ_->SetPosition(S(14), S(29));
        banyaRiddleQ_->SetWidth(rW - S(28));
        banyaRiddleQ_->SetWordwrap(true);
        banyaRiddleQ_->SetColor(Color(1.0f, 0.95f, 0.87f, 1.0f));
        StyleCrispText(banyaRiddleQ_);

        static const char* optKeys[4] = { "[Q] ", "[W] ", "[E] ", "[R] " };
        int optW = (rW - S(36)) / 2;
        for (int i = 0; i < 4; ++i) {
            BorderImage* ob = banyaRiddleBox_->CreateChild<BorderImage>();
            ob->SetPosition(S(14) + (i % 2) * (optW + S(8)), S(76) + (i / 2) * S(38));
            ob->SetSize(optW, S(33));
            ob->SetColor(Color(0.30f, 0.19f, 0.10f, 0.90f));
            banyaRiddleOptBg_[i] = SharedPtr<BorderImage>(ob);

            Text* ot = ob->CreateChild<Text>();
            ot->SetFont(font_, SF(13));
            ot->SetPosition(S(8), S(7));
            ot->SetWidth(optW - S(16));
            ot->SetText(optKeys[i]);
            ot->SetColor(Color(1.0f, 0.92f, 0.80f, 1.0f));
            StyleCrispText(ot);
            banyaRiddleOptTxt_[i] = SharedPtr<Text>(ot);
        }

        BorderImage* rTrk = banyaRiddleBox_->CreateChild<BorderImage>();
        rTrk->SetPosition(S(14), S(156));
        rTrk->SetSize(rW - S(28), S(5));
        rTrk->SetColor(Color(1.0f, 1.0f, 1.0f, 0.16f));
        banyaRiddleTimerBar_ = rTrk->CreateChild<BorderImage>();
        banyaRiddleTimerBar_->SetSize(rW - S(28), S(5));
        banyaRiddleTimerBar_->SetColor(Color(1.0f, 0.72f, 0.32f, 1.0f));

        banyaRiddleFb_ = banyaRiddleBox_->CreateChild<Text>();
        banyaRiddleFb_->SetFont(font_, SF(12));
        banyaRiddleFb_->SetPosition(S(14), S(165));
        banyaRiddleFb_->SetWidth(rW - S(28));
        banyaRiddleFb_->SetColor(Color(0.85f, 0.95f, 0.78f, 1.0f));
        StyleCrispText(banyaRiddleFb_);

        // Строка реплик и 5 кнопок действий под сценой
        banyaCaptionText_ = banyaSceneBox_->CreateChild<Text>();
        banyaCaptionText_->SetFont(font_, SF(15));
        banyaCaptionText_->SetTextAlignment(HA_CENTER);
        banyaCaptionText_->SetPosition(S(40), scH - S(34));
        banyaCaptionText_->SetWidth(scW - S(80));
        banyaCaptionText_->SetColor(Color(1.0f, 0.94f, 0.84f, 0.96f));
        StyleCrispText(banyaCaptionText_, true);

        const char* actLabels[5] = {
            "[1] Поддать пару",
            "[2] Веником",
            "[3] Окатить",
            "[4] Проветрить",
            "[5] Загадка"
        };
        int btnW = S(186), btnGap = S(10);
        int totalBtnsW = btnW * 5 + btnGap * 4;
        int btnStartX = (sw - totalBtnsW) / 2;
        int btnY = scY + scH + S(12);
        for (int i = 0; i < 5; ++i) {
            BorderImage* b = banyaPanel_->CreateChild<BorderImage>();
            b->SetPosition(btnStartX + i * (btnW + btnGap), btnY);
            b->SetSize(btnW, S(42));
            b->SetColor(Color(0.27f, 0.16f, 0.09f, 0.95f));
            banyaActBtnBg_[i] = SharedPtr<BorderImage>(b);

            Text* bt = b->CreateChild<Text>();
            bt->SetFont(font_, SF(14));
            bt->SetText(actLabels[i]);
            bt->SetTextAlignment(HA_CENTER);
            bt->SetPosition(S(6), S(11));
            bt->SetWidth(btnW - S(12));
            bt->SetColor(Color(1.0f, 0.91f, 0.78f, 1.0f));
            StyleCrispText(bt);
        }

        Text* foot = banyaPanel_->CreateChild<Text>();
        foot->SetFont(font_, SF(12));
        foot->SetTextAlignment(HA_CENTER);
        foot->SetPosition(S(20), btnY + S(50));
        foot->SetWidth(sw - S(40));
        foot->SetColor(Color(0.84f, 0.76f, 0.64f, 0.72f));
        foot->SetText("Цель: 3 минуты и минимум 3 загадки смотрящего (6 — на Ковш Смотрящего). [1..5] действия, [Q/W/E/R] ответы, [6] статистика, [ESC] выход.");
        StyleCrispText(foot);

        // Оверлей концовки (14 концовок)
        banyaEndBox_ = banyaPanel_->CreateChild<BorderImage>();
        int eW = S(580), eH = S(360);
        banyaEndBox_->SetPosition((sw - eW) / 2, (sh - eH) / 2);
        banyaEndBox_->SetSize(eW, eH);
        static_cast<BorderImage*>(banyaEndBox_.Get())->SetColor(Color(0.18f, 0.11f, 0.06f, 0.98f));
        banyaEndBox_->SetVisible(false);

        banyaEndTitle_ = banyaEndBox_->CreateChild<Text>();
        banyaEndTitle_->SetFont(font_, SF(28));
        banyaEndTitle_->SetTextAlignment(HA_CENTER);
        banyaEndTitle_->SetPosition(S(20), S(18));
        banyaEndTitle_->SetWidth(eW - S(40));
        banyaEndTitle_->SetColor(Color(1.0f, 0.85f, 0.55f, 1.0f));
        StyleCrispText(banyaEndTitle_, true);

        banyaEndText_ = banyaEndBox_->CreateChild<Text>();
        banyaEndText_->SetFont(font_, SF(15));
        banyaEndText_->SetPosition(S(24), S(66));
        banyaEndText_->SetWidth(eW - S(48));
        banyaEndText_->SetWordwrap(true);
        banyaEndText_->SetColor(Color(0.94f, 0.87f, 0.78f, 1.0f));
        StyleCrispText(banyaEndText_);

        banyaEndHint_ = banyaEndBox_->CreateChild<Text>();
        banyaEndHint_->SetFont(font_, SF(13));
        banyaEndHint_->SetPosition(S(24), S(148));
        banyaEndHint_->SetWidth(eW - S(48));
        banyaEndHint_->SetWordwrap(true);
        banyaEndHint_->SetColor(Color(0.86f, 0.78f, 0.66f, 0.92f));
        StyleCrispText(banyaEndHint_);

        banyaEndStats_ = banyaEndBox_->CreateChild<Text>();
        banyaEndStats_->SetFont(font_, SF(14));
        banyaEndStats_->SetTextAlignment(HA_CENTER);
        banyaEndStats_->SetPosition(S(20), S(208));
        banyaEndStats_->SetWidth(eW - S(40));
        banyaEndStats_->SetColor(Color(1.0f, 0.88f, 0.58f, 1.0f));
        StyleCrispText(banyaEndStats_);

        auto mkEndBtn = [&](int bx, const char* lbl, const Color& bgCol) -> SharedPtr<BorderImage> {
            BorderImage* bb = banyaEndBox_->CreateChild<BorderImage>();
            bb->SetPosition(bx, eH - S(58));
            bb->SetSize(S(168), S(40));
            bb->SetColor(bgCol);
            Text* tt = bb->CreateChild<Text>();
            tt->SetFont(font_, SF(14));
            tt->SetText(lbl);
            tt->SetTextAlignment(HA_CENTER);
            tt->SetPosition(S(6), S(10));
            tt->SetWidth(S(156));
            tt->SetColor(Color(1.0f, 0.94f, 0.84f, 1.0f));
            StyleCrispText(tt);
            return SharedPtr<BorderImage>(bb);
        };
        banyaEndBtnRestart_ = mkEndBtn(S(26),  "[ENTER] Ещё раз",  Color(0.36f, 0.21f, 0.10f, 0.98f));
        banyaEndBtnStats_   = mkEndBtn(S(206), "[6] Статистика",   Color(0.24f, 0.15f, 0.08f, 0.95f));
        banyaEndBtnBack_    = mkEndBtn(S(386), "[ESC] В Чикенган", Color(0.14f, 0.34f, 0.24f, 0.98f));

        // Двухколоночная панель статистики [6] (ставит симуляцию и таймер загадки на паузу!)
        banyaStatsBox_ = banyaPanel_->CreateChild<BorderImage>();
        int sW = S(640), sH = S(420);
        banyaStatsBox_->SetPosition((sw - sW) / 2, (sh - sH) / 2);
        banyaStatsBox_->SetSize(sW, sH);
        static_cast<BorderImage*>(banyaStatsBox_.Get())->SetColor(Color(0.16f, 0.10f, 0.05f, 0.98f));
        banyaStatsBox_->SetVisible(false);

        Text* stHead = banyaStatsBox_->CreateChild<Text>();
        stHead->SetFont(font_, SF(22));
        stHead->SetText("СТАТИСТИКА ПАРНОЙ (пауза)");
        stHead->SetPosition(S(22), S(16));
        stHead->SetColor(Color(1.0f, 0.86f, 0.56f, 1.0f));
        StyleCrispText(stHead, true);

        banyaStatsCloseBtn_ = banyaStatsBox_->CreateChild<BorderImage>();
        banyaStatsCloseBtn_->SetPosition(sW - S(128), S(14));
        banyaStatsCloseBtn_->SetSize(S(106), S(32));
        banyaStatsCloseBtn_->SetColor(Color(0.32f, 0.19f, 0.10f, 0.95f));
        Text* clTxt = banyaStatsCloseBtn_->CreateChild<Text>();
        clTxt->SetFont(font_, SF(13));
        clTxt->SetText("[6/ESC] Закр.");
        clTxt->SetTextAlignment(HA_CENTER);
        clTxt->SetPosition(S(4), S(7));
        clTxt->SetWidth(S(98));
        clTxt->SetColor(Color(1.0f, 0.92f, 0.80f, 1.0f));
        StyleCrispText(clTxt);

        banyaStatsRunText_ = banyaStatsBox_->CreateChild<Text>();
        banyaStatsRunText_->SetFont(font_, SF(13));
        banyaStatsRunText_->SetPosition(S(22), S(62));
        banyaStatsRunText_->SetWidth(sW / 2 - S(30));
        banyaStatsRunText_->SetColor(Color(0.95f, 0.89f, 0.80f, 1.0f));
        StyleCrispText(banyaStatsRunText_);

        banyaStatsAllText_ = banyaStatsBox_->CreateChild<Text>();
        banyaStatsAllText_->SetFont(font_, SF(13));
        banyaStatsAllText_->SetPosition(sW / 2 + S(12), S(62));
        banyaStatsAllText_->SetWidth(sW / 2 - S(30));
        banyaStatsAllText_->SetColor(Color(0.95f, 0.89f, 0.80f, 1.0f));
        StyleCrispText(banyaStatsAllText_);
    }

    void OpenBanyaSimulator(bool fromMenu) {
        banyaFromMenu_ = fromMenu;
        banyaOpen_ = true;
        banyaRewardApplied_ = false;
        banyaSim_.Seed((uint32_t)(timeAcc_ * 1000.0f) ^ (uint32_t)rand() ^ 0xC0C4E7u);
        banyaSim_.Reset();
        banyaVenikAnim_ = 0.0f;

        if (!banyaPanel_) CreateBanyaPanel();
        if (banyaPanel_) banyaPanel_->SetVisible(true);
        if (mainMenuPanel_) mainMenuPanel_->SetVisible(false);
        SetGameHudVisible(false);

        auto* input = GetSubsystem<Input>();
        if (input) {
            input->SetMouseVisible(true);
            isMouseVisible_ = true;
        }
        SetBanyaCaption("Банька натоплена. Три минуты, три загадки смотрящего — и выйдешь с зачётом.", 6.0f);
        SyncBanyaVisuals(0.0f);
    }

    void CloseBanyaSimulator() {
        banyaOpen_ = false;
        banyaSim_.SetPaused(false);
        if (banyaPanel_) banyaPanel_->SetVisible(false);
        if (banyaStatsBox_) banyaStatsBox_->SetVisible(false);

        auto* input = GetSubsystem<Input>();
        if (banyaFromMenu_) {
            ShowMainMenu(true);
        } else {
            SetGameHudVisible(!photoMode_);
            if (input) {
                input->SetMouseVisible(false);
                isMouseVisible_ = false;
            }
            UpdateHealthDisplay();
            UpdateScoreDisplay();
            UpdateDashAndBuffHUD();
        }
    }

    void ApplyBanyaEndingRewards() {
        if (banyaRewardApplied_ || banyaSim_.phase != Banya::PHASE_OVER) return;
        banyaRewardApplied_ = true;

        int pts = banyaSim_.CurrentScore();
        int cr  = Banya::ComputeCreditsReward(pts, banyaSim_.ending);
        int tier = Banya::BuffTierForEnding(banyaSim_.ending);

        if (banyaFromMenu_) banyaBonusCredits_ += cr;
        else                credits_ += cr;

        player_.score_ += pts;
        if (player_.score_ > highScore_) highScore_ = player_.score_;

        if (tier > banyaBuffTier_) banyaBuffTier_ = tier;
        if (!banyaFromMenu_ && tier > 0) {
            float addHp  = (tier >= 2) ? 50.0f : 25.0f;
            float addArm = (tier >= 2) ? 50.0f : 25.0f;
            player_.maxHealth_ = Max(player_.maxHealth_, 100.0f + addHp + 25.0f * (float)perkHpLvl_);
            player_.health_    = Min(player_.maxHealth_, player_.health_ + 60.0f);
            player_.armor_     = Min(player_.maxArmor_,  player_.armor_ + addArm);
            if (tier >= 2) orbitalCharge_ = Min(100.0f, orbitalCharge_ + 35.0f);
        }

        if (banyaSim_.ending == Banya::END_WIN || banyaSim_.ending == Banya::END_STEWARD)
            UnlockAchievement(12, "С ЛЁГКИМ ПАРОМ! (ЗАЧЁТ В БАНЬКЕ)");
        if (banyaSim_.ending == Banya::END_STEWARD)
            UnlockAchievement(13, "СМОТРЯЩИЙ ЗА БАНЕЙ (КОВШ И 6+ ЗАГАДОК)");

        SaveSettingsFile();
    }

    void RefreshBanyaStatsTexts() {
        if (!banyaStatsRunText_ || !banyaStatsAllText_) return;
        const auto& R = banyaSim_.run;
        const auto& L = banyaSim_.lifetime;
        int runTot = R.correct + R.wrong + R.missed;
        String runAcc = runTot > 0 ? (String((R.correct * 100) / runTot) + "%") : String("—");
        int allTot = L.correct + L.wrong;
        String allAcc = allTot > 0 ? (String((L.correct * 100) / allTot) + "%") : String("—");

        String sRun = "ЭТОТ ЗАХОД\n\n"
            "Время в бане:  " + FormatBanyaClock(Min(banyaSim_.elapsed, Banya::DURATION)) + " / 3:00\n"
            "Диапазон жара: " + String((int)lroundf(R.minTemp)) + "–" + String((int)lroundf(R.maxTemp)) + " °C\n"
            "В середине:    " + FormatBanyaClock(R.secComfort) + "\n"
            "Красная зона:  " + FormatBanyaClock(R.secHot) + "\n"
            "На холоде:     " + FormatBanyaClock(R.secCold) + "\n"
            "Поддач пару:   " + String(R.pours) + "\n"
            "Веником:       " + String(R.veniks) + "\n"
            "Окатился:      " + String(R.rinses) + "\n"
            "Проветрил:     " + String(R.vents) + "\n"
            "Загадок:       " + String(R.asked) + " (верно " + String(R.correct) + ")\n"
            "Точность:      " + runAcc + " (серия " + String(R.bestStreak) + ")\n"
            "Мин. воздух:   " + String((int)lroundf(R.minAir)) + "%\n"
            "Очков:         " + String(banyaSim_.CurrentScore());

        String sAll = "ЗА ВСЁ ВРЕМЯ\n\n"
            "Заходов:       " + String(L.runs) + "\n"
            "Побед:         " + String(L.wins) + "\n"
            "Лучший счёт:   " + String(L.bestScore) + "\n"
            "Верных загадок:" + String(L.correct) + "\n"
            "Промахов:      " + String(L.wrong) + "\n"
            "Точность:      " + allAcc + "\n"
            "Лучшая серия:  " + String(L.bestStreak) + "\n"
            "Всего в парной:" + FormatBanyaClock((float)L.totalSeconds) + "\n"
            "Рекорд загадок:" + String(L.bestRiddles) + "\n"
            "Открыто конц.: " + String(Banya::CountSeenEndings(L.seenEndingsMask)) + " из 14\n"
            "Бафф Арены:    " + String(banyaBuffTier_ >= 2 ? "Ковш Смотрящего" : (banyaBuffTier_ == 1 ? "Банный закал" : "нет"));

        banyaStatsRunText_->SetText(sRun);
        banyaStatsAllText_->SetText(sAll);
    }

    void ToggleBanyaStats() {
        if (!banyaStatsBox_) return;
        bool open = !banyaStatsBox_->IsVisible();
        banyaStatsBox_->SetVisible(open);
        banyaSim_.SetPaused(open);
        if (open) RefreshBanyaStatsTexts();
        PlayUISound();
    }

    void TriggerBanyaAction(int actIdx) {
        if (banyaSim_.paused) return;
        if (actIdx == 4) {
            if (banyaSim_.OpenRiddle(false)) {
                PlayUISound();
                SetBanyaCaption("Смотрящий прищурился из пара и задал вопрос вне очереди.", 3.4f);
            }
            return;
        }
        if (banyaSim_.phase != Banya::PHASE_PLAY) return;

        if (actIdx == 0) {
            if (banyaSim_.ActSteam()) {
                PlayBanyaSound(banyaSteamSound_ ? banyaSteamSound_ : barrelBoomSound_, 0.85f);
                for (int i = 0; i < 16; ++i)
                    SpawnBanyaParticle(0.18f + (rand() % 100) * 0.0012f, 0.42f, ((rand() % 100) - 50) * 0.001f, -0.14f - (rand() % 100) * 0.001f, 1.8f, 12.0f, 0);
                if (banyaSim_.phase == Banya::PHASE_PLAY)
                    SetBanyaCaption("Пшшш… пар пошёл под потолок, камни довольны.");
            }
        } else if (actIdx == 1) {
            if (banyaSim_.ActVenik()) {
                banyaVenikAnim_ = 1.6f;
                PlayBanyaSound(banyaVenikSound_ ? banyaVenikSound_ : wingSlapSound_, 0.88f);
                for (int i = 0; i < 14; ++i)
                    SpawnBanyaParticle(0.62f + (rand() % 100) * 0.001f, 0.38f, ((rand() % 100) - 50) * 0.002f, 0.10f + (rand() % 100) * 0.001f, 1.6f, 6.0f, 3);
                if (banyaSim_.phase == Banya::PHASE_PLAY)
                    SetBanyaCaption("Берёзовый дух… косточки размякли, кочет тает.");
            }
        } else if (actIdx == 2) {
            bool wasCold = false;
            if (banyaSim_.ActWater(&wasCold)) {
                PlayBanyaSound(banyaSplashSound_ ? banyaSplashSound_ : armorPickupSound_, 0.85f);
                for (int i = 0; i < 16; ++i)
                    SpawnBanyaParticle(0.76f + (rand() % 100) * 0.0008f, 0.76f, ((rand() % 100) - 65) * 0.002f, -0.18f + (rand() % 100) * 0.002f, 1.1f, 5.0f, 2);
                if (banyaSim_.phase == Banya::PHASE_PLAY)
                    SetBanyaCaption(wasCold ? "Ледяная вода по остывшему кочету — брр, квохчет от холода!" : "Ух! Прохладная водица из шайки — голова ясная.");
            }
        } else if (actIdx == 3) {
            bool wasCold = false;
            if (banyaSim_.ActVent(&wasCold)) {
                PlayBanyaSound(banyaDoorSound_ ? banyaDoorSound_ : dashSound_, 0.80f);
                for (int i = 0; i < 20; ++i)
                    SpawnBanyaParticle(0.78f + (rand() % 100) * 0.0014f, 0.22f + (rand() % 100) * 0.0018f, -0.10f - (rand() % 100) * 0.001f, 0.08f, 1.9f, 4.5f, 4);
                if (banyaSim_.phase == Banya::PHASE_PLAY)
                    SetBanyaCaption(wasCold ? "И без того прохладно, а ты дверь настежь — кочет дрожит!" : "Дверь скрипнула, морозный воздух — дышать легче.");
            }
        }
    }

    void AnswerBanyaRiddleUI(int optIdx) {
        if (!banyaSim_.riddle.active || banyaSim_.riddle.settled) return;
        if (banyaSim_.AnswerRiddle(optIdx)) {
            bool ok = (optIdx == banyaSim_.riddle.correctIdx);
            PlayBanyaSound(ok ? comboSound_ : emptyClickSound_, 0.85f);
            for (int i = 0; i < 12; ++i)
                SpawnBanyaParticle(0.65f, 0.36f, ((rand() % 100) - 50) * 0.0025f, -0.12f, 1.2f, 5.0f, ok ? 6 : 1);
        }
    }

    void HandleBanyaKey(int key) {
        if (key == KEY_6) { ToggleBanyaStats(); return; }
        if (key == KEY_ESCAPE) {
            if (banyaStatsBox_ && banyaStatsBox_->IsVisible()) { ToggleBanyaStats(); return; }
            PlayUISound();
            CloseBanyaSimulator();
            return;
        }
        if (banyaSim_.paused) return;

        if (banyaSim_.phase == Banya::PHASE_OVER) {
            if (key == KEY_RETURN || key == KEY_SPACE || key == KEY_R) {
                PlayUISound();
                banyaRewardApplied_ = false;
                banyaSim_.Reset();
                if (banyaEndBox_) banyaEndBox_->SetVisible(false);
                SetBanyaCaption("Новый заход! Три минуты и три загадки смотрящего.", 5.0f);
                SyncBanyaVisuals(0.0f);
            }
            return;
        }

        if (key >= KEY_1 && key <= KEY_5) { TriggerBanyaAction(key - KEY_1); return; }
        if (banyaSim_.riddle.active && !banyaSim_.riddle.settled) {
            if (key == KEY_Q) { AnswerBanyaRiddleUI(0); return; }
            if (key == KEY_W) { AnswerBanyaRiddleUI(1); return; }
            if (key == KEY_E) { AnswerBanyaRiddleUI(2); return; }
            if (key == KEY_R) { AnswerBanyaRiddleUI(3); return; }
        }
    }

    static bool HitUIElement(UIElement* el, const IntVector2& mp) {
        if (!el || !el->IsVisible()) return false;
        IntVector2 p = el->GetScreenPosition();
        IntVector2 s = el->GetSize();
        return mp.x_ >= p.x_ && mp.x_ <= p.x_ + s.x_ && mp.y_ >= p.y_ && mp.y_ <= p.y_ + s.y_;
    }

    void HandleBanyaClick() {
        auto* input = GetSubsystem<Input>();
        if (!input) return;
        IntVector2 mp = input->GetMousePosition();

        if (banyaStatsBox_ && banyaStatsBox_->IsVisible()) {
            if (HitUIElement(banyaStatsCloseBtn_.Get(), mp) || !HitUIElement(banyaStatsBox_.Get(), mp))
                ToggleBanyaStats();
            return;
        }
        if (HitUIElement(banyaStatsBtnBg_.Get(), mp)) { ToggleBanyaStats(); return; }

        if (banyaSim_.phase == Banya::PHASE_OVER && banyaEndBox_ && banyaEndBox_->IsVisible()) {
            if (HitUIElement(banyaEndBtnRestart_.Get(), mp)) {
                PlayUISound();
                banyaRewardApplied_ = false;
                banyaSim_.Reset();
                banyaEndBox_->SetVisible(false);
                SetBanyaCaption("Новый заход! Три минуты и три загадки смотрящего.", 5.0f);
                SyncBanyaVisuals(0.0f);
            } else if (HitUIElement(banyaEndBtnStats_.Get(), mp)) {
                ToggleBanyaStats();
            } else if (HitUIElement(banyaEndBtnBack_.Get(), mp)) {
                PlayUISound();
                CloseBanyaSimulator();
            }
            return;
        }

        if (banyaSim_.riddle.active && !banyaSim_.riddle.settled) {
            for (int i = 0; i < 4; ++i) {
                if (HitUIElement(banyaRiddleOptBg_[i].Get(), mp)) {
                    AnswerBanyaRiddleUI(i);
                    return;
                }
            }
        }
        for (int i = 0; i < 5; ++i) {
            if (HitUIElement(banyaActBtnBg_[i].Get(), mp)) {
                TriggerBanyaAction(i);
                return;
            }
        }
    }

    void SyncBanyaVisuals(float dt) {
        if (!banyaPanel_ || !banyaSceneBox_) return;
        int scW = banyaSceneBox_->GetWidth(), scH = banyaSceneBox_->GetHeight();

        int wallet = banyaFromMenu_ ? (100 + banyaBonusCredits_) : credits_;
        String buffStr = (banyaBuffTier_ >= 2) ? "👑 КОВШ СМОТРЯЩЕГО (+50HP/+15%УР)"
                       : (banyaBuffTier_ == 1) ? "♨ БАННЫЙ ЗАКАЛ (+25HP/+25БР)"
                                               : "нет баффа";
        if (banyaBadgeText_) {
            banyaBadgeText_->SetText("🐔 CHIKENGUN 2.5  •  КОШЕЛЁК: " + String(wallet) + "₵  •  БАФФ АРЕНЫ: " + buffStr +
                                     "  •  ОТКРЫТО КОНЦОВОК: " + String(Banya::CountSeenEndings(banyaSim_.lifetime.seenEndingsMask)) + "/14");
        }

        if (banyaTempText_) {
            banyaTempText_->SetText(String((int)lroundf(banyaSim_.temp)) + " °C");
            banyaTempText_->SetColor(banyaSim_.temp > 100.0f ? Color(1.0f, 0.52f, 0.36f, 1.0f) : Color(1.0f, 0.87f, 0.68f, 1.0f));
        }
        if (banyaMoodText_)  banyaMoodText_->SetText(banyaSim_.CurrentMood());
        if (banyaClockText_) banyaClockText_->SetText(FormatBanyaClock(banyaSim_.left) + "   |   ✓ " + String(banyaSim_.riddles) + "/3");

        auto setBar = [&](BorderImage* b, float ratio, int fullW) {
            if (b) b->SetSize(Clamp((int)(fullW * Clamp(ratio, 0.0f, 1.0f)), 0, fullW), b->GetHeight());
        };
        setBar(banyaBarSteam_.Get(), banyaSim_.steam, S(126));
        setBar(banyaBarFire_.Get(),  banyaSim_.fire / 100.0f, S(126));
        setBar(banyaBarAir_.Get(),   banyaSim_.air  / 100.0f, S(126));
        setBar(banyaBarLife_.Get(),  banyaSim_.life / 100.0f, S(126));
        setBar(banyaBarTime_.Get(),  banyaSim_.left / Banya::DURATION, S(154));

        // Свечение печи и камней от температуры
        if (banyaStonesGlow_) {
            float hot = Clamp((banyaSim_.temp - 80.0f) / 32.0f, 0.0f, 1.0f);
            banyaStonesGlow_->SetColor(Color(0.45f + 0.55f * hot, 0.41f - 0.12f * hot, 0.38f - 0.25f * hot, 1.0f));
        }
        if (banyaStoveFire_) {
            float f = Clamp(0.35f + banyaSim_.fire / 110.0f, 0.2f, 1.0f);
            banyaStoveFire_->SetColor(Color(1.0f, 0.42f + 0.32f * f, 0.12f, f));
        }

        // Анимация веника и мимики Кочета
        if (banyaVenikVis_) {
            if (banyaVenikAnim_ > 0.0f) {
                float u = 1.0f - banyaVenikAnim_ / 1.6f;
                int vx = (int)(scW * (0.41f + 0.20f * sinf(u * 3.14159f)));
                int vy = (int)(scH * (0.31f + 0.05f * sinf(u * 12.56f)));
                banyaVenikVis_->SetPosition(vx, vy);
            } else {
                banyaVenikVis_->SetPosition((int)(scW * 0.41f), (int)(scH * 0.31f));
            }
        }
        if (banyaRoosterBody_ && banyaRoosterEyesText_) {
            int baseX = (int)(scW * 0.61f), baseY = (int)(scH * 0.36f);
            if (banyaSim_.phase == Banya::PHASE_OVER) {
                banyaRoosterEyesText_->SetText((banyaSim_.ending == Banya::END_WIN || banyaSim_.ending == Banya::END_STEWARD) ? "◠" : "✕");
               banyaRoosterCrown_->SetVisible(banyaSim_.ending == Banya::END_STEWARD);
                if (banyaSim_.ending == Banya::END_FROZEN)      banyaRoosterBody_->SetColor(Color(0.68f, 0.88f, 0.98f, 1.0f));
                else if (banyaSim_.ending == Banya::END_BURNED || banyaSim_.ending == Banya::END_FIRE || banyaSim_.ending == Banya::END_BLAST)
                    banyaRoosterBody_->SetColor(Color(0.38f, 0.24f, 0.18f, 1.0f));
                else if (banyaSim_.ending == Banya::END_VAPOR || banyaSim_.ending == Banya::END_ESCAPE)
                    banyaRoosterBody_->SetColor(Color(0.95f, 0.95f, 0.95f, 0.12f));
                else banyaRoosterBody_->SetColor(Color(0.96f, 0.91f, 0.82f, 1.0f));
            } else {
                banyaRoosterCrown_->SetVisible(false);
                int sway = (banyaSim_.blissRemaining > 0.0f) ? (int)(sinf(timeAcc_ * 3.0f) * S(4.0f))
                         : (banyaSim_.ventRemaining  > 0.0f) ? (int)(sinf(timeAcc_ * 22.0f)  * S(2.0f)) : 0;
                banyaRoosterBody_->SetPosition(baseX + sway, baseY);
                banyaRoosterEyesText_->SetText(banyaSim_.blissRemaining > 0.0f ? "◡" : "●");
                Color bodyC = (banyaSim_.temp > 100.0f) ? Color(1.0f, 0.72f, 0.64f, 1.0f)
                            : (banyaSim_.temp < 60.0f)  ? Color(0.84f, 0.91f, 0.98f, 1.0f)
                                                        : Color(0.96f, 0.91f, 0.82f, 1.0f);
                banyaRoosterBody_->SetColor(bodyC);
            }
        }

        // Опасные виньетки
        if (banyaHeatWash_) {
            float ha = banyaSim_.temp > 100.0f ? Clamp((banyaSim_.temp - 100.0f) / 18.0f, 0.0f, 0.42f) : 0.0f;
            banyaHeatWash_->SetColor(Color(1.0f, 0.25f, 0.06f, ha));
        }
        if (banyaAirWash_) {
            float aa = banyaSim_.air < 30.0f ? Clamp((30.0f - banyaSim_.air) / 30.0f, 0.0f, 0.55f) : 0.0f;
            banyaAirWash_->SetColor(Color(0.03f, 0.06f, 0.03f, aa));
        }
        if (banyaColdWash_) {
            float ca = banyaSim_.ventRemaining > 0.0f ? 0.16f : 0.0f;
            banyaColdWash_->SetColor(Color(0.66f, 0.86f, 0.95f, ca));
        }

        // Карточка загадки
        if (banyaRiddleBox_) {
            bool showR = banyaSim_.riddle.active && banyaSim_.phase == Banya::PHASE_PLAY;
            banyaRiddleBox_->SetVisible(showR);
            if (showR) {
                const auto& rd = banyaSim_.riddle;
                banyaRiddleQ_->SetText(rd.q);
                banyaRiddleTime_->SetText(String(rd.timeLeft, 1) + "с");
                int rBarFull = banyaRiddleBox_->GetWidth() - S(28);
                setBar(banyaRiddleTimerBar_.Get(), rd.timeLeft / 15.0f, rBarFull);
                static const char* kPref[4] = { "[Q] ", "[W] ", "[E] ", "[R] " };
                for (int i = 0; i < 4; ++i) {
                    banyaRiddleOptTxt_[i]->SetText(String(kPref[i]) + rd.opts[i]);
                    if (!rd.settled) {
                        banyaRiddleOptBg_[i]->SetColor(Color(0.30f, 0.19f, 0.10f, 0.92f));
                    } else if (i == rd.correctIdx) {
                        banyaRiddleOptBg_[i]->SetColor(Color(0.20f, 0.52f, 0.22f, 0.95f));
                    } else if (i == rd.chosenIdx) {
                        banyaRiddleOptBg_[i]->SetColor(Color(0.58f, 0.18f, 0.14f, 0.95f));
                    } else {
                        banyaRiddleOptBg_[i]->SetColor(Color(0.22f, 0.14f, 0.08f, 0.70f));
                    }
                }
                if (rd.settled) {
                    bool ok = (rd.chosenIdx == rd.correctIdx);
                    String fb = ok ? (String("Верно! ") + rd.explanation)
                                   : (String(rd.chosenIdx < 0 ? "Молчание! " : "Мимо! ") + "Правильно: «" + rd.opts[rd.correctIdx] + "». " + rd.explanation);
                    banyaRiddleFb_->SetText(fb);
                    banyaRiddleFb_->SetColor(ok ? Color(0.75f, 0.95f, 0.68f, 1.0f) : Color(1.0f, 0.74f, 0.66f, 1.0f));
                } else {
                    banyaRiddleFb_->SetText("");
                }
            }
        }

        // Карточка концовки
        if (banyaEndBox_) {
            bool over = (banyaSim_.phase == Banya::PHASE_OVER);
            banyaEndBox_->SetVisible(over);
            if (over) {
                ApplyBanyaEndingRewards();
                const auto& ei = Banya::GetEndingInfo(banyaSim_.ending);
                int pts = banyaSim_.CurrentScore();
                int cr  = Banya::ComputeCreditsReward(pts, banyaSim_.ending);
                int tier = Banya::BuffTierForEnding(banyaSim_.ending);
                banyaEndTint_->SetColor(Color(ei.tintR, ei.tintG, ei.tintB, ei.tintA));
                banyaEndTitle_->SetText(ei.title);
                banyaEndText_->SetText(ei.text);
                banyaEndHint_->SetText(String("Совет: ") + ei.hint);
                String rewardLine = "Награда Чикенган: +" + String(cr) + "₵";
                if (tier >= 2)      rewardLine += "  •  👑 БАФФ «КОВШ СМОТРЯЩЕГО» (+50 HP/Брони, +15% Урона, +35% Орбиты)!";
                else if (tier == 1) rewardLine += "  •  ♨ БАФФ «БАННЫЙ ЗАКАЛ» (+25 HP/Брони)!";
                banyaEndStats_->SetText(
                    "Продержался " + FormatBanyaClock(Min(banyaSim_.elapsed, Banya::DURATION)) + " из 3:00  •  загадок " +
                    String(banyaSim_.riddles) + "  •  очков " + String(pts) + "\n" +
                    "Открыто концовок: " + String(Banya::CountSeenEndings(banyaSim_.lifetime.seenEndingsMask)) + " из 14\n" +
                    rewardLine);
            } else {
                banyaEndTint_->SetColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
            }
        }

        // Фоновые частицы пара
        if (dt > 0.0f) {
            if (banyaSim_.phase == Banya::PHASE_PLAY && !banyaSim_.paused) {
                banyaSteamAcc_ += dt * (2.5f + banyaSim_.steam * 10.0f);
                while (banyaSteamAcc_ >= 1.0f) {
                    banyaSteamAcc_ -= 1.0f;
                    SpawnBanyaParticle(0.14f + (rand() % 100) * 0.0016f, 0.42f,
                                       ((rand() % 100) - 50) * 0.0006f, -0.09f - (rand() % 100) * 0.0006f,
                                       2.2f, 10.0f + banyaSim_.steam * 10.0f, 0);
                }
            }
            for (unsigned i = banyaParticles_.Size(); i-- > 0;) {
                BanyaUIParticle& p = banyaParticles_[i];
                p.life_ += dt;
                if (p.life_ >= p.ttl_ || !p.el_) {
                    if (p.el_) p.el_->Remove();
                    banyaParticles_.Erase(i);
                    continue;
                }
                if (p.kind_ == 2) p.vy_ += 0.45f * dt;
                p.x_ += p.vx_ * dt;
                p.y_ += p.vy_ * dt;
                p.el_->SetPosition((int)(p.x_ * scW), (int)(p.y_ * scH));
                p.el_->SetOpacity(Clamp(1.0f - p.life_ / p.ttl_, 0.0f, 1.0f) * (p.kind_ == 0 ? 0.35f : 0.85f));
            }
        }
    }

    void UpdateBanyaSimulator(float dt) {
        if (banyaVenikAnim_ > 0.0f) banyaVenikAnim_ = Max(0.0f, banyaVenikAnim_ - dt);
        if (banyaCaptionTimer_ > 0.0f) {
            banyaCaptionTimer_ -= dt;
            if (banyaCaptionTimer_ <= 0.0f && banyaCaptionText_)
                banyaCaptionText_->SetOpacity(0.0f);
        }

        bool wasRiddle = banyaSim_.riddle.active;
        banyaSim_.Step(dt);
        if (!wasRiddle && banyaSim_.riddle.active) {
            PlayUISound();
            SetBanyaCaption("Смотрящий из пара загадывает загадку. Не ответишь — кочету худо.", 3.5f);
        }

        // Предупреждения состояния парной
        if (banyaSim_.phase == Banya::PHASE_PLAY && !banyaSim_.paused && (timeAcc_ - banyaLastWarnTime_) > 7.0f) {
            if (banyaSim_.fire > 82.0f)       SetBanyaCaption("Печь раскалилась — того и гляди займётся! Дай ей остыть.");
            else if (banyaSim_.air < 26.0f)   SetBanyaCaption("Душно! Проветри [4], пока кочет не угорел.");
            else if (banyaSim_.temp > 103.0f) SetBanyaCaption("Кочет красный, как гребешок. Окати [3] или проветри [4]!");
            else if (banyaSim_.temp < 60.0f)  SetBanyaCaption("Холодно! Поддай пару [1], не то кочет замёрзнет.");
            else if (banyaSim_.steamMax > 10.0f) SetBanyaCaption("Пар на максимуме — ещё немного, и кочет растворится в облаке!");
            else if (banyaSim_.life < 32.0f)  SetBanyaCaption("Силы на исходе. Верный ответ на загадку [5] их вернёт.");
        }

        SyncBanyaVisuals(dt);
    }
};

// ============================================================================
// Макрос регистрации приложения
// ============================================================================

URHO3D_DEFINE_APPLICATION_MAIN(MyApp)

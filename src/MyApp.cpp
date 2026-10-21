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
    Node* weaponModelNode_ = nullptr;
    Light* muzzleLight_ = nullptr;
    Node* muzzleLightNode_ = nullptr;
    float wallHeight_ = 5.0f;
    
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
    void RemoveColliderLite(int idx) {
        if (idx >= 0 && idx < (int)colliders_.Size()) colliders_[(unsigned)idx].tag = 7;
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
        for (unsigned i = 0; i < colliders_.Size(); ++i) {
            const Lite::Collider& c = colliders_[i];
            if (c.tag == 7 || !(tagMask & (1 << c.tag))) continue; // 7 = удалён
            float t = c.sphere ? Lite::RaySphere(origin, dir, c.center, c.half.x_)
                               : Lite::RayBox(origin, dir, c.center, c.half);
            if (t >= 0.0f && t < best) {
                best = t; bestTag = c.tag; bestPtr = c.ptr; hit = true;
                bestCenter = c.center; bestHalf = c.half; bestSphere = c.sphere;
            }
        }
        if (hit) {
            outDist = best; outPoint = origin + dir * best;
            if (outTag) *outTag = bestTag;
            if (outPtr) *outPtr = bestPtr;
            if (outNormal) {
                if (bestSphere) *outNormal = (outPoint - bestCenter).Normalized();
                else *outNormal = HitNormalLite(outPoint, bestCenter, bestHalf);
            }
        }
        return hit;
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
    Vector3 lastCamPos_ = Vector3(0, GameConstants::PLAYER_HEIGHT, 0);
    Vector3 camVelocitySmoothed_ = Vector3::ZERO; // для velocity-based наклона
    
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
    int settingsSel_ = 0;           // 0 тени, 1 bloom, 2 разрешение, 3 анизотропия, 4 SFX, 5 музыка, 6 назад
    int gfxShadowIdx_ = 2;          // 0=1K 1=2K 2=4K
    int gfxGlowIdx_ = 2;            // 0=выкл 1=слабый 2=полный
    int gfxResIdx_ = 0;             // 0=натив 1=75% 2=50%
    int gfxAnisoIdx_ = 2;           // 0=4x 1=8x 2=16x
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
    SharedPtr<Text> gameOverStats_;
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
    };
    Vector<EnemyData> enemies_;
    Vector<SharedPtr<Node>> enemyNodes_;   // владение нодами до удаления
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
    int waveNum_ = 0;
    float waveRespawnTimer_ = 0.0f;
    // Комбо и рекорды
    int comboCount_ = 0;
    float comboTimer_ = 0.0f;
    int bestCombo_ = 0;
    int highScore_ = 0;
    float rangeRespawnTimer_ = 0.0f;
    SharedPtr<Material> robotBodyMat_;
    SharedPtr<Material> robotEyeMat_;
    int currentWeapon_ = W_RIFLE;
    int currentSkin_ = 0;
    Vector<Node*> robotNodes_;
    float waveSpawnTimer_ = 0.0f;
    int waveNumber_ = 0;
    Vector<SharedPtr<Node>> decoNodes_;
    SharedPtr<Text> weaponNameText_;
    bool armoryOpen_ = false;
    SharedPtr<UIElement> armoryPanel_;
    Vector<SharedPtr<Text>> armoryWeaponRows_;
    Vector<SharedPtr<Text>> armorySkinRows_;
    float autoFireTimer_ = 0.0f;
    SharedPtr<UIElement> hudPanelBg_;
    SharedPtr<BorderImage> healthBarFill_;
    
    // Ресурсы
    Model* boxModel_ = nullptr;
    Model* sphereModel_ = nullptr;
    Model* planeModel_ = nullptr;
    Material* stoneMaterial_ = nullptr;
    Material* metalMaterial_ = nullptr;
    Material* targetMaterial_ = nullptr;
    Material* targetHitMaterial_ = nullptr;
    Material* floorMaterialLight_ = nullptr;
    Material* floorMaterialDark_ = nullptr;
    Material* hdStone_ = nullptr;
    Material* hdFloor_ = nullptr;
    Material* hdPanel_ = nullptr;
    Material* hdMetal_ = nullptr;
    Material* hdCrate_ = nullptr;
    Material* skyMaterial_ = nullptr;
    Texture* skyTexture_ = nullptr;
    Vector<SharedPtr<Material>> weaponTintMats_;
    ParticleEffect* sparkEffect_ = nullptr;
    ParticleEffect* smokeEffect_ = nullptr;
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
        ShowMessage("CHIKENGUN 2.1 | WASD - движение, SHIFT - бег, SPACE - прыжок, CTRL - присед\nЛКМ - огонь, ПКМ - прицел, 1-7 оружие, TAB - арсенал, F - режим, Esc - выход");
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
        renderer->SetHDRRendering(true);
        ConfigureGlow(renderer, true);              // HDR-bloom через XML-параметры (API-сеттеров нет в этой сборке)
        
        // Настройка окна
    }
    
    bool LoadResources() {
        auto* cache = GetSubsystem<ResourceCache>();
        
        // Добавляем директории ресурсов
        cache->AddResourceDir("Data", false);
        cache->AddResourceDir("CoreData", false);
        
        // Загружаем базовые модели
        boxModel_ = cache->GetResource<Model>("Models/Box.mdl");
        sphereModel_ = cache->GetResource<Model>("Models/Sphere.mdl");
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

    SharedPtr<Material> MakeTinted(const SharedPtr<Texture>& diff, const SharedPtr<Texture>& norm, const Color& c) {
        auto* cache = GetSubsystem<ResourceCache>();
        // Готовый unlit-материал из xml (базовая техника уже в нём) — Technique.h недоступен в этой сборке Urho3D
        //SharedPtr<Material> m = cache->GetResource<Material>("Materials/Game/TintUnlit.xml");
        Material* raw_m = cache->GetResource<Material>("Materials/Game/TintUnlit.xml");
        SharedPtr<Material> m(raw_m);
        if (!m) return m;

        m = m->Clone();
        if (diff) { m->SetTexture(TU_DIFFUSE, diff); }
        // SetDiffuseColor нет — цвет через шейдерный параметр MatDiffColor (0..1)
        m->SetShaderParameter("MatDiffColor", Variant(Color(c.r_ / 255.0f, c.g_ / 255.0f, c.b_ / 255.0f, 1.0f)));
        tintedMatStore_.Push(m);   // держим ссылку: указатели остаются живыми
        return m;
    }

 void CreateMaterials() {
     auto* cache = GetSubsystem<ResourceCache>();
     // --- HD текстуры (сгенерированы tools/gen_textures.py) ---
     hdStone_  = cache->GetResource<Material>("Materials/Game/StoneHD.xml");
     hdFloor_  = cache->GetResource<Material>("Materials/Game/FloorTile.xml");
     hdPanel_  = cache->GetResource<Material>("Materials/Game/MetalPanel.xml");
     hdMetal_  = cache->GetResource<Material>("Materials/Game/BrushedMetal.xml");
     hdCrate_  = cache->GetResource<Material>("Materials/Game/CrateWood.xml");
     skyTexture_ = cache->GetResource<Texture>("Textures/SkyDay.png"); // базовый класс Texture — заголовки Technique/Texture2D недоступны в этой сборке

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
         for (const char* tn : hdTexNames) {
            // В этой сборке Urho3D GetResource возвращает голый указатель — не оборачиваем в SharedPtr
            Texture* t = cache->GetResource<Texture>(String(tn));
            if (t) {
                t->SetFilterMode(FILTER_ANISOTROPIC); // анизотропная фильтрация для пола под углом
            }
        }
     }

     stoneMaterial_ = hdStone_ ? hdStone_ : cache->GetResource<Material>("Materials/Stone.xml");
     if (!stoneMaterial_) stoneMaterial_ = new Material(context_);
     metalMaterial_ = hdMetal_ ? hdMetal_ : stoneMaterial_;
     // Мишени: яркие процедурные unlit-материалы, чтобы были видны всегда
     {
         SharedPtr<Texture> white;
         targetMaterial_ = MakeTinted(white, nullptr, Color(230, 60, 60));
         targetHitMaterial_ = MakeTinted(white, nullptr, Color(255, 220, 120));
     }
     floorMaterialLight_ = hdFloor_ ? hdFloor_ : stoneMaterial_;
     floorMaterialDark_  = hdPanel_ ? hdPanel_ : stoneMaterial_;
     // Анизотропная фильтрация: глобальная настройка движка (per-material MAT_ANISOTROPY в этой сборке недоступен)
     // Глобальная анизотропия недоступна в этой сборке — фильтрация по умолчанию
     // Материалы оружия под текущий скин
     RefreshWeaponMaterials();
     URHO3D_LOGINFO(stoneMaterial_ ? "Base material OK (HD)" : "Base material NULL");
 }

    void RefreshWeaponMaterials() {
        weaponTintMats_.Clear();
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
        
        // Эффект искр
        sparkEffect_ = cache->GetResource<ParticleEffect>("Particle/Spark.pfx");
        if (!sparkEffect_) {
            sparkEffect_ = CreateSparkEffect();
        }
        
        // Эффект дыма
        smokeEffect_ = cache->GetResource<ParticleEffect>("Particle/Smoke.pfx");
        if (!smokeEffect_) {
            smokeEffect_ = CreateSmokeEffect();
        }
    }
    
    ParticleEffect* CreateSparkEffect() {
        auto* effect = new ParticleEffect(context_);
        
        // Настройки эмиттера
        effect->SetMinEmissionRate(50.0f); effect->SetMaxEmissionRate(50.0f);
        effect->SetNumParticles(100);
        effect->SetMinTimeToLive(0.5f); effect->SetMaxTimeToLive(0.5f);
        
        // Настройки частиц
        effect->SetMinVelocity(1.0f);
        effect->SetMaxVelocity(3.0f);
        return effect;
    }
    
    ParticleEffect* CreateSmokeEffect() {
        auto* effect = new ParticleEffect(context_);
        
        effect->SetMinEmissionRate(30.0f); effect->SetMaxEmissionRate(30.0f);
        effect->SetNumParticles(50);
        effect->SetMinTimeToLive(1.0f); effect->SetMaxTimeToLive(1.0f);
        effect->SetMinVelocity(1.0f);
        effect->SetMaxVelocity(3.0f);
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
        CreateTargets();
        
        // Небо и декорации новой арены
        CreateSky();
        CreateArenaDecor();

        // Создаем освещение
        CreateLighting();

        // Зона рендеринга: ambient + дымка по дальности (ощущение масштаба арены)
        CreateZone();
    }

    void CreateZone() {
        Node* zoneNode = scene_->CreateChild("Zone");
        Zone* zone = zoneNode->CreateComponent<Zone>();
        zone->SetBoundingBox(BoundingBox(-700.0f, 700.0f));
        // Мягкий холодный ambient — нет чёрных провалов, но контраст теней сохраняется
        zone->SetAmbientColor(Color(0.30f, 0.34f, 0.44f));
        zone->SetFogColor(Color(0.60f, 0.70f, 0.84f));
        zone->SetFogStart(80.0f);
        zone->SetFogEnd(260.0f);
        // SetFarDistance нет в этой сборке — дальность тумана уже задана SetFogEnd
    }

    void CreateSky() {
        // Небо через Skybox: рисуется до сцены на бесконечности — не мерцает с геометрией
        Node* skyNode = scene_->CreateChild("Sky");
        auto* cache = GetSubsystem<ResourceCache>();
        // Техника берётся из xml-материала (Technique.h недоступен в этой сборке Urho3D)
        {
            //SharedPtr<Material> skyBase = cache->GetResource<Material>("Materials/Game/TintUnlit.xml");
            Material* raw_skyBase = cache->GetResource<Material>("Materials/Game/TintUnlit.xml");
            SharedPtr<Material> skyBase(raw_skyBase);
            if (skyBase) {
                SharedPtr<Material> skyClone = skyBase->Clone();
                skyMaterial_ = skyClone.Get();
                tintedMatStore_.Push(skyClone);   // держим ссылку, чтобы материал не удалился
            } else {
                skyMaterial_ = new Material(context_);
                tintedMatStore_.Push(SharedPtr<Material>(skyMaterial_));
            }
        }
        if (skyTexture_) skyMaterial_->SetTexture(TU_DIFFUSE, skyTexture_);
        skyMaterial_->SetShaderParameter("MatDiffColor", Variant(Color(150.0f / 255.0f, 185.0f / 255.0f, 235.0f / 255.0f, 1.0f)));
        skyMaterial_->SetCullMode(CULL_NONE);
        Skybox* sky = skyNode->CreateComponent<Skybox>();
        sky->SetModel(boxModel_);
        sky->SetMaterial(skyMaterial_);
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
            Vector3(-14, 1, -14), Vector3(22, 1, 4), Vector3(-24, 1, -2)
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
            hm->SetMaterial(MakeTinted(nullptr, nullptr, Color(255, 230, 160)).Get());
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
            Node* ln = scene_->CreateChild("BeaconLight");
            ln->SetPosition(tp + Vector3(0, 12.4f, 0));
            Light* l = ln->CreateComponent<Light>();
            l->SetLightType(LIGHT_POINT);
            l->SetColor(Color(1.0f, 0.15f, 0.15f));
            l->SetRange(8.0f);
        }
    }
    
    void CreateFloor() {
        const int gridSize = GameConstants::ARENA_SIZE / 2;
        const float tileSize = GameConstants::TILE_SIZE;
        
        for (int x = -gridSize; x < gridSize; ++x) {
            for (int z = -gridSize; z < gridSize; ++z) {
                Node* tileNode = scene_->CreateChild("FloorTile");
                
                float posX = (x + 0.5f) * tileSize;
                float posZ = (z + 0.5f) * tileSize;
                tileNode->SetPosition(Vector3(posX, -0.1f, posZ));
                tileNode->SetScale(Vector3(tileSize * 0.98f, 1.0f, tileSize * 0.98f));
                
                StaticModel* model = tileNode->CreateComponent<StaticModel>();
                model->SetModel(planeModel_ ? planeModel_ : boxModel_);
                
                // Шахматный узор
                bool isLight = ((x + z) % 2) == 0;
                model->SetMaterial(isLight ? floorMaterialLight_ : floorMaterialDark_);
                
                // Пол в Lite-физике — плоскость y=-0.1 (см. RaycastLite), коллайдер на тайл не нужен
            }
        }
    }
    
    void CreateWalls() {
        const float wallHeight = 8.0f;    // выше: арена 192x192 м выглядит «стадионом»
        const float wallThickness = 2.0f;
        const float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        
        // Северная стена
        CreateWall(Vector3(0, wallHeight/2, -arenaSize), Vector3(arenaSize, wallHeight, wallThickness));
        // Южная стена
        CreateWall(Vector3(0, wallHeight/2, arenaSize), Vector3(arenaSize, wallHeight, wallThickness));
        // Западная стена
        CreateWall(Vector3(-arenaSize, wallHeight/2, 0), Vector3(wallThickness, wallHeight, arenaSize));
        // Восточная стена
        CreateWall(Vector3(arenaSize, wallHeight/2, 0), Vector3(wallThickness, wallHeight, arenaSize));

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
        // Создаем различные препятствия на арене
        
        // Колонны по углам
        float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        float offset = arenaSize * 0.6f;
        float columnHeight = 4.0f;
        
        Vector3 positions[] = {
            Vector3(offset, columnHeight/2, offset),
            Vector3(-offset, columnHeight/2, offset),
            Vector3(offset, columnHeight/2, -offset),
            Vector3(-offset, columnHeight/2, -offset)
        };
        
        for (const auto& pos : positions) {
            Node* columnNode = scene_->CreateChild("Column");
            columnNode->SetPosition(pos);
            columnNode->SetScale(Vector3(1.5f, columnHeight, 1.5f));
            
            StaticModel* model = columnNode->CreateComponent<StaticModel>();
            model->SetModel(boxModel_);
            model->SetMaterial(metalMaterial_);
            
            AddBoxCollider(pos, Vector3(0.75f, columnHeight/2, 0.75f)); // Lite
        }
        
        // Центральная платформа
        Node* platformNode = scene_->CreateChild("Platform");
        platformNode->SetPosition(Vector3(0, 0.5f, 0));
        platformNode->SetScale(Vector3(10.0f, 1.0f, 10.0f));
        
        StaticModel* platformModel = platformNode->CreateComponent<StaticModel>();
        platformModel->SetModel(boxModel_);
        platformModel->SetMaterial(stoneMaterial_);
        
        AddBoxCollider(Vector3(0, 0.5f, 0), Vector3(5.0f, 0.5f, 5.0f)); // Lite-платформа
        
        // Пандусы
        CreateRamp(Vector3(8.0f, 0.5f, 0), Vector3(0, 0, 1));
        CreateRamp(Vector3(-8.0f, 0.5f, 0), Vector3(0, 0, -1));
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
        // Основной направленный свет (солнце)
        Node* sunNode = scene_->CreateChild("Sun");
        sunNode->SetPosition(Vector3(0, 20, 10));
        sunNode->SetDirection(Vector3(-0.55f, -0.62f, -0.56f).Normalized());
        
        Light* sunLight = sunNode->CreateComponent<Light>();
        sunLight->SetLightType(LIGHT_DIRECTIONAL);
        sunLight->SetColor(Color(1.0f, 0.92f, 0.74f, 1.0f)); // «золотой час»: тёплый свет, длинные тени
        sunLight->SetBrightness(0.9f);
        sunLight->SetCastShadows(true);
        sunLight->SetShadowDistance(140.0f);   // 4K-карта покрывает 140 м вокруг игрока — края арены в дымке
        // SetNumCascadeShadows нет в этой сборке — тени одним каскадом, радиус задан выше через SetShadowDistance
        // SetShadowFocus нет в этой сборке
        // GetShadowBias() возвращает копию — правка не применяется, оставляем дефолтный bias
        // Точечные источники света
        float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        
        // Свет над платформой
        Node* pointLight1 = scene_->CreateChild("PointLight1");
        pointLight1->SetPosition(Vector3(0, 8, 0));
        
        Light* pl1 = pointLight1->CreateComponent<Light>();
        pl1->SetLightType(LIGHT_POINT);
        pl1->SetColor(Color(0.8f, 0.85f, 1.0f, 1.0f));
        pl1->SetBrightness(1.25f);
        pl1->SetRange(30.0f);
        pl1->SetCastShadows(true);
        pl1->SetShadowDistance(35.0f);
        
        // Дополнительные точечные светильники по углам
        Vector3 lightPositions[] = {
            Vector3(arenaSize * 0.5f, 6, arenaSize * 0.5f),
            Vector3(-arenaSize * 0.5f, 6, arenaSize * 0.5f),
            Vector3(arenaSize * 0.5f, 6, -arenaSize * 0.5f),
            Vector3(-arenaSize * 0.5f, 6, -arenaSize * 0.5f)
        };
        
        for (int i = 0; i < 4; ++i) {
            Node* lightNode = scene_->CreateChild("PointLight" + String(i + 2));
            lightNode->SetPosition(lightPositions[i]);
            
            Light* pl = lightNode->CreateComponent<Light>();
            pl->SetLightType(LIGHT_POINT);
            pl->SetColor(Color(1.0f, 0.9f, 0.7f, 1.0f));
            pl->SetBrightness(1.15f);
            pl->SetRange(30.0f);
            pl->SetCastShadows(false); // экономия: дорогие кубические тени только у солнца и центра
        }
        
        // ambient light
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

    Node* AddPart(Node* parent, const Vector3& pos, const Vector3& scale, Material* mat, Model* model = nullptr) {
        Node* n = parent->CreateChild("Part");
        n->SetPosition(pos);
        n->SetScale(scale);
        StaticModel* sm = n->CreateComponent<StaticModel>();
        sm->SetModel(model ? model : boxModel_);
        sm->SetMaterial(mat);
        sm->SetCastShadows(false);
        return n;
    }

    void BuildWeaponModel(int type) {
        if (!weaponModelNode_) return;
        weaponModelNode_->RemoveAllChildren();
        weaponModelNode_->SetPosition(Vector3::ZERO);
        weaponModelNode_->SetRotation(Quaternion::IDENTITY);
        weaponModelNode_->SetScale(Vector3::ONE);

        Material* gunMat = weaponTintMats_.Size() ? weaponTintMats_[type] : metalMaterial_;
        SharedPtr<Material> dark = MakeTinted(nullptr, nullptr, Color(35, 35, 40));
        SharedPtr<Material> accent = SKIN_DEFS[currentSkin_].emissive
            ? MakeTinted(nullptr, nullptr, Color(60, 255, 170))
            : MakeTinted(nullptr, nullptr, Color(25, 25, 28));
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
        currentWeapon_ = type;
        const WeaponDef& def = WEAPON_DEFS[type];
        weapon_.weaponType_ = type;
        weapon_.isReloading_ = false;
        weapon_.reloadTimer_ = 0;
        weapon_.fireTimer_ = 0;
        weapon_.currentAmmo_ = def.magSize;
        if (full) weapon_.reserveAmmo_ = def.reserveMax;
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
        messageText_->SetPosition(screenWidth / 2, screenHeight / 2);
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

        // Подсказка управления (левый верх)
        Text* help = root->CreateChild<Text>();
        help->SetFont(font_, 14);
        help->SetText("WASD — движение | SHIFT — бег | SPACE — прыжок | CTRL/C — присед | ПКМ — прицел | R — перезарядка | 1-7 — оружие | Q/E — скин | TAB — арсенал");
        help->SetPosition(20, 90);
        help->SetColor(Color(0.8f, 0.85f, 0.9f, 0.75f));
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
        if (!inMenu_ && !gameOver_ && !armoryOpen_ && !isMouseVisible_ && input->GetMouseButtonDown(MOUSEB_LEFT)) {
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
        
        // Применяем отдачу к оружию
        ApplyRecoilToWeapon();
        
        // === ОБРАБОТКА ВВОДА ДЛЯ ДВИЖЕНИЯ ===
        if (!gameOver_) HandleMovement(dt);

        // Обновляем анимацию покачивания оружия
        UpdateWeaponBob(dt);

        // === Game feel: хитмаркер и виньетка низкого HP ===
        UpdateHitMarker(dt);
        UpdateLowHealthVignette(dt);
        UpdateTracers(dt);
        UpdateEnemyProjectiles(dt);
        UpdateMovingTargetsLite(dt);
        UpdateDebrisLite(dt);

        // Камера медленно крутится вокруг арены, пока игрок в меню
        if (inMenu_) {
            yaw_ += dt * 6.0f;
        }

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
        
        // Если мышь видима (меню), не обрабатываем вращение
        if (isMouseVisible_) {
            return;
        }
        
        auto* input = GetSubsystem<Input>();
        
        // Получаем смещение мыши
        IntVector2 mouseMove = input->GetMouseMove();
        
        // Обновляем углы Эйлера
        float sens = GameConstants::MOUSE_SENSITIVITY;
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
                if (key == KEY_UP || key == KEY_W) { settingsSel_ = (settingsSel_ + 6) % 7; PlayUISound(); UpdateSettingsDisplay(); }
                else if (key == KEY_DOWN || key == KEY_S) { settingsSel_ = (settingsSel_ + 1) % 7; PlayUISound(); UpdateSettingsDisplay(); }
                else if (key == KEY_LEFT || key == KEY_A) { ChangeSetting(-1); }
                else if (key == KEY_RIGHT || key == KEY_D) { ChangeSetting(1); }
                else if (key == KEY_RETURN) { if (settingsSel_ == 6) ShowSettings(false); else ChangeSetting(1); }
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

        // Esc: сначала закрыть меню, потом выйти
        if (key == KEY_ESCAPE) {
            if (armoryOpen_) { ToggleArmory(); }
            else { engine_->Exit(); }
            return;
        }

        // 1..7 — быстрое переключение оружия
        if (key >= KEY_1 && key <= KEY_7) {
            int idx = key - KEY_1;
            if (idx < WEAPON_COUNT) { EquipWeapon(idx, true); ShowMessage(String("Оружие: ") + WEAPON_DEFS[idx].name); }
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

        // Левая кнопка мыши - стрельба
        if (button == MOUSEB_LEFT) {
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
        
        // Дробины дробовика — отдельными лучами
        const WeaponDef& wdef = WEAPON_DEFS[currentWeapon_];
        for (int p = 0; p < wdef.pellets; ++p) {
            PerformRaycast(wdef.damage, wdef.spread);
        }
        
        // Обновляем UI
        UpdateAmmoDisplay();
    }
    
    void PerformRaycast(float damage, float spread) {
        // Получаем направление из центра экрана
        auto* camera = cameraNode_->GetComponent<Camera>();
        if (!camera) return;
        
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
            player_.shotsHit_++;

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
        SharedPtr<Material> mat = MakeTinted(nullptr, nullptr, Color(255, 230, 150));
        sm->SetMaterial(mat.Get());
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
    
    void CreateImpactEffect(const Vector3& position, const Vector3& normal) {
        // Создаем узел для эффекта
        Node* effectNode = scene_->CreateChild("ImpactEffect");
        effectNode->SetPosition(position);
        effectNode->LookAt(position + normal);
        
        // Эмиттер искр
        ParticleEmitter* emitter = effectNode->CreateComponent<ParticleEmitter>();
        emitter->SetEffect(sparkEffect_);
        
        // Автоудаление через 1 секунду
        { PendingRemove pr; pr.node_ = effectNode; pr.ttl_ = 1.0f; pendingRemoves_.Push(pr); }
    }
    
    void CreateExplosionEffect(const Vector3& position) {
        Node* effectNode = scene_->CreateChild("ExplosionEffect");
        effectNode->SetPosition(position);
        
        // Эмиттер дыма
        ParticleEmitter* smokeEmitter = effectNode->CreateComponent<ParticleEmitter>();
        smokeEmitter->SetEffect(smokeEffect_);
        
        // Точечный свет для вспышки
        Node* lightNode = effectNode->CreateChild("ExplosionLight");
        Light* light = lightNode->CreateComponent<Light>();
        light->SetLightType(LIGHT_POINT);
        light->SetColor(Color(1.0f, 0.5f, 0.0f, 1.0f));
        light->SetRange(10.0f);
        
        // Анимация затухания света
        // (в реальной игре нужна бы анимация)
        
        { PendingRemove pr; pr.node_ = effectNode; pr.ttl_ = 2.0f; pendingRemoves_.Push(pr); }
    }
    
    void PlayMuzzleFlash() {
        if (muzzleLight_) {
            muzzleLightNode_->SetEnabled(true);
            
            // Гасим свет через короткое время
            muzzleLightNode_->SetEnabled(false);
        }
    }
    
    void StartReload() {
        if (weapon_.currentAmmo_ >= GameConstants::MAX_AMMO) {
            ShowMessage("Магазин полон!");
            return;
        }
        
        if (weapon_.reserveAmmo_ <= 0) {
            ShowMessage("Нет запасных патронов!");
            return;
        }
        
        weapon_.isReloading_ = true;
        weapon_.reloadTimer_ = GameConstants::RELOAD_TIME;
        
        PlayReloadSound();
        ShowMessage("Перезарядка...");
    }
    
    void CompleteReload() {
        int needed = GameConstants::MAX_AMMO - weapon_.currentAmmo_;
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
            if (pdist < radius && gameMode_ == MODE_ARENA && !gameOver_) {
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
                String("ТОЧНОСТЬ: ") + String((int)player_.GetAccuracy()) + "%   |   РЕКОРД: " + String(highScore_));
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
            if (rp.x_ <= mp.x_ && mp.x_ < rp.x_ + (int)row->GetWidth() && rp.y_ <= mp.y_ && mp.y_ < rp.y_ + (int)row->GetHeight()) { EquipWeapon((int)i, true); UpdateArmoryHighlight(); return; }
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
            gfxResIdx_    = Clamp(XmlGetInt(gfx, "resolution", gfxResIdx_), 0, 2);
            gfxAnisoIdx_  = Clamp(XmlGetInt(gfx, "anisotropic", gfxAnisoIdx_), 0, 2);
        }
        XMLElement snd = rootEl.GetChild("sound");
        if (snd) {
            //sfxVolume_   = Clamp(String(snd.GetAttribute("sfx")).ToFloat(), 0.0f, 1.0f);
            //musicVolume_ = Clamp(String(snd.GetAttribute("music")).ToFloat(), 0.0f, 1.0f);
            sfxVolume_   = Clamp((float)atof(snd.GetAttribute("sfx").CString()), 0.0f, 1.0f);
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
        XmlSetInt(gfx, "resolution", gfxResIdx_);
        XmlSetInt(gfx, "anisotropic", gfxAnisoIdx_);
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

    void ApplyGraphicsSettings() {
        auto* renderer = GetSubsystem<Renderer>();
        if (!renderer) return;

        static const int shadowSizes[3] = { 1024, 2048, 4096 };
        renderer->SetShadowMapSize((unsigned)shadowSizes[Clamp(gfxShadowIdx_, 0, 2)]);

        switch (gfxGlowIdx_) {
        case 0: renderer->SetHDRRendering(false); break;
        default: renderer->SetHDRRendering(true); break;
        }

        // Разрешение/анизотропия применяются при старте (engine-параметры window width/height,
        // пересоздание окна на лету в этой сборке небезопасно) — здесь остаётся только звук.
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
        if (hudPanelBg_) hudPanelBg_->SetVisible(vis);
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

        for (int i = 0; i < 7; ++i) {
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
        static const char* resNames[3]    = { "нативное", "75%", "50%" };
        static const char* anisoNames[3]  = { "4x", "8x", "16x" };
        const char* names[7] = { "Тени", "HDR Bloom", "Разрешение", "Анизотропная фильтрация", "Громкость эффектов", "Громкость музыки", "Назад" };
        String vals[7];
        vals[0] = shadowNames[Clamp(gfxShadowIdx_, 0, 2)];
        vals[1] = bloomNames[Clamp(gfxGlowIdx_, 0, 2)];
        vals[2] = resNames[Clamp(gfxResIdx_, 0, 2)];
        vals[3] = anisoNames[Clamp(gfxAnisoIdx_, 0, 2)];
        vals[4] = String((int)(sfxVolume_ * 100.0f)) + "%";
        vals[5] = String((int)(musicVolume_ * 100.0f)) + "%";
        vals[6] = "<- в меню";
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
        case 2: gfxResIdx_    = Clamp(gfxResIdx_ + delta, 0, 2); break;
        case 3: gfxAnisoIdx_  = Clamp(gfxAnisoIdx_ + delta, 0, 2); break;
        case 4: sfxVolume_   = Clamp(sfxVolume_ + delta * 0.1f, 0.0f, 1.0f); break;
        case 5: musicVolume_ = Clamp(musicVolume_ + delta * 0.1f, 0.0f, 1.0f); break;
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
        for (unsigned i = 0; i < enemies_.Size(); ++i)
            RemoveColliderLite(enemies_[i].colliderIdx_); // чистим Lite-реестр роботов
        for (unsigned i = 0; i < enemyNodes_.Size(); ++i)
            if (enemyNodes_[i]) enemyNodes_[i]->Remove();
        enemyNodes_.Clear();
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
        ClearEnemies();
        UpdateHealthDisplay();

        if (mode == MODE_ARENA) {
            waveNum_ = 0;
            SpawnWave();
            ShowMessage("РЕЖИМ: АРЕНА - отбивай волны роботов! F - сменить режим");
        } else {
            waveNum_ = 0;
            ShowMessage("РЕЖИМ: ТИР - уничтожь все мишени! F - арена");
        }
        UpdateScoreDisplay();
        ShowMainMenu(false);
    }

    // =========================================================================
    // ВРАГИ-РОБОТЫ: спавн, ИИ, урон
    // =========================================================================

    void EnsureRobotMaterials() {
        if (robotBodyMat_) return;
        robotBodyMat_ = MakeTinted(nullptr, nullptr, Color(150, 155, 165));
        // Глаза роботов: светящиеся — glow-проход + bloom = «злые» глаза в темноте.
        // Цвет глаз задаётся при спавне через SetGlowColor конкретного материала.
        robotEyeMat_  = MakeTinted(nullptr, nullptr, Color(255, 40, 40));
    }

    void SpawnEnemyRobot(const Vector3& pos, EnemyType type) {
        EnsureRobotMaterials();
        const bool elite = (type == E_BOSS);
        Node* rob = scene_->CreateChild("Enemy", LOCAL);
        rob->SetPosition(pos);
        rob->SetVar("enemy", true); rob->AddTag("Enemy");

        // Внешний вид и цвет глаз по типу робота
        Material* bodyMat = robotBodyMat_.Get();
        Color eyeColor(255, 40, 40);
        if (type == E_CHARGER) {
            static SharedPtr<Material> keepOrange;
            if (!keepOrange) keepOrange = MakeTinted(nullptr, nullptr, Color(255, 130, 30));
            bodyMat = keepOrange.Get();
            eyeColor = Color(255, 90, 20);
        } else if (type == E_SHOOTER) {
            static SharedPtr<Material> keepViolet;
            if (!keepViolet) keepViolet = MakeTinted(nullptr, nullptr, Color(160, 70, 220));
            bodyMat = keepViolet.Get();
            eyeColor = Color(220, 120, 255);
        } else if (elite) {
            static SharedPtr<Material> keepCyan;
            if (!keepCyan) keepCyan = MakeTinted(nullptr, nullptr, Color(40, 220, 255));
            bodyMat = keepCyan.Get();
            eyeColor = Color(255, 200, 40);
        }

        // Масштаб модели: босс крупнее, стрелок чуть «суше»
        float scale = (type == E_BOSS) ? 1.8f : (type == E_SHOOTER ? 0.9f : 1.0f);
        rob->SetScale(Vector3::ONE * scale);

        StaticModel* torso = rob->CreateComponent<StaticModel>();
        torso->SetModel(boxModel_);
        torso->SetMaterial(bodyMat);
        torso->SetCastShadows(true);

        Node* head = rob->CreateChild("Head");
        head->SetPosition(Vector3(0, 0.95f, 0));
        head->SetScale(Vector3(0.5f, 0.45f, 0.5f));
        StaticModel* hm = head->CreateComponent<StaticModel>();
        hm->SetModel(boxModel_);
        hm->SetMaterial(bodyMat);
        hm->SetCastShadows(true);

        static Vector<SharedPtr<Material>> eyeMats;
        SharedPtr<Material> eyeMat = MakeTinted(nullptr, nullptr, eyeColor);
        // Свечение глаз по типу: яркие ядовитые цвета читаются в темноте даже без bloom-пасса
        if (eyeMat) {
            Color glow(1.0f, 0.25f, 0.15f);
            if (type == E_CHARGER)      glow = Color(1.0f, 0.55f, 0.05f);
            else if (type == E_SHOOTER) glow = Color(0.85f, 0.3f, 1.0f);
            else if (elite)             glow = Color(1.0f, 0.8f, 0.15f);
            eyeMat->SetShaderParameter("MatDiffColor", Variant(glow));
        }
        eyeMats.Push(eyeMat);
        Material* em = eyeMats.Back().Get();
        Node* eyeL = head->CreateChild("EyeL");
        eyeL->SetPosition(Vector3(-0.12f, 0.05f, 0.26f));
        eyeL->SetScale(Vector3(0.1f, 0.08f, 0.03f));
        StaticModel* elm = eyeL->CreateComponent<StaticModel>();
        elm->SetModel(boxModel_); elm->SetMaterial(em);
        Node* eyeR = head->CreateChild("EyeR");
        eyeR->SetPosition(Vector3(0.12f, 0.05f, 0.26f));
        eyeR->SetScale(Vector3(0.1f, 0.08f, 0.03f));
        StaticModel* erm = eyeR->CreateComponent<StaticModel>();
        erm->SetModel(boxModel_); erm->SetMaterial(em);

        // Lite-коллайдер робота: tag 2, ptr = узел (рейкасты пуль и LOS проходят через него)
        int colIdx = AddBoxCollider(pos + Vector3(0, 0.9f, 0), Vector3(0.4f, 0.9f, 0.3f), 2, rob);

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
        enemies_.Push(e);
        enemyNodes_.Push(SharedPtr<Node>(rob));
    }

    // Выбор типа робота для спавна с учётом номера волны
    EnemyType PickEnemyType(int slot, int total) {
        if (waveNum_ >= 5 && slot == 0 && (waveNum_ % 5 == 0)) return E_BOSS;      // босс каждые 5 волн
        if (waveNum_ >= 2) {                                                        // стрелки со 2-й волны
            int roll = rand() % 100;
            if (roll < 25) return E_SHOOTER;
            if (roll < 50) return E_CHARGER;
        } else if (waveNum_ >= 3 && (rand() % 100) < 20) {
            return E_CHARGER;                                                       // чарджеры с 3-й волны в тире-миксе
        }
        return E_MELEE;
    }

    void SpawnWave() {
        ++waveNum_;
        const bool bossWave = (waveNum_ % 5 == 0) && waveNum_ >= 5;
        int count = Min(3 + waveNum_, 10);
        const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f - 6.0f;
        for (int i = 0; i < count; ++i) {
            float ang = (float)(rand() % 360) * 0.0174533f;
            float r = 10.0f + (float)(rand() % 30);
            Vector3 pos(cosf(ang) * r, 1.0f, sinf(ang) * r);
            pos.x_ = Clamp(pos.x_, -half, half);
            pos.z_ = Clamp(pos.z_, -half, half);
            EnemyType t = PickEnemyType(i, count);
            SpawnEnemyRobot(pos, t);
        }
        String msg = "ВОЛНА " + String(waveNum_) + "! Роботов: " + String(count);
        if (bossWave) msg += "  [БОСС!]";
        ShowMessage(msg);
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
        SharedPtr<Material> mat = MakeTinted(nullptr, nullptr, Color(255, 80, 255));
        sm->SetMaterial(mat.Get());
        sm->SetCastShadows(false);
        pr->SetScale(Vector3::ONE * 0.12f);

        ProjectileData pd;
        pd.node_ = pr;
        pd.vel_ = dir * 26.0f; // скорость заметно ниже пули — игрок успевает увернуться
        pd.ttl_ = 3.0f;
        pd.damage_ = damage;
        enemyProjectiles_.Push(pd);

        // Короткий «звук» выстрела робота — тот же UI-клик на его источнике
        PlayHitSound();
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
                if (p.node_) p.node_->Remove();
                enemyProjectiles_.Erase(i);
            }
        }
    }

    // Движение «летучих» мишеней тира без Bullet: простая интеграция + отскок от границ
    void UpdateMovingTargetsLite(float dt) {
        const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f - 3.0f;
        for (unsigned i = 0; i < movingTargetsLite_.Size(); ++i) {
            MovingTargetLite& mt = movingTargetsLite_[i];
            if (!mt.node_) continue;
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

            // Интеграция движения с ограничением по арене
            Vector3 np = p + move * dt;
            const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f - 2.0f;
            np.x_ = Clamp(np.x_, -half, half);
            np.z_ = Clamp(np.z_, -half, half);
            float bob = 0.05f * sinf(timeAcc_ * 3.0f + e.phase_);
            // Визуальный «откат» при выстреле/ударе: приседание модели
            if (e.recoilKick_ > 0.0f) e.recoilKick_ = Max(0.0f, e.recoilKick_ - dt);
            float kick = e.recoilKick_ * 0.6f;
            Vector3 newPos(np.x_, 1.0f + bob - kick, np.z_);
            n->SetPosition(newPos);
            MoveColliderLite(e.colliderIdx_, newPos + Vector3(0, -0.1f, 0)); // рейкасты пуль идут за роботом

            if (dist > 0.5f) {
                float yawDeg = atan2f(toCam.x_, toCam.z_) * 57.29578f;
                n->SetRotation(Quaternion(yawDeg, Vector3::UP));
            }
        }
    }

    void DamageEnemy(EnemyData& e, float damage) {
        if (!e.alive_) return;
        e.health_ -= damage;
        Node* n = e.node_.Get();
        if (n) {
            CreateHitSpark(n->GetPosition() + Vector3(0, 0.5f, 0));
            PlayHitSound();
            // Визуальный «откид» без физики: короткий наклон модели робота
            e.recoilKick_ = 0.35f;
        }
        if (e.health_ <= 0.0f) {
            e.alive_ = false;
            if (n) {
                CreateExplosionEffect(n->GetPosition());
                CreateDebrisBurst(n->GetPosition());
                PlayExplosionAt(n->GetPosition());
                n->Remove();
            }
            AddScoreWithCombo(e.scoreValue_);
            player_.kills_++;
            UpdateScoreDisplay();
            ShowMessage("РОБОТ УНИЧТОЖЕН! +" + String(e.scoreValue_) +
                        (comboCount_ > 1 ? (" (COMBO x" + String(comboCount_) + ")") : ""));
        }
    }

    // =========================================================================
    // ЧАСТИЦЫ: осколки и искры
    // =========================================================================

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

        if (sparkEffect_) {
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
        if (!sparkEffect_) return;
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
        if (player_.score_ > highScore_) {
            highScore_ = player_.score_;
            SaveSettingsFile();
        }
    }
};

// ============================================================================
// Макрос регистрации приложения
// ============================================================================

URHO3D_DEFINE_APPLICATION_MAIN(MyApp)

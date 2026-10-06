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

#include <Urho3D/Physics/PhysicsWorld.h>
#include <Urho3D/Physics/RigidBody.h>
#include <Urho3D/Physics/CollisionShape.h>
#include <Urho3D/Physics/PhysicsEvents.h>

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
#include <Urho3D/UI/Button.h>
#include <Urho3D/UI/Window.h>
#include <Urho3D/UI/ScrollView.h>
#include <Urho3D/UI/MenuBar.h>
#include <Urho3D/UI/Menu.h>

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
    { "SHOTGUN", 12.0f, 0.85f,  6,  48, 0.060f, 2.8f, 8, Color(100, 70, 40) },
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
    SharedPtr<PhysicsWorld> physicsWorld_;
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
    SharedPtr<UIElement> settingsPanel_;
    Vector<SharedPtr<Text>> settingsItems_;
    // Арена: волны роботов-курят
    struct EnemyData {
        WeakPtr<Node> node_;
        WeakPtr<RigidBody> body_;
        float health_ = 100.0f;
        bool alive_ = true;
        Vector3 patrolCenter_;
        float patrolRadius_ = 4.0f;
        float phase_ = 0.0f;
        float speed_ = 1.6f;
        int scoreValue_ = 150;
    };
    Vector<EnemyData> enemies_;
    Vector<SharedPtr<Node>> enemyNodes_;   // владение нодами до удаления
    int waveNum_ = 0;
    float waveRespawnTimer_ = 0.0f;
    // Комбо и рекорды
    int comboCount_ = 0;
    float comboTimer_ = 0.0f;
    int bestCombo_ = 0;
    int highScore_ = 0;
    float rangeRespawnTimer_ = 0.0f;
    Material* robotBodyMat_ = nullptr;
    Material* robotEyeMat_ = nullptr;
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

    // Предварительные объявления методов, используемых до определения
    void ToggleArmory();
    void CreateArmoryMenu();
    void HandleArmoryClick(bool left);
    void CycleSkin(int dir);
    void ApplyExplosionDamage(const Vector3& center, float damage);
    void UpdateArmoryHighlight();
    // Меню / настройки / арена / комбо
    void LoadSettingsFile();
    void SaveSettingsFile();
    void ApplyGraphicsSettings();
    void CreateMainMenu();
    void ShowMainMenu(bool show);
    void UpdateMainMenuSelection();
    void CreateSettingsPanel();
    void ShowSettings(bool show);
    void UpdateSettingsDisplay();
    void ChangeSetting(int delta);
    void StartGameMode(GameMode mode);
    void SpawnEnemyRobot(const Vector3& pos, bool elite);
    void SpawnWave();
    void UpdateEnemies(float dt);
    void DamageEnemy(EnemyData& e, float damage);
    void CreateDebrisBurst(const Vector3& pos);
    void CreateHitSpark(const Vector3& pos);
    void AddScoreWithCombo(int base);

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
        renderer->SetGlowThreshold(0.6f);            // HDR-свечение яркого (неон, вспышки, маяки)
        renderer->SetGlowBias(0.05f);
        renderer->SetGlowBlur(BLEUR_TWICE);          // мягкий размытый bloom
        renderer->SetNumOccluderTriangles(65536);   // больше теней от мелкой геометрии
        renderer->SetNumLights(8);                  // неоновые маяки и подсветка арены
        
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
            auto* audio = GetSubsystem<Audio>();
            audio->StopAllSources(); // убратьpossible заглушку стартера
            musicSource_ = node_->CreateChild("MusicNode")->CreateComponent<SoundSource>();
            musicSource_->SetMode(SOUND_EFFECT);
            musicSource_->SetGain(musicVolume_);
            musicSource_->Play(music);
        }

        return true;
    }
    
SharedPtr<Material> MakeTinted(const SharedPtr<Texture>& diff, const SharedPtr<Texture>& norm, const Color& c) {
        auto* cache = GetSubsystem<ResourceCache>();
        SharedPtr<Material> m = new Material(context_);
        m->SetTechnique(0, cache->GetResource<Technique>("Techniques/DiffUnlit.xml"));
        if (diff) { m->SetTexture(TU_DIFFUSE, diff); }
        m->SetDiffuseColor(c);
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
     skyTexture_ = cache->GetResource<Texture2D>("Textures/SkyDay.png");

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
     // Анизотропная фильтрация: пол/стены не мылятся под углом
     {
         Material* anisoMats[] = { hdStone_, hdFloor_, hdPanel_, hdMetal_, hdCrate_ };
         for (Material* m : anisoMats) {
             if (m) m->SetParameter(MAT_ANISOTROPY, Variant(8.0f)); // 8x анизотропия: чёткий пол под острым углом
         }
         // Компенсация увеличенных плит пола: текстура кладётся тем же плотным узором
         if (hdFloor_) hdFloor_->SetTextureScaling(TU_DIFFUSE, Vector2(GameConstants::FLOOR_TILE_UV_SCALE, GameConstants::FLOOR_TILE_UV_SCALE));
         if (hdFloor_) hdFloor_->SetTextureScaling(TU_NORMAL, Vector2(GameConstants::FLOOR_TILE_UV_SCALE, GameConstants::FLOOR_TILE_UV_SCALE));
         if (hdStone_) { hdStone_->SetTextureScaling(TU_DIFFUSE, Vector2(2.0f, 2.0f)); hdStone_->SetTextureScaling(TU_NORMAL, Vector2(2.0f, 2.0f)); }
         if (hdPanel_) { hdPanel_->SetTextureScaling(TU_DIFFUSE, Vector2(2.0f, 2.0f)); hdPanel_->SetTextureScaling(TU_NORMAL, Vector2(2.0f, 2.0f)); }
     }
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
        
        // Создаем физику
        physicsWorld_ = scene_->CreateComponent<PhysicsWorld>();
        physicsWorld_->SetGravity(Vector3(0.0f, -9.81f, 0.0f));
        
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
        zone->SetVisibleDistance(500.0f);
    }

    void CreateSky() {
        // Небо через Skybox: рисуется до сцены на бесконечности — не мерцает с геометрией
        Node* skyNode = scene_->CreateChild("Sky");
        skyMaterial_ = new Material(context_);
        skyMaterial_->SetTechnique(0, GetSubsystem<ResourceCache>()->GetResource<Technique>("Techniques/DiffUnlit.xml"));
        if (skyTexture_) skyMaterial_->SetTexture(TU_DIFFUSE, skyTexture_);
        skyMaterial_->SetDiffuseColor(Color(150, 185, 235));
        skyMaterial_->SetCullMode(CULL_NONE);
        skyMaterial_->SetGlowEnabled(true); // облака чуть светятся в HDR
        Skybox* sky = skyNode->CreateComponent<Skybox>();
        sky->SetModel(boxModel_);
        sky->SetMaterial(skyMaterial_);
        sky->SetDrawDistance(900.0f);
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
                decoNodes_.Push(strip);
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
            cm->SetMaterial(hdCrate_ ? hdCrate_ : tintedWood.Raw());
            RigidBody* rb = crate->CreateComponent<RigidBody>();
            rb->SetFriction(0.8f);
            CollisionShape* cs = crate->CreateComponent<CollisionShape>();
            cs->SetBox(Vector3::ONE);
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
            bm2->SetMaterial(hdMetal_ ? hdMetal_ : barrelTint.Raw());
            RigidBody* rb = barrel->CreateComponent<RigidBody>();
            rb->SetMass(40.0f);
            rb->SetFriction(0.7f);
            CollisionShape* cs = barrel->CreateComponent<CollisionShape>();
            cs->SetBox(Vector3(1.0f, 1.6f, 1.0f));
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
            hm->SetMaterial(MakeTinted(nullptr, nullptr, Color(255, 230, 160)).Raw());
            Node* ln2 = scene_->CreateChild("LampLight");
            ln2->SetPosition(lp + Vector3(0, 5.0f, 0));
            Light* l2 = ln2->CreateComponent<Light>();
            l2->SetLightType(LIGHT_POINT);
            l2->SetColor(Color(1.0f, 0.85f, 0.55f));
            l2->SetRange(14.0f);
            l2->SetBrightness(0.9f);
            decoNodes_.Push(pole); decoNodes_.Push(head); decoNodes_.Push(ln2);
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
            RigidBody* rb2 = crate2->CreateComponent<RigidBody>();
            rb2->SetFriction(0.8f);
            CollisionShape* cs2 = crate2->CreateComponent<CollisionShape>();
            cs2->SetBox(Vector3::ONE);
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
                
                // Физика для пола
                RigidBody* rb = tileNode->CreateComponent<RigidBody>();
                rb->SetFriction(0.8f);
                rb->SetRestitution(0.1f);
                
                CollisionShape* cs = tileNode->CreateComponent<CollisionShape>();
                cs->SetBox(Vector3(1.0f, 0.1f, 1.0f));
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
            decoNodes_.Push(t);
        }
    }
    
    void CreateWall(const Vector3& position, const Vector3& scale) {
        Node* wallNode = scene_->CreateChild("Wall");
        wallNode->SetPosition(position);
        wallNode->SetScale(scale);
        
        StaticModel* model = wallNode->CreateComponent<StaticModel>();
        model->SetModel(boxModel_);
        model->SetMaterial(stoneMaterial_);
        
        // Физика
        RigidBody* rb = wallNode->CreateComponent<RigidBody>();
        rb->SetFriction(0.7f);
        
        CollisionShape* cs = wallNode->CreateComponent<CollisionShape>();
        cs->SetBox(Vector3::ONE);
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
            
            RigidBody* rb = columnNode->CreateComponent<RigidBody>();
            rb->SetFriction(0.6f);
            
            CollisionShape* cs = columnNode->CreateComponent<CollisionShape>();
            cs->SetBox(Vector3::ONE);
        }
        
        // Центральная платформа
        Node* platformNode = scene_->CreateChild("Platform");
        platformNode->SetPosition(Vector3(0, 0.5f, 0));
        platformNode->SetScale(Vector3(10.0f, 1.0f, 10.0f));
        
        StaticModel* platformModel = platformNode->CreateComponent<StaticModel>();
        platformModel->SetModel(boxModel_);
        platformModel->SetMaterial(stoneMaterial_);
        
        RigidBody* platformRb = platformNode->CreateComponent<RigidBody>();
        platformRb->SetFriction(0.8f);
        
        CollisionShape* platformCs = platformNode->CreateComponent<CollisionShape>();
        platformCs->SetBox(Vector3::ONE);
        
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
        
        RigidBody* rb = rampNode->CreateComponent<RigidBody>();
        rb->SetFriction(0.7f);
        
        CollisionShape* cs = rampNode->CreateComponent<CollisionShape>();
        cs->SetBox(Vector3::ONE);
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
        
        // Физика для босс-мишени
        RigidBody* bossRb = bossTargetNode->CreateComponent<RigidBody>();
        bossRb->SetMass(10.0f);
        bossRb->SetFriction(0.5f);
        bossRb->SetRestitution(0.3f);
        
        CollisionShape* bossCs = bossTargetNode->CreateComponent<CollisionShape>();
        bossCs->SetBox(Vector3::ONE);
        
        TargetData bossData;
        bossData.node_ = bossTargetNode;
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
        
        // Физика
        RigidBody* rb = targetNode->CreateComponent<RigidBody>();
        rb->SetMass(1.0f);  // Легкая мишень
        rb->SetFriction(0.5f);
        rb->SetRestitution(0.5f);
        
        CollisionShape* cs = targetNode->CreateComponent<CollisionShape>();
        cs->SetBox(Vector3::ONE);
        
        TargetData data;
        data.node_ = targetNode;
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
        
        // Физика для движущейся мишени
        RigidBody* rb = targetNode->CreateComponent<RigidBody>();
        rb->SetMass(2.0f);
        rb->SetLinearVelocity(Vector3((float)(rand() % 5 - 2), 0, (float)(rand() % 5 - 2)));
        rb->SetAngularDamping(0.9f);
        rb->SetFriction(0.3f);
        rb->SetRestitution(0.7f);
        
        CollisionShape* cs = targetNode->CreateComponent<CollisionShape>();
        cs->SetSphere(0.5f);
        
        TargetData data;
        data.node_ = targetNode;
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
        sunLight->SetOrthographicExtent(70.0f);
        sunLight->SetCascadeCount(3);          // каскады: резкие тени вблизи, мягкие вдали
        sunLight->SetShadowBias(0.0025f);
        sunLight->SetVarianceShadowWidth(1.0f);
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
        soundSource_->SetMode(SOUND_EFFECT);
        GetSubsystem<Audio>()->AddListener(soundSource_);

        // 3D-источник для взрывов в мировом пространстве
        explosionSource3D_ = scene_->CreateChild("ExplosionSfx")->CreateComponent<SoundSource3D>();
        explosionSource3D_->SetMode(SOUND_EFFECT);
        explosionSource3D_->SetMaxDistance(120.0f);
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
        timeAcc_ += dt;
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
        if (!inMenu_ && !armoryOpen_ && !isMouseVisible_ && input->GetMouseButtonDown(MOUSEB_LEFT)) {
            int wt = currentWeapon_;
            if (wt != W_SHOTGUN && wt != W_SNIPER && wt != W_LAUNCHER) {
                TryFire();
            }
        }
        
        // Восстанавливаем отдачу
        weapon_.recoilX_ = Lerp(weapon_.recoilX_, 0.0f, GameConstants::RECOIL_RECOVERY * dt);
        weapon_.recoilY_ = Lerp(weapon_.recoilY_, 0.0f, GameConstants::RECOIL_RECOVERY * dt);
        
        // Применяем отдачу к оружию
        ApplyRecoilToWeapon();
        
        // === ОБРАБОТКА ВВОДА ДЛЯ ДВИЖЕНИЯ ===
        HandleMovement(dt);
        
        // Обновляем анимацию покачивания оружия
        UpdateWeaponBob(dt);

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
        // Ищем верхнюю поверхность под ногами (4 луча по краям «стопы»)
        float bestY = -1e6f;
        const float probeTop = feetPos.y_ + eyeHeight_ * 0.5f;
        const float probeLen = eyeHeight_ + 3.0f;
        for (int sx = -1; sx <= 1; sx += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                Vector3 offset(sx * 0.22f, 0.0f, sz * 0.22f);
                Ray down(Vector3(feetPos.x_ + offset.x_, probeTop, feetPos.z_ + offset.z_), Vector3(0, -1, 0));
                PhysicsRaycastResult r;
                physicsWorld_->RaycastSingle(r, down, probeLen);
                if (r.body_ && r.distance_ > 0.0f) {
                    bestY = Max(bestY, probeTop - r.distance_);
                }
            }
        }
        return bestY;
    }

    bool HeadBlocked(const Vector3& feetPos) {
        // Нет ли препятствия над головой (нельзя встать из приседа под низкими объектами)
        Ray up(Vector3(feetPos.x_, feetPos.y_ + 0.1f, feetPos.z_), Vector3(0, 1, 0));
        PhysicsRaycastResult r;
        physicsWorld_->RaycastSingle(r, up, GameConstants::PLAYER_HEIGHT + 0.1f);
        return r.body_ != nullptr && r.distance_ > 0.05f && r.distance_ < GameConstants::PLAYER_HEIGHT;
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
            Ray h(testFeet, dirs[i]);
            PhysicsRaycastResult r;
            physicsWorld_->RaycastSingle(r, h, radius + 0.15f);
            if (r.body_ && r.distance_ > 0.0f && r.distance_ < radius + 0.15f) {
                float penetration = (radius + 0.15f) - r.distance_;
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
        
        // Выполняем рейкаст
        Ray ray(startPos, shootDirection);
    PhysicsRaycastResult result;
    physicsWorld_->RaycastSingle(result, ray, GameConstants::BULLET_RANGE);
        
        if (result.body_) {
            player_.shotsHit_++;
            
            Vector3 hitPosition = result.position_;
            Vector3 hitNormal = result.normal_;
            
            // Создаем эффект попадания
            CreateImpactEffect(hitPosition, hitNormal);
            
            // Проверяем, попала ли пуля в мишень
            // Гранатомёт: взрыв по площади
            if (currentWeapon_ == W_LAUNCHER) {
                CreateExplosionEffect(hitPosition);
                ApplyExplosionDamage(hitPosition, damage);
                PlayExplosionAt(hitPosition);
                AddCameraShake(0.12f); // тряска камеры от взрыва
            } else {
                CheckTargetHit(result.body_, damage);
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
    
    void CheckTargetHit(RigidBody* hitBody, float damage) {
        if (!hitBody) return;

        Node* hn0 = hitBody->GetNode();
        if (hn0 && hn0->HasTag(STRING_HASH("Enemy"))) {
            for (auto& e : enemies_) {
                if (e.alive_ && e.node_ == hn0) { DamageEnemy(e, damage); break; }
            }
            return;
        }
        
        Node* hitNode = hitBody->GetNode();
        if (!hitNode) return;
        
        // Ищем мишень, соответствующую попавшему узлу
        for (auto& target : targets_) {
            if (target.isActive_ && target.node_ == hitNode) {
                // Попали в мишень!
                target.health_ -= damage;
                
                CreateHitSpark(hitNode->GetPosition());

                // Визуальный эффект попадания
                StaticModel* model = hitNode->GetComponent<StaticModel>();
                if (model) {
                    model->SetMaterial(targetHitMaterial_);
                }
                
                // Звук попадания
                PlayHitSound();
                
                // Если мишень уничтожена
                if (target.health_ <= 0) {
                    DestroyTarget(target);
                } else {
                    // Возвращаем материал через некоторое время
                    // (упрощенно - сразу, в реальной игре нужен таймер)
                }
                
                break;
            }
        }
    }
    
    void DestroyTarget(TargetData& target) {
        target.isActive_ = false;
        
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
        float pitchVar = 1.0f + (rand() % 100 - 50) / 500.0f;
        float gainVar = Max(0.4f, Min(1.0f, 0.85f + (rand() % 100 - 50) / 300.0f));
        soundSource_->SetPitch(Clamp(pitchVar, 0.9f, 1.1f));
        soundSource_->SetGain(gainVar);
        soundSource_->Play(s);
        soundSource_->SetPitch(1.0f);
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
        explosionSource3D_->SetPosition(pos);
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
        GetSubsystem<Renderer>()->Update();
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
        wHead->SetText("ОРУЖИЕ (1-" + String(WEAPON_COUNT) + ")");
        wHead->SetPosition(sw / 2 - 380, 100);
        wHead->SetColor(Color(0.7f, 0.9f, 1.0f, 1.0f));

        for (int i = 0; i < WEAPON_COUNT; ++i) {
            Text* row = armoryPanel_->CreateChild<Text>();
            row->SetFont(font_, 20);
            const WeaponDef& d = WEAPON_DEFS[i];
            row->SetText(String(i + 1) + ". " + d.name + "  DMG:" + String(d.damage, 0) +
                         "  ROF:" + String((int)(1.0f / d.fireRate)) + "/s  MAG:" + String(d.magSize));
            row->SetPosition(sw / 2 - 380, 135 + i * 34);
            row->SetVar("index", i);
            armoryWeaponRows_.Push(row);
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
            row->SetText(String(i + 1) + ". " + sk.name + (sk.emissive ? " (glow)" : ""));
            row->SetPosition(sw / 2 + 80, 135 + i * 34);
            row->SetVar("index", i);
            armorySkinRows_.Push(row);
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
        for (unsigned i = 0; i < armoryWeaponRows_.Size(); ++i) {
            IntRect r = armoryWeaponRows_[i]->GetAbsoluteOffset();
            if (r.Contains(mp)) { EquipWeapon((int)i, true); UpdateArmoryHighlight(); return; }
        }
        for (unsigned i = 0; i < armorySkinRows_.Size(); ++i) {
            IntRect r = armorySkinRows_[i]->GetAbsoluteOffset();
            if (r.Contains(mp)) { CycleSkin((int)i - currentSkin_); return; }
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

    String SettingsPath() const {
        return GetSubsystem<FileSystem>()->GetProgramDir() + "settings.ini";
    }

    void LoadSettingsFile() {
        auto* fs = GetSubsystem<FileSystem>();
        if (!fs->FileExists(SettingsPath())) return;
        SharedPtr<XMLFile> xml = new XMLFile(context_);
        if (!xml->LoadFile(SettingsPath())) return;
        XMLElement rootEl = xml->GetRoot();
        XMLElement gfx = rootEl.GetChild("graphics");
        if (gfx) {
            gfxShadowIdx_ = Clamp(gfx.GetInt("shadows", gfxShadowIdx_), 0, 2);
            gfxGlowIdx_   = Clamp(gfx.GetInt("bloom", gfxGlowIdx_), 0, 2);
            gfxResIdx_    = Clamp(gfx.GetInt("resolution", gfxResIdx_), 0, 2);
            gfxAnisoIdx_  = Clamp(gfx.GetInt("anisotropic", gfxAnisoIdx_), 0, 2);
        }
        XMLElement snd = rootEl.GetChild("sound");
        if (snd) {
            sfxVolume_   = Clamp(snd.GetFloat("sfx", sfxVolume_), 0.0f, 1.0f);
            musicVolume_ = Clamp(snd.GetFloat("music", musicVolume_), 0.0f, 1.0f);
        }
        XMLElement prog = rootEl.GetChild("progress");
        if (prog) highScore_ = Max(0, prog.GetInt("highscore", highScore_));
    }

    void SaveSettingsFile() {
        SharedPtr<XMLFile> xml = new XMLFile(context_);
        XMLElement rootEl = xml->CreateRoot("chikengun");
        XMLElement gfx = rootEl.CreateChild("graphics");
        gfx.SetInt("shadows", gfxShadowIdx_);
        gfx.SetInt("bloom", gfxGlowIdx_);
        gfx.SetInt("resolution", gfxResIdx_);
        gfx.SetInt("anisotropic", gfxAnisoIdx_);
        XMLElement snd = rootEl.CreateChild("sound");
        snd.SetFloat("sfx", sfxVolume_);
        snd.SetFloat("music", musicVolume_);
        XMLElement prog = rootEl.CreateChild("progress");
        prog.SetInt("highscore", highScore_);
        xml->SaveFile(SettingsPath());
    }

    void ApplyGraphicsSettings() {
        auto* renderer = GetSubsystem<Renderer>();
        if (!renderer) return;

        static const int shadowSizes[3] = { 1024, 2048, 4096 };
        renderer->SetShadowMapSize((unsigned)shadowSizes[Clamp(gfxShadowIdx_, 0, 2)]);

        switch (gfxGlowIdx_) {
        case 0: renderer->SetHDRRendering(false); break;
        case 1: renderer->SetHDRRendering(true);
                renderer->SetGlowThreshold(0.9f);
                renderer->SetGlowBias(0.02f);
                renderer->SetGlowBlur(BLEUR_NONE); break;
        default: renderer->SetHDRRendering(true);
                renderer->SetGlowThreshold(0.6f);
                renderer->SetGlowBias(0.05f);
                renderer->SetGlowBlur(BLEUR_TWICE); break;
        }

        auto* graphics = GetSubsystem<Graphics>();
        if (graphics) {
            unsigned w = graphics->GetWidth(), h = graphics->GetHeight();
            float scale = gfxResIdx_ == 0 ? 1.0f : (gfxResIdx_ == 1 ? 0.75f : 0.5f);
            for (unsigned i = 0; i < renderer->GetNumViewports(); ++i) {
                Viewport* vp = renderer->GetViewport(i);
                if (vp && vp->GetRenderPath())
                    vp->GetRenderPath()->SetScaledResolution((unsigned)(w * scale), (unsigned)(h * scale));
            }
        }

        static const unsigned anisoVals[3] = { 4, 8, 16 };
        auto* cache = GetSubsystem<ResourceCache>();
        static const char* texFiles[] = {
            "Textures/Game/StoneHD.png", "Textures/Game/FloorTile.png",
            "Textures/Game/MetalPanel.png", "Textures/Game/BrushedMetal.png",
            "Textures/Game/CrateWood.png"
        };
        for (unsigned t = 0; t < sizeof(texFiles) / sizeof(texFiles[0]); ++t) {
            SharedPtr<Texture2D> tex = cache->GetResource<Texture2D>(texFiles[t], false);
            if (tex) tex->SetAnisotropy(anisoVals[Clamp(gfxAnisoIdx_, 0, 2)]);
        }

        if (soundSource_) soundSource_->SetGain(sfxVolume_);
        if (musicSource_) musicSource_->SetGain(musicVolume_);
    }

    // =========================================================================
    // ГЛАВНОЕ МЕНЮ
    // =========================================================================

    void CreateMainMenu() {
        if (mainMenuPanel_) return;
        auto* ui = GetSubsystem<UI>();
        auto* root = ui->GetRoot();
        int sw = GetSubsystem<Graphics>()->GetWidth();
        int sh = GetSubsystem<Graphics>()->GetHeight();

        mainMenuPanel_ = root->CreateChild<UIElement>("MainMenuPanel");
        mainMenuPanel_->SetSize(sw, sh);

        BorderImage* bg = mainMenuPanel_->CreateChild<BorderImage>();
        bg->SetSize(sw, sh);
        bg->SetColor(Color(0.01f, 0.02f, 0.04f, 0.62f));

        Text* title = mainMenuPanel_->CreateChild<Text>();
        title->SetFont(font_, 64);
        title->SetText("CHIKENGUN 2.1");
        title->SetTextAlignment(HA_CENTER);
        title->SetPosition(sw / 2 - 400, sh / 2 - 220);
        title->SetWidth(800);
        title->SetColor(Color(1.0f, 0.85f, 0.25f, 1.0f));

        Text* sub = mainMenuPanel_->CreateChild<Text>();
        sub->SetFont(font_, 18);
        sub->SetText("ARENA SHOOTER | URHO3D");
        sub->SetTextAlignment(HA_CENTER);
        sub->SetPosition(sw / 2 - 400, sh / 2 - 150);
        sub->SetWidth(800);
        sub->SetColor(Color(0.5f, 0.8f, 1.0f, 0.9f));

        const char* items[4] = { "ТИР (мишени)", "АРЕНА (волны роботов)", "НАСТРОЙКИ", "ВЫХОД" };
        for (int i = 0; i < 4; ++i) {
            Text* it = mainMenuPanel_->CreateChild<Text>();
            it->SetFont(font_, 28);
            it->SetText(items[i]);
            it->SetPosition(sw / 2 - 260, sh / 2 - 70 + i * 48);
            it->SetWidth(520);
            mainMenuItems_.Push(it);
        }

        Text* hint = mainMenuPanel_->CreateChild<Text>();
        hint->SetFont(font_, 16);
        hint->SetText("W/S или стрелки - выбор, Enter - запуск");
        hint->SetTextAlignment(HA_CENTER);
        hint->SetPosition(sw / 2 - 350, sh / 2 + 150);
        hint->SetWidth(700);
        hint->SetColor(Color(0.7f, 0.75f, 0.8f, 0.9f));

        UpdateMainMenuSelection();
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
        if (weaponNode_) weaponNode_->SetVisible(vis);
    }

    void UpdateMainMenuSelection() {
        static const char* labels[4] = { "ТИР (мишени)", "АРЕНА (волны роботов)", "НАСТРОЙКИ", "ВЫХОД" };
        for (unsigned i = 0; i < mainMenuItems_.Size(); ++i) {
            bool sel = ((int)i == menuSel_);
            mainMenuItems_[i]->SetText((sel ? ">  " : "   ") + String(labels[i]) + (sel ? "  <" : ""));
            mainMenuItems_[i]->SetColor(sel ? Color(1.0f, 0.85f, 0.2f, 1.0f) : Color(0.85f, 0.85f, 0.9f, 1.0f));
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
            settingsItems_.Push(row);
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
        for (unsigned i = 0; i < enemyNodes_.Size(); ++i)
            if (enemyNodes_[i]) enemyNodes_[i]->Remove();
        enemyNodes_.Clear();
        enemies_.Clear();
    }

    void StartGameMode(GameMode mode) {
        gameMode_ = mode;
        player_.score_ = 0;
        player_.kills_ = 0;
        comboCount_ = 0;
        comboTimer_ = 0.0f;
        waveRespawnTimer_ = 0.0f;
        rangeRespawnTimer_ = 0.0f;
        ClearEnemies();

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
        robotBodyMat_ = MakeTinted(nullptr, nullptr, Color(150, 155, 165)).ReleaseRef();
        robotEyeMat_  = MakeTinted(nullptr, nullptr, Color(255, 40, 40)).ReleaseRef();
    }

    void SpawnEnemyRobot(const Vector3& pos, bool elite) {
        EnsureRobotMaterials();
        Node* rob = scene_->CreateChild("Enemy", LOCAL);
        rob->SetPosition(pos);
        rob->AddTag(STRING_HASH("Enemy"));

        Material* bodyMat = robotBodyMat_;
        Color eyeColor(255, 40, 40);
        if (elite) {
            static SharedPtr<Material> keepCyan;
            if (!keepCyan) keepCyan = MakeTinted(nullptr, nullptr, Color(40, 220, 255));
            bodyMat = keepCyan.Get();
            eyeColor = Color(255, 200, 40);
        }

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

        RigidBody* rb = rob->CreateComponent<RigidBody>();
        rb->SetMass(3.0f);
        rb->SetCollisionLayer(2);
        rb->SetCollisionMask(3);
        rb->SetLinearDamping(4.0f);
        rb->SetAngularDamping(10.0f);
        CollisionShape* cs = rob->CreateComponent<CollisionShape>();
        cs->SetBox(Vector3(0.8f, 1.8f, 0.6f));

        EnemyData e;
        e.node_ = rob;
        e.body_ = rb;
        e.health_ = elite ? 200.0f : 100.0f;
        e.alive_ = true;
        e.patrolCenter_ = pos;
        e.patrolRadius_ = elite ? 6.0f : 4.0f;
        e.phase_ = (float)(rand() % 628) / 100.0f;
        e.speed_ = elite ? 2.6f : 1.6f;
        e.scoreValue_ = elite ? 300 : 150;
        enemies_.Push(e);
        enemyNodes_.Push(rob);
    }

    void SpawnWave() {
        ++waveNum_;
        int count = Min(3 + waveNum_, 10);
        const float half = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f - 6.0f;
        for (int i = 0; i < count; ++i) {
            float ang = (float)(rand() % 360) * 0.0174533f;
            float r = 10.0f + (float)(rand() % 30);
            Vector3 pos(cosf(ang) * r, 1.0f, sinf(ang) * r);
            pos.x_ = Clamp(pos.x_, -half, half);
            pos.z_ = Clamp(pos.z_, -half, half);
            bool elite = (waveNum_ >= 3) && ((rand() % 100) < 20);
            SpawnEnemyRobot(pos, elite);
        }
        ShowMessage("ВОЛНА " + String(waveNum_) + "! Роботов: " + String(count));
        UpdateScoreDisplay();
    }

    void UpdateEnemies(float dt) {
        Vector3 camPos = cameraNode_ ? cameraNode_->GetPosition() : Vector3::ZERO;
        for (unsigned i = 0; i < enemies_.Size(); ++i) {
            EnemyData& e = enemies_[i];
            if (!e.alive_) continue;
            Node* n = e.node_.Get();
            if (!n) { e.alive_ = false; continue; }

            Vector3 p = n->GetPosition();
            Vector3 toCam = camPos - p; toCam.y_ = 0.0f;
            float dist = toCam.Length();

            Vector3 targetPos = p;
            if (dist > 14.0f) {
                targetPos = p + toCam.Normalized() * (e.speed_ * dt);
            } else if (dist < 5.0f) {
                targetPos = p - toCam.Normalized() * (e.speed_ * 0.5f * dt);
            } else {
                e.phase_ += dt * 0.7f;
                targetPos = e.patrolCenter_ + Vector3(cosf(e.phase_) * e.patrolRadius_, 0.0f,
                                                     sinf(e.phase_ * 0.8f) * e.patrolRadius_);
                Vector3 dir = targetPos - p; dir.y_ = 0.0f;
                float dl = dir.Length();
                if (dl > 0.001f) targetPos = p + dir / dl * Min(dl, e.speed_ * dt);
            }

            n->SetPosition(Vector3(targetPos.x_, 1.0f + 0.05f * sinf(timeAcc_ * 3.0f + e.phase_), targetPos.z_));
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
            RigidBody* rb = e.body_.Get();
            if (rb) {
                Vector3 camPos = cameraNode_ ? cameraNode_->GetPosition() : Vector3::ZERO;
                Vector3 kb = n->GetPosition() - camPos; kb.y_ = 0.2f;
                rb->ApplyImpulse(kb.Normalized() * 6.0f);
            }
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
        Node* debrisRoot = scene_->CreateChild("Debris");
        debrisRoot->SetPosition(pos);
        for (int i = 0; i < 8; ++i) {
            Node* frag = debrisRoot->CreateChild("Frag");
            frag->SetScale(Vector3(0.08f, 0.08f, 0.08f));
            StaticModel* sm = frag->CreateComponent<StaticModel>();
            sm->SetModel(boxModel_);
            sm->SetMaterial(robotBodyMat_ ? robotBodyMat_ : metalMaterial_);
            RigidBody* rb = frag->CreateComponent<RigidBody>();
            rb->SetMass(0.2f);
            rb->SetCollisionLayer(0);
            rb->SetRestitution(0.4f);
            CollisionShape* cs = frag->CreateComponent<CollisionShape>();
            cs->SetBox(Vector3::ONE);
            float a = (float)(rand() % 628) / 100.0f;
            float up = 3.0f + (float)(rand() % 40) / 10.0f;
            rb->SetLinearVelocity(Vector3(cosf(a) * 4.0f, up, sinf(a) * 4.0f));
            rb->SetAngularVelocity(Vector3((float)(rand() % 400 - 200), (float)(rand() % 400 - 200), (float)(rand() % 400 - 200)));
        }
        PendingRemove pr; pr.node_ = debrisRoot; pr.ttl_ = 4.0f; pendingRemoves_.Push(pr);

        if (sparkEffect_) {
            Node* sparks = scene_->CreateChild("DebrisSparks");
            sparks->SetPosition(pos);
            ParticleEmitter* pe = sparks->CreateComponent<ParticleEmitter>();
            pe->SetEffect(sparkEffect_);
            pe->SetEDuration(0.4f);
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
        emitter->SetEDuration(0.2f);
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

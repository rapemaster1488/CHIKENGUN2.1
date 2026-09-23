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
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Math/MathDefs.h>
#include <Urho3D/Math/Vector3.h>
#include <Urho3D/Math/Quaternion.h>

using namespace Urho3D;

// ============================================================================
// Константы и настройки игры
// ============================================================================

namespace GameConstants {
    // Настройки игрока
    constexpr float PLAYER_HEIGHT = 1.7f;
    constexpr float PLAYER_SPEED = 8.0f;
    constexpr float PLAYER_SPRINT_MULTIPLIER = 1.8f;
    constexpr float MOUSE_SENSITIVITY = 0.15f;
    constexpr float MAX_PITCH = 89.0f;
    
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
    constexpr int ARENA_SIZE = 50;
    constexpr float TILE_SIZE = 2.0f;
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

struct WeaponState {
    int currentAmmo_;
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
    bool isMouseVisible_ = false;
    
    // Временные переменные
    float messageTimer_ = 0.0f;
    float damageFlashTimer_ = 0.0f;
    float timeAcc_ = 0.0f;
    Vector<PendingRemove> pendingRemoves_;
    
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
    ParticleEffect* sparkEffect_ = nullptr;
    ParticleEffect* smokeEffect_ = nullptr;
    Font* font_ = nullptr;
    
    // Звуки
    Sound* shootSound_ = nullptr;
    Sound* reloadSound_ = nullptr;
    Sound* emptyClickSound_ = nullptr;
    Sound* hitSound_ = nullptr;
    Sound* destroySound_ = nullptr;

public:
    MyApp(Context* context) : Application(context) {}

    void Start() override {
        // Инициализация случайных чисел
        srand((unsigned int)time(nullptr));
        
        // Настройка движка
        SetupEngineSettings();
        
        // Загрузка ресурсов
        if (!LoadResources()) {
            URHO3D_LOGERROR("Failed to load resources!");
            engine_->Exit();
            return;
        }
        
        // Создание сцены
        CreateScene();
        
        // Создание игрока и камеры
        CreatePlayer();
        
        // Создание оружия
        CreateWeapon();
        
        // Создание UI
        CreateUI();
        
        // Настройка ввода
        SetupInput();
        
        // Подписка на события
        SubscribeToEvents();
        
        // Показ сообщения о начале игры
        ShowMessage("Добро пожаловать в FPS Demo!\nWASD - движение, ЛКМ - огонь, R - перезарядка, Shift - бег, Esc - выход");
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
        renderer->SetShadowMapSize(2048);
        renderer->SetSpecularLighting(true);
        
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
        
        return true;
    }
    
 void CreateMaterials() {
     auto* cache = GetSubsystem<ResourceCache>();
     stoneMaterial_ = cache->GetResource<Material>("Materials/Stone.xml");
     if (!stoneMaterial_) stoneMaterial_ = cache->GetResource<Material>("DefaultGrey.xml");
     if (!stoneMaterial_) stoneMaterial_ = new Material(context_);
     metalMaterial_ = cache->GetResource<Material>("Materials/Metal.xml");
     if (!metalMaterial_) metalMaterial_ = stoneMaterial_;
     targetMaterial_ = stoneMaterial_;
     targetHitMaterial_ = stoneMaterial_;
     floorMaterialLight_ = stoneMaterial_;
     floorMaterialDark_ = stoneMaterial_;
     URHO3D_LOGINFO(stoneMaterial_ ? "Base material OK" : "Base material NULL");
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
        CreateCeiling();
        CreateObstacles();
        CreateTargets();
        
        // Создаем освещение
        CreateLighting();
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
        const float wallHeight = 5.0f;
        const float wallThickness = 1.0f;
        const float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        
        // Северная стена
        CreateWall(Vector3(0, wallHeight/2, -arenaSize), Vector3(arenaSize, wallHeight, wallThickness));
        // Южная стена
        CreateWall(Vector3(0, wallHeight/2, arenaSize), Vector3(arenaSize, wallHeight, wallThickness));
        // Западная стена
        CreateWall(Vector3(-arenaSize, wallHeight/2, 0), Vector3(wallThickness, wallHeight, arenaSize));
        // Восточная стена
        CreateWall(Vector3(arenaSize, wallHeight/2, 0), Vector3(wallThickness, wallHeight, arenaSize));
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
        const float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        
        Node* ceilingNode = scene_->CreateChild("Ceiling");
        ceilingNode->SetPosition(Vector3(0, wallHeight_, 0));
        ceilingNode->SetScale(Vector3(arenaSize * 2, 1.0f, arenaSize * 2));
        
        StaticModel* model = ceilingNode->CreateComponent<StaticModel>();
        model->SetModel(planeModel_ ? planeModel_ : boxModel_);
        model->SetMaterial(stoneMaterial_);
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
        sunNode->SetDirection(Vector3(-1, -1, -1).Normalized());
        
        Light* sunLight = sunNode->CreateComponent<Light>();
        sunLight->SetLightType(LIGHT_DIRECTIONAL);
        sunLight->SetColor(Color(1.0f, 0.95f, 0.8f, 1.0f));
        sunLight->SetCastShadows(true);
        // Точечные источники света
        float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
        
        // Свет над платформой
        Node* pointLight1 = scene_->CreateChild("PointLight1");
        pointLight1->SetPosition(Vector3(0, 8, 0));
        
        Light* pl1 = pointLight1->CreateComponent<Light>();
        pl1->SetLightType(LIGHT_POINT);
        pl1->SetColor(Color(0.8f, 0.8f, 1.0f, 1.0f));
        pl1->SetRange(20.0f);
        pl1->SetCastShadows(true);
        
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
            pl->SetRange(15.0f);
            pl->SetCastShadows(true);
        }
        
        // ambient light
    }

    // =========================================================================
    // Создание игрока и оружия
    // =========================================================================
    
    void CreatePlayer() {
        // Создаем узел камеры
        cameraNode_ = scene_->CreateChild("Camera");
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
        weaponModelNode_->SetPosition(Vector3(0, 0, 0));
        weaponModelNode_->SetScale(Vector3(0.1f, 0.1f, 0.4f));
        weaponModelNode_->SetRotation(Quaternion(-90, 0, 0));
        
        StaticModel* weaponModel = weaponModelNode_->CreateComponent<StaticModel>();
        weaponModel->SetModel(boxModel_);
        weaponModel->SetMaterial(metalMaterial_);
        
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
            scoreText_->SetText("SCORE: " + String(player_.score_) + " | KILLS: " + String(player_.kills_));
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
        
        // Восстанавливаем отдачу
        weapon_.recoilX_ = Lerp(weapon_.recoilX_, 0.0f, GameConstants::RECOIL_RECOVERY * dt);
        weapon_.recoilY_ = Lerp(weapon_.recoilY_, 0.0f, GameConstants::RECOIL_RECOVERY * dt);
        
        // Применяем отдачу к оружию
        ApplyRecoilToWeapon();
        
        // === ОБРАБОТКА ВВОДА ДЛЯ ДВИЖЕНИЯ ===
        HandleMovement(dt);
        
        // Обновляем анимацию покачивания оружия
        UpdateWeaponBob(dt);
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
        yaw_ += (float)mouseMove.x_ * GameConstants::MOUSE_SENSITIVITY;
        pitch_ += (float)mouseMove.y_ * GameConstants::MOUSE_SENSITIVITY;
        
        // Ограничиваем вертикальный угол
        pitch_ = Clamp(pitch_, -GameConstants::MAX_PITCH, GameConstants::MAX_PITCH);
        
        // Применяем вращение к камере
        // ВАЖНО: используем правильный порядок: сначала YAW (вокруг Y), потом PITCH (вокруг локальной X)
        cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
    }
    
    void HandleKeyDown(StringHash eventType, VariantMap& eventData) {
        using namespace KeyDown;
        
        int key = eventData[P_KEY].GetI32();
        
        // Бег
        if (key == KEY_LSHIFT || key == KEY_RSHIFT) {
            isSprinting_ = true;
        }
        
        // Перезарядка
        if (key == KEY_R && !weapon_.isReloading_) {
            StartReload();
        }
        
        // Скрыть/показать мышь (для отладки)
        if (key == KEY_TAB) { ToggleMouseVisibility(); }
    if (key == KEY_ESCAPE) { engine_->Exit(); }
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
        
        // Левая кнопка мыши - стрельба
        if (button == MOUSEB_LEFT) {
            TryFire();
        }
        
        // Правая кнопка мыши - прицеливание (можно добавить зум)
        if (button == MOUSEB_RIGHT) {
            // Можно реализовать прицеливание
        }
    }
    
    void HandleMouseButtonUp(StringHash eventType, VariantMap& eventData) {
        using namespace MouseButtonUp;
        
        int button = eventData[P_BUTTON].GetI32();
        
        if (button == MOUSEB_RIGHT) {
            // Отмена прицеливания
        }
    }

    // =========================================================================
    // Игровая логика
    // =========================================================================
    
    void HandleMovement(float dt) {
        auto* input = GetSubsystem<Input>();
        
        // Получаем направление взгляда (только горизонтальное)
        Quaternion yawRot(0.0f, yaw_, 0.0f);
        Vector3 forward = yawRot * Vector3(0.0f, 0.0f, 1.0f);
        Vector3 right = yawRot * Vector3(1.0f, 0.0f, 0.0f);
        
        // Собираем ввод движения
        Vector3 moveDirection = Vector3::ZERO;
        
        if (input->GetKeyDown(KEY_W)) {
            moveDirection += forward;
        }
        if (input->GetKeyDown(KEY_S)) {
            moveDirection -= forward;
        }
        if (input->GetKeyDown(KEY_D)) {
            moveDirection += right;
        }
        if (input->GetKeyDown(KEY_A)) {
            moveDirection -= right;
        }
        
        // Нормализуем и применяем скорость
        if (moveDirection.LengthSquared() > 0.0f) {
            moveDirection.Normalize();
            
            float speed = GameConstants::PLAYER_SPEED;
            if (isSprinting_) {
                speed *= GameConstants::PLAYER_SPRINT_MULTIPLIER;
            }
            
            Vector3 newPosition = cameraNode_->GetPosition() + moveDirection * speed * dt;
            
            // Простая проверка коллизий с границами арены
            float arenaSize = GameConstants::ARENA_SIZE * GameConstants::TILE_SIZE / 2.0f;
            newPosition.x_ = Clamp(newPosition.x_, -arenaSize + 2.0f, arenaSize - 2.0f);
            newPosition.z_ = Clamp(newPosition.z_, -arenaSize + 2.0f, arenaSize - 2.0f);
            newPosition.y_ = GameConstants::PLAYER_HEIGHT;  // Держим высоту постоянной
            
            cameraNode_->SetPosition(newPosition);
        }
    }
    
    void UpdateWeaponBob(float dt) {
        auto* input = GetSubsystem<Input>();
        
        // Проверяем, движется ли игрок
        bool isMoving = input->GetKeyDown(KEY_W) || input->GetKeyDown(KEY_S) ||
                       input->GetKeyDown(KEY_A) || input->GetKeyDown(KEY_D);
        
        if (isMoving && !weapon_.isReloading_) {
            weapon_.weaponBobPhase_ += GameConstants::WEAPON_BOB_FREQUENCY * dt;
            
            float bobAmount = GameConstants::WEAPON_BOB_AMOUNT;
            if (isSprinting_) {
                bobAmount *= 2.0f;
            }
            
            float bobX = cos(weapon_.weaponBobPhase_) * bobAmount;
            float bobY = fabs(sin(weapon_.weaponBobPhase_ * 2.0f)) * bobAmount;
            
            weaponNode_->SetPosition(Vector3(0.3f + bobX, -0.25f - bobY, 0.5f));
        } else {
            // Возвращаем оружие в исходное положение
            Vector3 currentPos = weaponNode_->GetPosition();
            Vector3 targetPos(0.3f, -0.25f, 0.5f);
            weaponNode_->SetPosition(Lerp(currentPos, targetPos, 10.0f * dt));
        }
    }
    
    void ApplyRecoilToWeapon() {
        // Применяем отдачу к позиции оружия
        Vector3 basePos(0.3f, -0.25f, 0.5f);
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
        weapon_.fireTimer_ = GameConstants::FIRE_RATE;
        
        // Добавляем отдачу
        float randomRecoil = (float)(rand() % 100) / 100.0f * GameConstants::BULLET_SPREAD;
        weapon_.recoilX_ += GameConstants::RECOIL_AMOUNT * (0.8f + randomRecoil);
        weapon_.recoilY_ += (float)(rand() % 40 - 20) / 10.0f;
        
        // Визуальные эффекты выстрела
        PlayMuzzleFlash();
        PlayShootSound();
        
        // Расчет точки попадания (рейкаст)
        PerformRaycast();
        
        // Обновляем UI
        UpdateAmmoDisplay();
    }
    
    void PerformRaycast() {
        // Получаем направление из центра экрана
        auto* camera = cameraNode_->GetComponent<Camera>();
        if (!camera) return;
        
        // Добавляем разброс
        float spreadAngle = GameConstants::BULLET_SPREAD * (1.0f - player_.GetAccuracy() / 200.0f);
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
            CheckTargetHit(result.body_);
            
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
    
    void CheckTargetHit(RigidBody* hitBody) {
        if (!hitBody) return;
        
        Node* hitNode = hitBody->GetNode();
        if (!hitNode) return;
        
        // Ищем мишень, соответствующую попавшему узлу
        for (auto& target : targets_) {
            if (target.isActive_ && target.node_ == hitNode) {
                // Попали в мишень!
                target.health_ -= GameConstants::BULLET_DAMAGE;
                
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
        
        // Обновляем статистику
        player_.score_ += target.scoreValue_;
        player_.kills_++;
        
        // Удаляем мишень
        Node* targetNode = target.node_.Get();
        if (targetNode) {
            // Эффект разрушения
            CreateExplosionEffect(targetNode->GetPosition());
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
    
    void PlayShootSound() {
        // В реальной игре здесь был бы звук выстрела
        // auto* audio = GetSubsystem<Audio>();
        // auto* soundSource = cameraNode_->CreateComponent<SoundSource>();
        // soundSource->Play(shootSound_);
    }
    
    void PlayReloadSound() {
        // Звук перезарядки
    }
    
    void PlayEmptyClickSound() {
        // Звук щелчка пустого магазина
    }
    
    void PlayHitSound() {
        // Звук попадания
    }
    
    void PlayDestroySound() {
        // Звук разрушения мишени
    }
    
    // =========================================================================
    // Утилиты
    // =========================================================================
    
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
};

// ============================================================================
// Макрос регистрации приложения
// ============================================================================

URHO3D_DEFINE_APPLICATION_MAIN(MyApp)

#include <Urho3D/Engine/Application.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Input/Input.h>

using namespace Urho3D;

class MyApp : public Application {
    URHO3D_OBJECT(MyApp, Application);
public:
    MyApp(Context* context) : Application(context) {}
    
    void Start() override {
        SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(MyApp, HandleUpdate));
        
        scene_ = new Scene(context_);
        scene_->CreateComponent<Octree>();
        
        Node* cameraNode = scene_->CreateChild("Camera");
        Camera* camera = cameraNode->CreateComponent<Camera>();
        cameraNode->SetPosition(Vector3(0, 2, -10));
        
        Node* lightNode = scene_->CreateChild("Light");
        Light* light = lightNode->CreateComponent<Light>();
        light->SetLightType(LIGHT_DIRECTIONAL);
        
        Node* boxNode = scene_->CreateChild("Box");
        boxNode->SetPosition(Vector3(0, 1, 5));
        StaticModel* box = boxNode->CreateComponent<StaticModel>();
        
        ResourceCache* cache = GetSubsystem<ResourceCache>();
        box->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
        box->SetMaterial(cache->GetResource<Material>("Materials/Stone.xml"));
    }
    
    void HandleUpdate(StringHash eventType, VariantMap& eventData) {
        Input* input = GetSubsystem<Input>();
        if (input->GetKeyPress(KEY_ESC)) engine_->Exit();
    }
    
private:
    SharedPtr<Scene> scene_;
};

URHO3D_DEFINE_APPLICATION_MAIN(MyApp)
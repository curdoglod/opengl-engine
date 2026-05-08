#ifndef OBJECT_H
#define OBJECT_H

#include <SDL.h>
#include <vector>
#include "Utils.h"


class Component; 
class Image; 
class Scene; 

class Object {
public:
    bool Crossing(Object* obj, const float& x_range, const float& y_range);
    bool Crossing(Object* obj);
    Vector2 GetPosition() const;
    Vector3 GetPosition3D() const;
    Vector3 GetAngle() const {return angle; }; 
    void SetPosition(const Vector2& vec2);
    void SetPosition(const Vector3 &vec3);
    void SetRotation(float angle);
    void SetRotation(const Vector3& angle);
    void SetSize(const Vector2& vec2); 
    void SetSize(const Vector3& vec3);
    void SetPositionOnPlatform(const Vector2 & vec2);
    void MoveY(const float& pos_y);
    void MoveX(const float& pos_x);

    Vector2 GetSize();
    Vector3 GetSize3D() const { return size; }
    void InitSize(Image* img);
    void InitSize();
    void SetLayer(int layer);
    int GetLayer() const;
    void AddComponent(Component* component);
    Component* GetComponent(const std::type_info& ti) const;

    template<typename T>
    void RemoveComponent();
  
    void update(float deltaTime);
    void lateUpdate(float deltaTime);
    Scene* GetScene() const;
    void UpdateEvents(SDL_Event& event);
    void SetActive(bool status);
    bool IsActive() const { return active; }
    void SetStatic(bool s) { m_isStatic = s; }
    bool IsStatic() const { return m_isStatic; }
    template <typename T>
    T* GetComponent() const {
        Component* comp = this->GetComponent(typeid(T));
        return dynamic_cast<T*>(comp);
    }
    Object* CloneObject() const;

    void notifyCollisionEnter(Object* other);
    void notifyTriggerEnter(Object* other);

    friend class Engine; 
    friend class Scene;
private:
    virtual ~Object();
    Object(Scene* _game);
    Scene* currentScene;
    std::vector<Component*> components;
    Vector3 position;
    Vector3 size;
    Vector3 angle; 
    int layer;
    bool active; 
    bool m_isStatic = false;
    float deltatime;
   
  


};

#endif // OBJECT_H

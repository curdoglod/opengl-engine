#ifndef COMPONENT_H
#define COMPONENT_H
#include "Utils.h"
#include <SDL.h>

class Object;

class Component {
public:
    
    virtual ~Component() {}

    virtual void Init() {}

    virtual void Update() {
    }
    virtual void Update(float deltaTime) {
    }

    virtual void LateUpdate(float deltaTime) {
    }
 
    virtual void OnMouseButtonDown(Vector2 mouse_position) {

    }

    virtual void OnMouseButtonUp(Vector2 mouse_position) {

    }
    virtual void OnMouseButtonMotion(Vector2 mouse_position) {

    }
    virtual void onKeyPressed(SDL_Keycode key) {}
    virtual void onKeyReleased(SDL_Keycode key) {}

    virtual void OnCollisionEnter(Object* other) {}
    virtual void OnTriggerEnter(Object* other) {}

    virtual Component* Clone() const { return nullptr; }
   
protected:
    Object* object = nullptr;
    Object* CreateObject();
private:
    friend class Object; 
    void setOwner(Object* owner) {
        object = owner;
    }
}; 




#endif // COMPONENT_H

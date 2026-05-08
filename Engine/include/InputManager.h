#pragma once

#include <SDL.h>
#include <unordered_set>
#include "Utils.h"

// Frame-based keyboard and mouse state.
class InputManager {
public:
    static InputManager& Get() {
        static InputManager instance;
        return instance;
    }

    void BeginFrame() {
        prevKeys         = currentKeys;
        prevMouseButtons = currentMouseButtons;
        SDL_GetRelativeMouseState(&mouseDeltaX, &mouseDeltaY);
        SDL_GetMouseState(&mouseX, &mouseY);
    }

    void ProcessEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_KEYDOWN:
                if (!event.key.repeat)
                    currentKeys.insert(event.key.keysym.sym);
                break;
            case SDL_KEYUP:
                currentKeys.erase(event.key.keysym.sym);
                break;
            case SDL_MOUSEBUTTONDOWN:
                currentMouseButtons.insert(event.button.button);
                break;
            case SDL_MOUSEBUTTONUP:
                currentMouseButtons.erase(event.button.button);
                break;
            default:
                break;
        }
    }

    bool IsKeyDown(SDL_Keycode key) const {
        return currentKeys.count(key) > 0;
    }

    bool IsKeyPressed(SDL_Keycode key) const {
        return currentKeys.count(key) > 0 && prevKeys.count(key) == 0;
    }

    bool IsKeyReleased(SDL_Keycode key) const {
        return currentKeys.count(key) == 0 && prevKeys.count(key) > 0;
    }

    bool IsMouseButtonDown(Uint8 button) const {
        return currentMouseButtons.count(button) > 0;
    }

    bool IsMouseButtonPressed(Uint8 button) const {
        return currentMouseButtons.count(button) > 0
            && prevMouseButtons.count(button) == 0;
    }

    bool IsMouseButtonReleased(Uint8 button) const {
        return currentMouseButtons.count(button) == 0
            && prevMouseButtons.count(button) > 0;
    }

    Vector2 GetMousePosition() const {
        return Vector2(static_cast<float>(mouseX),
                       static_cast<float>(mouseY));
    }

    Vector2 GetMouseDelta() const {
        return Vector2(static_cast<float>(mouseDeltaX),
                       static_cast<float>(mouseDeltaY));
    }

private:
    InputManager() = default;
    InputManager(const InputManager&)            = delete;
    InputManager& operator=(const InputManager&) = delete;

    std::unordered_set<SDL_Keycode> currentKeys;
    std::unordered_set<SDL_Keycode> prevKeys;
    std::unordered_set<Uint8>       currentMouseButtons;
    std::unordered_set<Uint8>       prevMouseButtons;

    int mouseX = 0, mouseY = 0;
    int mouseDeltaX = 0, mouseDeltaY = 0;
};

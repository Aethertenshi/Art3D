#pragma once
#include <SDL3/SDL.h>
#include <cmath>
#include "../art.datatypes/Matrix4x4.h"
#include "imgui.h"

class CameraController {
public:
    float X = 0.0f;
    float Y = 0.0f;
    float Z = -2.5f;

    float Yaw = 0.0f;
    float Pitch = 0.0f;

    bool IsLooking = false;
    float Speed = 0.05f;
    float Sensitivity = 0.003f;

    void ProcessEvent(const SDL_Event& event, SDL_Window* window) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event.button.button == SDL_BUTTON_RIGHT) {
                if (!ImGui::GetIO().WantCaptureMouse) {
                    IsLooking = true;
                    SDL_SetWindowRelativeMouseMode(window, true);
                }
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (event.button.button == SDL_BUTTON_RIGHT) {
                if (IsLooking) {
                    IsLooking = false;
                    SDL_SetWindowRelativeMouseMode(window, false);
                }
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            if (IsLooking) {
                Yaw += event.motion.xrel * Sensitivity;
                Pitch += event.motion.yrel * Sensitivity;

                // Clamp pitch
                float maxPitch = 89.0f * 3.14159265f / 180.0f;
                if (Pitch > maxPitch) Pitch = maxPitch;
                if (Pitch < -maxPitch) Pitch = -maxPitch;
            }
        }
    }

    void ReleaseMouse(SDL_Window* window) {
        if (IsLooking) {
            IsLooking = false;
            SDL_SetWindowRelativeMouseMode(window, false);
        }
    }

    void Update() {
        const bool* keys = SDL_GetKeyboardState(nullptr);

        // Calculate 3D forward vector based on Yaw and Pitch
        float cosPitch = cosf(Pitch);
        float sinPitch = sinf(Pitch);
        float sinYaw = sinf(Yaw);
        float cosYaw = cosf(Yaw);

        float fwdX = sinYaw * cosPitch;
        float fwdY = -sinPitch;
        float fwdZ = cosYaw * cosPitch;

        // Right vector (remains horizontal for standard strafe controls)
        float rgtX = cosYaw;
        float rgtZ = -sinYaw;

        if (keys[SDL_SCANCODE_W]) {
            X += fwdX * Speed;
            Y += fwdY * Speed;
            Z += fwdZ * Speed;
        }
        if (keys[SDL_SCANCODE_S]) {
            X -= fwdX * Speed;
            Y -= fwdY * Speed;
            Z -= fwdZ * Speed;
        }
        if (keys[SDL_SCANCODE_A]) {
            X -= rgtX * Speed;
            Z -= rgtZ * Speed;
        }
        if (keys[SDL_SCANCODE_D]) {
            X += rgtX * Speed;
            Z += rgtZ * Speed;
        }

        // Vertical movement (Global Y-axis)
        if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_E]) {
            Y += Speed;
        }
        if (keys[SDL_SCANCODE_Q]) {
            Y -= Speed;
        }
    }

    Matrix4x4 GetViewMatrix() const {
        return Matrix4x4::Translation(-X, -Y, -Z)
            .Multiply(Matrix4x4::RotationY(-Yaw))
            .Multiply(Matrix4x4::RotationX(-Pitch));
    }
};

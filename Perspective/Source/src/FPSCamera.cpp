//
// Created by user on 1/26/2021.
//

/* Start Header -------------------------------------------------------
 * Copyright (C) 2020 DigiPen Institute of Technology.
 * Reproduction or disclosure of this file or its contents without the prior
 * written consent of DigiPen Institute of Technology is prohibited.
 * File Name: FPSCamera.cpp
 * Purpose: Lorem ipsum dolor sit amet, consectetur adipiscing elit.
 * Language: C++, G++
 * Platform: g++ (Ubuntu 9.3.0-10ubuntu2) 9.3, ThinkPad T430u, Nvidia GT 620M,
 *           OpenGL version string: 4.6.0 NVIDIA 390.138
 * Project: OpenGLFramework
 * Author: Roland Shum, roland.shum@digipen.edu
 * Creation date: 1/26/2021
 * End Header --------------------------------------------------------*/
#include "stdafx.h"
#include "FPSCamera.h"
#include "Input.h"
#include "GuiSystem.h"
#include "App.h"

void FpsCamera::Update(const Input &input, float dt) {
    // We do impulse physics
    using namespace glm;

    vec3 acceleration{0.0f};

    if (input.IsKeyDown(GLFW_KEY_W)) acceleration.z += 1;
    if (input.IsKeyDown(GLFW_KEY_S)) acceleration.z -= 1;

    if (input.IsKeyDown(GLFW_KEY_D)) acceleration.x += 1;
    if (input.IsKeyDown(GLFW_KEY_A)) acceleration.x -= 1;

    if (input.IsKeyDown(GLFW_KEY_E))      acceleration.y += 1;
    if (input.IsKeyDown(GLFW_KEY_Q))    acceleration.y -= 1;

    // Normal slowdown (decelerate based on division)
    if (length2(acceleration) > 0.01f && !GuiSystem::IsUsingKeyboard()) {
        acceleration = normalize(acceleration) * (maxSpeed / timeToMaxSpeed) * dt;
        velocity += rotate(orientation, acceleration);
    } else {
        // We are at the threshold where acceleration turns to deceleration
        if (length2(velocity) < stopThreshold) {
            velocity = vec3(0, 0, 0);
        } else {
            vec3 deaccel = -normalize(velocity) * (maxSpeed / timeFromMaxSpeed) * dt;
            velocity += deaccel;
        }
    }

    // Velocity movement
    float speed = length(velocity);
    if (speed > 0.0f) {
        speed = clamp(speed, 0.0f, maxSpeed);
        velocity = normalize(velocity) * speed;
        position += velocity * dt;
    }

    // Now calculate rotation velocity from mouse
    if (input.IsButtonDown(GLFW_MOUSE_BUTTON_2) && !GuiSystem::IsUsingMouse()) {
        vec2 screenSize = {App::GetApp().windowWidth, App::GetApp().windowHeight};
        // Screen size independent but also aspect ratio dependent!
        vec2 mouseDelta = input.GetMouseDelta() / screenSize.x;

        // Make rotations less sensitive when zoomed in
        float fovMultiplier = 0.2f + ((fieldOfView - minFieldOfView) / (maxFieldOfView - minFieldOfView)) * 0.8f;

        pitchYawRoll.x += mouseDelta.x * rotationMultiplier * fovMultiplier * dt;
        pitchYawRoll.y += mouseDelta.y * rotationMultiplier * fovMultiplier * dt;
    }

    // Calculate banking due to movement
    vec3 right = rotate(orientation, vec3(1, 0, 0));
    forward = rotate(orientation, vec3(0, 0, 1));
    if (speed > 0.0f) {
        auto direction = velocity / speed;
        float speedAlongRight = dot(direction, right) * speed;
        float signOrZeroSpeed = float(speedAlongRight > 0.0f) - float(speedAlongRight < 0.0f);
        float bankAmountSpeed = std::abs(speedAlongRight) / maxSpeed * 2.0f;

        float rotationAlongY = pitchYawRoll.x;
        float signOrZeroRotation = float(rotationAlongY > 0.0f) - float(rotationAlongY < 0.0f);
        float bankAmountRotation = clamp(std::abs(rotationAlongY) * 100.0f, 0.0f, 3.0f);

        float targetBank =
                ((signOrZeroSpeed * -bankAmountSpeed) + (signOrZeroRotation * -bankAmountRotation)) * baselineBankAngle;
        pitchYawRoll.z = mix(pitchYawRoll.z, targetBank, 1.0f - pow(0.35f, dt));
    }

    // Damp rotation continuously

    pitchYawRoll *= pow(rotationDampening, dt);

    // Apply rotation

    orientation = angleAxis(pitchYawRoll.y, right) * orientation;
    orientation = angleAxis(pitchYawRoll.x, vec3(0, 1, 0)) * orientation;

    bankingOrientation = angleAxis(pitchYawRoll.z, forward);

    // Apply zoom
    if (!GuiSystem::IsUsingMouse()) {
        targetFieldOfView += -input.GetScrollDelta() * zoomSensitivity;
        targetFieldOfView = clamp(targetFieldOfView, minFieldOfView, maxFieldOfView);
    }
    fieldOfView = mix(fieldOfView, targetFieldOfView, 1.0f - pow(0.01f, dt));

    // Create the view matrix

    auto preAdjustedUp = rotate(orientation, vec3(0, 1, 0));
    auto up = rotate(bankingOrientation, preAdjustedUp);

    lookAt = position + forward;
    viewFromWorld = glm::lookAtLH(position, lookAt, up);

    // Create the projection matrix

    float aspectRatio = float(targetPixelsWidth) / float(targetPixelsHeight);
    projectionFromView = glm::perspectiveLH(fieldOfView, aspectRatio, zNear, zFar);
}

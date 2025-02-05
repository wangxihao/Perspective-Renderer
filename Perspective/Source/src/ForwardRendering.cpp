//
// Created by user on 1/30/2021.
//

/* Start Header -------------------------------------------------------
 * Copyright (C) 2020 DigiPen Institute of Technology.
 * Reproduction or disclosure of this file or its contents without the prior
 * written consent of DigiPen Institute of Technology is prohibited.
 * File Name: ForwardRendering.cpp
 * Purpose: Lorem ipsum dolor sit amet, consectetur adipiscing elit.
 * Language: C++, G++
 * Platform: g++ (Ubuntu 9.3.0-10ubuntu2) 9.3, ThinkPad T430u, Nvidia GT 620M,
 *           OpenGL version string: 4.6.0 NVIDIA 390.138
 * Project: OpenGLFramework
 * Author: Roland Shum, roland.shum@digipen.edu
 * Creation date: 1/30/2021
 * End Header --------------------------------------------------------*/
#include "stdafx.h"
#include "ForwardRendering.h"
#include "ShaderSystem.h"
#include "TransformSystem.h"
#include "Model.h"
#include "DebugModel.h"
#include "Scene.h"
#include "DebugSystem.h"
#include "Animator.h"
#include "MaterialSystem.h"

void ForwardRendering::Draw(const Scene &scene) {
    static bool once = false;
    if (!once) {
        this->debugLineMaterial = (new DebugLineMaterial());
        MaterialSystem::getInstance().ManageMaterial(debugLineMaterial);

        cylinder = new Model("Common/cylinder.fbx");
        quad = new Model("Common/cylinder.fbx");
        once = true;
    }
    TransformSystem &transformSystem = TransformSystem::getInstance();
    static bool debugOptions[DebugSystem::DebugType::All] = {false, false};

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    static bool drawBones = true;
    ImGui::Checkbox("Draw bones?", &drawBones);

    if(drawBones)
    { 
        if (scene.entities[0].animator->GetAnimation() != nullptr) {
            glUseProgram(this->debugLineMaterial->program);
            Transform &trans = TransformSystem::getInstance().Get(scene.entities[0].model->transformID);

            {
                auto finalMatrices = scene.entities[0].animator->DrawBones(MyMath::VQS(trans.matrix));

                for (int i = 0; i < finalMatrices.size(); ++i) {
                    this->debugLineMaterial->BindUniforms(finalMatrices[i].ToMat4());
                    if (i == 0) {
                        debugLineMaterial->BindColor(glm::vec4{0.7, 0.1, 0.1, 1.0});
                    } else {
                        debugLineMaterial->BindColor(glm::vec4{0.2, 0.8, 0.27, 1.0});
                    }
                    cylinder->Draw();
                }
            }
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    static bool drawFloor = false;
    ImGui::Checkbox("Draw Floor", &drawFloor);

    if(drawFloor)
    { 
        // Draw quad
        glUseProgram(this->debugLineMaterial->program);

        glm::mat4 transform = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
        transform = glm::translate(transform, glm::vec3(0.0f, -10.f, 0.0f));
        transform = glm::rotate(transform, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::scale(transform, glm::vec3(10000,1,10000));

        this->debugLineMaterial->BindUniforms(transform);
        debugLineMaterial->BindColor(glm::vec4{ 0.5, 0.5, 0.3, 0.4 });
        quad->Draw();


    }
    else
    {
        glm::vec3 pos{ 0,0,0 };
        glm::vec3 norm{ 0, 1, 0 };
        glm::vec3 color{ 1, 1, 0.1 };
        glm::vec3 color2{ 1, 0.1, 0.1 };
        // dd::plane(glm::value_ptr(pos), glm::value_ptr(norm), glm::value_ptr(color), glm::value_ptr(color2), 1000, 100);
        dd::xzSquareGrid(-1000, 1000, -1.0, 100, glm::value_ptr(color));
    }


}

void ForwardRendering::ProgramLoaded(GLuint program) {


}

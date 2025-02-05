//
// Created by user on 1/30/2021.
//

/* Start Header -------------------------------------------------------
 * Copyright (C) 2020 DigiPen Institute of Technology.
 * Reproduction or disclosure of this file or its contents without the prior
 * written consent of DigiPen Institute of Technology is prohibited.
 * File Name: Math.cpp
 * Purpose: Lorem ipsum dolor sit amet, consectetur adipiscing elit.
 * Language: C++, G++
 * Platform: g++ (Ubuntu 9.3.0-10ubuntu2) 9.3, ThinkPad T430u, Nvidia GT 620M,
 *           OpenGL version string: 4.6.0 NVIDIA 390.138
 * Project: OpenGLFramework
 * Author: Roland Shum, roland.shum@digipen.edu
 * Creation date: 1/30/2021
 * End Header --------------------------------------------------------*/
#include "stdafx.h"
#include "MyMath.h"
#include "Model.h"
#include "ModelSystem.h"
#include "TransformSystem.h"


glm::vec4 MyMath::Remap(const glm::vec4 &value, const glm::vec2 &inMinMax, const glm::vec2 &outMinMax) {
    return outMinMax.x + (value - inMinMax.x) * (outMinMax.y - outMinMax.x) / (inMinMax.y - inMinMax.x);
}

float MyMath::Remap(float value, float low1, float high1, float low2, float high2) {
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

void MyMath::CsoSupport(
        const Model &modelA, const Model &modelB, const glm::vec3 &dir, glm::vec3 &support, glm::vec3 &supportA
        , glm::vec3 &supportB
                       ) {
    Transform &transformA = TransformSystem::getInstance().Get(modelA.transformID);
    Transform &transformB = TransformSystem::getInstance().Get(modelA.transformID);

    // Convert search direction to model space
    const glm::vec3 localDirA = transformA.WorldToLocal(glm::vec4(dir, 0));
    const glm::vec3 localDirB = transformA.WorldToLocal(glm::vec4(-dir, 0));

    // Compute support points in model space

}

glm::vec3 MyMath::FindSupportPoint(const std::vector<Vertex> &vertices, const glm::vec3 &dir) {
    float highest = std::numeric_limits<float>::max();
    glm::vec3 support{0, 0, 0};
    for (int i = 0; i < vertices.size(); i++) {
        glm::vec3 v = vertices[i].position;
        float dot = glm::dot(dir, v);
        if (dot > highest) {
            highest = dot;
            support = v;
        }
    }
    return support;
}

MyMath::Quaternion MyMath::Slerp(const MyMath::Quaternion &begin, const MyMath::Quaternion &end, float factor) {
    MyMath::Quaternion myQuatBegin = begin.Norm();
    MyMath::Quaternion myQuatEnd = end.Norm();

    float cosTheta = myQuatBegin.Dot(myQuatEnd);


    // If cosTheta < 0, the interpolation will take the long way around the sphere.
    // To fix this, one quat must be negated.
    if (cosTheta < 0.0f) {
        myQuatEnd = (myQuatEnd * -1);
        cosTheta = -cosTheta;
    }

    // Perform a linear interpolation when cosTheta is close to 1 to avoid side effect of sin(angle) becoming a zero denominator
    if (cosTheta > 1.0f - glm::epsilon<float>()) {
        // Linear interpolation all components
        return MyMath::Quaternion(
                MyMath::Lerp(myQuatBegin.s, myQuatEnd.s, factor),
                {MyMath::Lerp(myQuatBegin.v.x, myQuatEnd.v.x, factor),
                 MyMath::Lerp(myQuatBegin.v.y, myQuatEnd.v.y, factor),
                 MyMath::Lerp(myQuatBegin.v.z, myQuatEnd.v.z, factor)});
    }

    float angle = acos(cosTheta);

    return (myQuatBegin * sin((1.0f - factor) * angle) + myQuatEnd * sin(factor * angle)) / sin(angle);
}

glm::vec3 MyMath::Slerp(const glm::vec3 &begin, const glm::vec3 &end, float factor) {
    float cosTheta = glm::dot(begin, end);
    glm::vec3 localEnd = end;
    if (cosTheta < 0) {
        localEnd = (end * -1);
        cosTheta = -cosTheta;
    }

    float angle = acos(cosTheta);
    return (begin * (sin((1 - factor) * angle)) + localEnd * sin(factor * angle)) * (1 / sin(angle));
}

glm::vec3 MyMath::Lerp(const glm::vec3 &begin, const glm::vec3 &end, float factor) {
    return (1 - factor) * begin + factor * end;
}

float MyMath::Lerp(float begin, float end, float factor) {
    return (1 - factor) * begin + factor * end;
}

float MyMath::ExpoLerp(float begin, float end, float factor) {
    return pow((end / begin), factor) * begin;
}

glm::vec3 MyMath::ExpoLerp(const glm::vec3 &begin, const glm::vec3 &end, float factor) {
    return {
            ExpoLerp(begin.x, end.x, factor),
            ExpoLerp(begin.y, end.y, factor),
            ExpoLerp(begin.z, end.z, factor)
    };
}

#if TINYOBJLOADER
glm::vec3 MyMath::FindSupportPoint(const std::vector<Shapes::Triangle>& trigs, const glm::vec3& dir) {
    float highest = std::numeric_limits<float>::max();
    glm::vec3 support{ 0, 0, 0 };
    for (int i = 0; i < trigs.size(); i++) {
        glm::vec3 v[] = { trigs[i].v1, trigs[i].v2, trigs[i].v3 };
        for (int i = 0; i < 3; i++) {
            float dot = glm::dot(dir, v[i]);
            if (dot > highest) {
                highest = dot;
                support = v[i];
}
        }
    }
    return support;
}

#endif

void MyMath::iSlerp::iSlerpInit(const MyMath::Quaternion &begin, const MyMath::Quaternion &end, int count) {
    // iSlerp constant
    const float alpha = acos(begin.Dot(end));
    const float beta = alpha / 2;
    const glm::vec3 u = (begin.s * begin.v - end.s * begin.v + glm::cross(begin.v, end.v)) / sin(alpha);
    qc = MyMath::Quaternion(cos(beta), sin(beta) * u);
    qk = begin;
    currCount = 0;
    Count = count;
}

bool MyMath::iSlerp::iSlerpStep() {
    if (++currCount < Count) {
        qk *= qc;
        return true;
    } else {
        return false;
    }
}

void MyMath::iVQS::iVQSInit(const MyMath::VQS &begin, const MyMath::VQS &end, int count) {
    // iLerp constant
    const glm::vec3 vc = (end.v - begin.v) / count;

    // iSlerp constant
    const float alpha = acos(begin.q.Dot(end.q));
    const float beta = alpha / 2;
    const glm::vec3 u = (begin.s * begin.v - end.s * begin.v + glm::cross(begin.v, end.v)) / sin(alpha);
    const MyMath::Quaternion qc(cos(beta), sin(beta) * u);

    // iELerp constant
    const float sc = pow((end.s / begin.s), (1.0f / count));
    const glm::mat3 Mc = glm::mat3(qc.ToMat4());

    m_Nc = sc * Mc;
    m_Vc = vc;
    m_Vk = begin.v;
    currCount = 0;
    Count = count;
}

bool MyMath::iVQS::iVQSStep() {
    if (++currCount < Count) {
        m_Vk += m_Vc;
        return true;
    } else {
        return false;
    }
}

glm::vec3 MyMath::iVQS::iVQSTransform(const glm::vec3 &rk) const {
    return m_Nc * rk + m_Vk;
}

MyMath::VQS MyMath::iVQS::GetVQSIter() const {
    glm::mat4 res{m_Nc};
    res[3] = glm::vec4(m_Vk, 1.0f);
    return MyMath::VQS(res);
}

void MyMath::iLerp::iLerpInit(const glm::vec3 &begin, const glm::vec3 &end, int count) {
    // iLerp constant
    vc = (end - begin) / count;
    vk = begin;
    currCount = 0;
    Count = count;
}

bool MyMath::iLerp::iLerpStep() {
    if (++currCount < Count) {
        vk += vc;
        return true;
    } else {
        return false;
    }
}

glm::vec3 MyMath::iLerp::GetValue() const {
    return vk;
}

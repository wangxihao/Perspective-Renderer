//
// Created by user on 1/25/2021.
//

/* Start Header -------------------------------------------------------
 * Copyright (C) 2020 DigiPen Institute of Technology.
 * Reproduction or disclosure of this file or its contents without the prior
 * written consent of DigiPen Institute of Technology is prohibited.
 * File Name: TextureSystem.cpp
 * Purpose: Lorem ipsum dolor sit amet, consectetur adipiscing elit.
 * Language: C++, G++
 * Platform: g++ (Ubuntu 9.3.0-10ubuntu2) 9.3, ThinkPad T430u, Nvidia GT 620M,
 *           OpenGL version string: 4.6.0 NVIDIA 390.138
 * Project: OpenGLFramework
 * Author: Roland Shum, roland.shum@digipen.edu
 * Creation date: 1/25/2021
 * End Header --------------------------------------------------------*/
#include "stdafx.h"
#include "TextureSystem.h"
#include "Logger.h"

// STBI
//#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG


#pragma region InternalFunctions

static GLuint CreateEmptyTextureObject() {
    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);

    // Set some defaults (min-filter is required)
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Set max anisotropy to largest supported value
    static GLfloat textureMaxAnisotropy = -1.0f;
    if (textureMaxAnisotropy == -1.0f) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &textureMaxAnisotropy);
    }
    glTextureParameterf(texture, GL_TEXTURE_MAX_ANISOTROPY, textureMaxAnisotropy);

    return texture;
}

void CreateMutableTextureFromPixel(GLuint texture, const uint8_t pixel[4]) {
    // Note that we can't use the bindless API for this since we need to resize the texture later,
    // which wouldn't be possible because that API only creates immutable textures (i.e. can't be resized)

    GLint lastBoundTexture2D;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastBoundTexture2D);
    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    }
    glBindTexture(GL_TEXTURE_2D, lastBoundTexture2D);
}

static void CreateImmutableTextureFromImage(const ImageLoadDescription &dsc, const LoadedImage &image) {
    // It's possible that mipmaps were requested, but now when we know the size we see that's not possible
    bool sameWidthAsHeight = image.width == image.height;
    bool powerOfTwoSize = (image.width & (image.width - 1)) == 0;
    bool generateMipmaps = dsc.requestMipmaps && sameWidthAsHeight && powerOfTwoSize;

    if (generateMipmaps) {
        int size = image.width;
        int numLevels = 1 + int(std::log2(size));

        glTextureParameteri(dsc.texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(dsc.texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTextureStorage2D(dsc.texture, numLevels, dsc.internalFormat, image.width, image.height);
        glTextureSubImage2D(dsc.texture, 0, 0, 0, image.width, image.height, dsc.format, image.type, image.pixels);
        glGenerateTextureMipmap(dsc.texture);

        // Since we now are using TAA (at least in most cases) add a -1 mip bias to sharpen everything a bit!
        glTextureParameterf(dsc.texture, GL_TEXTURE_LOD_BIAS, -1.0f);
    } else {
        glTextureParameteri(dsc.texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(dsc.texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTextureStorage2D(dsc.texture, 1, dsc.internalFormat, image.width, image.height);
        glTextureSubImage2D(dsc.texture, 0, 0, 0, image.width, image.height, dsc.format, image.type, image.pixels);
    }
}

#pragma endregion

void TextureSystem::Init() {
    // Basic setup
    stbi_set_flip_vertically_on_load(true);

    // Start the background thread for loading
    runBackgroundLoop = true;
    backgroundThread = std::thread([this]() {
        while (runBackgroundLoop) {
            ImageLoadDescription currentJob;
            {
                std::unique_lock<std::mutex> lock(accessMutex);
                while (pendingJobs.IsEmpty() && runBackgroundLoop) {
                    runCondition.wait(lock);
                }

                if (!runBackgroundLoop) {
                    return;
                }

                currentJob = pendingJobs.Pop();
            }

            const char *filename = currentJob.filename.c_str();
            LoadedImage image;

            // NOTE: If we add more threads for image loading, this check needs to be more rigorous!
            // We should never load an image if it's already loaded, but this can happen if we quickly
            // call some LoadImage function a second time before the first image has finished loading.
            if (loadedImages.find(filename) != loadedImages.end()) {
                std::lock_guard<std::mutex> lock(accessMutex);
                finishedJobs.Push(currentJob);
                continue;
            }

            if (currentJob.isHdr) {
                image.pixels = stbi_loadf(filename, &image.width, &image.height, nullptr, STBI_rgb);
                if (!image.pixels) {
                    Log("Could not load HDR image '%s': %s.\n", filename, stbi_failure_reason());
                    currentJobsCounter -= 1;
                    continue;
                }
                image.type = GL_FLOAT;
            } else {
                image.pixels = stbi_load(filename, &image.width, &image.height, nullptr, STBI_rgb_alpha);
                if (!image.pixels) {
                    Log("Could not load image '%s': %s.\n", filename, stbi_failure_reason());
                    currentJobsCounter -= 1;
                    continue;
                }
                image.type = GL_UNSIGNED_BYTE;
            }

            loadedImages[filename] = image;

            std::lock_guard<std::mutex> lock(accessMutex);
            finishedJobs.Push(currentJob);
        }
    });
}

void TextureSystem::Destroy() {
    // Shut down the background thread
    runBackgroundLoop = false;
    runCondition.notify_all();
    backgroundThread.join();

    // Release all loaded images (but NOT textures!)
    for (auto &nameImagePair: loadedImages) {
        stbi_image_free(nameImagePair.second.pixels);
    }
}

void TextureSystem::Update() {
    // This is the only place that consumes finished jobs, so a check like this should work fine and be thread safe. It might be possibile that
    // another thread will push a job and this thread doesn't notice it until later. That is okay, though, since this is called every frame.
    while (!finishedJobs.IsEmpty()) {
        ImageLoadDescription job = finishedJobs.Pop();
        const LoadedImage &image = loadedImages[job.filename];
        CreateImmutableTextureFromImage(job, image);
        currentJobsCounter -= 1;
    }
}

bool TextureSystem::IsIdle() {
    assert(currentJobsCounter >= 0);
    return currentJobsCounter == 0;
}

bool TextureSystem::IsHdrFile(const std::string &filename) {
    return stbi_is_hdr(filename.c_str());
}

GLuint TextureSystem::CreatePlaceholder(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    GLuint texture = CreateEmptyTextureObject();

    uint8_t pixel[4] = {r, g, b, a};
    CreateMutableTextureFromPixel(texture, pixel);

    return texture;
}

GLuint
TextureSystem::CreateTexture(int width, int height, GLenum format, GLenum minFilter, GLenum magFilter, bool useMips) {
    int numMips = 1;
    if (useMips) {
        int maxSize = std::max(width, height);
        numMips = 1 + int(std::floor(std::log2(maxSize)));
    }

    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, numMips, format, width, height);

    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, minFilter);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, magFilter);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texture;
}

GLuint TextureSystem::LoadDataTexture(const std::string &filename, GLenum internalFormat) {
    if (IsHdrFile(filename)) {
        Log("Texture file '%s' is an HDR image and must be loaded as such\n", filename.c_str());
    }

    ImageLoadDescription dsc;
    dsc.filename = filename;
    dsc.texture = CreateEmptyTextureObject();
    dsc.format = GL_RGBA;
    dsc.internalFormat = GL_SRGB8_ALPHA8;
    dsc.requestMipmaps = true;
    dsc.isHdr = false;

    if (loadedImages.find(filename) != loadedImages.end()) {
        // The file is already loaded into memory, just fill in the GPU texture data
        const LoadedImage &image = loadedImages[filename];
        CreateImmutableTextureFromImage(dsc, image);
    } else {
        currentJobsCounter += 1;

        // Fill texture with placeholder data and request an image load
        static uint8_t placeholderImageData[4] = {200, 200, 200, 255};
        CreateMutableTextureFromPixel(dsc.texture, placeholderImageData);

        pendingJobs.Push(dsc);
        runCondition.notify_all();
    }

    return dsc.texture;
}

GLuint TextureSystem::LoadCubeMap(const std::array<std::string, 6> &fileNames, GLenum internalFormat) {
    // Load all 6 cubes
    std::array<stbi_uc *, 6> faces;
    int x, y, c;
    for (int i = 0; i < 6; ++i) {
        faces[i] = stbi_load(fileNames[i].c_str(), &x, &y, &c, STBI_rgb_alpha);
    }

    const auto name = CreateTextureCube(GL_RGBA8, GL_RGBA, x, y, faces);

    for (auto face: faces) {
        stbi_image_free(face);
    }
    return name;
}


GLuint TextureSystem::CreateTextureCube(
        GLenum internal_format, GLenum format, GLsizei width, GLsizei height, std::array<stbi_uc *, 6> const &data
                                       ) {
    GLuint tex = 0;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex);
    glTextureStorage2D(tex, 1, internal_format, width, height);

    for (GLint i = 0; i < 6; ++i) {
        if (data[i]) {
            glTextureSubImage3D(tex, 0, 0, 0, i, width, height, 1, format, GL_UNSIGNED_BYTE, data[i]);
        }
    }

    return tex;
}
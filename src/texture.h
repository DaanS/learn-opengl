#ifndef TEXTURE_H
#define TEXTURE_H

#include <iostream>

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "model.h"

struct cubemap {
    GLuint id;

    cubemap(std::array<const std::string, 6> const & face_paths) {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, channel_count;
        for (size_t i = 0; i < face_paths.size(); ++i) {
            uint8_t * img_data = stbi_load(face_paths[i].c_str(), &width, &height, &channel_count, 4);
            if (!img_data) {
                std::cerr << "ERROR loading " << face_paths[i] << std::endl;
            }

            GLenum format = GL_RGBA;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, img_data);
            stbi_image_free(img_data);
        }
    }

    cubemap(size_t size) {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        for (size_t i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        }
    }

    cubemap(cubemap && other) {
        id = other.id;
        other.id = 0;
    }

    cubemap & operator=(cubemap && other) {
        std::swap(id, other.id);
        return *this;
    }

    cubemap(cubemap const & other) = delete;
    cubemap & operator=(cubemap const & other) = delete;

    ~cubemap() {
        glDeleteTextures(1, &id);
    }

    void activate(GLenum unit) {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id);
    }
};

struct texture {
    GLuint id;

    texture(char const * path) {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, channel_count;
        uint8_t * img_data = stbi_load(path, &width, &height, &channel_count, 4);
        if (!img_data) {
            std::cerr << "ERROR loading " << path << std::endl;
        }

        GLenum format = GL_RGBA;
        GLenum internal_format = format;
        if (std::string(path).find("nanosuit") != std::string::npos) internal_format = GL_SRGB_ALPHA;
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }

    texture(std::string const& path) : texture(path.c_str()) {}

    texture(texture && other) {
        id = other.id;
        other.id = 0;
    }

    texture & operator=(texture && other) {
        std::swap(id, other.id);
        return *this;
    }

    texture(texture const & other) = delete;
    texture & operator=(texture const & other) = delete;

    ~texture() {
        glDeleteTextures(1, &id);
    }

    void activate(GLenum unit) {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, id);
    }
};

#endif

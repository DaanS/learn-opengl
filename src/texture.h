#ifndef TEXTURE_H
#define TEXTURE_H

#include <iostream>

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

enum class texture_type { diffuse, specular };

struct texture {
    GLuint id;
    texture_type type;

    texture(char const * path, texture_type type = texture_type::diffuse)
        : type{type}
    {
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
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }

    texture(std::string const& path, texture_type type) : texture(path.c_str(), type) {}
    texture(std::string const& path) : texture(path.c_str()) {}

    texture(texture && other) {
        id = other.id;
        type = other.type;
        other.id = 0;
    }

    texture & operator=(texture && other) {
        std::swap(id, other.id);
        type = other.type;
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

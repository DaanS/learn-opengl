#ifndef TEXTURE_H
#define TEXTURE_H

#include <iostream>

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct texture {
    GLuint id;

    texture(char const * path) {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_set_flip_vertically_on_load(true);

        int width, height, channel_count;
        uint8_t * img_data = stbi_load(path, &width, &height, &channel_count, 0);
        if (!img_data) {
            std::cerr << "ERROR loading " << path << std::endl;
        }

        GLenum type = GL_RGB;
        switch (channel_count) {
            case 3: type = GL_RGB; break;
            case 4: type = GL_RGBA; break;
            default: std::cerr << "ERROR loading " << path << "; channel count is not 3 or 4" << std::endl;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0, type, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }

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

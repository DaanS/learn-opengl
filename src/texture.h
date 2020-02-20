#ifndef TEXTURE_H
#define TEXTURE_H

#include <array>
#include <iostream>

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct cubemap {
    GLuint id;

    cubemap(std::array<const std::string, 6> const & face_paths) {
        glActiveTexture(GL_TEXTURE0);
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
            GLenum internal_format = GL_SRGB_ALPHA;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, img_data);
            stbi_image_free(img_data);
        }
    }

    cubemap(size_t size) {
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        for (size_t i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
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

    void activate(GLenum unit) const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id);
        glActiveTexture(GL_TEXTURE0);
    }
};

struct texture {
    GLuint id;

    texture(std::string const& path, bool filter, bool srgb) {
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);

        float aniso = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &aniso);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, aniso);

        int width, height, channel_count;
        uint8_t * img_data = stbi_load(path.c_str(), &width, &height, &channel_count, 4);
        if (!img_data) {
            std::cerr << "ERROR loading " << path << std::endl;
        }

        GLenum format = GL_RGBA;
        GLenum internal_format = srgb ? GL_SRGB_ALPHA : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, img_data);
        if (filter) glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }

    texture(std::string const& path) : texture(path, true, true) { }

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

    void activate(GLenum unit) const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, id);
        glActiveTexture(GL_TEXTURE0);
    }
};

struct hdr {
    GLuint id;

    hdr(std::string const& path) {
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, channel_count;
        stbi_set_flip_vertically_on_load(true);
        float * img_data = stbi_loadf(path.c_str(), &width, &height, &channel_count, 0);
        if (!img_data) {
            std::cerr << "ERROR loading " << path << std::endl;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, img_data);
        stbi_image_free(img_data);
        stbi_set_flip_vertically_on_load(false);
    }

    hdr(hdr && other) {
        id = other.id;
        other.id = 0;
    }

    hdr & operator=(hdr && other) {
        std::swap(id, other.id);
        return *this;
    }

    hdr(hdr const & other) = delete;
    hdr & operator=(hdr const & other) = delete;

    ~hdr() {
        glDeleteTextures(1, &id);
    }

    void activate(GLenum unit) const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, id);
        glActiveTexture(GL_TEXTURE0);
    }
};

struct tex_params {
    std::string path;
    bool filter; // TODO remove support?
    bool srgb;

    bool operator==(tex_params const& other) const {
        return (path == other.path && filter == other.filter && srgb == other.srgb);
    }
};

template<typename TexType>
struct loader {

    static inline std::unordered_map<tex_params, std::shared_ptr<TexType>> loaded_textures;

    static std::shared_ptr<TexType> load(std::string path, bool srgb) {
        tex_params params{path, true, srgb};
        auto found = loaded_textures.find(params);
        if (found == loaded_textures.end()) {
            found = loaded_textures.emplace(std::pair(params, std::make_shared<texture>(path, true, srgb))).first;
        }
        return found->second;
    }
};

namespace std {
    template<>
    struct hash<tex_params> {
        size_t operator()(tex_params const& params) const noexcept {
            return hash_values(params.path, params.filter, params.srgb);
        }
    };
}

#endif

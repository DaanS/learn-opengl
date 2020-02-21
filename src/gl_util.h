#ifndef GL_UTIL_H
#define GL_UTIL_H

#include <functional>

#include "shader.h"
#include "model.h"
#include "texture.h"

struct vao {
    GLuint id;
    GLuint vbo;

    template<typename T, size_t Len>
    vao(T (&vertices)[Len], size_t stride, std::initializer_list<std::pair<size_t, size_t>> attribs) {
        glGenVertexArrays(1, &id);
        glBindVertexArray(id);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        size_t idx{0};
        for (auto attrib : attribs) {
            glVertexAttribPointer(idx, attrib.first, GL_FLOAT, GL_FALSE, stride * sizeof(T), (void *) (attrib.second * sizeof(T)));
            glEnableVertexAttribArray(idx);
            ++idx;
        }
    }

    void use() const {
        glBindVertexArray(id);
    }
};

struct light {
    std::string name;
    glm::vec3 pos;
    glm::vec3 dir;
    glm::vec3 color;
    glm::vec3 strength;
    float constant;
    float linear;
    float quadratic;

    void setup(shader_program const& program) const {
        program.set_uniform(name + ".pos", pos);
        program.set_uniform(name + ".dir", dir);
        program.set_uniform(name + ".ambient", strength.x * color);
        program.set_uniform(name + ".diffuse", strength.y * color);
        program.set_uniform(name + ".specular", strength.z * color);
        program.set_uniform(name + ".constant", constant);
        program.set_uniform(name + ".linear", linear);
        program.set_uniform(name + ".quadratic", quadratic);
    }
};

static const std::array<glm::vec3, 6> targets{
    glm::vec3( 1.0f,  0.0f,  0.0f),
    glm::vec3(-1.0f,  0.0f,  0.0f),
    glm::vec3( 0.0f,  1.0f,  0.0f),
    glm::vec3( 0.0f, -1.0f,  0.0f),
    glm::vec3( 0.0f,  0.0f,  1.0f),
    glm::vec3( 0.0f,  0.0f, -1.0f)
};

static const std::array<glm::vec3, 6> ups{
    glm::vec3( 0.0f, -1.0f,  0.0f),
    glm::vec3( 0.0f, -1.0f,  0.0f),
    glm::vec3( 0.0f,  0.0f,  1.0f),
    glm::vec3( 0.0f,  0.0f, -1.0f),
    glm::vec3( 0.0f, -1.0f,  0.0f),
    glm::vec3( 0.0f, -1.0f,  0.0f)
};

struct env_map {
    size_t size;
    GLuint fb;
    GLuint tex;
    GLuint rbo;

    env_map(size_t size) : size{size} {
        glActiveTexture(GL_TEXTURE0);

        glGenFramebuffers(1, &fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        for (size_t i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, tex, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void render(glm::vec3 pos, GLuint vp_ubo, std::function<void(glm::vec3)> render_func) const {
        glBindFramebuffer(GL_FRAMEBUFFER, fb);

        glm::mat4 cube_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
        glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(cube_proj));
        for (size_t face_idx = 0; face_idx < 6; ++face_idx) {
            glm::mat4 cube_view = glm::lookAt(pos, pos + targets[face_idx], ups[face_idx]);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(cube_view));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx, tex, 0);
            glViewport(0, 0, size, size);
            render_func(pos);
            glClear(GL_DEPTH_BUFFER_BIT);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void activate(shader_program const & program, std::string name, int unit) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glActiveTexture(GL_TEXTURE0);
        program.set_uniform(name, unit);
    }
};

struct reflection_map {
    size_t size;
    GLuint fb;
    GLuint tex;
    GLuint rbo;

    reflection_map(size_t size) : size{size} {
        glActiveTexture(GL_TEXTURE0);

        glGenFramebuffers(1, &fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        for (size_t i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP, tex, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void render(glm::vec3 pos, GLuint vp_ubo, std::function<void(glm::vec3, float)> render_func) const {
        glBindFramebuffer(GL_FRAMEBUFFER, fb);

        glm::mat4 cube_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
        glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(cube_proj));

        constexpr static size_t mip_levels = 5;
        for (size_t mip = 0; mip < mip_levels; ++mip) {
            size_t mip_size = size * std::pow(0.5, mip);

            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mip_size, mip_size);
            glViewport(0, 0, mip_size, mip_size);

            float roughness = static_cast<float>(mip) / static_cast<float>(mip_levels - 1);
            for (size_t face_idx = 0; face_idx < 6; ++face_idx) {
                glm::mat4 cube_view = glm::lookAt(pos, pos + targets[face_idx], ups[face_idx]);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(cube_view));
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx, tex, mip);
                render_func(pos, roughness);
                glClear(GL_DEPTH_BUFFER_BIT);
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void activate(shader_program const & program, std::string name, int unit) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glActiveTexture(GL_TEXTURE0);
        program.set_uniform(name, unit);
    }
};

struct dir_shadow_map {
    size_t size;
    GLuint fb;
    GLuint tex;

    dir_shadow_map(size_t size) : size{size} {
        glActiveTexture(GL_TEXTURE0);

        glGenFramebuffers(1, &fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void render(shader_program const & program, glm::mat4 light_space, std::vector<std::pair<model*, glm::mat4>> const & geometry) const {
        program.use();
        program.set_uniform("light_space", light_space);
        glViewport(0, 0, size, size);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glClear(GL_DEPTH_BUFFER_BIT);
        for (auto && object : geometry) {
            program.set_uniform("model", object.second);
            object.first->draw(program);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void activate(shader_program const& program, std::string name, int unit) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, tex);
        glActiveTexture(GL_TEXTURE0);
        program.set_uniform(name, unit);
    }
};

struct omni_shadow_map {
    size_t size;
    GLuint fb;
    GLuint tex;

    omni_shadow_map(size_t size) : size{size} {
        glActiveTexture(GL_TEXTURE0);

        glGenFramebuffers(1, &fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        for (size_t face_idx = 0; face_idx < 6; ++face_idx) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void render(shader_program const & program, glm::vec3 light_pos, float far, std::vector<std::pair<model*, glm::mat4>> const & geometry) const {
        glm::mat4 omni_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 400.0f);
        glViewport(0, 0, size, size);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0);
        glClear(GL_DEPTH_BUFFER_BIT);
        std::array<glm::mat4, 6> omni_views{
            glm::lookAt(light_pos, light_pos + glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)),
            glm::lookAt(light_pos, light_pos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)),
            glm::lookAt(light_pos, light_pos + glm::vec3( 0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)),
            glm::lookAt(light_pos, light_pos + glm::vec3( 0.0,-1.0, 0.0), glm::vec3(0.0, 0.0,-1.0)),
            glm::lookAt(light_pos, light_pos + glm::vec3( 0.0, 0.0, 1.0), glm::vec3(0.0,-1.0, 0.0)),
            glm::lookAt(light_pos, light_pos + glm::vec3( 0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0))
        };
        std::array<glm::mat4, omni_views.size()> omni_transforms;
        program.use();
        for (size_t i = 0; i < omni_views.size(); ++i) {
            omni_transforms[i] = omni_projection * omni_views[i];
            program.set_uniform("shadow_transforms[" + std::to_string(i) + "]", omni_transforms[i]);
        }

        program.set_uniforms("far", far, "light_pos", light_pos);
        for (auto && object : geometry) {
            program.set_uniform("model", object.second);
            object.first->draw(program);
        }
    }

    void activate(shader_program const & program, std::string name, int unit) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glActiveTexture(GL_TEXTURE0);
        program.set_uniform(name, unit);
    }
};

struct framebuffer {
    GLuint id;
    GLuint color_buf;
    GLuint rbo;
    size_t width;
    size_t height;

    framebuffer(size_t width, size_t height, bool use_rbo) : width{width}, height{height} {
        glGenFramebuffers(1, &id);
        glBindFramebuffer(GL_FRAMEBUFFER, id);

        glGenTextures(1, &color_buf);
        glBindTexture(GL_TEXTURE_2D, color_buf);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_buf, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (use_rbo) {
            glGenRenderbuffers(1, &rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    framebuffer(size_t width, size_t height) : framebuffer(width, height, true) {}

    framebuffer(GLuint id, GLuint color_buf, GLuint rbo, size_t width, size_t height) :
            id{id}, color_buf{color_buf}, rbo{rbo}, width{width}, height{height} {}

    void filter(GLenum min_mag_filter) {
        filter(min_mag_filter, min_mag_filter);
    }

    void filter(GLenum min_filter, GLenum mag_filter) {
        glBindTexture(GL_TEXTURE_2D, color_buf);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void activate_texture(shader_program const& program, std::string name, int unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, color_buf);
        glActiveTexture(GL_TEXTURE0);
        program.set_uniform(name, unit);
    }
};

void blit_buffer(framebuffer src, framebuffer dst, GLenum buffer) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src.id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.id);
    glReadBuffer(buffer);
    glBlitFramebuffer(0, 0, src.width, src.height, 0, 0, dst.width, dst.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void render_to_buffer(shader_program const& program, framebuffer dst, std::vector<GLuint> textures) {
    static float const quad_vertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    vao quad_vao(quad_vertices, 4, {{2, 0}, {2, 2}});

    glBindFramebuffer(GL_FRAMEBUFFER, dst.id);
    glViewport(0, 0, dst.width, dst.height);
    program.use();
    for (size_t i = 0; i < textures.size(); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    quad_vao.use();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_DEPTH_TEST);
}

void render_to_buffer(shader_program const& program, GLuint id, size_t width, size_t height, std::vector<GLuint> textures) {
    static float const quad_vertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    vao quad_vao(quad_vertices, 4, {{2, 0}, {2, 2}});

    glBindFramebuffer(GL_FRAMEBUFFER, id);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    program.use();
    for (size_t i = 0; i < textures.size(); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    glDisable(GL_DEPTH_TEST);
    quad_vao.use();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_DEPTH_TEST);
}

template<typename TexType>
void activate_texture(TexType const & tex, shader_program const & program, std::string name, int unit) {
    tex.activate(GL_TEXTURE0 + unit);
    program.set_uniform(name, unit);
}

#endif
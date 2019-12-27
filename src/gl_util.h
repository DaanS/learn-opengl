#ifndef GL_UTIL_H
#define GL_UTIL_H

#include "shader.h"
#include "model.h"
#include "texture.h"

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

    void render(glm::vec3 pos, shader_program const& program, GLuint vp_ubo, std::vector<model*> models, std::vector<glm::mat4> model_transforms) {
        glBindFramebuffer(GL_FRAMEBUFFER, fb);

        program.use();
        program.set_uniform("view_pos", pos);
        glm::mat4 cube_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
        glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(cube_proj));
        for (size_t face_idx = 0; face_idx < 6; ++face_idx) {
            glm::mat4 cube_view = glm::lookAt(pos, pos + targets[face_idx], ups[face_idx]);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(cube_view));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx, tex, 0);
            glViewport(0, 0, size, size);
            for (size_t model_idx = 0; model_idx < models.size(); ++model_idx) {
                program.set_uniform("model", model_transforms[model_idx]);
                models[model_idx]->draw(program);
            }
            glClear(GL_DEPTH_BUFFER_BIT);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void activate(shader_program const& program, std::string name, int unit) {
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

    void render(shader_program const& program, glm::mat4 light_space, std::vector<model*> models, std::vector<glm::mat4> model_transforms) {
        program.use();
        program.set_uniform("light_space", light_space);
        glViewport(0, 0, size, size);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glClear(GL_DEPTH_BUFFER_BIT);
        for (size_t i = 0; i < models.size(); ++i) {
            program.set_uniform("model", model_transforms[i]);
            models[i]->draw(program);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void activate(shader_program const& program, std::string name, int unit) {
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

    void render(shader_program const& program, glm::vec3 light_pos, float far, std::vector<model*> models, std::vector<glm::mat4> model_transforms) {
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
        for (size_t i = 0; i < models.size(); ++i) {
            program.set_uniform("model", model_transforms[i]);
            models[i]->draw(program);
        }
    }

    void activate(shader_program const& program, std::string name, int unit) {
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

    framebuffer(size_t width, size_t height) : width{width}, height{height} {
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

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    framebuffer(GLuint id, GLuint color_buf, GLuint rbo, size_t width, size_t height) :
            id{id}, color_buf{color_buf}, rbo{rbo}, width{width}, height{height} {}
};

void blit_buffer(framebuffer src, framebuffer dst, GLenum buffer) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src.id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.id);
    glReadBuffer(buffer);
    glBlitFramebuffer(0, 0, src.width, src.height, 0, 0, dst.width, dst.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

#endif
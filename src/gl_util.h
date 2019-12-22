#ifndef GL_UTIL_H
#define GL_UTIL_H

#include "shader.h"
#include "model.h"
#include "texture.h"

cubemap make_cubemap(glm::vec3 pos, size_t size, shader_program const& program, GLuint vp_ubo, std::vector<model*> models) {
    cubemap map{size};

    glm::mat4 model;

    glm::vec3 targets[6] = {
        glm::vec3( 1.0f,  0.0f,  0.0f),
        glm::vec3(-1.0f,  0.0f,  0.0f),
        glm::vec3( 0.0f,  1.0f,  0.0f),
        glm::vec3( 0.0f, -1.0f,  0.0f),
        glm::vec3( 0.0f,  0.0f,  1.0f),
        glm::vec3( 0.0f,  0.0f, -1.0f)
    };

    glm::vec3 ups[6] = {
        glm::vec3( 0.0f, -1.0f,  0.0f),
        glm::vec3( 0.0f, -1.0f,  0.0f),
        glm::vec3( 0.0f,  0.0f,  1.0f),
        glm::vec3( 0.0f,  0.0f, -1.0f),
        glm::vec3( 0.0f, -1.0f,  0.0f),
        glm::vec3( 0.0f, -1.0f,  0.0f)
    };

    program.use();
    program.set_uniform("view_pos", pos);
    glm::mat4 cube_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
    glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(cube_proj));
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -1.75f, 0.0f));
    model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));
    program.set_uniform("model", model);
    for (size_t i = 0; i < 6; ++i) {
        glm::mat4 cube_view = glm::lookAt(pos, pos + targets[i], ups[i]);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(cube_view));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, map.id, 0);
        glViewport(0, 0, 1024, 1024);
        for (auto model : models) model->draw(program);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    return map;
}

#endif
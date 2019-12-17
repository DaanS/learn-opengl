#include <cmath>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>

#include <glad/glad.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include "vertices.h"
#include "shader.h"
#include "model.h"
#include "texture.h"
#include "gl_util.h"

static unsigned int width = 1920;
static unsigned int height = 1080;

glm::vec3 camera_pos(0.0f, 25.0f, 0.0f);
glm::vec3 camera_front(1.0f, 0.0f, 0.0f);
glm::vec3 camera_up(0.0f, 1.0f, 0.0f);
float fov = 45.0f;

glm::vec3 light_pos(1.2f, 1.0f, 2.0f);

struct sdl_window {
    SDL_Window * window;
    SDL_GLContext context;
    bool running;

    sdl_window(unsigned int width, unsigned int height, char const * title) : running(true) {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
            std::cerr << "ERROR initializing SDL: " << SDL_GetError() << std::endl;
            std::exit(1);
        }

        window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);

        if (!window) {
            std::cerr << "ERROR creating SDL window: " << SDL_GetError() << std::endl;
            std::exit(1);
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

        context = SDL_GL_CreateContext(window);

        if (!context) {
            std::cerr << "ERROR creating OpenGL context: " << SDL_GetError() << std::endl;
            std::exit(1);
        }

        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    float get_time() {
        return (float) SDL_GetTicks() / 1000.0f;
    }

    void swap_buffer() {
        SDL_GL_SwapWindow(window);
    }

    void handle_events() {
        SDL_Event event;

        while(SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT: running = false; break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_ESCAPE: running = false; break;
                        default: break;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    handle_mouse(event.motion);
                    break;
                case SDL_MOUSEWHEEL:
                    handle_scroll(event.wheel);
                    break;
                default: break;
            }
        }

        uint8_t const * key_state = SDL_GetKeyboardState(nullptr);

        static float prev_time = get_time();
        float cur_time = get_time();
        float delta_time = cur_time - prev_time;
        prev_time = cur_time;
        float camera_speed = 50.0f * delta_time;

        glPolygonMode(GL_FRONT_AND_BACK, key_state[SDL_SCANCODE_F] ? GL_LINE : GL_FILL);

        if (key_state[SDL_SCANCODE_W]) camera_pos += camera_speed * camera_front;
        if (key_state[SDL_SCANCODE_S]) camera_pos -= camera_speed * camera_front;
        if (key_state[SDL_SCANCODE_A]) camera_pos -= camera_speed * glm::normalize(glm::cross(camera_front, camera_up));
        if (key_state[SDL_SCANCODE_D]) camera_pos += camera_speed * glm::normalize(glm::cross(camera_front, camera_up));
        if (key_state[SDL_SCANCODE_SPACE]) camera_pos += camera_speed * camera_up;
        if (key_state[SDL_SCANCODE_LCTRL]) camera_pos -= camera_speed * camera_up;
    }

    void handle_mouse(SDL_MouseMotionEvent e) {
        static float sensitivity = 0.05f;
        static float yaw = 90.0f;
        static float pitch = 0.0f;

        yaw += sensitivity * e.xrel;
        pitch += sensitivity * e.yrel;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        camera_front = glm::normalize(glm::vec3(
            std::cos(glm::radians(pitch)) * std::sin(glm::radians(yaw)),
            std::sin(glm::radians(pitch)),
            -std::cos(glm::radians(pitch)) * std::cos(glm::radians(yaw))
        ));

        camera_up = glm::normalize(glm::vec3(
            -std::sin(glm::radians(pitch)) * std::sin(glm::radians(yaw)),
            std::cos(glm::radians(pitch)),
            std::sin(glm::radians(pitch)) * std::cos(glm::radians(yaw))
        ));
    }

    void handle_scroll(SDL_MouseWheelEvent e) {
        fov -= e.y;

        if (fov > 90.0f) fov = 90.0f;
        if (fov < 30.0f) fov = 30.0f;
    }
};

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

    void use() {
        glBindVertexArray(id);
    }
};

struct light {
    std::string name;
    glm::vec3 pos;
    glm::vec3 dir;
    glm::vec3 color;
    float ambient;
    float diffuse;
    float specular;
    float constant;
    float linear;
    float quadratic;

    void setup(shader_program& program) {
        program.set_uniform(name + ".pos", pos);
        program.set_uniform(name + ".dir", dir);
        program.set_uniform(name + ".ambient", ambient * color);
        program.set_uniform(name + ".diffuse", diffuse * color);
        program.set_uniform(name + ".specular", specular * color);
        program.set_uniform(name + ".constant", constant);
        program.set_uniform(name + ".linear", linear);
        program.set_uniform(name + ".quadratic", quadratic);
    }
};

int main() {
    sdl_window window(width, height, "LearnOpenGL");

    if (!gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glm::vec3 point_light_pos[] = {
        glm::vec3(-124.0f, 25.0f, -44.0f),
        glm::vec3(-124.0f, 25.0f,  29.0f),
        glm::vec3(  97.5f, 25.0f,  29.0f),
        glm::vec3(  97.5f, 25.0f, -44.0f)
    };

    constexpr size_t POINT_LIGHT_COUNT = 4;

    shader_program program({{GL_VERTEX_SHADER, "src/shaders/main.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/main.frag"}});
    shader_program lamp({{GL_VERTEX_SHADER, "src/shaders/lamp.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/lamp.frag"}});
    shader_program post({{GL_VERTEX_SHADER, "src/shaders/post.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/post.frag"}});
    shader_program sky({{GL_VERTEX_SHADER, "src/shaders/sky.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/sky.frag"}});
    shader_program reflect({{GL_VERTEX_SHADER, "src/shaders/reflect.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/reflect.frag"}});
    shader_program explode({{GL_VERTEX_SHADER, "src/shaders/explode.vert"}, {GL_GEOMETRY_SHADER, "src/shaders/explode.geom"}, {GL_FRAGMENT_SHADER, "src/shaders/explode.frag"}});
    shader_program normals({{GL_VERTEX_SHADER, "src/shaders/normals.vert"}, {GL_GEOMETRY_SHADER, "src/shaders/normals.geom"}, {GL_FRAGMENT_SHADER, "src/shaders/lamp.frag"}});

    model sponza{"res/sponza/sponza.obj"};
    model nanosuit{"res/nanosuit/nanosuit.obj"};

    cubemap sky_map{{"res/skybox/right.jpg", "res/skybox/left.jpg", "res/skybox/top.jpg", "res/skybox/bottom.jpg", "res/skybox/front.jpg", "res/skybox/back.jpg"}};

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);

    glEnable(GL_MULTISAMPLE);

    vao lamp_vao(vertices, 8, {{3, 0}});
    vao sky_vao(skybox_vertices, 3, {{3, 0}});
    vao post_vao(post_vertices, 4, {{2, 0}, {2, 2}});
    vao cube_vao(vertices, 8, {{3, 0}, {3, 3}});
    vao points_vao(point_vertices, 5, {{2, 0}, {3, 2}});

    GLuint ms_fb;
    glGenFramebuffers(1, &ms_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, ms_fb);

    GLuint ms_color_buf;
    glGenTextures(1, &ms_color_buf);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms_color_buf);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_RGB, width, height, GL_TRUE);
    //glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, ms_color_buf, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    GLuint ms_rbo;
    glGenRenderbuffers(1, &ms_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, ms_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 8, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ms_rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: ms_fb lacking completeness" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    GLuint tex_color_buf;
    glGenTextures(1, &tex_color_buf);
    glBindTexture(GL_TEXTURE_2D, tex_color_buf);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_color_buf, 0);

    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint vp_ubo;
    glGenBuffers(1, &vp_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, vp_ubo);

    cubemap env_map(1024);
    bool first = true;

    while (window.running) {
        window.handle_events();

        //glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glEnable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_FRAMEBUFFER, ms_fb);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glStencilMask(0xFF);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glStencilMask(0x00);

        program.use();

        light dir_light{"dir_light", glm::vec3(0.0f), glm::vec3(-0.2f, -1.0f, 0.1f), glm::vec3(1.0f), 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        dir_light.setup(program);

        glm::vec3 point_light_color{1.0f, 0.5f, 0.0f};
        for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
            light point_light{"point_lights[" + std::to_string(i) + "]", point_light_pos[i], glm::vec3(0.0f), point_light_color, 0.05f, 0.8f, 1.0f, 1.0f, 0.014f, 0.0007f};
            point_light.setup(program);
        }

        light spot_light{"spot_light", camera_pos, camera_front, glm::vec3(1.0f), 0.0f, 1.0f, 1.0f, 1.0f, 0.045f, 0.0075f};
        spot_light.setup(program);
        program.set_uniforms("spot_light.cutoff", glm::cos(glm::radians(12.5f)), "spot_light.outer_cutoff", glm::cos(glm::radians(15.0f)));

        glm::mat4 model;

        // TODO find a better place/way to render this cubemap
        if (first) env_map = make_cubemap(glm::vec3(10.0f, 25.0f, 0.0f), 1024, program, vp_ubo, {&sponza});
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, ms_color_buf, 0);
        glViewport(0, 0, width, height);
        program.set_uniform("view_pos", camera_pos);
        program.set_uniform("camera_dir", camera_front);

        glm::mat4 projection = glm::perspective(glm::radians(fov), (float) width / (float) height, 0.1f, 1000.0f);
        glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);

        glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projection));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // draw room
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -1.75f, 0.0f));
        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));
        program.set_uniform("model", model);
        sponza.draw(program);

        // draw hair
        normals.use();
        normals.set_uniforms("model", model, "color", point_light_color);
        sponza.draw(normals);

        // draw outlined nanosuit
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(120.0f, -1.75f, 20.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));
        program.use();
        program.set_uniform("model", model);
        lamp.use();
        lamp.set_uniforms("model", model, "color", point_light_color);
        nanosuit.draw_outlined(program, lamp);

        // draw exploding nanosuit
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-120.0f, -1.75f, 20.0f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));
        explode.use();
        explode.set_uniforms("model", model, "time", window.get_time());
        nanosuit.draw(explode);

        // draw light placholders
        glDisable(GL_CULL_FACE);
        for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, point_light_pos[i]);
            model = glm::scale(model, glm::vec3(0.2f));

            lamp.use();
            lamp.set_uniforms("model", model, "color", point_light_color);

            lamp_vao.use();
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glEnable(GL_CULL_FACE);

        // draw magicube
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(10.0f, 25.0f, 0.0f));
        model = glm::scale(model, glm::vec3(5.0f));
        glDisable(GL_CULL_FACE);
        reflect.use();
        reflect.set_uniforms("model", model, "camera_pos", camera_pos);
        env_map.activate(GL_TEXTURE0);
        cube_vao.use();
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glEnable(GL_CULL_FACE);

        // draw skybox
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        sky.use();
        sky.set_uniforms("view", glm::mat4(glm::mat3(view)), "projection", projection, "tex", 0);
        sky_map.activate(GL_TEXTURE0);
        sky_vao.use();
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);

        // post-processing
        glBindFramebuffer(GL_READ_FRAMEBUFFER, ms_fb);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
        glDrawBuffer(GL_BACK);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        post.use();
        post.set_uniforms("width", static_cast<float>(width), "height", static_cast<float>(height));
        post_vao.use();
        glDisable(GL_DEPTH_TEST);
        glBindTexture(GL_TEXTURE_2D, tex_color_buf);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);

        window.swap_buffer();
        first = false;
    }

    return 0;
}

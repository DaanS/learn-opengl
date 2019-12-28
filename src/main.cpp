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

static glm::vec3 camera_pos(0.0f, 25.0f, 0.0f);
static glm::vec3 camera_front(1.0f, 0.0f, 0.0f);
static glm::vec3 camera_up(0.0f, 1.0f, 0.0f);
static float fov = 45.0f;
static float far = 1000.0f;

static bool draw_hair{false};
static bool draw_magicube{false};
static bool use_bloom{true};
static bool draw_outline_suit{false};
static bool use_spotlight{false};
static bool use_gamma{true};
static bool is_day{false};
static bool light_changed{true};
static const float gamma_strength{2.2f};

static float point_falloff = 0.0015f;
static float const point_falloff_delta = 0.0001f;

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
                        case SDL_SCANCODE_B: use_bloom = !use_bloom; break;
                        case SDL_SCANCODE_G: use_gamma = !use_gamma; break;
                        case SDL_SCANCODE_M: draw_magicube = !draw_magicube; break;
                        case SDL_SCANCODE_N: draw_hair = !draw_hair; break;
                        case SDL_SCANCODE_O: draw_outline_suit = !draw_outline_suit; break;
                        case SDL_SCANCODE_P: use_spotlight = !use_spotlight; break;
                        case SDL_SCANCODE_T: is_day = !is_day; light_changed = true; break;
                        case SDL_SCANCODE_KP_MINUS: point_falloff -= point_falloff_delta; std::cout << std::to_string(point_falloff) << std::endl; break;
                        case SDL_SCANCODE_KP_PLUS: point_falloff += point_falloff_delta; std::cout << std::to_string(point_falloff) << std::endl; break;
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
    shader_program g_pass({{GL_VERTEX_SHADER, "src/shaders/g_pass.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/g_pass.frag"}});
    shader_program lamp({{GL_VERTEX_SHADER, "src/shaders/lamp.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/lamp.frag"}});
    shader_program post({{GL_VERTEX_SHADER, "src/shaders/post.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/post.frag"}});
    shader_program pre_post({{GL_VERTEX_SHADER, "src/shaders/pre_post.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/pre_post.frag"}});
    shader_program blend({{GL_VERTEX_SHADER, "src/shaders/blend.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/blend.frag"}});
    shader_program sky({{GL_VERTEX_SHADER, "src/shaders/sky.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/sky.frag"}});
    shader_program reflect({{GL_VERTEX_SHADER, "src/shaders/reflect.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/reflect.frag"}});
    shader_program normals({{GL_VERTEX_SHADER, "src/shaders/normals.vert"}, {GL_GEOMETRY_SHADER, "src/shaders/normals.geom"}, {GL_FRAGMENT_SHADER, "src/shaders/lamp.frag"}});
    shader_program depth({{GL_VERTEX_SHADER, "src/shaders/depth.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/depth.frag"}});
    shader_program depth_cube({{GL_VERTEX_SHADER, "src/shaders/depth_cube.vert"}, {GL_GEOMETRY_SHADER, "src/shaders/depth_cube.geom"}, {GL_FRAGMENT_SHADER, "src/shaders/depth_cube.frag"}});

    model sponza{"res/sponza/sponza.obj"};
    model nanosuit{"res/nanosuit/nanosuit.obj"};

    cubemap sky_map{{"res/skybox/right.jpg", "res/skybox/left.jpg", "res/skybox/top.jpg", "res/skybox/bottom.jpg", "res/skybox/front.jpg", "res/skybox/back.jpg"}};
    cubemap star_map{{"res/starbox/right.png", "res/starbox/left.png", "res/starbox/top.png", "res/starbox/bottom.png", "res/starbox/front.png", "res/starbox/back.png"}};

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

    GLuint ms_fb;
    glGenFramebuffers(1, &ms_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, ms_fb);
    GLuint ms_color_buf;
    glGenTextures(1, &ms_color_buf);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms_color_buf);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_RGB16F, width, height, GL_TRUE);
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

    GLuint g_fb;
    glGenFramebuffers(1, &g_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fb);
    std::array<GLuint, 4> g_color_bufs;
    std::array<GLenum, g_color_bufs.size()> g_color_attachments;
    glGenTextures(g_color_bufs.size(), g_color_bufs.data());
    for (size_t i = 0; i < g_color_bufs.size(); ++i) {
        glBindTexture(GL_TEXTURE_2D, g_color_bufs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, g_color_bufs[i], 0);
        g_color_attachments[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(g_color_attachments.size(), g_color_attachments.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    GLuint g_rbo;
    glGenRenderbuffers(1, &g_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, g_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 8, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: ms_fb lacking completeness" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::array<framebuffer, 8> bloom_fbs{
        framebuffer(width, height),
        framebuffer(width / 2, height / 2),
        framebuffer(width / 4, height / 4),
        framebuffer(width / 8, height / 8),
        framebuffer(width / 16, height / 16),
        framebuffer(width / 32, height / 32),
        framebuffer(width / 64, height / 64),
        framebuffer(width / 128, height / 128)
    };

    std::array<framebuffer, 7> blend_fbs{
        framebuffer(width, height),
        framebuffer(width / 2, height / 2),
        framebuffer(width / 4, height / 4),
        framebuffer(width / 8, height / 8),
        framebuffer(width / 16, height / 16),
        framebuffer(width / 32, height / 32),
        framebuffer(width / 64, height / 64)
    };

    framebuffer pp_fb(width, height);

    dir_shadow_map dir_shadow{8192};

    omni_shadow_map omni_shadows[POINT_LIGHT_COUNT]{
        omni_shadow_map{2048},
        omni_shadow_map{2048},
        omni_shadow_map{2048},
        omni_shadow_map{2048}
    };

    GLuint vp_ubo;
    glGenBuffers(1, &vp_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, vp_ubo);

    env_map reflect_map(2048);
    bool first = true;

    while (window.running) {
        window.handle_events();

        //static float prev_time = window.get_time();
        //float cur_time = window.get_time();
        //std::cout << cur_time - prev_time << std::endl;
        //prev_time = cur_time;

        glEnable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_FRAMEBUFFER, ms_fb);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glStencilMask(0xFF);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glStencilMask(0x00);

        // set up light colors
        glm::vec3 sunlight{1.0f, 0.9f, 0.7f};
        if (use_gamma) sunlight = glm::pow(sunlight, glm::vec3(gamma_strength));

        glm::vec3 sunlight_dir{-0.2f, -1.0f, 0.1f};
        sunlight_dir = glm::normalize(sunlight_dir);

        glm::vec3 warm_orange{1.0f, 0.5f, 0.0f};
        if (use_gamma) warm_orange = glm::pow(warm_orange, glm::vec3(gamma_strength));

        glm::vec3 spot_light_color(1.0f);
        if (!use_spotlight) spot_light_color = glm::vec3(0.0f);
        if (use_gamma) spot_light_color = glm::pow(spot_light_color, glm::vec3(gamma_strength));

        program.use();

        // set up day/night colors and skybox
        cubemap* skybox = &sky_map;
        glm::vec3 point_light_color{warm_orange};
        glm::vec3 sunlight_color{sunlight};
        glm::vec3 sunlight_strength;

        if (is_day) {
            point_light_color = glm::vec3(0.0f);
            sunlight_strength = glm::vec3(0.002f, 3.0f, 1.0f);
        } else {
            sunlight_color = glm::vec3(1.0f, 12.0f / 14.0f, 13.0f/ 14.0f);
            sunlight_strength = glm::vec3(0.003f, 0.0f, 0.0f);

            skybox = &star_map;
        }

        // set up lights
        light dir_light{"dir_light", glm::vec3(0.0f), sunlight_dir, sunlight_color, sunlight_strength.x, sunlight_strength.y, sunlight_strength.z, 0.0f, 0.0f, 0.0f};
        dir_light.setup(program);

        for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
            light point_light{"point_lights[" + std::to_string(i) + "]", point_light_pos[i], glm::vec3(0.0f), point_light_color, 0.000f, 0.8f, 1.0f, 0.0f, 0.0f, point_falloff};
            point_light.setup(program);
        }

        light spot_light{"spot_light", camera_pos, camera_front, spot_light_color, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.03f};
        spot_light.setup(program);
        program.set_uniforms("spot_light.cutoff", glm::cos(glm::radians(12.5f)), "spot_light.outer_cutoff", glm::cos(glm::radians(15.0f)));

        // set up matrices
        glm::mat4 light_projection = glm::ortho(-350.0f, 420.0f, -230.0f, 250.0f, 10.0f, 600.0f);
        glm::mat4 light_view = glm::lookAt(-sunlight_dir * 500.0f, glm::vec3(0.0f), glm::vec3(0.2f, 1.0f, 0.0f));
        glm::mat4 light_space = light_projection * light_view;

        glm::mat4 projection = glm::perspective(glm::radians(fov), (float) width / (float) height, 0.1f, far);
        glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);
        glm::mat4 model;

        // prepare room
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -1.75f, 0.0f));
        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));

        program.use();
        program.set_uniforms("far", far, "light_space", light_space, "view_pos", camera_pos, "model", model);

        // TODO find a better place/way to render these cubemaps
        if (first || light_changed) {
            // draw directional shadow map
            dir_shadow.render(depth, light_space, {&sponza}, {model});

            // draw omni-directional shadow map
            for (size_t i =0; i < POINT_LIGHT_COUNT; ++i) {
                omni_shadows[i].render(depth_cube, point_light_pos[i], far, {&sponza}, {model});
            }

            // draw reflection map
            // TODO render the skybox to the cubemap as well
            program.use();
            program.set_uniforms("far", far, "light_space", light_space, "model", model);
            dir_shadow.activate(program, "dir_light.shadow_map", 6);
            for (size_t i =0; i < POINT_LIGHT_COUNT; ++i) {
                omni_shadows[i].activate(program, "point_lights[" + std::to_string(i) + "].shadow_cube", 7 + i);
            }
            reflect_map.render(glm::vec3(10.0f, 25.0f, 0.0f), program, vp_ubo, {&sponza}, {model});
            glViewport(0, 0, width, height);

            light_changed = false;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projection));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // draw room
        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, ms_fb);
        program.use();
        program.set_uniforms("far", far, "light_space", light_space, "view_pos", camera_pos, "model", model);
        dir_shadow.activate(program, "dir_light.shadow_map", 6);
        for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
            omni_shadows[i].activate(program, "point_lights[" + std::to_string(i) + "].shadow_cube", static_cast<int>(7 + i));
        }
        sponza.draw(program);


        // draw room (g-pass)
        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, g_fb);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        g_pass.use();
        g_pass.set_uniforms("light_space", light_space, "model", model);
        glDisable(GL_BLEND);
        sponza.draw(g_pass);
        glEnable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, ms_fb);

        // draw hair
        if (draw_hair) {
            normals.use();
            normals.set_uniforms("model", model, "color", warm_orange);
            sponza.draw(normals);
        }

        // draw outlined nanosuit
        if (draw_outline_suit) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(120.0f, -1.75f, 20.0f));
            model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));
            program.use();
            program.set_uniform("model", model);
            lamp.use();
            lamp.set_uniforms("model", model, "color", warm_orange);
            nanosuit.draw_outlined(program, lamp);
        }

        // draw light placholders
        if (!is_day) {
            glDisable(GL_CULL_FACE);
            for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, point_light_pos[i]);
                model = glm::scale(model, glm::vec3(1.2f));

                lamp.use();
                lamp.set_uniforms("model", model, "color", warm_orange);
                lamp_vao.use();
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, -sunlight_dir * 900.0f);
            model = glm::scale(model, glm::vec3(15.0f));

            lamp.use();
            lamp.set_uniforms("model", model, "color", sunlight);
            lamp_vao.use();
            glDrawArrays(GL_TRIANGLES, 0, 36);

            glEnable(GL_CULL_FACE);
        }

        // draw magicube
        if (draw_magicube) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(10.0f, 25.0f, 0.0f));
            model = glm::scale(model, glm::vec3(5.0f));
            glDisable(GL_CULL_FACE);
            reflect.use();
            reflect.set_uniforms("model", model, "camera_pos", camera_pos);
            reflect_map.activate(reflect, "tex", 0);
            cube_vao.use();
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glEnable(GL_CULL_FACE);
        }

        // draw skybox
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        sky.use();
        sky.set_uniforms("view", glm::mat4(glm::mat3(view)), "projection", projection, "tex", 0, "is_day", true);
        skybox->activate(GL_TEXTURE0);
        sky_vao.use();
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);

        // blit multisample FB to regular FB
        glBindFramebuffer(GL_READ_FRAMEBUFFER, ms_fb);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pp_fb.id);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // extract and downscale bloom
        pre_post.use();
        blend.set_uniform("tex", 0);
        render_to_buffer(pre_post, bloom_fbs[0], {pp_fb.color_buf});
        for (size_t i = 1; i < bloom_fbs.size(); ++i) {
            blit_buffer(bloom_fbs[i - 1], bloom_fbs[i], GL_COLOR_ATTACHMENT0);
        }

        // blur and upscale bloom
        constexpr static int bloom_levels = 4;
        for (int i = bloom_levels; i >= 0; --i) {
            blend.use();
            blend.set_uniforms("Large", 0, "small", 1);
            render_to_buffer(blend, blend_fbs[i], {bloom_fbs[i].color_buf, (i == bloom_levels ? bloom_fbs[i + 1] : blend_fbs[i + 1]).color_buf});
        }

        // render to screen FB
        // TODO look into temporal AA to reduce bloom shimmer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        post.use();
        post.set_uniforms("width", static_cast<float>(width), "height", static_cast<float>(height), "use_gamma", use_gamma, "gamma", gamma_strength, "exposure", 1.0f);
        post.set_uniforms("tex", 0, "bloom", 1, "use_bloom", use_bloom);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pp_fb.color_buf);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, blend_fbs[0].color_buf);
        post_vao.use();
        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);

        // debug render g-pass
        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //glViewport(0, 0, width, height);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //post.use();
        //post.set_uniforms("width", static_cast<float>(width), "height", static_cast<float>(height), "use_gamma", use_gamma, "gamma", gamma_strength, "exposure", 1.0f);
        //post.set_uniforms("tex", 0, "bloom", 1, "use_bloom", false);
        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, g_color_bufs[1]);
        //glActiveTexture(GL_TEXTURE1);
        //glBindTexture(GL_TEXTURE_2D, blend_fbs[0].color_buf);
        //post_vao.use();
        //glDisable(GL_DEPTH_TEST);
        //glDrawArrays(GL_TRIANGLES, 0, 6);
        //glEnable(GL_DEPTH_TEST);

        window.swap_buffer();

        if (first) first = false;
    }

    return 0;
}

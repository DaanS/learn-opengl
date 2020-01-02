#include <cmath>
#include <iostream>
#include <fstream>
#include <iterator>
#include <random>
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
static bool use_ao{true};
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
                        case SDL_SCANCODE_C: use_ao = !use_ao; break;
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

// TODO replace with dir_light.dir
glm::vec3 const sunlight_dir = glm::normalize(glm::vec3{-0.2f, -1.0f, 0.1f});

glm::vec3 const gamma_vec = glm::vec3(gamma_strength);

glm::vec3 const sunlight = glm::pow(glm::vec3{1.0f, 0.9f, 0.7f}, gamma_vec);
glm::vec3 const warm_orange = glm::pow(glm::vec3{1.0f, 0.5f, 0.0f}, gamma_vec);
glm::vec3 const spot_light_color = glm::pow(glm::vec3(1.0f), gamma_vec);

struct environment {
    static constexpr size_t point_light_count{4};

    glm::vec3 const point_light_pos[point_light_count] = {
        glm::vec3(-124.0f, 25.0f, -44.0f),
        glm::vec3(-124.0f, 25.0f,  29.0f),
        glm::vec3(  97.5f, 25.0f,  29.0f),
        glm::vec3(  97.5f, 25.0f, -44.0f)
    };

    const glm::mat4 light_projection = glm::ortho(-350.0f, 420.0f, -230.0f, 250.0f, 10.0f, 600.0f);
    const glm::mat4 light_view = glm::lookAt(-sunlight_dir * 500.0f, glm::vec3(0.0f), glm::vec3(0.2f, 1.0f, 0.0f));
    const glm::mat4 light_space = light_projection * light_view;

    light dir_light;
    light point_lights[point_light_count];
    light spot_light;

    cubemap* skybox;

    dir_shadow_map dir_shadow{8192};

    omni_shadow_map omni_shadows[point_light_count]{
        omni_shadow_map{2048},
        omni_shadow_map{2048},
        omni_shadow_map{2048},
        omni_shadow_map{2048}
    };

    env_map reflect_map{2048};

    float ev;

    void update(bool is_day, cubemap* skybox) {
        // set up day/night colors and skybox
        glm::vec3 point_light_color{warm_orange};
        glm::vec3 sunlight_color{sunlight};
        glm::vec3 sunlight_strength;
        glm::vec3 point_light_strength;

        if (is_day) {
            float ambient_ev = 8.0f;
            float diffuse_ev = 15.0f;
            float specular_ev = 16.0f;

            float ambient_str = 1.0f;
            float diffuse_str = pow(2.0f, diffuse_ev - ambient_ev) - ambient_str;
            float specular_str = pow(2.0f, specular_ev - ambient_ev) - ambient_str - diffuse_str;

            point_light_strength = glm::vec3(0.0f);
            sunlight_strength = glm::vec3(ambient_str, diffuse_str, specular_str);
            ev = -5.0;
        } else {
            float ambient_ev = -2.0f;
            float diffuse_ev = 5.0f;
            float specular_ev = 7.0f;

            float ambient_str = 1.0f;
            float diffuse_str = pow(2.0f, diffuse_ev - ambient_ev) - ambient_str;
            float specular_str = pow(2.0f, specular_ev - ambient_ev) - ambient_str - diffuse_str;

            point_light_strength = glm::vec3(0.0f, diffuse_str, specular_str);
            sunlight_strength = glm::vec3(ambient_str, 0.0f, 0.0f);
            ev = -7.0;
        }

        this->skybox = skybox;

        glm::vec3 spot_light_strength{0.0f, 1.0f, 1.0f};

        // set up lights
        dir_light = light{"dir_light", glm::vec3(0.0f), sunlight_dir, sunlight_color, sunlight_strength, 0.0f, 0.0f, 0.0f};

        for (size_t i = 0; i < point_light_count; ++i) {
            point_lights[i] = light{"point_lights[" + std::to_string(i) + "]", point_light_pos[i], glm::vec3(0.0f), point_light_color, point_light_strength, 0.0f, 0.0f, point_falloff};
        }

        spot_light = light{"spot_light", camera_pos, camera_front, spot_light_color, spot_light_strength, 0.0f, 0.0f, 0.03f};
    }

    void setup(shader_program const& program) const {
        dir_light.setup(program);
        for (auto& point_light : point_lights) point_light.setup(program);
        spot_light.setup(program);
        program.set_uniforms("spot_light.cutoff", glm::cos(glm::radians(12.5f)), "spot_light.outer_cutoff", glm::cos(glm::radians(15.0f)));
    }

    void render_maps(shader_program const& program, GLuint vp_ubo, std::vector<std::pair<model*, glm::mat4>> geometry) const;
};

void render_scene(environment const & env, glm::vec3 view_pos) {
    static const shader_program program({{GL_VERTEX_SHADER, "src/shaders/main.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/main.frag"}});
    static const shader_program sky({{GL_VERTEX_SHADER, "src/shaders/sky.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/sky.frag"}});
    static const shader_program lamp({{GL_VERTEX_SHADER, "src/shaders/lamp.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/lamp.frag"}});

    static const model sponza{"res/sponza/sponza.obj"};
    static const vao sky_vao(vertices, 8, {{3, 0}});
    static const vao lamp_vao(vertices, 8, {{3, 0}});

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -1.75f, 0.0f));
    model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));

    // draw room
    program.use();
    env.setup(program);
    program.set_uniforms("use_spotlight", use_spotlight, "far", far, "light_space", env.light_space, "view_pos", view_pos, "model", model);
    env.dir_shadow.activate(program, "dir_light.shadow_map", 6);
    for (size_t i = 0; i < env.point_light_count; ++i) {
        env.omni_shadows[i].activate(program, "point_lights[" + std::to_string(i) + "].shadow_cube", static_cast<int>(7 + i));
    }
    sponza.draw(program);

    // draw skybox
    sky.use();
    model = glm::translate(glm::mat4(1.0f), view_pos);
    sky.set_uniforms("model", model, "tex", 0, "is_day", true);
    env.skybox->activate(GL_TEXTURE0);
    sky_vao.use();
    glDepthMask(GL_FALSE);
    glCullFace(GL_FRONT);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);

    // draw light placeholders
    if (!is_day) {
        for (size_t i = 0; i < env.point_light_count; ++i) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, env.point_light_pos[i]);
            model = glm::scale(model, glm::vec3(1.2f));

            lamp.use();
            lamp.set_uniforms("model", model, "color", warm_orange);
            lamp_vao.use();
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    } else {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, -sunlight_dir * 900.0f);
        model = glm::scale(model, glm::vec3(15.0f));

        lamp.use();
        lamp.set_uniforms("model", model, "color", sunlight);
        lamp_vao.use();
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

void environment::render_maps(shader_program const& program, GLuint vp_ubo, std::vector<std::pair<model*, glm::mat4>> geometry) const {
    static const shader_program depth({{GL_VERTEX_SHADER, "src/shaders/depth.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/depth.frag"}});
    static const shader_program depth_cube({{GL_VERTEX_SHADER, "src/shaders/depth_cube.vert"}, {GL_GEOMETRY_SHADER, "src/shaders/depth_cube.geom"}, {GL_FRAGMENT_SHADER, "src/shaders/depth_cube.frag"}});

    glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(float), &ev);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // draw directional shadow map
    dir_shadow.render(depth, light_space, geometry);

    // draw omni-directional shadow map
    for (size_t i =0; i < point_light_count; ++i) {
        omni_shadows[i].render(depth_cube, point_light_pos[i], far, geometry);
    }

    // draw reflection map
    program.use();
    program.set_uniforms("use_spotlight", false, "far", far, "light_space", light_space, "model", geometry[0].second);
    dir_shadow.activate(program, "dir_light.shadow_map", 6);
    for (size_t i =0; i < point_light_count; ++i) {
        omni_shadows[i].activate(program, "point_lights[" + std::to_string(i) + "].shadow_cube", 7 + i);
    }
    reflect_map.render(glm::vec3{10.0f, 25.0f, 0.0f}, vp_ubo, [this](glm::vec3 pos) { render_scene(*this, pos); });
    glViewport(0, 0, width, height);
}

int main() {
    sdl_window window(width, height, "LearnOpenGL");

    if (!gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    shader_program program({{GL_VERTEX_SHADER, "src/shaders/main.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/main.frag"}});
    shader_program lamp({{GL_VERTEX_SHADER, "src/shaders/lamp.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/lamp.frag"}});
    shader_program post({{GL_VERTEX_SHADER, "src/shaders/post.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/post.frag"}});
    shader_program pre_post({{GL_VERTEX_SHADER, "src/shaders/pre_post.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/pre_post.frag"}});
    shader_program reflect({{GL_VERTEX_SHADER, "src/shaders/reflect.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/reflect.frag"}});

    static const shader_program depth({{GL_VERTEX_SHADER, "src/shaders/depth.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/depth.frag"}});
    static const shader_program depth_cube({{GL_VERTEX_SHADER, "src/shaders/depth_cube.vert"}, {GL_GEOMETRY_SHADER, "src/shaders/depth_cube.geom"}, {GL_FRAGMENT_SHADER, "src/shaders/depth_cube.frag"}});
    static const shader_program sky({{GL_VERTEX_SHADER, "src/shaders/sky.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/sky.frag"}});

    static const shader_program g_pass({{GL_VERTEX_SHADER, "src/shaders/g_pass.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/g_pass.frag"}});
    static const shader_program lit_pass({{GL_VERTEX_SHADER, "src/shaders/lit_pass.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/lit_pass.frag"}});
    static const shader_program ssao({{GL_VERTEX_SHADER, "src/shaders/ssao.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/ssao.frag"}});
    static const shader_program blur({{GL_VERTEX_SHADER, "src/shaders/blur.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/blur.frag"}});
    static const shader_program blend({{GL_VERTEX_SHADER, "src/shaders/blend.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/blend.frag"}});
    static const shader_program view_deb({{GL_VERTEX_SHADER, "src/shaders/view_deb.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/view_deb.frag"}});

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

    vao post_vao(post_vertices, 4, {{2, 0}, {2, 2}});
    vao cube_vao(vertices, 8, {{3, 0}, {3, 3}});

    GLuint g_fb;
    glGenFramebuffers(1, &g_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fb);
    std::array<GLuint, 6> g_color_bufs;
    std::array<GLenum, g_color_bufs.size()> g_color_attachments;
    glGenTextures(g_color_bufs.size(), g_color_bufs.data());
    for (size_t i = 0; i < g_color_bufs.size(); ++i) {
        glBindTexture(GL_TEXTURE_2D, g_color_bufs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
        std::cerr << "ERROR: g_fb lacking completeness" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::array<framebuffer, 8> bloom_fbs{
        framebuffer(width, height, false),
        framebuffer(width / 2, height / 2, false),
        framebuffer(width / 4, height / 4, false),
        framebuffer(width / 8, height / 8, false),
        framebuffer(width / 16, height / 16, false),
        framebuffer(width / 32, height / 32, false),
        framebuffer(width / 64, height / 64, false),
        framebuffer(width / 128, height / 128, false)
    };

    std::array<framebuffer, 7> blend_fbs{
        framebuffer(width, height, false),
        framebuffer(width / 2, height / 2, false),
        framebuffer(width / 4, height / 4, false),
        framebuffer(width / 8, height / 8, false),
        framebuffer(width / 16, height / 16, false),
        framebuffer(width / 32, height / 32, false),
        framebuffer(width / 64, height / 64, false)
    };

    framebuffer pp_fb(width, height);

    environment env;

    GLuint vp_ubo;
    glGenBuffers(1, &vp_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4) + sizeof(float), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, vp_ubo);

    std::uniform_real_distribution<float> random_float(0.0f, 1.0f);
    std::default_random_engine gen;
    static constexpr size_t ssao_samples = 64;
    std::array<glm::vec3, ssao_samples> ssao_kernel;
    for (size_t i = 0; i < ssao_samples; ++i) {
        glm::vec3 sample{
            random_float(gen) * 2.0f - 1.0f,
            random_float(gen) * 2.0f - 1.0f,
            random_float(gen)
        };
        sample = glm::normalize(sample) * random_float(gen);

        ssao_kernel[i] = sample;
    }

    static constexpr size_t ssao_noise_samples = 16;
    std::array<glm::vec3, ssao_noise_samples> ssao_noise;
    for (size_t i = 0; i < ssao_noise_samples; ++i) {
        glm::vec3 sample{
            random_float(gen) * 2.0f - 1.0f,
            random_float(gen) * 2.0f - 1.0f,
            0.0f
        };

        ssao_noise[i] = glm::normalize(sample);
    }

    GLuint noise_tex;
    glGenTextures(1, &noise_tex);
    glBindTexture(GL_TEXTURE_2D, noise_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, ssao_noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    framebuffer ssao_fb{width, height, false};
    ssao_fb.filter(GL_NEAREST);

    framebuffer ssao_blur_fb{width, height, false};
    ssao_blur_fb.filter(GL_NEAREST);

    bool first = true;

    while (window.running) {
        window.handle_events();

        //static float prev_time = window.get_time();
        //float cur_time = window.get_time();
        //std::cout << cur_time - prev_time << std::endl;
        //prev_time = cur_time;

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glStencilMask(0xFF);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glStencilMask(0x00);

        program.use();

        env.update(is_day, is_day ? &sky_map : &star_map);
        env.setup(program);

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
        program.set_uniforms("use_spotlight", use_spotlight, "far", far, "light_space", light_space, "view_pos", camera_pos, "model", model);

        // TODO find a better place/way to render these cubemaps
        if (first || light_changed) {
            env.render_maps(program, vp_ubo, {{&sponza, model}});

            light_changed = false;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projection));
        glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(float), &env.ev);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // draw room (g-pass)
        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, g_fb);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        g_pass.use();
        g_pass.set_uniforms("light_space", light_space, "model", model);
        glDisable(GL_BLEND);
        sponza.draw(g_pass);
        glEnable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // generate ssao
        ssao.use();
        std::vector<GLuint> ssao_textures;
        for (size_t i = 0; i < g_color_bufs.size(); ++i) {
            ssao_textures.push_back(g_color_bufs[i]);
            ssao.set_uniform("g_bufs[" + std::to_string(i) + "]", static_cast<int>(i));
        }
        ssao_textures.push_back(noise_tex);
        ssao.set_uniforms("noise", g_color_bufs.size());
        for (size_t i = 0; i < ssao_samples; ++i) {
            ssao.set_uniform("samples[" + std::to_string(i) + "]", ssao_kernel[i]);
        }
        render_to_buffer(ssao, ssao_fb, ssao_textures);

        // blur ssao
        blur.use();
        blur.set_uniform("tex", 0);
        render_to_buffer(blur, ssao_blur_fb, {ssao_fb.color_buf});

        // light g-pass
        lit_pass.use();
        env.setup(lit_pass);
        lit_pass.set_uniforms("light_space", light_space, "far", far, "use_spotlight", use_spotlight, "use_ao", use_ao, "view_pos", camera_pos);
        std::vector<GLuint> lit_textures;
        for (size_t i = 0; i < g_color_bufs.size(); ++i) {
            lit_textures.push_back(g_color_bufs[i]);
            lit_pass.set_uniform("g_bufs[" + std::to_string(i) + "]", static_cast<int>(i));
        }
        lit_textures.push_back(ssao_blur_fb.color_buf);
        lit_pass.set_uniform("ssao", static_cast<int>(g_color_bufs.size()));
        static constexpr int shadow_tex_idx = g_color_bufs.size() + 1;
        env.dir_shadow.activate(lit_pass, "dir_light.shadow_map", shadow_tex_idx);
        for (size_t i = 0; i < env.point_light_count; ++i) {
            env.omni_shadows[i].activate(lit_pass, "point_lights[" + std::to_string(i) + "].shadow_cube", static_cast<int>(shadow_tex_idx + 1 + i));
        }
        render_to_buffer(lit_pass, pp_fb, lit_textures);

        // blit g-pass depth and stencil buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_fb);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pp_fb.id);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, pp_fb.id);

        // draw skybox
        static const vao sky_vao(vertices, 8, {{3, 0}});
        sky.use();
        model = glm::translate(glm::mat4(1.0f), camera_pos);
        sky.set_uniforms("model", model, "tex", 0, "is_day", true);
        env.skybox->activate(GL_TEXTURE0);
        sky_vao.use();
        glDepthMask(GL_FALSE);
        glCullFace(GL_FRONT);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glCullFace(GL_BACK);
        glDepthMask(GL_TRUE);

        // draw light placeholders
        static const vao lamp_vao(vertices, 8, {{3, 0}});
        if (!is_day) {
            for (size_t i = 0; i < env.point_light_count; ++i) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, env.point_light_pos[i]);
                model = glm::scale(model, glm::vec3(1.2f));

                lamp.use();
                lamp.set_uniforms("model", model, "color", warm_orange);
                lamp_vao.use();
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        } else {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, camera_pos);
            model = glm::translate(model, -sunlight_dir * 900.0f);
            model = glm::scale(model, glm::vec3(15.0f));

            lamp.use();
            lamp.set_uniforms("model", model, "color", sunlight);
            lamp_vao.use();
            glDrawArrays(GL_TRIANGLES, 0, 36);
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
            nanosuit.draw(program);
        }

        // draw magicube
        if (draw_magicube) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(10.0f, 25.0f, 0.0f));
            model = glm::scale(model, glm::vec3(5.0f));
            reflect.use();
            reflect.set_uniforms("model", model, "camera_pos", camera_pos);
            env.reflect_map.activate(reflect, "tex", 0);
            cube_vao.use();
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // extract and downscale bloom
        pre_post.use();
        pre_post.set_uniforms("user_ev", env.ev, "tex", 0);
        render_to_buffer(pre_post, bloom_fbs[0], {pp_fb.color_buf});
        for (size_t i = 1; i < bloom_fbs.size(); ++i) {
            blit_buffer(bloom_fbs[i - 1], bloom_fbs[i], GL_COLOR_ATTACHMENT0);
        }

        // blur and upscale bloom
        constexpr static int bloom_levels = 4;
        for (int i = bloom_levels; i >= 0; --i) {
            blend.use();
            blend.set_uniforms("large", 0, "small", 1);
            render_to_buffer(blend, blend_fbs[i], {bloom_fbs[i].color_buf, (i == bloom_levels ? bloom_fbs[i + 1] : blend_fbs[i + 1]).color_buf});
        }

        // render to screen FB
        // TODO look into temporal AA to reduce bloom shimmer
        post.use();
        post.set_uniforms("width", static_cast<float>(width), "height", static_cast<float>(height), "gamma", gamma_strength, "exposure", 1.0f);
        post.set_uniforms("user_ev", env.ev, "tex", 0, "bloom", 1, "use_bloom", use_bloom, "DEBUG", false);
        render_to_buffer(post, 0, width, height, {pp_fb.color_buf, blend_fbs[0].color_buf});
        //post.set_uniforms("user_ev", env.ev, "tex", 0, "bloom", 1, "use_bloom", use_bloom, "DEBUG", true);
        //render_to_buffer(post, 0, width, height, {g_color_bufs[3], blend_fbs[0].color_buf});

        window.swap_buffer();

        if (first) first = false;
    }

    return 0;
}

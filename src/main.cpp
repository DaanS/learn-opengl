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

static bool draw_hair{false};
static bool draw_magicube{false};
static bool draw_explodo_suit{false};
static bool draw_outline_suit{false};
static bool use_spotlight{false};
static bool use_gamma{true};
static bool is_day{false};
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
                        case SDL_SCANCODE_B: draw_explodo_suit = !draw_explodo_suit; break;
                        case SDL_SCANCODE_G: use_gamma = !use_gamma; break;
                        case SDL_SCANCODE_M: draw_magicube = !draw_magicube; break;
                        case SDL_SCANCODE_N: draw_hair = !draw_hair; break;
                        case SDL_SCANCODE_O: draw_outline_suit = !draw_outline_suit; break;
                        case SDL_SCANCODE_P: use_spotlight = !use_spotlight; break;
                        case SDL_SCANCODE_T: is_day = !is_day; break;
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
    vao points_vao(point_vertices, 5, {{2, 0}, {3, 2}});

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
    
    GLuint pp_fb;
    glGenFramebuffers(1, &pp_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, pp_fb);
    GLuint pp_color_buf;
    glGenTextures(1, &pp_color_buf);
    glBindTexture(GL_TEXTURE_2D, pp_color_buf);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pp_color_buf, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    GLuint pp_rbo;
    glGenRenderbuffers(1, &pp_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, pp_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pp_rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int const shadow_size{8192};

    GLuint depth_fb;
    glGenFramebuffers(1, &depth_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, depth_fb);
    GLuint depth_buf;
    glGenTextures(1, &depth_buf);
    glBindTexture(GL_TEXTURE_2D, depth_buf);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, shadow_size, shadow_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_buf, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: framebuffer lacking completeness" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int const pos_shadow_size{2048};

    GLuint depth_cube_fb;
    glGenFramebuffers(1, &depth_cube_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, depth_cube_fb);
    GLuint depth_cube_buf[POINT_LIGHT_COUNT];
    glGenTextures(POINT_LIGHT_COUNT, depth_cube_buf);
    for (size_t light_idx = 0; light_idx < POINT_LIGHT_COUNT; ++ light_idx) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, depth_cube_buf[light_idx]);
        for (size_t face_idx = 0; face_idx < 6; ++face_idx) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx, 0, GL_DEPTH_COMPONENT32F, pos_shadow_size, pos_shadow_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
    }
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_cube_buf[0], 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
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

        glm::vec3 warm_orange{1.0f, 0.5f, 0.0f};
        if (use_gamma) warm_orange = glm::pow(warm_orange, glm::vec3(gamma_strength));

        glm::vec3 sunlight{1.0f, 0.9f, 0.7f};
        if (use_gamma) sunlight = glm::pow(sunlight, glm::vec3(gamma_strength));

        glm::vec3 sunlight_dir{-0.2f, -1.0f, 0.1f};
        sunlight_dir = glm::normalize(sunlight_dir);

        program.use();

        cubemap* skybox = &sky_map;
        glm::vec3 point_light_color{warm_orange};
        glm::vec3 sunlight_color{sunlight};
        glm::vec3 sunlight_strength;

        if (is_day) {
            point_light_color = glm::vec3(0.0f);
            sunlight_strength = glm::vec3(0.002f, 0.898f, 0.1f);
        } else {
            sunlight_color = glm::vec3(1.0f, 12.0f / 14.0f, 13.0f/ 14.0f);
            sunlight_strength = glm::vec3(0.0002f, 0.0f, 0.0f);

            skybox = &star_map;
        }

        light dir_light{"dir_light", glm::vec3(0.0f), sunlight_dir, sunlight_color, sunlight_strength.x, sunlight_strength.y, sunlight_strength.z, 0.0f, 0.0f, 0.0f};
        dir_light.setup(program);
        program.set_uniform("dir_light.shadow_map", 6);

        for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
            light point_light{"point_lights[" + std::to_string(i) + "]", point_light_pos[i], glm::vec3(0.0f), point_light_color, 0.001f, 0.8f, 1.0f, 0.0f, 0.0f, point_falloff};
            point_light.setup(program);
            program.set_uniform("point_lights[" + std::to_string(i) + "].shadow_cube", static_cast<int>(7 + i));
        }

        glm::vec3 spot_light_color(1.0f);
        if (!use_spotlight) spot_light_color = glm::vec3(0.0f);
        if (use_gamma) spot_light_color = glm::pow(spot_light_color, glm::vec3(gamma_strength));
        light spot_light{"spot_light", camera_pos, camera_front, spot_light_color, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.03f};
        spot_light.setup(program);
        program.set_uniforms("spot_light.cutoff", glm::cos(glm::radians(12.5f)), "spot_light.outer_cutoff", glm::cos(glm::radians(15.0f)));

        glm::mat4 model;

        // TODO find a better place/way to render this cubemap
        // TODO render the skybox to the cubemap as well
        if (draw_magicube && first) {
            env_map = make_cubemap(glm::vec3(10.0f, 25.0f, 0.0f), 1024, program, vp_ubo, {&sponza});
            first = false;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, ms_color_buf, 0);
            glViewport(0, 0, width, height);
        }

        program.set_uniform("view_pos", camera_pos);
        program.set_uniform("camera_dir", camera_front);

        glm::mat4 light_projection = glm::ortho(-350.0f, 420.0f, -230.0f, 250.0f, 10.0f, 600.0f);
        glm::mat4 light_view = glm::lookAt(-sunlight_dir * 500.0f, glm::vec3(0.0f), glm::vec3(0.2f, 1.0f, 0.0f));
        glm::mat4 light_space = light_projection * light_view;

        glm::mat4 projection = glm::perspective(glm::radians(fov), (float) width / (float) height, 0.1f, 1000.0f);
        glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);

        glBindBuffer(GL_UNIFORM_BUFFER, vp_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projection));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // prepare room
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -1.75f, 0.0f));
        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));

        // draw directional shadow map
        depth.use();
        depth.set_uniforms("light_space", light_space, "model", model);
        glViewport(0, 0, shadow_size, shadow_size);
        glBindFramebuffer(GL_FRAMEBUFFER, depth_fb);
        glClear(GL_DEPTH_BUFFER_BIT);
        sponza.draw(depth);
        glBindFramebuffer(GL_FRAMEBUFFER, ms_fb);
        glViewport(0, 0, width, height);

        // draw omni-directional shadow map
        glm::mat4 omni_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 400.0f);
        glViewport(0, 0, pos_shadow_size, pos_shadow_size);
        glBindFramebuffer(GL_FRAMEBUFFER, depth_cube_fb);
        for (size_t light_idx = 0; light_idx < POINT_LIGHT_COUNT; ++light_idx) {
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_cube_buf[light_idx], 0);
            glClear(GL_DEPTH_BUFFER_BIT);
            std::array<glm::mat4, 6> omni_views{
                glm::lookAt(point_light_pos[light_idx], point_light_pos[light_idx] + glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)),
                glm::lookAt(point_light_pos[light_idx], point_light_pos[light_idx] + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)),
                glm::lookAt(point_light_pos[light_idx], point_light_pos[light_idx] + glm::vec3( 0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)),
                glm::lookAt(point_light_pos[light_idx], point_light_pos[light_idx] + glm::vec3( 0.0,-1.0, 0.0), glm::vec3(0.0, 0.0,-1.0)),
                glm::lookAt(point_light_pos[light_idx], point_light_pos[light_idx] + glm::vec3( 0.0, 0.0, 1.0), glm::vec3(0.0,-1.0, 0.0)),
                glm::lookAt(point_light_pos[light_idx], point_light_pos[light_idx] + glm::vec3( 0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0))
            };
            std::array<glm::mat4, omni_views.size()> omni_transforms;
            depth_cube.use();
            for (size_t i = 0; i < omni_views.size(); ++i) {
                omni_transforms[i] = omni_projection * omni_views[i];
                depth_cube.set_uniform("shadow_transforms[" + std::to_string(i) + "]", omni_transforms[i]);
            }
            depth_cube.set_uniforms("far", 1000.0f, "light_pos", point_light_pos[light_idx], "model", model);
            sponza.draw(depth_cube);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, ms_fb);
        glViewport(0, 0, width, height);

        // draw room
        program.use();
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, depth_buf);
        for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
            glActiveTexture(GL_TEXTURE7 + i);
            glBindTexture(GL_TEXTURE_CUBE_MAP, depth_cube_buf[i]);
        }
        program.set_uniforms("far", 1000.0f, "shadow_map", 6, "light_space", light_space, "model", model);
        sponza.draw(program);

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

        // draw exploding nanosuit
        if (draw_explodo_suit) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-120.0f, -1.75f, 20.0f));
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));
            explode.use();
            explode.set_uniforms("model", model, "time", window.get_time());
            nanosuit.draw(explode);
        }

        // draw light placholders
        if (!is_day) {
            glDisable(GL_CULL_FACE);
            for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, point_light_pos[i]);
                model = glm::scale(model, glm::vec3(0.2f));

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
            model = glm::scale(model, glm::vec3(5.0f));

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
            env_map.activate(GL_TEXTURE0);
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

        // post-processing
        glBindFramebuffer(GL_READ_FRAMEBUFFER, ms_fb);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pp_fb);
        glDrawBuffer(GL_BACK);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        post.use();
        post.set_uniforms("width", static_cast<float>(width), "height", static_cast<float>(height), "use_gamma", use_gamma, "gamma", gamma_strength);
        post_vao.use();
        glDisable(GL_DEPTH_TEST);
        glBindTexture(GL_TEXTURE_2D, pp_color_buf);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);

        window.swap_buffer();
    }

    return 0;
}

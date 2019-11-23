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

#include "shader.h"
#include "model.h"
#include "texture.h"

static unsigned int width = 800;
static unsigned int height = 600;

glm::vec3 camera_pos(0.0f, 0.0f, 3.0f);
glm::vec3 camera_front(0.0f, 0.0f, -1.0f);
glm::vec3 camera_up(0.0f, 1.0f, 0.0f);
float fov = 45.0f;

glm::vec3 light_pos(1.2f, 1.0f, 2.0f);

#define ENABLE_MOUSE 1

struct sdl_window {
    SDL_Window * window;
    SDL_GLContext context;
    bool running;

    sdl_window(unsigned int width, unsigned int height, char const * title) : running(true) {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
            std::cerr << "ERROR initializing SDL: " << SDL_GetError() << std::endl;
            std::exit(1);
        }

        window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL);

        if (!window) {
            std::cerr << "ERROR creating SDL window: " << SDL_GetError() << std::endl;
            std::exit(1);
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

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
#ifdef ENABLE_MOUSE
                case SDL_MOUSEMOTION:
                    handle_mouse(event.motion);
                    break;
#endif
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
        float camera_speed = 2.5f * delta_time;

        glPolygonMode(GL_FRONT_AND_BACK, key_state[SDL_SCANCODE_F] ? GL_LINE : GL_FILL);

        if (key_state[SDL_SCANCODE_W]) camera_pos += camera_speed * camera_front;
        if (key_state[SDL_SCANCODE_S]) camera_pos -= camera_speed * camera_front;
        if (key_state[SDL_SCANCODE_A]) camera_pos -= camera_speed * glm::normalize(glm::cross(camera_front, camera_up));
        if (key_state[SDL_SCANCODE_D]) camera_pos += camera_speed * glm::normalize(glm::cross(camera_front, camera_up));
        if (key_state[SDL_SCANCODE_SPACE]) camera_pos += camera_speed * camera_up;
        if (key_state[SDL_SCANCODE_LCTRL]) camera_pos -= camera_speed * camera_up;
    }

#ifdef ENABLE_MOUSE
    void handle_mouse(SDL_MouseMotionEvent e) {
        static float sensitivity = 0.05f;
        static float yaw = 0.0f;
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
    }
#endif

    void handle_scroll(SDL_MouseWheelEvent e) {
        fov -= e.y;

        if (fov > 90.0f) fov = 90.0f;
        if (fov < 30.0f) fov = 30.0f;
    }
};

std::string load_string(char const * path) {
    std::ifstream ifs(path);
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

int main() {
    sdl_window window(width, height, "LearnOpenGL");

    if (!gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    std::vector<vertex> vertices{
        // positions          // normals           // texture coords
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f},  {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f},  {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f},  {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f},  {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f},  {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f},  {0.0f, 0.0f}},

        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f},  {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f},  {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f},  {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f},  {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f},  {0.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f},  {0.0f, 0.0f}},

        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f},  {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f},  {1.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f},  {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f},  {0.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f},  {0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f},  {1.0f, 0.0f}},

        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f},  {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f},  {1.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f},  {0.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f},  {0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f},  {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f},  {1.0f, 0.0f}},

        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f},  {0.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f},  {1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f},  {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f},  {1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f},  {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f},  {0.0f, 1.0f}},

        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f},  {0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f},  {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f},  {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f},  {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f},  {0.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f},  {0.0f, 1.0f}}
    };

    glm::vec3 point_light_pos[] = {
        glm::vec3( 0.7f,  0.2f,  2.0f),
        glm::vec3( 2.3f, -3.3f, -4.0f),
        glm::vec3(-4.0f,  2.0f, -12.0f),
        glm::vec3( 0.0f,  0.0f, -3.0f)
    };

    constexpr size_t POINT_LIGHT_COUNT = 4;

    shader_program program({{GL_VERTEX_SHADER, "src/main.vert"}, {GL_FRAGMENT_SHADER, "src/main.frag"}});
    //shader_program program({{GL_VERTEX_SHADER, "src/suit.vert"}, {GL_FRAGMENT_SHADER, "src/suit.frag"}});
    shader_program lamp({{GL_VERTEX_SHADER, "src/main.vert"}, {GL_FRAGMENT_SHADER, "src/lamp.frag"}});

    model suit{"res/nanosuit/nanosuit.obj"};
    std::vector<GLuint> indices;
    for (size_t i = 0; i < 36; ++i) indices.push_back(i);
    std::vector<texture> textures;
    textures.push_back({"res/container2.png"});
    textures.push_back({"res/container2_specular.png", texture_type::specular});
    mesh container{std::move(vertices), std::move(indices), std::move(textures)};

    glEnable(GL_DEPTH_TEST);

    std::cout << GL_TEXTURE0 << std::endl;

    while (window.running) {
        window.handle_events();

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float) width / (float) height, 0.1f, 100.0f);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -1.75f, 0.0f));
        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));

        program.use();
        program.set_uniform("model", model);
        program.set_uniform("view", view);
        program.set_uniform("projection", projection);
        program.set_uniform("view_pos", camera_pos);
        program.set_uniform("shininess", 32.0f);

        program.set_uniform("dir_light.dir", glm::vec3(-0.2f, -1.0f, -0.3f));
        program.set_uniform("dir_light.ambient", glm::vec3(0.05f, 0.05f, 0.05f));
        program.set_uniform("dir_light.diffuse", glm::vec3(0.4f, 0.4f, 0.4f));
        program.set_uniform("dir_light.specular", glm::vec3(0.5f, 0.5f, 0.5f));

        for (size_t i = 0; i < POINT_LIGHT_COUNT; ++i) {
            std::string name = "point_lights[" + std::to_string(i) + "]";
            program.set_uniform(name + ".pos", point_light_pos[i]);
            program.set_uniform(name + ".ambient", glm::vec3(0.05f, 0.05f, 0.05f));
            program.set_uniform(name + ".diffuse", glm::vec3(0.8f, 0.8f, 0.8f));
            program.set_uniform(name + ".specular", glm::vec3(1.0f, 1.0f, 1.0f));
            program.set_uniform(name + ".constant", 1.0f);
            program.set_uniform(name + ".linear", 0.09f);
            program.set_uniform(name + ".quadratic", 0.032f);
        }

        program.set_uniform("spot_light.pos", camera_pos);
        program.set_uniform("spot_light.dir", camera_front);
        program.set_uniform("spot_light.ambient", glm::vec3(0.0f, 0.0f, 0.0f));
        program.set_uniform("spot_light.diffuse", glm::vec3(1.0f, 1.0f, 1.0f));
        program.set_uniform("spot_light.specular", glm::vec3(1.0f, 1.0f, 1.0f));
        program.set_uniform("spot_light.constant", 1.0f);
        program.set_uniform("spot_light.linear", 0.09f);
        program.set_uniform("spot_light.quadratic", 0.032f);
        program.set_uniform("spot_light.cutoff", glm::cos(glm::radians(12.5f)));
        program.set_uniform("spot_light.outer_cutoff", glm::cos(glm::radians(15.0f)));

        suit.draw(program);
        container.draw(program);

        window.swap_buffer();
    }

    return 0;
}

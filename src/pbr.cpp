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

glm::vec3 camera_pos(0.0f, 0.0f, 20.0f);
glm::vec3 camera_front(0.0f, 0.0f, -1.0f);
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
        float camera_speed = 20.0f * delta_time;

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

void render_sphere() {
    static GLuint sphere_vao = 0;
    static unsigned int index_count;

    if (sphere_vao == 0) {
        glGenVertexArrays(1, &sphere_vao);

        unsigned int vbo, ebo;
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uv;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        const float PI = 3.14159265359;
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
                float x_segment = (float)x / (float)X_SEGMENTS;
                float y_segment = (float)y / (float)Y_SEGMENTS;
                float x_pos = std::cos(x_segment * 2.0f * PI) * std::sin(y_segment * PI);
                float y_pos = std::cos(y_segment * PI);
                float z_pos = std::sin(x_segment * 2.0f * PI) * std::sin(y_segment * PI);

                positions.push_back(glm::vec3(x_pos, y_pos, z_pos));
                uv.push_back(glm::vec2(x_segment, y_segment));
                normals.push_back(glm::vec3(x_pos, y_pos, z_pos));
            }
        }

        bool odd_row = false;
        for (size_t y = 0; y < Y_SEGMENTS; ++y) {
            if (!odd_row) {
                for (size_t x = 0; x <= X_SEGMENTS; ++x) {
                    indices.push_back(y       * (X_SEGMENTS + 1) + x);
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                }
            } else {
                for (int x = X_SEGMENTS; x >= 0; --x) {
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    indices.push_back(y       * (X_SEGMENTS + 1) + x);
                }
            }
            odd_row = !odd_row;
        }
        index_count = indices.size();

        std::vector<float> data;
        for (size_t i = 0; i < positions.size(); ++i) {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            if (uv.size() > 0) {
                data.push_back(uv[i].x);
                data.push_back(uv[i].y);
            }
            if (normals.size() > 0) {
                data.push_back(normals[i].x);
                data.push_back(normals[i].y);
                data.push_back(normals[i].z);
            }
        }

        glBindVertexArray(sphere_vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        float stride = (3 + 2 + 3) * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    }

    glBindVertexArray(sphere_vao);
    glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, 0);
}

int main() {
    sdl_window window(width, height, "LearnOpenGL");

    if (!gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    static const glm::vec3 sphere_color = glm::pow(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(2.2f));
    static const shader_program program{{GL_VERTEX_SHADER, "src/shaders/pbr.vert"}, {GL_FRAGMENT_SHADER, "src/shaders/pbr.frag"}};
    static const glm::vec3 light_positions[] = {
        glm::vec3(-10.0f,  10.0f, 10.0f),
        glm::vec3( 10.0f,  10.0f, 10.0f),
        glm::vec3(-10.0f, -10.0f, 10.0f),
        glm::vec3( 10.0f, -10.0f, 10.0f),
    };
    static const glm::vec3 light_colors[] = {
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 000.0f),
        glm::vec3(000.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 000.0f, 300.0f)
    };

    light point_light{"point_lights[" + std::to_string(0) + "]", light_positions[0], glm::vec3(0.0f), light_colors[0], glm::vec3(0.1f, 0.7f, 0.2f), 0.0f, 0.0f, 0.002f};

    glEnable(GL_DEPTH_TEST);

    while (window.running) {
        window.handle_events();

        //static float prev_time = window.get_time();
        //float cur_time = window.get_time();
        //std::cout << cur_time - prev_time << std::endl;
        //prev_time = cur_time;

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        program.use();

        glm::mat4 projection = glm::perspective(glm::radians(fov), (float) width / (float) height, 0.1f, 1000.0f);
        glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);
        program.set_uniforms("projection", projection, "view", view, "view_pos", camera_pos);

        program.set_uniforms("material.albedo", sphere_color, "material.roughness", 40.0f, "material.ao", 1.0f);
        for (size_t i = 0; i < 4; ++i) {
            program.set_uniforms("point_lights[" + std::to_string(i) + "].pos", light_positions[i],
                                 "point_lights[" + std::to_string(i) + "].color", light_colors[i]);

        }

        static constexpr int rows = 7;
        static constexpr int cols = 7;
        static constexpr float spacing = 2.5;
        glm::mat4 model = glm::mat4(1.0f);
        for (int row = 0; row < rows; ++row) {
            program.set_uniform("material.metallic", (float) row / (float) rows);
            for (int col = 0; col < cols; ++col) {
                program.set_uniform("material.roughness", glm::clamp((float) col / (float) cols, 0.05f, 1.0f));

                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3{
                    (col - (cols / 2)) * spacing,
                    (row - (rows / 2)) * spacing,
                    0.0f
                });
                program.set_uniform("model", model);
                render_sphere();
            }
        }

        window.swap_buffer();
    }

    return 0;
}

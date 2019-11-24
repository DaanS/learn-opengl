#ifndef SHADER_H
#define SHADER_H

#include <fstream>
#include <iostream>
#include <vector>

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "util.h"

struct shader {
    GLuint id;

    template<typename PathType>
    shader(GLenum type, PathType path) {
        id = glCreateShader(type);

        std::string src = load_string(path);
        char const * src_str = src.c_str();
        glShaderSource(id, 1, &src_str, NULL);
        glCompileShader(id);

        GLint info_log_len;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &info_log_len);

        if (info_log_len > 0) {
            std::vector<char> msg;
            msg.reserve(info_log_len);
            glGetShaderInfoLog(id, info_log_len, NULL, msg.data());
            std::cout << "SHADER info log for " << path << ":" << std::endl;
            std::cout << msg.data() << std::endl;
        }
    }

    shader(shader && other) {
        id = other.id;
        other.id = 0;
    }

    shader & operator=(shader && other) {
        std::swap(id, other.id);
        return *this;
    }

    shader(shader const & other) = delete;
    shader & operator=(shader const & other) = delete;

    ~shader() {
        glDeleteShader(id);
    }
};

struct shader_program {
    GLuint id;

    shader_program(std::initializer_list<shader> shaders) {
        id = glCreateProgram();

        for (auto && s : shaders) {
            glAttachShader(id, s.id);
        }
        glLinkProgram(id);

        GLint info_log_len;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &info_log_len);

        if (info_log_len > 0) {
            std::vector<char> msg;
            msg.reserve(info_log_len);
            glGetProgramInfoLog(id, info_log_len, NULL, msg.data());
            std::cout << "PROGRAM info log:" << std::endl;
            std::cout << msg.data() << std::endl;
        }
    }

    shader_program(shader_program && other) {
        id = other.id;
        other.id = 0;
    }

    shader_program & operator=(shader_program && other) {
        std::swap(id, other.id);
        return *this;
    }

    shader_program(shader_program const & other) = delete;
    shader_program & operator=(shader_program const & other) = delete;

    ~shader_program() {
        glDeleteProgram(id);
    }

    void use() const {
        glUseProgram(id);
    }

    template<typename T>
    void set_uniform(GLchar const * name, T t) const {
        GLint uniform = glGetUniformLocation(id, name);
        if (uniform < 0) {
            //std::cerr << "ERROR finding uniform " << name << std::endl;
            //std::exit(1);
            return;
        }

        if constexpr (std::is_same_v<T, int> || std::is_same_v<T, bool>) {
            glUniform1i(uniform, t);
        } else if constexpr (std::is_same_v<T, size_t>) {
            glUniform1ui(uniform, t);
        } else if constexpr (std::is_same_v<T, float>) {
            glUniform1f(uniform, t);
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            glUniform3fv(uniform, 1, glm::value_ptr(t));
        } else if constexpr (std::is_same_v<T, glm::mat4>) {
            glUniformMatrix4fv(uniform, 1, GL_FALSE, glm::value_ptr(t));
        } else {
            std::cerr << "ERROR deducing type for uniform " << name << std::endl;
            std::exit(1);
        }
    }

    template<typename T>
    void set_uniform(std::string const& name, T t) const {
        set_uniform(name.c_str(), std::forward<T>(t));
    }
};

#endif

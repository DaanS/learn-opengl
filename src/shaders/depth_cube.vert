#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 2) in vec2 tex_coords;

uniform mat4 model;

out vec2 geom_tex_coords;

void main() {
    gl_Position = model * vec4(pos, 1.0);
    geom_tex_coords = tex_coords;
}
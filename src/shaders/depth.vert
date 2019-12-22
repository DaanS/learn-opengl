#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 2) in vec2 tex_coords;

uniform mat4 light_space;
uniform mat4 model;

out vec2 frag_tex_coords;

void main() {
    gl_Position = light_space * model * vec4(pos, 1.0);
    frag_tex_coords = tex_coords;
}
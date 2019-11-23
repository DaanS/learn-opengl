#version 330 core
layout (location = 0) in vec3 vert_pos;
layout (location = 1) in vec3 vert_normal;
layout (location = 2) in vec2 vert_tex_coords;

out vec2 tex_coords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    tex_coords = vert_tex_coords;
    gl_Position = projection * view * model * vec4(vert_pos, 1.0);
}

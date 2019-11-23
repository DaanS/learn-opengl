#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 in_tex_coords;

out vec3 vert_normal;
out vec3 frag_pos;
out vec2 tex_coords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(pos, 1.0);
    frag_pos = vec3(model * vec4(pos, 1.0));
    vert_normal = mat3(transpose(inverse(model))) * normal;
    tex_coords = in_tex_coords;
}

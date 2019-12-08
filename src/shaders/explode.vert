#version 420 core
layout (location = 0) in vec3 pos;
layout (location = 2) in vec2 tex_coords;

out vec2 geom_tex_coords;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
};

uniform mat4 model;

void main() {
    gl_Position = projection * view * model * vec4(pos, 1.0);
    geom_tex_coords = tex_coords;
}
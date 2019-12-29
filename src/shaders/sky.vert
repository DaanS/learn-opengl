#version 420 core
layout (location = 0) in vec3 pos;

out vec3 frag_tex_coords;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
};

uniform mat4 model;

void main() {
    frag_tex_coords = pos;
    gl_Position = (projection * view * model * vec4(pos, 1.0)).xyww;
}
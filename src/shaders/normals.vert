#version 420 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;

out vec3 geom_normal;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
};

uniform mat4 model;

void main() {
    gl_Position = projection * view * model * vec4(pos, 1.0);
    geom_normal = normalize(vec3(projection * vec4(mat3(transpose(inverse(view * model))) * normal, 0.0)));
}
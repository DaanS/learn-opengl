#version 420 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
};

out vec3 frag_pos;
out vec3 frag_normal;

uniform mat4 model;

void main() {
    frag_normal = mat3(transpose(inverse(model))) * normal;
    frag_pos = vec3(model * vec4(pos, 1.0));
    gl_Position = projection * view * model * vec4(pos, 1.0);
}
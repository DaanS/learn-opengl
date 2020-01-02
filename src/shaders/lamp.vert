#version 420 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coords;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
    float user_ev;
};

out vec3 frag_normal;
out vec3 frag_pos;
out vec2 frag_tex_coords;
out mat3 tbn;

uniform mat4 model;

out vec3 deb;

void main() {
    gl_Position = projection * view * model * vec4(pos + normal * 0.1, 1.0);
}

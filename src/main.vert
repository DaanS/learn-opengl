#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coords;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

out vec3 frag_normal;
out vec3 frag_pos;
out vec2 frag_tex_coords;
out mat3 tbn;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(pos, 1.0);
    frag_pos = vec3(model * vec4(pos, 1.0));
    frag_normal = mat3(transpose(inverse(model))) * normal;
    frag_tex_coords = tex_coords;

    vec3 t = normalize(vec3(model * vec4(tangent, 0.0)));
    vec3 b = normalize(vec3(model * vec4(bitangent, 0.0)));
    vec3 n = normalize(vec3(model * vec4(normal, 0.0)));

    if (dot(cross(t, b), n) >= 0) b *= 1;

    tbn = mat3(t, b, n);
}

#version 420 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coords;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
};

out vec3 frag_normal;
out vec3 frag_pos;
out vec2 frag_tex_coords;
out mat3 tbn;

uniform mat4 model;

out vec3 deb;

void main() {
    gl_Position = projection * view * model * vec4(pos, 1.0);
    frag_pos = vec3(view * model * vec4(pos, 1.0));
    frag_normal = transpose(inverse(mat3(view * model))) * normal;
    frag_tex_coords = tex_coords;

    vec3 t = normalize(vec3(view * model * vec4(tangent, 0.0)));
    vec3 b = normalize(vec3(view * model * vec4(bitangent, 0.0)));
    vec3 n = normalize(vec3(view * model * vec4(normal, 0.0)));

    float flip = 1.0;
    if (dot(cross(t, b), n) <= 0) flip = -1.0;
    t = normalize(t - dot(t, n) * n);
    b = cross(n, t) * flip;

    tbn = mat3(t, b, n);
}
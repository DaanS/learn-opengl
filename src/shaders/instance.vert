#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 2) in vec2 tex_coords;
layout (location = 3) in mat4 instance_matrix;

out vec2 frag_tex_coords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform bool is_instanced;

void main() {
    if (is_instanced) {
        gl_Position = projection * view * instance_matrix * vec4(pos, 1.0);
    } else {
        gl_Position = projection * view * model * vec4(pos, 1.0);
    }
    frag_tex_coords = tex_coords;
}
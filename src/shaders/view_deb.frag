#version 420 core

in vec2 frag_tex_coords;

out vec4 frag_color;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
};

uniform sampler2D tex;

void main() {
    vec3 val = texture(tex, frag_tex_coords).rgb;
    frag_color = inverse(view) * vec4(val, 1.0);
}
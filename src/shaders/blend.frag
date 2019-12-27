#version 330 core

in vec2 frag_tex_coords;

out vec4 frag_color;

uniform sampler2D large;
uniform sampler2D small;

const float weights[] = float[] (0.06136, 0.24477, 0.38774, 0.24477, 0.06136);

const int kernel_size = 5;
const int half_kernel_size = kernel_size / 2;

void main() {
    vec2 texel_size = 1.0 / textureSize(small, 0);

    vec3 color = vec3(texture(large, frag_tex_coords));

    for (int x = -half_kernel_size; x <= half_kernel_size; ++x) {
        for (int y = -half_kernel_size; y <= half_kernel_size; ++y) {
            vec3 sample = vec3(texture(small, frag_tex_coords + texel_size * vec2(x, y)));
            float mult = weights[x + half_kernel_size] * weights[y + half_kernel_size];
            color += mult * sample;
        }
    }
    frag_color = vec4(color, 1.0);
}
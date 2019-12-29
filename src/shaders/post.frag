#version 330 core
in vec2 frag_tex_coords[9];

out vec4 frag_color;

uniform sampler2D tex;
uniform sampler2D bloom;

uniform bool use_gamma;
uniform float gamma;
uniform float exposure;
uniform bool use_bloom;

const float radius = 1.0;
const float offset = 0.5 * (radius / 450.0);

const float weights[] = float[] (0.06136, 0.24477, 0.38774, 0.24477, 0.06136);
const int kernel_size = 5;
const int half_kernel_size = kernel_size / 2;

vec3 bloom_color(vec2 tex_coords) {
    vec2 texel_size = 1.0 / textureSize(bloom, 0);

    vec3 color = vec3(0.0);

    for (int x = -half_kernel_size; x <= half_kernel_size; ++x) {
        for (int y = -half_kernel_size; y <= half_kernel_size; ++y) {
            vec3 sample = vec3(texture(bloom, tex_coords + texel_size * vec2(x, y)));
            float mult = weights[x + half_kernel_size] * weights[y + half_kernel_size];
            color += mult * sample;
        }
    }
    return color;
}

void main() {
    vec3 color = texture(tex, frag_tex_coords[4]).rgb;
    if (use_bloom) color += bloom_color(frag_tex_coords[4]);

    // tone mapping
    color = vec3(1.0) - exp(-color * exposure);

    // gamma correction
    color = pow(color, vec3(1.0 / gamma));

    frag_color = vec4(color, 1.0);
    //frag_color = vec4(bloom_color(frag_tex_coords[4]), 1.0);
}
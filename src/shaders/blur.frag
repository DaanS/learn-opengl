#version 330 core

in vec2 frag_tex_coords;

out vec4 frag_color;

uniform sampler2D tex;

void main() {
    vec2 texel_size = 1.0 / textureSize(tex, 0);

    float result = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = (0.5 + vec2(float(x), float(y))) * texel_size;
            result += texture(tex, frag_tex_coords + offset).r;
        }
    }

    frag_color = vec4(vec3(result / float(4 * 4)), 1.0);
}
#version 330 core
in vec2 frag_tex_coords[9];

out vec4 frag_color;

uniform sampler2D tex;

const float radius = 1.0;
const float offset = 0.5 * (radius / 450.0);

const float sharpen[9] = float[9](-1, -1, -1, -1, 9, -1, -1, -1, -1);
const float blur[9] = float[9](1, 2, 1, 2, 4, 2, 1, 2, 1);
const float edge[9] = float[9](1, 1, 1, 1, -9, 1, 1, 1, 1);

void main() {
    //frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    //for (int i = 0; i < 9; i++) {
    //    frag_color += vec4(edge[i] * vec3(texture(tex, frag_tex_coords[i])), 1.0);
    //}
    frag_color = texture(tex, frag_tex_coords[4]);
}
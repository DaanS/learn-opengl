#version 330 core
in vec3 frag_tex_coords;

out vec4 frag_color;

uniform bool is_day;

uniform samplerCube tex;

void main() {
    frag_color = texture(tex, frag_tex_coords);
    if (!is_day) {
        frag_color.rgb = pow(frag_color.rgb, vec3(1.6));
    }
}
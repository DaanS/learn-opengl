#version 330 core
in vec3 frag_tex_coords;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 bright_color;

uniform bool is_day;

uniform samplerCube tex;

void main() {
    frag_color = texture(tex, frag_tex_coords);
    if (!is_day) {
        frag_color.rgb = pow(frag_color.rgb, vec3(1.6));
    }

    float brightness = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0) bright_color = frag_color;
    else bright_color = vec4(vec3(0.0), 1.0);
}
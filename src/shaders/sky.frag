#version 420 core
in vec3 frag_tex_coords;

out vec4 frag_color;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
    float user_ev;
};

uniform bool is_day;

uniform samplerCube tex;

void main() {
    vec3 sky_color = vec3(texture(tex, frag_tex_coords)) * pow(2.0, -user_ev);
    frag_color = vec4(sky_color, 1.0);
    if (!is_day) {
        frag_color.rgb = pow(frag_color.rgb, vec3(1.6));
    }
}
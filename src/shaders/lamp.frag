#version 420 core

uniform vec3 color;

out vec4 frag_color;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
    float user_ev;
};

void main() {
    //frag_color = vec4(color, 1.0);
    //frag_color = vec4(1.8 * pow(vec3(color), vec3(1.0 / 2.2)), 1.0);
    vec3 lamp_color = 4.0 * color * pow(2.0, -user_ev);
    frag_color = vec4(lamp_color, 1.0);
}

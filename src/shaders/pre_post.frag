#version 420 core

in vec2 frag_tex_coords;

out vec4 bright_color;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
    float user_ev;
};

uniform sampler2D tex;

const float soft_threshold = 0.5;

void main() {
    vec3 frag_color = vec3(texture(tex, frag_tex_coords));

    float brightness = max(frag_color.r, max(frag_color.g, frag_color.b));

    float threshold = pow(2.0, -user_ev);
    float knee = threshold * soft_threshold;
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0, 2 * knee);
    soft = soft * soft / (4 * knee + 0.00001);

    float factor = max(soft, brightness - threshold);
    factor /= max(brightness, 0.00001);
    bright_color = vec4(factor * frag_color, 1.0);
}
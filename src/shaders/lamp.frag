#version 330 core

uniform vec3 color;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 bright_color;

void main() {
    //frag_color = vec4(color, 1.0);
    frag_color = vec4(1.8 * pow(vec3(color), vec3(1.0 / 2.2)), 1.0);

    float brightness = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0) bright_color = frag_color;
    else bright_color= vec4(vec3(0.0), 1.0);
}

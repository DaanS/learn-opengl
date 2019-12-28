#version 330 core

uniform vec3 color;

out vec4 frag_color;

void main() {
    //frag_color = vec4(color, 1.0);
    frag_color = vec4(1.8 * pow(vec3(color), vec3(1.0 / 2.2)), 1.0);
}

#version 330 core

in vec2 vert_tex_coord;

out vec4 frag_color;

uniform sampler2D texture0;
uniform sampler2D texture1;

void main() {
    frag_color = mix(texture(texture0, vert_tex_coord), texture(texture1, vert_tex_coord), 0.2);
}

#version 420 core

in vec2 frag_tex_coords;

out float frag_color;

void main() {
    frag_color = frag_tex_coords.y;
}

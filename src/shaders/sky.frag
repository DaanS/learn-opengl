#version 330 core
in vec3 frag_tex_coords;

out vec4 frag_color;

uniform samplerCube tex;

void main() {
    frag_color = texture(tex, frag_tex_coords);
    //frag_color = vec4(1.0);
}
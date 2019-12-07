#version 330 core

in vec3 frag_pos;
in vec3 frag_normal;

out vec4 frag_color;

uniform vec3 camera_pos;
uniform samplerCube sky;

void main() {
    vec3 view_dir = normalize(frag_pos - camera_pos);
    vec3 reflect_dir = reflect(view_dir, normalize(frag_normal));

    frag_color = vec4(texture(sky, reflect_dir).rgb, 1.0);
}
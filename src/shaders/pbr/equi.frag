#version 420 core
in vec3 frag_pos;

out vec4 frag_color;

uniform sampler2D tex;

const float PI = 3.14159265359;
const vec2 atan_reciprocal = vec2(1.0 / (2.0 * PI), 1.0 / PI);

vec2 spherical_uv(vec3 V) {
    vec2 uv = vec2(atan(V.z, V.x), asin(V.y));
    uv *= atan_reciprocal;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = spherical_uv(normalize(frag_pos));
    vec3 color = texture(tex, uv).rgb;
    frag_color = vec4(color, 1.0);
}
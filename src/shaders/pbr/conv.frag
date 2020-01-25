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

vec3 irradiance(vec3 V) {
    vec3 N = normalize(V);
    vec3 res = vec3(0.0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float sample_delta = 0.025;
    int sample_count = 0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sample_delta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sample_delta) {
            vec3 tan_sample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sample_vec = tan_sample.x * right + tan_sample.y * up + tan_sample.z * N;

            vec2 sample_spherical = spherical_uv(sample_vec);

            res += texture(tex, sample_spherical).rgb * cos(theta) * sin(theta);
            sample_count++;
        }
    }

    res = PI * res * (1.0 / float(sample_count));
    return res;
}

void main() {
    frag_color = vec4(irradiance(frag_pos), 1.0);
}
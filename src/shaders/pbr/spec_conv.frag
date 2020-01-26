#version 420 core
in vec3 frag_pos;

out vec4 frag_color;

uniform samplerCube tex;

uniform float roughness;
uniform int src_size;

const float PI = 3.14159265359;
const int SAMPLE_COUNT = 1024;

const vec2 atan_reciprocal = vec2(1.0 / (2.0 * PI), 1.0 / PI);
vec2 spherical_uv(vec3 V) {
    vec2 uv = vec2(atan(V.z, V.x), asin(V.y));
    uv *= atan_reciprocal;
    uv += 0.5;
    return uv;
}

float radical_inverse_vdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radical_inverse_vdc(i));
}

vec3 importance_sample_ggx(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cos_theta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    vec3 H = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = normalize(cross(N, tangent));

    vec3 sample_vec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sample_vec);
}

float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom_base = (NdotH2 * (a2 - 1.0) + 1.0);
    float denom = PI * denom_base * denom_base;

    return num / denom;
}

void main() {
    vec3 N = normalize(frag_pos);
    vec3 R = N;
    vec3 V = R;

    float total_weight = 0.0;
    vec3 color = vec3(0.0);

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H = importance_sample_ggx(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);

            float D = distribution_ggx(N, H, roughness);
            float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001;

            float resolution = float(src_size);
            float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            float mip = (roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel));

            //color += texture(tex, spherical_uv(L)).rgb * NdotL;
            color += textureLod(tex, L, mip).rgb * NdotL;
            total_weight += NdotL;
        }
    }
    color = color / total_weight;

    frag_color = vec4(color, 1.0);
}
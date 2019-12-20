#version 330 core

struct dir_light_type {
    vec3 dir;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct point_light_type {
    vec3 pos;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct spot_light_type {
    vec3 pos;
    vec3 dir;

    float cutoff;
    float outer_cutoff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct material_type {
    float shininess;
    float refraction;
    float opacity_value;

    vec3 color_ambient;
    vec3 color_diffuse;
    vec3 color_specular;
    vec3 color_emissive;
    vec3 color_transport;

    sampler2D diffuse;
    sampler2D specular;
    sampler2D emissive;
    sampler2D bump;
    sampler2D normal;
    sampler2D opacity;

    bool has_diffuse_map;
    bool has_specular_map;
    bool has_emissive_map;
    bool has_bump_map;
    bool has_normal_map;
    bool has_opacity_map;
};

in vec3 frag_pos;
in vec4 frag_pos_light_space;
in vec3 frag_normal;
in vec2 frag_tex_coords;
in mat3 tbn;

in vec3 deb;

out vec4 frag_color;

uniform vec3 view_pos;

uniform dir_light_type dir_light;
#define POINT_LIGHT_COUNT 4
uniform point_light_type point_lights[POINT_LIGHT_COUNT];
uniform spot_light_type spot_light;

uniform material_type material;
uniform sampler2D shadow_map;

float shadow_strength(vec4 frag_pos_light_space, vec3 normal, vec3 light_dir) {
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    if (proj_coords.z > 1.0) return 0.0;

    float bias = max(0.005 * (1.0 - dot(normalize(frag_normal), light_dir)), 0.0005);

    float shadow = 0.0;
    vec2 texel_size = 1.0 / textureSize(shadow_map, 0);
    int dir_sample_size = 3;
    int dir_ext = dir_sample_size / 2;
    for (int x = -dir_ext; x <= dir_ext; ++x) {
        for (int y = -dir_ext; y <= dir_ext; ++y) {
            float closest_depth = texture(shadow_map, proj_coords.xy + vec2(x, y) * texel_size).r;
            shadow += (proj_coords.z - bias > closest_depth ? 1.0 : 0.0);
        }
    }
    shadow /= dir_sample_size * dir_sample_size;

    return shadow;
}

vec3 calc_base_light(vec3 ambient, vec3 diffuse, vec3 specular, vec3 light_dir, bool use_shadow) {
    vec3 normal;
    if (material.has_normal_map) {
        normal = vec3(texture(material.normal, frag_tex_coords));
        normal = normalize(normal * 2.0 - 1.0);
        normal = normalize(tbn * normal);
    } else {
        normal = normalize(frag_normal);
    }
    //return normal * 0.5 + 0.5;

    vec3 ambient_color = ambient * (material.has_diffuse_map ? vec3(texture(material.diffuse, frag_tex_coords)) : material.color_ambient);

    float diff_strength = max(dot(normal, light_dir), 0.0);
    vec3 diff_color = diff_strength * diffuse * (material.has_diffuse_map ? vec3(texture(material.diffuse, frag_tex_coords)) : material.color_diffuse);

    vec3 view_dir = normalize(view_pos - frag_pos);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec_strength = pow(max(dot(normal, halfway_dir), 0.0), 2 * material.shininess);
    vec3 spec_color = spec_strength * specular * (material.has_specular_map ? vec3(texture(material.specular, frag_tex_coords)) : material.color_specular);

    vec3 em_color = (material.has_emissive_map ? vec3(texture(material.emissive, frag_tex_coords)) : vec3(0.0));

    float shadow = use_shadow ? shadow_strength(frag_pos_light_space, normal, light_dir) : 0.0f;

    return max(em_color, ambient_color + (1.0 - shadow) * (diff_color + spec_color));
}

vec3 calc_dir_light(dir_light_type light) {
    vec3 light_dir = normalize(-light.dir);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, true);

    return result;
}

vec3 calc_point_light(point_light_type light) {
    vec3 light_dir = normalize(light.pos - frag_pos);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, false);

    float dist = length(light.pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    result *= attenuation;
    return result;
}

vec3 calc_spot_light(spot_light_type light) {
    vec3 light_dir = normalize(light.pos - frag_pos);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, false);

    float dist = length(light.pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    float theta = dot(light_dir, normalize(-light.dir));
    float intensity = clamp((theta - light.outer_cutoff) / (light.cutoff - light.outer_cutoff), 0.0, 1.0);

    result *= attenuation * intensity;
    return result;
}

void main() {
    if (material.has_opacity_map) {
        vec4 tex_color = texture(material.opacity, frag_tex_coords);
        if (tex_color.r < 0.1) discard;
    } else {
        vec4 tex_color = texture(material.diffuse, frag_tex_coords);
        //if (tex_color.a < 0.1) discard;
    }

    vec3 result = calc_dir_light(dir_light);
    for (int i  = 0; i < POINT_LIGHT_COUNT; ++i) result += calc_point_light(point_lights[i]);
    result += calc_spot_light(spot_light);

    frag_color = vec4(result, 1.0);

    //frag_color = vec4(deb, 1.0);
}

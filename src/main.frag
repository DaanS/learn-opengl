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

uniform bool normalize_normal;

vec3 calc_base_light(vec3 ambient, vec3 diffuse, vec3 specular, vec3 light_dir) {
    vec3 normal;
    if (material.has_normal_map) {
        normal = vec3(texture(material.normal, frag_tex_coords));
        if (normalize_normal) normal = normalize(normal);
        normal = normalize(normal * 2.0 - 1.0);
        normal = normalize(tbn * normal);
    } else {
        normal = normalize(frag_normal);
    }
    //return normal * 0.5 + 0.5;

    vec3 view_dir = normalize(view_pos - frag_pos);

    vec3 ambient_color = ambient * vec3(texture(material.diffuse, frag_tex_coords));

    float diff_strength = max(dot(normal, light_dir), 0.0);
    vec3 diff_color = diff_strength * diffuse * vec3(texture(material.diffuse, frag_tex_coords));

    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec_strength = pow(max(dot(normal, halfway_dir), 0.0), 2 * material.shininess);
    vec3 spec_color;
    if (material.has_specular_map) {
        spec_color = spec_strength * specular * vec3(texture(material.specular, frag_tex_coords));
    } else {
        spec_color = spec_strength * specular * material.color_specular;
    }

    vec3 em_color = vec3(0.0);
    if (material.has_emissive_map) {
        em_color = vec3(texture(material.emissive, frag_tex_coords));
    }

    return max(em_color, ambient_color + diff_color + spec_color);
}

vec3 calc_dir_light(dir_light_type light) {
    vec3 light_dir = normalize(-light.dir);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir);

    return result;
}

vec3 calc_point_light(point_light_type light) {
    vec3 light_dir = normalize(light.pos - frag_pos);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir);

    float dist = length(light.pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    result *= attenuation;
    return result;
}

vec3 calc_spot_light(spot_light_type light) {
    vec3 light_dir = normalize(light.pos - frag_pos);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir);

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

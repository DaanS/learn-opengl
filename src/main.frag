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
    float opacity;

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

    bool has_diffuse_map;
    bool has_specular_map;
    bool has_emissive_map;
    bool has_bump_map;
    bool has_normal_map;
};

in vec3 frag_pos;
in vec3 frag_normal;
in vec2 frag_tex_coords;
in mat3 tbn;

in vec3 deb_tan;

out vec4 frag_color;

uniform vec3 view_pos;

uniform dir_light_type dir_light;
#define POINT_LIGHT_COUNT 4
uniform point_light_type point_lights[POINT_LIGHT_COUNT];
uniform spot_light_type spot_light;

uniform material_type material;

vec3 calc_base_light(vec3 ambient, vec3 diffuse, vec3 specular, vec3 light_dir) {
    vec3 normal;
    if (material.has_normal_map) {
        normal = vec3(texture(material.normal, frag_tex_coords));
        normal = normalize(normal * 2.0 - 1.0);
        normal = normalize(tbn * normal);
    } else {
        normal = normalize(frag_normal);
    }
    vec3 view_dir = normalize(view_pos - frag_pos);

    vec3 ambient_color = ambient * vec3(texture(material.diffuse, frag_tex_coords));

    float diff_strength = max(dot(normal, light_dir), 0.0);
    vec3 diff_color = diff_strength * diffuse * vec3(texture(material.diffuse, frag_tex_coords));

    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec_strength = pow(max(dot(view_dir, reflect_dir), 0.0), material.shininess);
    vec3 spec_color;
    if (material.has_specular_map) {
        spec_color = spec_strength * specular * vec3(texture(material.specular, frag_tex_coords));
    } else {
        spec_color = spec_strength * specular * material.color_specular;
    }

    return ambient_color + diff_color + spec_color;
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
    vec4 tex_color = texture(material.diffuse, frag_tex_coords);
    if (tex_color.a < 0.5) discard;

    vec3 result = calc_dir_light(dir_light);

    for (int i  = 0; i < POINT_LIGHT_COUNT; ++i) {
        result += calc_point_light(point_lights[i]);
    }

    result += calc_spot_light(spot_light);

    frag_color = vec4(result, 1.0);
}

#version 410 core

out vec4 color;

in vec2 texture_coordinates;
in vec3 world_position;
in vec3 normal_in;

uniform vec3 view_position;
uniform vec3 light_positions[4];
uniform vec3 light_colors[4];
uniform int light_count;

uniform bool use_material_maps;

uniform sampler2D albedo_map;
uniform sampler2D normal_map;
uniform sampler2D metallic_map;
uniform sampler2D roughness_map;
uniform sampler2D ao_map;
uniform samplerCube irradiance_map;
uniform samplerCube prefilter_map;
uniform sampler2D brdf_lookup_texture;

uniform float u_ao;
uniform float u_metallic;
uniform float u_roughness;
uniform vec3 u_albedo;

const float PI = 3.14159265359;

float distribution_ggx(vec3 normal, vec3 halfway, float roughness);
float geometry_schlick_ggx(float n_dot_v, float roughness);
float geometry_smith(vec3 normal, vec3 view_direction, vec3 light_direction, float roughness);
vec3 fresnel_schlick(float cos_theta, vec3 base_reflectivity);
vec3 fresnel_schlick_roughness(float cos_theta, vec3 base_reflectivity, float roughness);

void main() {
	vec3 view_direction = normalize(view_position - world_position);
	vec3 normal = normalize(normal_in);
	vec3 reflected = reflect(-view_direction, normal);

	vec3 albedo;
	float metallic;
	float roughness;
	float ao;

	if (use_material_maps) {
		albedo = pow(texture(albedo_map, texture_coordinates).rgb, vec3(2.2));
		metallic = texture(metallic_map, texture_coordinates).r;
		roughness = texture(roughness_map, texture_coordinates).r;
		ao = texture(ao_map, texture_coordinates).r;

		vec3 tangent_normal = texture(normal_map, texture_coordinates).xyz * 2.0 - 1.0;
		vec3 q1 = dFdx(world_position);
		vec3 q2 = dFdy(world_position);
		vec2 st1 = dFdx(texture_coordinates);
		vec2 st2 = dFdy(texture_coordinates);
		vec3 n = normalize(normal);
		vec3 t = normalize(q1 * st2.t - q2 * st1.t);
		vec3 b = -normalize(cross(n, t));
		normal = normalize(mat3(t, b, n) * tangent_normal);
	} else {
		albedo = u_albedo;
		metallic = u_metallic;
		roughness = u_roughness;
		ao = u_roughness;
	}

	vec3 base_reflectivity = vec3(0.04);
	base_reflectivity = mix(base_reflectivity, albedo, metallic);

	vec3 Lo = vec3(0.0);
	for (int i = 0; i < light_count; i++) {
		// Calculate per-light radiance
		vec3 light_direction = normalize(light_positions[i] - world_position);
		vec3 halfway = normalize(view_direction + light_direction);
		float light_distance = length(light_positions[i] - world_position);
		float attenuation = 1.0 / (light_distance * light_distance);
		vec3 radiance = light_colors[i] * attenuation;

		// Cook-torrance BRDF
		float NDF = distribution_ggx(normal, halfway, roughness);
		float G = geometry_smith(normal, view_direction, light_direction, roughness);
		vec3 light_reflected = fresnel_schlick(clamp(dot(halfway, view_direction), 0.0, 1.0), base_reflectivity);
		// the refracted light is any light that wasn't reflected
		// we also multiply by 1 - metallic in order to reinforce the property that metallic surfaces don't have refractions
		vec3 light_refracted = (vec3(1.0) - light_reflected) * (1.0 - metallic);

		vec3 numerator = NDF * G * light_reflected;
		// adding 0.0001 to this prevents division by 0 in the case that the dot products are 0
		float denominator = 4.0 * max(dot(normal, view_direction), 0.0) * max(dot(normal, light_direction), 0.0) + 0.0001;
		vec3 specular = numerator / denominator;

		float n_dot_l = max(dot(normal, light_direction), 0.0);
		Lo += (light_refracted * albedo / PI + specular) * radiance * n_dot_l;
	}

	// IBL diffuse
	vec3 light_reflected = fresnel_schlick_roughness(max(dot(normal, view_direction), 0.0), base_reflectivity, roughness);
	vec3 light_refracted = (vec3(1.0) - light_reflected) * (1.0 - metallic);
	vec3 irradiance = texture(irradiance_map, normal).rgb;
	vec3 diffuse = irradiance * albedo;

	// IBL specular
	const float MAX_RELFECTION_LOD = 4.0;
	vec3 prefiltered_color = textureLod(prefilter_map, reflected, roughness * MAX_RELFECTION_LOD).rgb;
	vec2 brdf = texture(brdf_lookup_texture, vec2(max(dot(normal, view_direction), 0.0), roughness)).rg;
	vec3 specular = prefiltered_color * (light_reflected * brdf.x + brdf.y);
	
	vec3 ambient = (light_refracted * diffuse + specular) * ao;
	vec3 _color = ambient + Lo;

	// Reinhard tone mapping (HDR)
	_color = _color / (_color + vec3(1.0));
	// Gamma correction
	_color = pow(_color, vec3(1.0 / 2.2));

	color = vec4(_color, 1.0);
}

float distribution_ggx(vec3 normal, vec3 halfway, float roughness) {
	float a = roughness * roughness;
	float a_squared = a * a;
	float n_dot_h = max(dot(normal, halfway), 0.0);
	float n_dot_h_squared = n_dot_h * n_dot_h;

	float denominator = (n_dot_h_squared * (a_squared - 1.0) + 1.0);
	denominator = PI * denominator * denominator;

	return a_squared / denominator;
}

float geometry_schlick_ggx(float n_dot_v, float roughness) {
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;

	float denominator = n_dot_v * (1.0 - k) + k;
	return n_dot_v / denominator;
}

float geometry_smith(vec3 normal, vec3 view_direction, vec3 light_direction, float roughness) {
	float n_dot_v = max(dot(normal, view_direction), 0.0);
	float n_dot_l = max(dot(normal, light_direction), 0.0);
	float ggx2 = geometry_schlick_ggx(n_dot_v, roughness);
	float ggx1 = geometry_schlick_ggx(n_dot_l, roughness);

	return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cos_theta, vec3 base_reflectivity) {
	return base_reflectivity + (1.0 - base_reflectivity) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 fresnel_schlick_roughness(float cos_theta, vec3 base_reflectivity, float roughness) {
	return base_reflectivity + (max(vec3(1.0 - roughness), base_reflectivity) - base_reflectivity) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}
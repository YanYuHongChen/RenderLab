#include <CppUtil/Engine/BSDF_MetalWorkflow.h>

#include <CppUtil/Basic/Image.h>
#include <CppUtil/Basic/Math.h>

using namespace CppUtil::Engine;
using namespace CppUtil::Basic;
using namespace glm;

vec3 BSDF_MetalWorkflow::F(const vec3 & wo, const vec3 & wi, const vec2 & texcoord) {
	auto albedo = GetAlbedo(texcoord);
	auto metallic = GetMetallic(texcoord);
	auto roughness = GetRoughness(texcoord);
	auto ao = GetAO(texcoord);

	auto diffuse = albedo / Math::PI;
	return ao * ((1 - metallic)*diffuse + MS_BRDF(wo, wi, albedo, metallic, roughness));
}

float BSDF_MetalWorkflow::PDF(const vec3 & wo, const vec3 & wi, const vec2 & texcoord) {
	auto roughness = GetRoughness(texcoord);

	vec3 h = normalize(wo + wi);
	return NDF(h, roughness) / 4.0f;
	//return 1.0f / (2.0f * Math::PI);
}

vec3 BSDF_MetalWorkflow::Sample_f(const vec3 & wo, const vec2 & texcoord, vec3 & wi, float & pd) {
	float Xi1 = Math::Rand_F();
	float Xi2 = Math::Rand_F();
	auto roughness = GetRoughness(texcoord);

	// ���� NDF ����
	float alpha = roughness * roughness;
	float cosTheta2 = (1 - Xi1) / (Xi1*(alpha*alpha - 1) + 1);
	float cosTheta = sqrt(cosTheta2);
	float sinTheta = sqrt(1 - cosTheta2);
	float phi = 2 * Math::PI*Xi2;
	vec3 h(sinTheta*cos(phi), sinTheta*sin(phi), cosTheta);
	wi = reflect(-wo, h);
	if (wi.z <= 0) {
		pd = 0;
		return vec3(0);
	}
	pd = NDF(h, roughness) / 4.0f;

	/*
	// ���Ȳ���
	float cosTheta = Xi1;
	float sinTheta = sqrt(1 - cosTheta * cosTheta);
	float phi = 2 * Math::PI * Xi2;
	wi = vec3(sinTheta*cos(phi), sinTheta*sin(phi), cosTheta);
	vec3 h = normalize(wo + wi);
	pd = 1.0f / (2.0f * Math::PI);
	*/

	// sample
	auto albedo = GetAlbedo(texcoord);
	auto metallic = GetMetallic(texcoord);
	auto ao = GetAO(texcoord);
	//printf("albedo:(%f,%f,%f), metallic��%f, roughness:%f\n", albedo.x, albedo.y, albedo.z, metallic, roughness);

	auto diffuse = albedo / Math::PI;
	return ao * ((1 - metallic)*diffuse + MS_BRDF(wo, wi, albedo, metallic, roughness));
}

vec3 BSDF_MetalWorkflow::MS_BRDF(const vec3 & wo, const vec3 & wi, const vec3 & albedo, float metallic, float roughness) {
	vec3 h = normalize(wo + wi);
	return NDF(h, roughness)*Fr(wi, h, albedo, metallic)*G(wo, wi, roughness) / (4 * wo.z*wi.z);
}

float BSDF_MetalWorkflow::NDF(const vec3 & h, float roughness) {
	//  GGX/Trowbridge-Reitz

	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float NoH = h.z;
	return alpha2 / (Math::PI*pow(NoH*NoH*(alpha2 - 1) + 1, 2));
}

vec3 BSDF_MetalWorkflow::Fr(const vec3 & wi, const vec3 & h, const vec3 & albedo, float metallic) {
	// Schlick��s approximation
	// use a Spherical Gaussian approximation to replace the power.
	//  slightly more efficient to calculate and the difference is imperceptible

	vec3 F0 = mix(vec3(0.04f), albedo, metallic);
	float HoWi = dot(h, wi);
	return F0 + (vec3(1.0f) - F0) * pow(2.0f, (-5.55473f * HoWi - 6.98316f) * HoWi);
}

float BSDF_MetalWorkflow::G(const vec3 & wo, const vec3 & wi, float roughness) {
	// Schlick, remap roughness and k

	// k = alpha / 2
	// direct light: alpha = pow( (roughness + 1) / 2, 2)
	// IBL(image base lighting) : alpha = pow( roughness, 2)

	if (wo.z <= 0 || wi.z <= 0)
		return 0;

	float k = pow(roughness + 1, 2) / 8.f;
	float G1_wo = wo.z / (wo.z*(1 - k) + k);
	float G1_wi = wi.z / (wi.z*(1 - k) + k);
	return G1_wo * G1_wi;
}

const vec3 BSDF_MetalWorkflow::GetAlbedo(const vec2 & texcoord) const {
	if (!albedoTexture || !albedoTexture->IsValid())
		return albedoColor;

	bool blend = albedoTexture->GetChannel() == 4;
	return vec3(albedoTexture->Sample(texcoord, blend))*albedoColor;
}

float BSDF_MetalWorkflow::GetMetallic(const vec2 & texcoord) const {
	if (!metallicTexture || !metallicTexture->IsValid())
		return metallicFactor;

	return metallicTexture->Sample(texcoord).x * metallicFactor;
}

float BSDF_MetalWorkflow::GetRoughness(const vec2 & texcoord) const {
	if (!roughnessTexture || !roughnessTexture->IsValid())
		return roughnessFactor;

	return roughnessTexture->Sample(texcoord).x * roughnessFactor;
}

float BSDF_MetalWorkflow::GetAO(const glm::vec2 & texcoord) const {
	if (!aoTexture || !aoTexture->IsValid())
		return 1.0f;

	return aoTexture->Sample(texcoord).x;
}
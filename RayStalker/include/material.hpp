#ifndef MATERIAL_HPP
#define MATERIAL_HPP
#include <glm/vec3.hpp>

struct Material
{
	glm::vec3 Albedo{ 1.0f };
	float roughness{1.0};
	float metallic{ 0.0 };
};
#endif // !MATERIAL_HPP

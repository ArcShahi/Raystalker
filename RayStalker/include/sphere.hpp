#ifndef SPHERE_HPP
#define SPHERE_HPP

#include "material.hpp"

struct Sphere
{
    
	glm::vec3 position{};
	float radius{0.5f};
	int MaterialIndex{0};

};
#endif // !SPHERE_HPP

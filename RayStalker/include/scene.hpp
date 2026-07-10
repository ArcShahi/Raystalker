#ifndef SCENE_HPP
#define SCENE_HPP

#include <vector>
#include "sphere.hpp"

struct Scene
{
	std::vector<Sphere> Spheres{};
	std::vector<Material> Materials{};
};

#endif // !SCENE_HPP

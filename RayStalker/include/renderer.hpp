#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <Walnut/Image.h>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "camera.hpp"
#include "ray.hpp"
#include "scene.hpp"
#include <string>
// The input will be some scene description,output is an image
class Renderer
{
public:
	struct Settings 
	{
	    int Bounces{ 5 };
	    bool Accumulate{ true };
	};
public:
	Renderer() = default;
	// Render every pixel that makes up our viewport
	void OnResize(uint32_t width, uint32_t height);
	void Render(const Scene& scene, const Camera& camera);
	std::shared_ptr<Walnut::Image> GetFinalImage()const { return m_FinalImage; }
	void ResetFrameIndex() { m_FrameIndex = 1; }
	Settings& GetSettings() { return m_Settings; }
	void SaveScene(std::string& filename);
private:

	struct HitPayload
	{
		glm::vec3 WorldPosition{};
		glm::vec3 WorldNormal{};
		float HitDistance{};
		int ObjectIndex;
	};
	glm::vec4 PerPixel(uint32_t x,uint32_t y); // RayGen

	// shoots a ray to particular point -> returns info about ray
	HitPayload TraceRay(const Ray& ray);
	HitPayload ClosestHit(const Ray& ray,float hitDistance,int objectIndex);
	HitPayload Miss(const Ray& ray);
private:

	const Scene* m_ActiveScene{ nullptr };
	const Camera* m_ActiveCamera{ nullptr };

	std::shared_ptr<Walnut::Image> m_FinalImage{};
	Settings m_Settings{};
	uint32_t* m_ImageData{ nullptr };
	glm::vec4* m_AccumulationData{ nullptr };
	uint32_t m_FrameIndex{ 1 };
	std::vector<uint32_t> m_ImageHorizontalIter{}, m_ImageVerticalIter{};
};

#endif // !RENDERER_HPP



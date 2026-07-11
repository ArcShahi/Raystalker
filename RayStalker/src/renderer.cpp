#include "renderer.hpp"
#include <glm/geometric.hpp>
#include <numeric>
#include <algorithm>
#include <execution>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>


static uint32_t ConvertToRGBA(const glm::vec4& color)
{
	uint8_t r{ static_cast<uint8_t>(color.r * 255.f) };
	uint8_t g{ static_cast<uint8_t>(color.g * 255.f) };
	uint8_t b{ static_cast<uint8_t>(color.b * 255.f) };
	uint8_t a{ static_cast<uint8_t>(color.a * 255.f) };

	// The Alpha channel is at MSB
	uint32_t result{ static_cast<uint32_t>((a << 24) | (b << 16) | (g << 8) | r) };
	return result;
}

static uint32_t PCG_Hash(uint32_t input)
{
	uint32_t state = input * 747796405u + 2891336453u;
	uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

static float RandomFloat(uint32_t& seed)
{
	seed = PCG_Hash(seed);
	return static_cast<float>(seed) / static_cast<float>(std::numeric_limits<uint32_t>::max());
}

static glm::vec3 InUnitSphere(uint32_t seed)
{
	return glm::normalize(glm::vec3(
	     RandomFloat(seed) * 2.0f - 1.0f,
		RandomFloat(seed) * 2.0f - 1.0f,
		RandomFloat(seed) * 2.0f - 1.0f));
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	// If no image create one and return if no resize needed
	if (m_FinalImage)
	{
		// No resize necesscary
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;
		// checks if it needs resizing then rellocates
		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width,
			height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);

	/*	for (uint32_t i{ 0 }; i < width; ++i)
			m_ImageHorizontalIter[i] = i;
		for (uint32_t i{ 0 }; i < height; ++i)
			m_ImageVerticalIter[i] = i;
		*/
	std::iota(begin(m_ImageHorizontalIter), end(m_ImageHorizontalIter), 0);
	std::iota(begin(m_ImageVerticalIter), end(m_ImageVerticalIter), 0);
}



void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight()
			* sizeof(glm::vec4));

	// std::thread::hardware_concurrency();
#define MULTI_THREAD 1
#if MULTI_THREAD 

	std::for_each(std::execution::par,
		begin(m_ImageVerticalIter), end(m_ImageVerticalIter),
		[this](uint32_t y)
		{

			std::for_each(std::execution::par,
				begin(m_ImageHorizontalIter), end(m_ImageHorizontalIter),
				[this, y](uint32_t x)
				{

					glm::vec4 color = PerPixel(x, y);
					// Accumualte color
					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;
					glm::vec4 accumulatedColor = m_AccumulationData
						[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= static_cast<float>(m_FrameIndex);
					// clamp in range [0,1]
					accumulatedColor = glm::clamp(accumulatedColor,
						glm::vec4(0.f), glm::vec4(1.f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] =
						ConvertToRGBA(accumulatedColor);

				});
		});
#else
		// Left to right and then top to bottom
	for (uint32_t y{ 0 }; y < m_FinalImage->GetHeight(); ++y)
	{
		for (uint32_t x{ 0 }; x < m_FinalImage->GetWidth(); ++x)
		{
			PerPixel(x, y);
			glm::vec4 color = PerPixel(x, y);
			// clamp in range [0,1]
			color = glm::clamp(color, glm::vec4(0.f), glm::vec4(1.f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = ConvertToRGBA(color);
		}

	}
	#endif
	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		++m_FrameIndex;
	else
		m_FrameIndex = 1;
}

void Renderer::SaveScene(std::string& filename)
{
	if (!filename.ends_with(".png"))
		filename += ".png";

	stbi_write_png(
		filename.c_str(),
		m_FinalImage->GetWidth(),
		m_FinalImage->GetHeight(),
		4,
		m_ImageData,
		m_FinalImage->GetWidth() * sizeof(uint32_t)
	);
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray{};
	ray.origin = m_ActiveCamera->GetPosition();
	ray.direction = m_ActiveCamera->GetRayDirections()
		[x + y * m_FinalImage->GetWidth()];

	glm::vec3 light(0.0f);
	glm::vec3 throughput{ 1.0f };

	uint32_t seed{ x + y * m_FinalImage->GetWidth() };
	seed *= m_FrameIndex;

	for (int i{ 0 }; i < m_Settings.Bounces; ++i)
	{
		seed += i;
		auto payload{ TraceRay(ray) };
		if (payload.HitDistance < 0.0f) {
			glm::vec3 skyColor = glm::vec3{ 0.6f,0.7f,0.9f };
			//light += skyColor * throughput;
			break;
		}

		const Sphere& sphere{ m_ActiveScene->Spheres[payload.ObjectIndex] };
		const Material& material{ m_ActiveScene->Materials[sphere.MaterialIndex] };

		// Absorbing color wavelength
		throughput *= material.Albedo;
		light += material.GetEmission();

		// cast new from hitPoint(a bit forward so we don't collide with same ray)
		ray.origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		// Reflection based on roughness
		// the rougher the surface the more shift in direction
		// Bias it toward world normal
		ray.direction = glm::normalize(payload.WorldNormal + InUnitSphere(seed));
	}

	return glm::vec4(light, 1.0f);
}



Renderer::HitPayload Renderer::ClosestHit
(const Ray& ray, float hitDistance, int objectIndex)
{

	Renderer::HitPayload payload{};
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;


	const Sphere& closestSphere{ m_ActiveScene->Spheres[objectIndex] };

	const auto oc{ closestSphere.position - ray.origin };
	payload.WorldPosition = ray.origin + ray.direction * hitDistance;
	// Surface normal : Vector from sphereCenter to hitPoint
	payload.WorldNormal = glm::normalize(payload.WorldPosition - closestSphere.position);

	// payload.WorldPosition += closestSphere.position;
	return payload;

}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload{};
	payload.HitDistance = -1.0f;
	return payload;
}


Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{

	/*

	 Pixel shader : Shader toy : GLSL
	 Single function mainImage is called for every pixel in that viewport
	 Shader are small programs that run on GPU

	 The Ray orignates from camera and is sent through every pixel on viewport
	 then we check if it collides with our mathematically defined sphere ?
	 If yes then we color the pixel

	 */

	 // camera : ray originates from camera, -z means forward
	 // Ray passes through each pixel 


	// Only render the closest sphere which was hit
	int closestSphere{ -1 };
	float hitDistance{ std::numeric_limits<float>::max() };

	for (size_t i{ 0 }; i < m_ActiveScene->Spheres.size(); ++i)
	{
		const Sphere& sphere{ m_ActiveScene->Spheres[i] };
		// Arbitrary origin of sphere, Vector from Point P to Center 
		const auto oc{ sphere.position - ray.origin };
		const float a{ glm::dot(ray.direction,ray.direction) };
		const float h{ glm::dot(ray.direction, oc) };
		const float c{ glm::dot(oc,oc) - sphere.radius * sphere.radius };

		const float discriminant{ h * h - a * c };

		// If missded return clear color
		if (discriminant < 0.0f)
			continue;

		const float sqrtd{ glm::sqrt(discriminant) };

		// The smallest solution is our closest hit
		// a is always positive, so t1 will be always smallest
		// [[maybe_unused]] const float t0{ (h + sqrtd) / a };
		const float closeT{ (h - sqrtd) / a };

		if (closeT > 0.0f && closeT < hitDistance)
		{
			hitDistance = closeT;
			closestSphere = static_cast<int>(i);
		}
	}

	// Nothing was hit
	if (closestSphere < 0)
		return Miss(ray);

	return ClosestHit(ray, hitDistance, closestSphere);

}


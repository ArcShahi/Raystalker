#include "renderer.hpp"
#include <glm/geometric.hpp>
#include <numeric>
#include <algorithm>
#include <thread>
#include <execution>
#include <Walnut/Random.h>

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

	// std::thread::hardware_concurrency();
#define MULTI_THREAD 1
#if MULTI_THREAD 
	std::for_each(std::execution::par,
		begin(m_ImageVerticalIter), end(m_ImageVerticalIter), 
		[this](uint32_t y)
		{
		std::for_each(std::execution::par,
			begin(m_ImageHorizontalIter), end(m_ImageHorizontalIter),
			[this,y](uint32_t x) 
			{
				glm::vec4 color = PerPixel(x, y);
				// clamp in range [0,1]
				color = glm::clamp(color, glm::vec4(0.f), glm::vec4(1.f));
				m_ImageData[x + y * m_FinalImage->GetWidth()] = ConvertToRGBA(color);

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
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray{};
	ray.origin = m_ActiveCamera->GetPosition();
	ray.direction = m_ActiveCamera->GetRayDirections()
		[x + y * m_FinalImage->GetWidth()];

	glm::vec3 color(0.0f);
	float multiplier{ 1.0f };


	const int bounces{ 5 };
	for (int i{ 0 }; i < bounces; ++i)
	{
		auto payload{ TraceRay(ray) };
		if (payload.HitDistance < 0.0f) {
			glm::vec3 skyColor = glm::vec3{ 0.6f,0.7f,0.9f };
			color += skyColor * multiplier;
			break;
		}

		// Light source
		glm::vec3 lightDir{ glm::normalize(glm::vec3(-1.0f,-1.0f,-1.0f)) };
		// If theta >90 then gives 0
		float candela{ glm::max(glm::dot(payload.WorldNormal,-lightDir),0.0f) }; //cos(theta) [0,1]

		const Sphere& sphere{ m_ActiveScene->Spheres[payload.ObjectIndex] };
		const Material& material{ m_ActiveScene->Materials[sphere.MaterialIndex] };

		// Calculate where the ray hits the sphere
		// glm::vec3 sphereColor{ 0.7f,0.3f,0.9f };
		auto sphereColor{ material.Albedo };
		// sphereColor = normal * 0.5f + 0.5f; // -1*0.5+0.5=0 so color range now [0,1]
		sphereColor *= candela;
		color += sphereColor * multiplier;
		multiplier *= 0.5f;

		// cast new from hitPoint(a bit forward so we don't collide with same ray)
		ray.origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		// Reflection based on roughness
		// the rougher the surface the more shift in direction
		ray.direction = glm::reflect(ray.direction, payload.WorldNormal
			+material.roughness*Walnut::Random::Vec3(-0.5f,0.5f));
	}

	return glm::vec4(color, 1.0f);
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

	 Sphere centered at origin : x^2+y^2+z^2=r^2
	 Sphere be at an arbitrary point C(x,y,z) then our sphere equation

	 (x-i)^2+(y-j)^2+(z-k)^2=r^2

	 Sphere origin vector C[x,y,z] and our Circle is at vector P[i,j,k]
	 then vector from P to C = (C - P)

	 and (C-P)·(C-P) =  (x-a)^2+(y-b)^2+(z-c)^2
	 => (C-P)·(C-P)=r^2

	 Any point P that satisfies this equation is on sphere

	 Our Ray P(t)= A + Dt

	 We wanna know if our ray hits our sphere, if it does then there's some 't'
	 that satisfies that sphere equation

	 so (C-P(t) · (C-P(t)=r^2
	 => (C-A+Dt) · (C-A+Dt)=r^2
	 => (D·D)t^2  - 2D(C-A)t  + (C-A) · (C-A) - r^2 = 0

	 Quadratic formula :  (-b (+-) sqrt(b^2 - 4ac ))/2a

	 a = D · D  : Ray direction dot Ray Direction
	 b = -2D·(C-A) : -2  times Ray direction dot Sphere origin - Ray Origin
	 c = (Sphere oring - Ray origin dot Sphere Origin - Ray Origin) - radius * radius

	 dot(v,v) = ||v||^2

	 a = ||D||^2
	 c = ||(C-A)||^2

	 b has a factor of -2 , b=-2h then

	 Quadratic Eqn : (h (+-) sqrt(h^2-ac))/a

	 then b=-2h => h = b / -2
	 => h =  D · (C-Q)
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
		const float a{glm::dot(ray.direction,ray.direction)};
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

		if (closeT >0.0f && closeT < hitDistance)
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


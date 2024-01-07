#include "Renderer.h"
#include "Walnut/Random.h"
#include <execution>

namespace Utils {
	static uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		uint8_t r = (uint8_t)(color.r * 255.0f);
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);
		
		return 0x00000000 | (a << 24) | (b << 16) | (g << 8) | r;
	}

	static uint32_t PCG_Hash(uint32_t input)
	{
		uint32_t state = input + 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 2778037727u;
		return (word >> 22u) ^ word;
	}

	static float RandomFloat(uint32_t& seed)
	{
		seed = PCG_Hash(seed);
		return (float)seed / (float)std::numeric_limits<uint32_t>::max();
	}

	static glm::vec3 InUnitSphere(uint32_t& seed)
	{
		return glm::normalize(glm::vec3(
			RandomFloat(seed) * 2.0f - 1.0f, 
			RandomFloat(seed) * 2.0f - 1.0f, 
			RandomFloat(seed) * 2.0f - 1.0f));
	}
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		// Return if Image does not need resizing to avoid reallocating Image Data
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;

		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizantalIter.resize(width);
	m_ImageVerticalIter.resize(height);

	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizantalIter[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));

#define MT 1

#if MT

	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this](uint32_t y) 
		{
			std::for_each(std::execution::par, m_ImageHorizantalIter.begin(), m_ImageHorizantalIter.end(),
				[this, y](uint32_t x) 
				{
					glm::vec4 color = PerPixel(x, y);
					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= (float)m_FrameIndex;

					accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
				}
			);
		}
	);

#else

	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			glm::vec4 color = PerPixel(x, y);
			m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;
			
			glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
			accumulatedColor /= (float)m_FrameIndex;

			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
		}
	}

#endif

	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray;
	ray.m_Origin = m_ActiveCamera->GetPosition();
	ray.m_Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 light(0.0f);
	glm::vec3 contribution{ 1.0f };
	
	uint32_t seed = x + y * m_FinalImage->GetWidth();
	seed *= m_FrameIndex;

	int bounces = 4;
	for (int i = 0; i < bounces; i++) 
	{
		seed += i;

		Renderer::HitPayload payload = TraceRay(ray);
		if (payload.HitDistance < 0.0f)
		{
			if (m_Settings.RenderSky)
			{
				glm::vec3 sky(0.6f, 0.7f, 0.9f);
				// Only add light from sky
				light += sky * contribution;
			}
			break;
		}

		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		Material material = m_ActiveScene->Materials[sphere.MaterialIndex];

		contribution *= material.Color;
		light += material.GetEmission();

		ray.m_Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;

		if (m_Settings.SlowRandom)
			ray.m_Direction = glm::normalize(payload.WorldNormal + Walnut::Random::InUnitSphere());
		else
			ray.m_Direction = glm::normalize(payload.WorldNormal + Utils::InUnitSphere(seed));
	}

	return glm::vec4(light, 1.0f);
}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	int ClosestSphere = -1;
	float ClosestT = FLT_MAX;

	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++)
	{
		glm::vec3 newOrigin = ray.m_Origin - m_ActiveScene->Spheres[i].Position;

		float a = glm::dot(ray.m_Direction, ray.m_Direction);
		float b = 2.0f * glm::dot(newOrigin, ray.m_Direction);
		float c = glm::dot(newOrigin, newOrigin) - (m_ActiveScene->Spheres[i].Radius * m_ActiveScene->Spheres[i].Radius);

		float discriminant = b * b - 4.0f * a * c;
		if (discriminant < 0.0f) {
			continue;
		}
		
		float hitDistance = (-b - glm::sqrt(discriminant)) / (2.0f * a);
		if (hitDistance > 0.0f && ClosestT > hitDistance) {
			ClosestT = hitDistance;
			ClosestSphere = (int)i;
		}
	}

	if (ClosestSphere < 0) {
		return Miss(ray);
	}

	return ClosestHit(ray, ClosestT, ClosestSphere);

}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float HitDistance, int index)
{
	Renderer::HitPayload payload;
	payload.HitDistance = HitDistance;
	payload.ObjectIndex = index;

	const Sphere& sphere = m_ActiveScene->Spheres[index];

	glm::vec3 newOrigin = ray.m_Origin - sphere.Position;

	payload.WorldPosition = newOrigin + ray.m_Direction * HitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	payload.WorldPosition += sphere.Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}

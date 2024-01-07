#pragma once

#include "Walnut/Image.h"
#include "memory"
#include <glm/glm.hpp>
#include "Camera.h"
#include "Ray.h"
#include "Scene.h"

class Renderer
{
public:
	struct Settings
	{
		bool Accumulate = true;
		bool SlowRandom = false;
		bool RenderSky = true;
	};
public:
	Renderer() = default;

	void OnResize(uint32_t width, uint32_t height);

	void Render(const Scene& scene, const Camera& camera);

	std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }

	void ResetFrameIndex() { m_FrameIndex = 1; }

	Settings& GetSettings() { return m_Settings; }

private:
	struct HitPayload
	{
		float HitDistance;
		
		glm::vec3 WorldPosition;
		glm::vec3 WorldNormal;
		
		int ObjectIndex;
	};

	glm::vec4 PerPixel(uint32_t x, uint32_t y);

	HitPayload TraceRay(const Ray& ray);
	HitPayload ClosestHit(const Ray& ray, float HitDistance, int index);
	HitPayload Miss(const Ray& ray);

private:
	const Camera* m_ActiveCamera;
	const Scene* m_ActiveScene;

	std::vector<uint32_t> m_ImageHorizantalIter;
	std::vector<uint32_t> m_ImageVerticalIter;

	Settings m_Settings;

	std::shared_ptr<Walnut::Image> m_FinalImage;

	uint32_t* m_ImageData = nullptr;
	glm::vec4* m_AccumulationData = nullptr;

	uint32_t m_FrameIndex = 1;
};



#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Timer.h"
#include "renderer.hpp"
#include "camera.hpp"
#include "scene.hpp"
#include <glm/gtc/type_ptr.hpp>


class SceneLayer : public Walnut::Layer
{
public:

	SceneLayer()
		:m_Camera(45.0f, 0.1f, 100.f)
	{
		Material& redSphere = m_scene.Materials.emplace_back();
		redSphere.Albedo = { 1.0f,0.0f,0.0f };
		redSphere.roughness = 0.0f;

		Material& blackSphere = m_scene.Materials.emplace_back();
		blackSphere.Albedo = { 0.0f,0.0f,0.0f };
		blackSphere.roughness = 0.1f;

		{
			Sphere sphere;
			sphere.position = { 0.0f,0.0f,0.0f };
			sphere.radius = 1.0f;
			sphere.MaterialIndex = 0;
			m_scene.Spheres.push_back(sphere);
		}
		{
			Sphere sphere;
			sphere.position = { 0.0f,-101.0f,0.0f };
			sphere.radius = 100.0f;
			sphere.MaterialIndex = 1;
			m_scene.Spheres.push_back(sphere);
		}
	}

	virtual void OnUpdate(float ts) override
	{
		// Camera updates here
		m_Camera.OnUpdate(ts);
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Info");
		ImGui::Text("Last render : %.3fms", m_rendertime);
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

		/*if (ImGui::Button("Render")) {
			render();
		}*/
		ImGui::End();

		ImGui::Begin("Scene");
		for (size_t i{ 0 }; i < m_scene.Spheres.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			Sphere& sphere = m_scene.Spheres[i];
			ImGui::DragFloat3("Position", glm::value_ptr(sphere.position), 0.1f);
			ImGui::DragFloat("Radius", &sphere.radius, 0.1f,0.0f,500.f);
			ImGui::DragInt("Material", &sphere.MaterialIndex, 1.0f, 0,
				static_cast<int>(m_scene.Materials.size()-1));

			ImGui::Separator();
			ImGui::PopID();
		}

		for (size_t i{ 0 }; i < m_scene.Materials.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			Material& material{ m_scene.Materials[i] };
			ImGui::ColorEdit3("Albedo", glm::value_ptr(material.Albedo), 0.1f);
			ImGui::DragFloat("Roughness", &material.roughness, 0.02f, 0.0f, 1.0f);
			ImGui::DragFloat("Metallic", &material.metallic, 0.02f, 0.0f, 1.0f);
			ImGui::Separator();
			ImGui::PopID();
		}
		ImGui::End();

		// Fixed the black border between border
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::Begin("Viewport");
		// Size in pixel
		m_ViewportWidth = static_cast<uint32_t>(ImGui::GetContentRegionAvail().x);
		m_ViewportHeight = static_cast<uint32_t>(ImGui::GetContentRegionAvail().y);

		// Render as Imgui image within that viewport window
		// We're using Imgui image's dim to render so when tab will resize the image
		// won't stretch
		auto image = m_Renderer.GetFinalImage();
		if (image)
			ImGui::Image(image->GetDescriptorSet(),
				{ (float)image->GetWidth(),(float)image->GetHeight() },
				ImVec2(0, 1), ImVec2(1, 0));


		ImGui::End();
		ImGui::PopStyleVar();
		render();

	}
	void render()
	{
		Walnut::Timer timer{};
		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_scene, m_Camera);

		m_rendertime = timer.ElapsedMillis();
	}
private:

	Renderer m_Renderer{};
	Camera m_Camera;
	Scene m_scene;
	uint32_t m_ViewportWidth{ 0 }, m_ViewportHeight{ 0 };
	float m_rendertime{};
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Raystalker";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<SceneLayer>();
	app->SetMenubarCallback([app]()
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
				{
					app->Close();
				}
				ImGui::EndMenu();
			}
		});
	return app;
}
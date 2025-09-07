#pragma once

#include <memory>
#include <glm/glm.hpp>

#include <VAO.h>
#include <VBO.h>
#include <EBO.h>

class Sphere {
public:

	Sphere() {
		Init();
	}

	Sphere(float r, GLuint latDiv, GLuint lonDiv) {
		radius = 1.0f;
		Init(latDiv, lonDiv);
		SetRadius(r);
	}

	void Draw()
	{
		p_vertexArray->Bind();
		glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
		p_vertexArray->Unbind();
	}

	void SetRadius(double _radius) {
		if (_radius == 0) {
			_radius = 1e-6f;
		}

		double scaleFactor = _radius / radius;
		radius = _radius;

		modelMat = glm::scale(modelMat, glm::vec3(scaleFactor));
	}
	
	void SetPosition(const glm::vec3& position) {
		modelMat[3] = glm::vec4(position, modelMat[3].w);
	}

	void SetModelMatrix(const glm::mat4& model) {
		modelMat = model;
	}

	const glm::mat4& GetModelMatrix() {
		return modelMat;
	}

private:
	void Init( GLuint latDiv = 50, GLuint lonDiv = 50) {
		const float r = 1.0f; // Unit Parameters

		std::vector<glm::vec3> vertex;
		std::vector<GLuint> indices;

		vertex.reserve(latDiv * lonDiv);
		indices.reserve(latDiv * lonDiv * 6);

		for (GLuint i = 0; i <= latDiv; ++i) {
			float theta = glm::pi<float>() * float(i) / float(latDiv); // Latitude angle [0, π]
			float sinTheta = sin(theta);
			float cosTheta = cos(theta);

			for (int j = 0; j <= lonDiv; ++j) {
				float phi = 2.0f * glm::pi<float>() * float(j) / float(lonDiv); // Longitude angle [0, 2π]
				float sinPhi = sin(phi);
				float cosPhi = cos(phi);


				float x = r * sinTheta * cosPhi;
				float y = r * cosTheta;
				float z = r * sinTheta * sinPhi;

				vertex.push_back({ x, y, z });
			}
		}

		// Generate indices (triangles)
		for (int i = 0; i < latDiv; ++i) {
			for (int j = 0; j < lonDiv; ++j) {
				int first = (i * (lonDiv + 1)) + j;
				int second = first + lonDiv + 1;

				// First triangle
				indices.push_back(first);
				indices.push_back(second);
				indices.push_back(first + 1);

				// Second triangle
				indices.push_back(second);
				indices.push_back(second + 1);
				indices.push_back(first + 1);
			}
		}

		p_vertexArray = std::make_unique<VAO>();
		if (p_vertexArray == nullptr)
			throw;
		p_vertexArray->Bind();

		p_vertexBuffer = std::make_unique<VBO>(vertex.data(), vertex.size() * sizeof(vertex[0]));
		p_indicesBuffer = std::make_unique<EBO>(indices);

		p_vertexArray->LinkAttrib(*p_vertexBuffer, 0, 3, GL_FLOAT, sizeof(glm::vec3), (void*)0);
		p_vertexArray->Unbind();

		indicesCount = indices.size();
	}

	std::unique_ptr<VAO> p_vertexArray;
	std::unique_ptr<VBO> p_vertexBuffer;
	std::unique_ptr<EBO> p_indicesBuffer;

	double radius;
	glm::mat4 modelMat = glm::mat4(1.0f);
	size_t indicesCount;
};

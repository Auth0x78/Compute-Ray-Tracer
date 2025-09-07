#pragma once

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cerrno>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

std::string get_file_contents(const char* filename);

class Shader
{
public:
	// Reference ID of the Shader Program
	GLuint ID;

	// Constructor that build the Shader Program from 2 different shaders
	Shader(const char* vertexFile, const char* fragmentFile);

	// Constructor that builds the Compute Shader Program
	Shader(const char* shaderFile, GLenum type);

	// Activates the Shader Program
	void Activate();

	// Deactivates the Shader Program
	void Deactivate();

	// Deletes the Shader Program
	void Delete();
	
	// Reloads the shader
	void Reload(const char* vertexFile, const char* fragmentFile);

	// Set Uniform int
	void SetUniform1i(const std::string& name, int32_t i);
	
	// Set Uniform float
	void SetUniform1f(const std::string& name, float f);

	// Set Uniform vec2
	void SetUniform2fv(const std::string& name, const glm::vec2& vec);

	// Set Uniform vec3
	void SetUniform3fv(const std::string& name, const glm::vec3& vec);

	// Set Uniform Matrix4fv
	void SetUniformMatrix4fv(const std::string& name, const glm::mat4& mat4);

	// Get Uniform Location
	int GetUniformLocation(const std::string& name);

	// Get Shader Type
	GLenum GetShaderType();

private:
	
	GLenum shaderType;
	
	// Checks if the different Shaders have compiled properly
	void compileErrors(unsigned int shader, const char* type);

	void compileShader(const char* vertexFile, const char* fragmentFile);
	
	void parsePreprocessorDirectives(std::string& source, const std::string& parentDir);

	std::unordered_map<std::string, uint32_t> uniformLocationCache;
};


#include <iostream>
#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <glm/glm.hpp>

#include <stb_image/stb_image.h>
#include <imgui.h>

#include "shader.h"
#include "ModelLoaderBVHBuilder.h"
#include "openglDebug.h"
#include "SSBO.h"
#include "EBO.h"
#include "VBO.h"
#include "VAO.h"


float fullScreenQuad[] = {
	// Screen Coords // Tex Coords
	-1.0,  1.0,		0.0, 1.0, // 0
	 1.0,  1.0,		1.0, 1.0, // 1
	 1.0, -1.0,		1.0, 0.0, // 2
	-1.0, -1.0,		0.0, 0.0  // 3
};

std::vector<GLuint> quadIndices = {
	0, 1, 2,
	0, 2, 3
};

#define USE_GPU_ENGINE 1
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = USE_GPU_ENGINE;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = USE_GPU_ENGINE;
}

int main(void) {

	int width = 0, height = 0;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
		printf("Could not initialize SDL: %s.\n", SDL_GetError());
		return -1;
	}

	SDL_Window *pWindow = SDL_CreateWindow("Ray Tracer", 512, 512, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (pWindow == nullptr) {
		SDL_Log("Failed to create SDL3 window: %s", SDL_GetError());
		// Handle error appropriately (exit, cleanup, etc.)
	}


#pragma region report opengl errors to std
	//enable opengl debugging output.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#pragma endregion

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	// Create GL Context
	SDL_GLContext glContext = SDL_GL_CreateContext(pWindow);
	if (glContext == nullptr) {
		SDL_Log("Failed to create OpenGL context: %s", SDL_GetError());
		// Handle error (e.g., cleanup and exit)
	}

	SDL_GL_MakeCurrent(pWindow, glContext);
	gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
	// Load OpenGL functions using GLAD
	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(pWindow);
		SDL_Quit();
		return -1;
	}

	// Disable V-Sync
	SDL_GL_SetSwapInterval(0);

#pragma region report opengl errors to std
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(glDebugOutput, 0);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#pragma endregion

	// Get Window width and height
	SDL_GetWindowSizeInPixels(pWindow, &width, &height);
	glViewport(0, 0, width, height);

	// Create a computer shader
	Shader computeShader(RESOURCES_PATH "compute.glsl", GL_COMPUTE_SHADER);

	// Computer shader output texture
	unsigned int texture;

	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	// Create a basic Vertex and Fragment shader application
	Shader shader(RESOURCES_PATH "shader.vert", RESOURCES_PATH "shader.frag");
	shader.Activate();
	shader.SetUniform2fv("uResolution", glm::vec2(width, height));
	shader.SetUniform1i("screenTex", 0);
	shader.Deactivate();
	
	VAO VAO1;
	VAO1.Bind();
	VBO VBO1(fullScreenQuad, sizeof(fullScreenQuad));
	EBO EBO1(quadIndices);

	// Link vertex attributes
	VAO1.LinkAttrib(VBO1, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)0);
	VAO1.LinkAttrib(VBO1, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	VAO1.Unbind();

	// Load a Dragon 8K model
	{
		Uint64 startTime = SDL_GetPerformanceCounter();

		std::cout << "Give filepath of model to load: ";
		std::string modelPath;
		std::cin >> modelPath;
		std::cout << "Loading model...\n";
		LoadModel(modelPath.c_str(), "model");
		double timeToLoad = (double)(SDL_GetPerformanceCounter() - startTime) / SDL_GetPerformanceFrequency();
		std::cout << "It took: " << timeToLoad << " seconds to load the model.\n";
	}

	// Create 3 SSBO for models[], BVHNode[] and Triangle[]
	SSBO modelBO(1, GL_DYNAMIC_COPY_ARB, sizeof(Model) * modelsBuffer.size(), modelsBuffer.data());
	SSBO bvhBO(2, GL_DYNAMIC_COPY_ARB, sizeof(BVHNode) * BVHBuffer.size(), BVHBuffer.data());
	SSBO triBO(3, GL_DYNAMIC_COPY_ARB, sizeof(Triangle) * TrianglesBuffer.size(), TrianglesBuffer.data());
	
	// Binds SSBOs to compute shader
	//modelBO.BindBase();
	//bvhBO.BindBase();
	//triBO.BindBase();

	// Timing variables
	Uint64 NOW = SDL_GetPerformanceCounter();
	Uint64 LAST = 0;
	double deltaT;
	double timeElapsed = 0;
	
	// Frame variables
	int frame = 0;

	bool running = true;
	while (running) {
		// Update Frame count
		frame++;

		// Handle events
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
			switch (event.type) {
				case SDL_EVENT_QUIT:
					running = false;
					break;
				case SDL_EVENT_WINDOW_RESIZED:
				{
					int newWidth = event.window.data1;
					int newHeight = event.window.data2;
					glViewport(0, 0, newWidth, newHeight);
					width = newWidth;
					height = newHeight;
					break;
				}
				break;
			}
			// Handle other events as needed
		}

		LAST = NOW;
		NOW = SDL_GetPerformanceCounter();

		// Calculate deltaTime (time elapsed between frames)
		deltaT = (double)(NOW - LAST) / SDL_GetPerformanceFrequency();
		timeElapsed += deltaT;
		

		glClearColor(1.0, 0.3, 0.3, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		
		// Update the compute shader uniforms
		computeShader.Activate();
		computeShader.SetUniform2fv("uResolution", glm::vec2(width, height));
		computeShader.SetUniform1f("uTime", (float)timeElapsed);
		computeShader.SetUniform1i("uFrame", frame);

		// Bind the output texture image & activate it
		glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glActiveTexture(GL_TEXTURE0);

		// Dispatch Compute shader to run
		glDispatchCompute(width, height, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Read data back into cpu buffer
		glBindTexture(GL_TEXTURE_2D, texture);

		// Draw full screen quad
		shader.Activate();
		VAO1.Bind();

		shader.SetUniform2fv("uResolution", glm::vec2(width, height));
		shader.SetUniform1i("screenTex", 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);

		// Draw elements (EBO must be bound, last param is offset)
		glDrawElements(GL_TRIANGLES, quadIndices.size(), GL_UNSIGNED_INT, 0);
		VAO1.Unbind();
		
		// Print fps
		std::cout << "Avg FPS: " << floor(frame / timeElapsed) << std::endl;
		std::cout << "Current FPS: " << floor(1.0f / deltaT) << std::endl;
		
		// Swap frame buffers
		SDL_GL_SwapWindow(pWindow);
	}

	//there is no need to call the clear function for the libraries since the os will do that for us.
	//by calling this functions we are just wasting time.
	shader.Delete();
	SDL_GL_DestroyContext(glContext);
	SDL_DestroyWindow(pWindow);
	SDL_Quit();
	return 0;
}

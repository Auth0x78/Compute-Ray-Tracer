#pragma once
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

class Camera {
public:
  Camera(SDL_Window *window, float fovDeg, float aspectRatio, float nearPlane,
         float farPlane, const glm::vec3 &position = glm::vec3(0.0f),
         const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f));

  // Call this every frame to update position/orientation
  void Update(float deltaTime);

  // Handle SDL events (keyboard/mouse)
  void ProcessEvent(const SDL_Event &event);

  // Getters for shaders
  const glm::mat4 &GetViewMatrix() const { return viewMatrix; }
  const glm::mat4 &GetProjectionMatrix() const { return projectionMatrix; }
  const glm::mat4 &GetInvProjectionMat() const { return invProjectionMatrix; }
  const glm::mat4 &GetInvViewMatrix() { return invViewMatrix; }
  const glm::vec3 &GetPosition() const { return position; }

  // Camera settings
  void SetFOV(float fovDeg);
  void SetAspect(float aspectRatio);
  void SetNearFar(float nearPlane, float farPlane);

private:
  // Updates view matrix after movement/rotation
  void UpdateViewMatrix();

  // Set Cursor lock state
  void SetCursorLocked(bool locked);

  SDL_Window *pWindow;

  glm::vec3 position;
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 worldUp;

  float yaw;   // horizontal rotation (degrees)
  float pitch; // vertical rotation (degrees)
  float movementSpeed;
  float mouseSensitivity;

  float fov;
  float aspect;
  float nearPlane;
  float farPlane;

  glm::mat4 viewMatrix;
  glm::mat4 invViewMatrix;

  glm::mat4 projectionMatrix;
  glm::mat4 invProjectionMatrix;

  // Mouse handling
  bool cursorLocked;
};

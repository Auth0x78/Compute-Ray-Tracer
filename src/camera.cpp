#include "camera.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(SDL_Window *window, float fovDeg, float aspectRatio,
               float nearPlane, float farPlane, const glm::vec3 &startPos,
               const glm::vec3 &upDir)
    : pWindow(window), position(startPos), worldUp(upDir), yaw(-90.0f),
      pitch(0.0f), movementSpeed(5.0f), mouseSensitivity(0.1f), fov(fovDeg),
      aspect(aspectRatio), nearPlane(nearPlane), farPlane(farPlane),
      cursorLocked(false) {
  front = glm::vec3(0.0f, 0.0f, -1.0f);
  UpdateViewMatrix();
  projectionMatrix =
      glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
  invProjectionMatrix = glm::inverse(projectionMatrix);
}

void Camera::SetFOV(float fovDeg) {
  fov = fovDeg;
  projectionMatrix =
      glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
  invProjectionMatrix = glm::inverse(projectionMatrix);
}

void Camera::SetAspect(float aspectRatio) {
  aspect = aspectRatio;
  projectionMatrix =
      glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
  invProjectionMatrix = glm::inverse(projectionMatrix);
}

void Camera::SetNearFar(float nearP, float farP) {
  nearPlane = nearP;
  farPlane = farP;
  projectionMatrix =
      glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
  invProjectionMatrix = glm::inverse(projectionMatrix);
}

void Camera::Update(float deltaTime) {
  const bool *keyStates = SDL_GetKeyboardState(nullptr);

  glm::vec3 moveDir(0.0f);

  if (keyStates[SDL_SCANCODE_W])
    moveDir += front;
  if (keyStates[SDL_SCANCODE_S])
    moveDir -= front;
  if (keyStates[SDL_SCANCODE_A])
    moveDir -= right;
  if (keyStates[SDL_SCANCODE_D])
    moveDir += right;
  if (keyStates[SDL_SCANCODE_SPACE])
    moveDir += worldUp;
  if (keyStates[SDL_SCANCODE_LCTRL])
    moveDir -= worldUp;

  if (glm::length(moveDir) > 0.0f)
    moveDir = glm::normalize(moveDir);

  float step = movementSpeed * deltaTime; // small step each frame
  position += moveDir * step;

  if (glm::distance(position, glm::vec3(0.0f)) != 0.0f)
    UpdateViewMatrix();
}

void Camera::ProcessEvent(const SDL_Event &event) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    if (!cursorLocked)
      return;

    // SDL gives relative delta directly
    float xoffset = float(event.motion.xrel) * mouseSensitivity;
    float yoffset = float(-event.motion.yrel) * mouseSensitivity; // invert Y

    yaw += xoffset;
    pitch += yoffset;

    // Clamp pitch to avoid gimbal flip
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    // Update direction vectors
    glm::vec3 f;
    f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    f.y = sin(glm::radians(pitch));
    f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(f);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));

    UpdateViewMatrix();
  } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat == 0) {
    if (event.key.scancode == SDL_SCANCODE_TAB) {
      SetCursorLocked(!cursorLocked);
    }
  }
}

void Camera::UpdateViewMatrix() {
  viewMatrix = glm::lookAt(position, position + front, up);
  invViewMatrix = glm::inverse(viewMatrix);
}

void Camera::SetCursorLocked(bool locked) {
  cursorLocked = locked;
  float mx, my;
  SDL_GetMouseState(&mx, &my);
  static glm::vec2 posBeforeLock(mx, my);

  if (locked) {
    SDL_GetMouseState(&mx, &my);
    posBeforeLock = glm::vec2(mx, my);
    SDL_SetWindowRelativeMouseMode(pWindow, true);
  } else {
    SDL_WarpMouseInWindow(pWindow, posBeforeLock.x, posBeforeLock.y);
    SDL_SetWindowRelativeMouseMode(pWindow, false);
  }
}

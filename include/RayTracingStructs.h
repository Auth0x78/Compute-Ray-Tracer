#pragma once
#include <cstddef>  // offsetof
#include <type_traits>

#include <glm/glm.hpp>
// === Shared GPU <-> CPU structures (std430 layout) ===


__pragma(pack(push, 1))
// RayTracingMaterial (64 bytes)
struct RayTracingMaterial {
    glm::vec4 color;              // offset 0
    glm::vec4 emissionColor;      // offset 16
    glm::vec4 specularColor;      // offset 32

    float emissionStrength;       // offset 48
    float smoothness;             // offset 52
    float specularProbability;    // offset 56
    int   flag;                   // offset 60
    // stride = 64
};

// Model (208 bytes)
struct Model {
    int nodeOffset; // offset 0
    int triOffset;  // offset 4
	float _pad0[2];    // offset 8

    glm::mat4 worldToLocalMatrix;   // offset 16
    glm::mat4 localToWorldMatrix;   // offset 80

    RayTracingMaterial material;    // offset 144
    // stride = 208
};

// BVHNode (48 bytes)
struct BVHNode {
    glm::vec3 boundsMin; float _pad0;  // offset 0
    glm::vec3 boundsMax; // offset 16
    
    int startIndex; // offset 28
    int triangleCount; // offset 32
    int _pad1;    // offset 
    int _pad2;    // offset 
    int _pad3;    // offset 

    BVHNode() : boundsMin(glm::vec3(FLT_MAX)),
        boundsMax(glm::vec3(FLT_MIN)),
        startIndex(0),
        triangleCount(-1),
        _pad0(FP_NAN),
        _pad1(0),
        _pad2(0),
        _pad3(0)
    {}
};

// Triangle (96 bytes)
struct Triangle {
    glm::vec3 posA; float _pad0;    // offset 0
    glm::vec3 posB; float _pad1;    // offset 16
    glm::vec3 posC; float _pad2;    // offset 32

    glm::vec3 normA; float _pad3;    // offset 48
    glm::vec3 normB; float _pad4;    // offset 64
    glm::vec3 normC; float _pad5;    // offset 80
    // stride = 96
};
__pragma(pack(pop))


// === Compile-time checks ===
static_assert(sizeof(RayTracingMaterial) == 64, "RayTracingMaterial must be 64 bytes");
static_assert(offsetof(RayTracingMaterial, color) == 0);
static_assert(offsetof(RayTracingMaterial, emissionColor) == 16);
static_assert(offsetof(RayTracingMaterial, specularColor) == 32);
static_assert(offsetof(RayTracingMaterial, emissionStrength) == 48);
static_assert(offsetof(RayTracingMaterial, smoothness) == 52);
static_assert(offsetof(RayTracingMaterial, specularProbability) == 56);
static_assert(offsetof(RayTracingMaterial, flag) == 60);

static_assert(sizeof(Model) == 208, "Model must be 208 bytes");
static_assert(offsetof(Model, nodeOffset) == 0);
static_assert(offsetof(Model, triOffset) == 4);
static_assert(offsetof(Model, worldToLocalMatrix) == 16);
static_assert(offsetof(Model, localToWorldMatrix) == 80);
static_assert(offsetof(Model, material) == 144);

static_assert(sizeof(BVHNode) == 48, "BVHNode must be 48 bytes");
static_assert(offsetof(BVHNode, boundsMin) == 0);
static_assert(offsetof(BVHNode, boundsMax) == 16);
static_assert(offsetof(BVHNode, startIndex) == 28);
static_assert(offsetof(BVHNode, triangleCount) == 32);

static_assert(sizeof(Triangle) == 96, "Triangle must be 96 bytes");
static_assert(offsetof(Triangle, posA) == 0);
static_assert(offsetof(Triangle, posB) == 16);
static_assert(offsetof(Triangle, posC) == 32);
static_assert(offsetof(Triangle, normA) == 48);
static_assert(offsetof(Triangle, normB) == 64);
static_assert(offsetof(Triangle, normC) == 80);

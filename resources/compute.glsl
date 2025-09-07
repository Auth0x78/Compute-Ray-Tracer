#version 440 

// CONSTANTS
#define RAYS_PER_PIXEL 128
const float inf = 1. / 0.;

// Thanks to Sebastian-Lague for shader
// https://github.com/SebLague/Ray-Tracing/blob/main/Assets/Scripts/Shaders/RayTracer.shader


// Shader Property
layout (local_size_x = RAYS_PER_PIXEL, local_size_y = 1) in;

// include RNG functions
#pragma include "rng.glsl"

// Defined Data Structures
struct Ray {
    vec3 origin;
    vec3 dir;
    vec3 invDir;
};

struct Triangle {
    vec3 posA;
    vec3 posB;
    vec3 posC;
  
    vec3 normA;
    vec3 normB;
    vec3 normC;
};

struct TriangleHitInfo {
    bool didHit;
    float dst;
    vec3 hitPoint;
    vec3 normal;
    int triIndex;
};

struct RayTracingMaterial {
    vec4 color;
    vec4 emissionColor;
    vec4 specularColor;
    float emissionStrength;
    float smoothness;
    float specularProbability;
    int flag;
};

struct Model {
    int nodeOffset;
    int triOffset;
    mat4 worldToLocalMat;
    mat4 localToWorldMat;
    RayTracingMaterial material;
};

struct BVHNode {
    vec3 boundsMin;
    vec3 boundsMax;
    // If triangleCount > 0, then this is leaf node
    int startIndex;
    int triangleCount;
};

struct ModelHitInfo {
    bool didHit;
    float dst;
    vec3 hitPoint;
    vec3 normal;
    RayTracingMaterial material;
};

// Shader Inputs
// TODO: Allocate a large enough buffer and take count as param and only update needed data from cpu side
layout (rgba32f, binding = 0) uniform image2D outputImage;

// Scene Data SSBOs
layout (std430, binding = 1) buffer ModelsBuffer {
    Model ModelInfo[];
};

layout (std430, binding = 2) buffer BVHBuffer {
    BVHNode Nodes[];
};

layout (std430, binding = 3) buffer TrianglesBuffer {
    Triangle Triangles[];
};

shared vec3 ccontrib[RAYS_PER_PIXEL]; // store per-thread contribution

// Shader uniforms

uniform vec2 uResolution;
uniform float uTime;
uniform int uFrame;

// Ray tracer property
vec3 camPos = vec3(0.0, 0.0, 0.0);
vec3 debugColor = vec3(1.0, 0.0, 1.0);
float CameraZViewDir = -1.0;

const int UseSky = 1;
const vec3 GroundColour = vec3(0.35, 0.3, 0.35);
const vec3 SkyColourHorizon = vec3(1.0);
const vec3 SkyColourZenith = vec3(0.38, 0.45, 0.8);

const vec3 SunDirection = normalize(vec3(0.8, 0.6, -0.1));
const vec3 SunColor = vec3(1.0, 0.9, 0.6);
const float SunIntensity = 10.0;
const float SunFocus = 500.0;

const int maxBounces = 3;
const float maxDist = 1e8;
const float minDist = 1e-8;

// For testing right now
mat4 CamLocalToWorldMatrix = mat4(1.0);
vec3 viewParams = vec3(2.0, 2.0, 1.0);
vec3 _WorldSpaceCameraPos = CamLocalToWorldMatrix[3].xyz;
// Camera settings
float DefocusStrength = 1.0;
float DivergeStrength = 0.5;


// Crude Sky color function for ambient light
vec3 GetEnvironmentLight(vec3 dir) {
    if(UseSky == 0)
        return vec3(0.0);
    uint state = 1;
    float skyGradientT = pow(smoothstep(0.0, 0.4, dir.y), 0.35);
    float groundToSkyT = smoothstep(-0.01, 0.0, dir.y);
    
    vec3 skyGradient = mix(SkyColourHorizon, SkyColourZenith, skyGradientT);

    float sun = pow(max(0, dot(dir, SunDirection)), SunFocus) * SunIntensity;
    // Combine all 3
    vec3 composite = mix(GroundColour, skyGradient, groundToSkyT) + sun * SunColor * float(groundToSkyT >= 1);

    return composite;
}

// Ray Intersection function
TriangleHitInfo RayTriangle(Ray ray, Triangle tri) {
    vec3 edgeAB = tri.posB - tri.posA;
    vec3 edgeAC = tri.posC - tri.posA;
    vec3 normVec = cross(edgeAB, edgeAC);
    vec3 ao = ray.origin - tri.posA;
    vec3 dao = cross(ao, ray.dir);

    float det = -dot(ray.dir, normVec);
    float invDet = 1 / det;

    // Calculate dst to triangle & baycentric coords
    float dst= dot(ao, normVec) * invDet;
	float u = dot(edgeAC, dao) * invDet;
	float v = -dot(edgeAB, dao) * invDet;
	float w = 1 - u - v;


    TriangleHitInfo hitInfo;
    hitInfo.didHit = det >= 1E-8 && dst >= 0 && u >= 0 && v >= 0 && w >= 0;
    hitInfo.dst = dst;
    hitInfo.hitPoint = ray.origin + ray.dir * dst;
    hitInfo.normal = normalize(w * tri.normA + u * tri.normB + v * tri.normC);
    return hitInfo;
}

// Thanks to https://tavianator.com/2011/ray_box.html
float RayBoundingBoxDst(Ray ray, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - ray.origin) * ray.invDir;
    vec3 tMax = (boxMax - ray.origin) * ray.invDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    bool hit = tFar >= tNear && tFar > 0;
    float dst = hit ? (tNear > 0 ? tNear : 0) : inf;
    return dst;
}

TriangleHitInfo RayTriangleBVH(inout Ray ray, float rayLength, int nodeOffset, int triOffset) {
    TriangleHitInfo result;
    result.didHit = false;
    result.dst = rayLength;
    result.triIndex = -1;

    int stack[32];
    int stackIndex = 0;
    // Push current node onto stack
    stack[stackIndex++] = nodeOffset;

    // Essentially while stack not empty
    while(stackIndex > 0) {
        int nodeIdx = stack[--stackIndex];
        // if(nodeIdx >= Nodes.length() || nodeIdx < 0)
        //     return result;
        
        BVHNode node = Nodes[nodeIdx];

        if(node.triangleCount > 0) {
            for(int i = 0; i < node.triangleCount; ++i) {
                // out of bounds check here pls
                // if(triOffset + node.startIndex + i >= Triangles.length() || triOffset + node.startIndex + i < 0)
                //     return result;
                Triangle tri = Triangles[triOffset + node.startIndex + i];
                TriangleHitInfo triHitInfo = RayTriangle(ray, tri);

                if(triHitInfo.didHit && triHitInfo.dst < result.dst) {
                    result = triHitInfo;
                    result.triIndex = node.startIndex + i;
                }
            }
        } 
        else {
            int leftChildIndex = nodeOffset + node.startIndex + 0;
            int rightChildIndex = nodeOffset + node.startIndex + 1; 
            // if(leftChildIndex >= Nodes.length() || rightChildIndex >= Nodes.length() || leftChildIndex < 0 || rightChildIndex < 0)
            //     return result;

            BVHNode leftChild = Nodes[leftChildIndex];
            BVHNode rightChild = Nodes[rightChildIndex];

            float dstLeft = RayBoundingBoxDst(ray, leftChild.boundsMin, leftChild.boundsMax);
            float dstRight = RayBoundingBoxDst(ray, rightChild.boundsMin, rightChild.boundsMax);

            bool isLeftNear = dstLeft <= dstRight;
            float dstNear = isLeftNear ? dstLeft : dstRight;
            float dstFar = isLeftNear ? dstRight : dstLeft;
            int childIndexNear = isLeftNear ? leftChildIndex : rightChildIndex;
            int childIndexFar = isLeftNear ? rightChildIndex : leftChildIndex;

            // If current box distance is less than last recorded
            // then, push node onto stack
            if (dstFar < result.dst) stack[stackIndex++] = childIndexFar;
            if (dstNear < result.dst) stack[stackIndex++] = childIndexNear;
        }
    }

    return result;
}

ModelHitInfo CalculateRayCollision(Ray worldRay) {
    ModelHitInfo result;
    result.didHit = false;
    result.dst = inf;
    Ray localRay;

    int modelCount = ModelInfo.length();

    for(int i = 0; i < modelCount; ++i)  {
        Model model = ModelInfo[i];
        
        // Transform ray into model's local coordinate system
        localRay.origin = (model.worldToLocalMat * vec4(worldRay.origin, 1.0)).xyz;
        localRay.dir = (model.worldToLocalMat * vec4(worldRay.dir, 0.0)).xyz;
        localRay.invDir = 1 / localRay.dir;

        // Transform bvh to find closest triangle intersection with current model
        TriangleHitInfo hit = RayTriangleBVH(localRay, result.dst, model.nodeOffset, model.triOffset);
        
        if(hit.dst < result.dst) {
            result.didHit = true;
            result.dst = hit.dst;
            result.normal = normalize(model.localToWorldMat * vec4(hit.normal, 0.0)).xyz;
            result.hitPoint = worldRay.origin + worldRay.dir * hit.dst;
            result.material = model.material;
        }

    }

    return result;
}

vec2 mod2(vec2 x, vec2 y) {
    return x - y * floor(x / y);
}

vec3 Trace(vec3 rayOrigin, vec3 rayDir, inout uint rngState) {
    vec3 incomingLight = vec3(0.0);
    vec3 rayColor = vec3(1.0);

    float dstSum = 0.0;
    for(int bounceIndex = 0; bounceIndex <= maxBounces; ++bounceIndex) {
        Ray ray;
        ray.origin = rayOrigin;
        ray.dir = rayDir;
        ModelHitInfo hitInfo = CalculateRayCollision(ray);

        if(hitInfo.didHit) {
            dstSum += hitInfo.dst;
            RayTracingMaterial material = hitInfo.material;

            // IDK, Seb's code had these flags so kept them
            // TODO: Understand what these flags do
            if(material.flag == 1) 
            {
                vec2 c = mod2(floor(hitInfo.hitPoint.xy), vec2(2.0));
                material.color = (c.x == c.y) ? material.color : material.emissionColor;
            }

            // Figure out new ray pos & dir
            bool isSpecularBounce = material.specularProbability >= RandomValue(rngState);

            rayOrigin = hitInfo.hitPoint;
            vec3 diffuseDir = normalize(hitInfo.normal + RandomDirection(rngState));
            vec3 specularDir = reflect(rayDir, hitInfo.normal);

            // Might cause problem, bool implicit conv to int
            rayDir = normalize(mix(diffuseDir, specularDir, material.smoothness * float(isSpecularBounce)));

            // Update light calculation
            vec3 emittedLight = material.emissionColor.rgb * material.emissionStrength;
            incomingLight += emittedLight * rayColor;
            // This too might cause problem, bool conv. to int
            rayColor *= mix(material.color, material.specularColor, isSpecularBounce).rgb;

            // Random early exit if rayColor is nearly 0 (no contrib. to final color anyways)
            float p = max(rayColor.r, max(rayColor.g, rayColor.b));
            if(RandomValue(rngState) >= p) {
                break;
            }
            rayColor *= 1.0 / p;
        } else {
            incomingLight += GetEnvironmentLight(rayDir) * rayColor;
            break;
        }
    }

    return incomingLight;
}

// BIG ASSUMPTION: I am assuming the i.uv in seb's code is from 0.0, 0.0(bot-left) to 1.0, 1.0(top-right)
void main() {
    // Local Thread ID
    uint localThreadID = gl_LocalInvocationID.x;

    ivec2 pixelCoord = ivec2(gl_WorkGroupID.xy);
    int pixelIndex = pixelCoord.x + pixelCoord.y * int(uResolution.x);
    vec2 uv = vec2(pixelCoord) / uResolution; 

    // Create a rngState
    uint rngState = pixelIndex + uFrame * 719393 + localThreadID * 16943;

    // Calculate focal point
    vec3 focusPointLocal = vec3(uv - vec2(0.5), 1.0) * viewParams;

    // --- Transform focus point into world space ---
    vec3 focusPoint = (CamLocalToWorldMatrix * vec4(focusPointLocal, 1.0)).xyz;
     
    // --- Extract camera right and up vectors ---
    vec3 camRight = CamLocalToWorldMatrix[0].xyz; // Column 0
    vec3 camUp = CamLocalToWorldMatrix[1].xyz; // Column 1

    // Trace multiple rays and average together (done at last)
    // Here, I have 32 threads running for each pixel, & 1 ray per thread
    // So we will have overall 32 rays per pixel
    
    // Calculate ray origin and direction
    vec2 defocusJitter = RandomPointInCircle(rngState) * DefocusStrength / uResolution.x;
    vec3 rayOrigin = _WorldSpaceCameraPos + camRight * defocusJitter.x + camUp * defocusJitter.y;
    
    // Jitter the focus point when calculating the ray direction to allow for blurring the image
    // (at low strengths, this can be used for anti-aliasing)
    vec2 jitter = RandomPointInCircle(rngState) * DivergeStrength / uResolution.x;
    vec3 jitteredFocusPoint = focusPoint + camRight * jitter.x + camUp * jitter.y;
    vec3 rayDir = normalize(jitteredFocusPoint - rayOrigin);

    // Trace the ray
    ccontrib[localThreadID] = Trace(rayOrigin, rayDir, rngState);
    
    barrier(); // Synchronize threads in the workgroup

    //Only thread - 0 writes the final value
    if(localThreadID == 0) { 
        vec3 val = vec3(0.0);

        //#pragma optionNV(unroll all)
        for(int i = 0; i < RAYS_PER_PIXEL; i++)
            val += ccontrib[i];
        
        val /= float(RAYS_PER_PIXEL); // Average contribution
        imageStore(outputImage, pixelCoord, vec4(val, 1.0));
    }
}
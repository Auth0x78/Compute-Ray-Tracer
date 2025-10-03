#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <vector>

#include "OBJ_Loader.h"
#include "RayTracingStructs.h"

#define MAX_SPLIT_RES 6
#define MAX_SPLIT_DEPTH 32
#define MIN_TRIANGLES_PER_NODE 2

#define GRANULAR_SPLIT_TEST_BOUNDS 20
#define SPLIT_TEST_MIN 4
#define SPLIT_TEST_MAX 8

std::vector<Model> g_modelsBuffer;
std::vector<BVHNode> g_BVHBuffer;
std::vector<Triangle> g_TrianglesBuffer;

static std::unordered_map<std::string, int> s_modelMap;
static int s_modelNodeOffset = 0;
static int s_modelTriOffset = 0;

static struct AABB {
  glm::vec3 min;
  glm::vec3 max;

  AABB()
      : min(std::numeric_limits<float>::max()),
        max(-std::numeric_limits<float>::max()) {}

  void fitTris(const Triangle &tris) {
    min = glm::min(min, tris.posA);
    max = glm::max(max, tris.posA);

    min = glm::min(min, tris.posB);
    max = glm::max(max, tris.posB);

    min = glm::min(min, tris.posC);
    max = glm::max(max, tris.posC);
  }

  float costAABB(int numTris) const {
    if (numTris == 0)
      return 0.0f;

    // If degenerate AABB
    glm::vec3 d = max - min;
    if (d.x <= 0.0f || d.y <= 0.0f || d.z <= 0.0f)
      return 0.0f;

    return numTris * (d.x * d.y + d.y * d.z + d.z * d.x);
  }
};

static float NodeCost(float x, float y, float z, int numTris) {
  if (numTris == 0)
    return 0;
  float area = x * y + y * z + z * x;
  return area * numTris;
}

static void expandToFit(int idx, const glm::vec3 &point) {
  BVHNode &node = g_BVHBuffer[idx];

  node.boundsMin = glm::min(node.boundsMin, point);
  node.boundsMax = glm::max(node.boundsMax, point);
}

static void expandToFitTris(int idx, const Triangle &triangle) {
  expandToFit(idx, triangle.posA);
  expandToFit(idx, triangle.posB);
  expandToFit(idx, triangle.posC);
}

static float EvaluateSplit(char splitAxis, float splitPos, int start,
                           int count) {
  int64_t numLeftTris = 0;
  int64_t numRightTris = 0;
  AABB leftAABB;
  AABB rightAABB;

  size_t end = (size_t)start + count;

  for (size_t i = start; i < end; ++i) {
    Triangle &tri = g_TrianglesBuffer[i];

    float c = 0.0f;
    switch (splitAxis) {
    case 'X':
      c = (tri.posA.x + tri.posB.x + tri.posC.x) / 3.0f;
      break;
    case 'Y':
      c = (tri.posA.y + tri.posB.y + tri.posC.y) / 3.0f;
      break;
    default:
      c = (tri.posA.z + tri.posB.z + tri.posC.z) / 3.0f;
      break;
    }

    if (c < splitPos) {
      leftAABB.fitTris(tri);
      numLeftTris++;
    } else {
      rightAABB.fitTris(tri);
      numRightTris++;
    }
  }

  float costLeft = leftAABB.costAABB(numLeftTris);
  float costRight = rightAABB.costAABB(numRightTris);

  return costLeft + costRight;
}

static std::tuple<char, float, float> chooseSplit(const BVHNode &node,
                                                  int start, int count) {
  if (count <= 1)
    return {'X', 0.0f, std::numeric_limits<float>::max()};
  float sizeX = node.boundsMax.x - node.boundsMin.x;
  float sizeY = node.boundsMax.y - node.boundsMin.y;
  float sizeZ = node.boundsMax.z - node.boundsMin.z;

  float bestSplitPos = 0.0f;
  char bestSplitAxis = 'X';
  int maxSplitTest =
      count < GRANULAR_SPLIT_TEST_BOUNDS ? SPLIT_TEST_MIN : SPLIT_TEST_MAX;

  float maxAxis = std::max({sizeX, sizeY, sizeZ});
  float bestCost = std::numeric_limits<float>::max();

  // Estimate best split
  for (char axis = 'X'; axis <= 'Z'; ++axis) {
    float axisSize;
    float axisMin;

    switch (axis) {
    case 'X':
      axisSize = sizeX;
      axisMin = node.boundsMin.x;
      break;
    case 'Y':
      axisSize = sizeY;
      axisMin = node.boundsMin.y;
      break;
    default:
      axisSize = sizeZ;
      axisMin = node.boundsMin.z;
      break;
    }

    int numSplitTests = ceil(axisSize / maxAxis * maxSplitTest);
    numSplitTests = std::clamp(numSplitTests, 1, maxSplitTest);

    for (int i = 0; i < numSplitTests; ++i) {
      float splitT = (i + 1) / (numSplitTests + 1.0f);
      float splitPos = axisMin + axisSize * splitT;
      float cost = EvaluateSplit(axis, splitPos, start, count);

      if (cost < bestCost) {
        bestCost = cost;
        bestSplitPos = splitPos;
        bestSplitAxis = axis;
      }
    }
  }

  return {bestSplitAxis, bestSplitPos, bestCost};
}

static void splitBVH(int parentIndex, int trisGlobalStart, int triNum,
                     int depth = 0) {
  // Implemented splitting using SAH
  BVHNode &parent = g_BVHBuffer[parentIndex];

  float sizeX = parent.boundsMax.x - parent.boundsMin.x;
  float sizeY = parent.boundsMax.y - parent.boundsMin.y;
  float sizeZ = parent.boundsMax.z - parent.boundsMin.z;

  float parentCost = NodeCost(sizeX, sizeY, sizeZ, parent.triangleCount);

  auto &[splitAxis, splitPos, cost] =
      chooseSplit(parent, trisGlobalStart, triNum);

  if (cost < parentCost && depth < MAX_SPLIT_DEPTH) {
    AABB leftAABB;
    AABB rightAABB;
    int numLeft = 0;

    for (size_t i = trisGlobalStart; i < (size_t)trisGlobalStart + triNum;
         ++i) {
      Triangle tri = g_TrianglesBuffer[i];

      float c = 0.0f;
      switch (splitAxis) {
      case 'X':
        c = (tri.posA.x + tri.posB.x + tri.posC.x) / 3.0f;
        break;
      case 'Y':
        c = (tri.posA.y + tri.posB.y + tri.posC.y) / 3.0f;
        break;
      default:
        c = (tri.posA.z + tri.posB.z + tri.posC.z) / 3.0f;
        break;
      }

      if (c < splitPos) {
        leftAABB.fitTris(tri);
        std::swap(g_TrianglesBuffer[i],
                  g_TrianglesBuffer[(size_t)trisGlobalStart + numLeft]);
        numLeft++;
      } else {
        rightAABB.fitTris(tri);
      }
    }

    int numRight = triNum - numLeft;
    int trisStartLeft = trisGlobalStart + 0;
    int trisStartRight = trisGlobalStart + numLeft;

    BVHNode childLeft;
    childLeft.boundsMin = leftAABB.min;
    childLeft.boundsMax = leftAABB.max;
    childLeft.startIndex = trisStartLeft;
    childLeft.triangleCount = numLeft;

    // Push the bvh left node onto vector
    int childLeftIndex = g_BVHBuffer.size();
    g_BVHBuffer.push_back(childLeft);

    BVHNode childRight;
    childRight.boundsMin = rightAABB.min;
    childRight.boundsMax = rightAABB.max;
    childRight.startIndex = trisStartRight;
    childRight.triangleCount = numRight;

    // Push the bvh right node onto vector
    int childRightIndex = g_BVHBuffer.size();
    g_BVHBuffer.push_back(childRight);

    // parent ref is may not be valid after pushing elements onto vector
    g_BVHBuffer[parentIndex].startIndex = childLeftIndex;
    g_BVHBuffer[parentIndex].triangleCount = -1;

    // Recursively split the bvh node further
    splitBVH(childLeftIndex, trisGlobalStart, numLeft, depth + 1);
    splitBVH(childRightIndex, trisGlobalStart + numLeft, numRight, depth + 1);
  }
}

// Make the top level BVH Node
static void makeRootBVH(int trisCount) {

  // Make a BVH node
  g_BVHBuffer.emplace_back();
  // Relative indexing to s_modelNodeOffset
  // int relative = (g_BVHBuffer.size() - 1) - s_modelNodeOffset = 0 (for root
  // always);

  g_BVHBuffer[(size_t)s_modelNodeOffset + 0].startIndex = 0;
  g_BVHBuffer[(size_t)s_modelNodeOffset + 0].triangleCount = trisCount;

  for (int i = 0; i < trisCount; ++i) {
    expandToFitTris(s_modelNodeOffset + 0,
                    g_TrianglesBuffer[(size_t)s_modelTriOffset + 0 + i]);
  }

  // Split root into smaller BVH nodes
  // TODO: for multiple model the trisGlobalStart maybe not be 0 for all models
  splitBVH(0, 0, trisCount);
}

// Return -1 if model failed to load, else model's position in g_modelsBuffer
int LoadModel(const char *modelPath, std::string internalModelName) {
  objl::Loader loader;
  bool loadout = loader.LoadFile(modelPath);

  // Check to see if file is loaded
  if (!loadout)
    return -1;

  int modelIdx = g_modelsBuffer.size();
  g_modelsBuffer.emplace_back();

  s_modelMap[internalModelName] = modelIdx;
  g_modelsBuffer[modelIdx].triOffset = g_TrianglesBuffer.size();

  // Load all triangles into the g_TrianglesBuffer
  int meshSize = loader.LoadedMeshes.size();
  int trisCount = 0;
  for (int i = 0; i < meshSize; ++i) {
    const objl::Mesh &currMesh = loader.LoadedMeshes[i];
    const auto &verts = currMesh.Vertices;
    const auto &indices = currMesh.Indices;

    int indicesCount = currMesh.Indices.size();
    for (int idx = 0; idx < indicesCount; idx += 3) {
      Triangle &tri = g_TrianglesBuffer.emplace_back();
      trisCount++;

      tri.posA = glm::vec3(verts[indices[idx + 0]].Position.X,
                           verts[indices[idx + 0]].Position.Y,
                           verts[indices[idx + 0]].Position.Z);
      tri.posB = glm::vec3(verts[indices[idx + 1]].Position.X,
                           verts[indices[idx + 1]].Position.Y,
                           verts[indices[idx + 1]].Position.Z);
      tri.posC = glm::vec3(verts[indices[idx + 2]].Position.X,
                           verts[indices[idx + 2]].Position.Y,
                           verts[indices[idx + 2]].Position.Z);

      tri.normA = glm::vec3(verts[indices[idx + 0]].Normal.X,
                            verts[indices[idx + 0]].Normal.Y,
                            verts[indices[idx + 0]].Normal.Z);
      tri.normB = glm::vec3(verts[indices[idx + 1]].Normal.X,
                            verts[indices[idx + 1]].Normal.Y,
                            verts[indices[idx + 1]].Normal.Z);
      tri.normC = glm::vec3(verts[indices[idx + 2]].Normal.X,
                            verts[indices[idx + 2]].Normal.Y,
                            verts[indices[idx + 2]].Normal.Z);
    }
  }
  // If no triangles were loaded, remove the model and return -1
  if (trisCount == 0) {
    g_modelsBuffer.pop_back();
    s_modelMap.erase(internalModelName);
    return -1;
  }

  // Set offsets so that everyone can use it
  s_modelNodeOffset = g_BVHBuffer.size();
  s_modelTriOffset = g_modelsBuffer[modelIdx].triOffset;

  // Expects us to make sure triangles are ready in buffer
  g_modelsBuffer[modelIdx].nodeOffset = g_BVHBuffer.size();
  makeRootBVH(trisCount);

  // TODO:
  // Give all model default RayTracingMaterial
  RayTracingMaterial &rtMat = g_modelsBuffer[modelIdx].material;
  rtMat.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
  rtMat.emissionColor = glm::vec4(0.0f);
  rtMat.specularColor = glm::vec4(1.0f);

  rtMat.emissionStrength = 0.0f;
  rtMat.smoothness = 1.0f;
  rtMat.specularProbability = 0.5f;

  rtMat.flag = 0;

  // TODO:
  // Default Model position
  glm::mat4 &localToWorldMat = g_modelsBuffer[modelIdx].localToWorldMatrix;

  localToWorldMat = glm::mat4(1.0f);
  localToWorldMat = glm::translate(g_modelsBuffer[modelIdx].localToWorldMatrix,
                                   glm::vec3(0.0f, 0.0f, 1.0f));
  localToWorldMat =
      glm::rotate(g_modelsBuffer[modelIdx].localToWorldMatrix,
                  glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  localToWorldMat =
      glm::scale(g_modelsBuffer[modelIdx].localToWorldMatrix, glm::vec3(1.0f));

  // Find the inverse
  g_modelsBuffer[modelIdx].worldToLocalMatrix =
      glm::inverse(g_modelsBuffer[modelIdx].localToWorldMatrix);

  return modelIdx;
}
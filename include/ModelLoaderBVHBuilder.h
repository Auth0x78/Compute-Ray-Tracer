#include <glm/gtc/constants.hpp> // for glm::radians
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <vector>

#include "OBJ_Loader.h"
#include "RayTracingStructs.h"

#define MAX_SPLIT_RES 6
#define MAX_SPLIT_DEPTH 32
#define MIN_TRIANGLES_PER_NODE 2

static std::unordered_map<std::string, int> s_modelMap;

std::vector<Model> g_modelsBuffer;
std::vector<BVHNode> g_BVHBuffer;
std::vector<Triangle> g_TrianglesBuffer;

static int s_modelNodeOffset = 0;
static int s_modelTriOffset = 0;

void expandToFit(int idx, const glm::vec3 &point) {
  BVHNode &node = g_BVHBuffer[idx];

  node.boundsMin = glm::min(node.boundsMin, point);
  node.boundsMax = glm::max(node.boundsMax, point);
}

void expandToFitTris(int idx, const Triangle &triangle) {
  expandToFit(idx, triangle.posA);
  expandToFit(idx, triangle.posB);
  expandToFit(idx, triangle.posC);
}

float costFunction() {
  // TODO: Implement a cost function for SAH
  return FP_NAN;
}

void splitBVH(int relativeRootIndex, int depth = 0) {
  // FOR NOW: Using split along longest axis
  // TODO: Surface Area Heuristics

  int rootIdx = s_modelNodeOffset + relativeRootIndex;

  if (depth >= MAX_SPLIT_DEPTH ||
      g_BVHBuffer[rootIdx].triangleCount <= MIN_TRIANGLES_PER_NODE)
    return;

  // Calculate extents
  glm::vec3 center =
      (g_BVHBuffer[rootIdx].boundsMin + g_BVHBuffer[rootIdx].boundsMax) * 0.5f;
  float xExtent =
      g_BVHBuffer[rootIdx].boundsMax.x - g_BVHBuffer[rootIdx].boundsMin.x;
  float yExtent =
      g_BVHBuffer[rootIdx].boundsMax.y - g_BVHBuffer[rootIdx].boundsMin.y;
  float zExtent =
      g_BVHBuffer[rootIdx].boundsMax.z - g_BVHBuffer[rootIdx].boundsMin.z;

  // Find the longest axis
  int axis = 0; // 0: x, 1: y, 2: z
  {
    float longest = xExtent;
    if (yExtent > longest) {
      axis = 1;
      longest = yExtent;
    }
    if (zExtent > longest) {
      axis = 2;
      longest = zExtent;
    }
  }

  g_BVHBuffer.emplace_back();
  int leftIdx = g_BVHBuffer.size() - 1; // absolute address

  g_BVHBuffer.emplace_back();
  int rightIdx = g_BVHBuffer.size() - 1; // absolute address

  int numLeft = 0;
  int trisRelativeStart = g_BVHBuffer[rootIdx].startIndex;

  for (int i = s_modelTriOffset + trisRelativeStart;
       i < s_modelTriOffset + trisRelativeStart +
               g_BVHBuffer[rootIdx].triangleCount;
       ++i) {
    // Current triangle
    const Triangle &tri = g_TrianglesBuffer[i];
    glm::vec3 triCenter = (tri.posA + tri.posB + tri.posC) / 3.0f;

    if (triCenter[axis] < center[axis]) {
      expandToFitTris(leftIdx, tri);
      std::swap(g_TrianglesBuffer[i],
                g_TrianglesBuffer[(size_t)s_modelTriOffset + trisRelativeStart +
                                  numLeft]);
      numLeft++;
    } else {
      expandToFitTris(rightIdx, tri);
    }
  }

  int numRight = g_BVHBuffer[rootIdx].triangleCount - numLeft;
  int relativeLeftStart = trisRelativeStart;
  int relativeRightStart = trisRelativeStart + numLeft;

  assert(s_modelTriOffset + trisRelativeStart >= 0 &&
         s_modelTriOffset + trisRelativeStart +
                 g_BVHBuffer[rootIdx].triangleCount <=
             (int)g_TrianglesBuffer.size());
  if (numRight == 0 || numLeft == 0) {
    // left is the same as the root, no use
    g_BVHBuffer.pop_back(); // Remove right
    g_BVHBuffer.pop_back(); // Remove left
    return;
  }

  g_BVHBuffer[leftIdx].startIndex = relativeLeftStart;
  g_BVHBuffer[leftIdx].triangleCount = numLeft; // Set as leaf node

  g_BVHBuffer[rightIdx].startIndex = relativeRightStart;
  g_BVHBuffer[rightIdx].triangleCount = numRight; // Seat as leaf node

  // Mark Parent as a non-leaf node
  g_BVHBuffer[rootIdx].startIndex =
      leftIdx -
      s_modelNodeOffset; // Point to first child relative to ModelNodeOffset
  g_BVHBuffer[rootIdx].triangleCount = -1; // Mark current as non-leaf node

  // Recursively Split the left & right child
  splitBVH(leftIdx - s_modelNodeOffset, depth + 1);
  splitBVH(rightIdx - s_modelNodeOffset, depth + 1);
}

// Make the top level BVH Node
void makeRootBVH(int trisCount) {

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
  splitBVH(0);
}

// Return -1 if model failed to load, else model's position in the
// g_modelsBuffer
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
  rtMat.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
  rtMat.emissionColor = glm::vec4(0.0f);
  rtMat.specularColor = glm::vec4(1.0f);

  rtMat.emissionStrength = 0.0f;
  rtMat.smoothness = 0.5f;
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
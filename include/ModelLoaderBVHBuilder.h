#include <vector>
#include <optional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp> // for glm::radians

#include "RayTracingStructs.h"
#include "OBJ_Loader.h"

#define MAX_SPLIT_RES 6
#define MAX_SPLIT_DEPTH 32
#define MIN_TRIANGLES_PER_NODE 2

std::unordered_map <std::string, int> modelMap;

std::vector<Model> modelsBuffer;
std::vector<BVHNode> BVHBuffer;
std::vector<Triangle> TrianglesBuffer;

int modelNodeOffset = 0;
int modelTriOffset = 0;

void expandToFit(int idx, const glm::vec3& point) {
	BVHNode& node = BVHBuffer[idx];
	
	node.boundsMin = glm::min(node.boundsMin, point);
	node.boundsMax = glm::max(node.boundsMax, point);
}

void expandToFitTris(int idx, const Triangle& triangle) {
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

	int rootIdx = modelNodeOffset + relativeRootIndex;

	if (depth >= MAX_SPLIT_DEPTH || BVHBuffer[rootIdx].triangleCount <= MIN_TRIANGLES_PER_NODE)
		return;

	// Calculate extents
	glm::vec3 center = (BVHBuffer[rootIdx].boundsMin + BVHBuffer[rootIdx].boundsMax) * 0.5f;
	float xExtent = BVHBuffer[rootIdx].boundsMax.x - BVHBuffer[rootIdx].boundsMin.x;
	float yExtent = BVHBuffer[rootIdx].boundsMax.y - BVHBuffer[rootIdx].boundsMin.y;
	float zExtent = BVHBuffer[rootIdx].boundsMax.z - BVHBuffer[rootIdx].boundsMin.z;

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

	BVHBuffer.emplace_back();
	int leftIdx = BVHBuffer.size() - 1; // absolute address
	
	BVHBuffer.emplace_back();
	int rightIdx = BVHBuffer.size() - 1; // absolute address

	int numLeft = 0;
	int trisRelativeStart = BVHBuffer[rootIdx].startIndex;

	for (int i = modelTriOffset + trisRelativeStart; i < modelTriOffset + trisRelativeStart + BVHBuffer[rootIdx].triangleCount; ++i) {
		// Current triangle
		const Triangle& tri = TrianglesBuffer[i];
		glm::vec3 triCenter = (tri.posA + tri.posB + tri.posC) / 3.0f;

		if (triCenter[axis] < center[axis]) {
			expandToFitTris(leftIdx, tri);
			std::swap(TrianglesBuffer[i], TrianglesBuffer[modelTriOffset + trisRelativeStart + numLeft]);
			numLeft++;
		}
		else {
			expandToFitTris(rightIdx, tri);
		}
	}

	int numRight = BVHBuffer[rootIdx].triangleCount - numLeft;
	int relativeLeftStart = trisRelativeStart;
	int relativeRightStart = trisRelativeStart + numLeft;

	assert(modelTriOffset + trisRelativeStart >= 0 && modelTriOffset + trisRelativeStart + BVHBuffer[rootIdx].triangleCount <= (int)TrianglesBuffer.size());
	if (numRight == 0 || numLeft == 0) {
		// left is the same as the root, no use
		BVHBuffer.pop_back(); // Remove right
		BVHBuffer.pop_back(); // Remove left
		return;
	}

	BVHBuffer[leftIdx].startIndex = relativeLeftStart;
	BVHBuffer[leftIdx].triangleCount = numLeft; // Set as leaf node


	BVHBuffer[rightIdx].startIndex = relativeRightStart;
	BVHBuffer[rightIdx].triangleCount = numRight; // Seat as leaf node

	// Mark Parent as a non-leaf node 
	BVHBuffer[rootIdx].startIndex = leftIdx - modelNodeOffset; // Point to first child relative to ModelNodeOffset
	BVHBuffer[rootIdx].triangleCount = -1; // Mark current as non-leaf node

	// Recursively Split the left & right child
	splitBVH(leftIdx - modelNodeOffset, depth + 1);
	splitBVH(rightIdx - modelNodeOffset, depth + 1);
}

// Make the top level BVH Node
void makeRootBVH(int trisCount) {
	
	// Make a BVH node
	BVHBuffer.emplace_back();
	// Relative indexing to modelNodeOffset
	// int relative = (BVHBuffer.size() - 1) - modelNodeOffset = 0 (for root always);

	BVHBuffer[modelNodeOffset + 0].startIndex = 0;
	BVHBuffer[modelNodeOffset + 0].triangleCount = trisCount;

	for (int i = 0; i < trisCount; ++i) {
		expandToFitTris(modelNodeOffset + 0, TrianglesBuffer[modelTriOffset + 0 + i]);
	}

	// Split root into smaller BVH nodes
	splitBVH(0);
}

// Return -1 if model failed to load, else model's position in the modelsBuffer
int LoadModel(const char* modelPath, std::string internalModelName) {
	objl::Loader loader;
	bool loadout = loader.LoadFile(modelPath);

	// Check to see if file is loaded
	if (!loadout)
		return -1;

	int modelIdx = modelsBuffer.size();
	modelsBuffer.emplace_back();
	
	modelMap[internalModelName] = modelIdx;
	modelsBuffer[modelIdx].triOffset = TrianglesBuffer.size();
	
	// Load all triangles into the TrianglesBuffer
	int meshSize = loader.LoadedMeshes.size();
	int trisCount = 0;
	for (int i = 0; i < meshSize; ++i) {
		const objl::Mesh& currMesh = loader.LoadedMeshes[i];
		const auto& verts = currMesh.Vertices;
		const auto& indices = currMesh.Indices;
		
		int indicesCount = currMesh.Indices.size();
		for (int idx = 0; idx < indicesCount; idx += 3) {
			Triangle& tri = TrianglesBuffer.emplace_back();
			trisCount++;

			tri.posA = glm::vec3(verts[indices[idx + 0]].Position.X, verts[indices[idx + 0]].Position.Y, verts[indices[idx + 0]].Position.Z);
			tri.posB = glm::vec3(verts[indices[idx + 1]].Position.X, verts[indices[idx + 1]].Position.Y, verts[indices[idx + 1]].Position.Z);
			tri.posC = glm::vec3(verts[indices[idx + 2]].Position.X, verts[indices[idx + 2]].Position.Y, verts[indices[idx + 2]].Position.Z);

			tri.normA = glm::vec3(verts[indices[idx + 0]].Normal.X, verts[indices[idx + 0]].Normal.Y, verts[indices[idx + 0]].Normal.Z);
			tri.normB = glm::vec3(verts[indices[idx + 1]].Normal.X, verts[indices[idx + 1]].Normal.Y, verts[indices[idx + 1]].Normal.Z);
			tri.normC = glm::vec3(verts[indices[idx + 2]].Normal.X, verts[indices[idx + 2]].Normal.Y, verts[indices[idx + 2]].Normal.Z);
		}
	}
	// If no triangles were loaded, remove the model and return -1
	if(trisCount == 0) {
		modelsBuffer.pop_back();
		modelMap.erase(internalModelName);
		return -1;
	}

	// Set offsets so that everyone can use it
	modelNodeOffset = BVHBuffer.size();
	modelTriOffset = modelsBuffer[modelIdx].triOffset;
	
	// Expects us to make sure triangles are ready in buffer
	modelsBuffer[modelIdx].nodeOffset = BVHBuffer.size();
	makeRootBVH(trisCount);

	// TODO:
	// Give all model default RayTracingMaterial
	RayTracingMaterial& rtMat = modelsBuffer[modelIdx].material;
	rtMat.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	rtMat.emissionColor = glm::vec4(0.0f);
	rtMat.specularColor = glm::vec4(1.0f);

	rtMat.emissionStrength = 0.0f;
	rtMat.smoothness = 0.5f;
	rtMat.specularProbability = 0.5f;

	rtMat.flag = 0;

	// TODO:
	// Default Model position
	glm::mat4& localToWorldMat = modelsBuffer[modelIdx].localToWorldMatrix;

	localToWorldMat = glm::mat4(1.0f);
	localToWorldMat = glm::translate(modelsBuffer[modelIdx].localToWorldMatrix, glm::vec3(0.0f, 0.0f, 1.0f));
	localToWorldMat = glm::rotate(modelsBuffer[modelIdx].localToWorldMatrix, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	localToWorldMat = glm::scale(modelsBuffer[modelIdx].localToWorldMatrix, glm::vec3(1.0f));

	// Find the inverse
	modelsBuffer[modelIdx].worldToLocalMatrix = glm::inverse(modelsBuffer[modelIdx].localToWorldMatrix);

	return modelIdx;
}
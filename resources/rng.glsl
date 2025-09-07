#version 440

#define PI 3.14159265359
// RNG Functions
// PCG (permuted congruential generator). Thanks to:
// www.pcg-random.org and www.shadertoy.com/view/XlGcRh
uint NextRandom(inout uint state) {
    state = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result;
}

// Returns float value between [0..1]
float RandomValue(inout uint state) {
    return NextRandom(state) / 4294967295.0; // 2^32 - 1
}

// Random value in normal distribution (with mean=0 and sd=1)
float RandomValueNormalDistribution(inout uint state) {
    // Thanks to https://stackoverflow.com/a/6178290
    float theta = 2 * 3.1415926 * RandomValue(state);
	float rho = sqrt(-2 * log(RandomValue(state)));
	return rho * cos(theta);
}

vec3 RandomDirection(inout uint state) {
    float x = RandomValueNormalDistribution(state);
    float y = RandomValueNormalDistribution(state);
    float z = RandomValueNormalDistribution(state);

    return normalize(vec3(x, y, z));
}

vec2 RandomPointInCircle(inout uint rngState) {
    float angle = RandomValue(rngState) * 2 * PI;
	vec2 pointOnCircle = vec2(cos(angle), sin(angle));
	return pointOnCircle * sqrt(RandomValue(rngState));
}

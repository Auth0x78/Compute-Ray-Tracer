#version 440 core

in vec2 frag_uv;

layout (location = 0) out vec4 glFragColor;
uniform sampler2D screenTex;

void main() {
	vec4 pc = texture(screenTex, frag_uv);
	
	glFragColor = pc;

}
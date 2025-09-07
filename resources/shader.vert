#version 440 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 textUV;

// Program uniforms
uniform vec2 uResolution;

// Program Output
out vec2 frag_uv;

void main()
{
    frag_uv = textUV;

    // Standard fullscreen quad
    gl_Position = vec4(pos, 0.0, 1.0);
}

#version 460
#extension GL_ARB_separate_shader_objects: enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexture;

layout(location = 0) out vec4 outColor;

void main()
{
	//outColor = vec4(fragColor, 1.0);
	//outColor = texture(texSampler, fragTexture);
	outColor = vec4(fragColor * texture(texSampler, fragTexture).rgb, 1.0);
}

#version 450

// from input attachment
layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

// output color to SwapChain image
layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(subpassLoad(inputColor).rgb, 1.0f);
   	return;
	int xHalf = 800 / 2;
	if(gl_FragCoord.x > xHalf)
	{
		vec3 iColor = subpassLoad(inputColor).rgb;
		if(iColor.r > 0.9)
		{
			float temp_b = iColor.b;
			iColor.b = iColor.r;
			iColor.r = temp_b;
		}
		outColor = vec4(iColor, 1.0f);
	}
	else
	{
		outColor = vec4(subpassLoad(inputColor).rgb, 1.0f);
	}
}
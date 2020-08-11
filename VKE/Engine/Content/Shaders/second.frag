#version 450

// from input attachment
layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

// output color to SwapChain image
layout(location = 0) out vec4 outColor;

void main()
{
	/*
   	int xHalf = 800 / 2;
	if(gl_FragCoord.x > xHalf)
	{
		float lowerBound = 0.92;
		float upperBound = 1;
		
		float depth = subpassLoad(inputDepth).r;
		float depthColorScaled = 1.0f - ((depth - lowerBound) / (upperBound - lowerBound));
		outColor = vec4(subpassLoad(inputColor).rgb * depthColorScaled, 1.0f);
	}
	else
	{
		outColor = vec4(subpassLoad(inputColor).rgb, 1.0f);
	}
   */
   outColor = vec4(subpassLoad(inputColor).rgb, 1.0f);
}
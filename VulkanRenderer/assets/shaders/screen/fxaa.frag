#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D colorAttachment;
layout(binding = 1) uniform sampler2D depthAttachment;

layout(push_constant) uniform SceneFlags {
	uint flags;
} sceneFlags;

bool sceneFlag(uint mask)
{
	return flag(sceneFlags.flags, mask);
}

//This implementation is based on Timothy Lotte's original FXAA Whitepaper
// http://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
// and his public domain implementation:
// https://gist.github.com/kosua20/0c506b81b3812ac900048059d2383126


float luma(vec3 rgb)
{
	//Optimised luminance conversion from Timothy Lotte's FXAA whitepaper
	return rgb.g * (0.587/0.299) + rgb.r;
}

float sampleQuality(int i)
{
	//Corresponds to FXAA_QUALITY__PRESET==39 (extreme)
	const float qualities[12] = {
		1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0
	};

	return qualities[i];
}

//1/3 - too little; 1/4 low quality; 1/8 high quality; 1/16 overkill
const float FXAA_EDGE_THRESHOLD = 1.0 / 8.0;

//1/32 visible limit; 1/16 high quality; 1/12 upper limit
const float FXAA_EDGE_THRESHOLD_MIN = 1.0 / 16.0;

const float FXAA_SUBPIXEL_QUALITY = 1.0;

const float FXAA_SUBPIXEL_TRIM = 1.0 / 4.0;
const float FXAA_SUBPIXEL_TRIM_SCALE = 0.0;
const float FXAA_SUBPIXEL_CAP = 3.0 / 4.0;

const int FXAA_SEARCH_STEPS = 12;
const int FXAA_SEARCH_ACCELERATION = 1;
const float FXAA_SEARCH_THRESHOLD = 1.0 / 4.0;

void main()
{
	const vec3 pixel = texture(colorAttachment, uv).rgb;

	if(!sceneFlag(SCENEFLAG_ENABLEFXAA))
	{
		fragColor = vec4(pixel, 1.0);
		return;
	}

	const vec2 texelSize = 1.0 / textureSize(colorAttachment, 0);

	//
	// Local contrast check / early exit
	//

	const vec3 n = textureOffset(colorAttachment, uv, ivec2(0, -1)).rgb;
	const vec3 s = textureOffset(colorAttachment, uv, ivec2(0, 1)).rgb;
	const vec3 w = textureOffset(colorAttachment, uv, ivec2(-1, 0)).rgb;
	const vec3 e = textureOffset(colorAttachment, uv, ivec2(1, 0)).rgb;

	const float lumaM = luma(pixel);
	const float lumaW = luma(w); const float lumaE = luma(e);
	float lumaN = luma(n); float lumaS = luma(s);

	const float minLuma = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
	const float maxLuma = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));

	const float lumaRange = maxLuma - minLuma;
	if(lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, maxLuma * FXAA_EDGE_THRESHOLD))
	{
		fragColor = vec4(pixel, 1.0);
		return;
	}

	vec3 nw = textureOffset(colorAttachment, uv, ivec2(-1, -1)).rgb;
	vec3 sw = textureOffset(colorAttachment, uv, ivec2(-1, 1)).rgb;
	vec3 ne = textureOffset(colorAttachment, uv, ivec2(1, -1)).rgb;
	vec3 se = textureOffset(colorAttachment, uv, ivec2(1, 1)).rgb;

	// 
	// Neighbourhood luma test
	// 

	//float lumaL = (lN + lS + lW + lE) * 0.25;
	//float rangeL = (lumaL - lP);
	//float blendL = max(0.0, (rangeL - lumaRange) - FXAA_SUBPIXEL_TRIM);
	//blendL *= FXAA_SUBPIXEL_TRIM_SCALE;
	//blendL = min(FXAA_SUBPIXEL_CAP, blendL);

	//vec3 rgbL = n + s + w + e + pixel;
	//rgbL += (nw + sw + ne + se);
	//rgbL *= vec3(1.0/9.0);

	//
	// Vertical/horizontal edge test
	//

	float lumaNW = luma(nw); float lumaSW = luma(sw);
	float lumaNE = luma(ne); float lumaSE = luma(se);

	float lumaNS = lumaN + lumaS;
	float lumaWE = lumaW + lumaE;
	float subpixNSWE = lumaNS + lumaWE;

	float lumaNESE = lumaNE + lumaSE;
	float lumaNWNE = lumaNW + lumaNE;
	float lumaNWSW = lumaNW + lumaSW;
	float lumaSWSE = lumaSW + lumaSE;

	float subpixNWSWNESE = lumaNWSW + lumaNESE;

	float edgeVert =
		abs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE)) +
		abs((0.50 * lumaW ) + (-1.0 * lumaM) + (0.50 * lumaE )) +
		abs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));
	float edgeHorz =
		abs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW)) +
		abs((0.50 * lumaN ) + (-1.0 * lumaM) + (0.50 * lumaS )) +
		abs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));
	bool horzSpan = edgeHorz >= edgeVert;

	//
	// End-of-edge search
	//

	float lengthSign = texelSize.x;

	if(!horzSpan)
	{
		lumaN = lumaW;
		lumaS = lumaE;
	}
	else
	{
		lengthSign = texelSize.y;
	}

	float gradientN = lumaN - lumaM;
	float gradientS = lumaS - lumaM;

	float lumaNN = lumaN + lumaM;
	float lumaSS = lumaS + lumaM;

	bool pairN = abs(gradientN) >= abs(gradientS);
	float gradientScaled = max(abs(gradientN), abs(gradientS)) * 0.25;
	if(pairN) lengthSign = -lengthSign;

	vec2 posB = uv;
	vec2 offNP;
	offNP.x = (!horzSpan) ? 0.0 : texelSize.x;
	offNP.y = ( horzSpan) ? 0.0 : texelSize.y;
	if(!horzSpan) posB.x += lengthSign * 0.5;
	if( horzSpan) posB.y += lengthSign * 0.5;

	vec2 posN = posB - offNP;
	vec2 posP = posB + offNP;

	bool doneN = false;
	bool doneP = false;
	float lumaEndN, lumaEndP;

	if(!pairN) lumaNN = lumaSS;

	float lumaMM = lumaM - lumaNN * 0.5;
	bool lumaMLTZero = lumaMM < 0.0;

	for(int i = 0; i < FXAA_SEARCH_STEPS; i++)
	{
		lumaEndN = luma(textureLod(colorAttachment, posN, 0.0).rgb);
		lumaEndP = luma(textureLod(colorAttachment, posP, 0.0).rgb);

		doneN = doneN || (abs(lumaEndN - lumaNN * 0.5) >= gradientScaled);
		doneP = doneP || (abs(lumaEndP - lumaNN * 0.5) >= gradientScaled);

		if(doneN && doneP) break;

		if(!doneN) posN -= offNP * sampleQuality(i);
		if(!doneP) posP += offNP * sampleQuality(i);
	}

	//
	// Subpixel aliasing / lowpass filter
	//

	float dstN, dstP;
	if(horzSpan)
	{
		dstN = uv.x - posN.x;
		dstP = posP.x - uv.x;
	}
	else
	{
		dstN = uv.y - posN.y;
		dstP = posP.y - uv.y;
	}

	bool directionN = (dstN < dstP);
	bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
	bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
	bool goodSpan = directionN ? goodSpanN : goodSpanP;

	float spanLength = 1.0 / (dstN + dstP);
	float dst = min(dstN, dstP);

	float pixelOffset = (dst * (-spanLength)) + 0.5;
	float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;

	//
	// Subpixel lowpass
	//

	float subpixRcpRange = 1.0 / lumaRange;
	float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;
	float subpixB = (subpixA * (1.0/12.0)) - lumaM;
	float subpixC = clamp(abs(subpixB) * subpixRcpRange, 0.0, 1.0);
	float subpixD = ((-2.0) * subpixC) + 3.0;
	float subpixE = subpixC * subpixC;
	float subpixF = subpixD * subpixE;
	float subpixG = subpixF * subpixF;
	float subpixH = subpixG * FXAA_SUBPIXEL_QUALITY;
	
	//
	// Final sampling & output
	// 

	float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);

	vec2 sampledUV = uv;
	if(horzSpan)
		sampledUV.y += pixelOffsetSubpix * lengthSign;
	else
		sampledUV.x += pixelOffsetSubpix * lengthSign;

	vec3 sampledColor = textureLod(colorAttachment, sampledUV, 0.0).rgb;
	fragColor.rgb = sampledColor;
	//fragColor.rgb = pixel;
	fragColor.a = 1.0;
}


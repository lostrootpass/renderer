#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D colorAttachment;

void main()
{
    const float scanlineThickness = 3.0;
    const float scanlineDensity = 4.0;
    const float screenCoverage = 1.0;
    
    fragColor = vec4(0.0, 0.0, 1.0, 0.0);
    
    if(uv.x < screenCoverage)
    {
        float offset = 0.0;

        //Enable to curve the scanlines like a TV etc.
        //offset = sin(uv.x * 3.14) / 20.0;
        
        float blendFactor = 4.0 * ((1.0 - uv.y) / screenCoverage);
        blendFactor *= (sin(uv.y * 3.14) * 10.0);

   		float pix = floor((uv.y - offset) * 1000 / scanlineThickness);

        vec4 baseColor = texture(colorAttachment, vec2(uv.x, uv.y));

        fragColor = vec4(baseColor.xyz, 1.0);
        
    	if(mod(pix, scanlineDensity) == 0.0)
        {
        	fragColor *= vec4(fragColor.rgb - (fragColor.rgb/blendFactor), 1.0);
        }
    }
}
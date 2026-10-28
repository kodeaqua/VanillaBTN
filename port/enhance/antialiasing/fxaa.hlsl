// Pixel-frequency postprocess, independently expressed from the FXAA paper.
// Work in display-encoded RGB, with clamped texture sampling at the canvas edge.
Texture2D<float4> source_image : register(t0, space0);
SamplerState source_sampler : register(s0, space0);
RWTexture2D<float4> result_image : register(u0, space1);
float3 at(float2 uv){return source_image.SampleLevel(source_sampler,uv,0).rgb;}
float luma(float3 rgb){return dot(rgb,float3(0.299,0.587,0.114));}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID){
    uint w,h;result_image.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;
    float2 step=1.0/float2(w,h),uv=(float2(id.xy)+0.5)*step;
    float3 center=at(uv);
    float4 corners=float4(luma(at(uv+float2(-1,-1)*step)),luma(at(uv+float2(1,-1)*step)),
        luma(at(uv+float2(-1,1)*step)),luma(at(uv+float2(1,1)*step)));
    float middle=luma(center),low=min(middle,min(min(corners.x,corners.y),min(corners.z,corners.w))),
        high=max(middle,max(max(corners.x,corners.y),max(corners.z,corners.w)));
    if(high-low<max(1.0/32.0,high/8.0)){result_image[id.xy]=float4(center,1);return;}
    float2 tangent=float2(corners.z+corners.w-corners.x-corners.y,
                          corners.x+corners.z-corners.y-corners.w);
    float regularizer=max(dot(corners,float4(1,1,1,1))/32.0,1.0/128.0);
    tangent=clamp(tangent/(min(abs(tangent.x),abs(tangent.y))+regularizer),-8.0,8.0)*step;
    float3 narrow=(at(uv-tangent/6.0)+at(uv+tangent/6.0))*0.5;
    float3 broad=narrow*0.5+(at(uv-tangent*0.5)+at(uv+tangent*0.5))*0.25;
    float candidate=luma(broad);
    result_image[id.xy]=float4(candidate<low||candidate>high?narrow:broad,1);
}

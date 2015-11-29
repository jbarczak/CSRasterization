

void main(  float4 p : POSITION,
            float2 uv : TEXCOORD0,
            out float4 vPos : SV_POSITION,
            out float2 vUV : TEXCOORD0
             )
{
    vPos = float4(p.xy,0.5,1);
    vUV = uv;
}
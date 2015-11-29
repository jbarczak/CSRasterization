
cbuffer cb0
{
    row_major float4x4 mViewProj;
}
cbuffer cb1
{
    row_major float4x4 mWorld;
}

float4 main(  float4 p : POSITION ) : SV_Position
{
    p = mul( p, mWorld );
    return mul( p, mViewProj );
}
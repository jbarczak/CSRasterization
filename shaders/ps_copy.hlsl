
Texture2D<float> tx;

float4 main( float4 vpos : SV_POSITION ) : SV_TARGET
{
    return tx.Load(uint3(vpos.xy,0) ).xxxx;
}
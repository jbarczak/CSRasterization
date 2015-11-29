#define _CRT_SECURE_NO_WARNINGS
#include "Simpleton.h"
#include "PlyLoader.h"
#include "DX11/SimpletonDX11.h"

#include "shader_headers\vs_quad.h"
#include "shader_headers\ps_copy.h"
#include "shader_headers\ps_black.h"


#include "shader_headers\vs_raster_test.h"

#include "shader_headers\cs_raster.h"


class CopyQuad
{
public:

    void Init( ID3D11Device* pDev, Simpleton::DX11PipelineStateBuilder& builder )
    {
        m_ScreenQuad.InitXYQuad(pDev);
        builder.BeginState(pDev);
        builder.DisableZAndStencil();
        builder.SetInputLayout( m_ScreenQuad.GetVertexElements(), m_ScreenQuad.GetVertexElementCount() );
        builder.SetPixelShader( ps_copy, sizeof(ps_copy) );
        builder.SetVertexShader( vs_quad, sizeof(vs_quad));
        builder.EndState(&m_PSO);
        m_PSO.GetResourceSchema()->CreateResourceSet(&m_PSOResourceSet,pDev);
    }

    void Do( ID3D11RenderTargetView* pDest, ID3D11ShaderResourceView* pSrc, ID3D11DeviceContext* pCtx )
    {
        pCtx->OMSetRenderTargets(1,&pDest,0);

        m_PSO.Apply(pCtx);
        m_PSOResourceSet.BeginUpdate(pCtx);
        m_PSOResourceSet.BindSRV( "tx", pSrc );
        m_PSOResourceSet.EndUpdate(pCtx);
        m_PSOResourceSet.Apply(pCtx);
        m_ScreenQuad.Draw(pCtx);
        
    }

private:

    Simpleton::DX11Mesh m_ScreenQuad;
    Simpleton::DX11PipelineState         m_PSO;
    Simpleton::DX11PipelineResourceSet   m_PSOResourceSet;

};

class FakeZBuffer
{
public:

    void Init( size_t width, size_t height, ID3D11Device* pDev )
    {
        D3D11_TEXTURE2D_DESC td;
        td.ArraySize = 1;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_RENDER_TARGET;
        td.CPUAccessFlags=0;
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.Height = height;
        td.Width = width;
        td.MipLevels = 1;
        td.SampleDesc.Count=1;
        td.SampleDesc.Quality=0;
        td.MiscFlags=0;
        td.Usage = D3D11_USAGE_DEFAULT;

        pDev->CreateTexture2D( &td, 0,m_pFakeZBuffer.Address());

        D3D11_SHADER_RESOURCE_VIEW_DESC srv;
        srv.Format = DXGI_FORMAT_R32_FLOAT;
        srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels=1;
        srv.Texture2D.MostDetailedMip=0;
        pDev->CreateShaderResourceView( m_pFakeZBuffer, &srv, m_pSRV.Address());

        D3D11_UNORDERED_ACCESS_VIEW_DESC uav;
        uav.Format = DXGI_FORMAT_R32_UINT;
        uav.ViewDimension= D3D11_UAV_DIMENSION_TEXTURE2D;
        uav.Texture2D.MipSlice=0;
        pDev->CreateUnorderedAccessView( m_pFakeZBuffer, &uav, m_pUAV.Address());

        D3D11_RENDER_TARGET_VIEW_DESC rtv;
        rtv.Format = DXGI_FORMAT_R32_FLOAT;
        rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtv.Texture2D.MipSlice=0;
        pDev->CreateRenderTargetView( m_pFakeZBuffer, &rtv, m_pRTV.Address() );
    }

    ID3D11ShaderResourceView* GetSRV() { return m_pSRV; }
    ID3D11UnorderedAccessView* GetUAV() { return m_pUAV; }
    ID3D11RenderTargetView* GetRTV() { return m_pRTV; }

private:
    
    Simpleton::ComPtr<ID3D11Texture2D> m_pFakeZBuffer;
    Simpleton::ComPtr<ID3D11UnorderedAccessView> m_pUAV;
    Simpleton::ComPtr<ID3D11ShaderResourceView>  m_pSRV;
    Simpleton::ComPtr<ID3D11RenderTargetView>    m_pRTV;
};

#define ZOOM    2.0f
#define Y_OFFS -0.5f

class RasterTest : public Simpleton::DX11WindowController
{
public:

    Simpleton::DX11PipelineStateBuilder  m_StateBuilder;
    Simpleton::DX11PipelineState         m_PSO;
    Simpleton::DX11PipelineState         m_WirePSO;
    Simpleton::DX11PipelineResourceSet   m_PSOResourceSet;

    Simpleton::DX11ComputeKernel m_CSO;
    Simpleton::DX11ComputeResourceSet m_CSOResourceSet;

    
    Simpleton::DX11ShadowMap m_SMap;
    FakeZBuffer m_FakeZBuffer;

    CopyQuad m_CopyQuad;
    
    Simpleton::DX11Texture m_VB;
    Simpleton::DX11Texture m_IB;
    UINT m_nTriangles;
    bool m_bCompute;
    bool m_bWire;

    virtual bool OnCreate( Simpleton::DX11Window* pWin )
    {
        m_bCompute = true;
        m_bWire = false;
        D3D11_INPUT_ELEMENT_DESC il[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,0,D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        ID3D11Device* pDevice = pWin->GetDevice();
        m_StateBuilder.BeginState( pWin->GetDevice() );
        m_StateBuilder.SetInputLayout( il, 1 );
        m_StateBuilder.SetPixelShader( 0,0);
        m_StateBuilder.SetVertexShader( vs_raster_test, sizeof(vs_raster_test));
        m_StateBuilder.EndState( &m_PSO );
        m_PSO.GetResourceSchema()->CreateResourceSet( &m_PSOResourceSet, pWin->GetDevice() );
        
        D3D11_RASTERIZER_DESC rs;
        rs.CullMode = D3D11_CULL_BACK;
        rs.AntialiasedLineEnable = false;
        rs.DepthBias = -1000;
        rs.DepthBiasClamp=0;
        rs.DepthClipEnable=0;
        rs.FillMode = D3D11_FILL_WIREFRAME;
        rs.FrontCounterClockwise = false;
        rs.MultisampleEnable = false;
        rs.ScissorEnable = false;
        rs.SlopeScaledDepthBias = 0;
 

        D3D11_DEPTH_STENCIL_DESC ds;
        memset(&ds,0,sizeof(ds));
        ds.StencilEnable = false;
        ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        ds.DepthEnable= true;
    
        m_StateBuilder.BeginState( pWin->GetDevice() );
        m_StateBuilder.SetInputLayout( il, 1 );
        m_StateBuilder.SetPixelShader( ps_black, sizeof(ps_black));
        m_StateBuilder.SetVertexShader( vs_raster_test, sizeof(vs_raster_test));
        m_StateBuilder.SetRasterizerState(rs);
        m_StateBuilder.SetDepthStencilState(ds);
        m_StateBuilder.EndState( &m_WirePSO );
       
        /*
        Simpleton::PlyMesh ply;
        if( !Simpleton::LoadPly( "dragon_vrip.ply", ply, Simpleton::PF_STANDARDIZE_POSITIONS |
                                               Simpleton::PF_IGNORE_UVS            |
                                               Simpleton::PF_IGNORE_NORMALS        |
                                               Simpleton::PF_IGNORE_COLORS ) )
        {
            printf("DERP\n");
            return false;
        }
            

        m_VB.InitRawBuffer( pWin->GetDevice(), ply.pPositions, ply.nVertices*3*sizeof(float), D3D11_BIND_VERTEX_BUFFER );
        m_IB.InitRawBuffer( pWin->GetDevice(), ply.pVertexIndices, ply.nTriangles*3*sizeof(uint32), D3D11_BIND_INDEX_BUFFER );
    
        m_nTriangles = ply.nTriangles;
        
        printf("%u tris.  %.2f MB", ply.nTriangles, (sizeof(float)*3*ply.nVertices, sizeof(uint32)*3*ply.nTriangles)/(1024.0f*1024.0f) );
        */

        std::vector<Simpleton::TessVertex> vb;
        std::vector<uint32> ib;

        Simpleton::TessellateTeapot( 64, vb, ib );

            
        // slice out all the stuff we dont need
        std::vector<Simpleton::Vec3f> positions;
        positions.resize(vb.size());
        Simpleton::Vec3f* pData = positions.data();
        for( size_t i=0; i<vb.size(); i++ )
            pData[i] = vb[i].vPos;

        // ooops, mesh ain't exactly where I want it. Fix it
        Simpleton::FlipMeshWinding(ib.data(), ib.size()/3);
        Simpleton::Matrix4f m = Simpleton::MatrixRotate(1,0,0,-3.1415926f/2);
        m = m * Simpleton::MatrixScale(0.25f,0.25f,0.25f);
        for( size_t i=0; i<vb.size(); i++ )
            pData[i] = Simpleton::AffineTransformPoint( m, pData[i] );

        m_VB.InitRawBuffer( pWin->GetDevice(), positions.data(), positions.size()*3*sizeof(float), D3D11_BIND_VERTEX_BUFFER );
        m_IB.InitRawBuffer( pWin->GetDevice(), ib.data(), ib.size()*sizeof(uint32), D3D11_BIND_INDEX_BUFFER );
        m_nTriangles = ib.size()/3;

        printf("%u tris.  %.2f MB.  %ux%u(%u pixels)", m_nTriangles, (sizeof(float)*3*positions.size(), sizeof(uint32)*3*m_nTriangles)/(1024.0f*1024.0f), 
               pWin->GetViewportWidth(),
               pWin->GetViewportHeight(),
               pWin->GetViewportHeight()*pWin->GetViewportWidth());
     

        /*
        float vb[] = {
            -0.1,-0.05,0,
            0,0.1,0,
            0.1,0,0
        };
        uint32 ib[3] = {0,1,2};

        m_VB.InitRawBuffer( pWin->GetDevice(), vb, sizeof(vb), D3D11_BIND_VERTEX_BUFFER );
        m_IB.InitRawBuffer( pWin->GetDevice(), ib, sizeof(ib), D3D11_BIND_INDEX_BUFFER );
        m_nTriangles=1;
        
        */
        

        m_CSO.Init( cs_raster, sizeof(cs_raster), pDevice );
        m_CSO.GetResourceSchema()->CreateResourceSet( &m_CSOResourceSet, pDevice );

        m_CopyQuad.Init(pWin->GetDevice(),m_StateBuilder);

        m_FakeZBuffer.Init( pWin->GetViewportWidth(), pWin->GetViewportHeight(), pWin->GetDevice() );
        m_SMap.Init( pWin->GetViewportWidth(), pWin->GetViewportHeight(), 1, true, pWin->GetDevice() );
        return true;
    }


    void DoRasterizer( Simpleton::DX11Window* pWin )
    {

        float aspect = pWin->GetAspectRatio();
        Simpleton::Matrix4f mProj = Simpleton::MatrixOrthoLH(ZOOM,ZOOM/aspect,1,10);
        Simpleton::Matrix4f mXLate = Simpleton::MatrixTranslate(0,Y_OFFS,5);
        m_PSOResourceSet.BeginUpdate(pWin->GetDeviceContext());
        m_PSOResourceSet.BindConstant("mWorld",&mXLate, sizeof(mXLate));
        m_PSOResourceSet.BindConstant("mViewProj",&mProj, sizeof(mProj));
        m_PSOResourceSet.EndUpdate(pWin->GetDeviceContext());

        
        ID3D11DeviceContext* pCtx = pWin->GetDeviceContext();
                
        pCtx->ClearState();

        auto pBackBuffer = pWin->GetBackbufferRTV();
        auto pZBuffer = m_SMap.GetDSV();

     
        pCtx->ClearDepthStencilView( pZBuffer, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1, 0 );
        
        D3D11_VIEWPORT vp = pWin->BuildViewport();
        D3D11_RECT scissor = pWin->BuildScissorRect();

       
        pCtx->OMSetRenderTargets( 0,0, pZBuffer );
        pCtx->RSSetViewports( 1, &vp);
        pCtx->RSSetScissorRects(1,&scissor);
       
        
        UINT stride=12;
        UINT offs=0;
        ID3D11Buffer* pBuff = (ID3D11Buffer*) m_VB.GetResource();
        pCtx->IASetVertexBuffers( 0, 1, &pBuff, &stride, &offs );
        pCtx->IASetIndexBuffer((ID3D11Buffer*)m_IB.GetResource(),DXGI_FORMAT_R32_UINT, 0 );
        pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        

        m_PSO.Apply( pWin->GetDeviceContext() );
        m_PSOResourceSet.Apply(pCtx);
        pCtx->DrawIndexed( 3*m_nTriangles,0,0);

        m_CopyQuad.Do( pWin->GetBackbufferRTV(), m_SMap.GetSRV(), pWin->GetDeviceContext());
    }

    void DrawWireFrame(Simpleton::DX11Window* pWin )
    {    
        auto pCtx = pWin->GetDeviceContext();
        auto pBackBuffer = pWin->GetBackbufferRTV();
        auto pZBuffer    = pWin->GetBackbufferDSV();

         
        pCtx->ClearState();

        float aspect = pWin->GetAspectRatio();
        Simpleton::Matrix4f mProj = Simpleton::MatrixOrthoLH(ZOOM,ZOOM/aspect,1,10);
        Simpleton::Matrix4f mXLate = Simpleton::MatrixTranslate(0,Y_OFFS,5);
        m_PSOResourceSet.BeginUpdate(pWin->GetDeviceContext());
        m_PSOResourceSet.BindConstant("mWorld",&mXLate, sizeof(mXLate));
        m_PSOResourceSet.BindConstant("mViewProj",&mProj, sizeof(mProj));
        m_PSOResourceSet.EndUpdate(pWin->GetDeviceContext());
    
        pCtx->ClearDepthStencilView( pZBuffer, D3D11_CLEAR_DEPTH, 1, 0 );
        
        D3D11_VIEWPORT vp  = pWin->BuildViewport();
        D3D11_RECT scissor = pWin->BuildScissorRect();

        pCtx->OMSetRenderTargets( 0,0, pZBuffer );
        pCtx->RSSetViewports( 1, &vp);
        pCtx->RSSetScissorRects(1,&scissor);
       

        UINT stride=12;
        UINT offs=0;
        ID3D11Buffer* pBuff = (ID3D11Buffer*) m_VB.GetResource();
        pCtx->IASetVertexBuffers( 0, 1, &pBuff, &stride, &offs );
        pCtx->IASetIndexBuffer((ID3D11Buffer*)m_IB.GetResource(),DXGI_FORMAT_R32_UINT, 0 );
        pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        
        
        m_PSO.Apply( pWin->GetDeviceContext() );
        m_PSOResourceSet.Apply(pCtx);
        pCtx->DrawIndexed( 3*m_nTriangles,0,0);

        
        pCtx->OMSetRenderTargets( 1,&pBackBuffer, pZBuffer );
        m_WirePSO.Apply( pWin->GetDeviceContext() );
        pCtx->DrawIndexed( 3*m_nTriangles,0,0);

        
    }
   

    

    void DoCompute( Simpleton::DX11Window* pWin )
    {
        float aspect = pWin->GetAspectRatio();
        Simpleton::Matrix4f mProj = Simpleton::MatrixOrthoLH(ZOOM,ZOOM/aspect,1,10);
        Simpleton::Matrix4f mXLate = Simpleton::MatrixTranslate(0,Y_OFFS,5);
        float Resolution[2] = { pWin->GetViewportWidth(), pWin->GetViewportHeight() };

        pWin->GetDeviceContext()->ClearState();

        m_CSOResourceSet.BeginUpdate(pWin->GetDeviceContext());
        m_CSOResourceSet.BindConstant("mWorld",   &mXLate, sizeof(mXLate));
        m_CSOResourceSet.BindConstant("mViewProj",&mProj,  sizeof(mProj) );
        m_CSOResourceSet.BindConstant("g_vResolution", Resolution, sizeof(Resolution) );
        m_CSOResourceSet.BindConstant("g_nTriangles", &m_nTriangles, sizeof(m_nTriangles) );
        m_CSOResourceSet.BindSRV("g_Verts", m_VB.GetSRV());
        m_CSOResourceSet.BindSRV("g_Indices", m_IB.GetSRV());
        m_CSOResourceSet.EndUpdate(pWin->GetDeviceContext());


        auto pUAV = m_FakeZBuffer.GetUAV();
        ID3D11DeviceContext* pCtx = pWin->GetDeviceContext();
        m_CSO.Bind(pCtx);
        m_CSOResourceSet.Apply(pCtx);
        pCtx->OMSetRenderTargets(0,0,0);
        
        pCtx->CSSetUnorderedAccessViews( 0,1,&pUAV, 0 );
      
        // 3f800000 -> 1.0f DX11 makes me use hex here
        UINT ONE[] = {0x3f800000,0x3f800000,0x3f800000,0x3f800000};
        pCtx->ClearUnorderedAccessViewUint(pUAV,ONE);
       
        DWORD size = m_CSO.GetThreadGroupDims()[0];
        uint nGroups = m_nTriangles;
        if( nGroups%size )
            nGroups += size-(nGroups%size);
        nGroups /= size;

        
        pCtx->Dispatch(nGroups,1,1);

          
        D3D11_VIEWPORT vp = pWin->BuildViewport();
        D3D11_RECT scissor = pWin->BuildScissorRect();
        pCtx->RSSetViewports( 1, &vp);
        pCtx->RSSetScissorRects(1,&scissor);
        ID3D11UnorderedAccessView* pNULLS[] = {0};
        pCtx->CSSetUnorderedAccessViews(0,1,pNULLS,0);
        m_CopyQuad.Do( pWin->GetBackbufferRTV(), m_FakeZBuffer.GetSRV(), pWin->GetDeviceContext());
    }
   

    virtual void OnFrame( Simpleton::DX11Window* pWin )
    {
        if( !m_bCompute )
            DoRasterizer(pWin);
        else
            DoCompute(pWin);
     
        if( m_bWire )
            DrawWireFrame(pWin);
    }

    
    virtual void OnKeyUp( Simpleton::Window* pWindow, KeyCode eKey )
    {
        if( eKey == KEY_SPACE )
            m_bCompute = !m_bCompute;
        
        if( eKey == KEY_W )
            m_bWire = !m_bWire;
    }

};



int main()
{
    RasterTest app;
    
    Simpleton::DX11Window* pWin = Simpleton::DX11Window::Create( 1600,900, Simpleton::DX11Window::FPS_TITLE | Simpleton::DX11Window::USE_DEBUG_LAYER, &app );
    while( pWin->DoEvents() )
        pWin->DoFrame();

    return 0;
}


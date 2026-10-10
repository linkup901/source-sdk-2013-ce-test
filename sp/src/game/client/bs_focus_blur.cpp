#include "cbase.h"
#include "bs_focus_blur.h"
#include "view_scene.h"
#include "rendertexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier0/memdbgon.h"

extern float BS_FocusBlurRadius();

static void BS_SetConstant( IMaterial *material, int reg, int component, float value )
{
    char name[16];
    Q_snprintf( name, sizeof(name), "$c%d_%c", reg, "xyzw"[component] );
    bool found = false;
    IMaterialVar *var = material->FindVar( name, &found, false );
    if ( found ) var->SetFloatValue( value );
}

void BS_DrawFocusBlur( int x, int y, int width, int height )
{
    const float radius = BS_FocusBlurRadius();
    if ( radius <= 0.001f || width <= 0 || height <= 0 ||
         !engine->IsInGame() || engine->IsHammerRunning() ||
         !g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_b() )
        return;

    // Resolve through the material system each frame: safe across video resets
    // and mat_reloadallmaterials. No private render targets or stale RT sizes.
    IMaterial *material = materials->FindMaterial( "effects/bs_focus_blur", TEXTURE_GROUP_CLIENT_EFFECTS );
    if ( !material || material->IsErrorMaterial() ) return;
    CMaterialReference materialReference;
    materialReference.Init( material );
    ITexture *frame = GetFullFrameFrameBufferTexture( 0 );
    if ( !frame || frame->IsError() ) return;

    CMatRenderContextPtr context( materials );
    int targetWidth, targetHeight;
    context->GetRenderTargetDimensions( targetWidth, targetHeight );
    if ( targetWidth <= 0 || targetHeight <= 0 ) return;

    // Match UpdateScreenEffectTexture's destination rectangle, including when
    // the engine's full-frame texture is smaller than the current target.
    const float scaleX = targetWidth > frame->GetActualWidth() || targetHeight > frame->GetActualHeight()
        ? float(frame->GetActualWidth()) / targetWidth : 1.0f;
    const float scaleY = targetWidth > frame->GetActualWidth() || targetHeight > frame->GetActualHeight()
        ? float(frame->GetActualHeight()) / targetHeight : 1.0f;
    const int copyX = int(x * scaleX), copyY = int(y * scaleY);
    const int copyW = int(width * scaleX), copyH = int(height * scaleY);
    if ( copyW < 2 || copyH < 2 ) return;
    const float invW = 1.0f / frame->GetActualWidth();
    const float invH = 1.0f / frame->GetActualHeight();

    const float constants[4][4] = {
        { invW, invH, 1, 0 },
        { 0, 0, 0, 0 },
        { radius, 1, 1, 0 },
        { (copyX + 0.5f) * invW, (copyY + 0.5f) * invH,
          (copyW - 1) * invW, (copyH - 1) * invH }
    };
    for ( int r = 0; r < 4; ++r )
        for ( int c = 0; c < 4; ++c )
            BS_SetConstant( material, r, c, constants[r][c] );

    // Each call snapshots the framebuffer before drawing, avoiding read/write
    // feedback. Horizontal pass then vertical pass; color grading only once.
    BS_SetConstant( material, 2, 0, constants[2][0] * scaleX );
    DrawScreenEffectMaterial( material, x, y, width, height );
    BS_SetConstant( material, 0, 2, 0 );
    BS_SetConstant( material, 0, 3, 1 );
    BS_SetConstant( material, 2, 0, constants[2][0] * scaleY );
    BS_SetConstant( material, 2, 2, 1.0f );
    BS_SetConstant( material, 2, 3, 0.0f );
    DrawScreenEffectMaterial( material, x, y, width, height );
}

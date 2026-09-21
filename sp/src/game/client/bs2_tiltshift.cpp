#include "cbase.h"
#include "bs2_tiltshift.h"
#include "view_scene.h"
#include "rendertexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier0/memdbgon.h"

// Not archived: a map or preset opts in explicitly. Server-executable allows
// Hammer's existing point_clientcommand to control this singleplayer effect.
static ConVar bs2_tiltshift_enable( "bs2_tiltshift_enable", "0", FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE, "Enable miniature selective focus", true, 0, true, 1 );
static ConVar bs2_tiltshift_center( "bs2_tiltshift_center", "0.55", FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE, "Focus center: 0 top, 1 bottom", true, 0, true, 1 );
static ConVar bs2_tiltshift_width( "bs2_tiltshift_width", "0.24", FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE, "Full sharp band width as fraction of screen height", true, 0, true, 1 );
static ConVar bs2_tiltshift_falloff( "bs2_tiltshift_falloff", "0.25", FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE, "Transition outside each edge of sharp band", true, 0.01f, true, 1 );
static ConVar bs2_tiltshift_angle( "bs2_tiltshift_angle", "0", FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE, "Band tilt in degrees", true, -60, true, 60 );
static ConVar bs2_tiltshift_radius( "bs2_tiltshift_radius", "24", FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE, "Maximum blur support radius in screen pixels", true, 0, true, 32 );
static ConVar bs2_tiltshift_strength( "bs2_tiltshift_strength", "1", FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE, "Blur strength", true, 0, true, 1 );
static ConVar bs2_tiltshift_saturation( "bs2_tiltshift_saturation", "1.05", FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE, "Subtle color saturation; 1 preserves original", true, 0, true, 1.5f );
static ConVar bs2_tiltshift_debug( "bs2_tiltshift_debug", "0", FCVAR_CLIENTDLL, "Show focus mask: black sharp, white maximum blur", true, 0, true, 1 );

static void BS2_SetConstant( IMaterial *material, int reg, int component, float value )
{
    char name[16];
    Q_snprintf( name, sizeof(name), "$c%d_%c", reg, "xyzw"[component] );
    bool found = false;
    IMaterialVar *var = material->FindVar( name, &found, false );
    if ( found ) var->SetFloatValue( value );
}

void BS2_DrawTiltShift( int x, int y, int width, int height )
{
    if ( !bs2_tiltshift_enable.GetBool() || width <= 0 || height <= 0 ||
         !engine->IsInGame() || engine->IsHammerRunning() ||
         !g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_b() )
        return;

    // Resolve through the material system each frame: safe across video resets
    // and mat_reloadallmaterials. No private render targets or stale RT sizes.
    IMaterial *material = materials->FindMaterial( "effects/bs2_tiltshift", TEXTURE_GROUP_CLIENT_EFFECTS );
    if ( !material || material->IsErrorMaterial() ) return;
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
        { bs2_tiltshift_center.GetFloat(), bs2_tiltshift_width.GetFloat() * 0.5f,
          bs2_tiltshift_falloff.GetFloat(), tanf(DEG2RAD(bs2_tiltshift_angle.GetFloat())) * float(width) / height },
        { bs2_tiltshift_radius.GetFloat(),
          bs2_tiltshift_strength.GetFloat(), 1, 0 },
        { (copyX + 0.5f) * invW, (copyY + 0.5f) * invH,
          (copyW - 1) * invW, (copyH - 1) * invH }
    };
    for ( int r = 0; r < 4; ++r )
        for ( int c = 0; c < 4; ++c )
            BS2_SetConstant( material, r, c, constants[r][c] );

    // Each call snapshots the framebuffer before drawing, avoiding read/write
    // feedback. Horizontal pass then vertical pass; color grading only once.
    BS2_SetConstant( material, 2, 0, constants[2][0] * scaleX );
    DrawScreenEffectMaterial( material, x, y, width, height );
    BS2_SetConstant( material, 0, 2, 0 );
    BS2_SetConstant( material, 0, 3, 1 );
    BS2_SetConstant( material, 2, 0, constants[2][0] * scaleY );
    BS2_SetConstant( material, 2, 2, bs2_tiltshift_saturation.GetFloat() );
    BS2_SetConstant( material, 2, 3, bs2_tiltshift_debug.GetFloat() );
    DrawScreenEffectMaterial( material, x, y, width, height );
}

#include "cbase.h"
#include "bs2_tiltshift.h"
#include "c_prop_vehicle.h"
#include "view_scene.h"
#include "rendertexture.h"
#include "ivrenderview.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier0/memdbgon.h"

static ConVar bs2_buggy_speedfx( "bs2_buggy_speedfx", "1", FCVAR_ARCHIVE, "Black buggy boost lines and high-speed lens warp", true, 0, true, 1 );
static ConVar bs2_buggy_speedlines( "bs2_buggy_speedlines", "0.30", FCVAR_ARCHIVE, "Boost line opacity", true, 0, true, 1 );
static ConVar bs2_buggy_speedwarp( "bs2_buggy_speedwarp", "0.012", FCVAR_ARCHIVE, "High-speed peripheral lens distortion", true, 0, true, 0.04f );
static ConVar bs2_buggy_speedfx_debug( "bs2_buggy_speedfx_debug", "0", FCVAR_CHEAT, "Log speed, actual boost state and effect envelopes once a second" );

// Called only for the main view, before HUD. Geometry is in clip space.
static void SpeedVertex( CMeshBuilder &b, float u, float v, float tu, float tv, float alpha )
{
    b.Position3f( 2*u-1, 1-2*v, 0 );
    b.TexCoord2f( 0, tu, tv );
    b.Color4f( 0.82f, 0.88f, 0.94f, alpha );
    b.AdvanceVertex();
}

void BS2_DrawBuggySpeedFX( int x, int y, int width, int height )
{
    static CHandle<C_BaseEntity> lastVehicle;
    static float lines = 0, warp = 0, lastTime = -1;
    C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
    IClientVehicle *iv = player ? player->GetVehicle() : NULL;
    C_BaseEntity *ent = iv ? iv->GetVehicleEnt() : NULL;
    C_PropVehicleDriveable *car = ent ? dynamic_cast<C_PropVehicleDriveable *>(ent) : NULL;
    const char *model = ent && ent->GetModel() ? modelinfo->GetModelName(ent->GetModel()) : "";
    // Explicit model opt-in leaves stock jeeps, airboats and scripted cameras alone.
    if ( !bs2_buggy_speedfx.GetBool() || !engine->IsInGame() || !player || !player->IsAlive() ||
         player->IsObserver() || render->GetViewEntity() != player->entindex() || !car ||
         !Q_stristr(model, "models/bs2_black_buggy/blackbuggy") ||
         car->GetPassenger(VEHICLE_ROLE_DRIVER) != player || car->IsRunningEnterExitAnim() ||
         width < 2 || height < 2 )
    {
        lastVehicle = NULL; lines = warp = 0; lastTime = -1;
        return;
    }
    if ( lastVehicle.Get() != ent || gpGlobals->curtime < lastTime )
        lines = warp = 0;
    lastVehicle = ent;
    // Rendering can run more than once per game frame. Never advance twice.
    float dt = lastTime < 0 ? 0 : clamp(gpGlobals->curtime-lastTime, 0.0f, 0.1f);
    lastTime = gpGlobals->curtime;
    const float speed = car->GetBS2SpeedMPH();
    const float lineTarget = car->IsBS2BoostActive() ? SimpleSpline(clamp(speed/24.0f, 0.0f, 1.0f)) : 0;
    const float warpTarget = SimpleSpline(clamp((speed-30.0f)/28.0f, 0.0f, 1.0f));
    lines = Approach(lineTarget, lines, dt / (lineTarget > lines ? 0.25f : 0.45f));
    warp = Approach(warpTarget, warp, dt / (warpTarget > warp ? 0.8f : 0.5f));
    static float nextLog = 0;
    if ( bs2_buggy_speedfx_debug.GetBool() && (gpGlobals->curtime >= nextLog || gpGlobals->curtime < nextLog-1) )
    {
        Msg("BS2_SPEEDFX mph=%.0f boost=%d lines=%.3f warp=%.3f\n", speed, car->IsBS2BoostActive() ? 1 : 0, lines, warp);
        nextLog = gpGlobals->curtime+1;
    }
    if ( lines < 0.001f && warp < 0.001f ) return;

    IMaterial *warpMat = materials->FindMaterial("effects/bs2_buggy_speedwarp", TEXTURE_GROUP_CLIENT_EFFECTS);
    IMaterial *lineMat = materials->FindMaterial("effects/bs2_buggy_speedlines", TEXTURE_GROUP_CLIENT_EFFECTS);
    CMatRenderContextPtr ctx(materials);
    const MaterialMatrixMode_t matrices[3] = { MATERIAL_MODEL, MATERIAL_VIEW, MATERIAL_PROJECTION };
    for ( int m = 0; m < 3; ++m )
    {
        ctx->MatrixMode(matrices[m]);
        ctx->PushMatrix();
        ctx->LoadIdentity();
    }

    if ( warp * bs2_buggy_speedwarp.GetFloat() > 0.00001f && !warpMat->IsErrorMaterial() )
    {
        ITexture *frame = GetFullFrameFrameBufferTexture(0);
        if ( frame && !frame->IsError() )
        {
            Rect_t rect;
            UpdateScreenEffectTexture(0, x, y, width, height, false, &rect);
            warpMat->FindVar("$basetexture", NULL)->SetTextureValue(frame);
            IMesh *mesh = ctx->GetDynamicMesh(true, NULL, NULL, warpMat);
            CMeshBuilder b;
            const int nx = 32, ny = 18;
            b.Begin(mesh, MATERIAL_QUADS, nx*ny);
            for ( int j = 0; j < ny; ++j ) for ( int i = 0; i < nx; ++i )
            {
                const int ox[4] = {0,0,1,1}, oy[4] = {0,1,1,0};
                for ( int k = 0; k < 4; ++k )
                {
                    float u = float(i+ox[k])/nx, v = float(j+oy[k])/ny;
                    float dx = u-0.5f, dy = v-0.5f;
                    // Smooth radial stretch: still center, no shaking or blur.
                    float r2 = 2*(dx*dx+dy*dy);
                    float amount = warp*bs2_buggy_speedwarp.GetFloat()*r2*r2;
                    float su = clamp(u-dx*amount, 0.0f, 1.0f);
                    float sv = clamp(v-dy*amount, 0.0f, 1.0f);
                    b.Position3f(2*u-1, 1-2*v, 0);
                    b.TexCoord2f(0, (rect.x+0.5f+su*(rect.width-1))/frame->GetActualWidth(),
                                    (rect.y+0.5f+sv*(rect.height-1))/frame->GetActualHeight());
                    b.Color4f(1,1,1,1); b.AdvanceVertex();
                }
            }
            b.End(); mesh->Draw();
        }
    }
    if ( lines > 0.001f && !lineMat->IsErrorMaterial() )
    {
        IMesh *mesh = ctx->GetDynamicMesh(true, NULL, NULL, lineMat);
        CMeshBuilder b;
        const int count = 30;
        b.Begin(mesh, MATERIAL_QUADS, count*2);
        for ( int i = 0; i < count; ++i )
        {
            float angle = i*2.39996323f;
            float ax = cosf(angle), ay = sinf(angle);
            float phase = fmodf(gpGlobals->curtime*(0.62f+(i%5)*0.07f)+i*0.618034f, 1.0f);
            float radius = 0.64f+phase*0.60f;
            float length = 0.12f+(i%4)*0.025f;
            // One pixel at 1080p. Taper both ends using a transparent midpoint strip.
            float px = -ay*0.55f/width, py = ax*0.55f/height;
            float alpha = lines*bs2_buggy_speedlines.GetFloat()*sinf(phase*M_PI);
            for ( int segment = 0; segment < 2; ++segment )
            {
                float r0 = radius+length*segment*0.5f, r1 = r0+length*0.5f;
                float u0 = 0.5f+ax*r0*0.62f, v0 = 0.5f+ay*r0*0.62f;
                float u1 = 0.5f+ax*r1*0.62f, v1 = 0.5f+ay*r1*0.62f;
                float a0 = segment == 0 ? 0 : alpha, a1 = segment == 0 ? alpha : 0;
                SpeedVertex(b,u0-px,v0-py,0,0,a0);
                SpeedVertex(b,u1-px,v1-py,0,0,a1);
                SpeedVertex(b,u1+px,v1+py,0,0,a1);
                SpeedVertex(b,u0+px,v0+py,0,0,a0);
            }
        }
        b.End(); mesh->Draw();
    }
    for ( int m = 2; m >= 0; --m )
    {
        ctx->MatrixMode(matrices[m]); ctx->PopMatrix();
    }
    ctx->MatrixMode(MATERIAL_MODEL);
}

#pragma once
class CViewSetup;
// Main view only. Depth pass (SSAO_DepthPass) must already have filled _rt_ResolvedFullFrameDepth.
bool BS2_SSAOEnabled();
// Darkens creases of the opaque scene; call after opaque renderables, before translucents.
void BS2_DrawSSAO( const CViewSetup &view );

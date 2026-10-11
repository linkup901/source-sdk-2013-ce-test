# Transition Sync — Phase 1 pilot

**First test:** conservative point-prop region import from `forest-trail.vmf` to a separate `ridge` preview VMF.

Local prototype: `isa_bs2_map_analyzer_transition_sync_v0_4.zip` supplied in the development conversation (not yet hosted in this repository).

## Workflow
1. Back up both source VMFs; never edit production maps.
2. In the analyzer's Transition Sync tab, set source/destination paths and 3D selection bounds in source VMF coordinates.
3. Supply translation `(dx,dy,dz)` and generate `_TRANSITION_TEST.vmf` in workspace.
4. Open the candidate in Hammer++ and review placement/duplicates.
5. Compile and compare only after approving geometry.

## Actual implementation scope
- Imports uncomplicated `prop_static`, `prop_dynamic`, and `prop_physics` variants by origin-in-AABB selection.
- Remaps Hammer entity IDs, records stable source fingerprints, writes a manifest, blocks overwrite.
- Does **not** yet copy brushes, displacements, or skyboxes. Does not blend seams. No engine test yet.
- 13 inherited + new synthetic test cases pass locally.

## Next engineering tests
- Copy complete solids and displacements with full ID/reference rewrites and brush plane transforms.
- Reconstruct displacement surfaces, compare seam heights/normals, prevent invalid seam changes.
- Inspect actual `forest-trail` / `ridge` pair and visually approve before/after.

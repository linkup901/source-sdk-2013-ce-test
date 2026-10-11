# Isa's BS2 Map Analyzer (development branch)

Unified tool project for Black Stasis 2 map QA. This branch tracks the app integration and validation work, **not** changes to the Source SDK runtime.

## Local prototype

The current unified application is distributed as a ZIP in the development conversation. It consolidates a read-only VMF grounding scanner, Hammer++ issue-visgroup review generator, model manifest/contact tagger, before/after review app, and HLMV++ executable launcher. It is not yet fully uploaded into this branch.

## Safety gates

- No automatic edits of production VMFs.
- View bounds are not exact prop collision geometry.
- Displacement support is unresolved without height-mesh reconstruction.
- Preserve entity IDs and existing visgroups.
- Keep model and map assets local; do not commit Valve or proprietary game content.
- Require matched in-engine before/after screenshots and user sign-off.
- On second rejection of the same case, flag Needs Special Attention.

## Workstreams

1. Unified app and feedback ledger
2. VPK / MDL and displacement-aware prop contact analysis
3. Brush intersections, seams and clearance
4. Audited candidate VMF fix generation
5. Hammer++/HLMV++/Source live validation

The packaged local prototype passes 11 synthetic unit tests; in-engine results are pending.

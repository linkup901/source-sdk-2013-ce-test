# Crabby companion — first playable version

This requires the custom Source SDK 2013 CE preview build. Retail Half-Life 2 and a stock SDK server do not contain `npc_crabby`.

## Test

Launch `BS2-TiltShift-Preview` with the Source SDK Base 2013 Singleplayer executable, then enter:

```
map bs2_crabby_test
exec crabby_test
```

Crabby starts following. Walk around the central obstacle. The map has ground AI nodes for pathfinding. In the console:

```
crabby_wait
crabby_follow
crabby_recall
```

For combat, aim at clear floor some distance away and enter `npc_create npc_zombie`. Shoot at the zombie to ask Crabby to assist. Automatic assistance uses a trace along your aim while the attack button is held; it does not yet track projectile hits or grenade damage. Crabby does not bite the player and ignores player-inflicted damage. Other enemies can still hurt it.

The test aliases do not replace existing key bindings. Save/load preserves following and the commanded enemy. Crabby uses the fast headcrab's existing ground movement and leap attack. Switch puzzles are described in PASSAGE-PUZZLES.md.

## Shoulder riding

Look at Crabby within arm's reach (about 110 units) and press Use (E): it climbs onto your left shoulder. In first person it sits in the lower-left corner of the screen, view-locked so it does not lag when you turn, and now and then chirps or looks around. To set it down, aim at the floor and press E; it lands there (or beside you if the aimed spot is not reachable). Doors, buttons and pickups still take E normally while it rides.

Crabby hops off by itself when:

- you open fire on something hostile (it leaps at that target),
- you aim at a Crabby switch and press E (it goes and presses it),
- you get into a vehicle, swim, noclip or die,
- map logic sends `Wait`, `Dismount`, `DisableRiding`, `AttackTarget` or `CommandSwitch`.

It cannot be hurt while riding. Riding is not carried across level changes; a ride restored without its player ends on the spot.

The first-person seat can be tuned live with `cl_crabby_ride_forward`, `cl_crabby_ride_right`, `cl_crabby_ride_up`, `cl_crabby_ride_yaw`, `cl_crabby_ride_pitch` and `cl_crabby_ride_bob` (saved to config). Because it is drawn in the world pass, it can dip into a wall you are pressed against.

## Size and animations

Crabby spawns at 0.7 of the fast headcrab's size (`CrabbyScale` keyvalue, `SetCrabbyScale` input). Collision and navigation keep the tiny hull at any size. When it uses a switch it turns to face it and plays the rear-up (`rearup`) sequence; the press fires as the front legs come down. Landing from the shoulder plays `ceiling_land`; idling on the shoulder plays `lookaround`.

## Hammer++

Add `crabby.fgd` alongside your existing CE/EP2 FGD. Place `npc_crabby` and give it a targetname such as `crabby`. Place ground `info_node` entities along routes, especially around obstacles. The appearance assets must be mounted by Hammer and by the game.

Available inputs:

| Input | Parameter | Behavior |
|---|---|---|
| FollowPlayer | none | Recall and follow the player; enable combat assistance |
| Wait | none | Stop fighting and wait |
| AttackTarget | targetname of an NPC | Explicitly attack that NPC; player-allied classes are protected |
| StopAttacking | none | Clear the attack command without changing follow/wait mode |
| CommandSwitch | targetname of an info_crabby_switch | Go and press that switch |
| RideShoulder | none | Jump onto the player's shoulder from anywhere (scripted) |
| Dismount | none | Hop off onto nearby floor |
| EnableRiding / DisableRiding | none | Allow or forbid picking Crabby up |
| SetCrabbyScale | float | Change the visual size |

Outputs: `OnRideStart`, `OnRideEnd`. Keyvalues: `CrabbyScale` (default 0.7), `AllowRiding` (default yes).

The test BSP includes the model and textures. The companion DLL must still be installed separately; packing it into a BSP cannot add a new NPC class to the engine.

## Build and rollback

Source branch: `codex/bs2-combined` in `linkup901/source-sdk-2013-ce-test` (Black Hunter + Crabby in one DLL; built by the `SP_Windows_TiltShift` workflow, artifact `bs2_tiltshift_episodic_windows`). The original companion work is `codex/crabby-companion`, PR #4.
The installed client/server pair is in the preview mod's `bin` folder. Previous files are preserved in `crabby-work/before-companion-bin`. Close the game before replacing DLLs.

The original reskin package remains under `crabby-work/package`. Ordinary headcrab assets and the intro map are unchanged.

The preview's missing base `cfg/skill.cfg` was restored from the SDK's `sp/game/mod_hl2/cfg/skill.cfg`. Keep this alongside the episodic skill configuration so ordinary NPCs have their intended health and damage.

Verified in the CE runtime: entity/model loading, waiting, following around the test obstacle, explicit attack damage (zombie health 50 to 20), and automatic gunfire assistance after accounting for Source's feared-enemy relationships. The corrected automatic-assist test ended with Crabby killing the target after the player stopped firing. Full story integration and long playthrough testing remain future work.



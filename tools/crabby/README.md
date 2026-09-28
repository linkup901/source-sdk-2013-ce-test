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

The test aliases do not replace existing key bindings. Save/load preserves following and the commanded enemy. Crabby uses the fast headcrab's existing ground movement and leap attack. It cannot yet ride on the player, press switches, or perform the story's passage sequences.

## Hammer++

Add `crabby.fgd` alongside your existing CE/EP2 FGD. Place `npc_crabby` and give it a targetname such as `crabby`. Place ground `info_node` entities along routes, especially around obstacles. The appearance assets must be mounted by Hammer and by the game.

Available inputs:

| Input | Parameter | Behavior |
|---|---|---|
| FollowPlayer | none | Recall and follow the player; enable combat assistance |
| Wait | none | Stop fighting and wait |
| AttackTarget | targetname of an NPC | Explicitly attack that NPC; player-allied classes are protected |
| StopAttacking | none | Clear the attack command without changing follow/wait mode |

The test BSP includes the model and textures. The companion DLL must still be installed separately; packing it into a BSP cannot add a new NPC class to the engine.

## Build and rollback

Source branch: `codex/crabby-companion`, PR #4 in `linkup901/source-sdk-2013-ce-test`.
Windows artifact: `crabby-ce-windows`, build commit `04dca75febc3f218be3fce8ac5b1638f0d9c6a2d`.
The installed client/server pair is in the preview mod's `bin` folder. Previous files are preserved in `crabby-work/before-companion-bin`. Close the game before replacing DLLs.

The original reskin package remains under `crabby-work/package`. Ordinary headcrab assets and the intro map are unchanged.

The preview's missing base `cfg/skill.cfg` was restored from the SDK's `sp/game/mod_hl2/cfg/skill.cfg`. Keep this alongside the episodic skill configuration so ordinary NPCs have their intended health and damage.

Verified in the CE runtime: entity/model loading, waiting, following around the test obstacle, explicit attack damage (zombie health 50 to 20), and automatic gunfire assistance after accounting for Source's feared-enemy relationships. The corrected automatic-assist test ended with Crabby killing the target after the player stopped firing. Full story integration and long playthrough testing remain future work.



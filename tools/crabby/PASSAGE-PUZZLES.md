# Crabby passage switches

Aim at an opted-in switch and press the Use key (E by default). This is an explicit command; merely looking does not send Crabby away. Crabby stops assisting in combat, navigates to the switch, pauses at it, triggers the puzzle output once, and returns to following. A failed route returns it to following without activating anything.

## Hammer setup

Add the updated `crabby.fgd` alongside your existing CE/EP2 FGD.

1. Place a solid `func_button` or solid model for the visible switch. Give it a unique name, such as `vent_button`.
2. Place `info_crabby_switch` on the floor where Crabby should stand to press it. Name it `vent_switch_approach` and set **Visible switch entity** to `vent_button`.
3. Keep the marker within 96 units of the switch's center, with no solid obstruction between Crabby and that center. Use a low button; this version does not climb walls or reach high controls.
4. Connect the marker's **OnPressed** output to `vent_button` → **Press**, or to a `logic_relay` → **Trigger**. This output fires only after physical arrival and a brief pause. The activator is Crabby.
5. Place ground `info_node` entities through the passage, at its entrance, corners, exit and switch approach. Crabby uses its normal tiny hull: 24 units wide and 24 high. A 64-wide, 32-high test passage leaves clearance while excluding the crouched player. Do not use a player-blocking clip material that also blocks NPCs in the tunnel.
6. Wire **OnSearchStarted**, **OnSearchFailed** and **OnSearchCancelled** to optional sounds or subtitles. A search times out after 30 seconds. **Disable** also fails an active search. **Reset** allows a used switch to be commanded again; successful switches otherwise remain used.

The player must aim directly at the visible switch within 1024 units, with a living Crabby within 1024 units of the player. Walls obscure the command. The nearest Crabby is selected if multiple companions exist. Normal Use continues working on entities without an associated marker.

`FollowPlayer` recalls Crabby; `Wait` cancels and keeps it waiting. Map logic can bypass the aiming step by sending `CommandSwitch` to Crabby with `vent_switch_approach` as its parameter. It still has to navigate and physically reach the approach.

## Test map

Open the custom CE preview and run:

```
map bs2_crabby_passage
```

Look through the upper slot at the button and tap E. Crabby should enter the lower tunnel, press the button, turn on the green light, and come back. This test is separate from the forest intro.

The current behavior uses the existing fast-headcrab movement and a short pause for activation. A bespoke paw-press animation, riding, and story-map placement are not part of this version.


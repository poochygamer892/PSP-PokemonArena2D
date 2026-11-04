# Previous reference package: Empress: Pokémon 2D Arena

> Scope: this is analysis of the separate Empress Windows/Android build. The
> current PSP target is the Poke2D Arena browser edition at
> https://poke2darena.com/. Do not treat these IL2CPP findings as evidence for
> that website's TypeScript/Phaser game.

## Evidence received so far

The other Codex task downloaded and hashed the two shared v4.3.0 packages:

| Package | Size | SHA-256 |
| --- | ---: | --- |
| Windows ZIP | 447,488,842 bytes | `bdb1152abbe2397e6b7b50ad546ab961424e852359bc23f709f0e96c97053ec0` |
| Android APK | 474,830,781 bytes | `34da7ec02840ea655c90665931db16ca422ac32f4cd1539c7c6eeb11b1dd2943` |

The Windows package contains a Unity IL2CPP build, including `GameAssembly.dll`
and `global-metadata.dat`. REA found an `Assembly-CSharp.dll` metadata image
with 1,717 types. Relevant namespaces include `Empress.Combat`, `Empress.Arena`,
`Empress.Creatures`, `Empress.Network`, and `Empress.Replay`.

Static metadata names include `BattleEngine.Input`, `ApplyInput`, `Dodge`,
`SwitchPokemon`, `Step`, `Cast`, `Release`, and `UpdateAction`. These names and
the associated type structure support a real-time arena-combat model with
movement, attacks, dodging, fighter switching, projectiles, and collision
handling. They do not establish the exact implementation or gameplay values.

## Still unconfirmed

- Exact control mapping, attack inputs, movement speed, and dodge timing
- Damage, recovery, hitstun, knockback, cooldown, and invulnerability rules
- Fighter roster, move sets, arena boundaries, and match flow
- How much of the game runs locally versus through its network systems
- Which reference assets can be reused and their original dimensions/formats
- Runtime behavior; the reference executable has not been observed in a match

## PSP implementation direction

The reference build is not a PSP executable and its Unity runtime cannot be
ported directly. The PSP version will be a native homebrew reimplementation.
Keep the first slice deliberately small: one arena, two fighters, movement,
one attack per fighter, health, collision/hit response, and a restart flow.
Build controls and combat rules from observed evidence rather than guessing.

## Next research pass

1. Observe one short match and record controls, state transitions, timings, and
   visible hit reactions.
2. Inspect the Windows build's relevant metadata and native methods, then
   compare any confirmed behavior with the Android build.
3. Inventory the sprite, animation, arena, and audio resources for candidate
   assets.
4. Turn the observations into data tables for fighters, moves, and arena
   bounds before implementing the combat slice on PSP.

The package files and full REA evidence currently live in the isolated
workspace of the other task; this document records the verified findings
available here. Copy the packages or evidence bundle into `scratch/` before
running deeper asset or native-code analysis in this project.

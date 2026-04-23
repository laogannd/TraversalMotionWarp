# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

TraversalMotionWarp is an Unreal Engine 5.4+ plugin that provides a standalone fork of the engine's Motion Warping system. It dynamically adjusts animation-driven root motion to align with world-space targets — used for parkour, climbing, vaulting, and similar traversal mechanics.

Single runtime module. No editor-only modules. Beta status with some features marked experimental (PrecomputedWarp, AdjustmentBlendWarp, SwitchOffConditions).

## Build

This is an UE plugin — no standalone build commands. It compiles as part of an Unreal Engine project:
- Requires UE 5.4+
- Platforms: Win64, Mac, Linux, Android, iOS
- Dependencies are all built-in engine modules (Core, CoreUObject, Engine, NetCore; editor adds UnrealEd, AnimGraph)
- Build config: `TraversalMotionWarp.Build.cs` at repo root

To compile, place this plugin in a UE project's `Plugins/` directory and build the project normally.

## Architecture

### Data Flow

```
AnimNotifyState_TraversalMotionWarp (defines warping window in animation)
  → UTraversalRootMotionModifier (created from notify, runs warping algorithm)
  → UTraversalMotionWarpComponent (manages modifiers + warp targets on actor)
  → UTraversalMotionWarpBaseAdapter (adapts warping to actor type)
  → Character/Actor (receives warped root motion)
```

### Core Classes

- **UTraversalMotionWarpComponent** — Main ActorComponent. Manages active root motion modifiers and named warp targets. Replicates targets over network. Blueprint-callable API.
- **UTraversalMotionWarpBaseAdapter / CharacterAdapter** — Adapter pattern decoupling warping from specific actor types. CharacterAdapter hooks into CharacterMovementComponent's root motion preprocessing.
- **UTraversalRootMotionModifier** — Base class for warping algorithms. Tracks state (Waiting → Active → MarkedForRemoval/Disabled). Subclasses implement `ProcessRootMotion()`.
- **FTraversalMotionWarpTarget** — Named alignment point (static transform or component-following). Supports bone/socket references and configurable offset directions.

### Warping Algorithms (Modifier Subclasses)

- **SkewWarp** — Primary algorithm. Stretches/compresses animation motion with max speed clamping.
- **PrecomputedWarp** (experimental) — Precomputes full path on first frame. Supports steering and separate translation curves. Stationary targets only.
- **AdjustmentBlendWarp** (experimental) — Precomputed with IK bone support for foot placement.
- **Scale** — Simple vector multiplier on translation.

### Animation Integration

- **UAnimNotifyState_TraversalMotionWarp** — AnimNotifyState placed in montages/sequences to define warping windows. Contains an embedded modifier instance.
- **UTraversalMotionWarpUtilities** — Static helpers for extracting bone poses, root motion, and warping windows from animations.

### Other Systems

- **SwitchOffConditions** (experimental) — Conditional warping control based on distance, angle, composite logic, or Blueprint-defined conditions. Effects: CancelFollow, CancelWarping, PauseWarping, PauseRootMotion.
- **AttributeBasedRootMotionComponent** — Alternative root motion application (ApplyDelta or ApplyVelocity modes).
- **TraversalMotionWarpFunctionLibrary** — Blueprint factory for creating warp targets.

## Debug Console Variables

Available in non-shipping builds:
- `a.TraversalMotionWarp.Disable` — disable all warping
- `a.TraversalMotionWarp.Debug` — 0=Off, 1=Log, 2=DrawDebug, 3=Both
- `a.TraversalMotionWarp.DrawDebugLifeTime` — debug visualization duration
- `a.TraversalMotionWarp.Debug.Target` — warp target debug (0=off, 1=selected, 2=all)
- `a.TraversalMotionWarp.Debug.SwitchOffCondition` — switch-off condition debug

## Conventions

- All classes use `Traversal` prefix to avoid collision with engine's built-in MotionWarping types
- Modifier state machine: Waiting → Active → MarkedForRemoval/Disabled (never skip states)
- Warp targets are identified by FName and managed through the component's `AddOrUpdateWarpTarget` / `RemoveWarpTarget` API
- Network replication uses Push Model for warp targets
- Copyright: DGOne, 2026

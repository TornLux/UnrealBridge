# Animation pose capture — multi-view grid

Pending feature. Give an agent the ability to "watch" an `AnimSequence` /
`AnimMontage` and identify semantic beats (windup start, impact frame,
recovery end) so tooling can auto-place Notifies, SoundCues, or VFX
triggers without the user clicking through the timeline frame-by-frame.

## Why single-camera capture isn't enough

A single viewport occludes key joints. A front view of a punch hides
forearm extension; a side view of a high kick looks identical to a
walk-through. Pose disambiguation needs multiple views simultaneously,
*in a single image*, so the agent can reason about all angles in one
prompt instead of stitching four separate uploads.

## Proposed API (extend `UnrealBridgeLevelLibrary` vision section)

```cpp
bool CaptureAnimPoseGrid(
    FString AnimPath,             // soft path to UAnimSequenceBase
    float  Time,                  // seconds into the anim
    FString SkeletalMeshPath,     // mesh to pose (usually the Skeleton's preview mesh)
    TArray<FString> Views,        // subset of {"Front","Side","ThreeQuarter","Top","Back"}
    bool   bBoneOverlay,          // draw key bone lines on the RT (wrist/hip/foot/head)
    int32  GridCols,              // layout, e.g. 2 → 2×2 for 4 views
    FString OutPngPath);

bool CaptureAnimMontageTimeline(
    FString AnimPath, FString SkeletalMeshPath,
    int32  NumTimeSamples,        // e.g. 5 → rows for 0%, 25%, 50%, 75%, 100%
    TArray<FString> Views, bool bBoneOverlay,
    FString OutPngPath);          // one PNG, NumTimeSamples rows × len(Views) cols
```

## Implementation sketch

1. Spawn a transient `ASkeletalMeshActor` in the runtime world, bind
   `SkeletalMesh` + `AnimSequence` via
   `USkeletalMeshComponent::PlayAnimation(anim, false)`.
2. `SkelMeshComp->GlobalAnimRateScale = 0; SetPosition(t);
   TickComponent(0, LEVELTICK_All, nullptr)` to force-evaluate the pose
   at exactly `t`.
3. For each view angle, position a `ASceneCapture2D` around the mesh
   (offset from mesh bounds centre, rotation looking at the centre).
4. **Bone overlay (key insight):** after `CaptureScene()`, read pixels,
   then project a small set of "key bones" (head, pelvis, hands, feet,
   elbows, knees) to screen space and draw coloured lines between
   parent/child pairs directly into the pixel buffer. Mesh alone hides
   joints; skeleton alone lacks context — draw both.
5. Composite the N views into one `FImage` (memcpy sub-rects) before
   saving. Single PNG per timeline → one image upload, cheap token cost.

## Tradeoffs / gotchas

- `UAnimSequenceBase::GetSkeleton()` → `USkeleton::GetPreviewMesh()`
  usually gives a usable mesh without the caller specifying one; fall
  back to the required `SkeletalMeshPath` param when preview isn't set.
- **Perf:** 5 time samples × 4 views = 20 `CaptureScene()` calls ≈
  800–1200 ms total on a typical scene. Acceptable as an on-demand
  analysis tool, not a per-frame cost.
- **Vertical-flip bug** (same as vision capture fix in commit
  `2c5eddd`): SceneCapture2D readback is bottom-up; remember to flip
  rows when copying into the composite `FImage`.
- **Lighting:** use `SCS_FinalColorLDR` with an unlit material override
  or a simple point light on the preview actor — default PIE lighting
  can make the mesh come out very dark.

## Priority

Deferred. Core vision capture shipped in commit `5dd92c1`; this is the
natural next step when someone needs automated anim notify placement.
Budget: roughly one Phase-1-sized effort.

## Alternative to consider first

Read existing `UAnimNotify` events on the AnimSequence and bone
velocity curves. Most shipped animations already have impact notifies
placed by the animator; if so, the visual analysis isn't needed.

**Audit `UnrealBridgeAnimLibrary` for existing notify / curve readout
before building the render pipeline** — the notify route is zero-cost
and covers the majority of in-project animations.

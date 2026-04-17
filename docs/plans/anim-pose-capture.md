# Animation pose capture — multi-view grid

Status: **Feature complete as of 2026-04-17.** `CaptureAnimPoseGrid`
+ `CaptureAnimMontageTimeline` both support five independent overlay
axes: bone chains, per-view framing, ground grid, root trajectory, and
customizable ground Z. All modes verified (Mannequin VaultOver —
airborne + significant root motion, Naughty Dog Ellie 1079-bone rig
door-push — stationary).

Give an agent the ability to "watch" an `AnimSequence` / `AnimMontage`
and identify semantic beats (windup start, impact frame, recovery end)
so tooling can auto-place Notifies, SoundCues, or VFX triggers without
the user clicking through the timeline frame-by-frame.

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

## Current implementation (2026-04-17)

`UUnrealBridgeLevelLibrary::CaptureAnimPoseGrid(AnimPath, Time,
SkeletalMeshPath, Views, bBoneOverlay, GridCols, CellWidth, CellHeight,
FilePath)` implemented and runtime-verified against Ellie montages in
this project.

Key implementation choices that diverged from the original sketch:

- **Scene isolation via `FPreviewScene`**, not the runtime world.
  Confirmed: no scene lighting, fog, or stray actors reach the capture.
- **`USkeletalMeshComponent` via `PreviewScene.AddComponent`** (not
  `SpawnActor<ASkeletalMeshActor>` — simpler, doesn't rely on actor
  tick).
- **Pose eval:** `SkelComp->PlayAnimation(Anim, false)` +
  `SkelComp->SetPosition(Time, false)` + `TickAnimation(0)` +
  `RefreshBoneTransforms`. `GlobalAnimRateScale=0` was tried and removed —
  unnecessary since we pass `DeltaTime=0` to `TickAnimation`.
- **`SCS_BaseColor` capture source** (not `SCS_FinalColorLDR`). Unlit
  albedo against solid grey gives clean, consistent output regardless
  of the preview scene's light placement. Final-color capture produced
  mostly-black frames because the skylight has no cubemap.
- **`SkelComp->Bounds` does NOT reflect the posed AABB** — it caches
  the mesh's authored bounds. Workaround: walk `GetComponentSpaceTransforms`
  and compute a tight posed AABB from actual bone positions.
- **Framing from pelvis bone**, not AABB centre. 1079-bone rigs
  (Ellie) have most bones clustered in face + hands, so AABB centre is
  pulled toward extremities and asymmetric poses (raised arm) offset
  the frame. Pelvis (falls back through Pelvis/hips/Hips/spine_01/
  spine/root/Root if the bone isn't named "pelvis") gives a stable
  character-anchor. Radius uses the full bone AABB's max distance to
  any corner from Centre, padded 1.2×.
- **No row flip in composite copy.** The game-world capture flips rows
  because `SCS_FinalColorLDR` readback is bottom-up; `SCS_BaseColor` in
  a `FPreviewScene` arrives top-down and a flip would invert output.

**Shipped since the above:**
- **`CaptureAnimMontageTimeline(anim, mesh, N, views, overlay, w, h, path)`** —
  grid of `N` time samples × `len(views)` with motion-aware framing:
  samples each time to build a union bone AABB, then fixes cameras
  across rows so character scale + orientation are consistent for
  visual motion comparison. `ApplyPoseAtTime` + `ComputePoseFraming`
  + `FindPelvisBoneIndex` helpers extracted; `CaptureAnimPoseGrid`
  refactored to share them.

**Bone overlay (shipped 2026-04-17):**
- 5 colour-coded chains drawn per cell before composite: spine (cyan),
  left arm (yellow), right arm (orange), left leg (magenta), right
  leg (purple). Joint markers white.
- World→pixel projection built manually from
  `FRotationMatrix(CamRot).GetUnitAxis(X/Y/Z)` so the overlay matches
  UE's SceneCapture orientation — including the implicit up-flip on
  top/bottom views where roll is ambiguous.
- Bresenham line at thickness 3 + 5×5 joint squares. Bounds-checked,
  no blending; clear on grey background.
- Canonical name candidates cover UE Mannequin (`upperarm_l`, `spine_01`),
  Mixamo (`LeftArm`) and Naughty Dog (`l_shoulder`, `spinea`,
  `l_upper_leg`). Missing bones degrade gracefully — chains shorter
  than 2 are dropped.

**Per-view framing (shipped 2026-04-17):**
- New opt-in `per_view_framing` flag on both functions.
- Each view reprojects all bone world positions into its derived
  camera basis, computes a 2D bbox + mean depth, and picks a new
  target + required distance to fit. Consensus-max distance applied
  across views so character scale stays equal between cells; per-view
  target centres each cell on its own pose extent.
- Timeline version operates on the union bone set across all time
  samples, so scale stays fixed across rows AND columns.
- Fallback to legacy pelvis-anchored framing when bone list is empty
  or direction vector is zero.

**Ground grid + root trajectory (shipped 2026-04-17):**
- `bGroundGrid` projects a Z=GroundZ world-XY grid (21×21 lines, 50 cm
  cells, ~6 m square; auto-extended to cover trajectory extent when
  trajectory is on). Axis lines through origin drawn darker so the
  agent can read direction.
- `bRootTrajectory` densely samples the pelvis bone across the whole
  anim (30 Hz, capped at 240 points), flattens XY to Z=GroundZ, draws
  as a bright-green polyline. White tick squares mark interesting
  times (captured time for pose grid; every row's sample time for
  timeline). Current frame's time gets a bigger red marker.
- Tick spacing reads as a velocity profile: even = constant, bunched =
  slow, spread = fast, cluster = stationary. Lets the agent
  disambiguate "run then stop" vs "sprint then attack" from a single
  composite image.
- Per-view framing auto-extends its bbox with trajectory points +
  ground centre so overlays stay in frame.
- Near-plane clipping (D=5 cm) on world-line projection handles
  lines that cross behind the camera cleanly.

**Still deferred:** nothing load-bearing. Possible future refinement:
smarter Z-up selection for top/bottom views (currently relies on
`(Target - CamLoc).Rotation()` which chooses the implicit up).

---

## Bone overlay — implementation strategy (drafted 2026-04-17)

Both deferred items live in the same call site
(`UnrealBridgeLevelLibrary` cpp, inside the per-cell capture loop
shared between `CaptureAnimPoseGrid` and `CaptureAnimMontageTimeline`).
Land bone overlay first (smaller, no math gotchas), then per-view
framing.

### File / call site

- **File:** `Plugin/UnrealBridge/Source/UnrealBridge/Private/UnrealBridgeLevelLibrary.cpp`
- **Existing helpers to reuse:** `ApplyPoseAtTime`, `ComputePoseFraming`,
  `FindPelvisBoneIndex` (extracted during the
  `CaptureAnimMontageTimeline` refactor — both Capture functions call
  them).
- **Insertion point:** after `SceneCapture->CaptureScene()` reads pixels
  into the per-cell `FImage`, **before** the composite-copy step that
  blits cells into the grid PNG. Drawing into the cell preserves
  alignment automatically.

### Bone selection

Walk the skeleton's `FReferenceSkeleton` once per capture and resolve
the canonical bone names below to bone indices, with priority-ordered
fallbacks (rigs name them differently). Skip any bone the skeleton
lacks — overlay should degrade gracefully on non-humanoid rigs.

| Group       | Color     | Bone name candidates (priority order)                         |
| ----------- | --------- | ------------------------------------------------------------- |
| Spine       | Cyan      | `pelvis`/`Pelvis`/`hips` → `spine_01` → `spine_02`/`spine_03` → `neck_01`/`neck` → `head` |
| Left arm    | Yellow    | `clavicle_l` → `upperarm_l` → `lowerarm_l` → `hand_l`         |
| Right arm   | Yellow    | `clavicle_r` → `upperarm_r` → `lowerarm_r` → `hand_r`         |
| Left leg    | Magenta   | `pelvis` → `thigh_l` → `calf_l` → `foot_l`                    |
| Right leg   | Magenta   | `pelvis` → `thigh_r` → `calf_r` → `foot_r`                    |

Each group is a **chain** drawn as connected segments parent→child.
Collect into `TArray<TArray<int32>>` BoneChains where outer = group,
inner = ordered bone indices.

### World→screen projection

For each cell capture, the camera is a `USceneCaptureComponent2D`
configured per-view in `ComputePoseFraming`. To project bones:

1. Read camera world transform: `Cap->GetComponentLocation()`,
   `Cap->GetComponentRotation()`.
2. Read FOV: `Cap->FOVAngle` (degrees, horizontal).
3. Build view-projection matrix manually:

```cpp
const FMatrix ViewMtx = FInverseRotationMatrix(CamRot) * FMatrix(
    FPlane(0,  0,  1, 0),  // remap UE forward (+X) to clip-Z
    FPlane(1,  0,  0, 0),
    FPlane(0,  1,  0, 0),
    FPlane(0,  0,  0, 1));
const float HalfFovRad = FMath::DegreesToRadians(Cap->FOVAngle * 0.5f);
const FMatrix ProjMtx = FPerspectiveMatrix(HalfFovRad, CellW, CellH,
    GNearClippingPlane);
const FMatrix ViewProj = FTranslationMatrix(-CamLoc) * ViewMtx * ProjMtx;
```

(Pull from `FSceneViewProjectionData` if available — but
SceneCaptureComponent2D doesn't expose it cleanly outside the renderer
module. Manual construction matches what UE's renderer does for
captures of this type.)

4. For each bone:
   - `WorldPos = SkelComp->GetComponentTransform().TransformPosition(
     SkelComp->GetBoneSpaceTransforms()[i].GetLocation())` —
     wrong; bones are stored as component-space transforms after
     `RefreshBoneTransforms`. Use
     `SkelComp->GetComponentSpaceTransforms()[BoneIdx].GetLocation()`,
     then transform by `SkelComp->GetComponentTransform()` to world.
   - `FVector4 Clip = ViewProj.TransformFVector4(FVector4(WorldPos, 1));`
   - If `Clip.W <= 0` → bone is behind camera, skip the segment.
   - `NDC = (Clip.X / Clip.W, Clip.Y / Clip.W)` ∈ [-1, 1]
   - `Pixel = ((NDC.X * 0.5 + 0.5) * CellW, (1 - (NDC.Y * 0.5 + 0.5)) * CellH)`
     — flip Y because pixel origin is top-left. Confirm during
     verify; row-flip conventions in `FPreviewScene` differ from
     game-world capture, see existing comment at the cell-copy site.

### Line drawing

- **Algorithm:** Bresenham's line, classic int32 form. ~30 lines of
  code; no need for a heavyweight pluggable rasterizer.
- **Buffer format:** `FImage` with `ERawImageFormat::BGRA8`. Read
  `Cell.RawData.GetData()` as `uint8*` and index by
  `(Y * CellW + X) * 4 + channel`.
- **Helper signature:**
  ```cpp
  void DrawLineBGRA(uint8* Pixels, int32 W, int32 H,
                    int32 X0, int32 Y0, int32 X1, int32 Y1,
                    FColor Color, int32 Thickness = 2);
  ```
  Implement thickness by drawing the same line at ±1 / ±2 pixel
  offsets perpendicular to the line direction (cheap; no anti-aliasing
  needed for the analysis use case).
- **Joint dots:** before drawing chain segments, draw a 4-px filled
  square at each bone's pixel — makes joint positions easy to read.

### Wire-in

- Add private namespace function
  `BridgePoseImpl::DrawBoneOverlay(FImage& Cell,
  USceneCaptureComponent2D* Cap, USkeletalMeshComponent* Skel,
  const TArray<TArray<int32>>& BoneChains,
  const TArray<FColor>& ChainColors)`.
- In both Capture functions, when `bBoneOverlay`, replace the
  current "log warning and skip" branch with: build BoneChains
  once (after `ApplyPoseAtTime`'s first call so skeleton is
  populated); per-cell, after `CaptureScene` + readback, call
  `DrawBoneOverlay`; then continue to composite-copy.

### Verify

- Capture a known asymmetric pose (Ellie's cast or punch montage
  at peak frame) with `bone_overlay=True`. Inspect PNG:
  - Chains visible, colors as table, line thickness ~2 px.
  - Joints sit on the corresponding mesh feature (head dot on
    head, hand dots on hands).
  - No NaN / negative-W bones drawn outside the cell — check edge
    of frame for stray pixels.
- Capture against a non-humanoid skeleton (if any in project) to
  confirm graceful degradation when canonical bones are missing.

---

## Per-view framing — implementation strategy (drafted 2026-04-17)

Replaces "all views share the same camera distance + pelvis target"
with "each view computes its own optimal frame, then a consensus
pass picks one consistent scale across views." Keeps motion
comparison legible (same character size across cells) while
eliminating dead space in asymmetric poses.

### Algorithm

For each cell (combination of view direction × time sample):

1. **Build the bone position list** in world space: the same set
   used by `ComputePoseFraming` (every bone after
   `RefreshBoneTransforms`). Already cached during pose eval.
2. **Define camera basis** for this view direction:
   - `Forward = normalized(TargetCenter - CameraLocation)` (target
     center is initially the pelvis; refined below).
   - `Right   = normalize(cross(WorldUp, Forward))`
     (WorldUp = `FVector::UpVector`).
   - `Up      = cross(Forward, Right)`.
3. **Project each bone into camera-space 2D:**
   ```
   for B in bones:
       Rel = B.WorldPos - CameraLocation
       u   = dot(Rel, Right)
       v   = dot(Rel, Up)
       d   = dot(Rel, Forward)   # depth, used for scale
   ```
4. **Compute 2D bbox** of all (u, v) and **mean depth** `d_mean`.
5. **New camera target** = world point at `CameraLocation +
   d_mean * Forward + ((u_min + u_max) / 2) * Right + ((v_min +
   v_max) / 2) * Up`. This recenters the view on the projected
   bones, not the pelvis.
6. **Required radius** in world units = `0.5 * max(u_max - u_min,
   (v_max - v_min) * AspectRatio)` × `1.15` (15 % margin).
7. **Required distance** to fit that radius vertically at
   `Cap->FOVAngle` = `radius / tan(HalfFov)`.

### Consistency pass

Across all views in one capture (and across all time samples for
`CaptureAnimMontageTimeline`), pick a single `consensus_distance =
max(per_view_distance)`. Re-place each view's camera at
`per_view_target - consensus_distance * Forward`. Same scale
everywhere; per-view target offsets keep each cell well-framed.

For timeline captures, this collapses naturally — the existing
union-AABB pass already provides the bone position superset; do
the per-view projection over the union, not per-time-sample.

### Wire-in

- New helper `BridgePoseImpl::ComputePerViewFraming(
  const TArray<FVector>& BoneWorldPositions,
  const TArray<FVector>& ViewDirections,
  float FOVDegrees, float AspectRatio,
  TArray<FVector>& OutCameraLocations,
  TArray<FRotator>& OutCameraRotations)`.
- Replaces the current `ComputePoseFraming` in both Capture
  functions when an opt-in `bPerViewFraming` flag is true.
  Default false initially, flip to true after one verify cycle
  confirms quality matches/exceeds pelvis-centred output.

### Verify

- Capture a t-pose: per-view framing should produce same/very
  similar output to pelvis-centred (symmetric pose ⇒ pelvis is
  already the bbox center).
- Capture an asymmetric pose (one arm fully extended overhead):
  - **Front view:** head and extended hand both fully visible,
    character not pushed left/right.
  - **Side view:** profile shows full vertical extent without
    cropping the hand.
  - **Top view:** frame centered on horizontal extent including
    the arm, not just on pelvis.
- Capture a montage timeline: scale stays constant across rows
  (no "zooming in/out" between frames), but each frame is
  individually well-framed.

### Risks

- **Y-up sign flips.** UE's `FRotator` uses pitch/yaw/roll with
  +Z up but X-forward; verify projection sign by capturing a
  known-asymmetric pose first and inspecting whether "raised
  right arm" appears on the right of the front view.
- **`d_mean` instead of bone-by-bone depth.** Using the mean
  depth for distance calculation slightly over-frames bones
  that are closer to camera. Acceptable for character poses
  (~2 m depth range vs ~3 m camera distance ⇒ <10 % distortion);
  if needed, refine to using depth-corrected projected radius
  per bone.
- **`USceneCaptureComponent2D` doesn't always honor
  `OrthoWidth`-style overrides cleanly.** Stick to perspective
  projection and adjust distance.
- **Top/Bottom views** — `Forward` becomes parallel or
  anti-parallel to `WorldUp`, breaking the basis construction.
  Special-case: when `|dot(Forward, WorldUp)| > 0.95`, use
  `WorldForward` (`FVector::ForwardVector`) as the up reference
  instead.

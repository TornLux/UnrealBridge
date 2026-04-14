"""Interactive 3D NavMesh pathfinder — click two points on the mesh to find a path.

Uses pyvista (VTK-based) for real 3D picking and rendering. Unlike the 2D
version, this one:
  - renders the navmesh in a rotatable 3D viewport
  - picks click points directly on the mesh surface (proper ray-cast)
  - traces the path through 3D triangle centroids (respects stairs, ramps,
    multi-floor connectivity)

Usage:
    python navmesh_pathfinder_3d.py                      # default obj path, meters
    python navmesh_pathfinder_3d.py --units cm
    python navmesh_pathfinder_3d.py path/to/nav.obj

Controls (pyvista default plus our extensions):
    left-drag   → rotate camera
    scroll      → zoom
    shift+drag  → pan
    P           → pick a point on the mesh (1st press = start, 2nd = goal)
    R           → reset current start/goal
    Q           → quit

Dependencies: numpy, pyvista  (pip install numpy pyvista)
"""
from __future__ import annotations
import argparse
import heapq
import os
import sys
from collections import defaultdict
from pathlib import Path

DEFAULT_OBJ = r"G:\UEProjects\GameplayLocomotion\Saved\NavMeshExport.obj"


# ─── OBJ parsing ───────────────────────────────────────────────────────────

def parse_obj(path: str):
    import numpy as np
    verts: list[tuple[float, float, float]] = []
    faces: list[tuple[int, int, int]] = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            if not line or line[0] == "#":
                continue
            if line.startswith("v "):
                _, x, y, z = line.split()[:4]
                verts.append((float(x), float(y), float(z)))
            elif line.startswith("f "):
                toks = line.split()[1:]
                idx = [int(t.split("/")[0]) - 1 for t in toks]
                for i in range(1, len(idx) - 1):
                    faces.append((idx[0], idx[i], idx[i + 1]))
    return np.array(verts, dtype=np.float64), np.array(faces, dtype=np.int64)


# ─── Adjacency with coord-weld (tile-seam friendly) ────────────────────────

def build_adjacency(faces, verts, weld_tolerance: float = 1.0):
    """Returns (adj, weld_id). weld_id is returned because funnel path
    smoothing needs to match triangle edges by welded vertex id too."""
    import numpy as np
    rounded = np.rint(verts / weld_tolerance).astype(np.int64)
    key_to_id: dict[tuple[int, int, int], int] = {}
    weld_id = np.empty(len(verts), dtype=np.int64)
    for i, row in enumerate(rounded):
        key = (int(row[0]), int(row[1]), int(row[2]))
        wid = key_to_id.get(key)
        if wid is None:
            wid = len(key_to_id)
            key_to_id[key] = wid
        weld_id[i] = wid

    edge_to_tris: dict[tuple[int, int], list[int]] = defaultdict(list)
    for tri_idx, (a, b, c) in enumerate(faces):
        wa, wb, wc = int(weld_id[a]), int(weld_id[b]), int(weld_id[c])
        for u, v in ((wa, wb), (wb, wc), (wc, wa)):
            key = (u, v) if u < v else (v, u)
            edge_to_tris[key].append(tri_idx)

    adj: list[list[int]] = [[] for _ in range(len(faces))]
    for tris in edge_to_tris.values():
        if len(tris) >= 2:
            for i in range(len(tris)):
                for j in range(i + 1, len(tris)):
                    adj[tris[i]].append(tris[j])
                    adj[tris[j]].append(tris[i])
    return adj, weld_id


# ─── Portal extraction + Funnel / string-pulling ───────────────────────────
#
# A* gives us a corridor (sequence of triangles). Walking centroid→centroid
# produces a visibly zig-zag path because centroids aren't on the shared
# edges. The standard NavMesh fix is the "funnel algorithm" (Mikko Mononen,
# https://digestingduck.blogspot.com/2010/03/simple-stupid-funnel-algorithm.html):
# treat each shared edge as a pair of left/right boundary points and pull
# the string taut through them. This is done in 2D (XY) because UE navmeshes
# are agent-height tessellations; the Z of each corner is taken from its
# source vertex (naturally follows stairs / ramps).


def build_portals(path, faces, verts, weld_id):
    """For each consecutive triangle pair in `path`, return the two shared
    vertex positions oriented (left, right) from the travel direction.

    Returns list of tuples (left_xyz, right_xyz); one entry per triangle
    transition (len = len(path) - 1)."""
    import numpy as np
    portals = []
    for i in range(len(path) - 1):
        a_tri = faces[path[i]]
        b_tri = faces[path[i + 1]]
        # Welded id -> original vert id within each triangle.
        wa = {int(weld_id[v]): int(v) for v in a_tri}
        wb = {int(weld_id[v]): int(v) for v in b_tri}
        shared = list(set(wa.keys()) & set(wb.keys()))
        if len(shared) != 2:
            # Defensive — shouldn't happen for a properly-welded adjacency.
            continue
        p0 = verts[wa[shared[0]]]
        p1 = verts[wa[shared[1]]]
        # Orient: "left" is the vertex on the left side of the travel
        # direction centroid_a -> centroid_b, in XY.
        ca = verts[a_tri].mean(axis=0)
        cb = verts[b_tri].mean(axis=0)
        travel = cb - ca
        edge = p1 - p0
        # 2D cross (right-handed, Z-up): positive means p1 is CCW of (p0 + travel).
        cross_xy = travel[0] * edge[1] - travel[1] * edge[0]
        if cross_xy > 0:
            portals.append((p0, p1))  # p0 = left
        else:
            portals.append((p1, p0))
    return portals


def _tri_area_2d(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])


def _xy_eq(p, q, eps=1e-6):
    return abs(p[0] - q[0]) < eps and abs(p[1] - q[1]) < eps


def funnel_pull(portals, start_xyz, end_xyz):
    """Mikko's simple-stupid funnel algorithm in XY, Z preserved per-corner.

    Input: portals from build_portals(). start/end are world-space xyz.
    Returns list of xyz corners forming the pulled path, starting at `start`
    and ending at `end`."""
    import numpy as np

    out = [np.asarray(start_xyz, dtype=np.float64)]
    if not portals:
        out.append(np.asarray(end_xyz, dtype=np.float64))
        return out

    # Treat start/end as degenerate portals so the loop has uniform form.
    left_chain = [p[0] for p in portals] + [np.asarray(end_xyz, dtype=np.float64)]
    right_chain = [p[1] for p in portals] + [np.asarray(end_xyz, dtype=np.float64)]

    apex = np.asarray(start_xyz, dtype=np.float64)
    left = apex
    right = apex
    apex_i = 0
    left_i = 0
    right_i = 0

    i = 0
    while i < len(left_chain):
        new_left = left_chain[i]
        new_right = right_chain[i]

        # Update right side.
        if _tri_area_2d(apex, right, new_right) <= 0:
            if _xy_eq(apex, right) or _tri_area_2d(apex, left, new_right) > 0:
                right = new_right
                right_i = i
            else:
                # Right crosses left — left becomes the new apex / emitted corner.
                out.append(left)
                apex = left
                apex_i = left_i
                left = apex
                right = apex
                left_i = apex_i
                right_i = apex_i
                i = apex_i + 1
                continue

        # Update left side.
        if _tri_area_2d(apex, left, new_left) >= 0:
            if _xy_eq(apex, left) or _tri_area_2d(apex, right, new_left) < 0:
                left = new_left
                left_i = i
            else:
                out.append(right)
                apex = right
                apex_i = right_i
                left = apex
                right = apex
                left_i = apex_i
                right_i = apex_i
                i = apex_i + 1
                continue

        i += 1

    out.append(np.asarray(end_xyz, dtype=np.float64))
    return out


# ─── A* on triangle centroids ──────────────────────────────────────────────

def a_star(start: int, goal: int, adj, centroids):
    import numpy as np
    if start == goal:
        return [start]
    goal_xyz = centroids[goal]

    def h(idx):
        return float(np.linalg.norm(centroids[idx] - goal_xyz))

    open_set: list[tuple[float, int]] = [(h(start), start)]
    came_from: dict[int, int] = {}
    g_score: dict[int, float] = {start: 0.0}

    while open_set:
        _, current = heapq.heappop(open_set)
        if current == goal:
            path = [current]
            while path[-1] in came_from:
                path.append(came_from[path[-1]])
            path.reverse()
            return path
        cur_xyz = centroids[current]
        for nbr in adj[current]:
            step = float(np.linalg.norm(centroids[nbr] - cur_xyz))
            tentative = g_score[current] + step
            if tentative < g_score.get(nbr, float("inf")):
                came_from[nbr] = current
                g_score[nbr] = tentative
                heapq.heappush(open_set, (tentative + h(nbr), nbr))
    return None


# ─── 3D nearest triangle (for snap-after-pick) ─────────────────────────────

def nearest_tri_to_point(point_xyz, centroids):
    import numpy as np
    d2 = ((centroids - point_xyz) ** 2).sum(axis=1)
    return int(d2.argmin())


# ─── Main ──────────────────────────────────────────────────────────────────

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("obj", nargs="?", default=DEFAULT_OBJ)
    ap.add_argument("--units", choices=("cm", "m"), default="m")
    args = ap.parse_args()

    obj_path = Path(args.obj)
    if not obj_path.is_file():
        print(f"ERROR: file not found: {obj_path}", file=sys.stderr)
        return 1

    try:
        import numpy as np
        import pyvista as pv
    except ImportError as e:
        print(f"ERROR: missing dependency — {e}. Install with:  pip install numpy pyvista",
              file=sys.stderr)
        return 2

    print(f"Loading {obj_path} ({os.path.getsize(obj_path):,} bytes) ...")
    verts, faces = parse_obj(str(obj_path))
    print(f"verts={len(verts):,}  tris={len(faces):,}")
    print("Building adjacency graph ...")
    adj, weld_id = build_adjacency(faces, verts, weld_tolerance=1.0)
    avg = sum(len(n) for n in adj) / len(adj)
    print(f"avg neighbors/tri: {avg:.2f}")
    centroids = verts[faces].mean(axis=1)  # native units (cm)

    # Display scale: convert to meters by default.
    scale = 0.01 if args.units == "m" else 1.0
    verts_d = verts * scale
    centroids_d = centroids * scale

    # Build pyvista PolyData. pv.PolyData face array format:
    #   [3, v0, v1, v2,   3, v0, v1, v2,  ...]  (prefixed count per polygon)
    face_array = np.empty((len(faces), 4), dtype=np.int64)
    face_array[:, 0] = 3
    face_array[:, 1:] = faces
    mesh = pv.PolyData(verts_d, face_array.ravel())
    mesh["height"] = verts_d[:, 2]  # per-vertex Z for color

    plotter = pv.Plotter(window_size=(1400, 950))
    plotter.set_background("#1e1e24")
    plotter.add_mesh(
        mesh,
        scalars="height",
        cmap="viridis",
        show_edges=False,
        opacity=0.95,
        smooth_shading=False,
        scalar_bar_args={"title": f"Z ({args.units})"},
    )
    plotter.add_axes()
    plotter.enable_parallel_projection()  # easier top-down navigation for navmesh work

    # Mutable UI state.
    state: dict = {
        "click_points": [],   # world xyz (display units)
        "click_tris": [],     # tri index for each click
        "overlay_actors": [], # pyvista actors to remove on reset
    }

    def clear_overlay():
        for a in state["overlay_actors"]:
            try:
                plotter.remove_actor(a)
            except Exception:
                pass
        state["overlay_actors"].clear()
        state["click_points"].clear()
        state["click_tris"].clear()
        plotter.add_text("", name="status", position="upper_left")

    def set_status(msg: str, color="white"):
        plotter.add_text(msg, name="status", position="upper_left",
                         color=color, font_size=11)

    def on_pick(point, *_extra, **_kw):
        """Called with the 3D world-space point the user clicked on the mesh.
        `*_extra`/`**_kw` absorb anything pyvista passes in later versions
        (e.g. picker, actor) — we only need the point. Names are `_extra`
        rather than `args` to avoid shadowing the argparse `args` closure."""
        import time as _time
        if point is None:
            return
        print(f"[pick] received point={np.asarray(point).round(2).tolist()} "
              f"prev_clicks={len(state['click_points'])}")
        if len(state["click_points"]) >= 2:
            clear_overlay()

        # point is in display units — convert back to native for tri lookup.
        p_native = np.asarray(point) / scale
        tri = nearest_tri_to_point(p_native, centroids)
        state["click_points"].append(np.asarray(point))
        state["click_tris"].append(tri)

        is_start = len(state["click_points"]) == 1
        marker_color = "#44ff88" if is_start else "#ff5566"
        label = "start" if is_start else "goal"

        # Marker sphere at the pick.
        sphere = pv.Sphere(radius=0.3 if args.units == "m" else 30.0, center=point)
        actor = plotter.add_mesh(sphere, color=marker_color, name=f"{label}_marker")
        state["overlay_actors"].append(actor)

        # Highlight the snapped triangle.
        tri_verts = verts_d[faces[tri]]
        tri_poly = pv.PolyData(tri_verts, np.array([[3, 0, 1, 2]], dtype=np.int64).ravel())
        tri_actor = plotter.add_mesh(
            tri_poly, color=marker_color, opacity=0.75,
            show_edges=True, edge_color=marker_color, line_width=3.5,
            name=f"{label}_tri",
        )
        state["overlay_actors"].append(tri_actor)

        if len(state["click_points"]) == 2:
            s_tri, g_tri = state["click_tris"]
            t0 = _time.time()
            path = a_star(s_tri, g_tri, adj, centroids)
            dt = (_time.time() - t0) * 1000.0
            if path is None:
                msg = f"NO PATH  (search {dt:.1f} ms - start/goal on disconnected islands)"
                set_status(msg, "#ff7777")
                print(msg)
            else:
                # Smooth the corridor with the funnel algorithm. start/end
                # are the click points (native units), portals are built in
                # native units — scale only at display time.
                start_native = state["click_points"][0] / scale
                end_native = state["click_points"][1] / scale
                portals = build_portals(path, faces, verts, weld_id)
                smoothed_native = funnel_pull(portals, start_native, end_native)
                pts_arr = np.asarray(smoothed_native) * scale

                length = float(np.linalg.norm(np.diff(pts_arr, axis=0), axis=1).sum())

                # Build a PolyData polyline for pyvista.
                line_cells = np.empty(len(pts_arr) + 1, dtype=np.int64)
                line_cells[0] = len(pts_arr)
                line_cells[1:] = np.arange(len(pts_arr))
                line_poly = pv.PolyData(pts_arr)
                line_poly.lines = line_cells
                # Lift the line above the navmesh a touch so it doesn't z-fight.
                lift = 0.05 if args.units == "m" else 5.0
                line_poly.points[:, 2] += lift

                path_actor = plotter.add_mesh(
                    line_poly, color="#ffb347", line_width=4.0, name="path_line",
                    render_lines_as_tubes=True,
                )
                state["overlay_actors"].append(path_actor)

                # Dots at each funnel corner for visibility of turn points.
                corner_pts = pts_arr.copy()
                corner_pts[:, 2] += lift
                corners_actor = plotter.add_points(
                    corner_pts, color="#ffb347", point_size=10,
                    render_points_as_spheres=True, name="path_corners",
                )
                state["overlay_actors"].append(corners_actor)

                msg = (f"path: corridor={len(path)} tris -> funnel={len(pts_arr)} "
                       f"corners, length ~ {length:.1f} {args.units}  (search {dt:.1f} ms)")
                set_status(msg, "#ffd880")
                print(msg)
        else:
            set_status(f"{label} at ({point[0]:.1f}, {point[1]:.1f}, {point[2]:.1f}) "
                       f"{args.units} tri#{tri}", marker_color)

        # Force pyvista to redraw — without this, actors added inside the
        # picking callback don't show until the next unrelated interaction
        # (rotate, resize) triggers a render.
        plotter.render()

    # enable_point_picking's callback receives the picked world-space point.
    # show_point=False: we draw our own coloured markers; pyvista's default
    # pink sphere would otherwise overlap. left_clicking=False keeps the
    # default P-key trigger so left-drag still rotates the camera.
    plotter.enable_point_picking(
        callback=on_pick,
        show_message=False,
        show_point=False,
        pickable_window=False,
        left_clicking=False,
    )

    # Reset on 'r'.
    def on_reset():
        clear_overlay()
        set_status("reset - press P to pick start")
        plotter.render()
    plotter.add_key_event("r", on_reset)

    set_status("press P to pick start, then P again for goal  (R=reset, Q=quit)")
    # Non-ASCII chars in some earlier status strings caused cp/gbk encode
    # errors on Windows consoles — all status strings above use ASCII-only.
    print("Opening 3D viewport. Press P on the mesh to place start, P again for goal.")
    plotter.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())

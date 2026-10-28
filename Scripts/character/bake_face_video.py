"""Project Live Link Face video frames onto the MetaHuman head and bake them into its face base-colour texture.

Run from the repo root, after the MetaHuman's High build (Scripts/setup_metahuman.py):
  SETUP_MH_ONLY=bakeinputs (run_python.ps1)  -> <bake>/face.fbx and base_bc.png (the built face base colour: export it
      before the photoskin stage, or with the override off, or the fill is the last bake)
  blender -b -P Scripts/character/dump_face_mesh.py -- <bake>/face.fbx <bake>/face.npz
  python Scripts/character/bake_face_video.py  -> <bake>/face_bc_video.png, copied to CHARACTERS' face_basecolor
  SETUP_MH_ONLY=photoskin (run_python.ps1)   -> set as the face's base colour override, then build.
<bake> is BAKE_DIR (default SourceArt/Character/Models/Ramil/MetaHuman/bake, git-ignored like the scan); BAKE_MOV is the
Live Link Face take's video, BAKE_FRAMES its frames (front first). Needs opencv-python, mediapipe, numpy.
Opt-in: BAKE_FEATHER (px; 0 = the original blurred mask), BAKE_SHINE (highlights kept, 1 = no clamp), BAKE_EYE_GROW (px; 0 = eyes not masked), BAKE_MOLE (json), BAKE_MATCH (0.7).
  1. A software render of the head (front) -> MediaPipe -> the 478 landmarks' 3D positions on the head (depth lookup).
  2. Each video frame: MediaPipe 2D -> solvePnP with the TrueDepth intrinsics -> head pose in that frame.
  3. Every skin texel: its 3D point and normal (UV rasterisation), projected into each frame; colour sampled where visible
     (depth test), inside that frame's face oval, weighted by how squarely it faces the camera.
  4. The video colour is matched to the base in a ring at the edge, feathered in, and written as bake/face_bc_video.png.
BAKE_SCAN=<scan.npz> (dump_face_mesh.py of prep_mh_head.py's <Name>_head_mh.fbx) with BAKE_SCAN_TEX=<its diffuse>: the
RealityScan photo texture instead of the video (steps 1-3): the scan is aligned onto the head (rigid ICP), and each texel
takes the colour of the nearest scan surface (within BAKE_SCAN_REACH mm, facing the same way).
"""
import json
import os
import sys

import cv2
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from facemetrics import landmarks  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RAMIL = os.path.join(ROOT, "SourceArt", "Character", "Models", "Ramil")
B = os.environ.get("BAKE_DIR", os.path.join(RAMIL, "MetaHuman", "bake"))
MOV = os.environ.get("BAKE_MOV", os.path.join(RAMIL, "LiveLinkFace", "20261022_MySlate_4", "MySlate_4_iPhone.mov"))
OUT = os.environ.get("BAKE_OUT", os.path.join(RAMIL, "MetaHuman", "Ramil_face_bc_video.png"))  # setup_metahuman.py's face_basecolor
FRAMES = [int(x) for x in os.environ.get("BAKE_FRAMES", "180,270,900,360,720").split(",")]
VW, VH = 1080, 1920
FX = float(os.environ.get("BAKE_FX", 2728.3477)) * VW / 2260.0  # the take's depth_metadata.mhaical IntrinsicMatrix[0] (per take)
K = np.array([[FX, 0, 1129 * VW / 2260.0], [0, FX, 2008 * VH / 4016.0], [0, 0, 1]])
TEX = 2048
OVAL = [10, 338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288, 397, 365, 379, 378, 400, 377, 152, 148, 176, 149, 150, 136,
        172, 58, 132, 93, 234, 127, 162, 21, 54, 103, 67, 109]

EYE_RINGS = [[33, 7, 163, 144, 145, 153, 154, 155, 133, 173, 157, 158, 159, 160, 161, 246],
             [362, 382, 381, 380, 374, 373, 390, 249, 263, 466, 388, 387, 386, 385, 384, 398]]
EYE_GROW = int(os.environ.get("BAKE_EYE_GROW", "0"))  # 0 = eyes not masked  # px (video is 1080 wide): the lids and a little skin around them

d = np.load(os.path.join(B, "face.npz"), allow_pickle=True)
V, F, UV, M, mats = d["V"], d["F"], d["UV"], d["M"], list(d["mats"])
# The eyeballs' centres, before only the skin is kept: the sections shaped like a 2-4 cm ball (slot names are shuffled).
EYES = []
for i in np.unique(M):
    size = np.ptp(V[np.unique(d["F"][M == i])], 0)
    if 0.02 < size.min() and size.max() < 0.04 and size.max() / size.min() < 1.2:
        EYES.append(V[np.unique(d["F"][M == i])].mean(0))
print("eyeballs at", [np.round(e * 100, 1).tolist() for e in EYES])
skin = [int(np.bincount(M).argmax())]  # the head's skin is by far the biggest section (the FBX's slot names are shuffled)
keep = np.isin(M, skin)
F, UV = F[keep], UV[keep]
print("skin triangles", len(F), "of", len(keep))
# vertex normals
fn = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]])
VN = np.zeros_like(V)
for k in range(3):
    np.add.at(VN, F[:, k], fn)
VN /= np.linalg.norm(VN, axis=1, keepdims=True) + 1e-12


def raster(points2, values, w, h, depth=None, zvals=None):
    """Rasterise triangles (points2: (n,3,2) pixel coords) into (h,w); returns index buffer (-1 empty) and barycentrics.
    With zvals (n,3) keeps the nearest (z-buffer)."""
    idx = -np.ones((h, w), np.int32)
    bary = np.zeros((h, w, 3), np.float32)
    zbuf = np.full((h, w), np.inf, np.float32)
    for i, tri in enumerate(points2):
        x0, y0 = np.floor(tri.min(0)).astype(int)
        x1, y1 = np.ceil(tri.max(0)).astype(int)
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, w - 1), min(y1, h - 1)
        if x1 < x0 or y1 < y0:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        (ax, ay), (bx, by), (cx, cy) = tri
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        l0 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / den
        l1 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / den
        l2 = 1 - l0 - l1
        inside = (l0 >= -1e-4) & (l1 >= -1e-4) & (l2 >= -1e-4)
        if not inside.any():
            continue
        yy, xx = (ys[inside] - 0.5).astype(int), (xs[inside] - 0.5).astype(int)
        if zvals is not None:
            z = l0[inside] * zvals[i, 0] + l1[inside] * zvals[i, 1] + l2[inside] * zvals[i, 2]
            closer = z < zbuf[yy, xx]
            yy, xx, z = yy[closer], xx[closer], z[closer]
            zbuf[yy, xx] = z
            sel = inside.copy(); sel[inside] = closer
        else:
            sel = inside
        idx[yy, xx] = i
        bary[yy, xx] = np.stack([l0[sel], l1[sel], l2[sel]], -1)
    return idx, bary, zbuf


def project(P, R, t, Kmat):
    c = (R @ P.T).T + t
    uv = (Kmat @ c.T).T
    return uv[:, :2] / uv[:, 2:3], c[:, 2]


# 3. texels
uvpx = np.stack([UV[..., 0] * TEX, (1 - UV[..., 1]) * TEX], -1)
tidx, tbary, _ = raster(uvpx, None, TEX, TEX)
tm = tidx >= 0
tri = F[tidx[tm]]
Pt = (V[tri] * tbary[tm][..., None]).sum(1)
Nt = (VN[tri] * tbary[tm][..., None]).sum(1)
Nt /= np.linalg.norm(Nt, axis=1, keepdims=True) + 1e-12

SCAN = os.environ.get("BAKE_SCAN")


def scan_colours():
    """BAKE_SCAN: every texel's colour from the scan's photo texture, and 1 where the scan covers it (else 0)."""
    s = np.load(SCAN, allow_pickle=True)
    SV, SF, SUV = s["V"], s["F"], s["UV"]
    photo = cv2.imread(os.environ["BAKE_SCAN_TEX"]).astype(np.float32)
    head = V[np.unique(F)].astype(np.float32)
    index = cv2.flann_Index(head, dict(algorithm=1, trees=4))
    # Rigid ICP from the noses (the front-most points: the body's measurements move the conformed head a few cm), the
    # closest 60% of scan vertices pulling it onto the head (the rest: neck and shoulders the head mesh doesn't share).
    R, t = np.eye(3), head[head[:, 1].argmin()] - SV[SV[:, 1].argmin()]
    for step in range(40):
        P = SV @ R.T + t
        near, d2 = index.knnSearch(P.astype(np.float32), 1, params={})
        d = np.sqrt(d2.ravel())
        keep = d < np.percentile(d, 60)
        A, B = P[keep], head[near.ravel()[keep]].astype(np.float64)
        ca, cb = A.mean(0), B.mean(0)
        U, _, Wt = np.linalg.svd((A - ca).T @ (B - cb))
        dR = (U @ np.diag([1, 1, np.sign(np.linalg.det(U @ Wt))]) @ Wt).T
        R, t = dR @ R, dR @ (t - ca) + cb
        if step == 0:
            print(f"scan to head before ICP: median {np.median(d) * 1000:.1f} mm")
    print(f"scan to head after ICP: median {np.median(d) * 1000:.1f} mm, moved {np.round(t * 100, 2)} cm")
    SV = SV @ R.T + t
    a, b, c = SV[SF[:, 0]], SV[SF[:, 1]], SV[SF[:, 2]]
    sn = np.cross(b - a, c - a)
    sn /= np.linalg.norm(sn, axis=1, keepdims=True) + 1e-12
    tris = cv2.flann_Index(((a + b + c) / 3).astype(np.float32), dict(algorithm=1, trees=4))
    k, _ = tris.knnSearch(Pt.astype(np.float32), 1, params={})
    k = k.ravel()
    # The texel's point dropped onto that triangle's plane, its barycentrics (clamped into the triangle) -> the scan's UV.
    q = Pt - ((Pt - a[k]) * sn[k]).sum(1, keepdims=True) * sn[k]
    v0, v1, v2 = b[k] - a[k], c[k] - a[k], q - a[k]
    d00, d01, d11 = (v0 * v0).sum(1), (v0 * v1).sum(1), (v1 * v1).sum(1)
    d20, d21 = (v2 * v0).sum(1), (v2 * v1).sum(1)
    den = d00 * d11 - d01 * d01 + 1e-18
    w1, w2 = (d11 * d20 - d01 * d21) / den, (d00 * d21 - d01 * d20) / den
    w = np.clip(np.stack([1 - w1 - w2, w1, w2], 1), 0, None)
    w /= w.sum(1, keepdims=True)
    uv = (SUV[k] * w[..., None]).sum(1)
    h, wd = photo.shape[:2]
    x = np.clip(uv[:, 0] * wd - 0.5, 0, wd - 1.001)
    y = np.clip((1 - uv[:, 1]) * h - 0.5, 0, h - 1.001)
    x0, y0 = x.astype(int), y.astype(int)
    fx_, fy_ = (x - x0)[:, None], (y - y0)[:, None]
    col = (photo[y0, x0] * (1 - fx_) * (1 - fy_) + photo[y0, x0 + 1] * fx_ * (1 - fy_)
           + photo[y0 + 1, x0] * (1 - fx_) * fy_ + photo[y0 + 1, x0 + 1] * fx_ * fy_)
    reach = float(os.environ.get("BAKE_SCAN_REACH", "12")) / 1000
    hit = np.linalg.norm(Pt - (a[k] * w[:, :1] + b[k] * w[:, 1:2] + c[k] * w[:, 2:]), axis=1) < reach
    hit &= np.abs((sn[k] * Nt).sum(1)) > 0.2  # phone photogrammetry is lumpy: only reject the far side of a fold
    # Specks the scan missed (pits in its surface) are filled from their neighbours' sampling: a closing in texture space.
    cover = np.zeros((TEX, TEX), np.uint8)
    cover[tm] = hit
    cover = cv2.morphologyEx(cover, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (25, 25)))
    hit = cover[tm] > 0
    # Hair the prep left at the hairline and sideburns (dark or grey, not skin-warm) and the scalp and sides behind it.
    luma = col @ np.array([0.114, 0.587, 0.299]) / 255  # BGR
    warmth = (col[:, 2] - col[:, 0]) / 255
    hit &= (luma > float(os.environ.get("BAKE_SCAN_HAIR_LUMA", "0.22"))) & (warmth > 0.02)
    hit &= Nt[:, 1] < -0.25  # facing forward (the head looks down -Y)
    if EYES:  # the hairline's ragged fringe: nothing above BAKE_SCAN_TOP mm over the eyes
        hit &= Pt[:, 2] < np.mean([e[2] for e in EYES]) + float(os.environ.get("BAKE_SCAN_TOP", "45")) / 1000
    # The scan's eyes are open: its lids, lashes and whites would paint dark sockets over the closed lids here. The lids
    # (within BAKE_SCAN_EYES mm of an eyeball's centre) keep the base; the eye meshes and lash grooms draw the rest.
    for eye in EYES:
        hit &= np.linalg.norm(Pt - eye, axis=1) > float(os.environ.get("BAKE_SCAN_EYES", "17")) / 1000
    print(f"scan covers {hit.sum()} of {len(Pt)} skin texels")
    return col, hit.astype(np.float64)


if SCAN:
    acc, wsum = scan_colours()
    acc = acc * wsum[:, None]
else:
    # 1. landmarks on the head: a front render (camera on -Y looking +Y; Blender world, metres)
    centre = V[np.unique(F)].mean(0)
    front_y = V[np.unique(F)][:, 1].min()
    Rf = np.array([[1.0, 0, 0], [0, 0, -1], [0, 1, 0]])  # cam x = X, cam y = -Z, cam z = +Y
    C = np.array([centre[0], front_y - 0.34, centre[2] + 0.02])
    tf = -Rf @ C
    RW, RH = 1080, 1440
    Kr = np.array([[FX, 0, RW / 2], [0, FX, RH / 2], [0, 0, 1]])
    pv, zv = project(V, Rf, tf, Kr)
    idx, bary, zbuf = raster(pv[F], None, RW, RH, zvals=zv[F])
    shade = np.zeros((RH, RW), np.float32)
    hit = idx >= 0
    n = (VN[F[idx[hit]]] * bary[hit][..., None]).sum(1)
    shade[hit] = np.clip(-(Rf @ n.T).T[:, 2], 0, 1) * 0.85 + 0.1  # facing the camera = bright
    cv2.imwrite(os.path.join(B, "head_render.png"), (np.dstack([shade] * 3) * 255).astype(np.uint8))
    L2 = landmarks(os.path.join(B, "head_render.png"))
    if L2 is None:
        raise SystemExit("MediaPipe found no face on the head render")
    P3 = np.zeros((478, 3))
    ok = np.zeros(478, bool)
    for i, (x, y, _) in enumerate(L2):
        xi, yi = int(round(x)), int(round(y))
        if 0 <= xi < RW and 0 <= yi < RH and idx[yi, xi] >= 0:
            tri = F[idx[yi, xi]]
            P3[i] = (V[tri] * bary[yi, xi][:, None]).sum(0)
            ok[i] = True
    print("landmarks on the head:", ok.sum())

    # 2. poses in the video frames
    cap = cv2.VideoCapture(MOV)
    if not cap.isOpened():
        sys.exit(f"can't open {MOV}")
    views = []
    for fno in FRAMES:
        cap.set(cv2.CAP_PROP_POS_FRAMES, fno)
        good, img = cap.read()
        path = os.path.join(B, f"frame_{fno}.png")
        cv2.imwrite(path, img)
        L = landmarks(path)
        if L is None:
            print("no face in frame", fno); continue
        use = ok & ~np.isin(np.arange(478), list(range(468, 478)))  # irises out
        succ, rvec, tvec, inl = cv2.solvePnPRansac(P3[use].astype(np.float64), L[use, :2].astype(np.float64), K, None,
                                                  reprojectionError=10.0, iterationsCount=1000, flags=cv2.SOLVEPNP_ITERATIVE)
        if inl is not None and len(inl) > 20:
            ii = inl.ravel()
            rvec, tvec = cv2.solvePnPRefineLM(P3[use][ii].astype(np.float64), L[use, :2][ii].astype(np.float64), K, None, rvec, tvec)
        R, _ = cv2.Rodrigues(rvec)
        rp, _ = project(P3[use], R, tvec.ravel(), K)
        err = np.linalg.norm(rp - L[use, :2], axis=1)
        print(f"frame {fno}: pose ok={succ} inliers {0 if inl is None else len(inl)}/{use.sum()} reproj median {np.median(err):.1f}px "
              f"face distance {np.linalg.norm(R @ centre + tvec.ravel())*100:.1f}cm")
        views.append((fno, img.astype(np.float32), R, tvec.ravel(), L))

    # 3. texels: each one's colour from the frames
    acc = np.zeros((len(Pt), 3)); wsum = np.zeros(len(Pt))
    for fno, img, R, t, L in views:
        pv2, zv2 = project(V, R, t, K)
        vidx, _, vz = raster(pv2[F], None, VW, VH, zvals=zv2[F])
        p2, z2 = project(Pt, R, t, K)
        x, y = p2[:, 0], p2[:, 1]
        inimg = (x >= 0) & (x < VW - 1) & (y >= 0) & (y < VH - 1)
        xi, yi = np.clip(x.astype(int), 0, VW - 1), np.clip(y.astype(int), 0, VH - 1)
        visible = inimg & (z2 <= vz[yi, xi] + 0.004)  # 4 mm depth tolerance
        oval = cv2.fillPoly(np.zeros((VH, VW), np.uint8), [L[OVAL, :2].astype(np.int32)], 1)
        oval = cv2.erode(oval, np.ones((21, 21), np.uint8))  # just inside the outline (keeps the chin and jaw)
        # hair: dark pixels above the brows (the brows themselves and the stubble stay)
        hsv = cv2.cvtColor(img.astype(np.uint8), cv2.COLOR_BGR2HSV)
        brow_y = min(L[105, 1], L[334, 1], L[70, 1], L[300, 1]) - 0.03 * abs(L[152, 1] - L[10, 1])
        hair = (hsv[..., 2] < 70) & (np.arange(VH)[:, None] < brow_y)
        oval[cv2.dilate(hair.astype(np.uint8), np.ones((15, 15), np.uint8)) > 0] = 0
        # The eyes: open in the video, so their lids, lashes and whites land on the mesh's lids and read as heavy dark sockets.
        # They keep the base's (the eye meshes and lash grooms draw the rest).
        for ring in EYE_RINGS if EYE_GROW else []:
            eye = cv2.fillPoly(np.zeros((VH, VW), np.uint8), [L[ring, :2].astype(np.int32)], 1)
            oval[cv2.dilate(eye, np.ones((EYE_GROW, EYE_GROW), np.uint8)) > 0] = 0
        inside = oval[yi, xi] > 0
        view_dir = -((R @ Pt.T).T + t)
        view_dir /= np.linalg.norm(view_dir, axis=1, keepdims=True)
        facing = np.clip(((R @ Nt.T).T * view_dir).sum(1), 0, 1)
        # Sharp facing weight: the view that sees a texel most squarely dominates it, so views registered a few pixels
        # apart don't average into blur; the front frame (best registered) is preferred where it's nearly as good.
        w = np.where(visible & inside, facing ** 12 * (1.6 if fno == FRAMES[0] else 1.0), 0.0)
        x0, y0 = np.clip(np.floor(x).astype(int), 0, VW - 2), np.clip(np.floor(y).astype(int), 0, VH - 2)
        fx_, fy_ = (x - x0)[:, None], (y - y0)[:, None]
        col = (img[y0, x0] * (1 - fx_) * (1 - fy_) + img[y0, x0 + 1] * fx_ * (1 - fy_)
               + img[y0 + 1, x0] * (1 - fx_) * fy_ + img[y0 + 1, x0 + 1] * fx_ * fy_)
        acc += col * w[:, None]; wsum += w
        print(f"frame {fno}: {np.count_nonzero(w)} texels")
video = np.zeros((TEX, TEX, 3), np.float32); wmap = np.zeros((TEX, TEX), np.float32)
video[tm] = acc / np.maximum(wsum, 1e-6)[:, None]; wmap[tm] = wsum

# 4. blend into the base
base = cv2.imread(os.path.join(B, "base_bc.png"), cv2.IMREAD_UNCHANGED)
base = cv2.resize(base[..., :3], (TEX, TEX)).astype(np.float32)
valid = (wmap > 0.02).astype(np.uint8)
valid = cv2.erode(valid, np.ones((9, 9), np.uint8))
# Feather inside the patch only (distance from its edge), never into texels the video didn't cover: a blurred mask used to
# spread over them and drag the video's empty (black) fill in, leaving a hard, lighter or darker rim around the patch.
FEATHER = float(os.environ.get("BAKE_FEATHER", "0"))  # 0 = the original blurred mask
if FEATHER > 0:
    dist = cv2.distanceTransform(valid, cv2.DIST_L2, 5)
    alpha = np.clip(dist / FEATHER, 0, 1)
    alpha = alpha * alpha * (3 - 2 * alpha)  # smoothstep
else:  # the original: a wide, soft edge ("the video's light falls off where the base's doesn't")
    alpha = np.clip(wmap / 0.02, 0, 1)
    alpha = cv2.erode(alpha, np.ones((31, 31), np.uint8))
    alpha = cv2.GaussianBlur(alpha, (0, 0), 40)
    alpha = np.clip(alpha / max(alpha.max(), 1e-6), 0, 1) ** 1.5
# Colour: the video's skin in Lab moved MATCH of the way to the base's over the same patch (mean and spread), so the patch
# sits in the character's skin tone without losing its own detail.
MATCH = float(os.environ.get("BAKE_MATCH", "0.7"))
core = alpha > 0.9
vl = cv2.cvtColor(np.clip(video, 0, 255).astype(np.uint8), cv2.COLOR_BGR2LAB).astype(np.float32)
bl = cv2.cvtColor(base.astype(np.uint8), cv2.COLOR_BGR2LAB).astype(np.float32)
vm, vs = vl[core].mean(0), vl[core].std(0) + 1e-3
bm, bs = bl[core].mean(0), bl[core].std(0) + 1e-3
tm_, ts_ = vm + MATCH * (bm - vm), vs + MATCH * (bs - vs)
vl = (vl - vm) / vs * ts_ + tm_
print("Lab video mean", np.round(vm, 1), "base", np.round(bm, 1), "->", np.round(tm_, 1))
# The face in the video is oily and shines (forehead, nose bridge, cheeks): soft-clip lightness above the mean so the
# highlights aren't painted into the base colour (the skin shader adds its own).
SHINE = float(os.environ.get("BAKE_SHINE", "1"))  # 1 = no clamp
top = tm_[0] + 0.4 * ts_[0]
vl[..., 0] = np.where(vl[..., 0] > top, top + (vl[..., 0] - top) * SHINE, vl[..., 0])
video = cv2.cvtColor(np.clip(vl, 0, 255).astype(np.uint8), cv2.COLOR_LAB2BGR).astype(np.float32)
if FEATHER > 0:
    video = np.where(valid[..., None] > 0, video, base)  # texels the video didn't cover take the base
# Near the patch's edge the photo's broad light (darker temples, a brighter forehead) is swapped for the base's, so no band
# shows where they meet; the middle keeps the photo's own.
low_v = cv2.GaussianBlur(video, (0, 0), 60)
low_b = cv2.GaussianBlur(base, (0, 0), 60)
edge = (1 - np.clip((alpha - 0.05) / 0.35, 0, 1))[..., None]  # only the outer ring of the patch
print("alpha > 0.9 covers", round(float((alpha > 0.9).mean() * 100), 1), "% of the texture; ring", round(float(((alpha > 0.05) & (alpha < 0.4)).mean() * 100), 1), "%")
video = video + (low_b - low_v) * edge
out = base * (1 - alpha[..., None]) + video * alpha[..., None]
# A mole (BAKE_MOLE json: a MediaPipe landmark triangle + barycentric weights + size in outer-eye-corner distances, as
# measured on a photo): placed on the head through this head's landmarks, so it follows any sculpting.
MOLE = os.environ.get("BAKE_MOLE")
if MOLE and os.path.exists(MOLE) and not SCAN:
    m = json.load(open(MOLE))
    if all(ok[i] for i in m["tri"]):
        pm = (P3[m["tri"]] * np.array(m["w"])[:, None]).sum(0)
        ipd3 = np.linalg.norm(P3[33] - P3[263])
        sigma = m["diameter_px"] / m["ipd_px"] * ipd3 / 2.5 * float(os.environ.get("BAKE_MOLE_SCALE", "1"))
        ys, xs = np.nonzero(tm)
        d = np.linalg.norm(Pt - pm, axis=1)
        sel = d < 3.5 * sigma
        wgt = (0.85 * np.exp(-0.5 * (d[sel] / sigma) ** 2))[:, None]
        mole_bgr = np.array(m["color_rgb"][::-1], np.float32)
        out[ys[sel], xs[sel]] = out[ys[sel], xs[sel]] * (1 - wgt) + mole_bgr * wgt
        print(f"mole at {np.round(pm * 100, 2)} cm, sigma {sigma * 1000:.2f} mm, {int(sel.sum())} texels")
    else:
        print("mole: its landmarks aren't on the head render")
cv2.imwrite(os.path.join(B, "face_bc_video.png"), np.clip(out, 0, 255).astype(np.uint8))
cv2.imwrite(OUT, np.clip(out, 0, 255).astype(np.uint8))
cv2.imwrite(os.path.join(B, "face_bc_alpha.png"), (alpha * 255).astype(np.uint8))
json.dump({"scan": SCAN} if SCAN else {"frames": FRAMES}, open(os.path.join(B, "bake.json"), "w"))
print("wrote", OUT)

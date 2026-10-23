"""Face proportions from MediaPipe's 478 landmarks, normalised by the distance between the outer eye corners.
Usage: python facemetrics.py <image> [<image> ...]  -> prints a table; with --json writes <image>.lm.json"""
import json
import math
import os
import sys
import urllib.request

import mediapipe as mp
import numpy as np

# MediaPipe's face landmarker model, fetched once into Saved/ (not in git).
MODEL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Saved", "mediapipe", "face_landmarker.task")
MODEL_URL = "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/latest/face_landmarker.task"

# name: (a, b) landmark pairs; several pairs are averaged (left/right)
METRICS = {
    "cheek_w": [(234, 454)],
    "jaw_w": [(172, 397)],
    "chin_w": [(176, 400)],
    "nasion_chin": [(168, 152)],
    "lower_face": [(2, 152)],
    "lip_chin": [(17, 152)],
    "nose_len": [(168, 2)],
    "nose_w": [(129, 358)],
    "mouth_w": [(61, 291)],
    "eye_w": [(33, 133), (362, 263)],
    "eye_h": [(159, 145), (386, 374)],
    "brow_eye": [(105, 159), (334, 386)],
    "brow_w": [(70, 300)],
}


def landmarks(path):
    if not os.path.exists(MODEL):
        os.makedirs(os.path.dirname(MODEL), exist_ok=True)
        urllib.request.urlretrieve(MODEL_URL, MODEL)
    opts = mp.tasks.vision.FaceLandmarkerOptions(base_options=mp.tasks.BaseOptions(model_asset_path=MODEL), num_faces=1)
    with mp.tasks.vision.FaceLandmarker.create_from_options(opts) as det:
        img = mp.Image.create_from_file(path)
        res = det.detect(img)
        if not res.face_landmarks:
            return None
        # 3D: MediaPipe's z is in the same units as x (image width), so distances don't depend on head pose
        return np.array([[p.x * img.width, p.y * img.height, p.z * img.width] for p in res.face_landmarks[0]])


def metrics(lm):
    ipd = np.linalg.norm(lm[33] - lm[263])  # outer eye corners: steadier than the pupils, which follow the gaze
    out = {}
    for name, pairs in METRICS.items():
        out[name] = float(np.mean([np.linalg.norm(lm[a] - lm[b]) for a, b in pairs]) / ipd)
    return out


if __name__ == "__main__":
    paths = [a for a in sys.argv[1:] if not a.startswith("--")]
    rows = []
    for p in paths:
        lm = landmarks(p)
        if lm is None:
            print(f"{p}: no face")
            continue
        if "--json" in sys.argv:
            json.dump(lm.tolist(), open(p + ".lm.json", "w"))
        rows.append((p.rsplit("/", 1)[-1][-40:], metrics(lm)))
    names = list(METRICS)
    print("metric".ljust(12) + "".join(r[0][-14:].rjust(16) for r in rows))
    for n in names:
        line = n.ljust(12) + "".join(f"{r[1][n]:16.3f}" for r in rows)
        if len(rows) == 2:
            line += f"   {100 * (rows[1][1][n] / rows[0][1][n] - 1):+6.1f}%"
        print(line)

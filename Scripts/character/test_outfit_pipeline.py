"""Blender (5.2) checks for the outfit pipeline helpers (docs/character/outfit-steps.md). Run from the repo root:

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b --factory-startup -P Scripts/character/test_outfit_pipeline.py

Prints PASS, or fails with the first broken assertion (exit 1).
"""
import os
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

MODELS = os.path.join("SourceArt", "Character", "Models")


def body(name):
    return os.path.join(MODELS, name, f"{name}_rigged.fbx")


def test_landmarks():
    from body_landmarks import measure
    hero = measure(body("Hero"))
    assert hero["tail"] is None, hero["tail"]
    assert 0.18 < hero["chest"][1] < 0.28, hero["chest"]  # Hero's chest is ~0.23 m deep
    cat = measure(body("Cat"))
    assert cat["tail"] is not None, "the Cat has a tail"
    assert cat["tail"]["behind"] > 0.05 and cat["tail"]["tip"][2] < cat["hips_z"], cat["tail"]
    misha = measure(body("Misha"))
    assert misha["tail"] is not None, "Misha has a tail"
    assert measure(body("Pabu"))["tail"] is None, "Pabu's trouser folds are not a tail"
    bika = measure(body("Bika"))
    assert bika["tail"] is None
    assert bika["chest"][1] > hero["chest"][1] + 0.08, (bika["chest"], hero["chest"])  # ~0.35 vs ~0.23


try:
    for name, test in sorted(globals().items()):
        if name.startswith("test_") and callable(test):
            test()
            print(f"[test_outfit_pipeline] {name} ok")
    print("PASS")
except Exception:  # noqa: BLE001 - any failure fails the run
    traceback.print_exc()
    print("FAIL")
    sys.exit(1)

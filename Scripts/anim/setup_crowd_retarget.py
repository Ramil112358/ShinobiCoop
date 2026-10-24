"""IK_Crowd and RTG_MannyToCrowd in /Game/Shinobi/AnimGen: the retargeter that copies a Londoner's hidden Quinn (walking
with AnimGen) onto their City Sample crowd body (AShinobiVillager::WearCrowdLook). Idempotent; close the GUI editor.

  powershell -File Scripts/run_python.ps1 -Script Scripts/anim/setup_crowd_retarget.py

Built like setup_metahuman.py's retarget(): every crowd body is on metahuman_base_skel, which has Manny's bone names and
hands, so the chains map exactly and there is no arm spread or hand roll to fix. The rig is characterised on one body; the
retargeter re-reads the reference pose of whichever body it runs on (man or woman, any build).
"""
import unreal

DEST = "/Game/Shinobi/AnimGen"
BODY = "/Game/CitySampleCrowd/Character/Male/NormalWeight/Meshes/m_tal_nrw_body"
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(message):
    unreal.log(f"[setup_crowd_retarget] {message}")


def main():
    body = unreal.load_asset(BODY)
    ik_manny = unreal.load_asset("/Game/Shinobi/Retarget/IK_Manny")
    if not body or not ik_manny:
        raise RuntimeError(f"missing {BODY} or IK_Manny (Scripts/setup_chakra_pose.py)")
    rig = unreal.load_asset(f"{DEST}/IK_Crowd") or unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(DEST, "IK_Crowd")
    rig_ctl = unreal.IKRigController.get_controller(rig)
    if not rig_ctl.set_skeletal_mesh(body) or not rig_ctl.apply_auto_generated_retarget_definition():
        raise RuntimeError("IK_Crowd: auto-characterisation failed")
    EAL.save_loaded_asset(rig)
    rtg = unreal.load_asset(f"{DEST}/RTG_MannyToCrowd") or TOOLS.create_asset("RTG_MannyToCrowd", DEST, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctl = unreal.IKRetargeterController.get_controller(rtg)
    source, target = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    ctl.set_ik_rig(source, ik_manny)
    ctl.set_ik_rig(target, rig)
    ctl.remove_all_ops()
    ctl.add_default_ops()
    ctl.assign_ik_rig_to_all_ops(source, ik_manny)
    ctl.assign_ik_rig_to_all_ops(target, rig)
    ctl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    ctl.reset_retarget_pose(ctl.get_current_retarget_pose_name(target), [], target)
    ctl.auto_align_all_bones(target, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    # Root Motion op off: Londoners move by their capsule. Run IK Rig off, as on every other body.
    for index in range(ctl.get_num_retarget_ops()):
        if isinstance(ctl.get_op_controller(index), (unreal.IKRetargetRunIKRigController, unreal.IKRetargetRootMotionController)):
            ctl.set_retarget_op_enabled(index, False)
    missing = [c for c in ("Spine", "Neck", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg") if not str(ctl.get_source_chain(c))]
    if missing:
        raise RuntimeError(f"RTG_MannyToCrowd: no Manny chain for {missing}")
    EAL.save_loaded_asset(rtg)
    log(f"built {rtg.get_path_name()}")


main()

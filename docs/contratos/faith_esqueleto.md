# Esqueleto y animaciones de Faith en primera persona (A1)

> Generado por `python mod/tools/extractor/extract.py --step a1`. Solo nombres: ningún archivo del juego.

- **Malla elegida:** `SK_UpperBody` (74 huesos, 3540 vértices, 6876 triángulos, 2 materiales)
- **Otras mallas del paquete:** `CINE_Female1p`, `SK_LowerBody`, `FemaleArmsStatic1_pose1`, `glove`
- **Límite de Source:** 128 huesos por modelo → ✅ cabe

## Huesos

| # | Hueso | Padre |
|---|---|---|
| 0 | `root` | `—` |
| 1 | `Hips` | `root` |
| 2 | `Spine` | `Hips` |
| 3 | `Spine1` | `Spine` |
| 4 | `LeftUpLeg` | `Hips` |
| 5 | `LeftLeg` | `LeftUpLeg` |
| 6 | `LeftFoot` | `LeftLeg` |
| 7 | `LeftToeBase` | `LeftFoot` |
| 8 | `LeftUpLegRoll` | `LeftUpLeg` |
| 9 | `RightUpLeg` | `Hips` |
| 10 | `RightLeg` | `RightUpLeg` |
| 11 | `RightFoot` | `RightLeg` |
| 12 | `RightToeBase` | `RightFoot` |
| 13 | `RightUpLegRoll` | `RightUpLeg` |
| 14 | `SpineX` | `root` |
| 15 | `SpineXLeft` | `SpineX` |
| 16 | `LeftShoulder` | `SpineXLeft` |
| 17 | `LeftArm` | `LeftShoulder` |
| 18 | `LeftForeArm` | `LeftArm` |
| 19 | `LeftHand` | `LeftForeArm` |
| 20 | `LeftWeapon` | `LeftHand` |
| 21 | `LeftHandMiddle0` | `LeftHand` |
| 22 | `LeftHandMiddle1` | `LeftHandMiddle0` |
| 23 | `LeftHandMiddle2` | `LeftHandMiddle1` |
| 24 | `LeftHandMiddle3` | `LeftHandMiddle2` |
| 25 | `LeftHandRing0` | `LeftHand` |
| 26 | `LeftHandRing1` | `LeftHandRing0` |
| 27 | `LeftHandRing2` | `LeftHandRing1` |
| 28 | `LeftHandRing3` | `LeftHandRing2` |
| 29 | `LeftHandPinky0` | `LeftHand` |
| 30 | `LeftHandPinky1` | `LeftHandPinky0` |
| 31 | `LeftHandPinky2` | `LeftHandPinky1` |
| 32 | `LeftHandPinky3` | `LeftHandPinky2` |
| 33 | `LeftHandIndex0` | `LeftHand` |
| 34 | `LeftHandIndex1` | `LeftHandIndex0` |
| 35 | `LeftHandIndex2` | `LeftHandIndex1` |
| 36 | `LeftHandIndex3` | `LeftHandIndex2` |
| 37 | `LeftHandThumb1` | `LeftHand` |
| 38 | `LeftHandThumb2` | `LeftHandThumb1` |
| 39 | `LeftHandThumb3` | `LeftHandThumb2` |
| 40 | `LeftWristRoll` | `LeftHand` |
| 41 | `LeftForeArmRoll` | `LeftForeArm` |
| 42 | `Neck` | `SpineX` |
| 43 | `Head` | `Neck` |
| 44 | `SpineXRight` | `SpineX` |
| 45 | `RightShoulder` | `SpineXRight` |
| 46 | `RightArm` | `RightShoulder` |
| 47 | `RightForeArm` | `RightArm` |
| 48 | `RightHand` | `RightForeArm` |
| 49 | `RightWeapon` | `RightHand` |
| 50 | `RightHandMiddle0` | `RightHand` |
| 51 | `RightHandMiddle1` | `RightHandMiddle0` |
| 52 | `RightHandMiddle2` | `RightHandMiddle1` |
| 53 | `RightHandMiddle3` | `RightHandMiddle2` |
| 54 | `RightHandRing0` | `RightHand` |
| 55 | `RightHandRing1` | `RightHandRing0` |
| 56 | `RightHandRing2` | `RightHandRing1` |
| 57 | `RightHandRing3` | `RightHandRing2` |
| 58 | `RightHandPinky0` | `RightHand` |
| 59 | `RightHandPinky1` | `RightHandPinky0` |
| 60 | `RightHandPinky2` | `RightHandPinky1` |
| 61 | `RightHandPinky3` | `RightHandPinky2` |
| 62 | `RightHandIndex0` | `RightHand` |
| 63 | `RightHandIndex1` | `RightHandIndex0` |
| 64 | `RightHandIndex2` | `RightHandIndex1` |
| 65 | `RightHandIndex3` | `RightHandIndex2` |
| 66 | `RightHandThumb1` | `RightHand` |
| 67 | `RightHandThumb2` | `RightHandThumb1` |
| 68 | `RightHandThumb3` | `RightHandThumb2` |
| 69 | `RightWristRoll` | `RightHand` |
| 70 | `LeftHand_GameIK` | `RightHand` |
| 71 | `RightForeArmRoll` | `RightForeArm` |
| 72 | `EyeJoint` | `root` |
| 73 | `CameraJoint` | `EyeJoint` |

## Materiales

- `MI_Female_Arm_Skinn`
- `MI_Female_Arm_Glove`

## Animaciones

### `AS_C1P_Unarmed` (281 animaciones, 74 huesos)

| Animación | Frames | FPS | Usada como |
|---|---|---|---|
| `againstwall` | 60 | 30 |  |
| `AirBargeIdle` | 30 | 30 |  |
| `AirBargeImpact` | 30 | 30 |  |
| `AirBargeLand` | 15 | 30 |  |
| `autostepupleftleg` | 10 | 30 |  |
| `autostepuprightleg` | 10 | 30 |  |
| `bargeinleft` | 18 | 30 |  |
| `bargeoutleft` | 32 | 30 |  |
| `blendshapetest` | 179 | 30 |  |
| `cameralanding` | 33 | 30 |  |
| `crouchbwd` | 30 | 30 |  |
| `crouchfwd` | 30 | 30 | crouch_walk |
| `CrouchIntoStand` | 20 | 30 |  |
| `CrouchLeft1` | 30 | 30 |  |
| `crouchleft2` | 30 | 30 |  |
| `crouchright1` | 30 | 30 |  |
| `crouchright2` | 30 | 30 |  |
| `CrouchSlide` | 45 | 30 | slide |
| `CrouchSlideEnd` | 20 | 30 |  |
| `crouchslideend45` | 20 | 30 |  |
| `crouchslideintoend45` | 20 | 30 |  |
| `crouchslidetocrouch` | 15 | 30 |  |
| `crouchstill` | 60 | 30 | crouch |
| `CrouchTurn45Left` | 24 | 30 |  |
| `CrouchTurn45Right` | 24 | 30 |  |
| `CrouchTurn90Left` | 24 | 30 |  |
| `CrouchTurn90Right` | 24 | 30 |  |
| `die` | 130 | 30 |  |
| `diecrouch` | 130 | 30 |  |
| `DiePursuitFinish` | 41 | 30 |  |
| `dodgejumpleft` | 25 | 30 |  |
| `dodgejumpright` | 25 | 30 |  |
| `edgedetection` | 50 | 30 |  |
| `edgedetectionidle` | 60 | 30 |  |
| `evaderoll` | 30 | 30 |  |
| `FallingLandDie` | 170 | 30 |  |
| `FallingLandDieBwd` | 170 | 30 |  |
| `fallinglandhard` | 60 | 30 | land_hard |
| `fallinglandhard2` | 60 | 30 |  |
| `fallinglandintosoftlanding` | 49 | 30 |  |
| `FallingLandMedium` | 20 | 30 |  |
| `fallinglandmediumx` | 40 | 30 |  |
| `fallinglandroll` | 38 | 30 | skill_roll |
| `fallinglandsoftlanding` | 45 | 30 | land_soft |
| `fallinguncontrolled` | 60 | 30 | fall_uncontrolled |
| `fallinguncontrolledbwd` | 59 | 30 |  |
| `gethitback` | 24 | 30 |  |
| `GetHitFallback` | 26 | 30 |  |
| `GetHitFallbackTurn` | 18 | 30 |  |
| `gethitfront` | 24 | 30 |  |
| `gethitleft` | 24 | 30 |  |
| `gethitright` | 24 | 30 |  |
| `GetHitStumbleBwd` | 31 | 30 |  |
| `GetHitStumbleBwdFar` | 45 | 30 |  |
| `GetHitStumbleBwdFar2` | 49 | 30 |  |
| `gethittaser` | 24 | 30 |  |
| `Hang` | 60 | 30 | ledge_hang |
| `hang45left` | 60 | 30 |  |
| `hang45right` | 60 | 30 |  |
| `HangCornerOutSideLeft` | 41 | 30 |  |
| `HangCornerOutSideRight` | 41 | 30 |  |
| `HangEnd` | 10 | 30 |  |
| `hangfoldedendhang` | 40 | 30 |  |
| `HangFoldedHeaveUp` | 36 | 30 |  |
| `hangfoldedstart` | 26 | 30 |  |
| `HangFree` | 60 | 30 |  |
| `hangfree45left` | 1 | 30 |  |
| `hangfree45right` | 1 | 30 |  |
| `HangFreeCornerOutSideLeft` | 41 | 30 |  |
| `HangFreeCornerOutSideRight` | 41 | 30 |  |
| `HangFreeEnd` | 10 | 30 |  |
| `hangfreefoldedendhangfree` | 62 | 30 |  |
| `HangFreeHardStart` | 40 | 30 |  |
| `HangFreeHeaveOver` | 28 | 30 |  |
| `HangFreeHeaveUp` | 60 | 30 |  |
| `hangfreeheaveuptocrouch` | 60 | 30 |  |
| `hangfreejumppose` | 1 | 30 |  |
| `HangFreeStrafe` | 32 | 30 |  |
| `HangFreeStrafeLeft` | 32 | 30 |  |
| `hangfreetransferup` | 16 | 30 |  |
| `HangFreeTurnLeft` | 55 | 30 |  |
| `HangFreeTurnRight` | 55 | 30 |  |
| `HangHardStart` | 31 | 30 |  |
| `HangHardStart2` | 45 | 30 |  |
| `HangHardStart3` | 55 | 30 |  |
| `hanghardstartvertical` | 47 | 30 |  |
| `HangHeaveOver` | 30 | 30 |  |
| `HangHeaveUp` | 46 | 30 | ledge_climb_up |
| `HangHeaveUpToCrouch` | 45 | 30 |  |
| `HangStrafeLeft` | 32 | 30 |  |
| `HangStrafeRight` | 32 | 30 |  |
| `hangtransferup` | 16 | 30 |  |
| `hangtransferupidle` | 1 | 30 |  |
| `hangturnjump` | 45 | 30 |  |
| `HangTurnLeftEnd` | 17 | 30 |  |
| `HangTurnLeftIdle` | 60 | 30 |  |
| `hangturnleftidle02` | 60 | 30 |  |
| `HangTurnLeftStart` | 25 | 30 |  |
| `HangTurnRightEnd` | 17 | 30 |  |
| `HangTurnRightIdle` | 60 | 30 |  |
| `hangturnrightidle02` | 60 | 30 |  |
| `HangTurnRightStart` | 25 | 30 |  |
| `interactbutton` | 42 | 30 |  |
| `interactbuttonhigh` | 42 | 30 |  |
| `interactvalveend` | 51 | 30 |  |
| `interactvalveloop` | 40 | 30 |  |
| `interactvalvestart` | 81 | 30 |  |
| `jumpair` | 45 | 30 | fall |
| `jumpcoil` | 25 | 30 | coil |
| `jumpcoilend` | 18 | 30 |  |
| `jumpfast` | 45 | 30 | jump |
| `JumpLand` | 30 | 30 |  |
| `jumplandpose` | 15 | 30 |  |
| `JumpSlow` | 45 | 30 |  |
| `jumpstill` | 30 | 30 |  |
| `JumpTurnFly` | 18 | 30 | wallclimb_jump |
| `jumpturnflyend` | 60 | 30 |  |
| `jumpturnflyendleft` | 1 | 30 |  |
| `jumpturnflyendright` | 1 | 30 |  |
| `jumpturnlanding` | 34 | 30 |  |
| `jumpturnlandingaimcc` | 1 | 30 |  |
| `jumpturnlandingaimleft` | 1 | 30 |  |
| `jumpturnlandingaimright` | 1 | 30 |  |
| `jumpturnlandingidle` | 60 | 30 |  |
| `jumpturnlandingstand` | 40 | 30 |  |
| `LadderClimbDownFast` | 15 | 30 |  |
| `ladderclimbdownfast02` | 15 | 30 |  |
| `ladderclimbdownfast03` | 15 | 30 |  |
| `LadderClimbHangStart` | 30 | 30 |  |
| `LadderClimbHangStartLeft` | 30 | 30 |  |
| `LadderClimbHangStartRight` | 30 | 30 |  |
| `ladderclimblookleft` | 60 | 30 |  |
| `ladderclimblookright` | 60 | 30 |  |
| `LadderClimbUpLeftHand` | 10 | 30 |  |
| `LadderClimbUpLeftHandStill` | 60 | 30 |  |
| `LadderClimbUpRightHand` | 10 | 30 |  |
| `LadderClimbUpRightHandStill` | 60 | 30 |  |
| `LadderEnterBottom` | 30 | 30 |  |
| `LadderEnterTop` | 50 | 30 |  |
| `LadderExitBottom` | 1 | 30 |  |
| `LadderExitTopLeftHand` | 46 | 30 |  |
| `LadderExitTopRightHand` | 46 | 30 |  |
| `ledgeinto` | 20 | 30 |  |
| `ledgeleft` | 32 | 30 |  |
| `ledgeright` | 32 | 30 |  |
| `MarioMove` | 67 | 30 |  |
| `MeleeBentOverFinish` | 19 | 30 |  |
| `MeleeBentOverStart` | 23 | 30 |  |
| `MeleeCrouchHit` | 14 | 30 |  |
| `MeleeCrouchHitUppercut` | 31 | 30 |  |
| `MeleeCrouchStart` | 3 | 30 |  |
| `MeleeCrouchStartUpperCut` | 6 | 30 |  |
| `MeleeFromAbove` | 15 | 30 |  |
| `MeleeHit2Left` | 13 | 30 |  |
| `MeleeHit2Right` | 13 | 30 |  |
| `MeleeHitLeft` | 12 | 30 |  |
| `MeleeHitRight` | 12 | 30 |  |
| `MeleeHitShove` | 17 | 30 |  |
| `MeleeInAir` | 17 | 30 |  |
| `MeleeInAir2` | 25 | 30 |  |
| `MeleeInAirHit` | 16 | 30 |  |
| `MeleeInAirLand` | 20 | 30 |  |
| `MeleeInAirLand2` | 21 | 30 |  |
| `MeleeInAirStill` | 22 | 30 |  |
| `meleekickobject` | 26 | 30 |  |
| `MeleeMissed2Left` | 14 | 30 |  |
| `MeleeMissed2Right` | 14 | 30 |  |
| `MeleeMissedLeft` | 17 | 30 |  |
| `MeleeMissedRight` | 17 | 30 |  |
| `MeleeSlide` | 20 | 30 |  |
| `MeleeSlideHard` | 22 | 30 |  |
| `MeleeStart2Left` | 5 | 30 |  |
| `MeleeStart2Right` | 5 | 30 |  |
| `MeleeStartLeft` | 5 | 30 |  |
| `MeleeStartRight` | 5 | 30 |  |
| `MeleeStartShove` | 4 | 30 |  |
| `MeleeVaultOver` | 20 | 30 |  |
| `MeleeWallRunLeft` | 25 | 30 |  |
| `MeleeWallRunRight` | 23 | 30 |  |
| `notifierdummyidle` | 60 | 30 |  |
| `notifierdummyrun` | 60 | 30 |  |
| `notifierdummysprint` | 60 | 30 |  |
| `notifierdummywalk` | 60 | 30 |  |
| `PipeClimbDownFast` | 15 | 30 |  |
| `pipeclimbdownfast02` | 15 | 30 |  |
| `PipeClimbHangStart` | 30 | 30 |  |
| `PipeClimbHangStartHard` | 30 | 30 |  |
| `PipeClimbHangStartLeft` | 36 | 30 |  |
| `PipeClimbHangStartRight` | 36 | 30 |  |
| `PipeClimbStart` | 30 | 30 |  |
| `pipeclimbupfastlefthand` | 15 | 30 |  |
| `pipeclimbupfastrighthand` | 15 | 30 |  |
| `PipeClimbUpLeftHand` | 15 | 30 |  |
| `PipeClimbUpLeftHandStill` | 1 | 30 |  |
| `PipeClimbUpRightHand` | 15 | 30 |  |
| `PipeClimbUpRightHandStill` | 1 | 30 |  |
| `PipeExitBottom` | 8 | 30 |  |
| `pipeexittoplefthand` | 55 | 30 |  |
| `pipeexittoprighthand` | 55 | 30 |  |
| `runbwd` | 19 | 30 |  |
| `runbwdstiff` | 19 | 30 |  |
| `runfwd` | 20 | 30 | run |
| `runfwdstart` | 20 | 30 |  |
| `runfwdstiff` | 20 | 30 |  |
| `RunTurn180` | 31 | 30 | turn_180 |
| `SnatchFail` | 24 | 30 |  |
| `sneakbwd` | 24 | 30 |  |
| `sneakfwd` | 24 | 30 |  |
| `springboardfail` | 61 | 30 |  |
| `SpringBoardLeftLeg` | 57 | 30 |  |
| `SpringBoardRightLeg` | 57 | 30 |  |
| `SprintFwd` | 16 | 30 | sprint |
| `Stand` | 60 | 30 | idle |
| `standcc` | 1 | 30 |  |
| `standidle1` | 80 | 30 |  |
| `standidle2` | 140 | 30 |  |
| `standidle3` | 120 | 30 |  |
| `standlc` | 1 | 30 |  |
| `standrc` | 1 | 30 |  |
| `StandTurn180Right` | 24 | 30 |  |
| `StandTurn45Left` | 24 | 30 |  |
| `StandTurn45Right` | 24 | 30 |  |
| `StandTurn90Left` | 24 | 30 |  |
| `StandTurn90Right` | 24 | 30 |  |
| `stepup144` | 36 | 30 |  |
| `stepupleftleg48` | 11 | 30 |  |
| `stepupleftleg88` | 20 | 30 |  |
| `stepuprightleg48` | 11 | 30 |  |
| `stepuprightleg88` | 20 | 30 |  |
| `StumbleFwd` | 30 | 30 |  |
| `swing180` | 61 | 30 |  |
| `swingend` | 9 | 30 |  |
| `swinghardstart` | 14 | 30 |  |
| `swinghardstartwide` | 14 | 30 |  |
| `swingjumpoff` | 25 | 30 |  |
| `swingoff` | 30 | 30 |  |
| `swingposebackstraight` | 1 | 30 |  |
| `swingposebacktop` | 1 | 30 |  |
| `swingposefronttop` | 20 | 30 |  |
| `swingstrafe` | 37 | 30 |  |
| `Taunt` | 24 | 30 |  |
| `VaultOnto` | 24 | 30 |  |
| `VaultOntoHigh` | 35 | 30 |  |
| `vaultontohigh02` | 35 | 30 |  |
| `VaultOver` | 25 | 30 | vault |
| `VaultOverHigh` | 40 | 30 |  |
| `walkbalancefalloffleft` | 20 | 30 |  |
| `walkbalancefalloffright` | 20 | 30 |  |
| `walkbalancefwd` | 24 | 30 |  |
| `walkbalancefwdleanleft` | 24 | 30 |  |
| `walkbalancefwdleanright` | 24 | 30 |  |
| `walkbalancelosebalanceleft` | 48 | 30 |  |
| `walkbalancelosebalanceright` | 48 | 30 |  |
| `walkbalancestill` | 60 | 30 |  |
| `walkbalancestillleanleft` | 60 | 30 |  |
| `walkbalancestillleanright` | 60 | 30 |  |
| `walkbwd` | 30 | 30 |  |
| `walkbwdstiff` | 30 | 30 |  |
| `walkfwd` | 30 | 30 | walk |
| `walkfwdstiff` | 30 | 30 |  |
| `walktostandpassleft` | 20 | 30 |  |
| `walktostandpassright` | 20 | 30 |  |
| `wallrunimpactleft` | 10 | 30 |  |
| `wallrunimpactright` | 10 | 30 |  |
| `WallrunJumpLeft` | 45 | 30 | wallrun_jump |
| `wallrunjumpright` | 45 | 30 |  |
| `WallrunLeft` | 16 | 30 | wallrun_left |
| `wallrunleftstart` | 15 | 30 |  |
| `WallrunRight` | 16 | 30 | wallrun_right |
| `wallrunrightstart` | 15 | 30 |  |
| `WallRunVertical` | 14 | 30 | wallclimb |
| `wallrunvertical180turn` | 19 | 30 | wallclimb_turn |
| `wallrunverticaldodgeleft` | 12 | 30 |  |
| `wallrunverticaldodgeleftinair` | 1 | 30 |  |
| `wallrunverticaldodgeright` | 12 | 30 |  |
| `wallrunverticaldodgerightinair` | 1 | 30 |  |
| `wallrunverticalstart` | 9 | 30 |  |
| `ZipLine` | 120 | 30 |  |
| `ziplinehitwall` | 35 | 30 |  |
| `ziplineintohitwall` | 60 | 30 |  |
| `ziplinestart` | 37 | 30 |  |

## Asignación a `FaithAnimState`

| Secuencia | Animación de ME |
|---|---|
| `idle` | `Stand` |
| `walk` | `walkfwd` |
| `run` | `runfwd` |
| `sprint` | `SprintFwd` |
| `crouch` | `crouchstill` |
| `crouch_walk` | `crouchfwd` |
| `jump` | `jumpfast` |
| `fall` | `jumpair` |
| `fall_uncontrolled` | `fallinguncontrolled` |
| `coil` | `jumpcoil` |
| `land_soft` | `fallinglandsoftlanding` |
| `land_hard` | `fallinglandhard` |
| `skill_roll` | `fallinglandroll` |
| `slide` | `CrouchSlide` |
| `wallrun_left` | `WallrunLeft` |
| `wallrun_right` | `WallrunRight` |
| `wallrun_jump` | `WallrunJumpLeft` |
| `wallclimb` | `WallRunVertical` |
| `wallclimb_turn` | `wallrunvertical180turn` |
| `wallclimb_jump` | `JumpTurnFly` |
| `ledge_hang` | `Hang` |
| `ledge_climb_up` | `HangHeaveUp` |
| `vault` | `VaultOver` |
| `turn_180` | `RunTurn180` |

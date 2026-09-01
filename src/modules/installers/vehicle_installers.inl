void TryDisableSwingingCompletely() {
    __try {
        if (NearlyEqual(*reinterpret_cast<const float*>(kDoorApplyRateChassis),
                        kStockDoorApplyRateChassis)
            && WriteProtectedGameFloat(kDoorApplyRateChassis, 0.0f)) {
            g_swingingDisabled = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool InstallDoorSwingFix() {
    PatchSet patches("Door swing fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
        size_t size;
    };
    const std::array<Site, 4> sites{{
        {kDoorForceChassis, &DoorForceChassisThunk,
         kExpectedDoorForceChassis.data(), kExpectedDoorForceChassis.size()},
        {kDoorDampingFiretruck, &DoorDampingFiretruckThunk,
         kExpectedDoorDampingFiretruck.data(),
         kExpectedDoorDampingFiretruck.size()},
        {kDoorDampingOther, &DoorDampingOtherThunk,
         kExpectedDoorDampingOther.data(), kExpectedDoorDampingOther.size()},
        {kDoorIntegration, &DoorIntegrationThunk,
         kExpectedDoorIntegration.data(), kExpectedDoorIntegration.size()},
    }};

    for (size_t i = 0; i < sites.size(); ++i) {
        const Site& site = sites[i];
        if (!patches.Track(
                InstallBranch(g_doorSwingPatches[i], site.address, site.thunk,
                              site.expected, site.size, 0xE9),
                g_doorSwingPatches[i])) {
            Log("Door swing fix skipped: CDoor::Process bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }

    if (ReadSetting("vehicles", "disableSwingingCompletely", false)) {
        TryDisableSwingingCompletely();
    }

    patches.Commit();
    Log(g_swingingDisabled
            ? "Installed timestep-scaled door and firetruck ladder physics "
              "with chassis sway disabled."
            : "Installed timestep-scaled door, swinging chassis and "
              "firetruck ladder physics.");
    return true;
}

bool InstallWheelSpinFix() {
    PatchSet patches("Free wheel spin fix");
    struct Site {
        SitePatch* patch;
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {&g_wheelSpinPatches[0], kWheelSpinDecelLeft, &WheelSpinDecelThunk,
         kExpectedWheelSpinDecel.data()},
        {&g_wheelSpinPatches[1], kWheelSpinDecelRight, &WheelSpinDecelThunk,
         kExpectedWheelSpinDecel.data()},
        {&g_wheelSpinPatches[2], kWheelSpinAccelLeft, &WheelSpinAccelThunk,
         kExpectedWheelSpinAccel.data()},
        {&g_wheelSpinPatches[3], kWheelSpinAccelRight, &WheelSpinAccelThunk,
         kExpectedWheelSpinAccel.data()},
        {&g_wheelSpinPatches[4], kWheelSpinDampLeft, &WheelSpinDampThunk,
         kExpectedWheelSpinDamp.data()},
        {&g_wheelSpinPatches[5], kWheelSpinDampRight, &WheelSpinDampThunk,
         kExpectedWheelSpinDamp.data()}
    };

    for (const auto& site : sites) {
        if (!patches.Track(
                InstallBranch(*site.patch, site.address, site.thunk,
                              site.expected, 6, 0xE8),
                *site.patch)) {
            Log("Free wheel spin fix skipped: CAutomobile::ProcessCarWheelPair "
                "bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled free wheel spin rate.");
    return true;
}

bool InstallSwimPitchRateFix() {
    PatchSet patches("Swim pitch rate fix");
    const uintptr_t sites[] = {kSwimPitchDecayA, kSwimPitchDecayB,
                               kSwimPitchDecayC};
    for (size_t i = 0; i < g_swimPitchPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_swimPitchPatches[i], sites[i],
                              &WheelSpinDampThunk,
                              kExpectedWheelSpinDamp.data(), 6, 0xE8),
                g_swimPitchPatches[i])) {
            Log("Swim pitch rate fix skipped: "
                "CTaskSimpleSwim::ProcessSwimmingResistance bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled swim pitch rate.");
    return true;
}

bool InstallDrunkSteerDelayFix() {
    if (!InstallBranch(g_drunkSteerPatch, kDrunkSteerShift,
                       &DrunkSteerShiftThunk, kExpectedDrunkSteerShift.data(),
                       8, 0xE9)) {
        Log("Drunk steering delay fix skipped: CPad::Update bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    ResetFrameTicks();
    Log("Installed a time-based drunk driving steering delay.");
    return true;
}

bool InstallFatCounterFix() {
    if (!InstallBranch(g_fatCounterPatch, kFatCounterMath, &FatCounterThunk,
                       kExpectedFatCounterMath.data(), 29, 0xE8)) {
        Log("Fat counter fix skipped: CStats::UpdateFatAndMuscleStats bytes do "
            "not match GTA SA 1.0 US.");
        return false;
    }
    g_fatCounterCarry = 0.0f;
    Log("Installed a fat counter that carries the discarded remainder.");
    return true;
}

bool InstallFireSpreadFix() {
    PatchSet patches("Fire spread fix");
    struct Site {
        SitePatch* patch;
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {&g_fireGatePatches[0], kFireVehicleGate, &FireVehicleGateThunk,
         kExpectedFireVehicleGate.data()},
        {&g_fireGatePatches[1], kFireSpreadGate, &FireSpreadGateThunk,
         kExpectedFireSpreadGate.data()},
        {&g_fireGatePatches[2], kFireMergeGate, &FireMergeGateThunk,
         kExpectedFireMergeGate.data()}
    };

    for (const auto& site : sites) {
        if (!patches.Track(
                InstallBranch(*site.patch, site.address, site.thunk,
                              site.expected, 8, 0xE9),
                *site.patch)) {
            Log("Fire spread fix skipped: CFire::ProcessFire bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    g_fireEventCarries.fill(0.0f);
    patches.Commit();
    Log("Installed frame-rate independent fire event rates.");
    return true;
}

bool InstallMapZoomWheelFix() {
    PatchSet patches("Map zoom wheel fix");
    if (!patches.Track(
            InstallBranch(g_mapWheelSamplePatch, kMapWheelSample,
                          &MapWheelSampleThunk, kExpectedMapWheelSample.data(),
                          6, 0xE8),
            g_mapWheelSamplePatch)) {
        Log("Map zoom wheel fix skipped: the map bounds block does not match "
            "GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallBranch(g_mapZoomInGatePatch, kMapZoomInGate,
                          &MapZoomInGateThunk, kExpectedMapZoomInGate.data(), 9,
                          0xE9),
            g_mapZoomInGatePatch)
        || !patches.Track(
            InstallBranch(g_mapZoomOutGatePatch, kMapZoomOutGate,
                          &MapZoomOutGateThunk, kExpectedMapZoomOutGate.data(),
                          5, 0xE9),
            g_mapZoomOutGatePatch)) {
        Log("Map zoom wheel fix skipped: the map zoom gates do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed a frame-rate independent pause menu map zoom.");
    return true;
}

bool InstallBmxSprintLeanFix() {
    if (!InstallBranch(g_bmxSprintLeanPatch, kBmxSprintLeanDecay,
                       &WheelSpinDampThunk, kExpectedWheelSpinDamp.data(), 6,
                       0xE8)) {
        Log("BMX sprint lean fix skipped: CBmx::ProcessControl bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled BMX sprint lean decay.");
    return true;
}


bool InstallBikeWheelSpinFix() {
    PatchSet patches("Bike wheel spin fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kBikeWheelSpinDampA, &WheelSpinDampThunk,
         kExpectedWheelSpinDamp.data()},
        {kBikeWheelSpinDampB, &WheelSpinDampThunk,
         kExpectedWheelSpinDamp.data()},
        {kBikeWheelPitchIntegrate, &BikeWheelPitchIntegrateThunk,
         kExpectedBikeWheelPitchIntegrate.data()},
        {kBikeRearWheelSpeedDecel, &WheelSpinDecelThunk,
         kExpectedWheelSpinDecel.data()},
        {kBikeRearWheelSpeedAccel, &WheelSpinAccelThunk,
         kExpectedWheelSpinAccel.data()},
    };
    for (size_t i = 0; i < g_bikeWheelSpinPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_bikeWheelSpinPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_bikeWheelSpinPatches[i])) {
            Log("Bike wheel spin fix skipped: CBike::ProcessControl bytes do "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled bike front wheel spin.");
    return true;
}

bool InstallJetPackFxRampFix() {
    PatchSet patches("Jetpack flame ramp fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kJetPackFxRampUp, &JetPackFxRampUpThunk,
         kExpectedJetPackRampUp.data()},
        {kJetPackFxRampDown, &JetPackFxRampDownThunk,
         kExpectedJetPackRampDown.data()},
    };
    for (size_t i = 0; i < g_jetPackFxPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_jetPackFxPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_jetPackFxPatches[i])) {
            Log("Jetpack flame ramp fix skipped: "
                "CTaskSimpleJetPack::DoJetPackEffect bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a time-based jetpack flame ramp.");
    return true;
}

bool InstallHeadBoppingFix() {
    PatchSet patches("Head bopping fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kHeadBopRampUp, &HeadBopRampUpThunk, kExpectedHeadBopRampUp.data()},
        {kHeadBopRampDown, &HeadBopRampDownThunk,
         kExpectedHeadBopRampDown.data()},
    };
    for (size_t i = 0; i < g_headBopPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_headBopPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_headBopPatches[i])) {
            Log("Head bopping fix skipped: "
                "CTaskSimpleCarDrive::ProcessHeadBopping bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a time-based driver head bop ramp.");
    return true;
}

bool InstallWheelSettleFix() {
    PatchSet patches("Wheel settle fix");
    const uintptr_t sites[] = {
        kWheelSettleBikeA, kWheelSettleBikeB,
        kWheelSettleBmxA, kWheelSettleBmxB,
        kWheelSettleHeli, kWheelSettlePlane,
    };
    for (size_t i = 0; i < g_wheelSettlePatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_wheelSettlePatches[i], sites[i],
                              &WheelSettleThunk, kExpectedWheelSettle.data(), 6,
                              0xE8),
                g_wheelSettlePatches[i])) {
            Log("Wheel settle fix skipped: bike or aircraft PreRender bytes do not "
                "match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a real-time settle for drawn bike and aircraft wheels; "
        "automobile wheel travel remains stock.");
    return true;
}

bool InstallCollisionPushOutFix() {
    PatchSet patches("Collision push-out fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kPushOutScaleA, &PushOutMainThunk, kExpectedPushOutMain.data()},
        {kPushOutScaleB, &PushOutMainThunk, kExpectedPushOutMain.data()},
        {kPushOutScaleC, &PushOutMainThunk, kExpectedPushOutMain.data()},
        {kPushOutScaleD, &PushOutAltThunk, kExpectedPushOutAlt.data()},
        {kPushOutScaleE, &PushOutAltThunk, kExpectedPushOutAlt.data()},
        {kPushOutScaleF, &PushOutAltThunk, kExpectedPushOutAlt.data()},
    };
    for (size_t i = 0; i < g_pushOutPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_pushOutPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_pushOutPatches[i])) {
            Log("Collision push-out fix skipped: "
                "CPhysical::ProcessShiftSectorList bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled collision push-out.");
    return true;
}

bool InstallBmxLeanSettleFix() {
    PatchSet patches("BMX lean settle fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kBmxLeanLeftDecayA, &BmxLeanLeftDecayThunk,
         kExpectedBmxLeanLeftDecay.data()},
        {kBmxLeanFwdDecayA, &BmxLeanFwdDecayThunk,
         kExpectedBmxLeanFwdDecay.data()},
        {kBmxLeanLeftDecayB, &BmxLeanLeftDecayThunk,
         kExpectedBmxLeanLeftDecay.data()},
        {kBmxLeanFwdDecayB, &BmxLeanFwdDecayThunk,
         kExpectedBmxLeanFwdDecay.data()},
    };
    for (size_t i = 0; i < g_bmxLeanPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_bmxLeanPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_bmxLeanPatches[i])) {
            Log("BMX lean settle fix skipped: CBmx::ProcessDrivingAnims bytes "
                "do not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled BMX rider lean settle.");
    return true;
}
bool InstallJumpOutCarSpeedFix() {
    PatchSet patches("Jump out car speed fix");
    const uintptr_t sites[] = {kJumpOutTurnDampX, kJumpOutTurnDampY,
                               kJumpOutTurnDampZ, kJumpOutMoveDampX,
                               kJumpOutMoveDampY, kJumpOutMoveDampZ};
    for (size_t i = 0; i < g_jumpOutDampPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_jumpOutDampPatches[i], sites[i],
                              &JumpOutDampThunk, kExpectedJumpOutDamp.data(), 6,
                              0xE8),
                g_jumpOutDampPatches[i])) {
            Log("Jump out car speed fix skipped: CVehicle::CanPedJumpOutCar "
                "bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled jump out car damping.");
    return true;
}

bool InstallBoatEngineSpeedFix() {
    if (!InstallBranch(g_boatEngineDampingPatch, kBoatEngineDamping,
                       &WheelSpinDampThunk, kExpectedWheelSpinDamp.data(), 6,
                       0xE8)) {
        Log("Boat engine speed fix skipped: CBoat::ProcessControl bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled boat engine coast down.");
    return true;
}

bool InstallSuspensionDampingLimitFix() {
    __try {
        if (!NearlyEqual(*reinterpret_cast<const float*>(kDampingLimitInFrame),
                         kStockDampingLimitInFrame)) {
            Log("Suspension damping limit fix skipped: the limit does not match "
                "GTA SA 1.0 US.");
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("Suspension damping limit fix skipped: the limit is unreadable.");
        return false;
    }

    if (!InstallDetour(g_suspensionDampingPatch, kApplySpringDampening,
                       &HookedSpringDampening,
                       kExpectedApplySpringDampening.data(),
                       kExpectedApplySpringDampening.size())) {
        Log("Suspension damping fix skipped: ApplySpringDampening entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed exact real-time suspension damping with a stable cap.");
    return true;
}

bool InstallRollOntoWheelsFix() {
    PatchSet patches("Roll onto wheels fix");
    if (!patches.Track(
            RepointCall(g_rollOntoWheelsTurnPatch,
                        kRollOntoWheelsTurnForce, kApplyTurnForce,
                        &RollOntoWheelsTurnForceThunk),
            g_rollOntoWheelsTurnPatch)) {
        Log("Roll onto wheels fix skipped: CAutomobile::ProcessSuspension turn "
            "force bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            RepointCall(g_rollOntoWheelsMovePatch,
                        kRollOntoWheelsMoveForce, kApplyMoveForce,
                        &RollOntoWheelsMoveForceThunk),
            g_rollOntoWheelsMovePatch)) {
        Log("Roll onto wheels fix skipped: the move force site does not match.");
        return false;
    }
    patches.Commit();
    Log("Installed a timestep-scaled roll onto wheels assist.");
    return true;
}

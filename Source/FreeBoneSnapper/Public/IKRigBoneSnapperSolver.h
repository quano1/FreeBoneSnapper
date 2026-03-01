// Copyright longlt00502@gmail.com 2023-2025. All rights reserved.

#pragma once

#include "RigEditor/IKRigStructViewer.h"

#include "Rig/Solvers/IKRigSolverBase.h"
#include "FreeBoneSnapper.h"
#include "IKRigBoneSnapperSolver.generated.h"

UENUM(BlueprintType)
enum class ESnapMode : uint8 {
    Translation,
    Rotation,
    Scale,
};

USTRUCT(BlueprintType, meta=(UIWrapper="/Script/FreeBoneSnapper.BoneSnapperBoneSettingsWrapper"))
struct FIKRigBoneSnapperBoneSettings final : public FIKRigBoneSettingsBase
{
    GENERATED_BODY()

public:

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Settings")
    FName SourceBone = NAME_None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
    FName DestinationBone = NAME_None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (Bitmask, BitmaskEnum="/Script/FreeBoneSnapper.ESnapMode"))
    uint8 SnapMode = 7u;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
    FTransform Offset;

    inline bool IsSet(ESnapMode Mode) const
    {
        return (SnapMode & (1u << StaticCast<uint8>(Mode))) != 0;
    }
};

UCLASS()
class UBoneSnapperBoneSettingsWrapper : public UIKRigStructWrapperBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Bone Snapper Settings")
    FIKRigBoneSnapperBoneSettings Settings;
};

USTRUCT(BlueprintType, meta=(UIWrapper="/Script/FreeBoneSnapper.BoneSnapperSettingsWrapper"))
struct FIKRigBoneSnapperSettings final : public FIKRigSolverSettingsBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Bone Snapper Settings")
    FName RootBone;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Root")
    bool InPlace = false;

};

UCLASS()
class UBoneSnapperSettingsWrapper : public UIKRigStructWrapperBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Bone Snapper Settings")
	FIKRigBoneSnapperSettings Settings;
};

UCLASS(MinimalAPI, BlueprintType)
class UIKRigBoneSnapperSolverController final : public UIKRigSolverControllerBase
{
    GENERATED_BODY()

public:

};

USTRUCT(BlueprintType)
struct FIKRigBoneSnapperSolver final : public FIKRigSolverBase
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FIKRigBoneSnapperBoneSettings> BoneSettings;

    UPROPERTY()
    FIKRigBoneSnapperSettings SolverSettings;

    // runtime FIKRigSolverBase interface
    void Initialize(const FIKRigSkeleton& IKRigSkeleton) override;
    void Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& InGoals) override;
    inline void GetRequiredBones(TSet<FName>& OutRequiredBones) const override {}
    inline void GetRequiredGoals(TSet<FName>& OutRequiredGoals) const override {}

    // solver settings
    inline FIKRigSolverSettingsBase* GetSolverSettings() override { return &SolverSettings; }
    inline const UScriptStruct* GetSolverSettingsType() const override { return FIKRigBoneSnapperSettings::StaticStruct(); }

    // goal
    void AddGoal(const UIKRigEffectorGoal& InNewGoal) override {};
    void OnGoalRenamed(const FName& InOldName, const FName& InNewName) override {};
    void OnGoalMovedToDifferentBone(const FName& InGoalName, const FName& InNewBoneName) override {};
    void OnGoalRemoved(const FName& InGoalName) override {};

    // start bone
    bool UsesStartBone() const override;
    FName GetStartBone() const override;
    void SetStartBone(const FName& InRootBoneName) override;

    // BONE SETTINGS
    inline bool UsesCustomBoneSettings() const override { return true; };
    void AddSettingsToBone(const FName& InBoneName) override;
    void RemoveSettingsOnBone(const FName& InBoneName) override;
    FIKRigBoneSettingsBase *GetBoneSettings(const FName& InBoneName) override;
    void GetBonesWithSettings(TSet<FName>& OutBonesWithSettings) const override;
    const UScriptStruct* GetBoneSettingsType() const override;
    bool HasSettingsOnBone(const FName& InBoneName) const override;

#if WITH_EDITOR
    // UI
    FText GetNiceName() const override;
    bool GetWarningMessage(FText& OutWarningMessage) const override;
    bool IsBoneAffectedBySolver(const FName& InBoneName, const FIKRigSkeleton& InIKRigSkeleton) const override;

    inline UIKRigSolverControllerBase* GetSolverController(UObject* Outer) override { return CreateControllerIfNeeded(Outer, UIKRigBoneSnapperSolverController::StaticClass()); }
#endif

private:

    TArray<int32> BoneDepths;
    bool Dirty=true;

    friend struct FIKRigBoneSnapperBoneSettings;
};


// Copyright longlt00502@gmail.com 2023. All rights reserved.

#include "IKRig_BoneSnapperSolver.h"

#include "Misc/TransactionObjectEvent.h"

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION <= 2
#include "IKRigDataTypes.h"
#include "IKRigSkeleton.h"
#else
#include "Rig/IKRigDataTypes.h"
#include "Rig/IKRigSkeleton.h"
#endif
#endif


#include "SlateBasics.h"
#include "PropertyHandle.h"
#include "SSearchableComboBox.h"

// #include "tll/log.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRig_BoneSnapperSolver)

#define LOCTEXT_NAMESPACE "UIKRig_BoneSnapperSolver"

void UIKRig_BoneSnapperSolver::Initialize(const FIKRigSkeleton& IKRigSkeleton)
{
	BoneDepths.SetNum(0);

	for (int32 BoneIndex=0; BoneIndex<IKRigSkeleton.BoneNames.Num(); ++BoneIndex)
	{
		auto ParentIndex = IKRigSkeleton.ParentIndices[BoneIndex];
		int32 Depth = 0;

		while(ParentIndex != INDEX_NONE)
		{
			Depth++;
			ParentIndex = IKRigSkeleton.ParentIndices[ParentIndex];
		}

		BoneDepths.Add(Depth);
	}
}

void UIKRig_BoneSnapperSolver::Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& Goals)
{
	if(Dirty)
	{
		BoneSettings.Sort([this, &IKRigSkeleton](const UIKRig_FBoneSnapperSettings& A, const UIKRig_FBoneSnapperSettings& B) {
			auto AId = IKRigSkeleton.GetBoneIndexFromName(A.SourceBone);
			auto BId = IKRigSkeleton.GetBoneIndexFromName(B.SourceBone);

			if(AId == INDEX_NONE || BId == INDEX_NONE)
			{
				return AId == INDEX_NONE;
			}

			int32 DepthA = BoneDepths[AId];
			int32 DepthB = BoneDepths[BId];
			return DepthA < DepthB;
		});

		for(auto const &Settings : BoneSettings)
		{
			UE_LOG(FreeBoneSnapper, Verbose, TEXT("Snap %s to %s"), *Settings->SourceBone.ToString(), *Settings->DestinationBone.ToString());
		}

		Dirty = false;
	}

	auto &CurrentPoseLocal = IKRigSkeleton.CurrentPoseLocal;
	auto &CurrentPoseGlobal = IKRigSkeleton.CurrentPoseGlobal;
	auto &ParentIndices = IKRigSkeleton.ParentIndices;

	bool IsDirty = false;

	if(RootSnapBoneName != NAME_None)
	{
		auto RootSnapBoneId = IKRigSkeleton.GetBoneIndexFromName(RootSnapBoneName);


		if(InPlace)
		{
			CurrentPoseLocal[RootSnapBoneId].SetTranslation(FVector(0, 0, CurrentPoseLocal[RootSnapBoneId].GetTranslation().Z));
			IsDirty = true;
			for (int32 BoneIndex=1; BoneIndex<CurrentPoseGlobal.Num(); ++BoneIndex)
			{
				const int32 ParentIndex = ParentIndices[BoneIndex];
				if (ParentIndex == INDEX_NONE)
				{
					// root always in global space already, no conversion required
					CurrentPoseGlobal[BoneIndex] = CurrentPoseLocal[BoneIndex];
					continue; 
				}
				const FTransform& ChildLocalTransform = CurrentPoseLocal[BoneIndex];
				const FTransform& ParentGlobalTransform = CurrentPoseGlobal[ParentIndex];
				CurrentPoseGlobal[BoneIndex] = ChildLocalTransform * ParentGlobalTransform;
				CurrentPoseGlobal[BoneIndex].NormalizeRotation();
			}
		}
		else
		{
			auto RootDelta = CurrentPoseGlobal[RootSnapBoneId];

			if(!RootDelta.GetTranslation().Equals(FVector::Zero(), 1e-4))
			{
				RootDelta.SetTranslation(FVector(RootDelta.GetTranslation().X, RootDelta.GetTranslation().Y, 0));
				RootDelta.SetRotation(FQuat::Identity);
				RootDelta.SetScale3D(FVector::One());

				CurrentPoseGlobal[0] = RootDelta;

				auto RootDeltaInversed = RootDelta.Inverse();

				for(int32 BoneId=1; BoneId < CurrentPoseGlobal.Num(); BoneId++)
				{
					if(ParentIndices[BoneId] == 0)
					{
						CurrentPoseLocal[BoneId] = RootDeltaInversed * CurrentPoseLocal[BoneId];
					}
				}
			}
		}
	}

	for(auto const &Chain: BoneSettings)
	{
		auto SrcId = IKRigSkeleton.GetBoneIndexFromName(Chain->SourceBone);
		auto DstId = IKRigSkeleton.GetBoneIndexFromName(Chain->DestinationBone);

		if(SrcId == INDEX_NONE || DstId == INDEX_NONE)
		{
			continue;
		}

		if(Chain->SnapMode == 0)
		{
			continue;
		}

		auto const &DstGlobalTransform = CurrentPoseGlobal[DstId];
		auto const &SrcGlobalTransform = CurrentPoseGlobal[SrcId];

		auto Delta = DstGlobalTransform.GetRelativeTransform(SrcGlobalTransform);

		if(!Chain->IsSet(ESnapMode::Translation))
		{
			Delta.SetTranslation(FVector::ZeroVector);
		}

		if(!Chain->IsSet(ESnapMode::Rotation))
		{
			Delta.SetRotation(FQuat::Identity);
		}

		if(!Chain->IsSet(ESnapMode::Scale))
		{
			Delta.SetScale3D(FVector::OneVector);
		}

		Delta = Chain->Offset * Delta;

		CurrentPoseLocal[SrcId] = Delta * CurrentPoseLocal[SrcId];

		for (int32 BoneIndex=SrcId; BoneIndex<CurrentPoseGlobal.Num(); ++BoneIndex)
		{
			const int32 ParentIndex = ParentIndices[BoneIndex];
			if (ParentIndex == INDEX_NONE)
			{
				// root always in global space already, no conversion required
				CurrentPoseGlobal[BoneIndex] = CurrentPoseLocal[BoneIndex];
				continue; 
			}
			const FTransform& ChildLocalTransform = CurrentPoseLocal[BoneIndex];
			const FTransform& ParentGlobalTransform = CurrentPoseGlobal[ParentIndex];
			CurrentPoseGlobal[BoneIndex] = ChildLocalTransform * ParentGlobalTransform;
			CurrentPoseGlobal[BoneIndex].NormalizeRotation();
		}
	}
}

#if WITH_EDITOR

FText UIKRig_BoneSnapperSolver::GetNiceName() const
{
	return FText(LOCTEXT("SolverName", "Bone Snapper"));
}

bool UIKRig_BoneSnapperSolver::GetWarningMessage(FText& OutWarningMessage) const
{ 
	if(BoneSettings.IsEmpty() && RootSnapBoneName == NAME_None)
	{
		OutWarningMessage = FText(LOCTEXT("MissingData", "Missing Data"));
		return true;
	}

	return false;
};

bool UIKRig_BoneSnapperSolver::IsBoneAffectedBySolver(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton) const
{
	for(auto const &Chain : BoneSettings)
	{
		if(Chain->SourceBone == BoneName)
		{
			return true;
		}
	}

	return false;
}

void UIKRig_BoneSnapperSolver::AddBoneSetting(const FName& BoneName)
{
	if (GetBoneSetting(BoneName))
	{
		return; // already have settings on this bone
	}

	UIKRig_FBoneSnapperSettings* NewBoneSettings = NewObject<UIKRig_FBoneSnapperSettings>(this, UIKRig_FBoneSnapperSettings::StaticClass());
	NewBoneSettings->SourceBone = BoneName;
	NewBoneSettings->Solver = this;
	BoneSettings.Add(NewBoneSettings);
}

void UIKRig_BoneSnapperSolver::RemoveBoneSetting(const FName& BoneName)
{
	UIKRig_FBoneSnapperSettings* BoneSettingToRemove = nullptr; 
	for (UIKRig_FBoneSnapperSettings* BoneSetting : BoneSettings)
	{
		if (BoneSetting->SourceBone == BoneName)
		{
			BoneSettingToRemove = BoneSetting;
			break; // can only be one with this name
		}
	}

	if (BoneSettingToRemove)
	{
		BoneSettings.Remove(BoneSettingToRemove);
	}
}

UObject* UIKRig_BoneSnapperSolver::GetBoneSetting(const FName& BoneName) const
{
	for (UIKRig_FBoneSnapperSettings* BoneSetting : BoneSettings)
	{
		if (BoneSetting && BoneSetting->SourceBone == BoneName)
		{
			return BoneSetting;
		}
	}
	
	return nullptr;
}

void UIKRig_FBoneSnapperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.GetPropertyName() == TEXT("DestinationBone"))
		Solver->Dirty = true;
}

#endif

#undef LOCTEXT_NAMESPACE


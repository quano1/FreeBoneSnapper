// Copyright longlt00502@gmail.com 2023-2025. All rights reserved.

#include "IKRigBoneSnapperSolver.h"

#include "Misc/TransactionObjectEvent.h"

#include "Rig/IKRigDataTypes.h"
#include "Rig/IKRigSkeleton.h"

#include "SlateBasics.h"
#include "PropertyHandle.h"
#include "SSearchableComboBox.h"

#include "RetargetEditor/IKRetargeterController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRigBoneSnapperSolver)

#define LOCTEXT_NAMESPACE "FIKRigBoneSnapperSolver"

void FIKRigBoneSnapperSolver::Initialize(const FIKRigSkeleton& IKRigSkeleton)
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

void FIKRigBoneSnapperSolver::Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& Goals)
{
	if(Dirty)
	{
		BoneSettings.Sort([this, &IKRigSkeleton](const FIKRigBoneSnapperBoneSettings& A, const FIKRigBoneSnapperBoneSettings& B) {
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
			UE_LOG(FreeBoneSnapper, Verbose, TEXT("Snap %s to %s"), *Settings.SourceBone.ToString(), *Settings.DestinationBone.ToString());
		}

		Dirty = false;
	}

	auto &CurrentPoseLocal = IKRigSkeleton.CurrentPoseLocal;
	auto &CurrentPoseGlobal = IKRigSkeleton.CurrentPoseGlobal;
	auto &ParentIndices = IKRigSkeleton.ParentIndices;

	bool IsDirty = false;

	if(SolverSettings.RootBone != NAME_None)
	{
		auto RootSnapBoneId = IKRigSkeleton.GetBoneIndexFromName(SolverSettings.RootBone);


		if(SolverSettings.InPlace)
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
		auto SrcId = IKRigSkeleton.GetBoneIndexFromName(Chain.SourceBone);
		auto DstId = IKRigSkeleton.GetBoneIndexFromName(Chain.DestinationBone);

		if(SrcId == INDEX_NONE || DstId == INDEX_NONE)
		{
			continue;
		}

		if(Chain.SnapMode == 0)
		{
			continue;
		}

		auto const &DstGlobalTransform = CurrentPoseGlobal[DstId];
		auto const &SrcGlobalTransform = CurrentPoseGlobal[SrcId];

		auto Delta = DstGlobalTransform.GetRelativeTransform(SrcGlobalTransform);

		if(!Chain.IsSet(ESnapMode::Translation))
		{
			Delta.SetTranslation(FVector::ZeroVector);
		}

		if(!Chain.IsSet(ESnapMode::Rotation))
		{
			Delta.SetRotation(FQuat::Identity);
		}

		if(!Chain.IsSet(ESnapMode::Scale))
		{
			Delta.SetScale3D(FVector::OneVector);
		}

		Delta = Chain.Offset * Delta;

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

FText FIKRigBoneSnapperSolver::GetNiceName() const
{
	return FText(LOCTEXT("SolverName", "Bone Snapper"));
}

bool FIKRigBoneSnapperSolver::GetWarningMessage(FText& OutWarningMessage) const
{
	if(BoneSettings.IsEmpty() && SolverSettings.RootBone == NAME_None)
	{
		OutWarningMessage = FText(LOCTEXT("MissingData", "Missing Data"));
		return true;
	}

	return false;
};

bool FIKRigBoneSnapperSolver::IsBoneAffectedBySolver(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton) const
{
	for(auto const &Chain : BoneSettings)
	{
		if(Chain.SourceBone == BoneName)
		{
			return true;
		}
	}

	return false;
}

#endif

void FIKRigBoneSnapperSolver::AddSettingsToBone(const FName& BoneName)
{
	if (GetBoneSettings(BoneName))
	{
		return; // already have settings on this bone
	}

	auto &NewBoneSettings = BoneSettings.Add_GetRef(FIKRigBoneSnapperBoneSettings{});
	NewBoneSettings.SourceBone = BoneName;
}

void FIKRigBoneSnapperSolver::RemoveSettingsOnBone(const FName& BoneName)
{
	FIKRigBoneSnapperBoneSettings const *BoneSettingToRemove = nullptr;

	BoneSettings.RemoveAll([&](FIKRigBoneSnapperBoneSettings const &BoneSetting)
	{
		return BoneSetting.SourceBone == BoneName;
	});
}

FIKRigBoneSettingsBase *FIKRigBoneSnapperSolver::GetBoneSettings(const FName& BoneName)
{
	for (FIKRigBoneSnapperBoneSettings &BoneSetting : BoneSettings)
	{
		if (BoneSetting.SourceBone == BoneName)
		{
			return &BoneSetting;
		}
	}

	return nullptr;
}

void FIKRigBoneSnapperSolver::GetBonesWithSettings(TSet<FName>& OutBonesWithSettings) const
{
	for (const FIKRigBoneSnapperBoneSettings& BoneSetting : BoneSettings)
	{
		OutBonesWithSettings.Add(BoneSetting.SourceBone);
	}
}

const UScriptStruct* FIKRigBoneSnapperSolver::GetBoneSettingsType() const
{
	return FIKRigBoneSnapperBoneSettings::StaticStruct();
}

bool FIKRigBoneSnapperSolver::UsesStartBone() const { return true; };
FName FIKRigBoneSnapperSolver::GetStartBone() const { return SolverSettings.RootBone; };
void FIKRigBoneSnapperSolver::SetStartBone(const FName& InRootBoneName) { SolverSettings.RootBone = InRootBoneName; };

bool FIKRigBoneSnapperSolver::HasSettingsOnBone(const FName& InBoneName) const
{
	for (auto &BoneSetting : BoneSettings)
	{
		if (BoneSetting.SourceBone == InBoneName || BoneSetting.DestinationBone == InBoneName)
		{
			return true;
		}
	}

	return false;
}

/// RIDICULOUS!

// ********** Begin Class UIKRigStructViewer ****************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UIKRigStructViewer;
UClass* UIKRigStructViewer::GetPrivateStaticClass()
{
	using TClass = UIKRigStructViewer;
	if (!Z_Registration_Info_UClass_UIKRigStructViewer.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("IKRigBoneSnapperSolverController"),
			Z_Registration_Info_UClass_UIKRigStructViewer.InnerSingleton,
			StaticRegisterNativesUIKRigStructViewer,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_UIKRigStructViewer.InnerSingleton;
}

void UIKRigStructViewer::StaticRegisterNativesUIKRigStructViewer()
{
}

UIKRigStructViewer::UIKRigStructViewer(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UIKRigStructViewer);
UIKRigStructViewer::~UIKRigStructViewer() {}
// ********** End Class UIKRigStructViewer ******************************************

USkeleton* UIKRigStructViewer::GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle)
{
	if (!StructToView.IsValid())
	{
		return nullptr;
	}

	// NOTE: it's not ideal that we are hardcoding supported types here, but because UStruct's do not support multiple
	// inheritance we cannot use an interface to identify skeleton providers as we normally would.
	if (StructToView.Type->IsChildOf(FIKRetargetOpSettingsBase::StaticStruct()))
	{
		FName PropertyName = PropertyHandle->GetProperty()->GetFName();
		uint8* StructMemory = StructToView.MemoryProvider();
		FIKRetargetOpSettingsBase* SkeletonProvider = reinterpret_cast<FIKRetargetOpSettingsBase*>(StructMemory);
		return SkeletonProvider->GetSkeleton(PropertyName);
	}

	return nullptr;
}

// ********** Begin Class UIKRigStructWrapperBase ****************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UIKRigStructWrapperBase;
UClass* UIKRigStructWrapperBase::GetPrivateStaticClass()
{
	using TClass = UIKRigStructWrapperBase;
	if (!Z_Registration_Info_UClass_UIKRigStructWrapperBase.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("IKRigBoneSnapperSolverController"),
			Z_Registration_Info_UClass_UIKRigStructWrapperBase.InnerSingleton,
			StaticRegisterNativesUIKRigStructWrapperBase,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_UIKRigStructWrapperBase.InnerSingleton;
}
void UIKRigStructWrapperBase::StaticRegisterNativesUIKRigStructWrapperBase()
{
}
UIKRigStructWrapperBase::UIKRigStructWrapperBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UIKRigStructWrapperBase);
UIKRigStructWrapperBase::~UIKRigStructWrapperBase() {}
// ********** End Class UIKRigStructWrapperBase ******************************************

bool UIKRigStructWrapperBase::IsValid() const
{
	return StructToView.IsValid() && WrapperProperty != nullptr;
}

void UIKRigStructWrapperBase::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdateWrappedStructWithLatestValues();
}

void UIKRigStructWrapperBase::UpdateWrappedStructWithLatestValues()
{
	if (!(StructToView.IsValid() && WrapperProperty))
	{
		return;
	}

	// push wrapper values to the wrapped struct
	void* WrapperMemory = WrapperProperty->ContainerPtrToValuePtr<void>(this);
	void* WrappedMemory = StructToView.MemoryProvider();
	StructToView.Type->CopyScriptStruct(WrappedMemory, WrapperMemory);
}

#undef LOCTEXT_NAMESPACE


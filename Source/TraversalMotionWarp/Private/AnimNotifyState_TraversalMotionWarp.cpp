// Copyright (c) 2026 DGOne. All Rights Reserved.

#include "AnimNotifyState_TraversalMotionWarp.h"
#include "TraversalMotionWarpComponent.h"
#include "TraversalRootMotionModifier_SkewWarp.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_TraversalMotionWarp)

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#endif

UAnimNotifyState_TraversalMotionWarp::UAnimNotifyState_TraversalMotionWarp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootMotionModifier = ObjectInitializer.CreateDefaultSubobject<UTraversalRootMotionModifier_SkewWarp>(this, TEXT("RootMotionModifier_SkewWarp"));
}

void UAnimNotifyState_TraversalMotionWarp::OnBecomeRelevant(UTraversalMotionWarpComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const
{
	UTraversalRootMotionModifier* RootMotionModifierNew = AddRootMotionModifier(MotionWarpingComp, Animation, StartTime, EndTime);

	if (RootMotionModifierNew)
	{
		if (!RootMotionModifierNew->OnActivateDelegate.IsBound())
		{
			RootMotionModifierNew->OnActivateDelegate.BindDynamic(this, &UAnimNotifyState_TraversalMotionWarp::OnRootMotionModifierActivate);
		}

		if (!RootMotionModifierNew->OnUpdateDelegate.IsBound())
		{
			RootMotionModifierNew->OnUpdateDelegate.BindDynamic(this, &UAnimNotifyState_TraversalMotionWarp::OnRootMotionModifierUpdate);
		}

		if (!RootMotionModifierNew->OnDeactivateDelegate.IsBound())
		{
			RootMotionModifierNew->OnDeactivateDelegate.BindDynamic(this, &UAnimNotifyState_TraversalMotionWarp::OnRootMotionModifierDeactivate);
		}
	}
}

UTraversalRootMotionModifier* UAnimNotifyState_TraversalMotionWarp::AddRootMotionModifier_Implementation(UTraversalMotionWarpComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const
{
	if (MotionWarpingComp && RootMotionModifier)
	{
		return MotionWarpingComp->AddModifierFromTemplate(RootMotionModifier, Animation, StartTime, EndTime);
	}

	return nullptr;
}

void UAnimNotifyState_TraversalMotionWarp::OnRootMotionModifierActivate(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier)
{
	// Notify blueprint
	OnWarpBegin(MotionWarpingComp, Modifier);
}

void UAnimNotifyState_TraversalMotionWarp::OnRootMotionModifierUpdate(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier)
{
	// Notify blueprint
	OnWarpUpdate(MotionWarpingComp, Modifier);
}

void UAnimNotifyState_TraversalMotionWarp::OnRootMotionModifierDeactivate(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier)
{
	// Notify blueprint
	OnWarpEnd(MotionWarpingComp, Modifier);
}

#if WITH_EDITOR
void UAnimNotifyState_TraversalMotionWarp::ValidateAssociatedAssets()
{
	static const FName NAME_AssetCheck("AssetCheck");

	if(UAnimSequenceBase* ContainingAsset = Cast<UAnimSequenceBase>(GetContainingAsset()))
	{
		if (RootMotionModifier == nullptr)
		{
			FMessageLog AssetCheckLog(NAME_AssetCheck);

			const FText MessageLooping = FText::Format(
				NSLOCTEXT("AnimNotify", "MotionWarping_InvalidRootMotionModifier", "Motion Warping window in {0} doesn't have a valid RootMotionModifier"),
				FText::AsCultureInvariant(GetNameSafe(ContainingAsset)));
			AssetCheckLog.Warning()
				->AddToken(FUObjectToken::Create(ContainingAsset))
				->AddToken(FTextToken::Create(MessageLooping));

			if (GIsEditor)
			{
				const bool bForce = true;
				AssetCheckLog.Notify(MessageLooping, EMessageSeverity::Warning, bForce);
			}
		}
		else if (const UTraversalRootMotionModifier_Warp* RootMotionModifierWarp = Cast<UTraversalRootMotionModifier_Warp>(RootMotionModifier))
		{
			if (RootMotionModifierWarp->WarpTargetName.IsNone())
			{
				FMessageLog AssetCheckLog(NAME_AssetCheck);

				const FText MessageLooping = FText::Format(
					NSLOCTEXT("AnimNotify", "MotionWarping_InvalidWarpTargetName", "Motion Warping window in {0} doesn't specify a valid Warp Target Name"),
					FText::AsCultureInvariant(GetNameSafe(ContainingAsset)));
				AssetCheckLog.Warning()
					->AddToken(FUObjectToken::Create(ContainingAsset))
					->AddToken(FTextToken::Create(MessageLooping));

				if (GIsEditor)
				{
					const bool bForce = true;
					AssetCheckLog.Notify(MessageLooping, EMessageSeverity::Warning, bForce);
				}
			}
		}
	}
}

void UAnimNotifyState_TraversalMotionWarp::DrawInEditor(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, const FAnimNotifyEvent& NotifyEvent) const
{
	if (RootMotionModifier)
	{
		FAnimNotifyEvent NotifyEventWithColor = NotifyEvent;
		NotifyEventWithColor.NotifyColor = NotifyColor;
		
		// Necessary for FCompactPose (likely used by most RootMotionModifiers) that uses a FAnimStackAllocator (TMemStackAllocator) which allocates from FMemStack.
		// When allocating memory from FMemStack we need to explicitly use FMemMark to ensure items are freed when the scope exits. 
		FMemMark Mark(FMemStack::Get());
		RootMotionModifier->DrawInEditor(PDI, MeshComp, Animation, NotifyEventWithColor);
	}
}

void UAnimNotifyState_TraversalMotionWarp::DrawCanvasInEditor(FCanvas& Canvas, FSceneView& View, USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, const FAnimNotifyEvent& NotifyEvent) const
{
	if (RootMotionModifier)
	{
		FAnimNotifyEvent NotifyEventWithColor = NotifyEvent;
		NotifyEventWithColor.NotifyColor = NotifyColor;

		// Necessary for FCompactPose (likely used by most RootMotionModifiers) that uses a FAnimStackAllocator (TMemStackAllocator) which allocates from FMemStack.
		// When allocating memory from FMemStack we need to explicitly use FMemMark to ensure items are freed when the scope exits.
		FMemMark Mark(FMemStack::Get());
		RootMotionModifier->DrawCanvasInEditor(Canvas, View, MeshComp, Animation, NotifyEventWithColor);
	}
}

#endif

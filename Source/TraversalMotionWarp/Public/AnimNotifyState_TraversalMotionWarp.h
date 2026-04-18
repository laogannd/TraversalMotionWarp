// Copyright (c) 2026 DGOne. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_TraversalMotionWarp.generated.h"

#define UE_API TRAVERSALMOTIONWARP_API

class UTraversalMotionWarpComponent;
class UAnimSequenceBase;
class UTraversalRootMotionModifier;

/** AnimNotifyState used to define a motion warping window in the animation */
UCLASS(MinimalAPI, meta = (DisplayName = "Traversal Motion Warp"))
class UAnimNotifyState_TraversalMotionWarp : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	//@TODO: Prevent notify callbacks and add comments explaining why we don't use those here.

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Config")
	TObjectPtr<UTraversalRootMotionModifier> RootMotionModifier;

	UE_API UAnimNotifyState_TraversalMotionWarp(const FObjectInitializer& ObjectInitializer);

	/** Called from the MotionWarpingComp when this notify becomes relevant. See: UTraversalMotionWarpComponent::Update */
	UE_API void OnBecomeRelevant(UTraversalMotionWarpComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const;

	/** Creates a root motion modifier from the config class defined in the notify */
	UFUNCTION(BlueprintNativeEvent, Category = "Motion Warping")
	UE_API UTraversalRootMotionModifier* AddRootMotionModifier(UTraversalMotionWarpComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const;

	UFUNCTION()
	UE_API void OnRootMotionModifierActivate(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier);

	UFUNCTION()
	UE_API void OnRootMotionModifierUpdate(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier);

	UFUNCTION()
	UE_API void OnRootMotionModifierDeactivate(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier);

	UFUNCTION(BlueprintImplementableEvent, Category = "Motion Warping")
	UE_API void OnWarpBegin(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Motion Warping")
	UE_API void OnWarpUpdate(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Motion Warping")
	UE_API void OnWarpEnd(UTraversalMotionWarpComponent* MotionWarpingComp, UTraversalRootMotionModifier* Modifier) const;

#if WITH_EDITOR
	UE_API virtual void ValidateAssociatedAssets() override;

	UE_API virtual void DrawInEditor(class FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, const FAnimNotifyEvent& NotifyEvent) const override;
	UE_API virtual void DrawCanvasInEditor(FCanvas& Canvas, FSceneView& View, USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, const FAnimNotifyEvent& NotifyEvent) const override;
#endif
};

#undef UE_API

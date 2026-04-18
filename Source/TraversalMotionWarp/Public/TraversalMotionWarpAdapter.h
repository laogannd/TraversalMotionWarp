// Copyright (c) 2026 DGOne. All Rights Reserved.

#pragma once

#include "TraversalRootMotionModifier.h"
#include "TraversalMotionWarpAdapter.generated.h"


DECLARE_DELEGATE_RetVal_ThreeParams(FTransform, FTraversalOnWarpLocalspaceRootMotionWithContext, const FTransform&, float, const FTraversalMotionWarpUpdateContext*)
DECLARE_DELEGATE_RetVal_ThreeParams(FTransform, FTraversalOnWarpWorldspaceRootMotionWithContext, const FTransform&, float, const FTraversalMotionWarpUpdateContext*)

/**
 * MotionWarpingBaseAdapter: base class to adapt/apply motion warping to a target. Concrete subclasses should override
 */
UCLASS(MinimalAPI, Abstract)
class UTraversalMotionWarpBaseAdapter : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UTraversalMotionWarpBaseAdapter() {}
	virtual AActor* GetActor() const { return nullptr; }
	virtual USkeletalMeshComponent* GetMesh() const { return nullptr; }
	virtual FVector GetVisualRootLocation() const { return FVector::ZeroVector; }
	virtual FVector GetBaseVisualTranslationOffset() const { return FVector::ZeroVector; }
	virtual FQuat GetBaseVisualRotationOffset() const { return FQuat::Identity; }

	// A MotionWarpingComponent will bind to this delegate to perform warping when it is triggered
	FTraversalOnWarpLocalspaceRootMotionWithContext WarpLocalRootMotionDelegate;
};


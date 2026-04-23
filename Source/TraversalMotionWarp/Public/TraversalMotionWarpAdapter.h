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

	/** Teleport the actor to a new feet location. Returns true if successful.
	 *  If bSweep is true, performs a sweep test and returns false if blocked. */
	virtual bool TeleportTo(const FVector& NewFeetLocation, const FQuat& NewRotation, bool bSweep = false) { return false; }

	/** Sweep the actor's collision shape from Start to End (feet locations).
	 *  Returns true if the path is clear (no blocking hit). */
	virtual bool SweepTestMovePath(const FVector& StartFeetLocation, const FVector& EndFeetLocation, FHitResult& OutHit) const { return true; }

	/** Sweep with a uniformly shrunk collision shape. ShrinkFactor 0.0 = full size, 1.0 = zero size.
	 *  Returns true if the path is clear (no blocking hit). */
	virtual bool SweepTestMovePathShrunk(const FVector& StartFeetLocation, const FVector& EndFeetLocation, float ShrinkFactor, FHitResult& OutHit) const { return SweepTestMovePath(StartFeetLocation, EndFeetLocation, OutHit); }

	/** Test whether the actor's collision shape overlaps any blocking geometry at the given feet location.
	 *  Returns true if the location is clear (no overlap). */
	virtual bool OverlapTestAtLocation(const FVector& FeetLocation) const { return true; }

	// A MotionWarpingComponent will bind to this delegate to perform warping when it is triggered
	FTraversalOnWarpLocalspaceRootMotionWithContext WarpLocalRootMotionDelegate;
};


// Copyright (c) 2026 DGOne. All Rights Reserved.

#pragma once

#include "TraversalMotionWarpAdapter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TraversalMotionWarpCharacterAdapter.generated.h"

#define UE_API TRAVERSALMOTIONWARP_API

// Adapter for Character / ChararacterMovementComponent actors to participate in motion warping
UCLASS(MinimalAPI)
class UTraversalMotionWarpCharacterAdapter : public UTraversalMotionWarpBaseAdapter
{
	GENERATED_BODY()

public:
	UE_API virtual void BeginDestroy() override;

	UE_API void SetCharacter(ACharacter* InCharacter);

	UE_API virtual AActor* GetActor() const override;
	UE_API virtual USkeletalMeshComponent* GetMesh() const override;
	UE_API virtual FVector GetVisualRootLocation() const override;
	UE_API virtual FVector GetBaseVisualTranslationOffset() const override;
	UE_API virtual FQuat GetBaseVisualRotationOffset() const override;

	UE_API virtual bool TeleportTo(const FVector& NewFeetLocation, const FQuat& NewRotation, bool bSweep = false) override;

	UE_API virtual bool SweepTestMovePath(const FVector& StartFeetLocation, const FVector& EndFeetLocation, FHitResult& OutHit) const override;

	UE_API virtual bool SweepTestMovePathShrunk(const FVector& StartFeetLocation, const FVector& EndFeetLocation, float ShrinkFactor, FHitResult& OutHit) const override;

	UE_API virtual bool OverlapTestAtLocation(const FVector& FeetLocation) const override;

private:
	// Triggered when the character says it's time to pre-process local root motion. This adapter catches the request and passes along to the Warping component
	FTransform WarpLocalRootMotionOnCharacter(const FTransform& LocalRootMotionTransform, UCharacterMovementComponent* TargetMoveComp, float DeltaSeconds);

	/** The associated character */
	TWeakObjectPtr<ACharacter> TargetCharacter;
};

#undef UE_API

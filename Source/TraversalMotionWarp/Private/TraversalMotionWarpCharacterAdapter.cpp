// Copyright (c) 2026 DGOne. All Rights Reserved.

#include "TraversalMotionWarpCharacterAdapter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TraversalMotionWarpCharacterAdapter)



void UTraversalMotionWarpCharacterAdapter::BeginDestroy()
{
	const ACharacter* RawTargetCharacter = TargetCharacter.Get();
	UCharacterMovementComponent* CharacterMovement = RawTargetCharacter ? RawTargetCharacter->GetCharacterMovement() : nullptr;
	if (CharacterMovement)
	{
		CharacterMovement->ProcessRootMotionPreConvertToWorld.Unbind();
	}

	Super::BeginDestroy();
}

void UTraversalMotionWarpCharacterAdapter::SetCharacter(ACharacter* InCharacter)
{
	if (ensureMsgf(InCharacter && InCharacter->GetCharacterMovement(), TEXT("Invalid Character or missing CharacterMovementComponent. Motion warping will not function.")))
	{
		TargetCharacter = InCharacter;
		InCharacter->GetCharacterMovement()->ProcessRootMotionPreConvertToWorld.BindUObject(this, &UTraversalMotionWarpCharacterAdapter::WarpLocalRootMotionOnCharacter);
	}
}

AActor* UTraversalMotionWarpCharacterAdapter::GetActor() const
{ 
	return Cast<AActor>(TargetCharacter.Get());
}

USkeletalMeshComponent* UTraversalMotionWarpCharacterAdapter::GetMesh() const
{ 
	if (const ACharacter* RawTargetCharacter = TargetCharacter.Get())
	{
		return RawTargetCharacter->GetMesh();
	}
	return nullptr;
}

FVector UTraversalMotionWarpCharacterAdapter::GetVisualRootLocation() const
{
	if (const ACharacter* RawTargetCharacter = TargetCharacter.Get())
	{
		const float CapsuleHalfHeight = RawTargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FQuat CurrentRotation = RawTargetCharacter->GetActorQuat();
		return  (RawTargetCharacter->GetActorLocation() - CurrentRotation.GetUpVector() * CapsuleHalfHeight);
	}
	return FVector::ZeroVector;
}

FVector UTraversalMotionWarpCharacterAdapter::GetBaseVisualTranslationOffset() const
{
	if (const ACharacter* RawTargetCharacter = TargetCharacter.Get())
	{
		return RawTargetCharacter->GetBaseTranslationOffset();
	}
	return FVector::ZeroVector;
}

FQuat UTraversalMotionWarpCharacterAdapter::GetBaseVisualRotationOffset() const
{
	if (const ACharacter* RawTargetCharacter = TargetCharacter.Get())
	{
		return RawTargetCharacter->GetBaseRotationOffset();
	}
	return FQuat::Identity;
}

bool UTraversalMotionWarpCharacterAdapter::TeleportTo(const FVector& NewFeetLocation, const FQuat& NewRotation, bool bSweep)
{
	ACharacter* RawTargetCharacter = TargetCharacter.Get();
	if (!RawTargetCharacter)
	{
		return false;
	}

	// Convert feet location back to actor location (add capsule half height along up vector)
	const float HalfHeight = RawTargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector ActorLocation = NewFeetLocation + NewRotation.GetUpVector() * HalfHeight;

	if (bSweep)
	{
		FHitResult HitResult;
		RawTargetCharacter->SetActorLocationAndRotation(ActorLocation, NewRotation, true, &HitResult);
		return !HitResult.bBlockingHit;
	}

	return RawTargetCharacter->TeleportTo(ActorLocation, NewRotation.Rotator(), false, true);
}

bool UTraversalMotionWarpCharacterAdapter::SweepTestMovePath(const FVector& StartFeetLocation, const FVector& EndFeetLocation, FHitResult& OutHit) const
{
	const ACharacter* RawTargetCharacter = TargetCharacter.Get();
	if (!RawTargetCharacter)
	{
		return true;
	}

	const UCapsuleComponent* Capsule = RawTargetCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		return true;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FQuat ActorQuat = RawTargetCharacter->GetActorQuat();

	// Convert feet locations to capsule center locations
	const FVector StartCenter = StartFeetLocation + ActorQuat.GetUpVector() * HalfHeight;
	const FVector EndCenter = EndFeetLocation + ActorQuat.GetUpVector() * HalfHeight;

	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	// Use the character's collision channel and profile
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WarpPathSweep), false, RawTargetCharacter);
	const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());

	const bool bHit = RawTargetCharacter->GetWorld()->SweepSingleByChannel(
		OutHit,
		StartCenter,
		EndCenter,
		ActorQuat,
		Capsule->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams,
		ResponseParams
	);

	return !bHit;
}

bool UTraversalMotionWarpCharacterAdapter::SweepTestMovePathShrunk(const FVector& StartFeetLocation, const FVector& EndFeetLocation, float ShrinkFactor, FHitResult& OutHit) const
{
	if (ShrinkFactor <= 0.f)
	{
		return SweepTestMovePath(StartFeetLocation, EndFeetLocation, OutHit);
	}

	const ACharacter* RawTargetCharacter = TargetCharacter.Get();
	if (!RawTargetCharacter)
	{
		return true;
	}

	const UCapsuleComponent* Capsule = RawTargetCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		return true;
	}

	const float Scale = FMath::Clamp(1.f - ShrinkFactor, 0.05f, 1.f);
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() * Scale;
	const float Radius = Capsule->GetScaledCapsuleRadius() * Scale;
	const FQuat ActorQuat = RawTargetCharacter->GetActorQuat();

	// Use the shrunk half-height to compute the capsule center so it stays anchored to the feet
	const FVector StartCenter = StartFeetLocation + ActorQuat.GetUpVector() * HalfHeight;
	const FVector EndCenter = EndFeetLocation + ActorQuat.GetUpVector() * HalfHeight;

	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WarpPathSweepShrunk), false, RawTargetCharacter);
	const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());

	const bool bHit = RawTargetCharacter->GetWorld()->SweepSingleByChannel(
		OutHit,
		StartCenter,
		EndCenter,
		ActorQuat,
		Capsule->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams,
		ResponseParams
	);

	return !bHit;
}

bool UTraversalMotionWarpCharacterAdapter::OverlapTestAtLocation(const FVector& FeetLocation) const
{
	const ACharacter* RawTargetCharacter = TargetCharacter.Get();
	if (!RawTargetCharacter)
	{
		return true;
	}

	const UCapsuleComponent* Capsule = RawTargetCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		return true;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FQuat ActorQuat = RawTargetCharacter->GetActorQuat();

	// Convert feet location to capsule center
	const FVector CenterLocation = FeetLocation + ActorQuat.GetUpVector() * HalfHeight;

	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WarpTargetOverlap), false, RawTargetCharacter);
	const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());

	const bool bOverlap = RawTargetCharacter->GetWorld()->OverlapBlockingTestByChannel(
		CenterLocation,
		ActorQuat,
		Capsule->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams,
		ResponseParams
	);

	return !bOverlap;
}

bool UTraversalMotionWarpCharacterAdapter::IsAirborne() const
{
	if (const ACharacter* RawTargetCharacter = TargetCharacter.Get())
	{
		if (const UCharacterMovementComponent* CharacterMovement = RawTargetCharacter->GetCharacterMovement())
		{
			return CharacterMovement->IsFalling();
		}
	}
	return false;
}

bool UTraversalMotionWarpCharacterAdapter::IsReplayingMoves() const
{
	const ACharacter* RawTargetCharacter = TargetCharacter.Get();
	return RawTargetCharacter && RawTargetCharacter->bClientUpdating;
}

FTraversalWarpMovementState UTraversalMotionWarpCharacterAdapter::BeginWarpMovementControl(bool bControlVertical)
{
	// Nothing to suspend (grounded warp, or the air-warp flag is off) — return an invalid state and stay
	// out of the ref count entirely. This keeps a non-suspending warp from "claiming" control first and
	// blocking an overlapping airborne warp from actually suspending movement.
	if (!bControlVertical)
	{
		return FTraversalWarpMovementState();
	}

	ACharacter* RawTargetCharacter = TargetCharacter.Get();
	UCharacterMovementComponent* CharacterMovement = RawTargetCharacter ? RawTargetCharacter->GetCharacterMovement() : nullptr;
	if (!CharacterMovement)
	{
		return FTraversalWarpMovementState();
	}

	// First warp to take control captures the original state and applies the suspension.
	// Subsequent (overlapping) warps just share the already-captured state.
	if (WarpMovementControlRefCount == 0)
	{
		OriginalState = FTraversalWarpMovementState();
		OriginalState.bValid = true;
		OriginalState.bWasAirborne = CharacterMovement->IsFalling();
		OriginalState.PrevMovementMode = static_cast<uint8>(CharacterMovement->MovementMode.GetValue());
		OriginalState.PrevCustomMode = CharacterMovement->CustomMovementMode;
		OriginalState.PrevGravityScale = CharacterMovement->GravityScale;
		OriginalState.PrevVelocity = CharacterMovement->Velocity;

		// Zero velocity and clear any pending launch/acceleration so a queued LaunchCharacter
		// can't fire when we restore the movement mode at warp end.
		CharacterMovement->StopMovementImmediately();
		CharacterMovement->SetMovementMode(MOVE_Flying);
		// Defensive: even if something bumps us back to Falling mid-warp, gravity stays off.
		CharacterMovement->GravityScale = 0.f;
	}

	++WarpMovementControlRefCount;
	return OriginalState;
}

void UTraversalMotionWarpCharacterAdapter::EndWarpMovementControl(const FTraversalWarpMovementState& CapturedState, bool bResumeFalling)
{
	if (WarpMovementControlRefCount <= 0)
	{
		return;
	}

	// Only the last release restores movement.
	if (--WarpMovementControlRefCount > 0)
	{
		return;
	}

	ACharacter* RawTargetCharacter = TargetCharacter.Get();
	UCharacterMovementComponent* CharacterMovement = RawTargetCharacter ? RawTargetCharacter->GetCharacterMovement() : nullptr;

	if (CharacterMovement && OriginalState.bValid)
	{
		CharacterMovement->GravityScale = OriginalState.PrevGravityScale;

		// Let the engine re-resolve from here. On normal completion (bResumeFalling == false) the
		// character is on/at the platform, so Walking lets FindFloor settle it (auto-falls if no floor).
		// On cancellation mid-air (bResumeFalling == true) drop back into Falling so they fall naturally.
		// We deliberately do NOT restore PrevVelocity — that is the stale pre-warp falling velocity and
		// would cause a visible downward snap. The warp left velocity near zero; CMC re-accumulates next tick.
		CharacterMovement->SetMovementMode(bResumeFalling ? MOVE_Falling : MOVE_Walking);
	}

	OriginalState = FTraversalWarpMovementState();
}

FTransform UTraversalMotionWarpCharacterAdapter::WarpLocalRootMotionOnCharacter(const FTransform& LocalRootMotionTransform, UCharacterMovementComponent* TargetMoveComp, float DeltaSeconds)
{
	const ACharacter* RawTargetCharacter = TargetCharacter.Get();
	if (WarpLocalRootMotionDelegate.IsBound() && RawTargetCharacter)
	{
		FTraversalMotionWarpUpdateContext WarpingContext;
		
		WarpingContext.DeltaSeconds = DeltaSeconds;

		// When replaying saved moves we need to look at the contributor to root motion back then.
		if (RawTargetCharacter->bClientUpdating)
		{
			const UCharacterMovementComponent* MoveComp = RawTargetCharacter->GetCharacterMovement();
			check(MoveComp);

			const FSavedMove_Character* SavedMove = MoveComp->GetCurrentReplayedSavedMove();
			check(SavedMove);

			if (SavedMove->RootMotionMontage.IsValid())
			{
				WarpingContext.Animation = SavedMove->RootMotionMontage.Get();
				WarpingContext.CurrentPosition = SavedMove->RootMotionTrackPosition;
				WarpingContext.PreviousPosition = SavedMove->RootMotionPreviousTrackPosition;
				WarpingContext.PlayRate = SavedMove->RootMotionPlayRateWithScale;
			}
		}
		else // If we are not replaying a move, just use the current root motion montage
		{
			if (const FAnimMontageInstance* RootMotionMontageInstance = RawTargetCharacter->GetRootMotionAnimMontageInstance())
			{
				const UAnimMontage* Montage = RootMotionMontageInstance->Montage;
				check(Montage);

				WarpingContext.Animation = Montage;
				WarpingContext.CurrentPosition = RootMotionMontageInstance->GetPosition();
				WarpingContext.PreviousPosition = RootMotionMontageInstance->GetPreviousPosition();
				WarpingContext.Weight = RootMotionMontageInstance->GetWeight();
				WarpingContext.PlayRate = RootMotionMontageInstance->Montage->RateScale * RootMotionMontageInstance->GetPlayRate();
			}
		}

		// TODO: Consider simply having a pointer to the MWComponent whereby we can call a function on it, rather than using this delegate approach
		return WarpLocalRootMotionDelegate.Execute(LocalRootMotionTransform, DeltaSeconds, &WarpingContext);
	}

	return LocalRootMotionTransform;
}

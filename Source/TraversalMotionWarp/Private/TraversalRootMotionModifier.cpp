// Copyright (c) 2026 DGOne. All Rights Reserved.

#include "TraversalRootMotionModifier.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TraversalMotionWarpComponent.h"
#include "TraversalMotionWarpAdapter.h"
#include "DrawDebugHelpers.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TraversalRootMotionModifier)

// FTraversalMotionWarpTarget
///////////////////////////////////////////////////////////////

FTraversalMotionWarpTarget::FTraversalMotionWarpTarget(const FName& InName, const USceneComponent* InComp, FName InBoneName, bool bInFollowComponent, ETraversalWarpTargetLocationOffsetDirection InLocationOffsetDirection, const AActor* InAvatarActor, const FVector& InLocOffset, const FRotator& InRotOffset)
{
	if (ensure(InComp))
	{
		Name = InName;
		Component = InComp;
		BoneName = InBoneName;
		bFollowComponent = bInFollowComponent;
		LocationOffset = InLocOffset;
		RotationOffset = InRotOffset;
		AvatarActor = InAvatarActor;
		LocationOffsetDirection = InLocationOffsetDirection;

		FTransform Transform = FTransform::Identity;
		if (BoneName != NAME_None)
		{
			Transform = FTraversalMotionWarpTarget::GetTargetTransformFromComponent(InComp, InBoneName);
		}
		else
		{
			Transform = InComp->GetComponentTransform();
		}

		CacheOffset(Transform);
		RecalculateOffset(Transform);

		Location = Transform.GetLocation();
		Rotation = Transform.Rotator();
	}
}

FTraversalMotionWarpTarget::FTraversalMotionWarpTarget(const FName& InName, const USceneComponent* InComp, FName InBoneName, bool bInFollowComponent, const FVector& InLocOffset, const FRotator& InRotOffset)
	: FTraversalMotionWarpTarget(InName, InComp, InBoneName, bInFollowComponent, ETraversalWarpTargetLocationOffsetDirection::TargetsForwardVector, nullptr, InLocOffset, InRotOffset)
{
}

FTransform FTraversalMotionWarpTarget::GetTargetTransformFromComponent(const USceneComponent* Comp, const FName& BoneName)
{
	if (Comp == nullptr)
	{
		UE_LOG(LogTraversalMotionWarp, Warning, TEXT("FTraversalMotionWarpTarget::GetTargetTransformFromComponent: Invalid Component"));
		return FTransform::Identity;
	}

	if (Comp->DoesSocketExist(BoneName) == false)
	{
		UE_LOG(LogTraversalMotionWarp, Warning, TEXT("FTraversalMotionWarpTarget::GetTargetTransformFromComponent: Invalid Bone or Socket. Comp: %s Owner: %s BoneName: %s"),
			*GetNameSafe(Comp), *GetNameSafe(Comp->GetOwner()), *BoneName.ToString());

		return Comp->GetComponentTransform();
	}

	return Comp->GetSocketTransform(BoneName);
}

void UTraversalRootMotionModifier_Warp::Serialize(FArchive& Ar)
{
	// Handle change of default blend type
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	if (Ar.IsLoading())
	{
		const int32 CustomVersion = Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID);
		if (CustomVersion < FFortniteMainBranchObjectVersion::ChangeDefaultAlphaBlendType)
		{
			// Switch the default back to Linear so old data remains the same
			// Important: this is done before loading so if data has changed from default it still works
			AddTranslationEasingFunc = EAlphaBlendOption::Linear;
		}
	}
	
	Super::Serialize(Ar);
}

FTransform FTraversalMotionWarpTarget::GetTargetTrasform() const
{
	if (Component.IsValid() && bFollowComponent)
	{
		FTransform Transform = FTransform::Identity;
		if (BoneName != NAME_None)
		{
			Transform = FTraversalMotionWarpTarget::GetTargetTransformFromComponent(Component.Get(), BoneName);
		}
		else
		{
			Transform = Component->GetComponentTransform();
		}

		RecalculateOffset(Transform);
		return FTransform(Transform.GetRotation(), Transform.GetLocation());
	}

	return FTransform(Rotation, Location);
}

/*	Because vector from target to owner changes during warping, offset needs to be cached. */
void FTraversalMotionWarpTarget::CacheOffset(const FTransform& InTransform)
{
	// Forward offset doesn't need to be cached if it's the only one used. Otherwise, cache it too.
	bCacheForwardOffset =
		LocationOffsetDirection == ETraversalWarpTargetLocationOffsetDirection::VectorFromTargetToOwner &&
			LocationOffset.X > SMALL_NUMBER &&
				(LocationOffset.Y > SMALL_NUMBER || LocationOffset.Z > SMALL_NUMBER);
	
	if (LocationOffsetDirection == ETraversalWarpTargetLocationOffsetDirection::VectorFromTargetToOwner)
	{
		if (AvatarActor.IsValid())
		{
			const FVector ContextVector = (AvatarActor->GetActorLocation() - InTransform.GetLocation()).GetSafeNormal();
			const FVector RightVector = -FVector::CrossProduct(ContextVector, FVector::UpVector);

			if (bCacheForwardOffset)
			{
				CachedForwardOffset = (ContextVector * LocationOffset.X);
			}

			CachedRightOffset = RightVector * LocationOffset.Y;
			CachedUpOffset = -FVector::CrossProduct(RightVector, ContextVector) * LocationOffset.Z;
		}
		else
		{
			UE_LOG(LogTraversalMotionWarp, Warning, TEXT("Motion warping offset is set to VectorFromTargetToOwner but avator actor is invalid"))
		}
	}
}

void FTraversalMotionWarpTarget::RecalculateOffset(FTransform& InTransform) const
{
	FVector Offset = FVector::ZeroVector;
	
	switch (LocationOffsetDirection)
	{
		case ETraversalWarpTargetLocationOffsetDirection::TargetsForwardVector:
			Offset = LocationOffset;
			break;
			
		case ETraversalWarpTargetLocationOffsetDirection::VectorFromTargetToOwner:
			if (AvatarActor.IsValid())
			{
				if (bCacheForwardOffset)
				{
					Offset = Component->GetComponentTransform().Inverse().TransformVector(CachedForwardOffset + CachedUpOffset + CachedRightOffset);
				}
				else
				{
					const FVector ContextVector = (AvatarActor->GetActorLocation() - InTransform.GetLocation()).GetSafeNormal();
					const FVector ForwardOffset = (ContextVector * LocationOffset.X);
					
					Offset = Component->GetComponentTransform().Inverse().TransformVector(ForwardOffset + CachedUpOffset + CachedRightOffset);
				}
			}
			else
			{
				UE_LOG(LogTraversalMotionWarp, Warning, TEXT("Motion warping offset is set to VectorFromOwnerToTarget but avator actor is invalid"))
			}
			break;
			
		case ETraversalWarpTargetLocationOffsetDirection::WorldSpace:
			Offset = Component->GetComponentTransform().Inverse().TransformVector(LocationOffset);
			break;
			
		default:
			checkNoEntry();
	}
	
	InTransform = FTransform(RotationOffset, Offset) * InTransform;
}

// UTraversalRootMotionModifier
///////////////////////////////////////////////////////////////

UTraversalRootMotionModifier::UTraversalRootMotionModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UTraversalMotionWarpComponent* UTraversalRootMotionModifier::GetOwnerComponent() const
{
	return Cast<UTraversalMotionWarpComponent>(GetOuter());
}

UTraversalMotionWarpBaseAdapter* UTraversalRootMotionModifier::GetOwnerAdapter() const
{
	UTraversalMotionWarpComponent* OwnerComp = GetOwnerComponent();
	return OwnerComp ? OwnerComp->GetOwnerAdapter() : nullptr;
}

AActor* UTraversalRootMotionModifier::GetActorOwner() const
{
	if (UTraversalMotionWarpBaseAdapter* OwnerAdapter = GetOwnerAdapter())
	{
		return OwnerAdapter->GetActor();
	}

	return nullptr;
}

ACharacter* UTraversalRootMotionModifier::GetCharacterOwner() const
{
	return Cast<ACharacter>(GetActorOwner());
}

void UTraversalRootMotionModifier::Update(const FTraversalMotionWarpUpdateContext& Context)
{
	const AActor* ActorOwner = GetActorOwner();

	if (ActorOwner == nullptr)
	{
		return;
	}

	// Mark for removal if our animation is not relevant anymore
	if (!Context.Animation.IsValid() || Context.Animation.Get() != Animation)
	{
		UE_LOG(LogTraversalMotionWarp, Verbose, TEXT("MotionWarping: Marking RootMotionModifier for removal. Reason: Animation is not valid. Char: %s Current Animation: %s. Window: Animation: %s [%f %f] [%f %f]"),
			*GetNameSafe(ActorOwner), *GetNameSafe(Context.Animation.Get()), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

		SetState(ETraversalRootMotionModifierState::MarkedForRemoval);
		return;
	}

	// Update playback times and weight
	PreviousPosition = Context.PreviousPosition;
	CurrentPosition = Context.CurrentPosition;
	Weight = Context.Weight;
	PlayRate = Context.PlayRate;

	// Mark for removal if the animation already passed the warping window
	if (PreviousPosition >= EndTime)
	{
		UE_LOG(LogTraversalMotionWarp, Verbose, TEXT("MotionWarping: Marking RootMotionModifier for removal. Reason: Window has ended. Char: %s Animation: %s [%f %f] [%f %f]"),
			*GetNameSafe(ActorOwner), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

		SetState(ETraversalRootMotionModifierState::MarkedForRemoval);
		return;
	}

	// Mark for removal if we jumped to a position outside the warping window
	if (State == ETraversalRootMotionModifierState::Active && PreviousPosition < EndTime && (CurrentPosition > EndTime || CurrentPosition < StartTime))
	{
		const float ExpectedDelta = Context.DeltaSeconds * Context.PlayRate;
		const float ActualDelta = CurrentPosition - PreviousPosition;
		if (!FMath::IsNearlyZero(FMath::Abs(ActualDelta - ExpectedDelta), KINDA_SMALL_NUMBER))
		{
			UE_LOG(LogTraversalMotionWarp, Verbose, TEXT("MotionWarping: Marking RootMotionModifier for removal. Reason: CurrentPosition manually changed. PrevPos: %f CurrPos: %f DeltaTime: %f ExpectedDelta: %f ActualDelta: %f"),
				PreviousPosition, CurrentPosition, Context.DeltaSeconds, ExpectedDelta, ActualDelta);

			SetState(ETraversalRootMotionModifierState::MarkedForRemoval);
			return;
		}
	}

	// Check if we are inside the warping window
	if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
	{
		// If we were waiting, switch to active
		if (GetState() == ETraversalRootMotionModifierState::Waiting)
		{
			SetState(ETraversalRootMotionModifierState::Active);
		}
	}

	if (State == ETraversalRootMotionModifierState::Active)
	{
		if (UTraversalMotionWarpComponent* OwnerComp = GetOwnerComponent())
		{
			OnUpdateDelegate.ExecuteIfBound(OwnerComp, this);
		}
	}
}

void UTraversalRootMotionModifier::SetState(ETraversalRootMotionModifierState NewState)
{
	if (State != NewState)
	{
		ETraversalRootMotionModifierState LastState = State;

		State = NewState;

		OnStateChanged(LastState);
	}
}

void UTraversalRootMotionModifier::OnStateChanged(ETraversalRootMotionModifierState LastState)
{
	if (UTraversalMotionWarpComponent* OwnerComp = GetOwnerComponent())
	{
		if (LastState != ETraversalRootMotionModifierState::Active && State == ETraversalRootMotionModifierState::Active)
		{
			const UTraversalMotionWarpBaseAdapter* OwnerAdapter = GetOwnerAdapter();

			checkf(OwnerAdapter, TEXT("Root motion modifiers expect an owner and adapter"));

			const FVector CurrentLocation = OwnerAdapter->GetVisualRootLocation();
			const FQuat CurrentRotation = OwnerAdapter->GetActor()->GetActorQuat();
			
			ActualStartTime = PreviousPosition;

			StartTransform = FTransform(CurrentRotation, CurrentLocation);

			TotalRootMotionWithinWindow = UTraversalMotionWarpUtilities::ExtractRootMotionFromAnimation(Animation.Get(), StartTime, EndTime);

			OnActivateDelegate.ExecuteIfBound(OwnerComp, this);
		}
		else if (LastState == ETraversalRootMotionModifierState::Active && (State == ETraversalRootMotionModifierState::Disabled || State == ETraversalRootMotionModifierState::MarkedForRemoval))
		{
			OnDeactivateDelegate.ExecuteIfBound(OwnerComp, this);
		}
	}
}

UTraversalRootMotionModifier_Warp::UTraversalRootMotionModifier_Warp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTraversalRootMotionModifier_Warp::Update(const FTraversalMotionWarpUpdateContext& Context)
{
	// Update playback times and state
	Super::Update(Context);

	// Cache sync point transform and trigger OnTargetTransformChanged if needed
	const UTraversalMotionWarpComponent* OwnerComp = GetOwnerComponent();
	if (OwnerComp && GetState() == ETraversalRootMotionModifierState::Active)
	{
		const FTraversalMotionWarpTarget* WarpTargetPtr = OwnerComp->FindWarpTarget(WarpTargetName);

		// Disable if there is no target for us
		if (WarpTargetPtr == nullptr)
		{
			UE_LOG(LogTraversalMotionWarp, Verbose, TEXT("MotionWarping: Marking RootMotionModifier as Disabled. Reason: Invalid Warp Target (%s). Char: %s Animation: %s [%f %f] [%f %f]"),
				*WarpTargetName.ToString(), *GetNameSafe(OwnerComp->GetOwner()), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

			SetState(ETraversalRootMotionModifierState::Disabled);
			return;
		}

		bRootMotionPaused = WarpTargetPtr->bRootMotionPaused;
		bWarpingPaused = WarpTargetPtr->bWarpingPaused;
		
		// Get the warp point sent by the game
		FTransform WarpPointTransformGame = WarpTargetPtr->GetTargetTrasform();

		// Cache the rotation offset to later apply when rotation warping 
		RotationOffset = WarpTargetPtr->RotationOffset.Quaternion();
		
		// Initialize our target transform (where the root should end at the end of the window) with the warp point sent by the game
		FTransform TargetTransform = WarpPointTransformGame;

		// Check if a warp point is defined in the animation. If so, we need to extract it and offset the target transform 
		// the same amount the root bone is offset from the warp point in the animation
		if (WarpPointAnimProvider != ETraversalWarpPointAnimProvider::None)
		{
			if (!CachedOffsetFromWarpPoint.IsSet())
			{
				if (const UTraversalMotionWarpBaseAdapter* OwnerAdapter = GetOwnerAdapter())
				{
					if (WarpPointAnimProvider == ETraversalWarpPointAnimProvider::Static)
					{
						CachedOffsetFromWarpPoint = UTraversalMotionWarpUtilities::CalculateRootTransformRelativeToWarpPointAtTime(*OwnerAdapter, GetAnimation(), EndTime, WarpPointAnimTransform);
					}
					else if (WarpPointAnimProvider == ETraversalWarpPointAnimProvider::Bone)
					{
						CachedOffsetFromWarpPoint = UTraversalMotionWarpUtilities::CalculateRootTransformRelativeToWarpPointAtTime(*OwnerAdapter, GetAnimation(), EndTime, WarpPointAnimBoneName);
					}
				}
			}

			// Update Target Transform based on the offset between the root and the warp point in the animation
			TargetTransform = CachedOffsetFromWarpPoint.GetValue() * WarpPointTransformGame;
		}
		
		CachedTargetTransform *= RootMotionRemainingAfterNotify.Inverse();
		
		if (!CachedTargetTransform.Equals(TargetTransform))
		{
			CachedTargetTransform = TargetTransform;

			OnTargetTransformChanged();
		}
	}
}

void UTraversalRootMotionModifier_Warp::OnTargetTransformChanged()
{
 	if (const UTraversalMotionWarpBaseAdapter* WarpingAdapter = GetOwnerAdapter())
	{
		ActualStartTime = PreviousPosition;

		const FQuat CurrentRotation = WarpingAdapter->GetActor()->GetActorQuat();
		const FVector CurrentLocation = WarpingAdapter->GetVisualRootLocation();
		StartTransform = FTransform(CurrentRotation, CurrentLocation);
	}
}

void UTraversalRootMotionModifier_Warp::OnStateChanged(ETraversalRootMotionModifierState LastState)
{
	Super::OnStateChanged(LastState);

	if (bSubtractRemainingRootMotion)
	{
		RootMotionRemainingAfterNotify = UTraversalMotionWarpUtilities::ExtractRootMotionFromAnimation(Animation.Get(), EndTime, Animation.Get()->GetPlayLength());
	}
}

FQuat UTraversalRootMotionModifier_Warp::GetTargetRotation() const
{
	switch (RotationType)
	{
		case ETraversalMotionWarpRotationType::Default:
			return CachedTargetTransform.GetRotation();
		
		case ETraversalMotionWarpRotationType::Facing:
			if (const AActor* ActorOwner = GetActorOwner())
			{
				const FTransform& ActorTransform = ActorOwner->GetActorTransform();
				const FVector ToSyncPoint = (CachedTargetTransform.GetLocation() - ActorTransform.GetLocation()).GetSafeNormal2D();
				return RotationOffset * FRotationMatrix::MakeFromXZ(ToSyncPoint, FVector::UpVector).ToQuat();
			}
			break;
		
		case ETraversalMotionWarpRotationType::OppositeDefault:
			return FRotationMatrix::MakeFromXZ(CachedTargetTransform.GetRotation().GetForwardVector() * -1, CachedTargetTransform.GetRotation().GetUpVector()).ToQuat();

		case ETraversalMotionWarpRotationType::OppositeFacing:
			if (const AActor* ActorOwner = GetActorOwner())
			{
				const FTransform& ActorTransform = ActorOwner->GetActorTransform();
				const FVector ToSyncPoint = (ActorTransform.GetLocation() - CachedTargetTransform.GetLocation()).GetSafeNormal2D();
				return RotationOffset * FRotationMatrix::MakeFromXZ(ToSyncPoint, FVector::UpVector).ToQuat();
			}
			break;
		
		default:
			checkNoEntry();
	}

	return FQuat::Identity;
}

FQuat UTraversalRootMotionModifier_Warp::WarpRotation(const FTransform& RootMotionDelta, const FTransform& RootMotionTotal, float DeltaSeconds)
{
	if (bRootMotionPaused)
	{
		return FQuat::Identity;
	}

	if (bWarpingPaused)
	{
		return RootMotionDelta.GetRotation();
	}
	
	FQuat CurrentRotation;
	FQuat TargetRotation;

	if (const UTraversalMotionWarpBaseAdapter* WarpingAdapter = GetOwnerAdapter())
	{
		CurrentRotation = WarpingAdapter->GetActor()->GetActorQuat() * WarpingAdapter->GetBaseVisualRotationOffset();
		TargetRotation = CurrentRotation.Inverse() * (GetTargetRotation() * WarpingAdapter->GetBaseVisualRotationOffset() * RootMotionRemainingAfterNotify.GetRotation().Inverse());
	}
	else
	{
		// No owner, no warping possible
		return FQuat::Identity;
	}

	const FQuat TotalRootMotionRotation = RootMotionTotal.GetRotation();

	if (RotationMethod == ETraversalMotionWarpRotationMethod::Scale)
	{
		FRotator TotalRotator(TotalRootMotionRotation);
		FRotator TargetRotator(TargetRotation);
		const double YawDiff = FMath::FindDeltaAngleDegrees(TotalRotator.Yaw, TargetRotator.Yaw);
		const double PitchDiff = FMath::FindDeltaAngleDegrees(TotalRotator.Pitch, TargetRotator.Pitch);
		// To properly compute scale factor target rotation needs to be relative to total rotation.
		// To avoid cases like 170 & -170 resulting in -1 scale factor rather than 1.11.
		const double YawScale = FMath::IsNearlyZero(TotalRotator.Yaw) ? 0.0 : (TotalRotator.Yaw + YawDiff) / TotalRotator.Yaw;
		const double PitchScale = FMath::IsNearlyZero(TotalRotator.Pitch) ? 0.0 : (TotalRotator.Pitch + PitchDiff) / TotalRotator.Pitch;
		const float MaxRotation = WarpMaxRotationRate * DeltaSeconds;
		FRotator ScaledDeltaRotation(RootMotionDelta.GetRotation());
		ScaledDeltaRotation.Yaw = FMath::Clamp(ScaledDeltaRotation.Yaw * YawScale, -MaxRotation, MaxRotation);
		ScaledDeltaRotation.Pitch = FMath::Clamp(ScaledDeltaRotation.Pitch * PitchScale, -MaxRotation, MaxRotation);
		return ScaledDeltaRotation.Quaternion();
	}

	const float TimeRemaining = (EndTime - PreviousPosition) * WarpRotationTimeMultiplier;
	const float PlayRateAdjustedDeltaSeconds = DeltaSeconds * PlayRate;
	const float Alpha = FMath::Clamp(PlayRateAdjustedDeltaSeconds / TimeRemaining, 0.f, 1.f);
	FQuat TargetRotThisFrame = FQuat::Slerp(TotalRootMotionRotation, TargetRotation, Alpha);

	if (RotationMethod != ETraversalMotionWarpRotationMethod::Slerp)
	{
		const float AngleDeltaThisFrame = TotalRootMotionRotation.AngularDistance(TargetRotThisFrame);
		const float MaxAngleDelta = FMath::Abs(FMath::DegreesToRadians(PlayRateAdjustedDeltaSeconds * WarpMaxRotationRate));
		const float TotalAngleDelta = TotalRootMotionRotation.AngularDistance(TargetRotation);
		if (RotationMethod == ETraversalMotionWarpRotationMethod::ConstantRate && (TotalAngleDelta <= MaxAngleDelta))
		{
			TargetRotThisFrame = TargetRotation;
		}
		else if ((AngleDeltaThisFrame > MaxAngleDelta) || RotationMethod == ETraversalMotionWarpRotationMethod::ConstantRate)
		{
			const FVector CrossProduct = FVector::CrossProduct(TotalRootMotionRotation.Vector(), TargetRotation.Vector());
			const float SignDirection = FMath::Sign(CrossProduct.Z);
			const FQuat ClampedRotationThisFrame = FQuat(FVector(0, 0, 1), MaxAngleDelta * SignDirection);
			TargetRotThisFrame = ClampedRotationThisFrame;
		}
	}

	const FQuat DeltaOut = TargetRotThisFrame * TotalRootMotionRotation.Inverse();
	
	return (DeltaOut * RootMotionDelta.GetRotation());
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UTraversalRootMotionModifier_Warp::PrintLog(const FString& Name, const FTransform& OriginalRootMotion, const FTransform& WarpedRootMotion) const
{
	const AActor* ActorOwner = nullptr;

	FVector CurrentLocation;
	USkeletalMeshComponent* SkelMesh = nullptr;

	if (const UTraversalMotionWarpBaseAdapter* WarpingAdapter = GetOwnerAdapter())
	{
		ActorOwner = WarpingAdapter->GetActor();
		SkelMesh = WarpingAdapter->GetMesh();
		CurrentLocation = WarpingAdapter->GetVisualRootLocation();
	}

	if (ActorOwner && SkelMesh)
	{
		const FVector CurrentToTarget = (GetTargetLocation() - CurrentLocation).GetSafeNormal2D();
		const FVector FutureLocation = CurrentLocation + (SkelMesh->ConvertLocalRootMotionToWorld(WarpedRootMotion)).GetTranslation();
		const FRotator CurrentRotation = ActorOwner->GetActorRotation();
		const FRotator FutureRotation = (WarpedRootMotion.GetRotation() * ActorOwner->GetActorQuat()).Rotator();
		const float Dot = FVector::DotProduct(ActorOwner->GetActorForwardVector(), CurrentToTarget);
		const float CurrentDist2D = FVector::Dist2D(GetTargetLocation(), CurrentLocation);
		const float FutureDist2D = FVector::Dist2D(GetTargetLocation(), FutureLocation);
		const float DeltaSeconds = ActorOwner->GetWorld()->GetDeltaSeconds();
		const float Speed = WarpedRootMotion.GetTranslation().Size() / DeltaSeconds;
		const float EndTimeOffset = CurrentPosition - EndTime;

		UE_LOG(LogTraversalMotionWarp, Log, TEXT("%s NetMode: %d Char: %s Anim: %s Win: [%f %f][%f %f] DT: %f WT: %f ETOffset: %f Dist2D: %f Z: %f FDist2D: %f FZ: %f Dot: %f Delta: %s (%f) FDelta: %s (%f) Speed: %f Loc: %s FLoc: %s Rot: %s FRot: %s"),
			*Name, (int32)ActorOwner->GetWorld()->GetNetMode(), *GetNameSafe(ActorOwner), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition, DeltaSeconds, ActorOwner->GetWorld()->GetTimeSeconds(), EndTimeOffset,
			CurrentDist2D, (GetTargetLocation().Z - CurrentLocation.Z), FutureDist2D, (GetTargetLocation().Z - FutureLocation.Z), Dot,
			*OriginalRootMotion.GetTranslation().ToString(), OriginalRootMotion.GetTranslation().Size(), *WarpedRootMotion.GetTranslation().ToString(), WarpedRootMotion.GetTranslation().Size(), Speed,
			*CurrentLocation.ToString(), *FutureLocation.ToString(), *CurrentRotation.ToCompactString(), *FutureRotation.ToCompactString());
	}
}
#endif

// UTraversalRootMotionModifier_SimpleWarp
///////////////////////////////////////////////////////////////

UDEPRECATED_TraversalRootMotionModifier_SimpleWarp::UDEPRECATED_TraversalRootMotionModifier_SimpleWarp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FTransform UDEPRECATED_TraversalRootMotionModifier_SimpleWarp::ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds)
{
	const ACharacter* CharacterOwner = nullptr;

	if (const UTraversalMotionWarpBaseAdapter* Adapter = GetOwnerAdapter())
	{
		CharacterOwner = Cast<ACharacter>(Adapter->GetActor());
	}

	if (CharacterOwner == nullptr)
	{
		return InRootMotion;
	}

	const FTransform& CharacterTransform = CharacterOwner->GetActorTransform();

	FTransform FinalRootMotion = InRootMotion;

	const FTransform RootMotionTotal = UTraversalMotionWarpUtilities::ExtractRootMotionFromAnimation(Animation.Get(), PreviousPosition, EndTime);

	if (bWarpTranslation)
	{
		FVector DeltaTranslation = InRootMotion.GetTranslation();

		const FTransform RootMotionDelta = UTraversalMotionWarpUtilities::ExtractRootMotionFromAnimation(Animation.Get(), PreviousPosition, FMath::Min(CurrentPosition, EndTime));

		const float HorizontalDelta = RootMotionDelta.GetTranslation().Size2D();
		const float HorizontalTarget = FVector::Dist2D(CharacterTransform.GetLocation(), GetTargetLocation());
		const float HorizontalOriginal = RootMotionTotal.GetTranslation().Size2D();
		const float HorizontalTranslationWarped = !FMath::IsNearlyZero(HorizontalOriginal) ? ((HorizontalDelta * HorizontalTarget) / HorizontalOriginal) : 0.f;

		const FTransform MeshRelativeTransform = FTransform(CharacterOwner->GetBaseRotationOffset(), CharacterOwner->GetBaseTranslationOffset());
		const FTransform MeshTransform = MeshRelativeTransform * CharacterOwner->GetActorTransform();
		DeltaTranslation = MeshTransform.InverseTransformPositionNoScale(GetTargetLocation()).GetSafeNormal2D() * HorizontalTranslationWarped;

		if (!bIgnoreZAxis)
		{
			const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector CapsuleBottomLocation = (CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CapsuleHalfHeight));
			const float VerticalDelta = RootMotionDelta.GetTranslation().Z;
			const float VerticalTarget = GetTargetLocation().Z - CapsuleBottomLocation.Z;
			const float VerticalOriginal = RootMotionTotal.GetTranslation().Z;
			const float VerticalTranslationWarped = !FMath::IsNearlyZero(VerticalOriginal) ? ((VerticalDelta * VerticalTarget) / VerticalOriginal) : 0.f;

			DeltaTranslation.Z = VerticalTranslationWarped;
		}
		else
		{
			DeltaTranslation.Z = InRootMotion.GetTranslation().Z;
		}

		FinalRootMotion.SetTranslation(DeltaTranslation);
	}

	if (bWarpRotation)
	{
		const FQuat WarpedRotation = WarpRotation(InRootMotion, RootMotionTotal, DeltaSeconds);
		FinalRootMotion.SetRotation(WarpedRotation);
	}

	if (bIgnoreRootMotionRotation)
	{
		FinalRootMotion.SetRotation(FQuat::Identity);
	}

	// Debug
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 DebugLevel = FTraversalMotionWarpCVars::CVarMotionWarpingDebug.GetValueOnGameThread();
	if (DebugLevel == 1 || DebugLevel == 3)
	{
		PrintLog(TEXT("SimpleWarp"), InRootMotion, FinalRootMotion);
	}

	if (DebugLevel == 2 || DebugLevel == 3)
	{
		const float DrawDebugDuration = FTraversalMotionWarpCVars::CVarMotionWarpingDrawDebugDuration.GetValueOnGameThread();
		DrawDebugCoordinateSystem(CharacterOwner->GetWorld(), GetTargetLocation(), GetTargetRotator(), 50.f, false, DrawDebugDuration, 0, 1.f);
	}
#endif

	return FinalRootMotion;
}

// UTraversalRootMotionModifier_Scale
///////////////////////////////////////////////////////////////

UTraversalRootMotionModifier_Scale* UTraversalRootMotionModifier_Scale::AddRootMotionModifierScale(UTraversalMotionWarpComponent* InMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FVector InScale)
{
	if (ensureAlways(InMotionWarpingComp))
	{
		UTraversalRootMotionModifier_Scale* NewModifier = NewObject<UTraversalRootMotionModifier_Scale>(InMotionWarpingComp);
		NewModifier->Animation = InAnimation;
		NewModifier->StartTime = InStartTime;
		NewModifier->EndTime = InEndTime;
		NewModifier->Scale = InScale;

		InMotionWarpingComp->AddModifier(NewModifier);

		return NewModifier;
	}

	return nullptr;
}

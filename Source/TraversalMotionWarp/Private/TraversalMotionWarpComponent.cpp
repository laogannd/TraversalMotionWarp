// Copyright (c) 2026 DGOne. All Rights Reserved.

#include "TraversalMotionWarpComponent.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AnimNotifyState_TraversalMotionWarp.h"
#include "TraversalMotionWarpCharacterAdapter.h"
#include "TraversalMotionWarpSwitchOffCondition.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TraversalMotionWarpComponent)

DEFINE_LOG_CATEGORY(LogTraversalMotionWarp);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
TAutoConsoleVariable<int32> FTraversalMotionWarpCVars::CVarMotionWarpingDisable(TEXT("a.TraversalMotionWarp.Disable"), 0, TEXT("Disable Motion Warping"), ECVF_Cheat);
TAutoConsoleVariable<int32> FTraversalMotionWarpCVars::CVarMotionWarpingDebug(TEXT("a.TraversalMotionWarp.Debug"), 0, TEXT("0: Disable, 1: Only Log, 2: Only DrawDebug, 3: Log and DrawDebug"), ECVF_Cheat);
TAutoConsoleVariable<float> FTraversalMotionWarpCVars::CVarMotionWarpingDrawDebugDuration(TEXT("a.TraversalMotionWarp.DrawDebugLifeTime"), 1.f, TEXT("Time in seconds each draw debug persists.\nRequires 'a.TraversalMotionWarp.Debug 2'"), ECVF_Cheat);
TAutoConsoleVariable<int32> FTraversalMotionWarpCVars::CVarWarpedTargetDebug(TEXT("a.TraversalMotionWarp.Debug.Target"), false, TEXT("Shows warp target debug. 0 - disabled, 1 - enabled for selected actor, 2 - enabled for all actors"), ECVF_Cheat);
TAutoConsoleVariable<int32> FTraversalMotionWarpCVars::CVarWarpedSwitchOffConditionDebug(TEXT("a.TraversalMotionWarp.Debug.SwitchOffCondition"), false, TEXT("Shows switch off condition debug. 0 - disabled, 1 - enabled for selected actor, 2 - enabled for all actors"), ECVF_Cheat);
#endif

// UTraversalMotionWarpUtilities
///////////////////////////////////////////////////////////////////////

void UTraversalMotionWarpUtilities::ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose)
{
	OutPose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	FAnimExtractContext Context(static_cast<double>(Time), bExtractRootMotion);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(OutPose, Curve, Attributes);
	if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		AnimSequence->GetBonePose(AnimationPoseData, Context);
	}
	else if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		const FAnimTrack& AnimTrack = AnimMontage->SlotAnimTracks[0].AnimTrack;
		AnimTrack.GetAnimationPose(AnimationPoseData, Context);
	}
}

void UTraversalMotionWarpUtilities::ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose)
{
	FCompactPose Pose;
	ExtractLocalSpacePose(Animation, BoneContainer, Time, bExtractRootMotion, Pose);
	OutPose.InitPose(MoveTemp(Pose));
}

FTransform UTraversalMotionWarpUtilities::ExtractRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime)
{
	if (const UAnimMontage* Anim = Cast<UAnimMontage>(Animation))
	{
		// This is identical to UAnimMontage::ExtractRootMotionFromTrackRange and UAnimCompositeBase::ExtractRootMotionFromTrack but ignoring bEnableRootMotion
		// so we can extract root motion from the montage even if that flag is set to false in the AnimSequence(s)

		FRootMotionMovementParams AccumulatedRootMotionParams;

		if (Anim->SlotAnimTracks.Num() > 0)
		{
			const FAnimTrack& RootMotionAnimTrack = Anim->SlotAnimTracks[0].AnimTrack;

			TArray<FRootMotionExtractionStep> RootMotionExtractionSteps;
			RootMotionAnimTrack.GetRootMotionExtractionStepsForTrackRange(RootMotionExtractionSteps, StartTime, EndTime);

			for (const FRootMotionExtractionStep& CurStep : RootMotionExtractionSteps)
			{
				if (CurStep.AnimSequence)
				{
					AccumulatedRootMotionParams.Accumulate(CurStep.AnimSequence->ExtractRootMotionFromRange(CurStep.StartPosition, CurStep.EndPosition, FAnimExtractContext()));
				}
			}
		}

		return AccumulatedRootMotionParams.GetRootMotionTransform();
	}

	if (const UAnimSequence* Anim = Cast<UAnimSequence>(Animation))
	{
		return Anim->ExtractRootMotionFromRange(StartTime, EndTime, FAnimExtractContext());
	}

	return FTransform::Identity;
}

FTransform UTraversalMotionWarpUtilities::ExtractRootTransformFromAnimation(const UAnimSequenceBase* Animation, float Time)
{
	if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		if(const FAnimSegment* Segment = AnimMontage->SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(Time))
		{
			if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Segment->GetAnimReference()))
			{
				const float AnimSequenceTime = Segment->ConvertTrackPosToAnimPos(Time);
				return AnimSequence->ExtractRootTrackTransform(FAnimExtractContext(static_cast<double>(AnimSequenceTime)), nullptr);
			}	
		}
	}
	else if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		return AnimSequence->ExtractRootTrackTransform(FAnimExtractContext(static_cast<double>(Time)), nullptr);
	}

	return FTransform::Identity;
}

void UTraversalMotionWarpUtilities::GetMotionWarpingWindowsFromAnimation(const UAnimSequenceBase* Animation, TArray<FTraversalMotionWarpWindowData>& OutWindows)
{
	if(Animation)
	{
		OutWindows.Reset();

		for (int32 Idx = 0; Idx < Animation->Notifies.Num(); Idx++)
		{
			const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Idx];
			if (UAnimNotifyState_TraversalMotionWarp* Notify = Cast<UAnimNotifyState_TraversalMotionWarp>(NotifyEvent.NotifyStateClass))
			{
				FTraversalMotionWarpWindowData Data;
				Data.AnimNotify = Notify;
				Data.StartTime = NotifyEvent.GetTriggerTime();
				Data.EndTime = NotifyEvent.GetEndTriggerTime();
				OutWindows.Add(Data);
			}
		}
	}
}

void UTraversalMotionWarpUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(const UAnimSequenceBase* Animation, FName WarpTargetName, TArray<FTraversalMotionWarpWindowData>& OutWindows)
{
	if (Animation && WarpTargetName != NAME_None)
	{
		OutWindows.Reset();

		for (int32 Idx = 0; Idx < Animation->Notifies.Num(); Idx++)
		{
			const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Idx];
			if (UAnimNotifyState_TraversalMotionWarp* Notify = Cast<UAnimNotifyState_TraversalMotionWarp>(NotifyEvent.NotifyStateClass))
			{
				if (const UTraversalRootMotionModifier_Warp* Modifier = Cast<const UTraversalRootMotionModifier_Warp>(Notify->RootMotionModifier))
				{
					if(Modifier->WarpTargetName == WarpTargetName)
					{
						FTraversalMotionWarpWindowData Data;
						Data.AnimNotify = Notify;
						Data.StartTime = NotifyEvent.GetTriggerTime();
						Data.EndTime = NotifyEvent.GetEndTriggerTime();
						OutWindows.Add(Data);
					}
				}
			}
		}
	}
}


FTransform UTraversalMotionWarpUtilities::CalculateRootTransformRelativeToWarpPointAtTime(const ACharacter& Character, const UAnimSequenceBase* Animation, float Time, const FName& WarpPointBoneName)
{
	if (const USkeletalMeshComponent* Mesh = Character.GetMesh())
	{
		if (const UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			const FBoneContainer& FullBoneContainer = AnimInstance->GetRequiredBones();
			const int32 BoneIndex = FullBoneContainer.GetPoseBoneIndexForBoneName(WarpPointBoneName);
			if (BoneIndex != INDEX_NONE)
			{
				TArray<FBoneIndexType> RequiredBoneIndexArray = { 0, (FBoneIndexType)BoneIndex };
				FullBoneContainer.GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBoneIndexArray);

				FBoneContainer LimitedBoneContainer(RequiredBoneIndexArray, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *FullBoneContainer.GetAsset());

				FCSPose<FCompactPose> Pose;
				UTraversalMotionWarpUtilities::ExtractComponentSpacePose(Animation, LimitedBoneContainer, Time, false, Pose);

				// Inverse of mesh's relative rotation. Used to convert root and warp point in the animation from Y forward to X forward
				const FTransform MeshCompRelativeRotInverse = FTransform(Character.GetBaseRotationOffset().Inverse());

				const FTransform RootTransform = MeshCompRelativeRotInverse * Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(0));
				const FTransform WarpPointTransform = MeshCompRelativeRotInverse * Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(1));
				return RootTransform.GetRelativeTransform(WarpPointTransform);
			}
		}
	}

	return FTransform::Identity;
}


FTransform UTraversalMotionWarpUtilities::CalculateRootTransformRelativeToWarpPointAtTime(const UTraversalMotionWarpBaseAdapter& WarpingAdapter, const UAnimSequenceBase* Animation, float Time, const FName& WarpPointBoneName)
{
	if (const USkeletalMeshComponent* Mesh = WarpingAdapter.GetMesh())
	{
		if (const UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			const FBoneContainer& FullBoneContainer = AnimInstance->GetRequiredBones();
			const int32 BoneIndex = FullBoneContainer.GetPoseBoneIndexForBoneName(WarpPointBoneName);
			if (BoneIndex != INDEX_NONE)
			{
				TArray<FBoneIndexType> RequiredBoneIndexArray = { 0, (FBoneIndexType)BoneIndex };
				FullBoneContainer.GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBoneIndexArray);

				FBoneContainer LimitedBoneContainer(RequiredBoneIndexArray, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *FullBoneContainer.GetAsset());

				FCSPose<FCompactPose> Pose;
				UTraversalMotionWarpUtilities::ExtractComponentSpacePose(Animation, LimitedBoneContainer, Time, false, Pose);

				// Inverse of mesh's relative rotation. Used to convert root and warp point in the animation from Y forward to X forward
				const FTransform MeshCompRelativeRotInverse = FTransform(WarpingAdapter.GetBaseVisualRotationOffset().Inverse());

				const FTransform RootTransform = MeshCompRelativeRotInverse * Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(0));
				const FTransform WarpPointTransform = MeshCompRelativeRotInverse * Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(1));
				return RootTransform.GetRelativeTransform(WarpPointTransform);
			}
		}
	}

	return FTransform::Identity;
}


FTransform UTraversalMotionWarpUtilities::CalculateRootTransformRelativeToWarpPointAtTime(const ACharacter& Character, const UAnimSequenceBase* Animation, float Time, const FTransform& WarpPointTransform)
{
	// Inverse of mesh's relative rotation. Used to convert root and warp point in the animation from Y forward to X forward
	const FTransform MeshCompRelativeRotInverse = FTransform(Character.GetBaseRotationOffset().Inverse());
	const FTransform RootTransform = MeshCompRelativeRotInverse * UTraversalMotionWarpUtilities::ExtractRootTransformFromAnimation(Animation, Time);
	return RootTransform.GetRelativeTransform((MeshCompRelativeRotInverse * WarpPointTransform));
}


FTransform UTraversalMotionWarpUtilities::CalculateRootTransformRelativeToWarpPointAtTime(const UTraversalMotionWarpBaseAdapter& WarpingAdapter, const UAnimSequenceBase* Animation, float Time, const FTransform& WarpPointTransform)
{
	// Inverse of mesh's relative rotation. Used to convert root and warp point in the animation from Y forward to X forward
	const FTransform MeshCompRelativeRotInverse = FTransform(WarpingAdapter.GetBaseVisualRotationOffset().Inverse());
	const FTransform RootTransform = MeshCompRelativeRotInverse * UTraversalMotionWarpUtilities::ExtractRootTransformFromAnimation(Animation, Time);
	return RootTransform.GetRelativeTransform((MeshCompRelativeRotInverse * WarpPointTransform));
}

void UTraversalMotionWarpUtilities::ExtractBoneTransformFromAnimationAtTime(const UAnimInstance* AnimInstance, const UAnimSequenceBase* Animation, float Time, bool bExtractRootMotion, FName BoneName, bool bLocalSpace, FTransform& OutTransform)
{
	OutTransform = FTransform::Identity;

	if (AnimInstance && Animation)
	{
		FMemMark Mark(FMemStack::Get());

		const int32 BoneIndex = AnimInstance->GetRequiredBones().GetPoseBoneIndexForBoneName(BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			if (bLocalSpace)
			{
				FCompactPose Pose;
				ExtractLocalSpacePose(Animation, AnimInstance->GetRequiredBones(), Time, bExtractRootMotion, Pose);
				OutTransform = Pose[FCompactPoseBoneIndex(BoneIndex)];
			}
			else
			{
				FCSPose<FCompactPose> Pose;
				UTraversalMotionWarpUtilities::ExtractComponentSpacePose(Animation, AnimInstance->GetRequiredBones(), Time, bExtractRootMotion, Pose);
				OutTransform = Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex));
			}
		}
	}
}

// UTraversalMotionWarpComponent
///////////////////////////////////////////////////////////////////////

UTraversalMotionWarpComponent::UTraversalMotionWarpComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UTraversalMotionWarpComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_SimulatedOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UTraversalMotionWarpComponent, WarpTargets, Params);
}

void UTraversalMotionWarpComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Implicitly support Characters if no other adapter has already been setup
	if (GetOwnerAdapter() == nullptr)
	{
		if (ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner()))
		{
			UTraversalMotionWarpCharacterAdapter* CharacterAdapter = CreateOwnerAdapter<UTraversalMotionWarpCharacterAdapter>();
			CharacterAdapter->SetCharacter(CharacterOwner);
		}
	}
}

UTraversalMotionWarpBaseAdapter* UTraversalMotionWarpComponent::CreateOwnerAdapter(TSubclassOf<UTraversalMotionWarpBaseAdapter> AdapterClass)
{
	check(AdapterClass);	
	OwnerAdapter = NewObject<UTraversalMotionWarpBaseAdapter>(this, AdapterClass);
	OwnerAdapter->WarpLocalRootMotionDelegate.BindUObject(this, &UTraversalMotionWarpComponent::ProcessRootMotionPreConvertToWorld);

	return OwnerAdapter;
}

ACharacter* UTraversalMotionWarpComponent::GetCharacterOwner() const
{ 
	if (OwnerAdapter)
	{
		return Cast<ACharacter>(OwnerAdapter->GetActor());
	}

	return nullptr; 
}

bool UTraversalMotionWarpComponent::ContainsModifier(const UAnimSequenceBase* Animation, float StartTime, float EndTime) const
{
	return Modifiers.ContainsByPredicate([=](const UTraversalRootMotionModifier* Modifier)
		{
			return (Modifier->Animation == Animation && Modifier->StartTime == StartTime && Modifier->EndTime == EndTime);
		});
}

int32 UTraversalMotionWarpComponent::AddModifier(UTraversalRootMotionModifier* Modifier)
{
	if (ensureAlways(Modifier))
	{
		UE_LOG(LogTraversalMotionWarp, Verbose, TEXT("MotionWarping: RootMotionModifier added. NetMode: %d WorldTime: %f Char: %s Animation: %s [%f %f] [%f %f] Loc: %s Rot: %s"),
			GetWorld()->GetNetMode(), GetWorld()->GetTimeSeconds(), *GetNameSafe(GetOwner()), *GetNameSafe(Modifier->Animation.Get()), Modifier->StartTime, Modifier->EndTime, Modifier->PreviousPosition, Modifier->CurrentPosition,
			*GetOwner()->GetActorLocation().ToString(), *GetOwner()->GetActorRotation().ToCompactString());

		return Modifiers.Add(Modifier);
	}

	return INDEX_NONE;
}

void UTraversalMotionWarpComponent::DisableAllRootMotionModifiers()
{
	if (Modifiers.Num() > 0)
	{
		for (UTraversalRootMotionModifier* Modifier : Modifiers)
		{
			Modifier->SetState(ETraversalRootMotionModifierState::Disabled);
		}
	}
}

void UTraversalMotionWarpComponent::UpdateSwitchOffConditions()
{
	for (int32 i = WarpTargets.Num() - 1; i >= 0; --i)
	{
		FTraversalSwitchOffConditionData* SwitchOffConditionData = FindSwitchOffConditionData(WarpTargets[i].Name);
		if (!SwitchOffConditionData)
		{
			continue;
		}

		TArray<TObjectPtr<UTraversalMotionWarpSwitchOffCondition>>* Conditions = &SwitchOffConditionData->SwitchOffConditions;
		if (!Conditions)
		{
			continue;
		}

		bool bClearCondition = false;
		bool bPauseWarping = false;
		bool bPauseRootMotion = false;

		for (const UTraversalMotionWarpSwitchOffCondition* Condition : *Conditions)
		{
			if (!IsValid(Condition) || !Condition->IsConditionValid())
			{
				continue;
			}
			if (Condition->Check())
			{
				switch (Condition->GetEffect())
				{
				case ETraversalSwitchOffConditionEffect::CancelFollow:
					if (WarpTargets[i].bFollowComponent)
					{
						WarpTargets[i].bFollowComponent = false;
						WarpTargets[i].Location = Condition->GetTargetLocation();
						WarpTargets[i].Rotation = Condition->GetTargetRotation();
					}
					break;
				case ETraversalSwitchOffConditionEffect::CancelWarping:
					bClearCondition = true;
					break;
				case ETraversalSwitchOffConditionEffect::PauseWarping:
					bPauseWarping = true;
					break;
				case ETraversalSwitchOffConditionEffect::PauseRootMotion:
					bPauseRootMotion = true;
					break;
				default:
					checkNoEntry();
				}
			}
		}

		//remove finished and invalid conditions
		if (bClearCondition)
		{
			RemoveSwitchOffConditions(WarpTargets[i].Name);
			WarpTargets.RemoveAtSwap(i);
		}
		else
		{
			WarpTargets[i].bWarpingPaused = bPauseWarping;
			WarpTargets[i].bRootMotionPaused = bPauseRootMotion;
		}
	}
}

void UTraversalMotionWarpComponent::UpdateWithContext(const FTraversalMotionWarpUpdateContext& Context, float DeltaSeconds)
{
	UpdateSwitchOffConditions();

	if (Context.Animation.IsValid())
	{
		const UAnimSequenceBase* Animation = Context.Animation.Get();
		const float PreviousPosition = Context.PreviousPosition;
		const float CurrentPosition = Context.CurrentPosition;

		// Loop over notifies directly in the montage, looking for Motion Warping windows
		for (const FAnimNotifyEvent& NotifyEvent : Animation->Notifies)
		{
			const UAnimNotifyState_TraversalMotionWarp* MotionWarpingNotify = NotifyEvent.NotifyStateClass ? Cast<UAnimNotifyState_TraversalMotionWarp>(NotifyEvent.NotifyStateClass) : nullptr;
			if (MotionWarpingNotify)
			{
				if(MotionWarpingNotify->RootMotionModifier == nullptr)
				{
					UE_LOG(LogTraversalMotionWarp, Warning, TEXT("MotionWarpingComponent::Update. A motion warping window in %s doesn't have a valid root motion modifier!"), *GetNameSafe(Animation));
					continue;
				}

				const float StartTime = FMath::Clamp(NotifyEvent.GetTriggerTime(), 0.f, Animation->GetPlayLength());
				const float EndTime = FMath::Clamp(NotifyEvent.GetEndTriggerTime(), 0.f, Animation->GetPlayLength());

				if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
				{
					if (!ContainsModifier(Animation, StartTime, EndTime))
					{
						MotionWarpingNotify->OnBecomeRelevant(this, Animation, StartTime, EndTime);
					}
				}
			}
		}

		if(bSearchForWindowsInAnimsWithinMontages)
		{
			if(const UAnimMontage* Montage = Cast<const UAnimMontage>(Context.Animation.Get()))
			{
				// Same as before but scanning all animation within the montage
				for (int32 SlotIdx = 0; SlotIdx < Montage->SlotAnimTracks.Num(); SlotIdx++)
				{
					const FAnimTrack& AnimTrack = Montage->SlotAnimTracks[SlotIdx].AnimTrack;

					if (const FAnimSegment* AnimSegment = AnimTrack.GetSegmentAtTime(PreviousPosition))
					{
						if (const UAnimSequenceBase* AnimReference = AnimSegment->GetAnimReference())
						{
							for (const FAnimNotifyEvent& NotifyEvent : AnimReference->Notifies)
							{
								const UAnimNotifyState_TraversalMotionWarp* MotionWarpingNotify = NotifyEvent.NotifyStateClass ? Cast<UAnimNotifyState_TraversalMotionWarp>(NotifyEvent.NotifyStateClass) : nullptr;
								if (MotionWarpingNotify)
								{
									if (MotionWarpingNotify->RootMotionModifier == nullptr)
									{
										UE_LOG(LogTraversalMotionWarp, Warning, TEXT("MotionWarpingComponent::Update. A motion warping window in %s doesn't have a valid root motion modifier!"), *GetNameSafe(AnimReference));
										continue;
									}

									const float NotifyStartTime = FMath::Clamp(NotifyEvent.GetTriggerTime(), 0.f, AnimReference->GetPlayLength());
									const float NotifyEndTime = FMath::Clamp(NotifyEvent.GetEndTriggerTime(), 0.f, AnimReference->GetPlayLength());

									// Convert notify times from AnimSequence times to montage times
									const float StartTime = (NotifyStartTime - AnimSegment->AnimStartTime) + AnimSegment->StartPos;
									const float EndTime = (NotifyEndTime - AnimSegment->AnimStartTime) + AnimSegment->StartPos;

									if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
									{
										if (!ContainsModifier(Montage, StartTime, EndTime))
										{
											MotionWarpingNotify->OnBecomeRelevant(this, Montage, StartTime, EndTime);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	OnPreUpdate.Broadcast(this);

	// Update the state of all the modifiers
	if (Modifiers.Num() > 0)
	{
		for (UTraversalRootMotionModifier* Modifier : Modifiers)
		{
			Modifier->Update(Context);
		}

		// Remove the modifiers that has been marked for removal
		Modifiers.RemoveAll([this](const UTraversalRootMotionModifier* Modifier)
		{
			if (Modifier->GetState() == ETraversalRootMotionModifierState::MarkedForRemoval)
			{
				UE_LOG(LogTraversalMotionWarp, Verbose, TEXT("MotionWarping: RootMotionModifier removed. NetMode: %d WorldTime: %f Char: %s Animation: %s [%f %f] [%f %f] Loc: %s Rot: %s"),
					GetWorld()->GetNetMode(), GetWorld()->GetTimeSeconds(), *GetNameSafe(GetOwner()), *GetNameSafe(Modifier->Animation.Get()), Modifier->StartTime, Modifier->EndTime, Modifier->PreviousPosition, Modifier->CurrentPosition,
					*GetOwner()->GetActorLocation().ToString(), *GetOwner()->GetActorRotation().ToCompactString());

				return true;
			}

			return false;
		});
	}
}

FTransform UTraversalMotionWarpComponent::ProcessRootMotionPreConvertToWorld(const FTransform& InRootMotion, float DeltaSeconds, const FTraversalMotionWarpUpdateContext* InContext)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FTraversalMotionWarpCVars::CVarMotionWarpingDisable.GetValueOnGameThread() > 0)
	{
		return InRootMotion;
	}
#endif
	if (!InContext)
	{
		return InRootMotion;
	}

	// Check for warping windows and update modifier states
	UpdateWithContext(*InContext, DeltaSeconds);

	FTransform FinalRootMotion = InRootMotion;

	// Apply Local Space Modifiers
	for (UTraversalRootMotionModifier* Modifier : Modifiers)
	{
		if (Modifier->GetState() == ETraversalRootMotionModifierState::Active)
		{
			FinalRootMotion = Modifier->ProcessRootMotion(FinalRootMotion, DeltaSeconds);
		}
	}

	// Sweep-clamp the warped motion delta against collision geometry
	if (bClampWarpedMotionToCollision && OwnerAdapter && !FinalRootMotion.GetTranslation().IsNearlyZero())
	{
		if (USkeletalMeshComponent* Mesh = OwnerAdapter->GetMesh())
		{
			// Convert local-space warped delta to world-space
			const FVector WorldDelta = Mesh->ConvertLocalRootMotionToWorld(FTransform(FinalRootMotion.GetTranslation())).GetTranslation();

			if (!WorldDelta.IsNearlyZero())
			{
				const FVector CurrentFeetLocation = OwnerAdapter->GetVisualRootLocation();
				const FVector DesiredFeetLocation = CurrentFeetLocation + WorldDelta;

				FHitResult HitResult;
				if (!OwnerAdapter->SweepTestMovePathShrunk(CurrentFeetLocation, DesiredFeetLocation, CollisionClampShrinkFactor, HitResult))
				{
					// Clamp: scale the local-space translation by the hit fraction
					// Use a small pullback (1cm) to avoid ending exactly at the surface
					const float ClampedFraction = FMath::Max(HitResult.Time - 0.01f, 0.f);
					FinalRootMotion.SetTranslation(FinalRootMotion.GetTranslation() * ClampedFraction);

					UE_LOG(LogTraversalMotionWarp, Verbose,
						TEXT("MotionWarping: Warped motion clamped by collision with %s at %.1f%% of delta"),
						*GetNameSafe(HitResult.GetActor()), HitResult.Time * 100.f);
				}
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 DebugLevel = FTraversalMotionWarpCVars::CVarMotionWarpingDebug.GetValueOnGameThread();
	if (DebugLevel >= 2 && OwnerAdapter)
	{
		const float DrawDebugDuration = FTraversalMotionWarpCVars::CVarMotionWarpingDrawDebugDuration.GetValueOnGameThread();
		const float PointSize = 7.f;
		const FVector ActorFeetLocation = OwnerAdapter->GetVisualRootLocation();
		if (Modifiers.Num() > 0)
		{
			if (!OriginalRootMotionAccum.IsSet())
			{
				OriginalRootMotionAccum = ActorFeetLocation;
				WarpedRootMotionAccum = ActorFeetLocation;
			}
			
			OriginalRootMotionAccum = OriginalRootMotionAccum.GetValue() + (OwnerAdapter->GetMesh()->ConvertLocalRootMotionToWorld(FTransform(InRootMotion.GetLocation()))).GetLocation();
			WarpedRootMotionAccum = WarpedRootMotionAccum.GetValue() + (OwnerAdapter->GetMesh()->ConvertLocalRootMotionToWorld(FTransform(FinalRootMotion.GetLocation()))).GetLocation();

			DrawDebugPoint(GetWorld(), OriginalRootMotionAccum.GetValue(), PointSize, FColor::Red, false, DrawDebugDuration, 0);
			DrawDebugPoint(GetWorld(), WarpedRootMotionAccum.GetValue(), PointSize, FColor::Green, false, DrawDebugDuration, 0);
		}
		else
		{
			OriginalRootMotionAccum.Reset();
			WarpedRootMotionAccum.Reset();
		}

		DrawDebugPoint(GetWorld(), ActorFeetLocation, PointSize, FColor::Blue, false, DrawDebugDuration, 0);
	}

	const int32 DebugValSwitchOffCondition = FTraversalMotionWarpCVars::CVarWarpedSwitchOffConditionDebug->GetInt();

	const bool bDebugSwitchOffCondition = (DebugValSwitchOffCondition == 1 && GetOwner()->IsSelected()) || DebugValSwitchOffCondition == 2;

	const int32 DebugValTarget = FTraversalMotionWarpCVars::CVarWarpedTargetDebug->GetInt();
	const bool bDebugTarget = (DebugValTarget == 1 && GetOwner()->IsSelected()) || DebugValTarget == 2;

	const FVector ActorLocation = GetOwner()->GetActorLocation();
	FVector TextLocation = ActorLocation;
	constexpr float VerticalTextOffset = -10.0f;

	TArray<UTraversalRootMotionModifier_Warp*> WarpModifiers;
	for (UTraversalRootMotionModifier* Modifier : Modifiers)
	{
		if (Modifier->GetState() == ETraversalRootMotionModifierState::Active)
		{
			if (UTraversalRootMotionModifier_Warp* WarpModifier = Cast<UTraversalRootMotionModifier_Warp>(Modifier))
			{
				WarpModifiers.Add(WarpModifier);
			}
		}
	}

	for (int32 i = 0; i < WarpTargets.Num(); ++i)
	{
		const FTraversalMotionWarpTarget& WarpingTarget = WarpTargets[i];

		// Skip inactive warp targets
		if (!WarpModifiers.ContainsByPredicate([&WarpingTarget](const UTraversalRootMotionModifier_Warp* Mod) {return Mod->WarpTargetName == WarpingTarget.Name; }))
		{
			continue;
		}

		// Cycle between colors for better readability
		FColor WarpTargetColor = (i % 2) != 0 ? FColor(21, 76, 190) : FColor::Green;

		if (bDebugTarget)
		{
			FVector TargetLocation = WarpingTarget.GetTargetTrasform().GetLocation();
			DrawDebugSphere(GetWorld(), TargetLocation, 5.0f, 16, WarpTargetColor, false);

			DrawDebugLine(GetWorld(), TextLocation, WarpingTarget.GetTargetTrasform().GetLocation(), WarpTargetColor);

			FString DebugText = FString::Printf(TEXT("Warp target name: %s, is dynamic: %s"), *WarpingTarget.Name.ToString(), WarpingTarget.bFollowComponent ? TEXT("True") : TEXT("False"));
			DrawDebugString(GetWorld(), TextLocation, DebugText, nullptr, WarpTargetColor, 0.f, true);
			TextLocation.Z += VerticalTextOffset;
		}

		if (bDebugSwitchOffCondition)
		{
			if (FTraversalSwitchOffConditionData* ConditionData = FindSwitchOffConditionData(WarpingTarget.Name))
			{
				DrawDebugString(GetWorld(), TextLocation, TEXT("Switch off conditions:"), nullptr, FColor::White, 0.f, true);
				TextLocation.Z += VerticalTextOffset;

				for (const UTraversalMotionWarpSwitchOffCondition* Condition : ConditionData->SwitchOffConditions)
				{
					const bool bConditionTrue = Condition->Check();
					FColor SwitchOffConditionTextColor = bConditionTrue ? FColor::Red : FColor::Yellow;
					DrawDebugString(GetWorld(),
						TextLocation,
						FString::Printf(TEXT("%s - %s"), *Condition->ExtraDebugInfo(), bConditionTrue ? TEXT("TRUE") : TEXT("FALSE")),
						nullptr,
						SwitchOffConditionTextColor,
						0.f,
						true);
					TextLocation.Z += VerticalTextOffset;
				}
			}
		}

		TextLocation.Z += VerticalTextOffset;
	}
#endif

	return FinalRootMotion;
}


bool UTraversalMotionWarpComponent::FindAndUpdateWarpTarget(const FTraversalMotionWarpTarget& WarpTarget)
{
	for (int32 Idx = 0; Idx < WarpTargets.Num(); Idx++)
	{
		if (WarpTargets[Idx].Name == WarpTarget.Name)
		{
			WarpTargets[Idx] = WarpTarget;
			return true;
		}
	}

	return false;
}

void UTraversalMotionWarpComponent::AddOrUpdateWarpTarget(const FTraversalMotionWarpTarget& WarpTarget)
{
	if (WarpTarget.Name != NAME_None)
	{
		// if we did not find the target, add it
		if (!FindAndUpdateWarpTarget(WarpTarget))
		{
			int32 Idx = WarpTargets.Add(WarpTarget);

			if (FTraversalSwitchOffConditionData* SwitchOffConditionData = FindSwitchOffConditionData(WarpTarget.Name))
			{
				SwitchOffConditionData->SetMotionWarpingTarget(&WarpTargets[Idx]);
			}
		}

		MARK_PROPERTY_DIRTY_FROM_NAME(UTraversalMotionWarpComponent, WarpTargets, this);
	}
}

int32 UTraversalMotionWarpComponent::RemoveWarpTarget(FName WarpTargetName)
{
	const int32 NumRemoved = WarpTargets.RemoveAll([&WarpTargetName](const FTraversalMotionWarpTarget& WarpTarget) { return WarpTarget.Name == WarpTargetName; });
	
	if(NumRemoved > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UTraversalMotionWarpComponent, WarpTargets, this);
	}

	RemoveSwitchOffConditions(WarpTargetName);

	return NumRemoved;
}

int32 UTraversalMotionWarpComponent::RemoveWarpTargets(const TArray<FName>& WarpTargetNames)
{
	int32 NumRemoved = 0;
	for (const FName& WarpTargetName : WarpTargetNames)
	{
		NumRemoved += RemoveWarpTarget(WarpTargetName);		
	}

	if (NumRemoved > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UTraversalMotionWarpComponent, WarpTargets, this);
	}

	return NumRemoved;
}

void UTraversalMotionWarpComponent::AddSwitchOffCondition(FName WarpTargetName, UTraversalMotionWarpSwitchOffCondition* Condition)
{
	if (IsValid(Condition))
	{
		if (const FTraversalMotionWarpTarget* MotionWarpingTarget = FindWarpTarget(WarpTargetName))
		{
			Condition->SetMotionWarpingTarget(MotionWarpingTarget);
		}
		
		if (FTraversalSwitchOffConditionData* SwitchOffConditionData = FindSwitchOffConditionData(WarpTargetName))
		{
			SwitchOffConditionData->SwitchOffConditions.Add(Condition);
		}
		else
		{
			SwitchOffConditions.Add(FTraversalSwitchOffConditionData(WarpTargetName, Condition));
		}
	}
	else
	{
		UE_LOG(LogTraversalMotionWarp, Error, TEXT("Trying to add invalid switch off condition"))
	}
}

void UTraversalMotionWarpComponent::RemoveSwitchOffConditions(FName WarpTargetName)
{
	const int32 Index = SwitchOffConditions.IndexOfByPredicate([WarpTargetName](FTraversalSwitchOffConditionData Condition)
	{
		return Condition.WarpTargetName == WarpTargetName;
	});

	if (Index != INDEX_NONE)
	{
		SwitchOffConditions.RemoveAtSwap(Index);
	}	
}

FTraversalSwitchOffConditionData* UTraversalMotionWarpComponent::FindSwitchOffConditionData(FName WarpTargetName)
{
	return SwitchOffConditions.FindByPredicate([WarpTargetName](FTraversalSwitchOffConditionData Condition)
	{
		return Condition.WarpTargetName == WarpTargetName;
	});
}

int32 UTraversalMotionWarpComponent::RemoveAllWarpTargets()
{
	const int32 NumRemoved = WarpTargets.Num();

	for (const FTraversalMotionWarpTarget& Target : WarpTargets)
	{
		RemoveSwitchOffConditions(Target.Name);
	}
	
	WarpTargets.Reset();

	if (NumRemoved > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UTraversalMotionWarpComponent, WarpTargets, this);
	}

	return NumRemoved;
}

void UTraversalMotionWarpComponent::AddOrUpdateWarpTargetFromTransform(FName WarpTargetName, FTransform TargetTransform)
{
	AddOrUpdateWarpTarget(FTraversalMotionWarpTarget(WarpTargetName, TargetTransform));
}

void UTraversalMotionWarpComponent::AddOrUpdateWarpTargetFromComponent(FName WarpTargetName, const USceneComponent* Component, FName BoneName, bool bFollowComponent, FVector LocationOffset, FRotator RotationOffset)
{
	AddOrUpdateWarpTargetFromComponent(WarpTargetName, Component, BoneName, bFollowComponent, ETraversalWarpTargetLocationOffsetDirection::TargetsForwardVector, LocationOffset, RotationOffset);
}

void UTraversalMotionWarpComponent::AddOrUpdateWarpTargetFromComponent(FName WarpTargetName, const USceneComponent* Component, FName BoneName, bool bFollowComponent, ETraversalWarpTargetLocationOffsetDirection LocationOffsetDirection, FVector LocationOffset, FRotator RotationOffset)
{
	if (Component == nullptr)
	{
		UE_LOG(LogTraversalMotionWarp, Warning, TEXT("AddOrUpdateWarpTargetFromComponent has failed!. Reason: Invalid Component"));
		return;
	}

	AddOrUpdateWarpTarget(FTraversalMotionWarpTarget(WarpTargetName, Component, BoneName, bFollowComponent, LocationOffsetDirection, GetOwner(), LocationOffset, RotationOffset));
}

UTraversalRootMotionModifier* UTraversalMotionWarpComponent::AddModifierFromTemplate(UTraversalRootMotionModifier* Template, const UAnimSequenceBase* Animation, float StartTime, float EndTime)
{
	if (ensureAlways(Template))
	{
		FObjectDuplicationParameters Params(Template, this);
		UTraversalRootMotionModifier* NewRootMotionModifier = CastChecked<UTraversalRootMotionModifier>(StaticDuplicateObjectEx(Params));
		
		NewRootMotionModifier->Animation = Animation;
		NewRootMotionModifier->StartTime = StartTime;
		NewRootMotionModifier->EndTime = EndTime;

		AddModifier(NewRootMotionModifier);

		return NewRootMotionModifier;
	}

	return nullptr;
}

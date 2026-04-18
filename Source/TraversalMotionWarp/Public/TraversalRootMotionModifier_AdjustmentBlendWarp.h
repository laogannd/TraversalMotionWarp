// Copyright (c) 2026 DGOne. All Rights Reserved.

#pragma once

#include "Animation/AnimSequence.h"
#include "TraversalRootMotionModifier.h"
#include "TraversalRootMotionModifier_AdjustmentBlendWarp.generated.h"

#define UE_API TRAVERSALMOTIONWARP_API

class ACharacter;
template <class PoseType> struct FCSPose;

USTRUCT()
struct FTraversalMotionDeltaTrack
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTransform> BoneTransformTrack;

	UPROPERTY()
	TArray<FVector> DeltaTranslationTrack;

	UPROPERTY()
	TArray<FRotator> DeltaRotationTrack;

	UPROPERTY()
	FVector TotalTranslation = FVector::ZeroVector;

	UPROPERTY()
	FRotator TotalRotation = FRotator::ZeroRotator;
};

USTRUCT()
struct FTraversalMotionDeltaTrackContainer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTraversalMotionDeltaTrack> Tracks;

	void Init(int32 InNumTracks)
	{
		Tracks.Reserve(InNumTracks);
	}
};

// EXPERIMENTAL: Marked with 'hidedropdown' to prevent it from showing up in the UI since it is not ready for production.
UCLASS(MinimalAPI, hidedropdown, meta = (DisplayName = "Adjustment Blend Warp"))
class UTraversalRootMotionModifier_AdjustmentBlendWarp : public UTraversalRootMotionModifier_Warp
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bWarpIKBones = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TArray<FName> IKBones;

	UE_API UTraversalRootMotionModifier_AdjustmentBlendWarp(const FObjectInitializer& ObjectInitializer);

	UE_API virtual void OnTargetTransformChanged() override;
	UE_API virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;

	UE_API void GetIKBoneTransformAndAlpha(FName BoneName, FTransform& OutTransform, float& OutAlpha) const;

	//UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static UE_API UTraversalRootMotionModifier_AdjustmentBlendWarp* AddRootMotionModifierAdjustmentBlendWarp(UTraversalMotionWarpComponent* InMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime,
		FName InWarpTargetName, ETraversalWarpPointAnimProvider InWarpPointAnimProvider, FTransform InWarpPointAnimTransform, FName InWarpPointAnimBoneName,
		bool bInWarpTranslation, bool bInIgnoreZAxis, bool bInWarpRotation, bool bInWarpIKBones, const TArray<FName>& InIKBones, bool bInIgnoreRootMotionRotation = false);

	//UFUNCTION(BlueprintPure, Category = "Motion Warping")
	static UE_API void GetAdjustmentBlendIKBoneTransformAndAlpha(ACharacter* Character, FName BoneName, FTransform& OutTransform, float& OutAlpha);

protected:

	UPROPERTY()
	FTransform CachedMeshTransform;

	UPROPERTY()
	FTransform CachedMeshRelativeTransform;

	UPROPERTY()
	FTransform CachedRootMotion;

	UPROPERTY()
	FAnimSequenceTrackContainer Result;

	UE_API void PrecomputeWarpedTracks();

	UE_API FTransform ExtractWarpedRootMotion() const;

	UE_API void ExtractBoneTransformAtTime(FTransform& OutTransform, const FName& BoneName, float Time) const;
	UE_API void ExtractBoneTransformAtTime(FTransform& OutTransform, int32 TrackIndex, float Time) const;
	UE_API void ExtractBoneTransformAtFrame(FTransform& OutTransform, int32 TrackIndex, int32 Frame) const;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_API void DrawDebugWarpedTracks(float DrawDuration) const;
#endif

	static UE_API void ExtractMotionDeltaFromRange(const FBoneContainer& BoneContainer, const UAnimSequenceBase* Animation, float StartTime, float EndTime, float SampleRate, FTraversalMotionDeltaTrackContainer& OutMotionDeltaTracks);

	static UE_API void AdjustmentBlendWarp(const FBoneContainer& BoneContainer, const FCSPose<FCompactPose>& AdditivePose, const FTraversalMotionDeltaTrackContainer& MotionDeltaTracks, FAnimSequenceTrackContainer& Output);
};

#undef UE_API

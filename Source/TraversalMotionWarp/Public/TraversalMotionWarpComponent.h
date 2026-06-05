// Copyright (c) 2026 DGOne. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "TraversalRootMotionModifier.h"
#include "TraversalMotionWarpComponent.generated.h"

#define UE_API TRAVERSALMOTIONWARP_API

class UTraversalMotionWarpSwitchOffCondition;
struct FTraversalSwitchOffConditionData;
template <class T> class TAutoConsoleVariable;

class ACharacter;
class UAnimInstance;
class UAnimSequenceBase;
class UCharacterMovementComponent;
class UTraversalMotionWarpBaseAdapter;
class UTraversalMotionWarpComponent;
class UAnimNotifyState_TraversalMotionWarp;
struct FCompactPose;
template<class PoseType> struct FCSPose;

DECLARE_LOG_CATEGORY_EXTERN(LogTraversalMotionWarp, Log, All);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
struct FTraversalMotionWarpCVars
{
	static UE_API TAutoConsoleVariable<int32> CVarMotionWarpingDisable;
	static UE_API TAutoConsoleVariable<int32> CVarMotionWarpingDebug;
	static UE_API TAutoConsoleVariable<float> CVarMotionWarpingDrawDebugDuration;
	static UE_API TAutoConsoleVariable<int32> CVarWarpedTargetDebug;
	static UE_API TAutoConsoleVariable<int32> CVarWarpedSwitchOffConditionDebug;
};
#endif

USTRUCT(BlueprintType)
struct FTraversalMotionWarpWindowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	TObjectPtr<UAnimNotifyState_TraversalMotionWarp> AnimNotify = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	float StartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	float EndTime = 0.f;
};


UCLASS(MinimalAPI)
class UTraversalMotionWarpUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Extract bone pose in local space for all bones in BoneContainer. If Animation is a Montage the pose is extracted from the first track */
	static UE_API void ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose);

	/** Extract bone pose in component space for all bones in BoneContainer. If Animation is a Montage the pose is extracted from the first track */
	static UE_API void ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose);

	/** Extract Root Motion transform from a contiguous position range */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static UE_API FTransform ExtractRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime);

	/** Extract root bone transform at a given time */
	static UE_API FTransform ExtractRootTransformFromAnimation(const UAnimSequenceBase* Animation, float Time);

	/** @return All the MotionWarping windows within the supplied animation */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static UE_API void GetMotionWarpingWindowsFromAnimation(const UAnimSequenceBase* Animation, TArray<FTraversalMotionWarpWindowData>& OutWindows);

	/** @return All the MotionWarping windows within the supplied animation for a given Warp Target */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static UE_API void GetMotionWarpingWindowsForWarpTargetFromAnimation(const UAnimSequenceBase* Animation, FName WarpTargetName, TArray<FTraversalMotionWarpWindowData>& OutWindows);

	/** @return root transform relative to the warp point bone at the supplied time */
	static UE_API FTransform CalculateRootTransformRelativeToWarpPointAtTime(const ACharacter& Character, const UAnimSequenceBase* Animation, float Time, const FName& WarpPointBoneName);
	static UE_API FTransform CalculateRootTransformRelativeToWarpPointAtTime(const UTraversalMotionWarpBaseAdapter& WarpingAdapter, const UAnimSequenceBase* Animation, float Time, const FName& WarpPointBoneName);

	/** @return root transform relative to the warp point transform at the supplied time */
	static UE_API FTransform CalculateRootTransformRelativeToWarpPointAtTime(const ACharacter& Character, const UAnimSequenceBase* Animation, float Time, const FTransform& WarpPointTransform);
	static UE_API FTransform CalculateRootTransformRelativeToWarpPointAtTime(const UTraversalMotionWarpBaseAdapter& WarpingAdapter, const UAnimSequenceBase* Animation, float Time, const FTransform& WarpPointTransform);

	/** Extract bone transform from animation at a given time */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static UE_API void ExtractBoneTransformFromAnimationAtTime(const UAnimInstance* AnimInstance, const UAnimSequenceBase* Animation, float Time, bool bExtractRootMotion, FName BoneName, bool bLocalSpace, FTransform& OutTransform);
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTraversalMotionWarpPreUpdate, class UTraversalMotionWarpComponent*, MotionWarpingComp);

UCLASS(MinimalAPI, ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class UTraversalMotionWarpComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Whether to look inside animations within montage when looking for warping windows */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bSearchForWindowsInAnimsWithinMontages = false;

	/** Whether to sweep-test the warped root motion delta each frame and clamp it on collision.
	 *  This prevents the character from being pushed through thin walls or geometry by warped motion.
	 *  When a blocking hit is detected, the translation is shortened to stop at the impact point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Collision")
	bool bClampWarpedMotionToCollision = false;

	/** How much to shrink the collision shape when sweep-testing warped motion (0.0 = full size, 1.0 = zero size).
	 *  Higher values allow the character to get closer to walls before clamping kicks in.
	 *  For example, 0.2 shrinks the capsule radius/half-height by 20%, giving ~20% tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Collision", meta = (EditCondition = "bClampWarpedMotionToCollision", ClampMin = "0.0", ClampMax = "0.95", UIMin = "0.0", UIMax = "0.5"))
	float CollisionClampShrinkFactor = 0.f;

	/** Event called before Root Motion Modifiers are updated */
	UPROPERTY(BlueprintAssignable, Category = "Motion Warping")
	FTraversalMotionWarpPreUpdate OnPreUpdate;

	UE_API UTraversalMotionWarpComponent(const FObjectInitializer& ObjectInitializer);

	UE_API virtual void InitializeComponent() override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

	/** Set a new adapter of a particular type */
	UE_API UTraversalMotionWarpBaseAdapter* CreateOwnerAdapter(TSubclassOf<UTraversalMotionWarpBaseAdapter> AdapterClass);

	template <class T>
	T* CreateOwnerAdapter()
	{
		static_assert(TPointerIsConvertibleFromTo<T, const UTraversalMotionWarpBaseAdapter>::Value, "'T' template parameter to CreateOwnerAdapter must be derived from UTraversalMotionWarpBaseAdapter");
		return CastChecked<T>(CreateOwnerAdapter(T::StaticClass()));
	}

	/** Get the current adapter to the owner */
	UTraversalMotionWarpBaseAdapter* GetOwnerAdapter() const { return OwnerAdapter; }

	/** Gets the Character this component belongs to. Returns null if not owned by a Character actor. */
	UE_DEPRECATED(5.5, "Motion Warping is no longer limited to Character actors. Use GetOwnerAdapter()->GetActor() instead.")
	UE_API ACharacter* GetCharacterOwner() const;

	/** Returns the list of root motion modifiers */
	inline const TArray<UTraversalRootMotionModifier*>& GetModifiers() const { return Modifiers; }

	/** Check if we contain a RootMotionModifier for the supplied animation and time range */
	UE_API bool ContainsModifier(const UAnimSequenceBase* Animation, float StartTime, float EndTime) const;

	/** Add a new modifier */
	UE_API int32 AddModifier(UTraversalRootMotionModifier* Modifier);

	/** Mark all the modifiers as Disable */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	UE_API void DisableAllRootMotionModifiers();

	UE_API UTraversalRootMotionModifier* AddModifierFromTemplate(UTraversalRootMotionModifier* Template, const UAnimSequenceBase* Animation, float StartTime, float EndTime);

	/** Find the target associated with a specified name */
	inline const FTraversalMotionWarpTarget* FindWarpTarget(const FName& WarpTargetName) const 
	{ 
		return WarpTargets.FindByPredicate([&WarpTargetName](const FTraversalMotionWarpTarget& WarpTarget){ return WarpTarget.Name == WarpTargetName; });
	}

	/** Adds or update a warp target */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	UE_API void AddOrUpdateWarpTarget(const FTraversalMotionWarpTarget& WarpTarget);

	/** 
	 * Create and adds or update a target associated with a specified name 
	 * @param WarpTargetName Warp target identifier
	 * @param TargetTransform Transform used to set the location and rotation for the warp target
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	UE_API void AddOrUpdateWarpTargetFromTransform(FName WarpTargetName, FTransform TargetTransform);

	/**
	 * Create and adds or update a target associated with a specified name
	 * @param WarpTargetName Warp target identifier
	 * @param Component Scene Component used to get the target transform
	 * @param BoneName Optional bone or socket in the component used to get the target transform. 
	 * @param bFollowComponent Whether the target transform should update while the warping is active. Useful for tracking moving targets.
	 *		  Note that this will be one frame off if our owner ticks before the target actor. Add tick pre-requisites to avoid this.
	 * @param LocationOffsetDirection Direction of LocationOffset
	 * @param LocationOffset Optional translation offset to apply to the transform we get from the component
	 * @param RotationOffset Optional rotation offset to apply to the transform we get from the component
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	UE_API void AddOrUpdateWarpTargetFromComponent(FName WarpTargetName, const USceneComponent* Component, FName BoneName, bool bFollowComponent, ETraversalWarpTargetLocationOffsetDirection LocationOffsetDirection = ETraversalWarpTargetLocationOffsetDirection::TargetsForwardVector, FVector LocationOffset = FVector::ZeroVector, FRotator RotationOffset = FRotator::ZeroRotator);
	
	// @TODO: Deprecate this.
	UE_API void AddOrUpdateWarpTargetFromComponent(FName WarpTargetName, const USceneComponent* Component, FName BoneName, bool bFollowComponent, FVector LocationOffset = FVector::ZeroVector, FRotator RotationOffset = FRotator::ZeroRotator);

	/**
	 * Create and adds or update a target associated with a specified name
	 * @param WarpTargetName Warp target identifier
	 * @param TargetLocation Location for the warp target
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void AddOrUpdateWarpTargetFromLocation(FName WarpTargetName, FVector TargetLocation)
	{
		AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(TargetLocation));
	}

	/**
	 * Create and adds or update a target associated with a specified name
	 * @param WarpTargetName Warp target identifier
	 * @param TargetLocation Location for the warp target
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void AddOrUpdateWarpTargetFromLocationAndRotation(FName WarpTargetName, FVector TargetLocation, FRotator TargetRotation)
	{
		AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(TargetRotation, TargetLocation));
	}

	/** Removes the warp target associated with the specified key  */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	UE_API int32 RemoveWarpTarget(FName WarpTargetName);

	/** Removes all warp targets */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	UE_API int32 RemoveAllWarpTargets();

	/** Removes multiple warp targets */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	UE_API int32 RemoveWarpTargets(const TArray<FName>& WarpTargetNames);

	UFUNCTION(BlueprintCallable, Category="Motion Warping|Experimental")
	UE_API void AddSwitchOffCondition(FName WarpTargetName, UTraversalMotionWarpSwitchOffCondition* Condition);
	
	UE_API void RemoveSwitchOffConditions(FName WarpTargetName);
	
	UE_API FTraversalSwitchOffConditionData* FindSwitchOffConditionData(FName WarpTargetName);

protected:

	/** Adapter that connects motion warping to an owner */
	UPROPERTY(Transient)
	TObjectPtr<UTraversalMotionWarpBaseAdapter> OwnerAdapter;
	
	/** List of root motion modifiers */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTraversalRootMotionModifier>> Modifiers;

	UPROPERTY(Transient, Replicated)
	TArray<FTraversalMotionWarpTarget> WarpTargets;
		
	UPROPERTY(Transient, Experimental)
    TArray<FTraversalSwitchOffConditionData> SwitchOffConditions;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TOptional<FVector> OriginalRootMotionAccum;
	TOptional<FVector> WarpedRootMotionAccum;
#endif

	UE_API void UpdateSwitchOffConditions();

	UE_API void UpdateWithContext(const FTraversalMotionWarpUpdateContext& Context, float DeltaSeconds);

	UE_API bool FindAndUpdateWarpTarget(const FTraversalMotionWarpTarget& WarpTarget);

	UE_API FTransform ProcessRootMotionPreConvertToWorld(const FTransform& InRootMotion, float DeltaSeconds, const FTraversalMotionWarpUpdateContext* InContext=nullptr); // callback with optional context
};

#undef UE_API

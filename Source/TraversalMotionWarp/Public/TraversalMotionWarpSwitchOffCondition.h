// Copyright (c) 2026 DGOne. All Rights Reserved.

#pragma once

#include "TraversalRootMotionModifier.h"
#include "TraversalMotionWarpSwitchOffCondition.generated.h"

#define UE_API TRAVERSALMOTIONWARP_API

class UTraversalMotionWarpSwitchOffCondition;
struct FTraversalMotionWarpTarget;

USTRUCT(BlueprintType, Experimental)
struct FTraversalSwitchOffConditionData
{
	GENERATED_BODY()

	FTraversalSwitchOffConditionData()
		: WarpTargetName(NAME_None), SwitchOffConditions(TArray<UTraversalMotionWarpSwitchOffCondition*>())
	{}
	
	FTraversalSwitchOffConditionData(FName InWarpTargetName, UTraversalMotionWarpSwitchOffCondition* InSwitchOffCondition)
		: WarpTargetName(InWarpTargetName)
	{
		SwitchOffConditions.Add(InSwitchOffCondition);
	}

	FTraversalSwitchOffConditionData(FName InWarpTargetName)
		: WarpTargetName(InWarpTargetName), SwitchOffConditions(TArray<UTraversalMotionWarpSwitchOffCondition*>())
	{}
	
	UPROPERTY(BlueprintReadWrite, Category = "Defaults")
	FName WarpTargetName;

	UPROPERTY(BlueprintReadWrite, Category = "Defaults")
	TArray<TObjectPtr<UTraversalMotionWarpSwitchOffCondition>> SwitchOffConditions;

	UE_API void SetMotionWarpingTarget(const FTraversalMotionWarpTarget* MotionWarpingTarget);
};

/**Result of switch off condition.*/
UENUM(BlueprintType, Category = "TraversalMotionWarp", meta = (Experimental))
enum class ETraversalSwitchOffConditionEffect : uint8
{
	/**Changes associated motion warping target from component to a location of this component
	* in the frame in which this switch off condition appeared */
	CancelFollow UMETA(DisplayName = "Cancel Follow Component"),

	/**Removes associated motion warping target*/
	CancelWarping UMETA(DisplayName = "Cancel Warping"),

	/**During time slot in the animation, where switch off condition is true, only play root motion, without warping*/
	PauseWarping UMETA(DisplayName = "Pause Warping"),

	/**During time slot in the animation, where switch off condition is true, play the animation in place*/
	PauseRootMotion UMETA(DisplayName = "Pause Root Motion")
};

UCLASS(MinimalAPI, Abstract, EditInlineNew, Experimental, ClassGroup = TraversalMotionWarp)
class UTraversalMotionWarpSwitchOffCondition : public UObject
{
	GENERATED_BODY()

public:
	/** If set to false, switch off condition will use target actor location */
	UPROPERTY(EditAnywhere, Category = "TraversalMotionWarp")
	bool bUseWarpTargetAsTargetLocation = true;

	UPROPERTY(EditAnywhere, Category = "TraversalMotionWarp")
	ETraversalSwitchOffConditionEffect Effect = ETraversalSwitchOffConditionEffect::CancelWarping;
	
	/**
	 *	Initialize switch off condition
	 *	@param InOwnerActor Actor with MotionWarpingComponent, owning corresponding WarpingTarget
	 *	@param InTargetActor Target actor used for calculating switch off condition result
	 */
	ETraversalSwitchOffConditionEffect GetEffect() const { return Effect; }
	UE_API bool Check() const;
	
	/**
	 * If bUseWarpTargetAsTargetLocation is true, this will return target FTraversalMotionWarpTarget location.
	 * If bUseWarpTargetAsTargetLocation is false, this will return target actor location.
	 */
	UE_API FVector GetTargetLocation() const;

	/**
	* If bUseWarpTargetAsTargetLocation is true, this will return target FTraversalMotionWarpTarget rotation.
	* If bUseWarpTargetAsTargetLocation is false, this will return target actor rotation.
	*/
	UE_API FRotator GetTargetRotation() const;

	/**
	 * Set warp target as context for calculating switch off condition result
	 * if bUseWarpTargetAsTargetLocation is set to true
	 */
	UE_API virtual void SetWarpTargetForDestination(const FTraversalMotionWarpTarget* InMotionWarpingTarget);
	virtual bool OnCheck() const { return false; }

	/**
	 * Extra information used for debugging, if CVAR MotionWarping.Debug.SwitchOffCondition is true
	 */
	virtual FString ExtraDebugInfo() const { return FString(); }
	UE_API virtual bool IsConditionValid() const;

	virtual void SetOwnerActor(const AActor* InOwnerActor) { OwnerActor = InOwnerActor; }
	virtual void SetTargetActor(const AActor* InTargetActor) { TargetActor = InTargetActor; }
	virtual void SetMotionWarpingTarget(const FTraversalMotionWarpTarget* InMotionWarpingTarget) { MotionWarpingTarget = InMotionWarpingTarget; }
	
protected:
	bool bIsInitialized = false;

	UPROPERTY()
	TObjectPtr<const AActor> OwnerActor;

	UPROPERTY()
	TObjectPtr<const AActor> TargetActor;

	const FTraversalMotionWarpTarget* MotionWarpingTarget = nullptr;
};

UENUM(BlueprintType, Category = "TraversalMotionWarp", meta = (Experimental))
enum class ETraversalSwitchOffConditionDistanceOp : uint8
{
	LessThan,
	GreaterThan
};

UENUM(BlueprintType, Category = "TraversalMotionWarp", meta = (Experimental))
enum class ETraversalSwitchOffConditionDistanceAxesType : uint8
{
	AllAxes,
	IgnoreZAxis,
	OnlyZAxis,
};

UCLASS(Experimental)
class UTraversalMotionWarpSwitchOffDistanceCondition : public UTraversalMotionWarpSwitchOffCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "TraversalMotionWarp")
	ETraversalSwitchOffConditionDistanceOp Operator;

	UPROPERTY(EditAnywhere, Category = "TraversalMotionWarp")
	ETraversalSwitchOffConditionDistanceAxesType AxesType;
	
	UPROPERTY(EditAnywhere, Category = "TraversalMotionWarp")
	float Distance;

	/**
	 * Creates Switch Off Distance Condition that checks distance between Owner Actor and Target Location.
	 * If Use Warp Target Location is true, Target Location is corresponding Warp Target's location.
	 * If Use Warp Target Location is false, Target Location is Target Actor's parameter location.
	 * @param InOwnerActor Actor owning Motion Warping Component to which this switch off condition will be added.
	 * @param InEffect What should happen if switch off condition is true
	 * @param InOperator Distance comparison operator
	 * @param InDistance Distance to compare to
	 * @param InbUseWarpTargetAsTargetLocation Should use corresponding warp target as context for this Switch Off Condition.
	 * If set to false, will use TargetActor parameter
	 * @param InTargetActor Target Actor to use as a context for this Switch Off Condition if Use Warp Target As Location parameter is false.
	 * @return Created Switch Off Condition. This Switch Off Condition can be added to Motion Warping Component with Add Switch Off Condition node.
	 */
	UFUNCTION(BlueprintCallable, Category="Motion Warping", meta=(AdvancedDisplay="bInUseWarpTargetAsTargetLocation, InTargetActor"))
	static UTraversalMotionWarpSwitchOffDistanceCondition* CreateSwitchOffDistanceCondition(
		UPARAM(DisplayName = "Owner Actor", ref)		AActor* InOwnerActor,
		UPARAM(DisplayName = "Effect")					ETraversalSwitchOffConditionEffect InEffect,
		UPARAM(DisplayName = "Operator")				ETraversalSwitchOffConditionDistanceOp InOperator,
		UPARAM(DisplayName = "Distance")				float InDistance,
		UPARAM(DisplayName = "UseWarpTargetAsLocation") bool InbUseWarpTargetAsTargetLocation = true,
		UPARAM(DisplayName = "TargetActor")				AActor* InTargetActor = nullptr);
	
protected:
	virtual bool OnCheck() const override;
	virtual FString ExtraDebugInfo() const override;
	
	float CalculateSqDistance() const;
	float CalculateSqDistance2D() const;
	float CalculateZDistance() const;
};


UENUM(BlueprintType, Category = "TraversalMotionWarp", meta = (Experimental))
enum class ETraversalSwitchOffConditionAngleOp : uint8
{
	LessThan,
	GreaterThan
};

UCLASS(Experimental)
class UTraversalMotionWarpSwitchOffAngleToTargetCondition : public UTraversalMotionWarpSwitchOffCondition
{
	GENERATED_BODY()

public:
	/**
	 * Creates Switch Off Angle To Target Condition that checks angle between Owner Actor and Target Location.
	 * If Use Warp Target Location is true, Target Location is corresponding Warp Target's location.
	 * If Use Warp Target Location is false, Target Location is Target Actor's parameter location.
	 * @param InOwnerActor Actor owning Motion Warping Component to which this switch off condition will be added.
	 * @param InEffect What should happen if switch off condition is true
	 * @param InOperator Angle comparison operator
	 * @param InAngle Angle to compare to
	 * @param bInIgnoreZAxis Should ignore Z axis in Angle comparison
	 * @param bInUseWarpTargetAsTargetLocation Should use corresponding warp target as context for this Switch Off Condition.
	 * If set to false, will use TargetActor parameter
	 * @param InTargetActor Target Actor to use as a context for this Switch Off Condition if Use Warp Target As Location parameter is false.
	 * @return Created Switch Off Condition. This Switch Off Condition can be added to Motion Warping Component with Add Switch Off Condition node.
	 */
	UFUNCTION(BlueprintCallable, Category="Motion Warping|Experimental", meta=(AdvancedDisplay="bInUseWarpTargetAsTargetLocation, InTargetActor"))
	static UTraversalMotionWarpSwitchOffAngleToTargetCondition* CreateSwitchOffAngleToTargetCondition(
		UPARAM(DisplayName = "Owner Actor", ref)		AActor* InOwnerActor,
		UPARAM(DisplayName = "Effect")					ETraversalSwitchOffConditionEffect InEffect,
		UPARAM(DisplayName = "Operator")				ETraversalSwitchOffConditionAngleOp InOperator,
		UPARAM(DisplayName = "Angle")					float InAngle,
		UPARAM(DisplayName = "Ignore Z Axis")			bool bInIgnoreZAxis,
		UPARAM(DisplayName = "TargetActor")				bool bInUseWarpTargetAsTargetLocation = true,
		UPARAM(DisplayName = "TargetActor")				AActor* InTargetActor = nullptr);
	
protected:
	virtual bool OnCheck() const override;
	virtual FString ExtraDebugInfo() const override;

	float CalculateAngleToTarget() const;

	UPROPERTY(EditAnywhere, Category = "TraversalMotionWarp")
	ETraversalSwitchOffConditionAngleOp Operator;

	UPROPERTY(EditAnywhere, Category = "TraversalMotionWarp", meta = (ForceUnits="degrees"))
	float Angle;

	UPROPERTY(EditAnywhere, Category = "TraversalMotionWarp")
	bool bIgnoreZAxis;
};

UENUM(BlueprintType, Category = "TraversalMotionWarp", meta = (Experimental))
enum class ETraversalSwitchOffConditionCompositeOp : uint8
{
	Or UMETA(DisplayName = "OR"),
	And UMETA(DisplayName = "AND")
};

UCLASS(Experimental)
class UTraversalMotionWarpSwitchOffCompositeCondition : public UTraversalMotionWarpSwitchOffCondition
{
	GENERATED_BODY()
public:
	virtual void SetOwnerActor(const AActor* InOwnerActor) override;
	virtual void SetTargetActor(const AActor* InTargetActor) override;
	virtual void SetMotionWarpingTarget(const FTraversalMotionWarpTarget* InMotionWarpingTarget) override;

	/**
	 * Creates Switch Off Composite Condition that lets you combine different switch off conditions with logic AND/OR operators.
	 * @param InOwnerActor Actor owning Motion Warping Component to which this switch off condition will be added.
	 * @param InEffect What should happen if switch off condition is true
	 * @param InSwitchOffConditionA First Switch Off Condition to combine
	 * @param InLogicOperator Logic operator to use for Switch Off Condition combination
	 * @param InSwitchOffConditionB Second Switch Off Condition to combine
	 * @param bInUseWarpTargetAsTargetLocation Should use corresponding warp target as context for this Switch Off Condition.
	 * If set to false, will use TargetActor parameter
	 * @param InTargetActor Target Actor to use as a context for this Switch Off Condition if Use Warp Target As Location parameter is false.
	 * @return Created Switch Off Condition. This Switch Off Condition can be added to Motion Warping Component with Add Switch Off Condition node.
	 */
	UFUNCTION(BlueprintCallable, Category="Motion Warping|Experimental", meta=(AdvancedDisplay="bInUseWarpTargetAsTargetLocation, InTargetActor"))
	static UTraversalMotionWarpSwitchOffCompositeCondition* CreateSwitchOffCompositeCondition(
		UPARAM(DisplayName = "Owner Actor", ref)			AActor* InOwnerActor,
		UPARAM(DisplayName = "Effect")						ETraversalSwitchOffConditionEffect InEffect,
		UPARAM(DisplayName = "Switch Off Condition A", ref)	UTraversalMotionWarpSwitchOffCondition* InSwitchOffConditionA,
		UPARAM(DisplayName = "Logic Operator")				ETraversalSwitchOffConditionCompositeOp InLogicOperator,
		UPARAM(DisplayName = "Switch Off Condition B", ref)	UTraversalMotionWarpSwitchOffCondition* InSwitchOffConditionB,
		UPARAM(DisplayName = "Use Warp Target As Location")	bool bInUseWarpTargetAsTargetLocation = true,
		UPARAM(DisplayName = "TargetActor")					AActor* InTargetActor = nullptr);
	
protected:
	virtual void SetWarpTargetForDestination(const FTraversalMotionWarpTarget* InMotionWarpingTarget) override;
	virtual bool OnCheck() const override;
	virtual FString ExtraDebugInfo() const override;
	virtual bool IsConditionValid() const override;
	
	UPROPERTY(EditAnywhere, Instanced, Category="TraversalMotionWarp")
	TObjectPtr<UTraversalMotionWarpSwitchOffCondition> SwitchOffConditionA;

	UPROPERTY(EditAnywhere, Category="TraversalMotionWarp")
	ETraversalSwitchOffConditionCompositeOp LogicOperator;

	UPROPERTY(EditAnywhere, Instanced, Category="TraversalMotionWarp")
	TObjectPtr<UTraversalMotionWarpSwitchOffCondition> SwitchOffConditionB;
};

UCLASS(Blueprintable, Experimental)
class UTraversalMotionWarpSwitchOffBlueprintableCondition : public UTraversalMotionWarpSwitchOffCondition
{
	GENERATED_BODY()
	
	virtual bool OnCheck() const override;
	virtual FString ExtraDebugInfo() const override;

	/**
	 * Creates Switch Off Blueprintable Condition from WarpingSwitchOffBlueprintableCondition subclass.
	 * @param InOwnerActor Actor owning Motion Warping Component to which this switch off condition will be added.
	 * @param InEffect What should happen if switch off condition is true
	 * @param InBlueprintableCondition Subclass of WarpingSwitchOffBlueprintableCondition
	* @param bInUseWarpTargetAsTargetLocation Should use corresponding warp target as context for this Switch Off Condition.
	 * If set to false, will use TargetActor parameter 
	 * @param InTargetActor Target Actor to use as a context for this Switch Off Condition if Use Warp Target As Location parameter is false.
	 * @return Created Switch Off Condition. This Switch Off Condition can be added to Motion Warping Component with Add Switch Off Condition node.
	 */
	UFUNCTION(BlueprintCallable, Category="Motion Warping|Experimental", meta=(AdvancedDisplay="bInUseWarpTargetAsTargetLocation, InTargetActor"))
	static UTraversalMotionWarpSwitchOffBlueprintableCondition* CreateSwitchOffBlueprintableCondition(
		UPARAM(DisplayName = "Owner Actor", ref)					AActor* InOwnerActor,
		UPARAM(DisplayName = "Effect")								ETraversalSwitchOffConditionEffect InEffect,
		UPARAM(DisplayName = "Blueprintable Switch Off Condition")	TSubclassOf<UTraversalMotionWarpSwitchOffBlueprintableCondition> InBlueprintableCondition,
		UPARAM(DisplayName = "Use Warp Target As Location")			bool bInUseWarpTargetAsTargetLocation = true,
		UPARAM(DisplayName = "TargetActor")							AActor* InTargetActor = nullptr);
	
public:
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintNativeEvent)
	bool BP_Check(
		UPARAM(DisplayName = "Owner Actor")							const AActor* InOwnerActor,
		UPARAM(DisplayName = "Target Actor")						const AActor* InTargetActor,
		UPARAM(DisplayName = "Target Location")						FVector InTargetLocation,
		UPARAM(DisplayName = "Use warp target as target location")	bool bInUseWarpTargetAsTargetLocation) const;

	UFUNCTION(BlueprintNativeEvent)
	FString BP_ExtraDebugInfo(
		UPARAM(DisplayName = "Owner Actor")							const AActor* InOwnerActor,
		UPARAM(DisplayName = "Target Actor")						const AActor* InTargetActor,
		UPARAM(DisplayName = "Target Location")						FVector InTargetLocation,
		UPARAM(DisplayName = "Use warp target as target location")	bool bInUseWarpTargetAsTargetLocation) const;
};

#undef UE_API

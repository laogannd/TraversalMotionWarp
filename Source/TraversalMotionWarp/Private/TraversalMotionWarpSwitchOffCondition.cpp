// Copyright (c) 2026 DGOne. All Rights Reserved.

#include "TraversalMotionWarpSwitchOffCondition.h"
#include "TraversalMotionWarpComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TraversalMotionWarpSwitchOffCondition)

void FTraversalSwitchOffConditionData::SetMotionWarpingTarget(const FTraversalMotionWarpTarget* MotionWarpingTarget)
{
	for (UTraversalMotionWarpSwitchOffCondition* SwitchOffCondition : SwitchOffConditions)
	{
		SwitchOffCondition->SetMotionWarpingTarget(MotionWarpingTarget);
	}
}

bool UTraversalMotionWarpSwitchOffCondition::Check() const
{
	return OnCheck();
}

FVector UTraversalMotionWarpSwitchOffCondition::GetTargetLocation() const
{
	if (bUseWarpTargetAsTargetLocation)
	{
		if (MotionWarpingTarget)
		{
			return MotionWarpingTarget->GetLocation();
		}

		UE_LOG(LogTraversalMotionWarp, Warning, TEXT("Switch off condition is set to use warp target as target location, "
			"however warp target appears to be null. Make sure warp target is correctly passed, "
			"otherwise switch off condition will use target actor location."))
	}

	return IsValid(TargetActor) ? TargetActor->GetActorLocation() : FVector::ZeroVector;
}

FRotator UTraversalMotionWarpSwitchOffCondition::GetTargetRotation() const
{
	if (bUseWarpTargetAsTargetLocation)
	{
		if (MotionWarpingTarget)
		{
			return MotionWarpingTarget->Rotator();
		}

		UE_LOG(LogTraversalMotionWarp, Warning, TEXT("Switch off condition is set to use warp target as target rotation, "
			"however warp target appears to be null. Make sure warp target is correctly passed, "
			"otherwise switch off condition will use target actor rotation."))
	}

	return IsValid(TargetActor) ? TargetActor->GetActorRotation() : FRotator::ZeroRotator;
}

void UTraversalMotionWarpSwitchOffCondition::SetWarpTargetForDestination(const FTraversalMotionWarpTarget* InMotionWarpingTarget)
{
	MotionWarpingTarget = InMotionWarpingTarget;
}

bool UTraversalMotionWarpSwitchOffCondition::IsConditionValid() const
{
	if (!IsValid(OwnerActor))
	{
		
		UE_LOG(LogTraversalMotionWarp, Display, TEXT("%s won't work due to invalid Owner Actor"), *GetClass()->GetName())
		return false;
	}
	
	if (bUseWarpTargetAsTargetLocation)
	{
		if (!MotionWarpingTarget)
		{

			UE_LOG(LogTraversalMotionWarp, Display, TEXT("%s is set to use Motion Warping Target as target location, but won't work due to null Motion Warping Target"), *GetClass()->GetName())
			return false;
		}

		return true;
	}

	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTraversalMotionWarp, Display, TEXT("%s on actor %s is set to use Actor as target location, but won't work due to invalid Target Actor"), *GetClass()->GetName(), *OwnerActor->GetName());
		return  false;
	}
	
	return true;
}

UTraversalMotionWarpSwitchOffDistanceCondition* UTraversalMotionWarpSwitchOffDistanceCondition::CreateSwitchOffDistanceCondition(AActor* InOwnerActor, ETraversalSwitchOffConditionEffect InEffect, ETraversalSwitchOffConditionDistanceOp InOperator, float InDistance, bool InbUseWarpTargetAsTargetLocation, AActor* InTargetActor)
{
	UTraversalMotionWarpSwitchOffDistanceCondition* SwitchOffDistanceCondition = NewObject<UTraversalMotionWarpSwitchOffDistanceCondition>();
	SwitchOffDistanceCondition->OwnerActor = InOwnerActor;
	SwitchOffDistanceCondition->Effect = InEffect;
	SwitchOffDistanceCondition->Operator = InOperator;
	SwitchOffDistanceCondition->Distance = InDistance;
	SwitchOffDistanceCondition->bUseWarpTargetAsTargetLocation = InbUseWarpTargetAsTargetLocation;
	SwitchOffDistanceCondition->TargetActor = InTargetActor;

	return SwitchOffDistanceCondition;
}

bool UTraversalMotionWarpSwitchOffDistanceCondition::OnCheck() const
{
	switch (AxesType)
	{
		case ETraversalSwitchOffConditionDistanceAxesType::AllAxes:
			return Operator == ETraversalSwitchOffConditionDistanceOp::LessThan ?
				CalculateSqDistance() < FMath::Square(Distance) :
				CalculateSqDistance() > FMath::Square(Distance);
		
		case ETraversalSwitchOffConditionDistanceAxesType::IgnoreZAxis:
			return Operator == ETraversalSwitchOffConditionDistanceOp::LessThan ?
				CalculateSqDistance2D() < FMath::Square(Distance) :
				CalculateSqDistance2D() > FMath::Square(Distance);
		
		case ETraversalSwitchOffConditionDistanceAxesType::OnlyZAxis:
			return Operator == ETraversalSwitchOffConditionDistanceOp::LessThan ?
				CalculateZDistance() < Distance :
				CalculateZDistance() > Distance;
	}

	return false;
}

float UTraversalMotionWarpSwitchOffDistanceCondition::CalculateSqDistance() const
{
	return (OwnerActor->GetActorLocation() - GetTargetLocation()).SquaredLength();
}

float UTraversalMotionWarpSwitchOffDistanceCondition::CalculateSqDistance2D() const
{
	return (OwnerActor->GetActorLocation() - GetTargetLocation()).SizeSquared2D();
}

float UTraversalMotionWarpSwitchOffDistanceCondition::CalculateZDistance() const
{
	return FMath::Abs(OwnerActor->GetActorLocation().Z - GetTargetLocation().Z);
}

FString UTraversalMotionWarpSwitchOffDistanceCondition::ExtraDebugInfo() const
{
	switch (AxesType)
	{
		case ETraversalSwitchOffConditionDistanceAxesType::AllAxes:
			return FString::Printf(TEXT("Distance: %f %c %f"),
				FMath::Sqrt(CalculateSqDistance()),
				(Operator == ETraversalSwitchOffConditionDistanceOp::GreaterThan ? '>' : '<'),
				Distance);
		
		case ETraversalSwitchOffConditionDistanceAxesType::IgnoreZAxis:
			return FString::Printf(TEXT("Distance2D: %f %c %f"),
				FMath::Sqrt(CalculateSqDistance2D()),
				(Operator == ETraversalSwitchOffConditionDistanceOp::GreaterThan ? '>' : '<'),
				Distance);
		
		case ETraversalSwitchOffConditionDistanceAxesType::OnlyZAxis:
			return FString::Printf(TEXT("Distance Z: %f %c %f"),
				CalculateZDistance(),
				(Operator == ETraversalSwitchOffConditionDistanceOp::GreaterThan ? '>' : '<'),
				Distance);
	}

	return FString();
}

UTraversalMotionWarpSwitchOffAngleToTargetCondition* UTraversalMotionWarpSwitchOffAngleToTargetCondition::CreateSwitchOffAngleToTargetCondition(AActor* InOwnerActor, ETraversalSwitchOffConditionEffect InEffect, ETraversalSwitchOffConditionAngleOp InOperator, float InAngle, bool bInIgnoreZAxis, bool bInUseWarpTargetAsTargetLocation, AActor* InTargetActor)
{
	UTraversalMotionWarpSwitchOffAngleToTargetCondition* SwitchOffAngleToTargetCondition = NewObject<UTraversalMotionWarpSwitchOffAngleToTargetCondition>();
	SwitchOffAngleToTargetCondition->OwnerActor = InOwnerActor;
	SwitchOffAngleToTargetCondition->Effect = InEffect;
	SwitchOffAngleToTargetCondition->Operator = InOperator;
	SwitchOffAngleToTargetCondition->Angle = InAngle;
	SwitchOffAngleToTargetCondition->bUseWarpTargetAsTargetLocation = bInUseWarpTargetAsTargetLocation;
	SwitchOffAngleToTargetCondition->TargetActor = InTargetActor;

	return SwitchOffAngleToTargetCondition;
}

bool UTraversalMotionWarpSwitchOffAngleToTargetCondition::OnCheck() const
{
	if (Operator == ETraversalSwitchOffConditionAngleOp::LessThan)
	{
		return CalculateAngleToTarget() < Angle;
	}

	return CalculateAngleToTarget() > Angle;
}

FString UTraversalMotionWarpSwitchOffAngleToTargetCondition::ExtraDebugInfo() const
{
	return FString::Printf(TEXT("Angle: %f %c %f"),
		CalculateAngleToTarget(),
		(Operator == ETraversalSwitchOffConditionAngleOp::GreaterThan ? '>' : '<'),
		Angle);
}

float UTraversalMotionWarpSwitchOffAngleToTargetCondition::CalculateAngleToTarget() const
{
	FVector OwnerForward = OwnerActor->GetActorForwardVector();
	FVector OwnerToTarget = GetTargetLocation() - OwnerActor->GetActorLocation();

	if (bIgnoreZAxis)
	{
		OwnerForward = FVector(OwnerForward.X, OwnerForward.Y, 0);
		OwnerToTarget = FVector(OwnerToTarget.X, OwnerToTarget.Y, 0);
		OwnerForward.Normalize();
	}

	OwnerToTarget.Normalize();

	return FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(OwnerForward, OwnerToTarget)));
}

void UTraversalMotionWarpSwitchOffCompositeCondition::SetOwnerActor(const AActor* InOwnerActor)
{
	Super::SetOwnerActor(InOwnerActor);
	
	SwitchOffConditionA->SetOwnerActor(InOwnerActor);
	SwitchOffConditionB->SetOwnerActor(InOwnerActor);
}

void UTraversalMotionWarpSwitchOffCompositeCondition::SetTargetActor(const AActor* InTargetActor)
{
	Super::SetTargetActor(InTargetActor);
	
	SwitchOffConditionA->SetTargetActor(InTargetActor);
	SwitchOffConditionB->SetTargetActor(InTargetActor);
}

void UTraversalMotionWarpSwitchOffCompositeCondition::SetMotionWarpingTarget(const FTraversalMotionWarpTarget* InMotionWarpingTarget)
{
	Super::SetMotionWarpingTarget(InMotionWarpingTarget);
	
	SwitchOffConditionA->SetMotionWarpingTarget(InMotionWarpingTarget);
	SwitchOffConditionB->SetMotionWarpingTarget(InMotionWarpingTarget);
}

void UTraversalMotionWarpSwitchOffCompositeCondition::SetWarpTargetForDestination(const FTraversalMotionWarpTarget* InMotionWarpingTarget)
{
	Super::SetWarpTargetForDestination(InMotionWarpingTarget);

	if (ensureMsgf(IsValid(SwitchOffConditionA), TEXT("Switch off condition A not setup in composite switch off condition on actor %s"), *OwnerActor->GetName())
		&& ensureMsgf(IsValid(SwitchOffConditionB), TEXT("Switch off condition B not setup in composite switch off condition on actor %s"), *OwnerActor->GetName()))
	{
		SwitchOffConditionA->SetWarpTargetForDestination(InMotionWarpingTarget);
		SwitchOffConditionB->SetWarpTargetForDestination(InMotionWarpingTarget);
	}
}

bool UTraversalMotionWarpSwitchOffCompositeCondition::OnCheck() const
{
	if (ensureMsgf(IsValid(SwitchOffConditionA), TEXT("Switch off condition A not setup in composite switch off condition on actor %s"), *OwnerActor->GetName())
		&& ensureMsgf(IsValid(SwitchOffConditionB), TEXT("Switch off condition B not setup in composite switch off condition on actor %s"), *OwnerActor->GetName()))
	{
		if (LogicOperator == ETraversalSwitchOffConditionCompositeOp::Or)
		{
			return SwitchOffConditionA->Check() || SwitchOffConditionB->Check();
		}
		return SwitchOffConditionA->Check() && SwitchOffConditionB->Check();
	}

	return false;
}

FString UTraversalMotionWarpSwitchOffCompositeCondition::ExtraDebugInfo() const
{
	return FString::Printf(TEXT("%s %s %s"),
		*SwitchOffConditionA->ExtraDebugInfo(),
		*(LogicOperator == ETraversalSwitchOffConditionCompositeOp::Or ? FString("OR") : FString("AND")),
		*SwitchOffConditionB->ExtraDebugInfo());
}

bool UTraversalMotionWarpSwitchOffCompositeCondition::IsConditionValid() const
{
	return IsValid(SwitchOffConditionA) && IsValid(SwitchOffConditionB)
		&& SwitchOffConditionA->IsConditionValid() && SwitchOffConditionB->IsConditionValid();
}

UTraversalMotionWarpSwitchOffCompositeCondition* UTraversalMotionWarpSwitchOffCompositeCondition::CreateSwitchOffCompositeCondition(AActor* InOwnerActor, ETraversalSwitchOffConditionEffect InEffect, UTraversalMotionWarpSwitchOffCondition* InSwitchOffConditionA, ETraversalSwitchOffConditionCompositeOp InLogicOperator, UTraversalMotionWarpSwitchOffCondition* InSwitchOffConditionB, bool bInUseWarpTargetAsTargetLocation, AActor* InTargetActor)
{
	UTraversalMotionWarpSwitchOffCompositeCondition* SwitchOffCompositeCondition = NewObject<UTraversalMotionWarpSwitchOffCompositeCondition>();
	SwitchOffCompositeCondition->OwnerActor = InOwnerActor;
	SwitchOffCompositeCondition->Effect = InEffect;
	SwitchOffCompositeCondition->SwitchOffConditionA = InSwitchOffConditionA;
	SwitchOffCompositeCondition->LogicOperator = InLogicOperator;
	SwitchOffCompositeCondition->SwitchOffConditionB = InSwitchOffConditionB;
	SwitchOffCompositeCondition->bUseWarpTargetAsTargetLocation = bInUseWarpTargetAsTargetLocation;
	SwitchOffCompositeCondition->TargetActor = InTargetActor;

	return SwitchOffCompositeCondition;
}

bool UTraversalMotionWarpSwitchOffBlueprintableCondition::OnCheck() const
{
	return BP_Check(OwnerActor, TargetActor, GetTargetLocation(), bUseWarpTargetAsTargetLocation);
}

FString UTraversalMotionWarpSwitchOffBlueprintableCondition::ExtraDebugInfo() const
{
	return BP_ExtraDebugInfo(OwnerActor, TargetActor, GetTargetLocation(), bUseWarpTargetAsTargetLocation);
}

UTraversalMotionWarpSwitchOffBlueprintableCondition* UTraversalMotionWarpSwitchOffBlueprintableCondition::CreateSwitchOffBlueprintableCondition(AActor* InOwnerActor, ETraversalSwitchOffConditionEffect InEffect, TSubclassOf<UTraversalMotionWarpSwitchOffBlueprintableCondition> InBlueprintableCondition, bool bInUseWarpTargetAsTargetLocation, AActor* InTargetActor)
{
	UTraversalMotionWarpSwitchOffBlueprintableCondition* SwitchOffBlueprintableCondition = NewObject<UTraversalMotionWarpSwitchOffBlueprintableCondition>(GetTransientPackage(), InBlueprintableCondition);
	SwitchOffBlueprintableCondition->OwnerActor = InOwnerActor;
	SwitchOffBlueprintableCondition->Effect = InEffect;
	SwitchOffBlueprintableCondition->bUseWarpTargetAsTargetLocation = bInUseWarpTargetAsTargetLocation;
	SwitchOffBlueprintableCondition->TargetActor = InTargetActor;

	return SwitchOffBlueprintableCondition;
}

UWorld* UTraversalMotionWarpSwitchOffBlueprintableCondition::GetWorld() const
{
	if (IsValid(OwnerActor))
	{
		return OwnerActor->GetWorld();
	}

	return nullptr;
}

FString UTraversalMotionWarpSwitchOffBlueprintableCondition::BP_ExtraDebugInfo_Implementation(const AActor* InOwnerActor, const AActor* InTargetActor, FVector InTargetLocation, bool bInUseWarpTargetAsTargetLocation) const
{
	return FString("No extra debug info. Override BP_ExtraDebugInfo to add it.");
}

bool UTraversalMotionWarpSwitchOffBlueprintableCondition::BP_Check_Implementation(const AActor* InOwnerActor, const AActor* InTargetActor, FVector InTargetLocation, bool bInUseWarpTargetAsTargetLocation) const
{
	return false;
}

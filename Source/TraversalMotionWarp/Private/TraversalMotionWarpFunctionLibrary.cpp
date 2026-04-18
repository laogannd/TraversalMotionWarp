// Copyright (c) 2026 DGOne. All Rights Reserved.
#include "TraversalMotionWarpFunctionLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TraversalMotionWarpFunctionLibrary)

FTraversalMotionWarpTarget UTraversalMotionWarpFunctionLibrary::MakeTraversalMotionWarpTarget(const FName Name, const FVector Location, const FRotator Rotation, const USceneComponent* Component, FName BoneName, bool bFollowComponent, ETraversalWarpTargetLocationOffsetDirection LocationOffsetDirection, const AActor* AvatarActor, const FVector LocationOffset, const FRotator RotationOffset)
{
	if (Component)
	{
		return FTraversalMotionWarpTarget(Name, Component, BoneName, bFollowComponent, LocationOffsetDirection, AvatarActor, LocationOffset, RotationOffset);
	}
	else
	{
		FTraversalMotionWarpTarget Result = FTraversalMotionWarpTarget();

		// Only certain arguments are valid when a component isn't specified
		Result.Name = Name;
		Result.Location = Location;
		Result.Rotation = Rotation;
		Result.Component = nullptr;
		Result.BoneName = NAME_None;
		Result.bFollowComponent = false;

		return Result;
	}
}

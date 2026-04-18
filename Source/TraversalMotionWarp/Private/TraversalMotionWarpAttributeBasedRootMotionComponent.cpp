// Copyright (c) 2026 DGOne. All Rights Reserved.

#include "TraversalMotionWarpAttributeBasedRootMotionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TraversalMotionWarpAttributeBasedRootMotionComponent)

// UTraversalMotionWarpAttributeBasedRootMotionComponent
///////////////////////////////////////////////////////////////////////

UTraversalMotionWarpAttributeBasedRootMotionComponent::UTraversalMotionWarpAttributeBasedRootMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.bCanEverTick = true;
}

void UTraversalMotionWarpAttributeBasedRootMotionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	CharacterOwner = Cast<ACharacter>(GetOwner());
	
	ACharacter* Character = GetCharacterOwner();
	check(Character);
	
	PrePhysicsTickFunction.bCanEverTick = true;
	PrePhysicsTickFunction.bStartWithTickEnabled = true;
	PrePhysicsTickFunction.SetTickFunctionEnable(true);
	PrePhysicsTickFunction.TickGroup = TG_PrePhysics;
	PrePhysicsTickFunction.Target = this;
}

void UTraversalMotionWarpAttributeBasedRootMotionComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	if (Mode == ETraversalMotionWarpAttributeBasedRootMotionMode::ApplyVelocity)
	{
		if (bRegister)
		{
			CharacterOwner = Cast<ACharacter>(GetOwner());
			ACharacter* Character = GetCharacterOwner();
			check(Character);
			UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement<UCharacterMovementComponent>();

			PrePhysicsTickFunction.RegisterTickFunction(GetComponentLevel());
			CharacterMovement->PrimaryComponentTick.AddPrerequisite(this, PrePhysicsTickFunction);
		}
		else
		{
			PrePhysicsTickFunction.UnRegisterTickFunction();
		}
	}
}


void UTraversalMotionWarpAttributeBasedRootMotionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (bEnableRootMotion)
	{
		static const FName RootMotionAttributeName = "RootMotionDelta";
		static const UE::Anim::FAttributeId RootMotionAttributeId = { RootMotionAttributeName , FCompactPoseBoneIndex(0) };
	
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
		if (ACharacter* Character = GetCharacterOwner())
		{
			if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
			{
				if (const FTransformAnimationAttribute* RootMotionAttribute = Mesh->GetCustomAttributes().Find<FTransformAnimationAttribute>(RootMotionAttributeId))
				{
					TranslationVelocity = RootMotionAttribute->Value.GetTranslation() / DeltaTime;

					const FQuat RootMotionRotation = RootMotionAttribute->Value.GetRotation().GetShortestArcWith(FQuat::Identity);
					RotationVelocity = RootMotionRotation.ToRotationVector() / DeltaTime;

					if (Mode == ETraversalMotionWarpAttributeBasedRootMotionMode::ApplyDelta)
					{
						if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement<UCharacterMovementComponent>())
						{
							CharacterMovement->RootMotionParams.Set(RootMotionAttribute->Value);
						}
					}
				
					UE_VLOG_ARROW(this, "Root Motion", Display, Character->GetActorTransform().GetLocation(), Character->GetActorTransform().GetLocation() + Mesh->GetComponentTransform().TransformVector(TranslationVelocity) * 0.1, FColor::Green, TEXT(""));
				}
			}
		}
	}
}


void FTraversalMotionWarpAttributeBasedRootMotionTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
		{
			Target->PrePhysicsTickComponent(DilatedTime);
		});
}

FString FTraversalMotionWarpAttributeBasedRootMotionTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[UTraversalMotionWarpAttributeBasedRootMotionComponent::PrePhysicsTick]");
}

FName FTraversalMotionWarpAttributeBasedRootMotionTickFunction::DiagnosticContext(bool bDetailed)
{
	if (bDetailed)
	{
		return FName(*FString::Printf(TEXT("AttributeBasedRootMotionComponentPrePhysicsTick/%s"), *GetFullNameSafe(Target)));
	}

	return FName(TEXT("AttributeBasedRootMotionComponentPrePhysicsTick"));
}

void UTraversalMotionWarpAttributeBasedRootMotionComponent::PrePhysicsTickComponent(float DeltaTime)
{
	if (!bEnableRootMotion)
	{
		return;
	}
	
	if (ACharacter* Character = GetCharacterOwner())
	{
		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement<UCharacterMovementComponent>())
		{
			FTransform RootMotion;
			RootMotion.SetTranslation(TranslationVelocity * DeltaTime);
			RootMotion.SetRotation(FQuat::MakeFromRotationVector(RotationVelocity*DeltaTime));
			CharacterMovement->RootMotionParams.Set(RootMotion);
		}
	}
}

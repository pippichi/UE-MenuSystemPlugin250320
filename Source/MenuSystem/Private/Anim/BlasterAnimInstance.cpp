// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim/BlasterAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UBlasterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	APawn* GetPawnOwner = TryGetPawnOwner();
	ACharacter* Character = Cast<ACharacter>(GetPawnOwner);
	if (Character->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f)
	{
		// Accelerating
	}
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
}

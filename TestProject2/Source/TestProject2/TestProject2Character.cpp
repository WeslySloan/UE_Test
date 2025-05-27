// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestProject2Character.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "DrawDebugHelpers.h" // 디버깅용 추가분
#include "Kismet/KismetMathLibrary.h" // UKismetMathLibrary 포함

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ATestProject2Character

ATestProject2Character::ATestProject2Character()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// "올라가기" 관련 변수 초기화
	ClimbTraceOffset = FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()); // 대략 캐릭터 눈높이
	ClimbTraceDistance = 150.0f; // 적절한 거리로 조정
	ClimbSpeed = 500.0f; // 올라가는 속도
	bIsClimbing = false;
	ClimbTargetLocation = FVector::ZeroVector;
}

//////////////////////////////////////////////////////////////////////////
// Input

void ATestProject2Character::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ATestProject2Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATestProject2Character::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATestProject2Character::Look);

		// "올라가기" 액션 바인딩
		EnhancedInputComponent->BindAction(ClimbAction, ETriggerEvent::Started, this, &ATestProject2Character::TryClimb);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ATestProject2Character::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// bIsClimbing 상태일 때는 이동 입력을 받지 않음
	if (bIsClimbing)
	{
		return; // bIsClimbing이 true면 함수를 즉시 종료하여 이동 입력을 무시
	}

	if (Controller != nullptr) // 이전에 !bIsClimbing 조건이 있었으나, 위에서 먼저 처리했으므로 제거
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ATestProject2Character::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr && !bIsClimbing)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATestProject2Character::TryClimb()
{
	UE_LOG(LogTemp, Warning, TEXT("TryClimb function entered. bIsClimbing: %s"), bIsClimbing ? TEXT("True") : TEXT("False"));
	if (bIsClimbing)
	{
		// 이 로그는 bIsClimbing이 true일 때만 출력됩니다.
		UE_LOG(LogTemp, Warning, TEXT("Already climbing, returning."));
		return;
	}

	FVector StartLocation = GetActorLocation() + GetActorForwardVector() * ClimbTraceOffset.X + FVector(0.0f, 0.0f, ClimbTraceOffset.Z);
	FVector EndLocation = StartLocation + GetActorForwardVector() * ClimbTraceDistance;
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this); // 자기 자신은 무시

	// Line Trace 수행
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECC_Visibility, // 또는 원하는 Collision Channel
		Params
	);

	// 디버깅용 Line Trace 그리기
	DrawDebugLine(GetWorld(), StartLocation, EndLocation, bHit ? FColor::Green : FColor::Red, false, 0.1f);

	if (bHit && HitResult.GetActor() && HitResult.GetActor()->Tags.Contains(FName("Climbable")))
	{
		UE_LOG(LogTemp, Warning, TEXT("올라갈 수 있는 오브젝트 (%s) 를 발견했습니다."), *HitResult.GetActor()->GetName());

		ClimbTargetLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20.0f);
		bIsClimbing = true;
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
		GetCharacterMovement()->Velocity = FVector::ZeroVector;

		// 여기서 이벤트 호출
		UE_LOG(LogTemp, Warning, TEXT("OnClimbStarted is about to be called!")); // <-- 이 줄 추가
		OnClimbStarted();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Raycast X"));
	}
}

void ATestProject2Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsClimbing)
	{
		FVector CurrentLocation = GetActorLocation();
		FVector Direction = (ClimbTargetLocation - CurrentLocation).GetSafeNormal();
		FVector NewVelocity = Direction * ClimbSpeed;
		GetCharacterMovement()->Velocity = NewVelocity;

		// 목표 위치에 거의 도달했으면 올라가기 종료
		if (FVector::DistSquared(CurrentLocation, ClimbTargetLocation) < FMath::Square(5.0f))
		{
			bIsClimbing = false;
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
			SetActorLocation(ClimbTargetLocation); // 정확한 위치로 설정
		}
	}
}
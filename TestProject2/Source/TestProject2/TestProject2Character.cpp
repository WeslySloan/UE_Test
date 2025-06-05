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
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h" // UGameplayStatics::SetGlobalTimeDilation을 위해 포함

// 기존 로그 카테고리 정의
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
	ClimbTraceOffset = FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	ClimbTraceDistance = 150.0f;
	ClimbSpeed = 250.0f; // ClimbSpeed는 이제 사용하지 않습니다. (아래 Tick 함수에서 Velocity 설정 로직 삭제)
	bIsClimbing = false;
	ClimbTargetLocation = FVector::ZeroVector;

	ClimbMontageRef = nullptr; // 블루프린트에서 할당
	StartClimbLocation = FVector::ZeroVector;
	MontageStartTime = 0.0f;
	MontageTotalLength = 600.0f;
	bMontageAlreadyPlayingOnClimb = false;

	// =============== 슬로우 모션 변수 초기화 시작 ===============
	bIsSlowMotionActive = false;
	SlowMotionTimeDilationTarget = 0.2f; // 기본 슬로우 모션 속도 (20%)
	SlowMotionTransitionSpeed = 2.0f; // 기본 전환 속도
	// =============== 슬로우 모션 변수 초기화 끝 ===============
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

		// =============== 슬로우 모션 토글 바인딩 시작 ===============
		if (ToggleSlowMotionAction)
		{
			EnhancedInputComponent->BindAction(ToggleSlowMotionAction, ETriggerEvent::Started, this, &ATestProject2Character::ToggleSlowMotion); //
		}
		// =============== 슬로우 모션 토글 바인딩 끝 ===============
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
		/*UE_LOG(LogTemp, Warning, TEXT("Already climbing, returning."));*/
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

		// ClimbTargetLocation 설정:
		// 이 위치는 애니메이션이 끝났을 때 캐릭터가 '앞으로 나아가는' 동작까지 포함하여
		// 최종적으로 착지해야 하는 바닥의 정확한 위치여야 합니다.
		// 현재 ImpactPoint + Z 오프셋은 수직 클라이밍에만 맞춰져 있을 수 있습니다.
		// 애니메이션이 끝난 후 캐릭터가 착지하는 바닥 위치를 정확히 측정하여 이 값을 설정해야 합니다.
		// 예: ImpactPoint + FVector(X_Offset, Y_Offset, Z_Offset)
		// 일단 기존 로직을 유지하되, 이 값이 애니메이션 최종 포즈의 착지점과 일치하는지 확인 필요.
		ClimbTargetLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.0f); // 10.0f는 튜닝 필요

		// 클라이밍 시작 시점의 위치 저장
		StartClimbLocation = GetActorLocation();

		bIsClimbing = true;
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying); // 비행 모드 유지
		GetCharacterMovement()->StopMovementImmediately(); // 즉시 이동 정지
		// GetCharacterMovement()->Velocity = FVector::ZeroVector; // 더 이상 Tick에서 Velocity를 직접 제어하지 않으므로, 이 줄은 RemoveMovementInput을 대체하는 의미로만 남겨둠

		// 몽타주 재생 (C++에서 직접 호출)
		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (AnimInstance && ClimbMontageRef)
		{
			if (AnimInstance->Montage_IsPlaying(ClimbMontageRef)) // 몽타주가 이미 재생 중인 경우
			{
				AnimInstance->Montage_Stop(0.0f, ClimbMontageRef); // 기존 몽타주 정지 후 새로 재생
				bMontageAlreadyPlayingOnClimb = true;
			}
			else
			{
				bMontageAlreadyPlayingOnClimb = false;
			}
			AnimInstance->Montage_Play(ClimbMontageRef, 1.0f); // 1.0f 속도로 재생
			MontageStartTime = GetWorld()->GetTimeSeconds(); // 몽타주 재생 시작 시간 기록
			MontageTotalLength = ClimbMontageRef->GetPlayLength(); // 몽타주 총 길이 기록
			UE_LOG(LogTemp, Warning, TEXT("Montage Play Called from C++! Total Length: %f"), MontageTotalLength);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to play montage! Mesh, AnimInstance or ClimbMontageRef is null."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Raycast X"));
	}
}

// =============== 슬로우 모션 토글 함수 시작 ===============
void ATestProject2Character::ToggleSlowMotion()
{
	bIsSlowMotionActive = !bIsSlowMotionActive; // 슬로우 모션 상태 토글

	// 기존 타이머가 있다면 클리어
	GetWorldTimerManager().ClearTimer(SlowMotionTimerHandle); //

	// 새 타이머 시작 (UpdateSlowMotionDilation 함수를 반복 호출)
	GetWorldTimerManager().SetTimer(
		SlowMotionTimerHandle,
		this,
		&ATestProject2Character::UpdateSlowMotionDilation,
		0.01f, // 업데이트 주기 (예: 100fps로 업데이트)
		true   // 루핑
	);
}

// =============== 시간 딜레이 업데이트 함수 시작 ===============
void ATestProject2Character::UpdateSlowMotionDilation()
{
	float CurrentDilation = UGameplayStatics::GetGlobalTimeDilation(this); //
	float TargetDilation = bIsSlowMotionActive ? SlowMotionTimeDilationTarget : 1.0f; //

	// Lerp (선형 보간)를 사용하여 부드럽게 시간 딜레이 변경
	float NewDilation = FMath::FInterpTo(CurrentDilation, TargetDilation, GetWorld()->GetDeltaSeconds(), SlowMotionTransitionSpeed); //

	UGameplayStatics::SetGlobalTimeDilation(this, NewDilation); //

	// 목표 딜레이에 거의 도달했으면 타이머 중지
	if (FMath::IsNearlyEqual(NewDilation, TargetDilation, 0.01f)) // 오차 범위 0.01f
	{
		UGameplayStatics::SetGlobalTimeDilation(this, TargetDilation); // 정확히 목표 값으로 설정
		GetWorldTimerManager().ClearTimer(SlowMotionTimerHandle); // 타이머 중지
	}
}
// =============== 시간 딜레이 업데이트 함수 끝 ===============


void ATestProject2Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Climb 로직은 그대로 유지
	if (bIsClimbing)
	{
		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

		if (AnimInstance && ClimbMontageRef && AnimInstance->Montage_IsPlaying(ClimbMontageRef))
		{
			float CurrentMontageTime = GetWorld()->GetTimeSeconds() - MontageStartTime;
			float AnimProgress = FMath::Clamp(CurrentMontageTime / MontageTotalLength, 0.0f, 1.0f);

			// ===== 핵심 수정: ClimbZOffsetCurve를 사용하여 Z 오프셋 계산 =====
			float ZOffsetAlpha = 0.0f;
			if (ClimbZOffsetCurve) // 커브가 할당되어 있는지 확인
			{
				ZOffsetAlpha = ClimbZOffsetCurve->GetFloatValue(AnimProgress);
			}
			// =============================================================

			// 목표 Z 위치를 커브에 따라 보간합니다.
			// 시작 Z와 목표 Z의 차이 (총 이동량)에 ZOffsetAlpha를 곱합니다.
			float TargetZ = FMath::Lerp(StartClimbLocation.Z, ClimbTargetLocation.Z, ZOffsetAlpha);

			// 새로운 위치는 X와 Y는 시작 위치에서 목표 위치로 선형 보간하고, Z는 커브를 통해 얻은 TargetZ를 사용합니다.
			FVector NewLocation = FVector(
				FMath::Lerp(StartClimbLocation.X, ClimbTargetLocation.X, AnimProgress),
				FMath::Lerp(StartClimbLocation.Y, ClimbTargetLocation.Y, AnimProgress),
				TargetZ // Z축은 커브를 통해 계산된 값을 사용
			);

			// SetActorLocation으로 캐릭터 위치 강제 설정 (애니메이션 동기화)
			SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

			// 캐릭터 무브먼트의 Velocity는 0으로 유지 (수동 이동 제어 안함)
			GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
		else // 몽타주 재생이 끝났을 때
		{
			// 클라이밍 종료
			bIsClimbing = false;
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
			SetActorLocation(ClimbTargetLocation); // 마지막으로 정확한 목표 위치로 설정
			UE_LOG(LogTemp, Warning, TEXT("Climbing Finished, Montage ended."));
		}
	}
}
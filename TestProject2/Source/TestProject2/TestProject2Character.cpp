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
#include "GameFramework/PlayerController.h" 
#include "Components/AudioComponent.h" // UAudioComponent 사용을 위한 헤더 추가
#include "Sound/SoundCue.h" // USoundCue 사용을 위한 헤더 (필요시)
#include "Sound/SoundWave.h" // USoundWave 사용을 위한 헤더 (필요시)

// FPostProcessSettings를 사용하기 위해 필요한 헤더
#include "Engine/PostProcessVolume.h" // UCameraComponent.h에 FPostProcessSettings가 이미 포함되어 있을 가능성이 높습니다.


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

	// =============== 슬로우 모션 변수 초기화 시작 (protected 멤버이므로 생성자에서 초기화 가능) ===============
	bIsSlowMotionActive = false;
	SlowMotionTimeDilationTarget = 0.2f; // 기본 슬로우 모션 속도 (20%)
	SlowMotionTransitionSpeed = 2.0f;    // 기본 전환 속도

	// 흑백화를 위한 변수 초기화
	SlowMotionTargetSaturation = 0.0f; // 0.0f = 완전 흑백, 1.0f = 정상 컬러
	SlowMotionSaturationTransitionSpeed = 3.0f; // 채도 전환 속도
	// =============== 슬로우 모션 변수 초기화 끝 ===============

	// UCameraComponent의 PostProcessSettings 활성화 관련 설정
	// 생성자에서는 개별 PostProcessSettings 오버라이드 플래그를 설정하지 않습니다.
	// 이는 UpdateSlowMotionDilationAndSaturation 함수 내에서 동적으로 제어됩니다.
	// 만약 카메라 컴포넌트가 PostProcess설정을 사용하도록 기본적으로 활성화해야 한다면,
	// UCameraComponent의 PostProcessSettings 구조체의 멤버인 bOverride_* 플래그들을 기본값으로 설정할 수 있습니다.
	// 하지만, 현재 오류를 해결하기 위해 여기서는 아무것도 설정하지 않습니다.

	// =============== BGM_AudioComponent 초기화 시작 ===============
	BGM_AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("BGM_AudioComponent"));
	BGM_AudioComponent->SetupAttachment(RootComponent); // 캐릭터의 루트 컴포넌트에 부착
	BGM_AudioComponent->bAutoActivate = false; // 기본적으로 자동 재생 끄기 (BeginPlay에서 수동 재생)
	//BGM_AudioComponent->SetUISound(true); // UI 사운드로 설정하여 시간 딜레이의 영향을 받지 않도록 함 (BGM 목적)
	BGM_AudioComponent->SetVolumeMultiplier(1.0f); // 초기 볼륨 1.0f
	BGM_AudioComponent->SetPitchMultiplier(1.0f); // 초기 피치 1.0f (슬로우 모션 시 피치 변경을 원치 않으므로)

	BGM_Sound = nullptr; // 블루프린트에서 할당될 사운드
	BGM_SlowMotionVolumeTarget = 0.5f; // 슬로우 모션 시 BGM 목표 볼륨
	BGM_VolumeTransitionSpeed = 5.0f; // BGM 볼륨 전환 속도
	OriginalBGMVolume = 1.0f; // 원래 BGM 볼륨을 1.0으로 초기화
	// =============== BGM_AudioComponent 초기화 끝 ===============
}

void ATestProject2Character::BeginPlay()
{
	Super::BeginPlay();

	// BGM_AudioComponent에 사운드가 할당되어 있다면 재생
	if (BGM_AudioComponent && BGM_Sound)
	{
		BGM_AudioComponent->SetSound(BGM_Sound);
		BGM_AudioComponent->Play();
		OriginalBGMVolume = BGM_AudioComponent->VolumeMultiplier; // 현재 볼륨을 원본으로 저장
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BGM_AudioComponent or BGM_Sound not set. BGM will not play."));
	}
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
			EnhancedInputComponent->BindAction(ToggleSlowMotionAction, ETriggerEvent::Started, this, &ATestProject2Character::ToggleSlowMotion);
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

	if (Controller != nullptr)
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
	// ===== F2D 대신 FVector2D를 사용합니다. (이전 오류 해결) =====
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	// ===========================================

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

		ClimbTargetLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.0f); // 10.0f는 튜닝 필요

		// 클라이밍 시작 시점의 위치 저장
		StartClimbLocation = GetActorLocation();

		bIsClimbing = true;
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying); // 비행 모드 유지
		GetCharacterMovement()->StopMovementImmediately(); // 즉시 이동 정지

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
	GetWorldTimerManager().ClearTimer(SlowMotionTimerHandle);

	// 새 타이머 시작 (UpdateSlowMotionDilationAndSaturation 함수를 반복 호출)
	GetWorldTimerManager().SetTimer(
		SlowMotionTimerHandle,
		this,
		&ATestProject2Character::UpdateSlowMotionDilationAndSaturation,
		0.01f, // 업데이트 주기
		true    // 루핑
	);
}

// =============== 시간 딜레이와 채도 업데이트 함수 시작 ===============
void ATestProject2Character::UpdateSlowMotionDilationAndSaturation()
{
	// 1. 글로벌 시간 딜레이 조절
	float CurrentDilation = UGameplayStatics::GetGlobalTimeDilation(this);
	float TargetDilation = bIsSlowMotionActive ? SlowMotionTimeDilationTarget : 1.0f;

	float NewDilation = FMath::FInterpTo(CurrentDilation, TargetDilation, GetWorld()->GetDeltaSeconds(), SlowMotionTransitionSpeed);
	UGameplayStatics::SetGlobalTimeDilation(this, NewDilation);

	// 2. UCameraComponent의 Post Process 채도 조절
	if (FollowCamera) // FollowCamera가 유효한지 확인
	{
		FPostProcessSettings& CameraPPSettings = FollowCamera->PostProcessSettings; // 참조로 가져와서 바로 수정
		CameraPPSettings.bOverride_ColorSaturation = true;
		float CurrentSaturation = CameraPPSettings.ColorSaturation.X;
		float TargetSaturation = bIsSlowMotionActive ? SlowMotionTargetSaturation : 1.0f;
		float NewSaturation = FMath::FInterpTo(CurrentSaturation, TargetSaturation, GetWorld()->GetDeltaSeconds(), SlowMotionSaturationTransitionSpeed);
		CameraPPSettings.ColorSaturation = FVector4(NewSaturation, NewSaturation, NewSaturation, 1.0f);
	}

	// 3. BGM AudioComponent 볼륨 조절
	if (BGM_AudioComponent)
	{
		float CurrentBGMVolume = BGM_AudioComponent->VolumeMultiplier;
		float TargetBGMVolume = bIsSlowMotionActive ? BGM_SlowMotionVolumeTarget : OriginalBGMVolume; // 슬로우 모션 중이 아닐 때는 원래 볼륨으로 돌아감

		float NewBGMVolume = FMath::FInterpTo(CurrentBGMVolume, TargetBGMVolume, GetWorld()->GetDeltaSeconds(), BGM_VolumeTransitionSpeed);
		BGM_AudioComponent->SetVolumeMultiplier(NewBGMVolume);
	}

	// 4. 목표 딜레이, 채도, BGM 볼륨에 거의 도달했으면 타이머 중지
	bool bIsDilationNearlyEqual = FMath::IsNearlyEqual(NewDilation, TargetDilation, 0.01f);
	bool bIsSaturationNearlyEqual = true;
	bool bIsBGMVolumeNearlyEqual = true;

	if (FollowCamera)
	{
		float CurrentCameraSaturation = FollowCamera->PostProcessSettings.ColorSaturation.X;
		float TargetSaturation = bIsSlowMotionActive ? SlowMotionTargetSaturation : 1.0f;
		bIsSaturationNearlyEqual = FMath::IsNearlyEqual(CurrentCameraSaturation, TargetSaturation, 0.01f);
	}

	if (BGM_AudioComponent)
	{
		float CurrentBGMVolume = BGM_AudioComponent->VolumeMultiplier;
		float TargetBGMVolume = bIsSlowMotionActive ? BGM_SlowMotionVolumeTarget : OriginalBGMVolume;
		bIsBGMVolumeNearlyEqual = FMath::IsNearlyEqual(CurrentBGMVolume, TargetBGMVolume, 0.01f);
	}


	if (bIsDilationNearlyEqual && bIsSaturationNearlyEqual && bIsBGMVolumeNearlyEqual)
	{
		// 최종적으로 정확한 Dilation 설정
		UGameplayStatics::SetGlobalTimeDilation(this, TargetDilation);

		// 최종적으로 정확한 채도 설정 (FollowCamera를 통해)
		if (FollowCamera)
		{
			float FinalSaturation = bIsSlowMotionActive ? SlowMotionTargetSaturation : 1.0f;
			FollowCamera->PostProcessSettings.ColorSaturation = FVector4(FinalSaturation, FinalSaturation, FinalSaturation, 1.0f);
			if (!bIsSlowMotionActive) // 슬로우 모션이 비활성화될 때만 덮어쓰기 해제
			{
				FollowCamera->PostProcessSettings.bOverride_ColorSaturation = false;
			}
		}

		// 최종적으로 정확한 BGM 볼륨 설정
		if (BGM_AudioComponent)
		{
			float FinalBGMVolume = bIsSlowMotionActive ? BGM_SlowMotionVolumeTarget : OriginalBGMVolume;
			BGM_AudioComponent->SetVolumeMultiplier(FinalBGMVolume);
		}

		GetWorldTimerManager().ClearTimer(SlowMotionTimerHandle); // 타이머 중지
	}
}
// =============== 시간 딜레이와 채도 업데이트 함수 끝 ===============


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

			float ZOffsetAlpha = 0.0f;
			if (ClimbZOffsetCurve) // 커브가 할당되어 있는지 확인
			{
				ZOffsetAlpha = ClimbZOffsetCurve->GetFloatValue(AnimProgress);
			}

			float TargetZ = FMath::Lerp(StartClimbLocation.Z, ClimbTargetLocation.Z, ZOffsetAlpha);

			FVector NewLocation = FVector(
				FMath::Lerp(StartClimbLocation.X, ClimbTargetLocation.X, AnimProgress),
				FMath::Lerp(StartClimbLocation.Y, ClimbTargetLocation.Y, AnimProgress),
				TargetZ // Z축은 커브를 통해 계산된 값을 사용
			);

			SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

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
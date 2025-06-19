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
#include "Engine/PostProcessVolume.h" // UCameraComponent의 PostProcessSettings에 접근하기 위해 필요
#include "Curves/CurveFloat.h" // UCurveFloat 사용을 위한 헤더 추가
#include "TimerManager.h" // FTimerHandle, GetWorldTimerManager().SetTimer 등을 위해 필요

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
	ClimbTraceOffset = FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 30.0f);
	ClimbTraceDistance = 150.0f;
	ClimbSpeed = 250.0f; // ClimbSpeed는 이제 사용하지 않습니다. (아래 Tick 함수에서 Velocity 설정 로직 삭제)
	bIsClimbing = false;
	ClimbTargetLocation = FVector::ZeroVector; // 기본값 초기화
	StartClimbLocation = FVector::ZeroVector; // 기본값 초기화
	ClimbInterpSpeed = 5.0f; // 클라이밍 보간 속도 초기화 (새로 추가)

	ClimbMontageRef = nullptr; // 블루프린트에서 할당
	ClimbZOffsetCurve = nullptr; // 블루프린트에서 할당 <--- 이 부분이 nullptr로 잘 초기화 되어있어야 합니다.
	MontageStartTime = 0.0f;
	MontageTotalLength = 600.0f; // 실제 몽타주 길이에 맞게 설정하거나 동적으로 가져와야 함
	bMontageAlreadyPlayingOnClimb = false;

	// =============== 슬로우 모션 변수 초기화 시작 (protected 멤버이므로 생성자에서 초기화 가능) ===============
	bIsSlowMotionActive = false;
	SlowMotionTimeDilationTarget = 0.2f; // 기본 슬로우 모션 속도 (20%)
	SlowMotionTransitionSpeed = 2.0f; // 기본 전환 속도

	// 흑백화를 위한 변수 초기화
	SlowMotionTargetSaturation = 0.0f; // 0.0f = 완전 흑백, 1.0f = 정상 컬러
	SlowMotionSaturationTransitionSpeed = 3.0f; // 채도 전환 속도
	// =============== 슬로우 모션 변수 초기화 끝 ===============

	// =============== BGM_AudioComponent 초기화 시작 ===============
	BGM_AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("BGM_AudioComponent"));
	BGM_AudioComponent->SetupAttachment(RootComponent); // 캐릭터의 루트 컴포넌트에 부착
	BGM_AudioComponent->bAutoActivate = false; // 기본적으로 자동 재생 끄기 (BeginPlay에서 수동 재생)
	BGM_AudioComponent->SetVolumeMultiplier(1.0f); // 초기 볼륨 1.0f
	BGM_AudioComponent->SetPitchMultiplier(1.0f); // 초기 피치 1.0f

	BGM_Sound = nullptr; // 블루프린트에서 할당될 사운드
	BGM_SlowMotionPitchTarget = 0.5f; // 슬로우 모션 시 BGM 목표 피치
	OriginalBGMPitch = 1.0f; // 원래 BGM 피치를 1.0으로 초기화

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
		OriginalBGMPitch = BGM_AudioComponent->PitchMultiplier; // 현재 피치를 원본으로 저장
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
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr && !bIsClimbing) // 클라이밍 중에는 시야 조작도 제한 (선택 사항)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATestProject2Character::TryClimb()
{
	UE_LOG(LogTemp, Warning, TEXT("TryClimb function entered. bIsClimbing: %s"), bIsClimbing ? TEXT("True") : TEXT("False"));

	// 이미 클라이밍 중이면 함수를 종료
	if (bIsClimbing)
	{
		return;
	}

	// Line Trace의 시작점 계산
	FVector StartLocation = GetActorLocation() + GetActorForwardVector() * ClimbTraceOffset.X + FVector(0.0f, 0.0f, ClimbTraceOffset.Z);

	// Line Trace의 끝점 계산
	FVector EndLocation = StartLocation + GetActorForwardVector() * ClimbTraceDistance;

	FHitResult HitResult; // Line Trace 결과를 저장할 변수
	FCollisionQueryParams Params; // 충돌 쿼리 파라미터
	Params.AddIgnoredActor(this); // Trace 시 자기 자신은 무시하도록 설정 (캐릭터 자신과의 충돌 방지)

	// 실제 Line Trace 수행
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECC_Visibility, // 또는 원하는 Collision Channel
		Params
	);

	// 디버깅용 Line Trace 그리기 (에디터/게임에서 시각적으로 확인)
	DrawDebugLine(
		GetWorld(),
		StartLocation,
		EndLocation,
		bHit ? FColor::Green : FColor::Red,
		false,
		0.1f,
		SDPG_Foreground, // 항상 위에 보이도록 설정
		2.0f
	);

	// Line Trace가 오브젝트와 충돌했고, 충돌한 액터가 유효하며, "Climbable" 태그를 가지고 있다면
	if (bHit && HitResult.GetActor() && HitResult.GetActor()->Tags.Contains(FName("Climbable")))
	{
		UE_LOG(LogTemp, Warning, TEXT("올라갈 수 있는 오브젝트 (%s) 를 발견했습니다."), *HitResult.GetActor()->GetName());

		// === 기존 ClimbTargetLocation 계산 방식 (수정될 부분) ===
		// float VerticalOffset = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.0f;
		// float InwardOffset = 15.0f;
		// ClimbTargetLocation = HitResult.ImpactPoint - (HitResult.ImpactNormal * InwardOffset) + FVector(0.0f, 0.0f, VerticalOffset);

		// === 새로운 ClimbTargetLocation 계산 방식 ===
		// 1. 벽에 닿은 충돌 지점 (ImpactPoint)을 기준으로 합니다.
		FVector BaseLocation = HitResult.ImpactPoint;

		// 2. 캐릭터 캡슐의 반지름만큼 벽면 법선 반대 방향으로 이동시켜 벽에 딱 붙도록 합니다.
		//    GetScaledCapsuleRadius()는 캡슐의 반지름을 반환합니다.
		float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
		FVector AdjustedXY = BaseLocation + (HitResult.ImpactNormal * CapsuleRadius); // 벽에서 캡슐 반지름만큼 밖으로

		// 3. Z축은 캐릭터 캡슐의 절반 높이와 약간의 추가 오프셋을 더하여 올라갈 최종 높이를 설정합니다.
		float VerticalOffset = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 30.0f; // 기존 오프셋 유지
		float TargetZ = HitResult.ImpactPoint.Z + VerticalOffset;

		// 최종 ClimbTargetLocation 설정
		// X, Y는 벽에 딱 붙는 위치, Z는 올라갈 최종 높이
		ClimbTargetLocation = FVector(AdjustedXY.X, AdjustedXY.Y, TargetZ);

		// 중요: 시작 시 캐릭터의 현재 X, Y, Z가 아닌, 벽에 붙은 상태에서의 시작 X,Y와 현재 Z를 StartClimbLocation으로 설정합니다.
		// 이렇게 해야 X,Y 이동 보간이 벽면에서 시작됩니다.
		StartClimbLocation = GetActorLocation(); // 현재 위치를 시작점으로 설정하고,

		// 하지만, 사실상 X, Y는 벽면에 붙은 지점으로 바로 이동시키거나, 매우 빠른 속도로 보간해야 합니다.
		// 만약 애니메이션과 함께 부드럽게 벽으로 다가가길 원한다면, 추가적인 중간 보간 단계가 필요할 수 있습니다.
		// 여기서는 일단 최종 목표 Z는 위에 계산된 TargetZ를 사용하고, X,Y는 캐릭터의 현재 X,Y를 시작으로 합니다.
		// 하지만, 더 나은 방법은 StartClimbLocation의 X,Y도 벽에 붙은 위치로 조정하는 것입니다.
		// 예를 들어,
		// FVector TempStartXY = GetActorLocation() + (HitResult.ImpactNormal * (CapsuleRadius - GetCapsuleComponent()->GetScaledCapsuleRadius()));
		// StartClimbLocation = FVector(TempStartXY.X, TempStartXY.Y, GetActorLocation().Z);
		// 이 부분은 몽타주와 캐릭터 움직임이 어떻게 어우러질지에 따라 달라질 수 있습니다.
		// 현재 구현 방식은 시작 시점에서 최종 목적지까지 (X,Y)와 (Z)를 보간합니다.

		// 클라이밍 시작 시점의 캐릭터 위치 저장 (기존 방식 유지)
		StartClimbLocation = GetActorLocation();


		bIsClimbing = true; // 클라이밍 상태 활성화

		// 캐릭터 이동 모드를 MOVE_Flying으로 변경하여 중력 및 지면과의 상호작용 무시
		// 클라이밍 애니메이션 및 로직에 따라 자유롭게 움직이도록 함
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
		GetCharacterMovement()->StopMovementImmediately(); // 즉시 현재 이동 정지

		// 몽타주 재생 (블루프린트에서 ClimbMontageRef를 할당해야 함)
		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (AnimInstance && ClimbMontageRef)
		{
			AnimInstance->Montage_Play(ClimbMontageRef);
			MontageStartTime = GetWorld()->GetTimeSeconds(); // 몽타주 시작 시간 기록
			MontageTotalLength = ClimbMontageRef->GetPlayLength(); // 몽타주 총 길이 가져오기
		}

		OnClimbStarted(); // 블루프린트 이벤트 호출

		// 클라이밍 시작 시점과 목표 지점 사이를 보간하는 타이머 설정 (UpdateClimbProgress 함수 호출)
		// GetWorld()->GetDeltaSeconds()를 주기 값으로 사용하여 매 프레임마다 호출되도록 함
		GetWorldTimerManager().SetTimer(ClimbTimerHandle, this, &ATestProject2Character::UpdateClimbProgress, GetWorld()->GetDeltaSeconds(), true);
	}
	else
	{
		// Line Trace가 유효한 climbable 오브젝트를 찾지 못했을 때의 로그
		UE_LOG(LogTemp, Warning, TEXT("Raycast X (올라갈 수 있는 오브젝트를 찾지 못했습니다)."));
	}
}

void ATestProject2Character::UpdateClimbProgress()
{
	// 클라이밍 상태가 아니면 타이머를 중지하고 함수 종료
	if (!bIsClimbing)
	{
		GetWorldTimerManager().ClearTimer(ClimbTimerHandle);
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	float AnimProgress = 0.0f; // 애니메이션 진행률 (0.0 ~ 1.0)

	// 몽타주 재생 여부 확인 및 진행률 계산
	if (AnimInstance && ClimbMontageRef && AnimInstance->Montage_IsPlaying(ClimbMontageRef))
	{
		float CurrentMontageTime = GetWorld()->GetTimeSeconds() - MontageStartTime;
		AnimProgress = FMath::Clamp(CurrentMontageTime / MontageTotalLength, 0.0f, 1.0f);
	}
	else // 몽타주가 재생되지 않거나 끝났을 경우
	{
		// 몽타주가 끝났음을 감지, 클라이밍을 종료합니다.
		FinishClimb();
		return;
	}


	FVector CurrentLocation = GetActorLocation();

	// X, Y 축은 ClimbTargetLocation으로 FInterpTo 보간
	// Z 축은 ClimbZOffsetCurve와 함께 보간
	FVector TargetLocationXY = FVector(ClimbTargetLocation.X, ClimbTargetLocation.Y, CurrentLocation.Z); // Z는 나중에 계산

	FVector NewLocationXY = FMath::VInterpTo(CurrentLocation, TargetLocationXY, GetWorld()->GetDeltaSeconds(), ClimbInterpSpeed);

	// Z축 계산: ClimbZOffsetCurve를 사용하여 StartClimbLocation.Z와 ClimbTargetLocation.Z 사이를 보간
	float TargetZ = CurrentLocation.Z; // 기본값으로 현재 Z를 설정
	if (ClimbZOffsetCurve)
	{
		// AnimProgress를 사용하여 커브 값 가져오기
		float ZOffsetAlpha = ClimbZOffsetCurve->GetFloatValue(AnimProgress);
		// StartClimbLocation.Z에서 ClimbTargetLocation.Z까지 ZOffsetAlpha에 따라 선형 보간
		TargetZ = FMath::Lerp(StartClimbLocation.Z, ClimbTargetLocation.Z, ZOffsetAlpha);
	}
	else
	{
		// 커브가 없으면 단순히 목표 Z로 보간
		TargetZ = FMath::FInterpTo(CurrentLocation.Z, ClimbTargetLocation.Z, GetWorld()->GetDeltaSeconds(), ClimbInterpSpeed);
	}

	FVector NewLocation = FVector(NewLocationXY.X, NewLocationXY.Y, TargetZ);
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// 목표 위치에 충분히 가까워졌는지 확인 (X,Y,Z 모두)
	// 몽타주 진행률이 1.0에 가까워지거나 목표 위치에 충분히 가까워지면 종료
	if (AnimProgress >= 0.99f || FVector::DistSquared(CurrentLocation, ClimbTargetLocation) < FMath::Square(5.0f))
	{
		FinishClimb(); // 클라이밍 완료 로직 호출
	}
}

void ATestProject2Character::FinishClimb()
{
	UE_LOG(LogTemp, Warning, TEXT("Climbing Finished!"));

	bIsClimbing = false; // 클라이밍 상태 비활성화
	GetWorldTimerManager().ClearTimer(ClimbTimerHandle); // 클라이밍 타이머 정지

	// 캐릭터 이동 모드를 다시 Walking으로 변경하여 일반적인 이동 가능하게 함
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);

	// 최종적으로 목표 위치에 정확히 스냅
	SetActorLocation(ClimbTargetLocation);

	// 몽타주가 재생 중이었다면 정지 (선택 사항, 몽타주가 자연스럽게 끝나도록 둘 수도 있음)
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && ClimbMontageRef && AnimInstance->Montage_IsPlaying(ClimbMontageRef))
	{
		AnimInstance->Montage_Stop(0.2f, ClimbMontageRef); // 부드럽게 정지
	}

	// 필요하다면 클라이밍 완료 애니메이션 재생 등 추가 로직
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
		true // 루핑
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
		CameraPPSettings.bOverride_ColorSaturation = true; // 채도 오버라이드 활성화

		float CurrentSaturation = CameraPPSettings.ColorSaturation.X; // 현재 채도
		float TargetSaturation = bIsSlowMotionActive ? SlowMotionTargetSaturation : 1.0f; // 목표 채도
		float NewSaturation = FMath::FInterpTo(CurrentSaturation, TargetSaturation, GetWorld()->GetDeltaSeconds(), SlowMotionSaturationTransitionSpeed);
		CameraPPSettings.ColorSaturation = FVector4(NewSaturation, NewSaturation, NewSaturation, 1.0f); // 모든 채널에 적용
	}

	// 3. BGM AudioComponent 피치 조절
	if (BGM_AudioComponent)
	{
		float CurrentBGMPitch = BGM_AudioComponent->PitchMultiplier;
		float TargetBGMPitch = bIsSlowMotionActive ? BGM_SlowMotionPitchTarget : OriginalBGMPitch;

		float NewBGMPitch = FMath::FInterpTo(CurrentBGMPitch, TargetBGMPitch, GetWorld()->GetDeltaSeconds(), SlowMotionTransitionSpeed);
		BGM_AudioComponent->SetPitchMultiplier(NewBGMPitch);
	}

	// 4. 목표 딜레이, 채도, BGM 피치에 거의 도달했으면 타이머 중지
	bool bIsDilationNearlyEqual = FMath::IsNearlyEqual(NewDilation, TargetDilation, 0.01f);
	bool bIsSaturationNearlyEqual = true;
	bool bIsBGMPitchNearlyEqual = true;

	if (FollowCamera)
	{
		float CurrentCameraSaturation = FollowCamera->PostProcessSettings.ColorSaturation.X;
		float TargetSaturation = bIsSlowMotionActive ? SlowMotionTargetSaturation : 1.0f;
		bIsSaturationNearlyEqual = FMath::IsNearlyEqual(CurrentCameraSaturation, TargetSaturation, 0.01f);
	}

	if (BGM_AudioComponent)
	{
		float CurrentBGMPitch = BGM_AudioComponent->PitchMultiplier;
		float TargetBGMPitch = bIsSlowMotionActive ? BGM_SlowMotionPitchTarget : OriginalBGMPitch;
		bIsBGMPitchNearlyEqual = FMath::IsNearlyEqual(CurrentBGMPitch, TargetBGMPitch, 0.01f);
	}


	if (bIsDilationNearlyEqual && bIsSaturationNearlyEqual && bIsBGMPitchNearlyEqual)
	{
		// 최종적으로 정확한 Dilation 설정
		UGameplayStatics::SetGlobalTimeDilation(this, TargetDilation);

		// 최종적으로 정확한 채도 설정
		if (FollowCamera)
		{
			float FinalSaturation = bIsSlowMotionActive ? SlowMotionTargetSaturation : 1.0f;
			FollowCamera->PostProcessSettings.ColorSaturation = FVector4(FinalSaturation, FinalSaturation, FinalSaturation, 1.0f);
			if (!bIsSlowMotionActive) // 슬로우 모션이 비활성화될 때만 덮어쓰기 해제
			{
				FollowCamera->PostProcessSettings.bOverride_ColorSaturation = false;
			}
		}

		// 최종적으로 정확한 BGM 피치 설정
		if (BGM_AudioComponent)
		{
			float FinalBGMPitch = bIsSlowMotionActive ? BGM_SlowMotionPitchTarget : OriginalBGMPitch;
			BGM_AudioComponent->SetPitchMultiplier(FinalBGMPitch);
		}

		GetWorldTimerManager().ClearTimer(SlowMotionTimerHandle); // 타이머 중지
	}
}
// =============== 시간 딜레이와 채도 업데이트 함수 끝 ===============


void ATestProject2Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 클라이밍 중에는 CharacterMovementComponent가 MOVE_Flying 상태이고
	// UpdateClimbProgress 함수가 타이머에 의해 위치 보간을 처리하므로,
	// Tick에서는 단순히 속도를 0으로 유지하여 불필요한 움직임을 방지합니다.
	if (bIsClimbing)
	{
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
	// bIsClimbing이 아닐 때는 일반적인 CharacterMovementComponent 로직이 작동
}

void ATestProject2Character::PerformRaycast()
{
	// 이 함수는 현재 사용되지 않음.
	// TryClimb 함수가 직접 Line Trace를 수행합니다.
}
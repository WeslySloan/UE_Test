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
#include "DrawDebugHelpers.h" 
#include "Kismet/KismetMathLibrary.h" 
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h" 
#include "GameFramework/PlayerController.h" 
#include "Components/AudioComponent.h" // UAudioComponent 사용을 위한 헤더
#include "Sound/SoundCue.h" // USoundCue 사용을 위한 헤더 (필요시)
#include "Sound/SoundWave.h" // USoundWave 사용을 위한 헤더 (필요시)
#include "Sound/SoundBase.h" // USoundBase (SoundCue, SoundWave 모두 포함)
#include "Engine/PostProcessVolume.h"
#include "Curves/CurveFloat.h" 

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
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

    GetCharacterMovement()->JumpZVelocity = 700.f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
    GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

    // Create a camera boom
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    // Create a follow camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // "올라가기" 관련 변수 초기화
    ClimbTraceOffset = FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 30.0f);
    ClimbTraceDistance = 150.0f;
    ClimbSpeed = 250.0f;
    bIsClimbing = false;
    ClimbTargetLocation = FVector::ZeroVector;

    ClimbMontageRef = nullptr;
    StartClimbLocation = FVector::ZeroVector;
    MontageStartTime = 0.0f;
    MontageTotalLength = 600.0f;
    bMontageAlreadyPlayingOnClimb = false;

    // =============== 슬로우 모션 변수 초기화 시작 ===============
    bIsSlowMotionActive = false;
    SlowMotionTimeDilationTarget = 0.2f;
    SlowMotionTransitionSpeed = 2.0f;

    SlowMotionTargetSaturation = 0.0f;
    SlowMotionSaturationTransitionSpeed = 3.0f;
    // =============== 슬로우 모션 변수 초기화 끝 ===============

    // =============== BGM_AudioComponent 초기화 시작 (이미 존재) ===============
    BGM_AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("BGM_AudioComponent"));
    BGM_AudioComponent->SetupAttachment(RootComponent); // 캐릭터의 루트 컴포넌트에 부착
    BGM_AudioComponent->bAutoActivate = false; // 기본적으로 자동 재생 끄기 (BeginPlay에서 수동 재생)
    BGM_AudioComponent->SetVolumeMultiplier(1.0f); // 초기 볼륨 1.0f
    BGM_AudioComponent->SetPitchMultiplier(1.0f); // 초기 피치 1.0f

    BGM_Sound = nullptr; // 블루프린트에서 할당될 사운드
    BGM_SlowMotionPitchTarget = 0.5f;
    OriginalBGMPitch = 1.0f;

    // =============== BGM 토글을 위한 변수 초기화 시작 ===============
    bIsBGMPlaying = true; // BGM은 기본적으로 켜진 상태로 시작
    OriginalBGMVolume = 1.0f; // BGM의 원래 볼륨을 1.0으로 초기화 (BeginPlay에서 실제 설정된 볼륨을 가져옴)
    // =============== BGM 토글을 위한 변수 초기화 끝 ===============
}

void ATestProject2Character::BeginPlay()
{
    Super::BeginPlay();

    // BGM_AudioComponent에 사운드가 할당되어 있다면 재생
    if (BGM_AudioComponent && BGM_Sound)
    {
        BGM_AudioComponent->SetSound(BGM_Sound);
        BGM_AudioComponent->Play(); // 사운드재생코드
        OriginalBGMVolume = BGM_AudioComponent->VolumeMultiplier; // BeginPlay 시점의 실제 볼륨을 OriginalBGMVolume에 저장
        OriginalBGMPitch = BGM_AudioComponent->PitchMultiplier;
        bIsBGMPlaying = true; // BeginPlay에서 재생 시작했으므로 상태를 True로 설정
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BGM_AudioComponent or BGM_Sound not set. BGM will not play."));
        bIsBGMPlaying = false; // 재생되지 않으면 상태를 False로 설정
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

        // =============== BGM 토글 액션 바인딩 추가 시작 ===============
        if (ToggleBGMAction)
        {
            EnhancedInputComponent->BindAction(ToggleBGMAction, ETriggerEvent::Started, this, &ATestProject2Character::ToggleBGMVolume);
        }
        // =============== BGM 토글 액션 바인딩 추가 끝 ===============
    }
    else
    {
        UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
    }
}

void ATestProject2Character::Move(const FInputActionValue& Value)
{
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (bIsClimbing)
    {
        return;
    }

    if (Controller != nullptr)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void ATestProject2Character::Look(const FInputActionValue& Value)
{
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr && !bIsClimbing)
    {
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
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        ECC_Visibility,
        Params
    );

    DrawDebugLine(
        GetWorld(),
        StartLocation,
        EndLocation,
        bHit ? FColor::Green : FColor::Red,
        false,
        0.1f,
        SDPG_Foreground,
        2.0f
    );

    if (bHit && HitResult.GetActor() && HitResult.GetActor()->Tags.Contains(FName("Climbable")))
    {
        UE_LOG(LogTemp, Warning, TEXT("올라갈 수 있는 오브젝트 (%s) 를 발견했습니다."), *HitResult.GetActor()->GetName());

        ClimbTargetLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20.0f);

        StartClimbLocation = GetActorLocation();

        bIsClimbing = true;
        GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
        GetCharacterMovement()->StopMovementImmediately();

        UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
        if (AnimInstance && ClimbMontageRef)
        {
            if (AnimInstance->Montage_IsPlaying(ClimbMontageRef))
            {
                AnimInstance->Montage_Stop(0.0f, ClimbMontageRef);
                bMontageAlreadyPlayingOnClimb = true;
            }
            else
            {
                bMontageAlreadyPlayingOnClimb = false;
            }
            AnimInstance->Montage_Play(ClimbMontageRef, 1.0f);
            MontageStartTime = GetWorld()->GetTimeSeconds();
            MontageTotalLength = ClimbMontageRef->GetPlayLength();
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
    bIsSlowMotionActive = !bIsSlowMotionActive;

    GetWorldTimerManager().ClearTimer(SlowMotionTimerHandle);

    GetWorldTimerManager().SetTimer(
        SlowMotionTimerHandle,
        this,
        &ATestProject2Character::UpdateSlowMotionDilationAndSaturation,
        0.01f,
        true
    );
}

void ATestProject2Character::UpdateSlowMotionDilationAndSaturation()
{
    float CurrentDilation = UGameplayStatics::GetGlobalTimeDilation(this);
    float TargetDilation = bIsSlowMotionActive ? SlowMotionTimeDilationTarget : 1.0f;

    float NewDilation = FMath::FInterpTo(CurrentDilation, TargetDilation, GetWorld()->GetDeltaSeconds(), SlowMotionTransitionSpeed);
    UGameplayStatics::SetGlobalTimeDilation(this, NewDilation);

    if (FollowCamera)
    {
        FPostProcessSettings& CameraPPSettings = FollowCamera->PostProcessSettings;
        CameraPPSettings.bOverride_ColorSaturation = true;
        float CurrentSaturation = CameraPPSettings.ColorSaturation.X;
        float TargetSaturation = bIsSlowMotionActive ? SlowMotionTargetSaturation : 1.0f;
        float NewSaturation = FMath::FInterpTo(CurrentSaturation, TargetSaturation, GetWorld()->GetDeltaSeconds(), SlowMotionSaturationTransitionSpeed);
        CameraPPSettings.ColorSaturation = FVector4(NewSaturation, NewSaturation, NewSaturation, 1.0f);
    }

    if (BGM_AudioComponent)
    {
        float CurrentBGMPitch = BGM_AudioComponent->PitchMultiplier;
        float TargetBGMPitch = bIsSlowMotionActive ? BGM_SlowMotionPitchTarget : OriginalBGMPitch;

        float NewBGMPitch = FMath::FInterpTo(CurrentBGMPitch, TargetBGMPitch, GetWorld()->GetDeltaSeconds(), SlowMotionTransitionSpeed);
        BGM_AudioComponent->SetPitchMultiplier(NewBGMPitch);
    }

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
        UGameplayStatics::SetGlobalTimeDilation(this, TargetDilation);

        if (FollowCamera)
        {
            float FinalSaturation = bIsSlowMotionActive ? SlowMotionTargetSaturation : 1.0f;
            FollowCamera->PostProcessSettings.ColorSaturation = FVector4(FinalSaturation, FinalSaturation, FinalSaturation, 1.0f);
            if (!bIsSlowMotionActive)
            {
                FollowCamera->PostProcessSettings.bOverride_ColorSaturation = false;
            }
        }

        if (BGM_AudioComponent)
        {
            float FinalBGMPitch = bIsSlowMotionActive ? BGM_SlowMotionPitchTarget : OriginalBGMPitch;
            BGM_AudioComponent->SetPitchMultiplier(FinalBGMPitch);
        }

        GetWorldTimerManager().ClearTimer(SlowMotionTimerHandle);
    }
}
// =============== 시간 딜레이와 채도 업데이트 함수 끝 ===============

// =============== BGM 토글 함수 구현 시작 ===============
void ATestProject2Character::ToggleBGMVolume()
{
    if (!BGM_AudioComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("BGM_AudioComponent is null, cannot toggle BGM."));
        return;
    }

    bIsBGMPlaying = !bIsBGMPlaying; // BGM 재생 상태 토글

    if (bIsBGMPlaying)
    {
        // BGM을 켜는 경우: 원래 볼륨으로 설정
        BGM_AudioComponent->SetVolumeMultiplier(OriginalBGMVolume);
        BGM_AudioComponent->Play(); // 정지되어 있었다면 다시 재생
        UE_LOG(LogTemp, Warning, TEXT("BGM ON. Volume: %f"), OriginalBGMVolume);
    }
    else
    {
        // BGM을 끄는 경우: 볼륨을 0으로 설정하고 정지
        BGM_AudioComponent->SetVolumeMultiplier(0.0f);
        BGM_AudioComponent->Stop(); // 완전히 정지
        UE_LOG(LogTemp, Warning, TEXT("BGM OFF. Volume: 0.0"));
    }
}
// =============== BGM 토글 함수 구현 끝 ===============


void ATestProject2Character::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    DrawDebugBox(
        GetWorld(),
        GetActorLocation(),
        FVector(5.0f, 5.0f, 5.0f),
        FColor::Blue,
        false,
        -1.0f,
        0,
        1.0f
    );

    if (bIsClimbing)
    {
        DrawDebugBox(
            GetWorld(),
            ClimbTargetLocation,
            FVector(10.0f, 10.0f, 10.0f),
            FColor::Red,
            false,
            -1.0f,
            0,
            2.0f
        );
    }

    if (bIsClimbing)
    {
        UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

        if (AnimInstance && ClimbMontageRef && AnimInstance->Montage_IsPlaying(ClimbMontageRef))
        {
            float CurrentMontageTime = GetWorld()->GetTimeSeconds() - MontageStartTime;
            float AnimProgress = FMath::Clamp(CurrentMontageTime / MontageTotalLength, 0.0f, 1.0f);

            float ZOffsetAlpha = 0.0f;
            if (ClimbZOffsetCurve)
            {
                ZOffsetAlpha = ClimbZOffsetCurve->GetFloatValue(AnimProgress);
            }

            float TargetZ = FMath::Lerp(StartClimbLocation.Z, ClimbTargetLocation.Z, ZOffsetAlpha);

            FVector NewLocation = FVector(
                FMath::Lerp(StartClimbLocation.X, ClimbTargetLocation.X, AnimProgress),
                FMath::Lerp(StartClimbLocation.Y, ClimbTargetLocation.Y, AnimProgress),
                TargetZ
            );

            SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

            GetCharacterMovement()->Velocity = FVector::ZeroVector;
        }
        else
        {
            bIsClimbing = false;
            GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
            SetActorLocation(ClimbTargetLocation);
            UE_LOG(LogTemp, Warning, TEXT("Climbing Finished, Montage ended."));
        }
    }
}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "TestProject2Character.generated.h"

// 기존 로그 카테고리 정의 (이미 있을 수 있음)
DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UAudioComponent; // UAudioComponent 선언을 위해 추가
class USoundCue;      // USoundCue 선언을 위해 추가
class USoundWave;     // USoundWave 선언을 위해 추가

UCLASS(config = Game)
class ATestProject2Character : public ACharacter
{
    GENERATED_BODY()

    /** Camera boom positioning the camera behind the character */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* CameraBoom;

    /** Follow camera */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    UCameraComponent* FollowCamera;

    /** MappingContext */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputMappingContext* DefaultMappingContext;

    /** Jump Input Action */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* JumpAction;

    /** Move Input Action */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* MoveAction;

    /** Look Input Action */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* LookAction;

    // "올라가기" Input Action
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* ClimbAction;

    // =============== 슬로우 모션 Input Action 시작 ===============
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* ToggleSlowMotionAction;
    // =============== 슬로우 모션 Input Action 끝 ===============

    // =============== BGM 토글 Input Action 추가 시작 ===============
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* ToggleBGMAction; // BGM 토글을 위한 새로운 Input Action
    // =============== BGM 토글 Input Action 추가 끝 ===============

public:
    ATestProject2Character();

protected:
    /** Called for movement input */
    void Move(const FInputActionValue& Value);

    /** Called for looking input */
    void Look(const FInputActionValue& Value);

    // "올라가기" 함수
    void TryClimb();

    // =============== 슬로우 모션 토글 함수 시작 ===============
    void ToggleSlowMotion();
    void UpdateSlowMotionDilationAndSaturation();
    // =============== 슬로우 모션 토글 함수 끝 ===============

    // =============== BGM 토글 함수 추가 시작 ===============
    void ToggleBGMVolume(); // BGM 볼륨 토글을 처리할 함수
    // =============== BGM 토글 함수 추가 끝 ===============

protected:
    // APawn interface
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // To add mapping context
    virtual void NotifyControllerChanged() override;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

public:
    /** Returns CameraBoom subobject **/
    FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    /** Returns FollowCamera subobject **/
    FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

    // "올라가기" 관련 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    FVector ClimbTraceOffset;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    float ClimbTraceDistance;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    float ClimbSpeed; // 이제 사용하지 않습니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climb")
    bool bIsClimbing;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climb")
    FVector ClimbTargetLocation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    UAnimMontage* ClimbMontageRef; // 블루프린트에서 할당
    FVector StartClimbLocation;
    float MontageStartTime;
    float MontageTotalLength;
    bool bMontageAlreadyPlayingOnClimb;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    UCurveFloat* ClimbZOffsetCurve; // Z축 오프셋을 위한 커브

    // =============== 슬로우 모션 변수 시작 ===============
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SlowMotion")
    bool bIsSlowMotionActive;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowMotion")
    float SlowMotionTimeDilationTarget;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowMotion")
    float SlowMotionTransitionSpeed;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowMotion")
    float SlowMotionTargetSaturation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowMotion")
    float SlowMotionSaturationTransitionSpeed;

    FTimerHandle SlowMotionTimerHandle; // 슬로우 모션 타이머 핸들
    // =============== 슬로우 모션 변수 끝 ===============

    // =============== BGM AudioComponent 변수 시작 (이미 존재) ===============
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Audio)
    UAudioComponent* BGM_AudioComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Audio)
    USoundBase* BGM_Sound; // USoundCue 대신 USoundBase를 사용하여 SoundCue 또는 SoundWave 모두 할당 가능

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Audio)
    float BGM_SlowMotionPitchTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Audio)
    float OriginalBGMPitch;

    // =============== BGM 토글을 위한 변수 추가 시작 ===============
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Audio)
    bool bIsBGMPlaying; // BGM이 현재 재생 중인지 여부 (토글 상태 추적)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Audio)
    float OriginalBGMVolume; // BGM의 원래 볼륨을 저장하여 토글 시 복원
    // =============== BGM 토글을 위한 변수 추가 끝 ===============

    // Tick 함수 (이미 존재)
    virtual void Tick(float DeltaTime) override;
};
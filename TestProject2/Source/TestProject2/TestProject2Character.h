// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "TestProject2Character.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UAudioComponent; // UAudioComponent 헤더 추가

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

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

	/** "올라가기 시도" 액션 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ClimbAction;

protected: // 이 protected 섹션에 슬로우 모션 관련 함수와 기존 protected 멤버들이 위치합니다.

	// =============== 슬로우 모션 및 흑백화 관련 UPROPERTY 추가 부분 시작 ===============
	/** 슬로우 모션 토글 Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ToggleSlowMotionAction; // IA_ToggleSlowMotion과 매핑될 액션

	/** 슬로우 모션 활성 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SlowMotion")
	bool bIsSlowMotionActive;

	/** 슬로우 모션 시간 비율 (예: 0.2) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SlowMotion")
	float SlowMotionTimeDilationTarget; // 목표 시간 딜레이 값 (0.2, 1.0)

	/** 슬로우 모션 전환 속도 (Lerp에 사용, 초당 Time Dilation 변화량) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SlowMotion")
	float SlowMotionTransitionSpeed;

	/** 슬로우 모션 시 목표 채도 (0.0f = 흑백, 1.0f = 풀컬러) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SlowMotion")
	float SlowMotionTargetSaturation;

	/** 채도 전환 속도 (Lerp에 사용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SlowMotion")
	float SlowMotionSaturationTransitionSpeed;

	/** 슬로우 모션 전환 타이머 핸들 */
	FTimerHandle SlowMotionTimerHandle;
	// =============== 슬로우 모션 및 흑백화 관련 UPROPERTY 추가 부분 끝 ===============

	// =============== BGM_AudioComponent 관련 UPROPERTY 추가 시작 ===============
	/** BGM Audio Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio", meta = (AllowPrivateAccess = "true"))
	UAudioComponent* BGM_AudioComponent;

	/** BGM을 재생할 Sound Cue 또는 Sound Wave (블루프린트에서 할당) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	USoundBase* BGM_Sound;

	/** 슬로우 모션 시 BGM의 목표 볼륨 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	float BGM_SlowMotionVolumeTarget;

	/** BGM 볼륨 전환 속도 (Lerp에 사용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	float BGM_VolumeTransitionSpeed;

	float OriginalBGMVolume; // 원래 BGM 볼륨을 저장할 변수
	// =============== BGM_AudioComponent 관련 UPROPERTY 추가 끝 ===============


	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** 레이캐스트 시작 위치 오프셋 (캐릭터 기준) */
	UPROPERTY(EditDefaultsOnly, Category = Climbing)
	FVector ClimbTraceOffset;

	/** 레이캐스트 최대 거리 */
	UPROPERTY(EditDefaultsOnly, Category = Climbing)
	float ClimbTraceDistance;

	/** 올라가는 속도 */
	UPROPERTY(EditDefaultsOnly, Category = Climbing)
	float ClimbSpeed; // 이 값은 현재 Tick에서 사용되지 않음

	/** 올라가는 중인지 여부 */
	bool bIsClimbing;

	/** 올라갈 목표 위치 */
	FVector ClimbTargetLocation;

	/** "올라가기 시도" 액션 함수 */
	void TryClimb();

	// 블루프린트에서 구현할 수 있는 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Climbing")
	void OnClimbStarted();

	/** 레이캐스트를 수행하고 결과를 처리하는 함수 */
	void PerformRaycast() {} // 이제 TryClimb에서 호출

	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** "올라가기" 몽타주 참조 (블루프린트에서 할당) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Climbing)
	UAnimMontage* ClimbMontageRef;

	/** 애니메이션 진행률 (0.0 ~ 1.0) 에 따른 Z 오프셋 커브 (블루프린트에서 설정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Climbing)
	UCurveFloat* ClimbZOffsetCurve;

	// 클라이밍 시작 시점의 캐릭터 위치
	FVector StartClimbLocation;

	// 몽타주가 시작될 때의 타임 스탬프 (재생 시간을 추적하기 위함)
	float MontageStartTime;

	// 몽타주의 총 길이 (한 번만 계산)
	float MontageTotalLength;

	// 클라이밍 시작 시 몽타주가 이미 재생 중이었는지 (재시작 방지)
	bool bMontageAlreadyPlayingOnClimb;

	// 슬로우 모션 관련 함수들을 protected 섹션으로 옮깁니다.
	/** 슬로우 모션 활성화/비활성화 토글 함수 */
	void ToggleSlowMotion();

	/** 시간 딜레이와 채도를 부드럽게 업데이트하는 함수 */
	void UpdateSlowMotionDilationAndSaturation();

	// BeginPlay 오버라이드
	virtual void BeginPlay() override;

public: // public 함수들은 그대로 유지
	ATestProject2Character();

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	virtual void Tick(float DeltaTime) override;
};
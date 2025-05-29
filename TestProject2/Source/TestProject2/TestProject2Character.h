
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
class UInputAction; // Forward declaration (추가분)
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

protected:

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
	float ClimbSpeed;

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



public:
	ATestProject2Character();

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	virtual void Tick(float DeltaTime) override;

protected:
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
};
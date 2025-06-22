#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AMovingActor.generated.h" // 헤더 파일 이름 변경에 맞춤

// 이동 상태 열거형
UENUM(BlueprintType)
enum class EActorMoveState : uint8
{
	EMS_Idle UMETA(DisplayName = "Idle"),
	EMS_MovingToTarget UMETA(DisplayName = "Moving to Target"),
	EMS_MovingToStart UMETA(DisplayName = "Moving to Start")
};

UCLASS()
class TESTPROJECT2_API AMovingActor : public AActor // **** A 접두사 추가 ****
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMovingActor(); // **** 생성자 이름 A 접두사 추가 ****

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 루트 컴포넌트 역할을 할 스태틱 메시 (액터 외형)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Actor")
	UStaticMeshComponent* ActorMesh;

	// 액터의 시작 위치 (BeginPlay에서 현재 위치로 설정)
	FVector StartLocation;

	// 액터가 이동할 목표 위치 (블루프린트에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Actor")
	FVector TargetLocation;

	// 액터가 이동할 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Actor")
	float MoveSpeed;

	// 이동이 시작되었는지 여부
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Actor")
	bool bIsMoving;

	// 이동 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Actor")
	EActorMoveState CurrentMoveState;

	// 액터에 플레이어가 올라섰는지 여부
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Actor")
	bool bPlayerOnActor;

	// 액터가 밟히면 움직이기 시작할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moving Actor")
	bool bMoveOnOverlap;

	// OnComponentBeginOverlap 이벤트 핸들러
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// OnComponentEndOverlap 이벤트 핸들러
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 액터 이동을 시작하는 함수
	void StartMovingActor();

	// 액터를 정지시키는 함수
	void StopMovingActor();

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 블루프린트에서 호출할 수 있는 함수: 액터 이동 시작 (수동 트리거 시)
	UFUNCTION(BlueprintCallable, Category = "Moving Actor")
	void ActivateMovingActor();
};
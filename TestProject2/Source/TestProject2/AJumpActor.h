// AJumpActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h" // UBoxComponent를 위해 추가

// 모든 일반 include 문 뒤에 .generated.h 파일을 포함합니다.
#include "AJumpActor.generated.h" // <-- 위치를 변경했습니다.
// (참고: 언리얼 규칙에 따라 클래스 이름이 AJumpActor라도 Generated 파일 이름은 JumpActor.generated.h 입니다.)

UCLASS()
class TESTPROJECT2_API AJumpActor : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AJumpActor();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    /** 점프대 역할을 할 메쉬 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* JumpPadMesh;

    /** 캐릭터가 점프대에 닿았는지 감지하는 콜리전 박스 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    /** 캐릭터에게 가할 점프 속도 (Z축) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad")
    float JumpLaunchVelocityZ;

    /** 캐릭터에게 가할 수평 점프 속도 (XY축) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad")
    float JumpLaunchVelocityXY;

    /** 점프 후 캐릭터가 착지할 목표 위치 (월드 좌표) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad")
    FVector TargetLandingLocation;

    // 트리거 박스 오버랩 이벤트 함수
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // (선택 사항) 오버랩 종료 이벤트 함수
    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;
};
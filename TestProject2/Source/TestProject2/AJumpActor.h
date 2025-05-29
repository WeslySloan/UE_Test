// AJumpActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h" // UBoxComponent�� ���� �߰�

// ��� �Ϲ� include �� �ڿ� .generated.h ������ �����մϴ�.
#include "AJumpActor.generated.h" // <-- ��ġ�� �����߽��ϴ�.
// (����: �𸮾� ��Ģ�� ���� Ŭ���� �̸��� AJumpActor�� Generated ���� �̸��� JumpActor.generated.h �Դϴ�.)

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

    /** ������ ������ �� �޽� ������Ʈ */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* JumpPadMesh;

    /** ĳ���Ͱ� �����뿡 ��Ҵ��� �����ϴ� �ݸ��� �ڽ� */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    /** ĳ���Ϳ��� ���� ���� �ӵ� (Z��) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad")
    float JumpLaunchVelocityZ;

    /** ĳ���Ϳ��� ���� ���� ���� �ӵ� (XY��) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad")
    float JumpLaunchVelocityXY;

    /** ���� �� ĳ���Ͱ� ������ ��ǥ ��ġ (���� ��ǥ) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad")
    FVector TargetLandingLocation;

    // Ʈ���� �ڽ� ������ �̺�Ʈ �Լ�
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // (���� ����) ������ ���� �̺�Ʈ �Լ�
    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;
};
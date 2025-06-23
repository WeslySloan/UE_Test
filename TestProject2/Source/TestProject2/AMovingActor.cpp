// Fill out your copyright notice in the Description page of Project Settings.

#include "AMovingActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h" 
// 정확한 헤더 파일 경로와 확장자 포함
#include "GameFramework/CharacterMovementComponent.h" // 캐릭터 무브먼트 컴포넌트 접근을 위해 추가

// Sets default values
AMovingActor::AMovingActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // 액터 메시 생성 및 루트 컴포넌트로 설정
    ActorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ActorMesh"));
    RootComponent = ActorMesh;

    // **** ActorMesh 초기 충돌 설정: 물리적 충돌 (Block) 유지, Overlap 이벤트 비활성화 ****
    ActorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ActorMesh->SetCollisionResponseToAllChannels(ECR_Block); // 기본적으로 모든 채널 블록
    ActorMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); // Pawn 채널에 대해 Block!
    ActorMesh->SetGenerateOverlapEvents(false); // 메시에서는 오버랩 이벤트를 생성하지 않음

    // **** TriggerBox 생성 및 설정 ****
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(RootComponent); // ActorMesh(RootComponent)를 따라다니도록 부착

    // C++에서 TriggerBox의 초기 크기와 위치를 설정.
    // 블루프린트에서 ActorMesh의 크기와 위치에 맞게 시각적으로 조절하는 것이 중요합니다.
    TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
    // TriggerBox의 초기 상대 위치를 0,0,0으로 설정하여 블루프린트에서 정확히 맞출 수 있도록 함
    TriggerBox->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

    // **** TriggerBox 충돌 설정: 오버랩 감지 전용 ****
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 쿼리만 활성화 (물리X)
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore); // 모든 채널 무시
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Pawn과 Overlap!
    TriggerBox->SetGenerateOverlapEvents(true); // 오버랩 이벤트 생성 활성화

    // **** 오버랩 이벤트 바인딩을 TriggerBox로 변경 ****
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMovingActor::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AMovingActor::OnOverlapEnd);

    // 기본값 설정
    TargetLocation = FVector(0.0f, 0.0f, 500.0f);
    MoveSpeed = 100.0f;
    bIsMoving = false;
    bPlayerOnActor = false;
    bMoveOnOverlap = true;
    CurrentMoveState = EActorMoveState::EMS_Idle;
}

// Called when the game starts or when spawned
void AMovingActor::BeginPlay()
{
    Super::BeginPlay();
    StartLocation = GetActorLocation(); // 현재 위치를 시작 위치로 저장
}

// 트리거 박스 오버랩 시작 이벤트
void AMovingActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);

    // OtherActor가 유효한 플레이어 캐릭터인지, 그리고 현재 bPlayerOnActor가 false인지 확인
    if (bMoveOnOverlap && OtherActor == PlayerCharacter && !bPlayerOnActor)
    {
        if (PlayerCharacter && PlayerCharacter->GetCapsuleComponent() && ActorMesh && ActorMesh->GetStaticMesh())
        {
            // 플레이어 바닥과 플랫폼 상단 Z 검사
            float PlayerBottomZ = PlayerCharacter->GetActorLocation().Z - PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            FBoxSphereBounds MeshBounds = ActorMesh->GetStaticMesh()->GetBounds();
            // ActorMesh의 월드 스케일을 고려하여 정확한 상단 Z 값 계산
            float ActorMeshTopZ = ActorMesh->GetComponentLocation().Z + (MeshBounds.BoxExtent.Z * ActorMesh->GetComponentScale().Z);

            UE_LOG(LogTemp, Warning, TEXT("OverlapBegin: Player Z: %f, Mesh Top Z: %f"), PlayerBottomZ, ActorMeshTopZ);

            // 플레이어의 바닥이 플랫폼 메시의 상단 근처에 있을 때만 작동
            if (PlayerBottomZ >= ActorMeshTopZ - 20.0f && PlayerBottomZ <= ActorMeshTopZ + 20.0f)
            {
                // 이미 부착되어 있지 않은 경우에만 부착 로직 실행
                if (PlayerCharacter->GetAttachParentActor() != this)
                {
                    PlayerCharacter->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                    UE_LOG(LogTemp, Warning, TEXT("Character Attached to Moving Actor from OverlapBegin."));

                    // 부착 시 충돌 응답 변경: Pawn-ActorMesh 간 Ignore
                    ActorMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
                    if (UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent())
                    {
                        PlayerCapsule->SetCollisionResponseToChannel(ActorMesh->GetCollisionObjectType(), ECR_Ignore);
                    }
                    UE_LOG(LogTemp, Warning, TEXT("Ignoring collision between Player and ActorMesh."));

                    // **** 중요: 캐릭터 무브먼트 컴포넌트의 안정성 강화 (오류 발생 라인 제거) ****
                    if (UCharacterMovementComponent* MovementComp = PlayerCharacter->GetCharacterMovement())
                    {
                        MovementComp->SetMovementMode(MOVE_Walking); // 강제로 걷기 모드 설정
                        UE_LOG(LogTemp, Warning, TEXT("CharacterMovementComponent adjusted for platform attachment."));
                    }
                }

                bPlayerOnActor = true; // 플레이어가 플랫폼 위에 있음을 표시
                StartMovingActor(); // 액터 이동 시작
                UE_LOG(LogTemp, Warning, TEXT("Player detected on top of Moving Actor's trigger. Activating."));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("OverlapBegin: Z-axis check failed. Player not on top of mesh. PlayerBottomZ: %f, ActorMeshTopZ: %f"), PlayerBottomZ, ActorMeshTopZ);
            }
        }
    }
}

// 오버랩 종료 이벤트
void AMovingActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);

    if (OtherActor == PlayerCharacter && bPlayerOnActor)
    {
        bPlayerOnActor = false;
        UE_LOG(LogTemp, Warning, TEXT("Player left Moving Actor's trigger. bPlayerOnActor set to false."));

        if (PlayerCharacter->GetAttachParentActor() == this)
        {
            PlayerCharacter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            UE_LOG(LogTemp, Warning, TEXT("Character Detached from Moving Actor from OverlapEnd."));

            // 해제 시 충돌 응답 복원: Pawn-ActorMesh 간 Block
            ActorMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
            if (UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent())
            {
                PlayerCapsule->SetCollisionResponseToChannel(ActorMesh->GetCollisionObjectType(), ECR_Block);
            }
            UE_LOG(LogTemp, Warning, TEXT("Restoring collision between Player and ActorMesh."));

            // 캐릭터 무브먼트 컴포넌트의 상태 복원 (이전 오류 발생 라인 제거)
            if (UCharacterMovementComponent* MovementComp = PlayerCharacter->GetCharacterMovement())
            {
                // 일반적으로 Detach 후 자동으로 Walking 모드로 돌아감
                // 필요한 경우 여기서 추가적인 MovementComp 설정 초기화
                UE_LOG(LogTemp, Warning, TEXT("CharacterMovementComponent restored after detachment."));
            }
        }

        // 플레이어가 벗어나면 정지 (이동 중이었다면)
        if (bIsMoving && CurrentMoveState == EActorMoveState::EMS_MovingToTarget)
        {
            StopMovingActor();
            UE_LOG(LogTemp, Warning, TEXT("Moving Actor stopped because player left."));
        }
    }
}

// 액터 이동 시작 함수
void AMovingActor::StartMovingActor()
{
    if (!bIsMoving)
    {
        bIsMoving = true;
        CurrentMoveState = EActorMoveState::EMS_MovingToTarget;
        UE_LOG(LogTemp, Warning, TEXT("Moving Actor Activated!"));
    }
}

// 액터 이동 정지 함수
void AMovingActor::StopMovingActor()
{
    bIsMoving = false;
    CurrentMoveState = EActorMoveState::EMS_Idle;
    UE_LOG(LogTemp, Warning, TEXT("Moving Actor Stopped."));
}

// 블루프린트에서 호출할 수 있는 이동 활성화 함수
void AMovingActor::ActivateMovingActor()
{
    StartMovingActor();
}

// Called every frame
void AMovingActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsMoving)
    {
        FVector CurrentLocation = GetActorLocation();
        FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
        float DistanceToTarget = FVector::Dist(CurrentLocation, TargetLocation);

        if (DistanceToTarget > KINDA_SMALL_NUMBER)
        {
            FVector NewLocation = CurrentLocation + Direction * MoveSpeed * DeltaTime;
            SetActorLocation(NewLocation, true);
        }
        else
        {
            SetActorLocation(TargetLocation, false);

            if (!bPlayerOnActor)
            {
                StopMovingActor();
            }
        }
    }
}
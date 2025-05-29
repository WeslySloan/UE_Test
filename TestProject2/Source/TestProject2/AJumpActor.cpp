#include "AJumpActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h" // ACharacter를 사용하기 위해 추가
#include "GameFramework/CharacterMovementComponent.h" // LaunchCharacter 사용 시 유용

// Sets default values
AJumpActor::AJumpActor()
{
    // Set this actor to call Tick() every frame. You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    // 스태틱 메쉬 컴포넌트 생성 및 루트 컴포넌트로 설정
    JumpPadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("JumpPadMesh")); // BedMesh 대신 JumpPadMesh로 변경
    RootComponent = JumpPadMesh;

    // 트리거 박스 생성 및 메쉬에 부착
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(JumpPadMesh); // BedMesh 대신 JumpPadMesh로 변경
    // 콜리전 설정
    TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f)); // 적절한 크기로 조절
    TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic")); // 모든 동적 오브젝트와 오버랩되도록 설정
    // 또는 특정 채널에 맞게 설정:
    // TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    // TriggerBox->SetCollisionObjectType(ECC_WorldStatic); // 또는 다른 Object Type
    // TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    // TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Pawn과만 오버랩

    // 오버랩 이벤트 바인딩
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AJumpActor::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AJumpActor::OnOverlapEnd);

    // 점프 관련 기본 값 설정 (블루프린트에서 튜닝)
    JumpLaunchVelocityZ = 1500.0f; // 기본 Z축 점프 속도
    JumpLaunchVelocityXY = 0.0f;  // 기본 수평 점프 속도 (처음엔 0으로 시작)
    TargetLandingLocation = FVector::ZeroVector; // 기본 착지 목표 위치
}

// Called when the game starts or when spawned
void AJumpActor::BeginPlay()
{
    Super::BeginPlay();

}

// 트리거 박스 오버랩 시작 이벤트
void AJumpActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 오버랩된 액터가 캐릭터인지 확인
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character)
    {
        // 목표 위치로 날아가도록 LaunchVelocity 계산 (추천 방식)
        FVector CurrentCharacterXY = Character->GetActorLocation();
        CurrentCharacterXY.Z = 0; // Z는 무시
        FVector TargetLandingXY = TargetLandingLocation;
        TargetLandingXY.Z = 0; // Z는 무시

        FVector HorizontalDirection = TargetLandingXY - CurrentCharacterXY;
        HorizontalDirection.Normalize(); // 방향만 얻음

        FVector LaunchVelocity = HorizontalDirection * JumpLaunchVelocityXY;
        LaunchVelocity.Z = JumpLaunchVelocityZ; // Z 속도 적용

        // LaunchCharacter 호출
        Character->LaunchCharacter(LaunchVelocity, true, true); // XY와 Z 모두 현재 속도 무시

        UE_LOG(LogTemp, Warning, TEXT("Character launched towards Target: %s with Velocity: %s"), *TargetLandingLocation.ToString(), *LaunchVelocity.ToString());

        // (선택 사항) 점프 시 사운드나 파티클 재생
        // UGameplayStatics::PlaySoundAtLocation(this, JumpSound, GetActorLocation());
        // UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), JumpParticle, GetActorLocation());
    }
}

// 오버랩 종료 이벤트 (필요 시 구현)
void AJumpActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // UE_LOG(LogTemp, Warning, TEXT("Overlap ended with: %s"), *OtherActor->GetName());
}

// Called every frame
void AJumpActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}
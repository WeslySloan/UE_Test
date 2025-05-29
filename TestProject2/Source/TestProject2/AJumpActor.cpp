#include "AJumpActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h" // ACharacter�� ����ϱ� ���� �߰�
#include "GameFramework/CharacterMovementComponent.h" // LaunchCharacter ��� �� ����

// Sets default values
AJumpActor::AJumpActor()
{
    // Set this actor to call Tick() every frame. You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    // ����ƽ �޽� ������Ʈ ���� �� ��Ʈ ������Ʈ�� ����
    JumpPadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("JumpPadMesh")); // BedMesh ��� JumpPadMesh�� ����
    RootComponent = JumpPadMesh;

    // Ʈ���� �ڽ� ���� �� �޽��� ����
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(JumpPadMesh); // BedMesh ��� JumpPadMesh�� ����
    // �ݸ��� ����
    TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f)); // ������ ũ��� ����
    TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic")); // ��� ���� ������Ʈ�� �������ǵ��� ����
    // �Ǵ� Ư�� ä�ο� �°� ����:
    // TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    // TriggerBox->SetCollisionObjectType(ECC_WorldStatic); // �Ǵ� �ٸ� Object Type
    // TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    // TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Pawn���� ������

    // ������ �̺�Ʈ ���ε�
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AJumpActor::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AJumpActor::OnOverlapEnd);

    // ���� ���� �⺻ �� ���� (�������Ʈ���� Ʃ��)
    JumpLaunchVelocityZ = 1500.0f; // �⺻ Z�� ���� �ӵ�
    JumpLaunchVelocityXY = 0.0f;  // �⺻ ���� ���� �ӵ� (ó���� 0���� ����)
    TargetLandingLocation = FVector::ZeroVector; // �⺻ ���� ��ǥ ��ġ
}

// Called when the game starts or when spawned
void AJumpActor::BeginPlay()
{
    Super::BeginPlay();

}

// Ʈ���� �ڽ� ������ ���� �̺�Ʈ
void AJumpActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // �������� ���Ͱ� ĳ�������� Ȯ��
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character)
    {
        // ��ǥ ��ġ�� ���ư����� LaunchVelocity ��� (��õ ���)
        FVector CurrentCharacterXY = Character->GetActorLocation();
        CurrentCharacterXY.Z = 0; // Z�� ����
        FVector TargetLandingXY = TargetLandingLocation;
        TargetLandingXY.Z = 0; // Z�� ����

        FVector HorizontalDirection = TargetLandingXY - CurrentCharacterXY;
        HorizontalDirection.Normalize(); // ���⸸ ����

        FVector LaunchVelocity = HorizontalDirection * JumpLaunchVelocityXY;
        LaunchVelocity.Z = JumpLaunchVelocityZ; // Z �ӵ� ����

        // LaunchCharacter ȣ��
        Character->LaunchCharacter(LaunchVelocity, true, true); // XY�� Z ��� ���� �ӵ� ����

        UE_LOG(LogTemp, Warning, TEXT("Character launched towards Target: %s with Velocity: %s"), *TargetLandingLocation.ToString(), *LaunchVelocity.ToString());

        // (���� ����) ���� �� ���峪 ��ƼŬ ���
        // UGameplayStatics::PlaySoundAtLocation(this, JumpSound, GetActorLocation());
        // UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), JumpParticle, GetActorLocation());
    }
}

// ������ ���� �̺�Ʈ (�ʿ� �� ����)
void AJumpActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // UE_LOG(LogTemp, Warning, TEXT("Overlap ended with: %s"), *OtherActor->GetName());
}

// Called every frame
void AJumpActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}
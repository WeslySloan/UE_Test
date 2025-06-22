#include "AMovingActor.h" // 헤더 파일 이름 변경에 맞춤
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h" 
#include "GameFramework/Character.h" 
#include "Kismet/GameplayStatics.h" 
#include "Components/CapsuleComponent.h"

// Sets default values
AMovingActor::AMovingActor() // **** 생성자 이름 A 접두사 추가 ****
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 액터 메시 생성 및 루트 컴포넌트로 설정
	ActorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ActorMesh"));
	RootComponent = ActorMesh;

	// 메시의 충돌 설정 (필수): Pawn 또는 Character가 Overlap할 수 있도록 설정
	ActorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ActorMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ActorMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ActorMesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

	// StaticMeshComponent에 오버랩 이벤트 바인딩
	ActorMesh->OnComponentBeginOverlap.AddDynamic(this, &AMovingActor::OnOverlapBegin); // **** A 접두사 추가 ****
	ActorMesh->OnComponentEndOverlap.AddDynamic(this, &AMovingActor::OnOverlapEnd); // **** A 접두사 추가 ****

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

	StartLocation = GetActorLocation();
}

void AMovingActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);

	if (bMoveOnOverlap && OtherActor == PlayerCharacter)
	{
		if (PlayerCharacter && PlayerCharacter->GetCapsuleComponent() && ActorMesh && ActorMesh->GetStaticMesh())
		{
			float PlayerBottomZ = PlayerCharacter->GetActorLocation().Z - PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			FBoxSphereBounds Bounds = ActorMesh->GetStaticMesh()->GetBounds();
			float ActorTopZ = ActorMesh->GetComponentLocation().Z + Bounds.BoxExtent.Z;

			if (PlayerBottomZ > ActorTopZ - 10.0f)
			{
				bPlayerOnActor = true;
				StartMovingActor();
			}
		}
	}
}

void AMovingActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);

	if (OtherActor == PlayerCharacter)
	{
		bPlayerOnActor = false;

		if (PlayerCharacter->GetAttachParentActor() == this)
		{
			PlayerCharacter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}

void AMovingActor::StartMovingActor()
{
	if (!bIsMoving)
	{
		bIsMoving = true;
		CurrentMoveState = EActorMoveState::EMS_MovingToTarget;
		UE_LOG(LogTemp, Warning, TEXT("Moving Actor Activated!"));
	}
}

void AMovingActor::StopMovingActor()
{
	bIsMoving = false;
	CurrentMoveState = EActorMoveState::EMS_Idle;
	UE_LOG(LogTemp, Warning, TEXT("Moving Actor Stopped."));
}

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

		if (DistanceToTarget > MoveSpeed * DeltaTime)
		{
			SetActorLocation(CurrentLocation + Direction * MoveSpeed * DeltaTime, true);
		}
		else
		{
			SetActorLocation(TargetLocation, false);
			StopMovingActor();
		}

		if (bPlayerOnActor)
		{
			ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);
			if (PlayerCharacter)
			{
				if (PlayerCharacter->GetAttachParentActor() != this)
				{
					PlayerCharacter->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				}
			}
		}
	}
}
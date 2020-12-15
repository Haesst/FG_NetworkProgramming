#include "FGPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Player/FGPlayer.h"
#include "Net/UnrealNetwork.h"

AFGPickup::AFGPickup()
{
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SphereComponent->SetupAttachment(RootComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	SetReplicates(true);
}

void AFGPickup::ReActivatePickup()
{
	bPickedUp = false;
	RootComponent->SetVisibility(true, true);
	SphereComponent->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetActorTickEnabled(true);

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReActivateHandle);
	}
}

void AFGPickup::OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPickedUp)
	{
		return;
	}

	if (AFGPlayer* Player = Cast<AFGPlayer>(OtherActor))
	{
		Player->OnPickup(this);
	}
}

void AFGPickup::BeginPlay()
{
	Super::BeginPlay();

	SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AFGPickup::OverlapBegin);
	CachedMeshRelativeLocation = MeshComponent->GetRelativeLocation();
}

void AFGPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReActivateHandle);
	}
}

void AFGPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float PulsatingValue = FMath::MakePulsatingValue(GetWorld()->GetTimeSeconds(), 0.65f) * 30.0f;
	const FVector NewLocation = CachedMeshRelativeLocation + FVector(0.0f, 0.0f, PulsatingValue);
	FHitResult Hit;
	MeshComponent->SetRelativeLocation(NewLocation, false, &Hit, ETeleportType::TeleportPhysics);
	MeshComponent->SetRelativeRotation(FRotator(0.0f, 20.0f * DeltaTime, 0.0f), false, &Hit, ETeleportType::TeleportPhysics);
}

bool AFGPickup::IsPickedUp()
{
	return bPickedUp;
}

void AFGPickup::HandlePickup()
{
	bPickedUp = true;
	SphereComponent->SetCollisionProfileName(TEXT("NoCollision"));
	HidePickup();
	GetWorldTimerManager().SetTimer(ReActivateHandle, this, &AFGPickup::ReActivatePickup, ReActivateTime, false);
	SetActorTickEnabled(false);
}

void AFGPickup::HidePickup()
{
	RootComponent->SetVisibility(false, true);
}

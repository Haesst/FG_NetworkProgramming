#pragma once

#include "GameFramework/Actor.h"
#include "FGPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EFGPickupType : uint8
{
	Rocket,
	Health
};

UCLASS()
class FG_NET_API AFGPickup : public AActor
{
	GENERATED_BODY()

private:
	FVector CachedMeshRelativeLocation = FVector::ZeroVector;
	FTimerHandle ReActivateHandle;
	bool bPickedUp = false;

private:
	UFUNCTION()
	void ReActivatePickup();
	UFUNCTION()
	void OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
public:
	AFGPickup();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

	bool IsPickedUp();
	void HandlePickup();
	void HidePickup();

	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	USphereComponent* SphereComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere)
	EFGPickupType PickupType = EFGPickupType::Rocket;

	UPROPERTY(EditAnywhere)
	int32 NumRockets = 5;

	UPROPERTY(EditAnywhere)
	float ReActivateTime = 5.0f;
};
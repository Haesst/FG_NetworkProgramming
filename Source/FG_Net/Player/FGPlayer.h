#pragma once

#include "GameFramework/Pawn.h"
#include "FGPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFGMovementComponent;
class UStaticMeshComponent;
class USphereComponent;
class UFGPlayerSettings;
class UFGNetDebugWidget;
class AFGRocket;
class AFGPickup;

UCLASS()
class FG_NET_API AFGPlayer : public APawn
{
	GENERATED_BODY()
private:
	float Forward = 0.0f;
	float Turn = 0.0f;

	float MovementVelocity = 0.0f;
	float Yaw = 0.0f;

	bool bBrake = false;
	bool bShowDebugMenu = false;

	float ClientTimeStamp = 0.0f;
	float LastCorrectionDelta = 0.0f;
	float ServerTimeStamp = 0.0f;

	UPROPERTY(EditAnywhere, Category = Network)
	bool bPerformNetworkSmoothing = true;

	FVector OriginalMeshOffset = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float ReplicatedYaw = 0.0f;

	UPROPERTY(Replicated)
	FVector ReplicatedLocation;

	UPROPERTY(Replicated, Transient)
	TArray<AFGRocket*> RocketInstances;

	FVector DesiredLocation = FVector::ZeroVector;

	int32 ServerNumRockets = 0;
	int32 NumRockets = 0;
	FVector GetRocketStartLocation() const;
	AFGRocket* GetFreeRocket() const;

	void AddMovementVelocity(float DeltaTime);

	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	USphereComponent* CollisionComponent;
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	UStaticMeshComponent* MeshComponent;
	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	USpringArmComponent* SpringArmComponent;
	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	UCameraComponent* CameraComponent;
	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	UFGMovementComponent* MovementComponent;
	UPROPERTY(Transient)
	UFGNetDebugWidget* DebugMenuInstance = nullptr;

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	UFGPlayerSettings* PlayerSettings = nullptr;

	UPROPERTY(EditAnyWhere, Category = Debug)
	TSubclassOf<UFGNetDebugWidget> DebugMenuClass;

	UPROPERTY(EditAnywhere, Category = Weapon)
	TSubclassOf<AFGRocket> RocketClass;
	UPROPERTY(EditAnywhere, Category = Weapon)
	bool bUnlimitedRockets = false;

	int32 MaxActiveRockets = 3;
	float FireCooldownElapsed = 0.0f;

public:
	AFGPlayer();

private:
	void Handle_Accelerate(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();
	void Handle_FirePressed();

	void Handle_DebugMenuPressed();

	void CreateDebugWidget();
	void ShowDebugMenu();
	void HideDebugMenu();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void OnPickup(AFGPickup* Pickup);

	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }
	UFUNCTION(BlueprintPure)
	int32 GetPing() const;
	UFUNCTION(BlueprintPure)
	int32 GetNumRockets() const { return NumRockets; }
	UFUNCTION(BlueprintImplementableEvent, Category = Player, meta = (DisplayName = "On Num Rockets Changed"))
	void BP_OnNumRocketsChanged(int32 NewNumRockets);

	int32 GetNumActiveRockets() const;
	void FireRocket();
	void SpawnRockets();

	UFUNCTION(Server, Unreliable)
	void Server_SendLocation(const FVector& LocationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendLocation(const FVector& LocationToSend);

	UFUNCTION(Server, Unreliable)
	void Server_SendYaw(float NewYaw);

	UFUNCTION(Server, Unreliable)
	void Server_SendFaceDirection(const FQuat& LocationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendFaceDirection(const FQuat& LocationToSend);

	UFUNCTION(Server, Reliable)
	void Server_OnPickup(AFGPickup* Pickup);
	UFUNCTION(Server, Reliable)
	void Client_OnPickupRockets(AFGPickup* Pickup, int32 PickedUpRockets);

	UFUNCTION(Server, Reliable)
	void Server_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);

	UFUNCTION(Client, Reliable)
	void Client_RemoveRocket(AFGRocket* RocketToRemove);

	UFUNCTION(BlueprintCallable)
	void Cheat_IncreaseRockets(int32 InNumRockets);

	UFUNCTION(Server, Unreliable)
	void Server_SendMovement(const FVector& ClientLocation, float TimeStamp, float ClientForward, float ClientYaw);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendMovement(const FVector& InClientLocation, float TimeStamp, float ClientForward, float ClientYaw);
};
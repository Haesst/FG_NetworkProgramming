#pragma once

#include "GameFramework/Pawn.h"
#include "FGPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFGMovementComponent;
class UStaticMeshComponent;
class USphereComponent;

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

	FVector DesiredLocation = FVector::ZeroVector;

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

public:
	UPROPERTY(EditAnywhere, Category = Movement)
	float Acceleration = 500.0f;
	UPROPERTY(EditAnywhere, Category = Movement, meta = (DisplayName = "TurnSpeed"))
	float TurnSpeedDefault = 100.0f;
	UPROPERTY(EditAnywhere, Category = Movement)
	float MaxVelocity = 2000.0f;
	UPROPERTY(EditAnywhere, Category = Movement, meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DefaultFriction = 0.75f;
	UPROPERTY(EditAnywhere, Category = Movement, meta = (ClampMin = 0.0, ClampMax = 1.0))
	float BrakingFriction = 500.0f;
	UPROPERTY(EditAnywhere, Category = Movement, meta = (DisplayName = "Network Interpolation Speed"))
	float NetworkInterpolationSpeed = 5.0f;

public:
	AFGPlayer();

private:
	void Handle_Accelerate(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }
	UFUNCTION(BlueprintPure)
	int32 GetPing() const;

	UFUNCTION(Server, Unreliable)
	void Server_SendLocation(const FVector& LocationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendLocation(const FVector& LocationToSend);

	UFUNCTION(Server, Unreliable)
	void Server_SendFaceDirection(const FQuat& LocationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendFaceDirection(const FQuat& LocationToSend);
};

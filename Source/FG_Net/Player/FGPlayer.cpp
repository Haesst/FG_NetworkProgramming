#include "FGPlayer.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerState.h"
#include "Camera/CameraComponent.h"
#include "Engine/NetDriver.h"
#include "../Components/FGMovementComponent.h"
#include "../FGMovementStatics.h"

/// <summary>
/// Current Movie timestamp: 1:23:18
/// </summary>

AFGPlayer::AFGPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->bInheritYaw = false;
	SpringArmComponent->SetupAttachment(CollisionComponent);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);

	MovementComponent = CreateDefaultSubobject<UFGMovementComponent>(TEXT("MovementComponent"));

	SetReplicateMovement(false);
}

void AFGPlayer::Handle_Accelerate(float Value)
{
	Forward = Value;
}

void AFGPlayer::Handle_Turn(float Value)
{
	Turn = Value;
}

void AFGPlayer::Handle_BrakePressed()
{
	bBrake = true;
}

void AFGPlayer::Handle_BrakeReleased()
{
	bBrake = false;
}

void AFGPlayer::BeginPlay()
{
	Super::BeginPlay();
	MovementComponent->SetUpdatedComponent(CollisionComponent);
}

void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Friction = IsBraking() ? BrakingFriction : DefaultFriction;
	const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (MaxVelocity * 0.75f)), 0.0f, 1.0f);
	const float TurnSpeed = FMath::InterpEaseOut(0.0f, TurnSpeedDefault, Alpha, 5.0f);
	const float MovementDirection = MovementVelocity > 0.0f ? Turn : -Turn;

	Yaw += (MovementDirection * TurnSpeed) * DeltaTime;
	FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
	
	if (IsLocallyControlled())
	{
		MovementComponent->SetFacingRotation(WantedFacingDirection);
	}
	else
	{
		SetActorLocation(FMath::Lerp(GetActorLocation(), DesiredLocation, DeltaTime * NetworkInterpolationSpeed));
	}

	FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();

	MovementVelocity += Forward * Acceleration * DeltaTime;
	MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
	MovementVelocity *= FMath::Pow(Friction, DeltaTime);

	MovementComponent->ApplyGravity();
	FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
	MovementComponent->Move(FrameMovement);

	if (IsLocallyControlled())
	{
		Server_SendLocation(GetActorLocation());
		Server_SendFaceDirection(GetActorRotation().Quaternion());
	}
}

void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Accelerate);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);

	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);

	//PlayerInputComponent->BindAction(TEXT("DebugMenu"), IE_Pressed, this, &AFGPlayer::Handle_DebugMenuPressed);
}

int32 AFGPlayer::GetPing() const
{
	if (GetPlayerState())
	{
		return static_cast<int32>(GetPlayerState()->GetPing());
	}

	return 0;
}

void AFGPlayer::Server_SendLocation_Implementation(const FVector& LocationToSend)
{
	Multicast_SendLocation(LocationToSend);
}

void AFGPlayer::Multicast_SendLocation_Implementation(const FVector& LocationToSend)
{
	if (!IsLocallyControlled())
	{
		DesiredLocation = LocationToSend;
	}
}

void AFGPlayer::Server_SendFaceDirection_Implementation(const FQuat& FaceDirectionToSend)
{
	Multicast_SendFaceDirection(FaceDirectionToSend);
}

void AFGPlayer::Multicast_SendFaceDirection_Implementation(const FQuat& FaceDirectionToSend)
{
	if (!IsLocallyControlled())
	{
		MovementComponent->SetFacingRotation(FaceDirectionToSend);
	}
}
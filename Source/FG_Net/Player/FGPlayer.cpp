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
#include "Net/UnrealNetwork.h"
#include "FGPlayerSettings.h"
#include "../Debug/UI/FGNetDebugWidget.h"
#include "../FGPickup.h"
#include "../FGRocket.h"

const static float MaxMoveDeltaTime = 0.125f;

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

void AFGPlayer::Handle_DebugMenuPressed()
{
	bShowDebugMenu = !bShowDebugMenu;

	if (bShowDebugMenu)
	{
		ShowDebugMenu();
	}
	else
	{
		HideDebugMenu();
	}
}

void AFGPlayer::Handle_FirePressed()
{
	FireRocket();
}

void AFGPlayer::FireRocket()
{
	if (FireCooldownElapsed > 0.0f)
	{
		return;
	}

	if (NumRockets <= 0 && !bUnlimitedRockets)
	{
		return;
	}

	if (GetNumActiveRockets() >= MaxActiveRockets)
	{
		return;
	}

	AFGRocket* NewRocket = GetFreeRocket();

	if (!ensure(NewRocket != nullptr))
	{
		return;
	}

	FireCooldownElapsed = PlayerSettings->FireCooldown;

	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (HasAuthority())
		{
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
		else
		{
			NumRockets--;
			NewRocket->StartMoving(GetActorForwardVector(), GetRocketStartLocation());
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
			BP_OnNumRocketsChanged(NumRockets);
		}
	}
}

void AFGPlayer::Server_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation)
{
	if ((ServerNumRockets - 1) < 0 && !bUnlimitedRockets)
	{
		Client_RemoveRocket(NewRocket);
	}
	else
	{
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(RocketFacingRotation.Yaw, GetActorForwardVector().Rotation().Yaw);
		const FRotator NewFacingRotation = RocketFacingRotation + FRotator(0.0f, DeltaYaw, 0.0f);
		ServerNumRockets--;
		Multicast_FireRocket(NewRocket, RocketStartLocation, NewFacingRotation);
	}
}

void AFGPlayer::Multicast_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation)
{
	if (!ensure(NewRocket != nullptr))
	{
		return;
	}

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		NewRocket->ApplyCorrection(RocketFacingRotation.Vector());
	}
	else
	{
		NumRockets--;
		NewRocket->StartMoving(RocketFacingRotation.Vector(), RocketStartLocation);
	}

	if (!IsLocallyControlled())
	{
		BP_OnNumRocketsChanged(NumRockets);
	}
}

void AFGPlayer::Client_RemoveRocket_Implementation(AFGRocket* RocketToRemove)
{
	RocketToRemove->MakeFree();
}

FVector AFGPlayer::GetRocketStartLocation() const
{
	const FVector StartLoc = GetActorLocation() + GetActorForwardVector() * 100.0f;
	return StartLoc;
}

AFGRocket* AFGPlayer::GetFreeRocket() const
{
	for (AFGRocket* Rocket : RocketInstances)
	{
		if (Rocket == nullptr)
		{
			continue;
		}

		if (Rocket->IsFree())
		{
			return Rocket;
		}
	}

	return nullptr;
}

void AFGPlayer::AddMovementVelocity(float DeltaTime)
{
	if (!ensure(PlayerSettings != nullptr))
	{
		return;
	}

	const float MaxVelocity = PlayerSettings->MaxVelocity;
	const float Acceleration = PlayerSettings->Acceleration;

	MovementVelocity += Forward * Acceleration * DeltaTime;
	MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
}

void AFGPlayer::SpawnRockets()
{
	if (HasAuthority() && RocketClass != nullptr)
	{
		const int32 RocketCache = 8;

		for (int32 Index = 0; Index < RocketCache; ++Index)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.ObjectFlags = RF_Transient;
			SpawnParams.Instigator = this;
			SpawnParams.Owner = this;
			AFGRocket* NewRocketInstance = GetWorld()->SpawnActor<AFGRocket>(RocketClass, GetActorLocation(), GetActorRotation(), SpawnParams);
			RocketInstances.Add(NewRocketInstance);
		}
	}
}

void AFGPlayer::Cheat_IncreaseRockets(int32 InNumRockets)
{
	if (IsLocallyControlled())
	{
		NumRockets += InNumRockets;
	}
}

void AFGPlayer::Server_SendMovement_Implementation(const FVector& ClientLocation, float TimeStamp, float ClientForward, float ClientYaw)
{
	/*const float DeltaTime = FMath::Min(TimeStamp - ServerTimeStamp, MaxMoveDeltaTime);
	ServerTimeStamp = TimeStamp;*/

	Multicast_SendMovement(ClientLocation, TimeStamp, ClientForward, ClientYaw);
}

void AFGPlayer::Multicast_SendMovement_Implementation(const FVector& InClientLocation, float TimeStamp, float ClientForward, float ClientYaw)
{
	if (!IsLocallyControlled())
	{
		Forward = ClientForward;
		const float DeltaTime = FMath::Min(TimeStamp - ClientTimeStamp, MaxMoveDeltaTime);
		ClientTimeStamp = TimeStamp;
		
		AddMovementVelocity(DeltaTime);
		MovementComponent->SetFacingRotation(FRotator(0.0f, ClientYaw, 0.0f));

		const FVector DeltaDiff = InClientLocation - GetActorLocation();

		if (DeltaDiff.SizeSquared() > FMath::Square(40.0f))
		{
			if (bPerformNetworkSmoothing)
			{
				const FScopedPreventAttachedComponentMove PreventMeshMove(MeshComponent);
				MovementComponent->UpdatedComponent->SetWorldLocation(InClientLocation, false, nullptr, ETeleportType::TeleportPhysics);
				LastCorrectionDelta = DeltaTime;
			}
			else
			{
				SetActorLocation(InClientLocation);
			}
		}
	}
}

void AFGPlayer::BeginPlay()
{
	Super::BeginPlay();
	MovementComponent->SetUpdatedComponent(CollisionComponent);

	CreateDebugWidget();
	if (DebugMenuInstance != nullptr)
	{
		DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	SpawnRockets();

	BP_OnNumRocketsChanged(NumRockets);
	OriginalMeshOffset = MeshComponent->GetRelativeLocation();
}

void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FireCooldownElapsed -= DeltaTime;

	if (!ensure(PlayerSettings != nullptr))
	{
		return;
	}

	FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();

	if (IsLocallyControlled())
	{
		ClientTimeStamp += DeltaTime;

		const float MaxVelocity = PlayerSettings->MaxVelocity;
		const float Friction = IsBraking() ? PlayerSettings->BreakingFriction : PlayerSettings->DefaultFriction;
		const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (MaxVelocity * 0.75f)), 0.0f, 1.0f);
		const float TurnSpeed = FMath::InterpEaseOut(0.0f, PlayerSettings->TurnSpeedDefault, Alpha, 5.0f);
		const float MovementDirection = MovementVelocity > 0.0f ? Turn : -Turn;

		Yaw += (MovementDirection * TurnSpeed) * DeltaTime;
		FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		MovementComponent->SetFacingRotation(WantedFacingDirection);

		AddMovementVelocity(DeltaTime);
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);

		MovementComponent->ApplyGravity();
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		Server_SendMovement(GetActorLocation(), ClientTimeStamp, Forward, GetActorRotation().Yaw);
	}
	else
	{
		const float Friction = IsBraking() ? PlayerSettings->BreakingFriction : PlayerSettings->DefaultFriction;
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		if (bPerformNetworkSmoothing)
		{
			const FVector NewRelativeLocation = FMath::VInterpTo(MeshComponent->GetRelativeLocation(), OriginalMeshOffset, LastCorrectionDelta, 1.75f);
			MeshComponent->SetRelativeLocation(NewRelativeLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}

		/*const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), ReplicatedLocation, DeltaTime, PlayerSettings->NetworkInterpolationSpeed);
		SetActorLocation(NewLocation);
		MovementComponent->SetFacingRotation(FRotator(0.0f, ReplicatedYaw, 0.0f), 10.0f);
		SetActorRotation(MovementComponent->GetFacingRotation());*/
	}
}

void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Accelerate);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);

	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);

	PlayerInputComponent->BindAction(TEXT("DebugMenu"), IE_Pressed, this, &AFGPlayer::Handle_DebugMenuPressed);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AFGPlayer::Handle_FirePressed);
}

int32 AFGPlayer::GetPing() const
{
	if (GetPlayerState())
	{
		return static_cast<int32>(GetPlayerState()->GetPing());
	}

	return 0;
}

void AFGPlayer::OnPickup(AFGPickup* Pickup)
{
	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (HasAuthority())
		{
			if (Pickup->PickupType == EFGPickupType::Rocket)
			{
				HandleRocketPickup(Pickup);
			}

			Pickup->HandlePickup();
		}
		else if (IsLocallyControlled())
		{
			Pickup->HidePickup();
		}
	}
}

void AFGPlayer::HandleRocketPickup(AFGPickup* Pickup)
{
	ServerNumRockets += Pickup->NumRockets;
	Multicast_OnPickupRockets(Pickup, Pickup->NumRockets);
}

void AFGPlayer::Server_OnPickup_Implementation(AFGPickup* Pickup)
{
	Client_OnPickup(Pickup->IsPickedUp(), Pickup);
}

void AFGPlayer::Client_OnPickup_Implementation(bool ConfirmedPickup, AFGPickup* Pickup)
{
	if (!ConfirmedPickup)
	{
		Pickup->ShowPickup();
	}
}

void AFGPlayer::Multicast_OnPickupRockets_Implementation(AFGPickup* Pickup, int32 PickedUpRockets)
{
	UE_LOG(LogTemp, Warning, TEXT("This might not be logged for each client but lets see"));
	NumRockets += PickedUpRockets;
	BP_OnNumRocketsChanged(NumRockets);
	Pickup->HandlePickup();
}

void AFGPlayer::ShowDebugMenu()
{
	CreateDebugWidget();

	if (DebugMenuInstance == nullptr)
	{
		return;
	}

	DebugMenuInstance->SetVisibility(ESlateVisibility::Visible);
	DebugMenuInstance->BP_OnShowWidget();
}

void AFGPlayer::HideDebugMenu()
{
	if (DebugMenuInstance == nullptr)
	{
		return;
	}

	DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	DebugMenuInstance->BP_OnHideWidget();
}

int32 AFGPlayer::GetNumActiveRockets() const
{
	int32 NumActive = 0;
	for (AFGRocket* Rocket : RocketInstances)
	{
		 if (!Rocket->IsFree())
		 {
		 	NumActive++;
		 }
	}

	return NumActive;
}

void AFGPlayer::Server_SendLocation_Implementation(const FVector& LocationToSend)
{
	ReplicatedLocation = LocationToSend;
}

void AFGPlayer::Multicast_SendLocation_Implementation(const FVector& LocationToSend)
{
	if (!IsLocallyControlled())
	{
		SetActorLocation(LocationToSend);
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

void AFGPlayer::Server_SendYaw_Implementation(float NewYaw)
{
	ReplicatedYaw = NewYaw;
}

void AFGPlayer::CreateDebugWidget()
{
	if (DebugMenuClass == nullptr)
	{
		return;
	}

	if (!IsLocallyControlled())
	{
		return;
	}

	if (DebugMenuInstance == nullptr)
	{
		DebugMenuInstance = CreateWidget<UFGNetDebugWidget>(GetWorld(), DebugMenuClass);
		DebugMenuInstance->AddToViewport();
	}
}

void AFGPlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFGPlayer, ReplicatedYaw);
	DOREPLIFETIME(AFGPlayer, ReplicatedLocation);
	DOREPLIFETIME(AFGPlayer, RocketInstances);
}
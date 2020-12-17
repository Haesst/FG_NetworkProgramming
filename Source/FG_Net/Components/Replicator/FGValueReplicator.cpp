#include "FGValueReplicator.h"
#include "Net/UnrealNetwork.h"

void UFGValueReplicator::Tick(float DeltaTime)
{
	float CrumbDuration = (1.0f / static_cast<float>(NumberOfReplicationsPerSecond));

	if (IsLocallyControlled())
	{
		bool bIsTerminal = false;

		if (ReplicatedValueCurrent != ReplicatedValuePreviouslySent)
		{
			StaticValueTimer = 0.0f;
		}
		else
		{
			StaticValueTimer += DeltaTime;
			if (StaticValueTimer >= SleepAfterDuration)
			{
				bIsTerminal = true;
			}
		}

		SyncTimer -= DeltaTime;

		if (SyncTimer <= 0.0f)
		{
			if (bIsTerminal)
			{
				if (!bHasSentTerminalValue)
				{
					Server_SendTerminalValue(NextSyncTag++, ReplicatedValueCurrent);
					bHasSentTerminalValue = true;
				}
			}
			else
			{
				Server_SendReplicatedValue(NextSyncTag++, ReplicatedValueCurrent);
				bHasSentTerminalValue = false;
			}

			SyncTimer += CrumbDuration;
			ReplicatedValuePreviouslySent = ReplicatedValueCurrent;
		}
	}
	else
	{
		if (CrumbTrail.Num() != 0)
		{
			const float TrailLength = (CrumbDuration * (float)CrumbTrail.Num()) - (CrumbDuration - CurrentCrumbTimeRemaining);
			LerpSpeed = 1.0f;

			// If we're getting close to the end of the trail we slow down consumption
			if (TrailLength < CrumbDuration * 0.5f && !bHasReceivedTerminalValue)
			{
				LerpSpeed *= TrailLength / (CrumbDuration * 0.5f);
			}
			// If the crumb trail is getting to big we should increase consumption
			else if (TrailLength > CrumbDuration * 2.5f)
			{
				LerpSpeed *= (TrailLength / (CrumbDuration * 2.5f));
			}

			float FrameTarget = ReplicatedValueCurrent;
			float FrameTargetFuture = 0.0f;

			float RemainingLerp = LerpSpeed * DeltaTime;
			
			while (CrumbTrail.Num() > 0 && RemainingLerp > 0.001f)
			{
				float ConsumeLerp = FMath::Min(CurrentCrumbTimeRemaining, RemainingLerp);
				float CrumbSize = CurrentCrumbTimeRemaining;

				RemainingLerp -= ConsumeLerp;
				CurrentCrumbTimeRemaining -= ConsumeLerp;

				if (CurrentCrumbTimeRemaining <= 0.001f)
				{
					FrameTarget = CrumbTrail[0].Value;
					FrameTargetFuture = 0.0f;

					CrumbTrail.RemoveAt(0);
					CurrentCrumbTimeRemaining = CrumbDuration;
				}
				else
				{
					FrameTarget = CrumbTrail[0].Value;
					FrameTargetFuture = CrumbSize - ConsumeLerp;
				}
			}

			if (FrameTarget != ReplicatedValueCurrent)
			{
				if (FrameTargetFuture == 0.0f)
				{
					ReplicatedValueCurrent = FrameTarget;
				}
				else
				{
					const float AdvanceTime = (LerpSpeed * DeltaTime) - RemainingLerp;
					const float TimeToTarget = FrameTargetFuture + AdvanceTime;

					if (SmoothMode == EFGSmoothReplicatorMode::ConstantVelocity)
					{
						const float Alpha = FMath::Clamp(AdvanceTime / TimeToTarget, 0.0f, 1.0f);
						TFGSmoothReplicatorOperation<float>::InterpConstantVelocity(ReplicatedValueCurrent, FrameTarget, Alpha);
					}
				}
			}
		}
	}

	if (!ShouldTick())
	{
		SetShouldTick(false);
		bIsSleeping = true;
	}
}

void UFGValueReplicator::Init()
{
	bIsSleeping = true;
	bHasSentTerminalValue = true;
	bHasReceivedTerminalValue = true;
}

void UFGValueReplicator::SetValue(float InValue)
{
	if (InValue == ReplicatedValueCurrent)
	{
		return;
	}

	if (!IsLocallyControlled())
	{
		return;
	}

	if (bIsSleeping)
	{
		ReplicatedValueCurrent = InValue;
		SetShouldTick(true);
		bIsSleeping = false;
		bHasSentTerminalValue = false;
		SyncTimer = 0.0f;
	}
	else
	{
		ReplicatedValueCurrent = InValue;
	}

	BroadcastDelegate();
}

float UFGValueReplicator::GetValue() const
{
	return ReplicatedValueCurrent;
}

void UFGValueReplicator::BroadcastDelegate()
{
	if (OnValueChanged.IsBound())
	{
		OnValueChanged.Broadcast();
	}
}

void UFGValueReplicator::Server_SendTerminalValue_Implementation(int32 SyncTag, float TerminalValue)
{
	if (SyncTag < LastReceivedSyncTag)
	{
		return;
	}

	LastReceivedSyncTag = SyncTag;

	Multicast_SendTerminalValue(SyncTag, TerminalValue);
}

void UFGValueReplicator::Server_SendReplicatedValue_Implementation(int32 SyncTag, float ReplicatedValue)
{
	if (SyncTag < LastReceivedSyncTag)
	{
		return;
	}

	LastReceivedSyncTag = SyncTag;
	Mulitcast_SendReplicatedValue(SyncTag, ReplicatedValue);
}

void UFGValueReplicator::Multicast_SendTerminalValue_Implementation(int32 SyncTag, float TerminalValue)
{
	if (IsLocallyControlled())
	{
		return;
	}
	
	if (!HasAuthority() && SyncTag < LastReceivedSyncTag)
	{
		return;
	}
	
	LastReceivedSyncTag = SyncTag;
	bHasReceivedTerminalValue = true;
	
	auto& Crumb = CrumbTrail.Emplace_GetRef();
	Crumb.Value = TerminalValue;
	
	SetShouldTick(true);
}

void UFGValueReplicator::Mulitcast_SendReplicatedValue_Implementation(int32 SyncTag, float ReplicatedValue)
{
	if (IsLocallyControlled())
	{
		return;
	}

	if (!HasAuthority() && SyncTag < LastReceivedSyncTag)
	{
		return;
	}

	if (bHasReceivedTerminalValue)
	{
		if (CrumbTrail.Num() == 0)
		{
			auto& WaitCrumb = CrumbTrail.Emplace_GetRef();
			WaitCrumb.Value = ReplicatedValueCurrent;
		}
	}

	LastReceivedSyncTag = SyncTag;
	bHasReceivedTerminalValue = false;

	auto& Crumb = CrumbTrail.Emplace_GetRef();
	Crumb.Value = ReplicatedValue;

	if (CrumbTrail.Num() >= NumberOfReplicationsPerSecond * 2)
	{
		CrumbTrail.RemoveAt(0, 1, false);
	}

	SetShouldTick(true);
}

bool UFGValueReplicator::ShouldTick() const
{
	if (IsLocallyControlled())
	{
		if (bHasSentTerminalValue)
			return false;
	}
	else
	{
		if (bHasReceivedTerminalValue && CrumbTrail.Num() == 0)
		{
			return false;
		}
	}

	return true;
}
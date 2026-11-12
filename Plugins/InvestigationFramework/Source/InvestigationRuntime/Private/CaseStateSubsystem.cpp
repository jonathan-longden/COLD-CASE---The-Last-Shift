#include "CaseStateSubsystem.h"

#include "InvestigationSubsystem.h"

void UCaseStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Investigation = Collection.InitializeDependency<UInvestigationSubsystem>();
	if (Investigation)
	{
		CaseStartedHandle = Investigation->OnCaseStarted.AddUObject(this, &UCaseStateSubsystem::HandleCaseStarted);
	}
}

void UCaseStateSubsystem::Deinitialize()
{
	if (Investigation)
	{
		Investigation->OnCaseStarted.Remove(CaseStartedHandle);
	}
	Super::Deinitialize();
}

void UCaseStateSubsystem::HandleCaseStarted(const FCaseContent& Case)
{
	Flags.Reset();
}

void UCaseStateSubsystem::SetFlag(FName Flag)
{
	if (!Flag.IsNone())
	{
		Flags.AddUnique(Flag);
	}
}

bool UCaseStateSubsystem::HasAllFlags(const TArray<FName>& InFlags) const
{
	for (const FName Flag : InFlags)
	{
		if (!Flags.Contains(Flag))
		{
			return false;
		}
	}
	return true;
}

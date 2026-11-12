#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CaseStateSubsystem.generated.h"

struct FCaseContent;
class UInvestigationSubsystem;

/**
 * Case flags: small named facts about the state of the investigation, such as "Mick has been spooked".
 * Any system can set or read them, which is how one event comes to affect another later on.
 * Cleared whenever a case starts.
 */
UCLASS()
class INVESTIGATIONRUNTIME_API UCaseStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Case State")
	void SetFlag(FName Flag);

	UFUNCTION(BlueprintPure, Category = "Case State")
	bool HasFlag(FName Flag) const { return Flags.Contains(Flag); }

	bool HasAllFlags(const TArray<FName>& InFlags) const;

	const TArray<FName>& GetFlags() const { return Flags; }

private:
	void HandleCaseStarted(const FCaseContent& Case);

	UPROPERTY(Transient)
	TObjectPtr<UInvestigationSubsystem> Investigation;

	UPROPERTY()
	TArray<FName> Flags;

	FDelegateHandle CaseStartedHandle;
};

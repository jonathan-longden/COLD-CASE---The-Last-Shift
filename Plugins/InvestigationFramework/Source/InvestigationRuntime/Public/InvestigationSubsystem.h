#pragma once

#include "CoreMinimal.h"
#include "CaseDataTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InvestigationSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInvestigationCaseStarted, const FCaseContent& /*Case*/);
DECLARE_MULTICAST_DELEGATE(FOnInvestigationCaseConcluded);

/**
 * The front door to the investigation. Starts and ends a case and owns the loaded case content.
 *
 * It is a Game Instance Subsystem: Unreal creates exactly one when the game starts and keeps it
 * until the game closes, so the investigation survives moving between levels.
 *
 * Later milestones add the case file, interviews, saves and so on as their own subsystems that
 * read the active case from here.
 */
UCLASS()
class INVESTIGATIONRUNTIME_API UInvestigationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Loads and validates the case in the named case plugin. Returns false (and logs why) on failure. */
	UFUNCTION(BlueprintCallable, Category = "Investigation")
	bool StartCase(FName CasePluginName);

	/** Ends the active case, if any. */
	UFUNCTION(BlueprintCallable, Category = "Investigation")
	void EndCase();

	UFUNCTION(BlueprintPure, Category = "Investigation")
	bool IsCaseActive() const { return !ActiveCasePlugin.IsNone(); }

	/**
	 * Marks the investigation as concluded. This cannot be undone for this run of the case.
	 * Only after this may the case-review report read what really happened.
	 */
	UFUNCTION(BlueprintCallable, Category = "Investigation")
	void ConcludeCase();

	UFUNCTION(BlueprintPure, Category = "Investigation")
	bool IsCaseConcluded() const { return bConcluded; }

	/** The active case's content. Only meaningful while IsCaseActive() is true. */
	const FCaseContent& GetActiveCase() const { return ActiveCase; }

	/** The plugin the active case was loaded from. */
	FName GetActiveCasePlugin() const { return ActiveCasePlugin; }

	/** Broadcast after a case has loaded successfully. */
	FOnInvestigationCaseStarted OnCaseStarted;

	/** Broadcast when the player concludes the case. Not broadcast when a concluded save is loaded. */
	FOnInvestigationCaseConcluded OnCaseConcluded;

	/** For loading a save only: puts back whether the case had been concluded. */
	void RestoreConcluded(bool bInConcluded) { bConcluded = bInConcluded; }

	/** Writes a readable summary of the active case to the Output Log. */
	void LogActiveCaseSummary() const;

private:
	UPROPERTY(Transient)
	FCaseContent ActiveCase;

	FName ActiveCasePlugin;

	UPROPERTY()
	bool bConcluded = false;
};

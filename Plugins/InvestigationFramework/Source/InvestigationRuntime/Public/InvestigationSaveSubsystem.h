#pragma once

#include "CoreMinimal.h"
#include "InvestigationSaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InvestigationSaveSubsystem.generated.h"

class UCaseFileSubsystem;
class UCaseStateSubsystem;
class UInterviewSubsystem;
class UInvestigationSubsystem;
class UTheorySubsystem;

/**
 * Saves and loads the investigation.
 *
 * - Autosave: one slot, written by the game at safe moments (after an interview ends, and when the
 *   case is concluded). Never in the middle of an interview. Written in the background.
 * - Manual saves: numbered slots the player chooses.
 * Both kinds hold the same data.
 */
UCLASS()
class INVESTIGATIONRUNTIME_API UInvestigationSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** How many manual slots the player has. */
	static constexpr int32 NumManualSlots = 3;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Saves into a manual slot (1 to NumManualSlots). Not allowed during an interview. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveManual(int32 SlotNumber);

	/** Writes the autosave in the background. Does nothing during an interview. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void Autosave();

	/** Loads a manual slot, replacing the current investigation. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadManual(int32 SlotNumber);

	/** Loads the autosave, replacing the current investigation. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadAutosave();

	/** Reads a slot's label without loading it. Returns false if the slot is empty or unreadable. */
	bool GetManualSlotHeader(int32 SlotNumber, FSaveSlotHeader& OutHeader);
	bool GetAutosaveHeader(FSaveSlotHeader& OutHeader);

private:
	static FString ManualSlotName(int32 SlotNumber);
	static FString AutosaveSlotName();

	/** Gathers every system's state into a new save object. Returns nullptr if there is nothing to save. */
	UInvestigationSaveGame* CaptureSave(bool bAutosave) const;
	bool LoadSlot(const FString& SlotName);
	bool GetSlotHeader(const FString& SlotName, FSaveSlotHeader& OutHeader);
	bool CanSaveNow() const;

	void HandleInterviewEnded();
	void HandleCaseConcluded();

	UPROPERTY(Transient)
	TObjectPtr<UInvestigationSubsystem> Investigation;

	UPROPERTY(Transient)
	TObjectPtr<UCaseFileSubsystem> CaseFile;

	UPROPERTY(Transient)
	TObjectPtr<UCaseStateSubsystem> CaseState;

	UPROPERTY(Transient)
	TObjectPtr<UInterviewSubsystem> Interviews;

	UPROPERTY(Transient)
	TObjectPtr<UTheorySubsystem> Theory;

	/** Labels of slots written or read this session, so lists don't have to hit the disk each time. */
	TMap<FString, FSaveSlotHeader> HeaderCache;

	/** True while a save is being loaded, so loading never triggers an autosave. */
	bool bLoading = false;

	FDelegateHandle InterviewEndedHandle;
	FDelegateHandle CaseConcludedHandle;
};

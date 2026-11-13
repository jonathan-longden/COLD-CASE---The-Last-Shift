#pragma once

#include "CoreMinimal.h"
#include "CaseFileSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "InterviewSubsystem.h"
#include "TheorySubsystem.h"
#include "InvestigationSaveGame.generated.h"

/** Bumped whenever the save layout changes in a way old saves can't be read into. */
inline constexpr int32 GInvestigationSaveSchemaVersion = 1;

/** The small label on a save, shown in the save and load list. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FSaveSlotHeader
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 SchemaVersion = 0;

	/** True for a save the game made by itself, false for one the player made. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bAutosave = false;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime SavedAt;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FName CaseId;

	/** The plugin the case is loaded from. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FName CasePlugin;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FText CaseTitle;

	/** A one-line "where you were". */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString Summary;
};

/**
 * One saved investigation. It stores only what the player changed, and refers to case content by ID,
 * so a save keeps working when the case's text or details are edited later.
 * Autosaves and manual saves use exactly the same layout.
 */
UCLASS()
class INVESTIGATIONRUNTIME_API UInvestigationSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FSaveSlotHeader Header;

	UPROPERTY()
	bool bConcluded = false;

	UPROPERTY()
	FCaseFileSaveData CaseFile;

	UPROPERTY()
	TArray<FName> Flags;

	UPROPERTY()
	TArray<FCharacterRuntimeState> Characters;

	UPROPERTY()
	TArray<FTheoryAnswer> Answers;
};

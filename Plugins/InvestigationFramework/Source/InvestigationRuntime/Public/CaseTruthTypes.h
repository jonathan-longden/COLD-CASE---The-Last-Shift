#pragma once

#include "CoreMinimal.h"
#include "CaseTruthTypes.generated.h"

/**
 * One paragraph of what followed the investigation, shown at the end of the case review only when its
 * conditions match how the player ran the case. This is how choices made during play come home.
 */
USTRUCT()
struct INVESTIGATIONRUNTIME_API FCaseEpilogue
{
	GENERATED_BODY()

	/** Every one of these flags must have been set during the investigation. */
	UPROPERTY()
	TArray<FName> RequiresFlags;

	/** None of these flags may have been set. */
	UPROPERTY()
	TArray<FName> RequiresNotFlags;

	/** The player's conclusion must include every one of these Facts. */
	UPROPERTY()
	TArray<FName> RequiresAnswers;

	/** The player's conclusion must include none of these Facts. */
	UPROPERTY()
	TArray<FName> RequiresNotAnswers;

	UPROPERTY()
	FText Text;
};

/**
 * What actually happened (CaseData/Truth.json).
 *
 * Kept in its own type and file on purpose: systems that run during play must never read it.
 * Only the case-resolution step (after the player concludes), validators, tests and the
 * development debug overlay may load it.
 */
USTRUCT()
struct INVESTIGATIONRUNTIME_API FCaseTruth
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SchemaVersion = 0;

	UPROPERTY()
	FName CaseId;

	/** Exactly one true Fact per Question. */
	UPROPERTY()
	TArray<FName> TrueFacts;

	/** A plain-language account of what happened, for the case-review report and for writers. */
	UPROPERTY()
	FText Summary;

	/** What happened afterwards, depending on the player's choices. Every matching entry is shown, in order. */
	UPROPERTY()
	TArray<FCaseEpilogue> Epilogues;
};

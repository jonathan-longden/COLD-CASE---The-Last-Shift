#pragma once

#include "CoreMinimal.h"
#include "CaseTruthTypes.generated.h"

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
};

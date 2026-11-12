#pragma once

#include "CoreMinimal.h"

struct FCaseContent;
struct FCaseTruth;

/**
 * Consistency checks for authored case content: duplicate IDs, references to things that don't exist,
 * and (when given the truth) that exactly one Fact per Question is marked true.
 * Returns a list of human-readable problems; an empty list means no problems were found.
 */
struct INVESTIGATIONRUNTIME_API FCaseValidator
{
	static TArray<FString> Validate(const FCaseContent& Content, const FCaseTruth* Truth = nullptr);
};

#pragma once

#include "CoreMinimal.h"

struct FCaseContent;
struct FCaseTruth;

/**
 * Reads the JSON files in a case plugin's CaseData folder into the case structs.
 * Case plugins are found by plugin name, so the framework never hard-codes a case.
 */
struct INVESTIGATIONRUNTIME_API FCaseDataLoader
{
	/** Loads <CasePlugin>/CaseData/Case.json. On failure returns false and fills OutError. */
	static bool LoadContent(FName CasePluginName, FCaseContent& OutContent, FString& OutError);

	/**
	 * Loads <CasePlugin>/CaseData/Truth.json.
	 * Only for case resolution, validation, tests and debug tools. Never call this from systems used during play.
	 */
	static bool LoadTruth(FName CasePluginName, FCaseTruth& OutTruth, FString& OutError);
};

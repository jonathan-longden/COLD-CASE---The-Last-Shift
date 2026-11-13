#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InvestigationSettings.generated.h"

/**
 * Project-wide investigation settings.
 * Shown in the editor under Project Settings > Game > Investigation, saved to Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Investigation"))
class INVESTIGATIONRUNTIME_API UInvestigationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Name of the case plugin to open when play starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Cases")
	FName StartupCasePlugin = TEXT("Case000_TillRoll");

	/** Every case the player can open, in the order they are offered (and roughly of difficulty). */
	UPROPERTY(Config, EditAnywhere, Category = "Cases")
	TArray<FName> CasePlugins;
};

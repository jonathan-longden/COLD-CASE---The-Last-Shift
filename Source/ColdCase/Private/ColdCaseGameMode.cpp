#include "ColdCaseGameMode.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "InvestigationSettings.h"
#include "InvestigationSubsystem.h"

void AColdCaseGameMode::StartPlay()
{
	Super::StartPlay();

	UGameInstance* GameInstance = GetGameInstance();
	UInvestigationSubsystem* Investigation = GameInstance ? GameInstance->GetSubsystem<UInvestigationSubsystem>() : nullptr;
	if (!Investigation || Investigation->IsCaseActive())
	{
		return;
	}

	const FName CasePlugin = GetDefault<UInvestigationSettings>()->StartupCasePlugin;
	const bool bStarted = Investigation->StartCase(CasePlugin);

	// Temporary on-screen confirmation so the first build has something visible to check.
	if (GEngine)
	{
		const FString Message = bStarted
			? FString::Printf(TEXT("Case loaded: %s  (type ColdCase.DumpCase in the console for details)"), *Investigation->GetActiveCase().Title.ToString())
			: FString::Printf(TEXT("Could not load case plugin '%s'. See the Output Log (LogInvestigation)."), *CasePlugin.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 30.f, bStarted ? FColor::Green : FColor::Red, Message);
	}
}

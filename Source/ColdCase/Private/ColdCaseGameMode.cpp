#include "ColdCaseGameMode.h"

#include "ColdCasePlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "InvestigationSettings.h"
#include "InvestigationSubsystem.h"

AColdCaseGameMode::AColdCaseGameMode()
{
	PlayerControllerClass = AColdCasePlayerController::StaticClass();

	// The prototype is played entirely through its screens, so there is no character to walk around as.
	DefaultPawnClass = nullptr;
}

void AColdCaseGameMode::StartPlay()
{
	// Start the case before play begins, so the screen has a case to show from its first frame.
	UGameInstance* GameInstance = GetGameInstance();
	UInvestigationSubsystem* Investigation = GameInstance ? GameInstance->GetSubsystem<UInvestigationSubsystem>() : nullptr;
	if (Investigation && !Investigation->IsCaseActive())
	{
		const FName CasePlugin = GetDefault<UInvestigationSettings>()->StartupCasePlugin;
		if (!Investigation->StartCase(CasePlugin) && GEngine)
		{
			const FString Message = FString::Printf(TEXT("Could not load case plugin '%s'. See the Output Log (LogInvestigation)."), *CasePlugin.ToString());
			GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Red, Message);
		}
	}

	Super::StartPlay();
}

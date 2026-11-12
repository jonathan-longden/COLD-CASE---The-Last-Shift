#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ColdCaseGameMode.generated.h"

/**
 * The game's rules for a play session. It starts the configured case
 * (Project Settings > Game > Investigation > Startup Case Plugin) when play begins.
 */
UCLASS()
class COLDCASE_API AColdCaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AColdCaseGameMode();

	virtual void StartPlay() override;
};

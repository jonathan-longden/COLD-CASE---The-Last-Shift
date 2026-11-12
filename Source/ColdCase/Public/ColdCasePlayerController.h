#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ColdCasePlayerController.generated.h"

class SWidget;

/**
 * The player's controller. For the prototype its only job is to put the investigation screen
 * on the display and point mouse, keyboard and gamepad input at it.
 */
UCLASS()
class COLDCASE_API AColdCasePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<SWidget> InvestigationScreen;
};

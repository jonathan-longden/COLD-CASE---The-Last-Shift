#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ColdCasePlayerController.generated.h"

class SWidget;
class UCaseMediaPlayback;

/**
 * The player's controller. For the prototype its job is to put the investigation screen on the
 * display, point mouse, keyboard and gamepad input at it, and own the clip player the screen uses.
 */
UCLASS()
class COLDCASE_API AColdCasePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Plays video and sound clips for the screens. */
	UCaseMediaPlayback* GetMediaPlayback() const { return MediaPlayback; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCaseMediaPlayback> MediaPlayback;

	TSharedPtr<SWidget> InvestigationScreen;
};

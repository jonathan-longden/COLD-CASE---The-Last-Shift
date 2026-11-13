#include "ColdCasePlayerController.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/SInvestigationScreen.h"

void AColdCasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Only the local player has a screen. There is also nothing to draw on when running without a display.
	if (!IsLocalController() || !GEngine || !GEngine->GameViewport || !FSlateApplication::IsInitialized())
	{
		return;
	}

	InvestigationScreen = SNew(SInvestigationScreen).GameInstance(GetGameInstance());
	GEngine->GameViewport->AddViewportWidgetContent(InvestigationScreen.ToSharedRef(), 10);

	// "Game and UI" keeps the console key (`) working while the screen has focus.
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InvestigationScreen);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AColdCasePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InvestigationScreen.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(InvestigationScreen.ToSharedRef());
	}
	InvestigationScreen.Reset();

	Super::EndPlay(EndPlayReason);
}

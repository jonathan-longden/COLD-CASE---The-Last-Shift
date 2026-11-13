#include "ColdCasePlayerController.h"

#include "CaseDataLoader.h"
#include "CaseDataTypes.h"
#include "CaseMediaPlayback.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "InvestigationLog.h"
#include "InvestigationSubsystem.h"
#include "Misc/CoreMisc.h"
#include "Containers/Ticker.h"
#include "UI/SInvestigationScreen.h"

#if !UE_BUILD_SHIPPING
namespace ColdCaseMediaCommands
{
	// Development only: plays a clip by its ID without going through the screens, for testing.
	static FAutoConsoleCommandWithWorldAndArgs PlayClipCommand(
		TEXT("ColdCase.PlayClip"),
		TEXT("Development only. ColdCase.PlayClip <ClipId> plays one of the active case's clips."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
			const UInvestigationSubsystem* Investigation = Instance ? Instance->GetSubsystem<UInvestigationSubsystem>() : nullptr;
			const AColdCasePlayerController* Controller = World ? Cast<AColdCasePlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (Args.Num() < 1 || !Investigation || !Investigation->IsCaseActive() || !Controller || !Controller->GetMediaPlayback())
			{
				UE_LOG(LogInvestigation, Display, TEXT("Usage: ColdCase.PlayClip <ClipId> (a case must be active)."));
				return;
			}

			const FName ClipId(*Args[0]);
			const FCaseContent& Case = Investigation->GetActiveCase();
			const FCaseMediaClip* Found = (Case.OpeningFilm.Id == ClipId) ? &Case.OpeningFilm : nullptr;
			for (const FEvidenceItem& Item : Case.Evidence)
			{
				for (const FCaseMediaClip& Clip : Item.Media)
				{
					if (Clip.Id == ClipId)
					{
						Found = &Clip;
					}
				}
			}
			if (!Found)
			{
				UE_LOG(LogInvestigation, Display, TEXT("No clip with ID '%s'."), *ClipId.ToString());
				return;
			}
			Controller->GetMediaPlayback()->PlayFile(FCaseDataLoader::GetMediaPath(Investigation->GetActiveCasePlugin(), Found->File));
		}));

	// Development only: lets an unattended test wait for a clip before closing the game.
	static FAutoConsoleCommandWithWorldAndArgs QuitAfterCommand(
		TEXT("ColdCase.QuitAfter"),
		TEXT("Development only. ColdCase.QuitAfter <seconds> closes the game after a delay."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			// Timed on the real clock, not game time, which can run ahead when there is no display.
			const float Seconds = Args.Num() > 0 ? FMath::Max(0.1f, FCString::Atof(*Args[0])) : 5.f;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FPlatformMisc::RequestExit(false);
				return false;
			}), Seconds);
		}));
}
#endif

void AColdCasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		MediaPlayback = NewObject<UCaseMediaPlayback>(this);
		MediaPlayback->Initialize(this);
	}

	// Only the local player has a screen. There is also nothing to draw on when running without a display.
	if (!IsLocalController() || !GEngine || !GEngine->GameViewport || !FSlateApplication::IsInitialized())
	{
		return;
	}

	InvestigationScreen = SNew(SInvestigationScreen)
		.GameInstance(GetGameInstance())
		.MediaPlayback(MediaPlayback.Get());
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

	if (MediaPlayback)
	{
		MediaPlayback->Stop();
	}

	Super::EndPlay(EndPlayReason);
}

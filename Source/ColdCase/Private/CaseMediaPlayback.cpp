#include "CaseMediaPlayback.h"

#include "GameFramework/Actor.h"
#include "InvestigationLog.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"

void UCaseMediaPlayback::Initialize(AActor* SoundOwner)
{
	Player = NewObject<UMediaPlayer>(this, TEXT("ClipPlayer"));
	Player->PlayOnOpen = true;
	Player->SetLooping(false);
	Player->OnMediaOpened.AddDynamic(this, &UCaseMediaPlayback::HandleOpened);
	Player->OnMediaOpenFailed.AddDynamic(this, &UCaseMediaPlayback::HandleOpenFailed);
	Player->OnEndReached.AddDynamic(this, &UCaseMediaPlayback::HandleEndReached);

	Picture = NewObject<UMediaTexture>(this, TEXT("ClipPicture"));
	Picture->AutoClear = true;
	Picture->ClearColor = FLinearColor::Black;
	Picture->SetMediaPlayer(Player);
	Picture->UpdateResource();

	if (SoundOwner)
	{
		// The sound is not placed in the world: it plays flat, like any interface sound.
		Sound = NewObject<UMediaSoundComponent>(SoundOwner, TEXT("ClipSound"));
		Sound->bAllowSpatialization = false;
		Sound->bIsUISound = true;
		Sound->SetMediaPlayer(Player);
		Sound->RegisterComponent();
	}
}

bool UCaseMediaPlayback::PlayFile(const FString& FilePath)
{
	if (!Player)
	{
		return false;
	}

	Stop();

	if (FilePath.IsEmpty() || !FPaths::FileExists(FilePath))
	{
		UE_LOG(LogInvestigation, Warning, TEXT("Clip not found: '%s'."), *FilePath);
		return false;
	}

	if (!Player->OpenFile(FilePath))
	{
		UE_LOG(LogInvestigation, Warning, TEXT("Clip could not be opened: '%s'."), *FilePath);
		return false;
	}

	if (Sound)
	{
		Sound->Start();
	}
	UE_LOG(LogInvestigation, Log, TEXT("Clip opening: '%s'."), *FilePath);
	return true;
}

void UCaseMediaPlayback::Stop()
{
	bFinished = false;
	if (Player)
	{
		Player->Close();
	}
	if (Sound)
	{
		Sound->Stop();
	}
}

bool UCaseMediaPlayback::IsPlaying() const
{
	return Player && Player->IsPlaying();
}

void UCaseMediaPlayback::HandleOpened(FString OpenedUrl)
{
	UE_LOG(LogInvestigation, Log, TEXT("Clip playing: '%s' (%.1f seconds)."), *OpenedUrl, Player ? Player->GetDuration().GetTotalSeconds() : 0.0);
}

void UCaseMediaPlayback::HandleOpenFailed(FString FailedUrl)
{
	// A clip closed while it was still opening (the player left the page) reports a failure with no address.
	// That was deliberate, so it is not worth a warning.
	if (FailedUrl.IsEmpty())
	{
		UE_LOG(LogInvestigation, Verbose, TEXT("Clip closed before it finished opening."));
		return;
	}
	UE_LOG(LogInvestigation, Warning, TEXT("Clip failed to open: '%s'."), *FailedUrl);
}

void UCaseMediaPlayback::HandleEndReached()
{
	bFinished = true;
	UE_LOG(LogInvestigation, Log, TEXT("Clip finished."));
}

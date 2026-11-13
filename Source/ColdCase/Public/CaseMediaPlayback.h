#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CaseMediaPlayback.generated.h"

class AActor;
class UMediaPlayer;
class UMediaSoundComponent;
class UMediaTexture;

/**
 * Plays one video or sound clip at a time for the screens.
 *
 * It is presentation only: it is handed a file path and plays it. It knows nothing about cases,
 * exhibits or the state of the investigation.
 */
UCLASS()
class COLDCASE_API UCaseMediaPlayback : public UObject
{
	GENERATED_BODY()

public:
	/** Creates the player, the picture and the sound output. Sound is attached to SoundOwner. */
	void Initialize(AActor* SoundOwner);

	/** Stops whatever is playing and starts the file at FilePath. Returns false if it could not be opened. */
	bool PlayFile(const FString& FilePath);

	/** Stops and lets go of the current file. Safe to call when nothing is playing. */
	void Stop();

	/** The moving picture, for the screen to draw. Blank while nothing is playing. */
	UMediaTexture* GetPicture() const { return Picture; }

	bool IsPlaying() const;

	/** True once the current clip has played to its end. */
	bool HasFinished() const { return bFinished; }

private:
	UFUNCTION()
	void HandleOpened(FString OpenedUrl);

	UFUNCTION()
	void HandleOpenFailed(FString FailedUrl);

	UFUNCTION()
	void HandleEndReached();

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> Player;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> Picture;

	UPROPERTY(Transient)
	TObjectPtr<UMediaSoundComponent> Sound;

	bool bFinished = false;
};

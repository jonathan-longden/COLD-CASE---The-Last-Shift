#pragma once

#include "CoreMinimal.h"
#include "CaseDataTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InterviewSubsystem.generated.h"

class UCaseFileSubsystem;
class UCaseStateSubsystem;
class UInvestigationSubsystem;

/** How one person currently stands with the detective. Saved; refers to authored content by ID only. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FCharacterRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	FName CharacterId;

	/** Trust in the detective, 0-100. */
	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	int32 Trust = 50;

	/** Composure, 0-100. */
	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	int32 Composure = 70;

	/** Which step of their story they are on. */
	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	int32 StoryStep = 0;

	/** True once they have refused to speak to the detective again. */
	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	bool bRefusesInterview = false;

	/** "Once" responses they have already given. */
	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	TArray<FName> UsedResponses;
};

/** What came back from asking a question or presenting an exhibit. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FInterviewReply
{
	GENERATED_BODY()

	/** False if the action wasn't possible (no interview open, unknown topic, exhibit not held). */
	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	bool bValid = false;

	/** What the person said. */
	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	FText Line;

	/** True if the person ended the interview with this reply. */
	UPROPERTY(BlueprintReadOnly, Category = "Interview")
	bool bInterviewEnded = false;
};

/**
 * Runs interviews. The player picks a topic to ask about or an exhibit to present, and an approach.
 * The answer is chosen by rules from the character's authored responses and their current state.
 *
 * Deliberate design rules:
 * - Any topic can be asked and any held exhibit can be presented to anyone. What is offered never
 *   depends on what is true, so the choices themselves give nothing away.
 * - This system never reads the truth. It does not know which lines are lies.
 */
UCLASS()
class INVESTIGATIONRUNTIME_API UInterviewSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Opens an interview. Returns false if the person can't be interviewed or another interview is open.
	 * If they have stopped cooperating, returns false and OutRefusal holds what they say.
	 */
	bool StartInterview(FName CharacterId, FText& OutRefusal);

	/** Asks the current interviewee about a topic. */
	UFUNCTION(BlueprintCallable, Category = "Interview")
	FInterviewReply AskAbout(FName TopicId, EInterviewApproach Approach);

	/** Shows the current interviewee an exhibit from the case file. */
	UFUNCTION(BlueprintCallable, Category = "Interview")
	FInterviewReply PresentEvidence(FName EvidenceId, EInterviewApproach Approach);

	/** Closes the interview. Things waiting on "after your next interview" happen now. */
	UFUNCTION(BlueprintCallable, Category = "Interview")
	void EndInterview();

	UFUNCTION(BlueprintPure, Category = "Interview")
	bool IsInterviewActive() const { return !CurrentCharacterId.IsNone(); }

	FName GetCurrentCharacterId() const { return CurrentCharacterId; }

	/** Returns nullptr if no case is active or the character is unknown. */
	const FCharacterRuntimeState* GetCharacterState(FName CharacterId) const;

private:
	void HandleCaseStarted(const FCaseContent& Case);
	const FCaseContent* GetCase() const;
	FCharacterRuntimeState* FindState(FName CharacterId);

	/** Shared path for asking and presenting: Trigger is a topic ID or an exhibit ID. */
	FInterviewReply Respond(FName Trigger, EInterviewApproach Approach);

	/** Picks the best matching response, or nullptr if none match. */
	const FInterviewResponse* SelectResponse(const FCaseCharacter& Character, const FCharacterRuntimeState& State,
		FName Trigger, EInterviewApproach Approach) const;

	UPROPERTY(Transient)
	TObjectPtr<UInvestigationSubsystem> Investigation;

	UPROPERTY(Transient)
	TObjectPtr<UCaseFileSubsystem> CaseFile;

	UPROPERTY(Transient)
	TObjectPtr<UCaseStateSubsystem> CaseState;

	UPROPERTY()
	TArray<FCharacterRuntimeState> CharacterStates;

	/** NAME_None when no interview is open. Interviews are never saved half-way through. */
	FName CurrentCharacterId;

	FDelegateHandle CaseStartedHandle;
};

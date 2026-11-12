#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CaseFileSubsystem.generated.h"

struct FCaseContent;
class UInvestigationSubsystem;

/** The player's record of one exhibit. Saved; refers to authored content by ID only. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FHeldEvidence
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Case File")
	FName EvidenceId;

	/** Whether the player has examined it, revealing its findings. */
	UPROPERTY(BlueprintReadOnly, Category = "Case File")
	bool bExamined = false;
};

/** The reading the player chose for an ambiguous finding. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FChosenInterpretation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Case File")
	FName FindingId;

	/** NAME_None means the player has left it open. */
	UPROPERTY(BlueprintReadOnly, Category = "Case File")
	FName InterpretationId;
};

/** Something a person said in interview that asserts one or more Facts. The line itself is looked up by ID. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FRecordedStatement
{
	GENERATED_BODY()

	/** Order taken, starting at 1. Stable for the life of the save. */
	UPROPERTY(BlueprintReadOnly, Category = "Case File")
	int32 Number = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Case File")
	FName SpeakerId;

	UPROPERTY(BlueprintReadOnly, Category = "Case File")
	FName ResponseId;
};

DECLARE_MULTICAST_DELEGATE(FOnCaseFileChanged);

/**
 * Layer 3: the player's case file. What they hold, what they have examined, how they chose to read it.
 * It can be wrong, and it never reads the truth.
 *
 * Delays are event-based (no clock): a records request arrives after the player's next interview.
 */
UCLASS()
class INVESTIGATIONRUNTIME_API UCaseFileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True if the exhibit is in the case file. */
	UFUNCTION(BlueprintPure, Category = "Case File")
	bool HasEvidence(FName EvidenceId) const;

	/** Examines a held exhibit, revealing its findings. Returns false if it isn't held. */
	UFUNCTION(BlueprintCallable, Category = "Case File")
	bool ExamineEvidence(FName EvidenceId);

	/** Requests an on-request exhibit (e.g. a DVLA record). It arrives after the next interview ends. */
	UFUNCTION(BlueprintCallable, Category = "Case File")
	bool RequestEvidence(FName EvidenceId);

	/** Records the player's reading of an ambiguous finding. Pass NAME_None to leave it open. */
	UFUNCTION(BlueprintCallable, Category = "Case File")
	bool ChooseInterpretation(FName FindingId, FName InterpretationId);

	/** Called by the interview system when someone says something that asserts a Fact. Each line is recorded once. */
	void RecordStatement(FName SpeakerId, FName ResponseId);

	const TArray<FRecordedStatement>& GetStatements() const { return Statements; }

	/** Called by the interview system when an interview ends. Delivers pending requests. */
	void NotifyInterviewEnded();

	const TArray<FHeldEvidence>& GetHeldEvidence() const { return HeldEvidence; }
	const TArray<FName>& GetPendingRequests() const { return PendingRequests; }
	FName GetChosenInterpretation(FName FindingId) const;

	/**
	 * Facts the player's own material currently points to: direct findings of examined exhibits,
	 * plus the interpretations the player chose. Says nothing about whether they are true.
	 */
	TArray<FName> GetFactsSupportedByCaseFile() const;

	FOnCaseFileChanged OnCaseFileChanged;

	/** Writes the case file to the Output Log. */
	void LogCaseFile() const;

private:
	void HandleCaseStarted(const FCaseContent& Case);
	const FCaseContent* GetCase() const;
	void AddEvidence(FName EvidenceId);

	UPROPERTY(Transient)
	TObjectPtr<UInvestigationSubsystem> Investigation;

	UPROPERTY()
	TArray<FHeldEvidence> HeldEvidence;

	UPROPERTY()
	TArray<FName> PendingRequests;

	UPROPERTY()
	TArray<FChosenInterpretation> ChosenInterpretations;

	UPROPERTY()
	TArray<FRecordedStatement> Statements;

	FDelegateHandle CaseStartedHandle;
};

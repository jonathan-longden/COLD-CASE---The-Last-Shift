#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TheorySubsystem.generated.h"

struct FCaseContent;
class UCaseFileSubsystem;
class UInvestigationSubsystem;

/** The player's answer to one key Question. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FTheoryAnswer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Theory")
	FName QuestionId;

	UPROPERTY(BlueprintReadOnly, Category = "Theory")
	FName FactId;
};

/**
 * How well the player's own material supports an answer.
 * This is evidential strength only. It says nothing about whether the answer is correct:
 * a wrong answer can be well supported and a right one can be unsupported.
 */
UENUM(BlueprintType)
enum class EEvidentialStrength : uint8
{
	/** The player hasn't answered the Question. */
	Unanswered,
	/** Nothing in the case file supports it. */
	Unsupported,
	/** It rests on a single source, or only on people who weren't sure. */
	Thin,
	/** At least as much in the case file points the other way. */
	Contested,
	/** Two or more independent sources, outweighing anything against. */
	Supported
};

/** The working behind one strength assessment, for reviews and the final report. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FAnswerAssessment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Theory")
	FName QuestionId;

	UPROPERTY(BlueprintReadOnly, Category = "Theory")
	FName FactId;

	UPROPERTY(BlueprintReadOnly, Category = "Theory")
	EEvidentialStrength Strength = EEvidentialStrength::Unanswered;

	/** Independent sources currently behind the chosen answer. */
	UPROPERTY(BlueprintReadOnly, Category = "Theory")
	int32 SourcesFor = 0;

	/** Of those, how many sounded sure. */
	UPROPERTY(BlueprintReadOnly, Category = "Theory")
	int32 CertainSourcesFor = 0;

	/** Independent sources currently behind a different answer. */
	UPROPERTY(BlueprintReadOnly, Category = "Theory")
	int32 SourcesAgainst = 0;
};

/**
 * The player's theory: their answers to the case's key Questions, and how well their case file backs each one.
 *
 * IMPORTANT: this class must never read the truth. It judges only what the player can establish from
 * what they hold. Do not include CaseTruthTypes.h here.
 *
 * How sources are counted:
 * - A source is an examined exhibit (through its findings and the reading the player chose) or a person
 *   (through statements taken in interview, or an exhibit they authored).
 * - One person is one source, however many times they say it.
 * - A person's latest word on a Question replaces what they said before, including their old written
 *   statement. A witness who now doubts their account no longer supports it.
 */
UCLASS()
class INVESTIGATIONRUNTIME_API UTheorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Sets the player's answer to a key Question. Pass NAME_None as the Fact to clear it. */
	UFUNCTION(BlueprintCallable, Category = "Theory")
	bool SetAnswer(FName QuestionId, FName FactId);

	/** NAME_None if unanswered. */
	UFUNCTION(BlueprintPure, Category = "Theory")
	FName GetAnswer(FName QuestionId) const;

	/** True when every key Question has an answer. */
	UFUNCTION(BlueprintPure, Category = "Theory")
	bool IsComplete() const;

	/** Assesses the current answer to one key Question against the case file. */
	FAnswerAssessment AssessAnswer(FName QuestionId) const;

	/** One assessment per key Question, in the case's order. */
	TArray<FAnswerAssessment> AssessTheory() const;

	/** What a senior officer would say about an answer of this strength. Never refers to the truth. */
	static FText DescribeStrength(EEvidentialStrength Strength);

private:
	void HandleCaseStarted(const FCaseContent& Case);
	const FCaseContent* GetCase() const;

	UPROPERTY(Transient)
	TObjectPtr<UInvestigationSubsystem> Investigation;

	UPROPERTY(Transient)
	TObjectPtr<UCaseFileSubsystem> CaseFile;

	UPROPERTY()
	TArray<FTheoryAnswer> Answers;

	FDelegateHandle CaseStartedHandle;
};

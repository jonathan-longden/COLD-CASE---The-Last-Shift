#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TheorySubsystem.h"
#include "CaseResolutionSubsystem.generated.h"

class UCaseFileSubsystem;
class UInterviewSubsystem;
class UInvestigationSubsystem;

/** One key Question in the case-review report. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FCaseReviewAnswer
{
	GENERATED_BODY()

	/** What the player concluded and how well their file supported it. */
	UPROPERTY(BlueprintReadOnly, Category = "Case Review")
	FAnswerAssessment Assessment;

	/** What actually happened. */
	UPROPERTY(BlueprintReadOnly, Category = "Case Review")
	FName TrueFactId;

	UPROPERTY(BlueprintReadOnly, Category = "Case Review")
	bool bMatchesTruth = false;
};

/** The professional review written once the investigation is concluded. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FCaseReviewReport
{
	GENERATED_BODY()

	/** False if the report could not be produced (case not concluded, or its truth file is missing). */
	UPROPERTY(BlueprintReadOnly, Category = "Case Review")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Case Review")
	TArray<FCaseReviewAnswer> Answers;

	/** Lines of enquiry that were open to the investigation and not followed. */
	UPROPERTY(BlueprintReadOnly, Category = "Case Review")
	TArray<FText> Missed;

	/** A plain account of what happened. */
	UPROPERTY(BlueprintReadOnly, Category = "Case Review")
	FText TruthSummary;
};

/**
 * Concludes a case and writes the case-review report.
 *
 * This is the ONLY play-time class allowed to read the truth, and it refuses to until the
 * investigation has been concluded. Nothing here feeds back into play.
 */
UCLASS()
class INVESTIGATIONRUNTIME_API UCaseResolutionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Submits the player's theory as the investigation's conclusion. Fails if any key Question is unanswered.
	 * This is final: no more interviews or changes to the theory.
	 */
	UFUNCTION(BlueprintCallable, Category = "Case Review")
	bool SubmitConclusion();

	/** Builds the report. Returns an invalid report unless the case has been concluded. */
	UFUNCTION(BlueprintCallable, Category = "Case Review")
	FCaseReviewReport BuildReport() const;

	/** Writes the report to the Output Log. */
	void LogReport() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInvestigationSubsystem> Investigation;

	UPROPERTY(Transient)
	TObjectPtr<UCaseFileSubsystem> CaseFile;

	UPROPERTY(Transient)
	TObjectPtr<UInterviewSubsystem> Interviews;

	UPROPERTY(Transient)
	TObjectPtr<UTheorySubsystem> Theory;
};

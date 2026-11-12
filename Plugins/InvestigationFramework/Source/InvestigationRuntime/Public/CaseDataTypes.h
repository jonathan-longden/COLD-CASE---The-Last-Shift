#pragma once

#include "CoreMinimal.h"
#include "CaseDataTypes.generated.h"

/*
 * Authored, read-only case content.
 *
 * Prototype note: this content is loaded from JSON files in each case plugin's CaseData folder,
 * so it can be written and reviewed as text. The same structs will later sit inside Data Assets
 * without changing any of the systems that read them.
 *
 * IDs are stable names such as "TillRoll.F.SarahLeftAt.After2330". Saves and every reference
 * between pieces of content use these IDs, never list positions.
 *
 * Nothing in this file says what is TRUE. The truth lives in FCaseTruth (CaseTruthTypes.h),
 * which only the case-resolution step and tools may read.
 */

/** One possible answer to a Question. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FCaseFact
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	/** Player-facing wording, e.g. "After 23:30". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Text;
};

/** Something the investigation can ask, with its possible answers (Facts). */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FCaseQuestion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	/** Player-facing wording, e.g. "When did Sarah leave the diner?" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FCaseFact> Facts;
};

/** A person in the case. Beliefs, secrets and interview responses arrive in a later milestone. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FCaseCharacter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Name;

	/** Short label, e.g. "Waitress, worked the same shift". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Role;
};

/** One way of reading an ambiguous finding. The player chooses; the game never says which is right. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FEvidenceInterpretation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Text;

	/** Facts this reading supports. May be empty ("tells us nothing about Sarah"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FName> SupportsFacts;
};

/** Something learned from an exhibit by examining it. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FEvidenceFinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Text;

	/** Facts this finding supports directly (used when there are no interpretations). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FName> SupportsFacts;

	/** If not empty, the finding is ambiguous and the player picks one of these readings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FEvidenceInterpretation> Interpretations;
};

/** How an exhibit reaches the player's case file. */
UENUM(BlueprintType)
enum class EEvidenceAvailability : uint8
{
	/** In the original case file from the start. */
	InitialFile,
	/** Has to be requested (records, lab work) and arrives later. */
	OnRequest
};

/** One exhibit. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FEvidenceItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	/** UK-style exhibit reference: officer initials and number, e.g. "RT/3". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FString ExhibitRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	EEvidenceAvailability Availability = EEvidenceAvailability::InitialFile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FEvidenceFinding> Findings;
};

/** The root of a case's player-reachable content (CaseData/Case.json). */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FCaseContent
{
	GENERATED_BODY()

	/** Bumped whenever the file layout changes, so old files can be detected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	int32 SchemaVersion = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName CaseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Title;

	/** What the DCI tells you when you are handed the file. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Briefing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FCaseQuestion> Questions;

	/** The Questions a theory must answer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FName> KeyQuestions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FCaseCharacter> Characters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FEvidenceItem> Evidence;

	/** Lookups by ID. Return nullptr if the ID is unknown. */
	const FCaseQuestion* FindQuestion(FName QuestionId) const;
	const FCaseQuestion* FindQuestionForFact(FName FactId) const;
	const FCaseCharacter* FindCharacter(FName CharacterId) const;
	const FEvidenceItem* FindEvidence(FName EvidenceId) const;
};

/** The schema version this build of the framework understands. */
inline constexpr int32 GCaseContentSchemaVersion = 1;

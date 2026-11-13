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

/** Something the detective can ask a person about: a person, a place, an event, an object. */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FInterviewTopic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	/** Player-facing wording, e.g. "The night of 12 October". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Text;
};

/** How the detective puts a question. */
UENUM(BlueprintType)
enum class EInterviewApproach : uint8
{
	Neutral,
	/** Build rapport: patient, friendly. */
	Rapport,
	/** Challenge: press the point. */
	Challenge
};

/** Which approach a response is written for. */
UENUM(BlueprintType)
enum class EResponseApproach : uint8
{
	Any,
	Neutral,
	Rapport,
	Challenge
};

/** How sure the speaker sounds. This is about how they say it, not whether it is right. */
UENUM(BlueprintType)
enum class EStatementCertainty : uint8
{
	Certain,
	Unsure
};

/**
 * One authored thing a character can say, with the conditions under which they say it.
 *
 * There are no dialogue trees. When the player asks about a topic or presents an exhibit, the
 * interview system gathers every response whose conditions match and uses the best one
 * (highest Priority, then the one with the most conditions).
 *
 * A response never says whether it is true, a lie or a mistake. That is worked out by comparing
 * what it asserts with the truth, and only after the case is concluded.
 *
 * Note: true/false fields here have no "b" prefix so their names match the JSON files exactly.
 */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FInterviewResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	/** The topic or exhibit ID this answers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	FName Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	EResponseApproach Approach = EResponseApproach::Any;

	/** Only if the character's trust in the detective is at least this (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	int32 MinTrust = 0;

	/** Only if the character's composure is at most this (0-100). 100 means always. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	int32 MaxComposure = 100;

	/** Only while the character is on this step of their story. -1 means any step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	int32 StoryStep = -1;

	/** Only if all of these case flags are set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	TArray<FName> RequiresFlags;

	/** Only if the player holds all of these exhibits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	TArray<FName> RequiresHeld;

	/** Only if the player holds none of these exhibits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	TArray<FName> RequiresNotHeld;

	/** Higher wins when several responses match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	int32 Priority = 0;

	/** If true, the character says this only once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|When")
	bool Once = false;

	/** What they say. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Says")
	FText Line;

	/** Facts this literally asserts. If not empty, it is recorded as a statement in the case file. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Says")
	TArray<FName> AssertsFacts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Says")
	EStatementCertainty Certainty = EStatementCertainty::Certain;

	/** Change to the character's trust in the detective after saying this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Then")
	int32 TrustDelta = 0;

	/** Change to the character's composure after saying this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Then")
	int32 ComposureDelta = 0;

	/** Move the character to this step of their story. -1 leaves it alone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Then")
	int32 SetStoryStep = -1;

	/** Case flags to set. Other characters' responses can depend on them (this is how news spreads). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Then")
	TArray<FName> SetFlags;

	/** If true, the character ends the interview and refuses any further ones. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Then")
	bool EndsCooperation = false;
};

/** A person in the case. */
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

	/** Trust in the detective at the start (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Interview")
	int32 StartingTrust = 50;

	/** Composure at the start (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Interview")
	int32 StartingComposure = 70;

	/** How each approach lands with this person, applied every time it is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Interview")
	int32 RapportTrustDelta = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Interview")
	int32 ChallengeTrustDelta = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Interview")
	int32 ChallengeComposureDelta = 0;

	/** What they say when nothing else fits. A person with no DefaultLine cannot be interviewed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Interview")
	FText DefaultLine;

	/** What they say if approached after they have stopped cooperating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Interview")
	FText RefusalLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case|Interview")
	TArray<FInterviewResponse> Responses;

	bool CanBeInterviewed() const { return !DefaultLine.IsEmpty(); }
	const FInterviewResponse* FindResponse(FName ResponseId) const;
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

/** What a clip is, so the screen knows whether there is a picture to show. */
UENUM(BlueprintType)
enum class ECaseMediaKind : uint8
{
	/** Moving picture, with or without sound. */
	Video,
	/** Sound only, such as a recorded phone call. */
	Audio
};

/**
 * A recording the player can play: CCTV, a 999 call, an interview tape, an opening film.
 * The file itself lives in the case plugin's Media folder. A clip is presentation only:
 * it never changes the state of the investigation.
 */
USTRUCT(BlueprintType)
struct INVESTIGATIONRUNTIME_API FCaseMediaClip
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	ECaseMediaKind Kind = ECaseMediaKind::Video;

	/** File name inside the case plugin's Media folder, e.g. "Call999.mp4". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FString File;

	/** Short label shown in menus, e.g. "Play the 999 call". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Caption;

	/** What is said on the recording, shown beside it. May be empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FText Transcript;
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

	/**
	 * The person whose account this is, if it is one (a witness statement, a letter). Leave empty for
	 * physical evidence and records. Two sources from the same person are not independent of each other.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FName Author;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	EEvidenceAvailability Availability = EEvidenceAvailability::InitialFile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FEvidenceFinding> Findings;

	/** Recordings that belong to this exhibit. May be empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FCaseMediaClip> Media;
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

	/** Played once when the case is first opened. Leave File empty for no opening film. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FCaseMediaClip OpeningFilm;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FCaseQuestion> Questions;

	/** The Questions a theory must answer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FName> KeyQuestions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FCaseCharacter> Characters;

	/** Everything the detective can ask people about. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FInterviewTopic> Topics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TArray<FEvidenceItem> Evidence;

	/** Lookups by ID. Return nullptr if the ID is unknown. */
	const FCaseQuestion* FindQuestion(FName QuestionId) const;
	const FCaseQuestion* FindQuestionForFact(FName FactId) const;
	const FCaseCharacter* FindCharacter(FName CharacterId) const;
	const FEvidenceItem* FindEvidence(FName EvidenceId) const;
	const FInterviewTopic* FindTopic(FName TopicId) const;
	const FCaseFact* FindFact(FName FactId) const;
};

/** The schema version this build of the framework understands. */
inline constexpr int32 GCaseContentSchemaVersion = 1;

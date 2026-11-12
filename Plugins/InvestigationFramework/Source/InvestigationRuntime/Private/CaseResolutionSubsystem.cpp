#include "CaseResolutionSubsystem.h"

#include "CaseDataLoader.h"
#include "CaseDataTypes.h"
#include "CaseFileSubsystem.h"
#include "CaseTruthTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InterviewSubsystem.h"
#include "InvestigationLog.h"
#include "InvestigationSubsystem.h"

#define LOCTEXT_NAMESPACE "CaseReview"

void UCaseResolutionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Investigation = Collection.InitializeDependency<UInvestigationSubsystem>();
	CaseFile = Collection.InitializeDependency<UCaseFileSubsystem>();
	Interviews = Collection.InitializeDependency<UInterviewSubsystem>();
	Theory = Collection.InitializeDependency<UTheorySubsystem>();
}

bool UCaseResolutionSubsystem::SubmitConclusion()
{
	if (!Investigation || !Theory || !Interviews || !Investigation->IsCaseActive() || Investigation->IsCaseConcluded())
	{
		return false;
	}
	if (!Theory->IsComplete())
	{
		return false;
	}

	Interviews->EndInterview();
	Investigation->ConcludeCase();
	return true;
}

FCaseReviewReport UCaseResolutionSubsystem::BuildReport() const
{
	FCaseReviewReport Report;

	// The gate: no truth before the investigation is over.
	if (!Investigation || !CaseFile || !Interviews || !Theory || !Investigation->IsCaseConcluded())
	{
		return Report;
	}

	FCaseTruth Truth;
	FString Error;
	if (!FCaseDataLoader::LoadTruth(Investigation->GetActiveCasePlugin(), Truth, Error))
	{
		UE_LOG(LogInvestigation, Error, TEXT("Case review failed: %s"), *Error);
		return Report;
	}

	const FCaseContent& Case = Investigation->GetActiveCase();

	// 1. Each key Question: what the player concluded, how well supported, and what was true.
	for (const FAnswerAssessment& Assessment : Theory->AssessTheory())
	{
		FCaseReviewAnswer& Answer = Report.Answers.AddDefaulted_GetRef();
		Answer.Assessment = Assessment;
		if (const FCaseQuestion* Question = Case.FindQuestion(Assessment.QuestionId))
		{
			for (const FCaseFact& Fact : Question->Facts)
			{
				if (Truth.TrueFacts.Contains(Fact.Id))
				{
					Answer.TrueFactId = Fact.Id;
				}
			}
		}
		Answer.bMatchesTruth = !Answer.TrueFactId.IsNone() && Answer.TrueFactId == Assessment.FactId;
	}

	// 2. Lines of enquiry that were open and not followed.
	for (const FEvidenceItem& Item : Case.Evidence)
	{
		const FHeldEvidence* Held = CaseFile->GetHeldEvidence().FindByPredicate([&Item](const FHeldEvidence& H) { return H.EvidenceId == Item.Id; });
		if (!Held)
		{
			Report.Missed.Add(FText::Format(LOCTEXT("NeverObtained", "{0} was never obtained."), Item.Name));
			continue;
		}
		if (!Held->bExamined)
		{
			Report.Missed.Add(FText::Format(LOCTEXT("NeverExamined", "{0} ({1}) was in the file and never examined."), Item.Name, FText::FromString(Item.ExhibitRef)));
			continue;
		}
		for (const FEvidenceFinding& Finding : Item.Findings)
		{
			if (Finding.Interpretations.Num() > 0 && CaseFile->GetChosenInterpretation(Finding.Id).IsNone())
			{
				Report.Missed.Add(FText::Format(LOCTEXT("LeftOpen", "{0} ({1}): no view was taken on what it showed."), Item.Name, FText::FromString(Item.ExhibitRef)));
			}
		}
	}
	for (const FCharacterRuntimeState& State : Interviews->GetCharacterStates())
	{
		const FCaseCharacter* Character = Case.FindCharacter(State.CharacterId);
		if (!Character)
		{
			continue;
		}
		if (!State.bInterviewed)
		{
			Report.Missed.Add(FText::Format(LOCTEXT("NeverInterviewed", "{0} was available and was never interviewed."), Character->Name));
		}
		else if (State.bRefusesInterview)
		{
			Report.Missed.Add(FText::Format(LOCTEXT("LostWitness", "{0} withdrew cooperation during interview. Anything further they knew was lost to the investigation."), Character->Name));
		}
	}

	Report.TruthSummary = Truth.Summary;
	Report.bValid = true;
	return Report;
}

void UCaseResolutionSubsystem::LogReport() const
{
	const FCaseReviewReport Report = BuildReport();
	if (!Report.bValid)
	{
		UE_LOG(LogInvestigation, Display, TEXT("There is no case review until the investigation has been concluded."));
		return;
	}

	const FCaseContent& Case = Investigation->GetActiveCase();

	UE_LOG(LogInvestigation, Display, TEXT("=== CASE REVIEW: %s ==="), *Case.Title.ToString());
	UE_LOG(LogInvestigation, Display, TEXT("-- What the investigation concluded --"));
	for (const FCaseReviewAnswer& Answer : Report.Answers)
	{
		const FCaseQuestion* Question = Case.FindQuestion(Answer.Assessment.QuestionId);
		const FCaseFact* PlayerFact = Case.FindFact(Answer.Assessment.FactId);
		const FCaseFact* TrueFact = Case.FindFact(Answer.TrueFactId);
		UE_LOG(LogInvestigation, Display, TEXT("%s"), Question ? *Question->Text.ToString() : TEXT("?"));
		UE_LOG(LogInvestigation, Display, TEXT("    Concluded: %s"), PlayerFact ? *PlayerFact->Text.ToString() : TEXT("(no answer)"));
		UE_LOG(LogInvestigation, Display, TEXT("    On the evidence held: %s"), *UTheorySubsystem::DescribeStrength(Answer.Assessment.Strength).ToString());
		if (Answer.bMatchesTruth)
		{
			UE_LOG(LogInvestigation, Display, TEXT("    This was what happened."));
		}
		else
		{
			UE_LOG(LogInvestigation, Display, TEXT("    This was not what happened. In fact: %s"), TrueFact ? *TrueFact->Text.ToString() : TEXT("(not recorded)"));
		}
	}

	UE_LOG(LogInvestigation, Display, TEXT("-- Lines of enquiry not followed --"));
	if (Report.Missed.Num() == 0)
	{
		UE_LOG(LogInvestigation, Display, TEXT("None identified."));
	}
	for (const FText& Missed : Report.Missed)
	{
		UE_LOG(LogInvestigation, Display, TEXT("%s"), *Missed.ToString());
	}

	UE_LOG(LogInvestigation, Display, TEXT("-- What happened --"));
	UE_LOG(LogInvestigation, Display, TEXT("%s"), *Report.TruthSummary.ToString());
}

// ---------------------------------------------------------------------------------------------
// Console commands for testing the conclusion before the screens exist.
// ---------------------------------------------------------------------------------------------

namespace CaseResolutionConsole
{
	static UCaseResolutionSubsystem* GetSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCaseResolutionSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorld ConcludeCommand(
		TEXT("ColdCase.Conclude"),
		TEXT("Submits your theory as the final conclusion and prints the case review. This cannot be undone."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UCaseResolutionSubsystem* Resolution = GetSubsystem(World);
			if (!Resolution)
			{
				return;
			}
			if (Resolution->SubmitConclusion())
			{
				Resolution->LogReport();
			}
			else
			{
				UE_LOG(LogInvestigation, Display, TEXT("Can't conclude: answer every key question first (ColdCase.Theory), or the case is already concluded."));
			}
		}));

	static FAutoConsoleCommandWithWorld ReportCommand(
		TEXT("ColdCase.CaseReview"),
		TEXT("Prints the case review again. Only available after the case is concluded."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UCaseResolutionSubsystem* Resolution = GetSubsystem(World))
			{
				Resolution->LogReport();
			}
		}));
}

#undef LOCTEXT_NAMESPACE

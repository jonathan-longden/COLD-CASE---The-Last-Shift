#include "TheorySubsystem.h"

#include "CaseDataTypes.h"
#include "CaseFileSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InvestigationLog.h"
#include "InvestigationSubsystem.h"

#define LOCTEXT_NAMESPACE "Theory"

void UTheorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Investigation = Collection.InitializeDependency<UInvestigationSubsystem>();
	CaseFile = Collection.InitializeDependency<UCaseFileSubsystem>();
	if (Investigation)
	{
		CaseStartedHandle = Investigation->OnCaseStarted.AddUObject(this, &UTheorySubsystem::HandleCaseStarted);
	}
}

void UTheorySubsystem::Deinitialize()
{
	if (Investigation)
	{
		Investigation->OnCaseStarted.Remove(CaseStartedHandle);
	}
	Super::Deinitialize();
}

void UTheorySubsystem::HandleCaseStarted(const FCaseContent& Case)
{
	Answers.Reset();
}

const FCaseContent* UTheorySubsystem::GetCase() const
{
	return (Investigation && Investigation->IsCaseActive()) ? &Investigation->GetActiveCase() : nullptr;
}

bool UTheorySubsystem::SetAnswer(FName QuestionId, FName FactId)
{
	const FCaseContent* Case = GetCase();
	if (!Case || Investigation->IsCaseConcluded() || !Case->KeyQuestions.Contains(QuestionId))
	{
		return false;
	}

	// Any of the Question's possible answers may be chosen. The choice is never checked against the truth.
	const FCaseQuestion* Question = Case->FindQuestion(QuestionId);
	const bool bFactBelongs = Question && Question->Facts.ContainsByPredicate([FactId](const FCaseFact& F) { return F.Id == FactId; });
	if (!FactId.IsNone() && !bFactBelongs)
	{
		return false;
	}

	Answers.RemoveAll([QuestionId](const FTheoryAnswer& A) { return A.QuestionId == QuestionId; });
	if (!FactId.IsNone())
	{
		FTheoryAnswer& Answer = Answers.AddDefaulted_GetRef();
		Answer.QuestionId = QuestionId;
		Answer.FactId = FactId;
	}
	return true;
}

void UTheorySubsystem::RestoreAnswers(const TArray<FTheoryAnswer>& InAnswers)
{
	Answers.Reset();
	const FCaseContent* Case = GetCase();
	if (!Case)
	{
		return;
	}
	for (const FTheoryAnswer& Answer : InAnswers)
	{
		const FCaseQuestion* Question = Case->KeyQuestions.Contains(Answer.QuestionId) ? Case->FindQuestion(Answer.QuestionId) : nullptr;
		const FName FactId = Answer.FactId;
		if (Question && Question->Facts.ContainsByPredicate([FactId](const FCaseFact& F) { return F.Id == FactId; }))
		{
			Answers.Add(Answer);
		}
	}
}

FName UTheorySubsystem::GetAnswer(FName QuestionId) const
{
	const FTheoryAnswer* Answer = Answers.FindByPredicate([QuestionId](const FTheoryAnswer& A) { return A.QuestionId == QuestionId; });
	return Answer ? Answer->FactId : NAME_None;
}

bool UTheorySubsystem::IsComplete() const
{
	const FCaseContent* Case = GetCase();
	if (!Case)
	{
		return false;
	}
	for (const FName QuestionId : Case->KeyQuestions)
	{
		if (GetAnswer(QuestionId).IsNone())
		{
			return false;
		}
	}
	return true;
}

FAnswerAssessment UTheorySubsystem::AssessAnswer(FName QuestionId) const
{
	FAnswerAssessment Assessment;
	Assessment.QuestionId = QuestionId;
	Assessment.FactId = GetAnswer(QuestionId);

	const FCaseContent* Case = GetCase();
	const FCaseQuestion* Question = Case ? Case->FindQuestion(QuestionId) : nullptr;
	if (!Question || !CaseFile || Assessment.FactId.IsNone())
	{
		return Assessment;
	}

	// Where each independent source currently stands on this Question.
	struct FSourcePosition
	{
		FName FactId;
		bool bCertain = true;
	};
	TMap<FName, FSourcePosition> Positions;

	auto IsFactOfQuestion = [Question](FName FactId)
	{
		return Question->Facts.ContainsByPredicate([FactId](const FCaseFact& F) { return F.Id == FactId; });
	};

	// 1. Exhibits the player has examined, read the way the player chose to read them.
	for (const FHeldEvidence& Held : CaseFile->GetHeldEvidence())
	{
		const FEvidenceItem* Item = Held.bExamined ? Case->FindEvidence(Held.EvidenceId) : nullptr;
		if (!Item)
		{
			continue;
		}
		// A written account counts as its author speaking, so it isn't independent of them.
		const FName Source = Item->Author.IsNone() ? Item->Id : Item->Author;

		for (const FEvidenceFinding& Finding : Item->Findings)
		{
			TArray<FName> Facts = Finding.SupportsFacts;
			const FName ChosenId = CaseFile->GetChosenInterpretation(Finding.Id);
			if (const FEvidenceInterpretation* Chosen = Finding.Interpretations.FindByPredicate([ChosenId](const FEvidenceInterpretation& I) { return I.Id == ChosenId; }))
			{
				Facts.Append(Chosen->SupportsFacts);
			}
			for (const FName FactId : Facts)
			{
				if (IsFactOfQuestion(FactId))
				{
					Positions.Add(Source, { FactId, true });
				}
			}
		}
	}

	// 2. Statements taken in interview, oldest first, so a person's latest word replaces their earlier one.
	for (const FRecordedStatement& Statement : CaseFile->GetStatements())
	{
		const FCaseCharacter* Speaker = Case->FindCharacter(Statement.SpeakerId);
		const FInterviewResponse* Response = Speaker ? Speaker->FindResponse(Statement.ResponseId) : nullptr;
		if (!Response)
		{
			continue;
		}
		for (const FName FactId : Response->AssertsFacts)
		{
			if (IsFactOfQuestion(FactId))
			{
				Positions.Add(Statement.SpeakerId, { FactId, Response->Certainty == EStatementCertainty::Certain });
			}
		}
	}

	for (const TPair<FName, FSourcePosition>& Position : Positions)
	{
		if (Position.Value.FactId == Assessment.FactId)
		{
			++Assessment.SourcesFor;
			Assessment.CertainSourcesFor += Position.Value.bCertain ? 1 : 0;
		}
		else
		{
			++Assessment.SourcesAgainst;
		}
	}

	if (Assessment.SourcesFor == 0)
	{
		Assessment.Strength = EEvidentialStrength::Unsupported;
	}
	else if (Assessment.SourcesAgainst >= Assessment.SourcesFor)
	{
		Assessment.Strength = EEvidentialStrength::Contested;
	}
	else if (Assessment.SourcesFor == 1 || Assessment.CertainSourcesFor == 0)
	{
		Assessment.Strength = EEvidentialStrength::Thin;
	}
	else
	{
		Assessment.Strength = EEvidentialStrength::Supported;
	}
	return Assessment;
}

TArray<FAnswerAssessment> UTheorySubsystem::AssessTheory() const
{
	TArray<FAnswerAssessment> Assessments;
	if (const FCaseContent* Case = GetCase())
	{
		for (const FName QuestionId : Case->KeyQuestions)
		{
			Assessments.Add(AssessAnswer(QuestionId));
		}
	}
	return Assessments;
}

FText UTheorySubsystem::DescribeStrength(EEvidentialStrength Strength)
{
	switch (Strength)
	{
	case EEvidentialStrength::Unsupported:
		return LOCTEXT("Unsupported", "There is nothing in the file behind this. As it stands it is an opinion.");
	case EEvidentialStrength::Thin:
		return LOCTEXT("Thin", "This rests on very little: one source, or people who weren't sure. It would not survive a challenge.");
	case EEvidentialStrength::Contested:
		return LOCTEXT("Contested", "The file points both ways on this. You would have to explain away what contradicts it.");
	case EEvidentialStrength::Supported:
		return LOCTEXT("Supported", "More than one independent source says this, and they outweigh what is against it. This would stand up.");
	default:
		return LOCTEXT("Unanswered", "You haven't given an answer.");
	}
}

// ---------------------------------------------------------------------------------------------
// Console commands for testing the theory before the screens exist.
// ---------------------------------------------------------------------------------------------

namespace TheoryConsole
{
	static UTheorySubsystem* GetSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UTheorySubsystem>() : nullptr;
	}

	static const FCaseContent* GetCase(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UInvestigationSubsystem* Investigation = GameInstance ? GameInstance->GetSubsystem<UInvestigationSubsystem>() : nullptr;
		return (Investigation && Investigation->IsCaseActive()) ? &Investigation->GetActiveCase() : nullptr;
	}

	static FAutoConsoleCommandWithWorld TheoryCommand(
		TEXT("ColdCase.Theory"),
		TEXT("Lists the key questions, their possible answers and the answer you have chosen."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const UTheorySubsystem* Theory = GetSubsystem(World);
			const FCaseContent* Case = GetCase(World);
			if (!Theory || !Case)
			{
				return;
			}
			UE_LOG(LogInvestigation, Display, TEXT("=== Your theory ==="));
			for (const FName QuestionId : Case->KeyQuestions)
			{
				const FCaseQuestion* Question = Case->FindQuestion(QuestionId);
				if (!Question)
				{
					continue;
				}
				UE_LOG(LogInvestigation, Display, TEXT("%s  id=%s"), *Question->Text.ToString(), *Question->Id.ToString());
				const FName Chosen = Theory->GetAnswer(QuestionId);
				for (const FCaseFact& Fact : Question->Facts)
				{
					UE_LOG(LogInvestigation, Display, TEXT("    %s %s  id=%s"), Fact.Id == Chosen ? TEXT("(*)") : TEXT("( )"), *Fact.Text.ToString(), *Fact.Id.ToString());
				}
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs AnswerCommand(
		TEXT("ColdCase.Answer"),
		TEXT("ColdCase.Answer <QuestionId> [FactId]: sets your answer to a key question (omit the second to clear it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UTheorySubsystem* Theory = GetSubsystem(World);
			if (Theory && (Args.Num() == 1 || Args.Num() == 2))
			{
				const FName FactId = Args.Num() == 2 ? FName(*Args[1]) : NAME_None;
				UE_LOG(LogInvestigation, Display, TEXT("%s"), Theory->SetAnswer(FName(*Args[0]), FactId) ? TEXT("Noted.") : TEXT("Not possible."));
			}
		}));

	static FAutoConsoleCommandWithWorld ReviewCommand(
		TEXT("ColdCase.Review"),
		TEXT("Takes your theory to the DCI, who tells you how well the file supports each answer."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const UTheorySubsystem* Theory = GetSubsystem(World);
			const FCaseContent* Case = GetCase(World);
			if (!Theory || !Case)
			{
				return;
			}
			UE_LOG(LogInvestigation, Display, TEXT("=== DCI review ==="));
			for (const FAnswerAssessment& Assessment : Theory->AssessTheory())
			{
				const FCaseQuestion* Question = Case->FindQuestion(Assessment.QuestionId);
				const FCaseFact* Fact = Case->FindFact(Assessment.FactId);
				UE_LOG(LogInvestigation, Display, TEXT("%s  %s"), Question ? *Question->Text.ToString() : TEXT("?"),
					Fact ? *Fact->Text.ToString() : TEXT("(no answer)"));
				UE_LOG(LogInvestigation, Display, TEXT("    DCI: %s"), *UTheorySubsystem::DescribeStrength(Assessment.Strength).ToString());
			}
		}));
}

#undef LOCTEXT_NAMESPACE

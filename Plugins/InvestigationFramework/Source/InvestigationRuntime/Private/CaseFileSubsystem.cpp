#include "CaseFileSubsystem.h"

#include "CaseDataTypes.h"
#include "CaseStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InvestigationLog.h"
#include "InvestigationSubsystem.h"

void UCaseFileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Make sure the investigation subsystem exists first, then listen for cases starting.
	Investigation = Collection.InitializeDependency<UInvestigationSubsystem>();
	CaseState = Collection.InitializeDependency<UCaseStateSubsystem>();
	if (Investigation)
	{
		CaseStartedHandle = Investigation->OnCaseStarted.AddUObject(this, &UCaseFileSubsystem::HandleCaseStarted);
	}
}

void UCaseFileSubsystem::Deinitialize()
{
	if (Investigation)
	{
		Investigation->OnCaseStarted.Remove(CaseStartedHandle);
	}
	Super::Deinitialize();
}

const FCaseContent* UCaseFileSubsystem::GetCase() const
{
	return (Investigation && Investigation->IsCaseActive()) ? &Investigation->GetActiveCase() : nullptr;
}

bool UCaseFileSubsystem::IsOpen() const
{
	return Investigation && Investigation->IsCaseActive() && !Investigation->IsCaseConcluded();
}

void UCaseFileSubsystem::HandleCaseStarted(const FCaseContent& Case)
{
	HeldEvidence.Reset();
	PendingRequests.Reset();
	ChosenInterpretations.Reset();
	Statements.Reset();

	// The original file: everything that was in the box when it came up from the archive.
	for (const FEvidenceItem& Item : Case.Evidence)
	{
		if (Item.Availability == EEvidenceAvailability::InitialFile)
		{
			FHeldEvidence& Held = HeldEvidence.AddDefaulted_GetRef();
			Held.EvidenceId = Item.Id;
		}
	}
	OnCaseFileChanged.Broadcast();
}

FCaseFileSaveData UCaseFileSubsystem::GetSaveData() const
{
	FCaseFileSaveData Data;
	Data.HeldEvidence = HeldEvidence;
	Data.PendingRequests = PendingRequests;
	Data.ChosenInterpretations = ChosenInterpretations;
	Data.Statements = Statements;
	return Data;
}

void UCaseFileSubsystem::RestoreSaveData(const FCaseFileSaveData& Data)
{
	const FCaseContent* Case = GetCase();
	if (!Case)
	{
		return;
	}

	// Saves refer to content by ID. If an update removed something, drop it rather than fail the load.
	HeldEvidence = Data.HeldEvidence.FilterByPredicate([Case](const FHeldEvidence& H) { return Case->FindEvidence(H.EvidenceId) != nullptr; });
	PendingRequests = Data.PendingRequests.FilterByPredicate([Case](const FName& Id) { return Case->FindEvidence(Id) != nullptr; });
	ChosenInterpretations = Data.ChosenInterpretations;
	Statements = Data.Statements.FilterByPredicate([Case](const FRecordedStatement& S)
	{
		const FCaseCharacter* Speaker = Case->FindCharacter(S.SpeakerId);
		return Speaker && Speaker->FindResponse(S.ResponseId) != nullptr;
	});
	OnCaseFileChanged.Broadcast();
}

bool UCaseFileSubsystem::HasEvidence(FName EvidenceId) const
{
	return HeldEvidence.ContainsByPredicate([EvidenceId](const FHeldEvidence& H) { return H.EvidenceId == EvidenceId; });
}

void UCaseFileSubsystem::AddEvidence(FName EvidenceId)
{
	if (!HasEvidence(EvidenceId))
	{
		FHeldEvidence& Held = HeldEvidence.AddDefaulted_GetRef();
		Held.EvidenceId = EvidenceId;
	}
}

bool UCaseFileSubsystem::ExamineEvidence(FName EvidenceId)
{
	FHeldEvidence* Held = HeldEvidence.FindByPredicate([EvidenceId](const FHeldEvidence& H) { return H.EvidenceId == EvidenceId; });
	if (!Held || !IsOpen())
	{
		return false;
	}
	if (!Held->bExamined)
	{
		Held->bExamined = true;

		// Reading an exhibit can open a new line of enquiry (a name, a date, a place to ask about).
		const FCaseContent* Case = GetCase();
		const FEvidenceItem* Item = Case ? Case->FindEvidence(EvidenceId) : nullptr;
		if (Item && CaseState)
		{
			for (const FName Flag : Item->SetFlagsWhenExamined)
			{
				CaseState->SetFlag(Flag);
			}
		}
		OnCaseFileChanged.Broadcast();
	}
	return true;
}

bool UCaseFileSubsystem::CanRequestEvidence(FName EvidenceId) const
{
	const FCaseContent* Case = GetCase();
	const FEvidenceItem* Item = Case ? Case->FindEvidence(EvidenceId) : nullptr;
	if (!IsOpen() || !Item || Item->Availability != EEvidenceAvailability::OnRequest || HasEvidence(EvidenceId) || PendingRequests.Contains(EvidenceId))
	{
		return false;
	}
	return Item->RequiresFlags.Num() == 0 || (CaseState && CaseState->HasAllFlags(Item->RequiresFlags));
}

bool UCaseFileSubsystem::RequestEvidence(FName EvidenceId)
{
	if (!CanRequestEvidence(EvidenceId))
	{
		return false;
	}
	PendingRequests.Add(EvidenceId);
	OnCaseFileChanged.Broadcast();
	return true;
}

void UCaseFileSubsystem::RecordStatement(FName SpeakerId, FName ResponseId)
{
	const bool bAlreadyRecorded = Statements.ContainsByPredicate([SpeakerId, ResponseId](const FRecordedStatement& S)
	{
		return S.SpeakerId == SpeakerId && S.ResponseId == ResponseId;
	});
	if (bAlreadyRecorded)
	{
		return;
	}

	const int32 NextNumber = Statements.Num() > 0 ? Statements.Last().Number + 1 : 1;
	FRecordedStatement& Statement = Statements.AddDefaulted_GetRef();
	Statement.Number = NextNumber;
	Statement.SpeakerId = SpeakerId;
	Statement.ResponseId = ResponseId;
	OnCaseFileChanged.Broadcast();
}

void UCaseFileSubsystem::NotifyInterviewEnded()
{
	if (PendingRequests.Num() == 0)
	{
		return;
	}
	for (const FName EvidenceId : PendingRequests)
	{
		AddEvidence(EvidenceId);
	}
	PendingRequests.Reset();
	OnCaseFileChanged.Broadcast();
}

bool UCaseFileSubsystem::ChooseInterpretation(FName FindingId, FName InterpretationId)
{
	const FCaseContent* Case = GetCase();
	if (!Case || !IsOpen())
	{
		return false;
	}

	// The finding must belong to an exhibit the player holds and has examined,
	// and the interpretation (if any) must be one of that finding's readings.
	bool bValid = false;
	for (const FHeldEvidence& Held : HeldEvidence)
	{
		const FEvidenceItem* Item = Held.bExamined ? Case->FindEvidence(Held.EvidenceId) : nullptr;
		const FEvidenceFinding* Finding = Item ? Item->Findings.FindByPredicate([FindingId](const FEvidenceFinding& F) { return F.Id == FindingId; }) : nullptr;
		if (Finding)
		{
			bValid = InterpretationId.IsNone()
				|| Finding->Interpretations.ContainsByPredicate([InterpretationId](const FEvidenceInterpretation& I) { return I.Id == InterpretationId; });
			break;
		}
	}
	if (!bValid)
	{
		return false;
	}

	ChosenInterpretations.RemoveAll([FindingId](const FChosenInterpretation& C) { return C.FindingId == FindingId; });
	if (!InterpretationId.IsNone())
	{
		FChosenInterpretation& Chosen = ChosenInterpretations.AddDefaulted_GetRef();
		Chosen.FindingId = FindingId;
		Chosen.InterpretationId = InterpretationId;
	}
	OnCaseFileChanged.Broadcast();
	return true;
}

FName UCaseFileSubsystem::GetChosenInterpretation(FName FindingId) const
{
	const FChosenInterpretation* Chosen = ChosenInterpretations.FindByPredicate([FindingId](const FChosenInterpretation& C) { return C.FindingId == FindingId; });
	return Chosen ? Chosen->InterpretationId : NAME_None;
}

TArray<FName> UCaseFileSubsystem::GetFactsSupportedByCaseFile() const
{
	TArray<FName> Facts;
	const FCaseContent* Case = GetCase();
	if (!Case)
	{
		return Facts;
	}

	for (const FHeldEvidence& Held : HeldEvidence)
	{
		const FEvidenceItem* Item = Held.bExamined ? Case->FindEvidence(Held.EvidenceId) : nullptr;
		if (!Item)
		{
			continue;
		}
		for (const FEvidenceFinding& Finding : Item->Findings)
		{
			for (const FName Fact : Finding.SupportsFacts)
			{
				Facts.AddUnique(Fact);
			}
			const FName ChosenId = GetChosenInterpretation(Finding.Id);
			const FEvidenceInterpretation* Chosen = Finding.Interpretations.FindByPredicate([ChosenId](const FEvidenceInterpretation& I) { return I.Id == ChosenId; });
			if (Chosen)
			{
				for (const FName Fact : Chosen->SupportsFacts)
				{
					Facts.AddUnique(Fact);
				}
			}
		}
	}
	return Facts;
}

void UCaseFileSubsystem::LogCaseFile() const
{
	const FCaseContent* Case = GetCase();
	if (!Case)
	{
		UE_LOG(LogInvestigation, Display, TEXT("No case is active."));
		return;
	}

	UE_LOG(LogInvestigation, Display, TEXT("=== Case file: %s ==="), *Case->Title.ToString());
	for (const FHeldEvidence& Held : HeldEvidence)
	{
		const FEvidenceItem* Item = Case->FindEvidence(Held.EvidenceId);
		if (!Item)
		{
			continue;
		}
		UE_LOG(LogInvestigation, Display, TEXT("%s  %s  [%s]  id=%s"), *Item->ExhibitRef, *Item->Name.ToString(),
			Held.bExamined ? TEXT("examined") : TEXT("not examined"), *Item->Id.ToString());
		if (!Held.bExamined)
		{
			continue;
		}
		for (const FEvidenceFinding& Finding : Item->Findings)
		{
			UE_LOG(LogInvestigation, Display, TEXT("    %s"), *Finding.Text.ToString());
			const FName ChosenId = GetChosenInterpretation(Finding.Id);
			for (const FEvidenceInterpretation& Interpretation : Finding.Interpretations)
			{
				UE_LOG(LogInvestigation, Display, TEXT("      %s %s  id=%s"), Interpretation.Id == ChosenId ? TEXT("(*)") : TEXT("( )"),
					*Interpretation.Text.ToString(), *Interpretation.Id.ToString());
			}
		}
	}
	for (const FName Pending : PendingRequests)
	{
		UE_LOG(LogInvestigation, Display, TEXT("Requested, not yet arrived: %s"), *Pending.ToString());
	}

	if (Statements.Num() > 0)
	{
		UE_LOG(LogInvestigation, Display, TEXT("-- Statements taken --"));
		for (const FRecordedStatement& Statement : Statements)
		{
			const FCaseCharacter* Speaker = Case->FindCharacter(Statement.SpeakerId);
			const FInterviewResponse* Response = Speaker ? Speaker->FindResponse(Statement.ResponseId) : nullptr;
			if (Response)
			{
				UE_LOG(LogInvestigation, Display, TEXT("%d. %s%s: %s"), Statement.Number, *Speaker->Name.ToString(),
					Response->Certainty == EStatementCertainty::Unsure ? TEXT(" (unsure)") : TEXT(""), *Response->Line.ToString());
			}
		}
	}

	UE_LOG(LogInvestigation, Display, TEXT("-- Your exhibits currently point to --"));
	for (const FName FactId : GetFactsSupportedByCaseFile())
	{
		const FCaseQuestion* Question = Case->FindQuestionForFact(FactId);
		const FCaseFact* Fact = Question ? Question->Facts.FindByPredicate([FactId](const FCaseFact& F) { return F.Id == FactId; }) : nullptr;
		if (Question && Fact)
		{
			UE_LOG(LogInvestigation, Display, TEXT("%s  %s"), *Question->Text.ToString(), *Fact->Text.ToString());
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Console commands for testing the case file before the screens exist.
// ---------------------------------------------------------------------------------------------

namespace CaseFileConsole
{
	static UCaseFileSubsystem* GetSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCaseFileSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorld ShowCommand(
		TEXT("ColdCase.CaseFile"),
		TEXT("Prints the player's case file."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UCaseFileSubsystem* CaseFile = GetSubsystem(World))
			{
				CaseFile->LogCaseFile();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs ExamineCommand(
		TEXT("ColdCase.Examine"),
		TEXT("ColdCase.Examine <EvidenceId>: examines an exhibit you hold."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCaseFileSubsystem* CaseFile = GetSubsystem(World);
			if (CaseFile && Args.Num() == 1)
			{
				UE_LOG(LogInvestigation, Display, TEXT("%s"), CaseFile->ExamineEvidence(FName(*Args[0])) ? TEXT("Examined.") : TEXT("You don't hold that exhibit."));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs RequestCommand(
		TEXT("ColdCase.Request"),
		TEXT("ColdCase.Request <EvidenceId>: requests a record. It arrives after your next interview."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCaseFileSubsystem* CaseFile = GetSubsystem(World);
			if (CaseFile && Args.Num() == 1)
			{
				UE_LOG(LogInvestigation, Display, TEXT("%s"), CaseFile->RequestEvidence(FName(*Args[0])) ? TEXT("Requested.") : TEXT("That can't be requested."));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs InterpretCommand(
		TEXT("ColdCase.Interpret"),
		TEXT("ColdCase.Interpret <FindingId> [InterpretationId]: chooses how to read a finding (omit the second to leave it open)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCaseFileSubsystem* CaseFile = GetSubsystem(World);
			if (CaseFile && (Args.Num() == 1 || Args.Num() == 2))
			{
				const FName Interpretation = Args.Num() == 2 ? FName(*Args[1]) : NAME_None;
				UE_LOG(LogInvestigation, Display, TEXT("%s"), CaseFile->ChooseInterpretation(FName(*Args[0]), Interpretation) ? TEXT("Noted.") : TEXT("Not possible."));
			}
		}));

}

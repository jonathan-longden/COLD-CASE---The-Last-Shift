#include "InterviewSubsystem.h"

#include "CaseFileSubsystem.h"
#include "CaseStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InvestigationLog.h"
#include "InvestigationSubsystem.h"

void UInterviewSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Investigation = Collection.InitializeDependency<UInvestigationSubsystem>();
	CaseFile = Collection.InitializeDependency<UCaseFileSubsystem>();
	CaseState = Collection.InitializeDependency<UCaseStateSubsystem>();
	if (Investigation)
	{
		CaseStartedHandle = Investigation->OnCaseStarted.AddUObject(this, &UInterviewSubsystem::HandleCaseStarted);
	}
}

void UInterviewSubsystem::Deinitialize()
{
	if (Investigation)
	{
		Investigation->OnCaseStarted.Remove(CaseStartedHandle);
	}
	Super::Deinitialize();
}

const FCaseContent* UInterviewSubsystem::GetCase() const
{
	return (Investigation && Investigation->IsCaseActive()) ? &Investigation->GetActiveCase() : nullptr;
}

void UInterviewSubsystem::HandleCaseStarted(const FCaseContent& Case)
{
	CurrentCharacterId = NAME_None;
	CharacterStates.Reset();
	for (const FCaseCharacter& Character : Case.Characters)
	{
		if (Character.CanBeInterviewed())
		{
			FCharacterRuntimeState& State = CharacterStates.AddDefaulted_GetRef();
			State.CharacterId = Character.Id;
			State.Trust = FMath::Clamp(Character.StartingTrust, 0, 100);
			State.Composure = FMath::Clamp(Character.StartingComposure, 0, 100);
		}
	}
}

FCharacterRuntimeState* UInterviewSubsystem::FindState(FName CharacterId)
{
	return CharacterStates.FindByPredicate([CharacterId](const FCharacterRuntimeState& S) { return S.CharacterId == CharacterId; });
}

const FCharacterRuntimeState* UInterviewSubsystem::GetCharacterState(FName CharacterId) const
{
	return CharacterStates.FindByPredicate([CharacterId](const FCharacterRuntimeState& S) { return S.CharacterId == CharacterId; });
}

bool UInterviewSubsystem::IsCharacterAvailable(FName CharacterId) const
{
	const FCaseContent* Case = GetCase();
	const FCaseCharacter* Character = Case ? Case->FindCharacter(CharacterId) : nullptr;
	if (!Character || !Character->CanBeInterviewed() || !GetCharacterState(CharacterId))
	{
		return false;
	}
	return Character->RequiresFlags.Num() == 0 || (CaseState && CaseState->HasAllFlags(Character->RequiresFlags));
}

bool UInterviewSubsystem::IsTopicAvailable(FName TopicId) const
{
	const FCaseContent* Case = GetCase();
	const FInterviewTopic* Topic = Case ? Case->FindTopic(TopicId) : nullptr;
	return Topic && (Topic->RequiresFlags.Num() == 0 || (CaseState && CaseState->HasAllFlags(Topic->RequiresFlags)));
}

bool UInterviewSubsystem::StartInterview(FName CharacterId, FText& OutRefusal)
{
	OutRefusal = FText::GetEmpty();

	const FCaseContent* Case = GetCase();
	const FCaseCharacter* Character = Case ? Case->FindCharacter(CharacterId) : nullptr;
	FCharacterRuntimeState* State = FindState(CharacterId);
	if (!Character || !State || IsInterviewActive() || Investigation->IsCaseConcluded() || !IsCharacterAvailable(CharacterId))
	{
		return false;
	}

	if (State->bRefusesInterview)
	{
		OutRefusal = Character->RefusalLine;
		return false;
	}

	State->bInterviewed = true;
	CurrentCharacterId = CharacterId;
	return true;
}

void UInterviewSubsystem::EndInterview()
{
	if (!IsInterviewActive())
	{
		return;
	}
	CurrentCharacterId = NAME_None;
	if (CaseFile)
	{
		CaseFile->NotifyInterviewEnded();
	}
	OnInterviewEnded.Broadcast();
}

void UInterviewSubsystem::RestoreCharacterStates(const TArray<FCharacterRuntimeState>& InStates)
{
	CurrentCharacterId = NAME_None;
	for (const FCharacterRuntimeState& Saved : InStates)
	{
		if (FCharacterRuntimeState* State = FindState(Saved.CharacterId))
		{
			*State = Saved;
		}
	}
}

FInterviewReply UInterviewSubsystem::AskAbout(FName TopicId, EInterviewApproach Approach)
{
	const FCaseContent* Case = GetCase();
	if (!Case || !IsTopicAvailable(TopicId))
	{
		return FInterviewReply();
	}
	return Respond(TopicId, Approach);
}

FInterviewReply UInterviewSubsystem::PresentEvidence(FName EvidenceId, EInterviewApproach Approach)
{
	// Any exhibit the player holds can be shown to anyone. Whether it is relevant is for the player to judge.
	if (!CaseFile || !CaseFile->HasEvidence(EvidenceId))
	{
		return FInterviewReply();
	}
	return Respond(EvidenceId, Approach);
}

FInterviewReply UInterviewSubsystem::Respond(FName Trigger, EInterviewApproach Approach)
{
	FInterviewReply Reply;

	const FCaseContent* Case = GetCase();
	const FCaseCharacter* Character = Case ? Case->FindCharacter(CurrentCharacterId) : nullptr;
	FCharacterRuntimeState* State = FindState(CurrentCharacterId);
	if (!Character || !State)
	{
		return Reply;
	}

	// 1. The approach itself lands first: pressing someone can rattle them before they answer.
	if (Approach == EInterviewApproach::Rapport)
	{
		State->Trust += Character->RapportTrustDelta;
	}
	else if (Approach == EInterviewApproach::Challenge)
	{
		State->Trust += Character->ChallengeTrustDelta;
		State->Composure += Character->ChallengeComposureDelta;
	}
	State->Trust = FMath::Clamp(State->Trust, 0, 100);
	State->Composure = FMath::Clamp(State->Composure, 0, 100);

	Reply.bValid = true;

	// 2. Choose what they say.
	const FInterviewResponse* Response = SelectResponse(*Character, *State, Trigger, Approach);
	if (!Response)
	{
		Reply.Line = Character->DefaultLine;
		return Reply;
	}
	Reply.Line = Response->Line;

	// 3. Apply what saying it does.
	State->Trust = FMath::Clamp(State->Trust + Response->TrustDelta, 0, 100);
	State->Composure = FMath::Clamp(State->Composure + Response->ComposureDelta, 0, 100);
	if (Response->SetStoryStep >= 0)
	{
		State->StoryStep = Response->SetStoryStep;
	}
	if (Response->Once)
	{
		State->UsedResponses.AddUnique(Response->Id);
	}
	if (CaseState)
	{
		for (const FName Flag : Response->SetFlags)
		{
			CaseState->SetFlag(Flag);
		}
	}
	if (CaseFile && Response->AssertsFacts.Num() > 0)
	{
		CaseFile->RecordStatement(Character->Id, Response->Id);
	}
	if (Response->EndsCooperation)
	{
		State->bRefusesInterview = true;
		Reply.bInterviewEnded = true;
		EndInterview();
	}

	return Reply;
}

const FInterviewResponse* UInterviewSubsystem::SelectResponse(const FCaseCharacter& Character, const FCharacterRuntimeState& State,
	FName Trigger, EInterviewApproach Approach) const
{
	const FInterviewResponse* Best = nullptr;
	int32 BestPriority = 0;
	int32 BestSpecificity = 0;

	for (const FInterviewResponse& Response : Character.Responses)
	{
		if (Response.Trigger != Trigger)
		{
			continue;
		}

		// Count how many conditions this response sets, so the most specific match can win ties.
		int32 Specificity = 0;

		if (Response.Approach != EResponseApproach::Any)
		{
			const bool bApproachMatches =
				(Response.Approach == EResponseApproach::Neutral && Approach == EInterviewApproach::Neutral) ||
				(Response.Approach == EResponseApproach::Rapport && Approach == EInterviewApproach::Rapport) ||
				(Response.Approach == EResponseApproach::Challenge && Approach == EInterviewApproach::Challenge);
			if (!bApproachMatches)
			{
				continue;
			}
			++Specificity;
		}
		if (Response.MinTrust > 0)
		{
			if (State.Trust < Response.MinTrust)
			{
				continue;
			}
			++Specificity;
		}
		if (Response.MaxComposure < 100)
		{
			if (State.Composure > Response.MaxComposure)
			{
				continue;
			}
			++Specificity;
		}
		if (Response.StoryStep >= 0)
		{
			if (State.StoryStep != Response.StoryStep)
			{
				continue;
			}
			++Specificity;
		}
		if (Response.RequiresFlags.Num() > 0)
		{
			if (!CaseState || !CaseState->HasAllFlags(Response.RequiresFlags))
			{
				continue;
			}
			Specificity += Response.RequiresFlags.Num();
		}

		bool bEvidenceOk = true;
		for (const FName EvidenceId : Response.RequiresHeld)
		{
			bEvidenceOk &= CaseFile && CaseFile->HasEvidence(EvidenceId);
		}
		for (const FName EvidenceId : Response.RequiresNotHeld)
		{
			bEvidenceOk &= CaseFile && !CaseFile->HasEvidence(EvidenceId);
		}
		if (!bEvidenceOk)
		{
			continue;
		}
		Specificity += Response.RequiresHeld.Num() + Response.RequiresNotHeld.Num();

		if (Response.Once && State.UsedResponses.Contains(Response.Id))
		{
			continue;
		}

		// Highest priority wins; then most specific; then the one written first.
		const bool bBetter = !Best
			|| Response.Priority > BestPriority
			|| (Response.Priority == BestPriority && Specificity > BestSpecificity);
		if (bBetter)
		{
			Best = &Response;
			BestPriority = Response.Priority;
			BestSpecificity = Specificity;
		}
	}

	return Best;
}

// ---------------------------------------------------------------------------------------------
// Console commands for testing interviews before the screens exist.
// ---------------------------------------------------------------------------------------------

namespace InterviewConsole
{
	static UInterviewSubsystem* GetSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UInterviewSubsystem>() : nullptr;
	}

	static const FCaseContent* GetCase(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UInvestigationSubsystem* Investigation = GameInstance ? GameInstance->GetSubsystem<UInvestigationSubsystem>() : nullptr;
		return (Investigation && Investigation->IsCaseActive()) ? &Investigation->GetActiveCase() : nullptr;
	}

	/** Reads an optional approach word: "rapport" or "challenge". Anything else is neutral. */
	static EInterviewApproach ParseApproach(const TArray<FString>& Args, int32 Index)
	{
		if (Args.IsValidIndex(Index))
		{
			if (Args[Index].Equals(TEXT("rapport"), ESearchCase::IgnoreCase))
			{
				return EInterviewApproach::Rapport;
			}
			if (Args[Index].Equals(TEXT("challenge"), ESearchCase::IgnoreCase))
			{
				return EInterviewApproach::Challenge;
			}
		}
		return EInterviewApproach::Neutral;
	}

	static void LogReply(UWorld* World, const UInterviewSubsystem& Interviews, FName SpeakerId, const FInterviewReply& Reply)
	{
		if (!Reply.bValid)
		{
			UE_LOG(LogInvestigation, Display, TEXT("Not possible. Is an interview open, and is that a known topic or an exhibit you hold?"));
			return;
		}
		const FCaseContent* Case = GetCase(World);
		const FCaseCharacter* Speaker = Case ? Case->FindCharacter(SpeakerId) : nullptr;
		UE_LOG(LogInvestigation, Display, TEXT("%s: %s"), Speaker ? *Speaker->Name.ToString() : TEXT("?"), *Reply.Line.ToString());
		if (Reply.bInterviewEnded)
		{
			UE_LOG(LogInvestigation, Display, TEXT("(The interview is over.)"));
		}
	}

	static FAutoConsoleCommandWithWorld TopicsCommand(
		TEXT("ColdCase.Topics"),
		TEXT("Lists who can be interviewed and what they can be asked about."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const FCaseContent* Case = GetCase(World);
			if (!Case)
			{
				return;
			}
			UE_LOG(LogInvestigation, Display, TEXT("-- People you can interview --"));
			for (const FCaseCharacter& Character : Case->Characters)
			{
				if (Character.CanBeInterviewed())
				{
					UE_LOG(LogInvestigation, Display, TEXT("%s  id=%s"), *Character.Name.ToString(), *Character.Id.ToString());
				}
			}
			UE_LOG(LogInvestigation, Display, TEXT("-- Topics --"));
			for (const FInterviewTopic& Topic : Case->Topics)
			{
				UE_LOG(LogInvestigation, Display, TEXT("%s  id=%s"), *Topic.Text.ToString(), *Topic.Id.ToString());
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs InterviewCommand(
		TEXT("ColdCase.Interview"),
		TEXT("ColdCase.Interview <CharacterId>: starts an interview."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UInterviewSubsystem* Interviews = GetSubsystem(World);
			if (!Interviews || Args.Num() != 1)
			{
				return;
			}
			FText Refusal;
			if (Interviews->StartInterview(FName(*Args[0]), Refusal))
			{
				UE_LOG(LogInvestigation, Display, TEXT("Interview started."));
			}
			else if (!Refusal.IsEmpty())
			{
				UE_LOG(LogInvestigation, Display, TEXT("%s"), *Refusal.ToString());
			}
			else
			{
				UE_LOG(LogInvestigation, Display, TEXT("Can't start that interview."));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs AskCommand(
		TEXT("ColdCase.Ask"),
		TEXT("ColdCase.Ask <TopicId> [rapport|challenge]: asks the interviewee about a topic."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UInterviewSubsystem* Interviews = GetSubsystem(World);
			if (Interviews && Args.Num() >= 1)
			{
				const FName Speaker = Interviews->GetCurrentCharacterId();
				LogReply(World, *Interviews, Speaker, Interviews->AskAbout(FName(*Args[0]), ParseApproach(Args, 1)));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs PresentCommand(
		TEXT("ColdCase.Present"),
		TEXT("ColdCase.Present <EvidenceId> [rapport|challenge]: shows the interviewee an exhibit."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UInterviewSubsystem* Interviews = GetSubsystem(World);
			if (Interviews && Args.Num() >= 1)
			{
				const FName Speaker = Interviews->GetCurrentCharacterId();
				LogReply(World, *Interviews, Speaker, Interviews->PresentEvidence(FName(*Args[0]), ParseApproach(Args, 1)));
			}
		}));

	static FAutoConsoleCommandWithWorld EndCommand(
		TEXT("ColdCase.EndInterview"),
		TEXT("Ends the current interview."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UInterviewSubsystem* Interviews = GetSubsystem(World);
			if (Interviews && Interviews->IsInterviewActive())
			{
				Interviews->EndInterview();
				UE_LOG(LogInvestigation, Display, TEXT("Interview ended."));
			}
		}));

#if !UE_BUILD_SHIPPING
	static FAutoConsoleCommandWithWorld StateCommand(
		TEXT("ColdCase.Debug.People"),
		TEXT("Development only. Shows each person's hidden trust, composure and story step."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const UInterviewSubsystem* Interviews = GetSubsystem(World);
			const FCaseContent* Case = GetCase(World);
			if (!Interviews || !Case)
			{
				return;
			}
			for (const FCaseCharacter& Character : Case->Characters)
			{
				if (const FCharacterRuntimeState* State = Interviews->GetCharacterState(Character.Id))
				{
					UE_LOG(LogInvestigation, Display, TEXT("[debug] %s: trust %d, composure %d, story step %d%s"), *Character.Name.ToString(),
						State->Trust, State->Composure, State->StoryStep, State->bRefusesInterview ? TEXT(", refuses interviews") : TEXT(""));
				}
			}
		}));
#endif
}

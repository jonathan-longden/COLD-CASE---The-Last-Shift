#include "InvestigationSubsystem.h"

#include "CaseDataLoader.h"
#include "CaseTruthTypes.h"
#include "CaseValidator.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InvestigationLog.h"

bool UInvestigationSubsystem::StartCase(FName CasePluginName)
{
	EndCase();

	FCaseContent Loaded;
	FString Error;
	if (!FCaseDataLoader::LoadContent(CasePluginName, Loaded, Error))
	{
		UE_LOG(LogInvestigation, Error, TEXT("StartCase failed: %s"), *Error);
		return false;
	}

	const TArray<FString> Problems = FCaseValidator::Validate(Loaded);
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogInvestigation, Warning, TEXT("[%s] %s"), *Loaded.CaseId.ToString(), *Problem);
	}

	ActiveCase = MoveTemp(Loaded);
	ActiveCasePlugin = CasePluginName;
	bConcluded = false;

	UE_LOG(LogInvestigation, Log, TEXT("Case '%s' started from plugin '%s' (%d questions, %d people, %d exhibits, %d content warnings)."),
		*ActiveCase.CaseId.ToString(), *CasePluginName.ToString(),
		ActiveCase.Questions.Num(), ActiveCase.Characters.Num(), ActiveCase.Evidence.Num(), Problems.Num());

	OnCaseStarted.Broadcast(ActiveCase);
	return true;
}

void UInvestigationSubsystem::EndCase()
{
	if (!IsCaseActive())
	{
		return;
	}
	UE_LOG(LogInvestigation, Log, TEXT("Case '%s' ended."), *ActiveCase.CaseId.ToString());
	ActiveCase = FCaseContent();
	ActiveCasePlugin = NAME_None;
	bConcluded = false;
}

void UInvestigationSubsystem::ConcludeCase()
{
	if (IsCaseActive() && !bConcluded)
	{
		bConcluded = true;
		UE_LOG(LogInvestigation, Log, TEXT("Case '%s' concluded."), *ActiveCase.CaseId.ToString());
		OnCaseConcluded.Broadcast();
	}
}

void UInvestigationSubsystem::LogActiveCaseSummary() const
{
	if (!IsCaseActive())
	{
		UE_LOG(LogInvestigation, Display, TEXT("No case is active."));
		return;
	}

	UE_LOG(LogInvestigation, Display, TEXT("=== %s (%s) ==="), *ActiveCase.Title.ToString(), *ActiveCase.CaseId.ToString());
	UE_LOG(LogInvestigation, Display, TEXT("%s"), *ActiveCase.Briefing.ToString());

	UE_LOG(LogInvestigation, Display, TEXT("-- Questions --"));
	for (const FCaseQuestion& Question : ActiveCase.Questions)
	{
		const bool bKey = ActiveCase.KeyQuestions.Contains(Question.Id);
		UE_LOG(LogInvestigation, Display, TEXT("%s%s"), bKey ? TEXT("[KEY] ") : TEXT(""), *Question.Text.ToString());
		for (const FCaseFact& Fact : Question.Facts)
		{
			UE_LOG(LogInvestigation, Display, TEXT("    - %s"), *Fact.Text.ToString());
		}
	}

	UE_LOG(LogInvestigation, Display, TEXT("-- People --"));
	for (const FCaseCharacter& Character : ActiveCase.Characters)
	{
		UE_LOG(LogInvestigation, Display, TEXT("%s: %s"), *Character.Name.ToString(), *Character.Role.ToString());
	}

	UE_LOG(LogInvestigation, Display, TEXT("-- Exhibits --"));
	for (const FEvidenceItem& Item : ActiveCase.Evidence)
	{
		UE_LOG(LogInvestigation, Display, TEXT("%s  %s%s"), *Item.ExhibitRef, *Item.Name.ToString(),
			Item.Availability == EEvidenceAvailability::OnRequest ? TEXT("  (on request)") : TEXT(""));
	}
}

// ---------------------------------------------------------------------------------------------
// Console commands. Open the console in play with the ` key (left of 1) and type the command.
// ---------------------------------------------------------------------------------------------

namespace InvestigationConsole
{
	static UInvestigationSubsystem* GetSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UInvestigationSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorld DumpCaseCommand(
		TEXT("ColdCase.DumpCase"),
		TEXT("Prints the active case's questions, people and exhibits to the Output Log."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UInvestigationSubsystem* Investigation = GetSubsystem(World))
			{
				Investigation->LogActiveCaseSummary();
			}
		}));

#if !UE_BUILD_SHIPPING
	// Development only: checks the case against its truth file. Never available in a shipped game.
	static FAutoConsoleCommandWithWorld ValidateCaseCommand(
		TEXT("ColdCase.ValidateCase"),
		TEXT("Development only. Checks the active case's content and truth files for mistakes."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const UInvestigationSubsystem* Investigation = GetSubsystem(World);
			if (!Investigation || !Investigation->IsCaseActive())
			{
				UE_LOG(LogInvestigation, Display, TEXT("No case is active."));
				return;
			}

			FCaseTruth Truth;
			FString Error;
			const bool bHasTruth = FCaseDataLoader::LoadTruth(Investigation->GetActiveCasePlugin(), Truth, Error);
			if (!bHasTruth)
			{
				UE_LOG(LogInvestigation, Warning, TEXT("Truth file not checked: %s"), *Error);
			}

			TArray<FString> Problems = FCaseValidator::Validate(Investigation->GetActiveCase(), bHasTruth ? &Truth : nullptr);

			// Clips are files on disk, so check each one is really there.
			TArray<FCaseMediaClip> Clips;
			if (!Investigation->GetActiveCase().OpeningFilm.File.IsEmpty())
			{
				Clips.Add(Investigation->GetActiveCase().OpeningFilm);
			}
			for (const FEvidenceItem& Item : Investigation->GetActiveCase().Evidence)
			{
				Clips.Append(Item.Media);
			}
			for (const FCaseMediaClip& Clip : Clips)
			{
				const FString Path = FCaseDataLoader::GetMediaPath(Investigation->GetActiveCasePlugin(), Clip.File);
				if (!Clip.File.IsEmpty() && !FPaths::FileExists(Path))
				{
					Problems.Add(FString::Printf(TEXT("Media clip '%s': file not found at '%s'."), *Clip.Id.ToString(), *Path));
				}
			}
			for (const FString& Problem : Problems)
			{
				UE_LOG(LogInvestigation, Warning, TEXT("%s"), *Problem);
			}
			UE_LOG(LogInvestigation, Display, TEXT("Validation finished: %d problem(s)."), Problems.Num());
		}));
#endif
}

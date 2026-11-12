#include "InvestigationSaveSubsystem.h"

#include "CaseDataTypes.h"
#include "CaseFileSubsystem.h"
#include "CaseStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InterviewSubsystem.h"
#include "InvestigationLog.h"
#include "InvestigationSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "TheorySubsystem.h"

void UInvestigationSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Investigation = Collection.InitializeDependency<UInvestigationSubsystem>();
	CaseFile = Collection.InitializeDependency<UCaseFileSubsystem>();
	CaseState = Collection.InitializeDependency<UCaseStateSubsystem>();
	Interviews = Collection.InitializeDependency<UInterviewSubsystem>();
	Theory = Collection.InitializeDependency<UTheorySubsystem>();

	if (Interviews)
	{
		InterviewEndedHandle = Interviews->OnInterviewEnded.AddUObject(this, &UInvestigationSaveSubsystem::HandleInterviewEnded);
	}
	if (Investigation)
	{
		CaseConcludedHandle = Investigation->OnCaseConcluded.AddUObject(this, &UInvestigationSaveSubsystem::HandleCaseConcluded);
	}
}

void UInvestigationSaveSubsystem::Deinitialize()
{
	if (Interviews)
	{
		Interviews->OnInterviewEnded.Remove(InterviewEndedHandle);
	}
	if (Investigation)
	{
		Investigation->OnCaseConcluded.Remove(CaseConcludedHandle);
	}
	Super::Deinitialize();
}

FString UInvestigationSaveSubsystem::ManualSlotName(int32 SlotNumber)
{
	return FString::Printf(TEXT("ColdCase_Manual%d"), SlotNumber);
}

FString UInvestigationSaveSubsystem::AutosaveSlotName()
{
	return TEXT("ColdCase_Auto");
}

bool UInvestigationSaveSubsystem::CanSaveNow() const
{
	// Never mid-interview, so a save never has to describe a half-finished conversation.
	return Investigation && CaseFile && CaseState && Interviews && Theory
		&& Investigation->IsCaseActive() && !Interviews->IsInterviewActive() && !bLoading;
}

UInvestigationSaveGame* UInvestigationSaveSubsystem::CaptureSave(bool bAutosave) const
{
	if (!CanSaveNow())
	{
		return nullptr;
	}

	UInvestigationSaveGame* Save = NewObject<UInvestigationSaveGame>();
	const FCaseContent& Case = Investigation->GetActiveCase();

	Save->bConcluded = Investigation->IsCaseConcluded();
	Save->CaseFile = CaseFile->GetSaveData();
	Save->Flags = CaseState->GetFlags();
	Save->Characters = Interviews->GetCharacterStates();
	Save->Answers = Theory->GetAnswers();

	int32 Examined = 0;
	for (const FHeldEvidence& Held : Save->CaseFile.HeldEvidence)
	{
		Examined += Held.bExamined ? 1 : 0;
	}

	Save->Header.SchemaVersion = GInvestigationSaveSchemaVersion;
	Save->Header.bAutosave = bAutosave;
	Save->Header.SavedAt = FDateTime::Now();
	Save->Header.CaseId = Case.CaseId;
	Save->Header.CasePlugin = Investigation->GetActiveCasePlugin();
	Save->Header.CaseTitle = Case.Title;
	Save->Header.Summary = FString::Printf(TEXT("%d of %d exhibits examined, %d statements taken%s"),
		Examined, Save->CaseFile.HeldEvidence.Num(), Save->CaseFile.Statements.Num(),
		Save->bConcluded ? TEXT(", concluded") : TEXT(""));
	return Save;
}

bool UInvestigationSaveSubsystem::SaveManual(int32 SlotNumber)
{
	if (SlotNumber < 1 || SlotNumber > NumManualSlots)
	{
		return false;
	}
	UInvestigationSaveGame* Save = CaptureSave(false);
	if (!Save)
	{
		return false;
	}

	// A manual save is written straight away so the player knows it is safely on disk when told so.
	const FString SlotName = ManualSlotName(SlotNumber);
	if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))
	{
		UE_LOG(LogInvestigation, Error, TEXT("Saving to '%s' failed."), *SlotName);
		return false;
	}
	HeaderCache.Add(SlotName, Save->Header);
	UE_LOG(LogInvestigation, Log, TEXT("Saved to '%s' (%s)."), *SlotName, *Save->Header.Summary);
	return true;
}

void UInvestigationSaveSubsystem::Autosave()
{
	UInvestigationSaveGame* Save = CaptureSave(true);
	if (!Save)
	{
		return;
	}

	// Written in the background so play never pauses for it. Consoles require this.
	const FString SlotName = AutosaveSlotName();
	HeaderCache.Add(SlotName, Save->Header);
	UGameplayStatics::AsyncSaveGameToSlot(Save, SlotName, 0, FAsyncSaveGameToSlotDelegate::CreateLambda(
		[](const FString& SavedSlotName, const int32 UserIndex, bool bSuccess)
		{
			if (bSuccess)
			{
				UE_LOG(LogInvestigation, Log, TEXT("Autosaved."));
			}
			else
			{
				UE_LOG(LogInvestigation, Error, TEXT("Autosave to '%s' failed."), *SavedSlotName);
			}
		}));
}

void UInvestigationSaveSubsystem::HandleInterviewEnded()
{
	Autosave();
}

void UInvestigationSaveSubsystem::HandleCaseConcluded()
{
	Autosave();
}

bool UInvestigationSaveSubsystem::GetSlotHeader(const FString& SlotName, FSaveSlotHeader& OutHeader)
{
	if (const FSaveSlotHeader* Cached = HeaderCache.Find(SlotName))
	{
		OutHeader = *Cached;
		return true;
	}
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		return false;
	}
	const UInvestigationSaveGame* Save = Cast<UInvestigationSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save)
	{
		return false;
	}
	OutHeader = Save->Header;
	HeaderCache.Add(SlotName, Save->Header);
	return true;
}

bool UInvestigationSaveSubsystem::GetManualSlotHeader(int32 SlotNumber, FSaveSlotHeader& OutHeader)
{
	return GetSlotHeader(ManualSlotName(SlotNumber), OutHeader);
}

bool UInvestigationSaveSubsystem::GetAutosaveHeader(FSaveSlotHeader& OutHeader)
{
	return GetSlotHeader(AutosaveSlotName(), OutHeader);
}

bool UInvestigationSaveSubsystem::LoadManual(int32 SlotNumber)
{
	return SlotNumber >= 1 && SlotNumber <= NumManualSlots && LoadSlot(ManualSlotName(SlotNumber));
}

bool UInvestigationSaveSubsystem::LoadAutosave()
{
	return LoadSlot(AutosaveSlotName());
}

bool UInvestigationSaveSubsystem::LoadSlot(const FString& SlotName)
{
	if (!Investigation || !CaseFile || !CaseState || !Interviews || !Theory)
	{
		return false;
	}
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		return false;
	}

	const UInvestigationSaveGame* Save = Cast<UInvestigationSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save)
	{
		UE_LOG(LogInvestigation, Error, TEXT("'%s' could not be read."), *SlotName);
		return false;
	}
	if (Save->Header.SchemaVersion != GInvestigationSaveSchemaVersion)
	{
		UE_LOG(LogInvestigation, Error, TEXT("'%s' is save version %d; this build reads version %d."),
			*SlotName, Save->Header.SchemaVersion, GInvestigationSaveSchemaVersion);
		return false;
	}

	TGuardValue<bool> LoadingGuard(bLoading, true);

	// Close anything in progress, start the case fresh from its content, then lay the saved changes over it.
	Interviews->EndInterview();
	if (!Investigation->StartCase(Save->Header.CasePlugin))
	{
		return false;
	}
	CaseFile->RestoreSaveData(Save->CaseFile);
	CaseState->RestoreFlags(Save->Flags);
	Interviews->RestoreCharacterStates(Save->Characters);
	Theory->RestoreAnswers(Save->Answers);
	Investigation->RestoreConcluded(Save->bConcluded);

	UE_LOG(LogInvestigation, Log, TEXT("Loaded '%s' (%s)."), *SlotName, *Save->Header.Summary);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Console commands for testing saves.
// ---------------------------------------------------------------------------------------------

namespace InvestigationSaveConsole
{
	static UInvestigationSaveSubsystem* GetSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UInvestigationSaveSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs SaveCommand(
		TEXT("ColdCase.Save"),
		TEXT("ColdCase.Save <1-3>: saves into a manual slot."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UInvestigationSaveSubsystem* Saves = GetSubsystem(World);
			if (Saves && Args.Num() == 1)
			{
				UE_LOG(LogInvestigation, Display, TEXT("%s"), Saves->SaveManual(FCString::Atoi(*Args[0])) ? TEXT("Saved.") : TEXT("Could not save (bad slot, or an interview is in progress)."));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs LoadCommand(
		TEXT("ColdCase.Load"),
		TEXT("ColdCase.Load <1-3|auto>: loads a manual slot or the autosave."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UInvestigationSaveSubsystem* Saves = GetSubsystem(World);
			if (Saves && Args.Num() == 1)
			{
				const bool bLoaded = Args[0].Equals(TEXT("auto"), ESearchCase::IgnoreCase) ? Saves->LoadAutosave() : Saves->LoadManual(FCString::Atoi(*Args[0]));
				UE_LOG(LogInvestigation, Display, TEXT("%s"), bLoaded ? TEXT("Loaded.") : TEXT("Nothing to load in that slot."));
			}
		}));
}

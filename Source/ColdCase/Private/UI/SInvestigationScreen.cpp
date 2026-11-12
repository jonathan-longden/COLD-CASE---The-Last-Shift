#include "UI/SInvestigationScreen.h"

#include "CaseFileSubsystem.h"
#include "CaseResolutionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "InterviewSubsystem.h"
#include "InvestigationSaveSubsystem.h"
#include "InvestigationSubsystem.h"
#include "Styling/CoreStyle.h"
#include "TheorySubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace InvestigationScreenPrivate
{
	const FLinearColor ButtonNormal(0.10f, 0.12f, 0.15f, 1.f);
	const FLinearColor ButtonFocused(0.20f, 0.42f, 0.75f, 1.f);
	const FLinearColor TextMain(0.89f, 0.91f, 0.93f, 1.f);
	const FLinearColor TextDim(0.55f, 0.60f, 0.66f, 1.f);

	/** A list button that lights up when it has focus, so a controller user can always see where they are. */
	class SMenuButton : public SButton
	{
	public:
		SLATE_BEGIN_ARGS(SMenuButton) {}
			SLATE_ARGUMENT(FText, Label)
			SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			SButton::Construct(SButton::FArguments()
				.OnClicked(InArgs._OnClicked)
				.ContentPadding(FMargin(14.f, 10.f))
				.ButtonColorAndOpacity_Lambda([this]()
				{
					return FSlateColor((HasKeyboardFocus() || IsHovered()) ? ButtonFocused : ButtonNormal);
				})
				[
					SNew(STextBlock)
					.Text(InArgs._Label)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
					.ColorAndOpacity(FSlateColor(TextMain))
					.AutoWrapText(true)
				]);
		}
	};

	FString ApproachName(EInterviewApproach Approach)
	{
		switch (Approach)
		{
		case EInterviewApproach::Rapport: return TEXT("Build rapport");
		case EInterviewApproach::Challenge: return TEXT("Challenge");
		default: return TEXT("Neutral");
		}
	}
}

void SInvestigationScreen::Construct(const FArguments& InArgs)
{
	GameInstance = InArgs._GameInstance;
	if (UGameInstance* Instance = GameInstance.Get())
	{
		Investigation = Instance->GetSubsystem<UInvestigationSubsystem>();
		CaseFile = Instance->GetSubsystem<UCaseFileSubsystem>();
		Interviews = Instance->GetSubsystem<UInterviewSubsystem>();
		Theory = Instance->GetSubsystem<UTheorySubsystem>();
		Resolution = Instance->GetSubsystem<UCaseResolutionSubsystem>();
		Saves = Instance->GetSubsystem<UInvestigationSaveSubsystem>();
	}
	bRebuildPending = true;
}

const FCaseContent* SInvestigationScreen::GetCase() const
{
	const UInvestigationSubsystem* Subsystem = Investigation.Get();
	return (Subsystem && Subsystem->IsCaseActive()) ? &Subsystem->GetActiveCase() : nullptr;
}

void SInvestigationScreen::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Rebuilding is deferred to here so a button is never destroyed in the middle of its own click.
	if (bRebuildPending)
	{
		bRebuildPending = false;
		BuildPage();
		RebuildWidgets();
	}
}

FReply SInvestigationScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		GoBack();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SInvestigationScreen::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Clicking empty space would otherwise leave keyboard and gamepad with nothing selected.
	if (const TSharedPtr<SWidget> FirstButton = FirstButtonWeak.Pin())
	{
		return FReply::Handled().SetUserFocus(FirstButton.ToSharedRef(), EFocusCause::Mouse);
	}
	return FReply::Handled();
}

void SInvestigationScreen::GoTo(EPage NewPage)
{
	Page = NewPage;
	bRebuildPending = true;
}

void SInvestigationScreen::GoBack()
{
	switch (Page)
	{
	case EPage::Home:
		break;
	case EPage::Exhibit:
		GoTo(EPage::Exhibits);
		break;
	case EPage::Interview:
		// Leaving the room ends the interview.
		if (UInterviewSubsystem* Subsystem = Interviews.Get())
		{
			Subsystem->EndInterview();
		}
		GoTo(EPage::People);
		break;
	case EPage::PresentExhibit:
		GoTo(EPage::Interview);
		break;
	case EPage::ChooseAnswer:
	case EPage::Review:
	case EPage::ConfirmConclusion:
		GoTo(EPage::Theory);
		break;
	default:
		GoTo(EPage::Home);
		break;
	}
}

void SInvestigationScreen::AddItem(const FText& Label, TFunction<void()> Action)
{
	FMenuItem& Item = Items.AddDefaulted_GetRef();
	Item.Label = Label;
	Item.Action = MoveTemp(Action);
}

void SInvestigationScreen::AddBackItem()
{
	AddItem(FText::FromString(TEXT("Back")), [this]() { GoBack(); });
}

void SInvestigationScreen::BuildPage()
{
	Title = FText::GetEmpty();
	Body.Reset();
	Items.Reset();

	if (!GetCase() || !CaseFile.IsValid() || !Interviews.IsValid() || !Theory.IsValid() || !Resolution.IsValid() || !Saves.IsValid())
	{
		Title = FText::FromString(TEXT("COLD CASE"));
		Body = TEXT("No case is loaded. Check the Output Log (LogInvestigation) for the reason.");
		// Try again shortly in case the case is still starting.
		bRebuildPending = true;
		return;
	}

	switch (Page)
	{
	case EPage::Exhibits: BuildExhibits(); break;
	case EPage::Exhibit: BuildExhibit(); break;
	case EPage::Statements: BuildStatements(); break;
	case EPage::People: BuildPeople(); break;
	case EPage::Interview: BuildInterview(); break;
	case EPage::PresentExhibit: BuildPresentExhibit(); break;
	case EPage::Theory: BuildTheory(); break;
	case EPage::ChooseAnswer: BuildChooseAnswer(); break;
	case EPage::Review: BuildReview(); break;
	case EPage::ConfirmConclusion: BuildConfirmConclusion(); break;
	case EPage::CaseReview: BuildCaseReview(); break;
	case EPage::SaveLoad: BuildSaveLoad(); break;
	default: BuildHome(); break;
	}

	if (!Notice.IsEmpty())
	{
		Body = Notice + TEXT("\n\n") + Body;
		Notice.Reset();
	}
}

void SInvestigationScreen::BuildHome()
{
	const FCaseContent& Case = *GetCase();
	Title = Case.Title;
	Body = Case.Briefing.ToString();

	if (Investigation->IsCaseConcluded())
	{
		Body = TEXT("The investigation is concluded and the file is closed.");
		AddItem(FText::FromString(TEXT("Read the case review")), [this]() { GoTo(EPage::CaseReview); });
		AddItem(FText::FromString(TEXT("Look back through the exhibits")), [this]() { GoTo(EPage::Exhibits); });
		AddItem(FText::FromString(TEXT("Look back through the statements")), [this]() { GoTo(EPage::Statements); });
		AddItem(FText::FromString(TEXT("Save or load")), [this]() { GoTo(EPage::SaveLoad); });
		return;
	}

	AddItem(FText::FromString(TEXT("Exhibits")), [this]() { GoTo(EPage::Exhibits); });
	AddItem(FText::FromString(TEXT("Statements taken")), [this]() { GoTo(EPage::Statements); });
	AddItem(FText::FromString(TEXT("Interview someone")), [this]() { GoTo(EPage::People); });
	AddItem(FText::FromString(TEXT("Your theory")), [this]() { GoTo(EPage::Theory); });
	AddItem(FText::FromString(TEXT("Save or load")), [this]() { GoTo(EPage::SaveLoad); });
}

void SInvestigationScreen::BuildExhibits()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("Exhibits"));
	Body = TEXT("Everything in the file. Open an exhibit to examine it.");

	for (const FHeldEvidence& Held : CaseFile->GetHeldEvidence())
	{
		const FEvidenceItem* Item = Case.FindEvidence(Held.EvidenceId);
		if (!Item)
		{
			continue;
		}
		const FName EvidenceId = Item->Id;
		const FString Label = FString::Printf(TEXT("%s  %s%s"), *Item->ExhibitRef, *Item->Name.ToString(), Held.bExamined ? TEXT("") : TEXT("  (not yet examined)"));
		AddItem(FText::FromString(Label), [this, EvidenceId]()
		{
			SelectedEvidence = EvidenceId;
			GoTo(EPage::Exhibit);
		});
	}

	// Records that can be asked for. They arrive after the next interview.
	if (!Investigation->IsCaseConcluded())
	{
		for (const FEvidenceItem& Item : Case.Evidence)
		{
			if (Item.Availability != EEvidenceAvailability::OnRequest || CaseFile->HasEvidence(Item.Id))
			{
				continue;
			}
			if (CaseFile->GetPendingRequests().Contains(Item.Id))
			{
				Body += FString::Printf(TEXT("\n\nRequested and awaited: %s."), *Item.Name.ToString());
				continue;
			}
			const FName EvidenceId = Item.Id;
			AddItem(FText::FromString(FString::Printf(TEXT("Request: %s"), *Item.Name.ToString())), [this, EvidenceId]()
			{
				if (CaseFile.IsValid() && CaseFile->RequestEvidence(EvidenceId))
				{
					Notice = TEXT("Request sent. These things take time. It should be on your desk after your next interview.");
				}
				GoTo(EPage::Exhibits);
			});
		}
	}

	AddBackItem();
}

void SInvestigationScreen::BuildExhibit()
{
	const FCaseContent& Case = *GetCase();
	const FEvidenceItem* Item = Case.FindEvidence(SelectedEvidence);
	if (!Item || !CaseFile->HasEvidence(SelectedEvidence))
	{
		GoTo(EPage::Exhibits);
		return;
	}

	// Opening an exhibit is examining it.
	CaseFile->ExamineEvidence(SelectedEvidence);
	const bool bOpen = !Investigation->IsCaseConcluded();

	Title = FText::FromString(FString::Printf(TEXT("%s  %s"), *Item->ExhibitRef, *Item->Name.ToString()));
	Body = Item->Description.ToString();

	const FHeldEvidence* Held = CaseFile->GetHeldEvidence().FindByPredicate([this](const FHeldEvidence& H) { return H.EvidenceId == SelectedEvidence; });
	if (Held && Held->bExamined)
	{
		for (const FEvidenceFinding& Finding : Item->Findings)
		{
			Body += TEXT("\n\n") + Finding.Text.ToString();
			if (Finding.Interpretations.Num() == 0)
			{
				continue;
			}

			const FName FindingId = Finding.Id;
			const FName ChosenId = CaseFile->GetChosenInterpretation(FindingId);
			const FEvidenceInterpretation* Chosen = Finding.Interpretations.FindByPredicate([ChosenId](const FEvidenceInterpretation& I) { return I.Id == ChosenId; });
			Body += Chosen
				? FString::Printf(TEXT("\n\nYour reading: %s"), *Chosen->Text.ToString())
				: FString(TEXT("\n\nYou have not taken a view on what this shows."));

			if (!bOpen)
			{
				continue;
			}
			for (const FEvidenceInterpretation& Interpretation : Finding.Interpretations)
			{
				const FName InterpretationId = Interpretation.Id;
				AddItem(FText::FromString(FString::Printf(TEXT("Read it as: %s"), *Interpretation.Text.ToString())), [this, FindingId, InterpretationId]()
				{
					if (CaseFile.IsValid())
					{
						CaseFile->ChooseInterpretation(FindingId, InterpretationId);
					}
					GoTo(EPage::Exhibit);
				});
			}
			if (Chosen)
			{
				AddItem(FText::FromString(TEXT("Take no view for now")), [this, FindingId]()
				{
					if (CaseFile.IsValid())
					{
						CaseFile->ChooseInterpretation(FindingId, NAME_None);
					}
					GoTo(EPage::Exhibit);
				});
			}
		}
	}

	AddBackItem();
}

void SInvestigationScreen::BuildStatements()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("Statements taken"));

	for (const FRecordedStatement& Statement : CaseFile->GetStatements())
	{
		const FCaseCharacter* Speaker = Case.FindCharacter(Statement.SpeakerId);
		const FInterviewResponse* Response = Speaker ? Speaker->FindResponse(Statement.ResponseId) : nullptr;
		if (Response)
		{
			Body += FString::Printf(TEXT("%d. %s%s\n\"%s\"\n\n"), Statement.Number, *Speaker->Name.ToString(),
				Response->Certainty == EStatementCertainty::Unsure ? TEXT(" (not sure)") : TEXT(""), *Response->Line.ToString());
		}
	}
	if (Body.IsEmpty())
	{
		Body = TEXT("Nothing yet. What people tell you in interview is recorded here.");
	}

	AddBackItem();
}

void SInvestigationScreen::BuildPeople()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("Interview someone"));

	for (const FCaseCharacter& Character : Case.Characters)
	{
		Body += FString::Printf(TEXT("%s. %s\n"), *Character.Name.ToString(), *Character.Role.ToString());
		if (!Character.CanBeInterviewed() || Investigation->IsCaseConcluded())
		{
			continue;
		}
		const FName CharacterId = Character.Id;
		const FString Name = Character.Name.ToString();
		AddItem(FText::FromString(FString::Printf(TEXT("Interview %s"), *Name)), [this, CharacterId, Name]()
		{
			FText Refusal;
			if (Interviews.IsValid() && Interviews->StartInterview(CharacterId, Refusal))
			{
				Transcript.Reset();
				Approach = EInterviewApproach::Neutral;
				GoTo(EPage::Interview);
				return;
			}
			Notice = Refusal.IsEmpty() ? FString::Printf(TEXT("%s is not available."), *Name) : Refusal.ToString();
			GoTo(EPage::People);
		});
	}

	AddBackItem();
}

void SInvestigationScreen::BuildInterview()
{
	const FCaseContent& Case = *GetCase();
	const FCaseCharacter* Character = Case.FindCharacter(Interviews->GetCurrentCharacterId());

	// Newest exchange first, so the latest answer is always in view.
	for (int32 Index = Transcript.Num() - 1; Index >= 0; --Index)
	{
		Body += Transcript[Index] + TEXT("\n\n");
	}

	if (!Interviews->IsInterviewActive() || !Character)
	{
		Title = FText::FromString(TEXT("Interview over"));
		AddItem(FText::FromString(TEXT("Leave")), [this]() { GoTo(EPage::People); });
		return;
	}

	Title = FText::FromString(FString::Printf(TEXT("Interview: %s"), *Character->Name.ToString()));
	if (Transcript.Num() == 0)
	{
		Body = TEXT("Ask about anything, or show them anything from the file. How you put it matters.");
	}

	AddItem(FText::FromString(FString::Printf(TEXT("Approach: %s  (change)"), *InvestigationScreenPrivate::ApproachName(Approach))), [this]()
	{
		Approach = Approach == EInterviewApproach::Neutral ? EInterviewApproach::Rapport
			: Approach == EInterviewApproach::Rapport ? EInterviewApproach::Challenge
			: EInterviewApproach::Neutral;
		GoTo(EPage::Interview);
	});

	for (const FInterviewTopic& Topic : Case.Topics)
	{
		const FName TopicId = Topic.Id;
		AddItem(FText::FromString(FString::Printf(TEXT("Ask about: %s"), *Topic.Text.ToString())), [this, TopicId]() { AskOrPresent(TopicId, false); });
	}

	AddItem(FText::FromString(TEXT("Show an exhibit...")), [this]() { GoTo(EPage::PresentExhibit); });
	AddItem(FText::FromString(TEXT("End the interview")), [this]() { GoBack(); });
}

void SInvestigationScreen::BuildPresentExhibit()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("Show an exhibit"));
	Body = TEXT("Anything in the file can be put to anyone.");

	for (const FHeldEvidence& Held : CaseFile->GetHeldEvidence())
	{
		if (const FEvidenceItem* Item = Case.FindEvidence(Held.EvidenceId))
		{
			const FName EvidenceId = Item->Id;
			AddItem(FText::FromString(FString::Printf(TEXT("%s  %s"), *Item->ExhibitRef, *Item->Name.ToString())), [this, EvidenceId]() { AskOrPresent(EvidenceId, true); });
		}
	}

	AddBackItem();
}

void SInvestigationScreen::AskOrPresent(FName Id, bool bPresent)
{
	const FCaseContent* Case = GetCase();
	UInterviewSubsystem* Subsystem = Interviews.Get();
	const FCaseCharacter* Character = (Case && Subsystem) ? Case->FindCharacter(Subsystem->GetCurrentCharacterId()) : nullptr;
	if (!Character)
	{
		GoTo(EPage::People);
		return;
	}

	FString Subject;
	if (bPresent)
	{
		const FEvidenceItem* Item = Case->FindEvidence(Id);
		Subject = Item ? Item->Name.ToString() : Id.ToString();
	}
	else
	{
		const FInterviewTopic* Topic = Case->FindTopic(Id);
		Subject = Topic ? Topic->Text.ToString() : Id.ToString();
	}

	const TCHAR* Manner = Approach == EInterviewApproach::Rapport ? TEXT(", taking your time") : Approach == EInterviewApproach::Challenge ? TEXT(", pressing the point") : TEXT("");
	const FString Name = Character->Name.ToString();
	const FInterviewReply Reply = bPresent ? Subsystem->PresentEvidence(Id, Approach) : Subsystem->AskAbout(Id, Approach);

	if (Reply.bValid)
	{
		FString Exchange = FString::Printf(TEXT("You %s %s%s.\n%s: \"%s\""), bPresent ? TEXT("show them") : TEXT("ask about"), *Subject, Manner, *Name, *Reply.Line.ToString());
		if (Reply.bInterviewEnded)
		{
			Exchange += FString::Printf(TEXT("\n%s ends the interview."), *Name);
		}
		Transcript.Add(MoveTemp(Exchange));
	}
	GoTo(EPage::Interview);
}

void SInvestigationScreen::BuildTheory()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("Your theory"));
	Body = TEXT("Say what you think happened. You can change your answers until you submit. Your DCI will tell you how well the file supports them.");

	for (const FName QuestionId : Case.KeyQuestions)
	{
		const FCaseQuestion* Question = Case.FindQuestion(QuestionId);
		if (!Question)
		{
			continue;
		}
		const FCaseFact* Answer = Case.FindFact(Theory->GetAnswer(QuestionId));
		const FString Label = FString::Printf(TEXT("%s\n%s"), *Question->Text.ToString(), Answer ? *Answer->Text.ToString() : TEXT("No answer yet"));
		AddItem(FText::FromString(Label), [this, QuestionId]()
		{
			SelectedQuestion = QuestionId;
			GoTo(EPage::ChooseAnswer);
		});
	}

	AddItem(FText::FromString(TEXT("Take it to the DCI")), [this]() { GoTo(EPage::Review); });
	AddItem(FText::FromString(TEXT("Submit as your conclusion...")), [this]()
	{
		if (Theory.IsValid() && Theory->IsComplete())
		{
			GoTo(EPage::ConfirmConclusion);
			return;
		}
		Notice = TEXT("You need an answer to every question before you can submit.");
		GoTo(EPage::Theory);
	});
	AddBackItem();
}

void SInvestigationScreen::BuildChooseAnswer()
{
	const FCaseContent& Case = *GetCase();
	const FCaseQuestion* Question = Case.FindQuestion(SelectedQuestion);
	if (!Question)
	{
		GoTo(EPage::Theory);
		return;
	}

	Title = Question->Text;
	Body = TEXT("Choose the answer you are prepared to stand behind.");

	const FName QuestionId = Question->Id;
	for (const FCaseFact& Fact : Question->Facts)
	{
		const FName FactId = Fact.Id;
		AddItem(Fact.Text, [this, QuestionId, FactId]()
		{
			if (Theory.IsValid())
			{
				Theory->SetAnswer(QuestionId, FactId);
			}
			GoTo(EPage::Theory);
		});
	}
	AddItem(FText::FromString(TEXT("No answer yet")), [this, QuestionId]()
	{
		if (Theory.IsValid())
		{
			Theory->SetAnswer(QuestionId, NAME_None);
		}
		GoTo(EPage::Theory);
	});
	AddBackItem();
}

void SInvestigationScreen::BuildReview()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("DCI review"));

	for (const FAnswerAssessment& Assessment : Theory->AssessTheory())
	{
		const FCaseQuestion* Question = Case.FindQuestion(Assessment.QuestionId);
		const FCaseFact* Fact = Case.FindFact(Assessment.FactId);
		Body += FString::Printf(TEXT("%s\nYou say: %s\nDCI: \"%s\"\n\n"), Question ? *Question->Text.ToString() : TEXT("?"),
			Fact ? *Fact->Text.ToString() : TEXT("(no answer)"), *UTheorySubsystem::DescribeStrength(Assessment.Strength).ToString());
	}

	AddBackItem();
}

void SInvestigationScreen::BuildConfirmConclusion()
{
	Title = FText::FromString(TEXT("Submit your conclusion"));
	Body = TEXT("This closes the investigation. There will be no more interviews and the file cannot be changed. A case review will follow.");

	AddItem(FText::FromString(TEXT("Not yet")), [this]() { GoTo(EPage::Theory); });
	AddItem(FText::FromString(TEXT("Submit. This is final.")), [this]()
	{
		if (Resolution.IsValid() && Resolution->SubmitConclusion())
		{
			GoTo(EPage::CaseReview);
			return;
		}
		GoTo(EPage::Theory);
	});
}

void SInvestigationScreen::BuildCaseReview()
{
	Title = FText::FromString(TEXT("Case review"));
	Body = FString::Join(Resolution->GetReportLines(), TEXT("\n"));
	if (Body.IsEmpty())
	{
		Body = TEXT("There is no case review until the investigation has been concluded.");
	}
	AddBackItem();
}

void SInvestigationScreen::BuildSaveLoad()
{
	Title = FText::FromString(TEXT("Save or load"));
	Body = TEXT("The game also saves by itself after each interview and when you submit your conclusion.");

	auto Describe = [](const FSaveSlotHeader& Header)
	{
		return FString::Printf(TEXT("%s, %s\n%s"), *Header.CaseTitle.ToString(), *Header.SavedAt.ToString(TEXT("%d/%m/%Y %H:%M")), *Header.Summary);
	};

	for (int32 Slot = 1; Slot <= UInvestigationSaveSubsystem::NumManualSlots; ++Slot)
	{
		FSaveSlotHeader Header;
		const bool bUsed = Saves->GetManualSlotHeader(Slot, Header);

		AddItem(FText::FromString(FString::Printf(TEXT("Save in slot %d%s"), Slot, bUsed ? TEXT("  (replaces what is there)") : TEXT("  (empty)"))), [this, Slot]()
		{
			Notice = (Saves.IsValid() && Saves->SaveManual(Slot)) ? FString::Printf(TEXT("Saved in slot %d."), Slot) : FString(TEXT("The save failed."));
			GoTo(EPage::SaveLoad);
		});
		if (bUsed)
		{
			AddItem(FText::FromString(FString::Printf(TEXT("Load slot %d\n%s"), Slot, *Describe(Header))), [this, Slot]()
			{
				const bool bLoaded = Saves.IsValid() && Saves->LoadManual(Slot);
				Transcript.Reset();
				Notice = bLoaded ? FString::Printf(TEXT("Loaded slot %d."), Slot) : FString(TEXT("That save could not be loaded."));
				GoTo(bLoaded ? EPage::Home : EPage::SaveLoad);
			});
		}
	}

	FSaveSlotHeader AutoHeader;
	if (Saves->GetAutosaveHeader(AutoHeader))
	{
		AddItem(FText::FromString(FString::Printf(TEXT("Load the autosave\n%s"), *Describe(AutoHeader))), [this]()
		{
			const bool bLoaded = Saves.IsValid() && Saves->LoadAutosave();
			Transcript.Reset();
			Notice = bLoaded ? FString(TEXT("Loaded the autosave.")) : FString(TEXT("The autosave could not be loaded."));
			GoTo(bLoaded ? EPage::Home : EPage::SaveLoad);
		});
	}

	AddBackItem();
}

void SInvestigationScreen::RebuildWidgets()
{
	using namespace InvestigationScreenPrivate;

	const TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	TSharedPtr<SWidget> FirstButton;

	for (const FMenuItem& Item : Items)
	{
		const TFunction<void()> Action = Item.Action;
		const TSharedRef<SMenuButton> Button = SNew(SMenuButton)
			.Label(Item.Label)
			.OnClicked_Lambda([Action]()
			{
				if (Action)
				{
					Action();
				}
				return FReply::Handled();
			});

		Menu->AddSlot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 6.f)
			[
				Button
			];

		if (!FirstButton.IsValid())
		{
			FirstButton = Button;
		}
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&BackgroundBrush)
		.Padding(FMargin(48.f, 36.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 24.f)
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
				.ColorAndOpacity(FSlateColor(TextMain))
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.38f)
				[
					SNew(SScrollBox)
					.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
					+ SScrollBox::Slot()
					[
						Menu
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.62f)
				.Padding(32.f, 0.f, 0.f, 0.f)
				[
					SNew(SBorder)
					.BorderImage(&PanelBrush)
					.Padding(FMargin(24.f))
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(STextBlock)
							.Text(FText::FromString(Body))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
							.ColorAndOpacity(FSlateColor(TextMain))
							.AutoWrapText(true)
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 18.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Move: arrow keys or d-pad      Choose: Enter or A      Back: Esc or B")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				.ColorAndOpacity(FSlateColor(TextDim))
			]
		]
	];

	FirstButtonWeak = FirstButton;
	if (FirstButton.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocus(FirstButton, EFocusCause::SetDirectly);
	}
}

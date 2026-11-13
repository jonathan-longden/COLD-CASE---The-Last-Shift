#include "UI/SInvestigationScreen.h"

#include "CaseDataLoader.h"
#include "CaseFileSubsystem.h"
#include "CaseMediaPlayback.h"
#include "CaseResolutionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "InvestigationLog.h"
#include "InputCoreTypes.h"
#include "InterviewSubsystem.h"
#include "InvestigationSaveSubsystem.h"
#include "InvestigationSubsystem.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "TheorySubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace InvestigationScreenPrivate
{
	const FLinearColor ButtonNormal(0.f, 0.f, 0.f, 0.f);
	const FLinearColor ButtonFocused(0.78f, 0.52f, 0.22f, 0.22f);
	const FLinearColor Accent(0.78f, 0.52f, 0.22f, 1.f);
	const FLinearColor TextMain(0.89f, 0.90f, 0.91f, 1.f);
	const FLinearColor TextMenu(0.76f, 0.78f, 0.80f, 1.f);
	const FLinearColor TextDim(0.50f, 0.54f, 0.58f, 1.f);
	const FLinearColor Ink(0.09f, 0.085f, 0.08f, 1.f);
	const FLinearColor StampRed(0.62f, 0.10f, 0.08f, 0.9f);

	FSlateFontInfo TypedFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Mono", Size); }

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
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
				.OnClicked(InArgs._OnClicked)
				.ContentPadding(FMargin(0.f))
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor_Lambda([this]()
					{
						return FSlateColor(IsLit() ? ButtonFocused : ButtonNormal);
					})
					.Padding(FMargin(0.f))
					[
						SNew(SHorizontalBox)

						// A thin amber bar marks the chosen line, like a tab on a file.
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(3.f)
							[
								SNew(SImage)
								.Image(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
								.ColorAndOpacity_Lambda([this]()
								{
									return FSlateColor(IsLit() ? Accent : FLinearColor::Transparent);
								})
							]
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.Padding(FMargin(14.f, 10.f))
						[
							SNew(STextBlock)
							.Text(InArgs._Label)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
							.ColorAndOpacity_Lambda([this]()
							{
								return FSlateColor(IsLit() ? TextMain : TextMenu);
							})
							.AutoWrapText(true)
						]
					]
				]);
		}

		bool IsLit() const
		{
			return HasKeyboardFocus() || IsHovered();
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

namespace InvestigationScreenPrivate
{
	TWeakPtr<SInvestigationScreen> ActiveScreen;

	/** How long the opening film's buttons stay up after the film starts or the player touches a control. */
	constexpr double FilmMenuHoldSeconds = 3.0;
	constexpr double FilmMenuFadeSeconds = 0.6;
}

TSharedPtr<SInvestigationScreen> SInvestigationScreen::GetActive()
{
	return InvestigationScreenPrivate::ActiveScreen.Pin();
}

void SInvestigationScreen::Construct(const FArguments& InArgs)
{
	GameInstance = InArgs._GameInstance;
	InvestigationScreenPrivate::ActiveScreen = SharedThis(this);

	// UI pictures are plain image files in the project's Art/UI folder, copied beside the packaged game.
	const FString UiDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Art"), TEXT("UI")));
	const FString BackdropPath = FPaths::Combine(UiDir, TEXT("Backdrop.png"));
	const FString PaperPath = FPaths::Combine(UiDir, TEXT("Paper.png"));
	if (FPaths::FileExists(BackdropPath))
	{
		BackdropBrush = MakeShared<FSlateDynamicImageBrush>(FName(*BackdropPath), FVector2D(1920.f, 1080.f));
	}
	if (FPaths::FileExists(PaperPath))
	{
		PaperBrush = MakeShared<FSlateDynamicImageBrush>(FName(*PaperPath), FVector2D(256.f, 256.f));
		PaperBrush->DrawAs = ESlateBrushDrawType::Image;
		PaperBrush->Tiling = ESlateBrushTileType::Both;
	}
	MediaPlayback = InArgs._MediaPlayback;
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

	// When the opening film reaches its end, carry on to the case without waiting for a button.
	if (Page == EPage::Clip && ClipReturnPage == EPage::Home && MediaPlayback.IsValid() && MediaPlayback->HasFinished())
	{
		GoBack();
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

bool SInvestigationScreen::IsFullScreenFilm() const
{
	return Page == EPage::Clip && ClipReturnPage == EPage::Home && bClipOpened && CurrentClip.Kind == ECaseMediaKind::Video;
}

float SInvestigationScreen::GetFilmMenuOpacity() const
{
	using namespace InvestigationScreenPrivate;
	const double Since = FPlatformTime::Seconds() - FilmMenuShownAt;
	if (Since <= FilmMenuHoldSeconds)
	{
		return 1.f;
	}
	return static_cast<float>(FMath::Clamp(1.0 - (Since - FilmMenuHoldSeconds) / FilmMenuFadeSeconds, 0.0, 1.0));
}

FReply SInvestigationScreen::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsFullScreenFilm())
	{
		const bool bWasHidden = GetFilmMenuOpacity() < 0.05f;
		FilmMenuShownAt = FPlatformTime::Seconds();
		// While the buttons are hidden, the first press only brings them back, so a stray key never skips the film.
		// Esc / B still leave straight away.
		const FKey Key = InKeyEvent.GetKey();
		if (bWasHidden && Key != EKeys::Escape && Key != EKeys::Gamepad_FaceButton_Right)
		{
			return FReply::Handled();
		}
	}
	return SCompoundWidget::OnPreviewKeyDown(MyGeometry, InKeyEvent);
}

FReply SInvestigationScreen::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsFullScreenFilm() && !MouseEvent.GetCursorDelta().IsNearlyZero())
	{
		FilmMenuShownAt = FPlatformTime::Seconds();
	}
	return SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
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
	// Leaving a clip stops it.
	if (Page == EPage::Clip && NewPage != EPage::Clip && MediaPlayback.IsValid())
	{
		MediaPlayback->Stop();
	}
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
	case EPage::Clip:
		GoTo(ClipReturnPage);
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

	// The opening film plays once, the first time the case is on screen.
	if (!bOpeningFilmOffered)
	{
		bOpeningFilmOffered = true;
		const FCaseMediaClip& Opening = GetCase()->OpeningFilm;
		if (Page == EPage::Home && !Opening.File.IsEmpty() && !Investigation->IsCaseConcluded())
		{
			OpenClip(Opening, EPage::Home);
		}
	}

	switch (Page)
	{
	case EPage::Clip: BuildClip(); break;
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

	for (const FCaseMediaClip& Clip : Item->Media)
	{
		const FCaseMediaClip ClipCopy = Clip;
		const FText Label = Clip.Caption.IsEmpty() ? FText::FromString(TEXT("Play the recording")) : Clip.Caption;
		AddItem(Label, [this, ClipCopy]() { OpenClip(ClipCopy, EPage::Exhibit); });
	}

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
		Subject = FString::Printf(TEXT("exhibit %s, %s"), Item ? *Item->ExhibitRef : TEXT("?"), Item ? *Item->Name.ToString() : *Id.ToString());
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
		// "You ask about the night of 12 October 2000." / "You show Lorraine the exhibit: Till roll, ..."
		FString FirstName = Name;
		FirstName.Split(TEXT(" "), &FirstName, nullptr);
		if (!bPresent && Subject.StartsWith(TEXT("The ")))
		{
			Subject[0] = TEXT('t');
		}
		const FString Asked = bPresent
			? FString::Printf(TEXT("You show %s %s%s"), *FirstName, *Subject, Manner)
			: FString::Printf(TEXT("You ask about %s%s"), *Subject, Manner);
		FString Exchange = FString::Printf(TEXT("%s.\n%s: \"%s\""), *Asked, *Name, *Reply.Line.ToString());
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
	const FCaseReviewReport Report = Resolution.IsValid() ? Resolution->BuildReport() : FCaseReviewReport();
	const FCaseContent* Case = GetCase();
	if (!Report.bValid || !Case)
	{
		Body = TEXT("There is no case review until the investigation has been concluded.");
		AddBackItem();
		return;
	}

	// Written as a reviewing officer's memo: short paragraphs, no indents (wrapped lines would lose them).
	auto Sentence = [](FString Text)
	{
		Text.TrimStartAndEndInline();
		if (!Text.IsEmpty() && !FChar::IsPunct(Text[Text.Len() - 1]))
		{
			Text += TEXT(".");
		}
		return Text;
	};

	Body = FString::Printf(TEXT("REVIEW OF THE INVESTIGATION\n%s\n\n"), *Case->Title.ToString());
	Body += TEXT("WHAT THE INVESTIGATION CONCLUDED\n");
	int32 Number = 0;
	for (const FCaseReviewAnswer& Answer : Report.Answers)
	{
		const FCaseQuestion* Question = Case->FindQuestion(Answer.Assessment.QuestionId);
		const FCaseFact* PlayerFact = Case->FindFact(Answer.Assessment.FactId);
		const FCaseFact* TrueFact = Case->FindFact(Answer.TrueFactId);
		Body += FString::Printf(TEXT("\n%d. %s\n"), ++Number, Question ? *Question->Text.ToString() : TEXT("?"));
		Body += FString::Printf(TEXT("Concluded: %s\n"), *Sentence(PlayerFact ? PlayerFact->Text.ToString() : TEXT("no answer")));
		Body += FString::Printf(TEXT("Support: %s\n"), *Sentence(UTheorySubsystem::DescribeStrength(Answer.Assessment.Strength).ToString()));
		Body += Answer.bMatchesTruth
			? FString(TEXT("Finding: This is what happened.\n"))
			: FString::Printf(TEXT("Finding: This is not what happened. In fact: %s\n"), *Sentence(TrueFact ? TrueFact->Text.ToString() : TEXT("not recorded")));
	}

	Body += TEXT("\nLINES OF ENQUIRY NOT FOLLOWED\n");
	if (Report.Missed.Num() == 0)
	{
		Body += TEXT("None identified.\n");
	}
	for (const FText& Missed : Report.Missed)
	{
		Body += FString::Printf(TEXT("- %s\n"), *Missed.ToString());
	}

	Body += FString::Printf(TEXT("\nWHAT HAPPENED\n%s"), *Report.TruthSummary.ToString());
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

void SInvestigationScreen::OpenClip(const FCaseMediaClip& Clip, EPage ReturnTo)
{
	CurrentClip = Clip;
	ClipReturnPage = ReturnTo;
	bClipOpened = false;
	FilmMenuShownAt = FPlatformTime::Seconds();
	if (MediaPlayback.IsValid() && Investigation.IsValid())
	{
		bClipOpened = MediaPlayback->PlayFile(FCaseDataLoader::GetMediaPath(Investigation->GetActiveCasePlugin(), Clip.File));
	}
	GoTo(EPage::Clip);
}

void SInvestigationScreen::BuildClip()
{
	Title = CurrentClip.Caption.IsEmpty() ? FText::FromString(TEXT("Recording")) : CurrentClip.Caption;
	Body = CurrentClip.Transcript.ToString();
	if (!bClipOpened)
	{
		Body = TEXT("This recording could not be played.") + (Body.IsEmpty() ? FString() : TEXT("\n\n") + Body);
	}

	AddItem(FText::FromString(ClipReturnPage == EPage::Home ? TEXT("Continue") : TEXT("Back")), [this]() { GoBack(); });
	AddItem(FText::FromString(TEXT("Play again")), [this]()
	{
		const FCaseMediaClip Again = CurrentClip;
		OpenClip(Again, ClipReturnPage);
	});
}

void SInvestigationScreen::RebuildWidgets()
{
	using namespace InvestigationScreenPrivate;

	// The right-hand panel: the page's text typed on a sheet of paper, with the moving picture above it
	// when a video clip is on screen. Without the paper image it falls back to light text on dark.
	const bool bPaper = PaperBrush.IsValid();
	const FLinearColor BodyColour = bPaper ? Ink : TextMain;
	const TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);
	UMediaTexture* Picture = MediaPlayback.IsValid() ? MediaPlayback->GetPicture() : nullptr;
	if (Page == EPage::Clip && bClipOpened && CurrentClip.Kind == ECaseMediaKind::Video && Picture)
	{
		PictureBrush = FSlateBrush();
		PictureBrush.SetResourceObject(Picture);
		PictureBrush.ImageSize = FVector2D(1280.f, 720.f);
		PictureBrush.DrawAs = ESlateBrushDrawType::Image;

		Panel->AddSlot()
			.FillHeight(1.f)
			.Padding(0.f, 0.f, 0.f, 16.f)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(SImage)
					.Image(&PictureBrush)
				]
			];
		Panel->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Body))
				.Font(TypedFont(17))
				.ColorAndOpacity(FSlateColor(BodyColour))
				.AutoWrapText(true)
			];
	}
	else
	{
		Panel->AddSlot()
			.FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				.Padding(FMargin(0.f, 0.f, 18.f, 0.f))   // keeps wrapped text clear of the scroll bar
				[
					SNew(STextBlock)
					.Text(FText::FromString(Body))
					.Font(TypedFont(17))
					.LineHeightPercentage(1.15f)
					.ColorAndOpacity(FSlateColor(BodyColour))
					.AutoWrapText(true)
				]
			];
	}

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

	// The opening film fills the whole screen, with its choices tucked into a corner.
	if (Page == EPage::Clip && ClipReturnPage == EPage::Home && bClipOpened && CurrentClip.Kind == ECaseMediaKind::Video && Picture)
	{
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(&FilmBackgroundBrush)
			.Padding(FMargin(0.f))
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					[
						SNew(SImage)
						.Image(&PictureBrush)
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(24.f))
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
					.Padding(FMargin(0.f))
					.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, GetFilmMenuOpacity()); })
					[
						SNew(SBox)
						.WidthOverride(200.f)
						[
							Menu
						]
					]
				]
			]
		];

		FirstButtonWeak = FirstButton;
		if (FirstButton.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(FirstButton, EFocusCause::SetDirectly);
		}
		return;
	}

	const FSlateBrush* PanelImage = bPaper ? static_cast<const FSlateBrush*>(PaperBrush.Get()) : static_cast<const FSlateBrush*>(&PanelBrush);

	ChildSlot
	[
		SNew(SOverlay)

		// Background: the call centre at night, blurred and darkened.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(&BackgroundBrush)
			.Padding(FMargin(0.f))
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFill)
				[
					SNew(SImage)
					.Image(BackdropBrush.IsValid() ? static_cast<const FSlateBrush*>(BackdropBrush.Get()) : static_cast<const FSlateBrush*>(&BackgroundBrush))
				]
			]
		]

		+ SOverlay::Slot()
		.Padding(FMargin(56.f, 36.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("MAJOR CRIME REVIEW  /  CASE FILE")))
				.Font(TypedFont(15))
				.ColorAndOpacity(FSlateColor(Accent))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 10.f)
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
				.ColorAndOpacity(FSlateColor(TextMain))
				.ShadowOffset(FVector2D(1.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 22.f)
			[
				SNew(SBox)
				.HeightOverride(1.f)
				.WidthOverride(120.f)
				.HAlign(HAlign_Left)
				[
					SNew(SImage)
					.Image(&AccentBrush)
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.36f)
				[
					SNew(SBorder)
					.BorderImage(&ShadeBrush)
					.Padding(FMargin(10.f))
					[
						SNew(SScrollBox)
						.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
						+ SScrollBox::Slot()
						[
							Menu
						]
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.64f)
				.Padding(32.f, 0.f, 0.f, 0.f)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(PanelImage)
						.Padding(FMargin(40.f, 64.f, 40.f, 32.f))
						[
							Panel
						]
					]

					// A faint stamp in the corner of the sheet.
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Top)
					.Padding(FMargin(0.f, 14.f, 22.f, 0.f))
					[
						SNew(STextBlock)
						.Visibility(bPaper ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
						.Text(FText::FromString(TEXT("RESTRICTED")))
						.Font(TypedFont(20))
						.ColorAndOpacity(FSlateColor(StampRed))
						.RenderTransformPivot(FVector2D(0.5f, 0.5f))
						.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-6.f))))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 18.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("MOVE  arrows / d-pad      CHOOSE  Enter / A      BACK  Esc / B")))
				.Font(TypedFont(14))
				.ColorAndOpacity(FSlateColor(TextMenu))
			]
		]
	];

	FirstButtonWeak = FirstButton;
	if (FirstButton.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocus(FirstButton, EFocusCause::SetDirectly);
	}
}

bool SInvestigationScreen::DevCommand(const TArray<FString>& Args)
{
	const FCaseContent* Case = GetCase();
	if (Args.Num() < 1 || !Case || !CaseFile.IsValid())
	{
		return false;
	}
	const FString What = Args[0];
	const FName Id = Args.Num() > 1 ? FName(*Args[1]) : NAME_None;

	if (What == TEXT("Home")) { GoTo(EPage::Home); return true; }
	if (What == TEXT("Exhibits")) { GoTo(EPage::Exhibits); return true; }
	if (What == TEXT("Statements")) { GoTo(EPage::Statements); return true; }
	if (What == TEXT("People")) { GoTo(EPage::People); return true; }
	if (What == TEXT("Theory")) { GoTo(EPage::Theory); return true; }
	if (What == TEXT("SaveLoad")) { GoTo(EPage::SaveLoad); return true; }
	if (What == TEXT("CaseReview"))
	{
		if (!Investigation.IsValid() || !Investigation->IsCaseConcluded())
		{
			return false;
		}
		GoTo(EPage::CaseReview);
		return true;
	}
	if (What == TEXT("Exhibit"))
	{
		if (!Case->FindEvidence(Id) || !CaseFile->HasEvidence(Id))
		{
			return false;
		}
		SelectedEvidence = Id;
		GoTo(EPage::Exhibit);
		return true;
	}
	if (What == TEXT("Clip"))
	{
		if (Case->OpeningFilm.Id == Id)
		{
			OpenClip(Case->OpeningFilm, EPage::Home);
			return true;
		}
		for (const FEvidenceItem& Item : Case->Evidence)
		{
			for (const FCaseMediaClip& Clip : Item.Media)
			{
				if (Clip.Id == Id && CaseFile->HasEvidence(Item.Id))
				{
					SelectedEvidence = Item.Id;
					OpenClip(Clip, EPage::Exhibit);
					return true;
				}
			}
		}
		return false;
	}
	if (What == TEXT("Interview"))
	{
		FText Refusal;
		if (!Interviews.IsValid() || !Interviews->StartInterview(Id, Refusal))
		{
			return false;
		}
		Transcript.Reset();
		Approach = EInterviewApproach::Neutral;
		GoTo(EPage::Interview);
		return true;
	}
	if (What == TEXT("Ask") || What == TEXT("Present"))
	{
		if (Page != EPage::Interview || !Interviews.IsValid() || !Interviews->IsInterviewActive())
		{
			return false;
		}
		AskOrPresent(Id, What == TEXT("Present"));
		if (Transcript.Num() > 0)
		{
			UE_LOG(LogInvestigation, Display, TEXT("%s"), *Transcript.Last().Replace(TEXT("\n"), TEXT("  /  ")));   // one log line, so it can be searched
		}
		return true;
	}
	return false;
}

#if !UE_BUILD_SHIPPING
namespace InvestigationScreenPrivate
{
	// Development only: drives the screens without a keyboard or controller, for unattended screenshots.
	static FAutoConsoleCommand UiCommand(
		TEXT("ColdCase.UI"),
		TEXT("Development only. ColdCase.UI <Home|Exhibits|Exhibit|Statements|People|Interview|Theory|CaseReview|SaveLoad|Clip|Ask|Present> [Id]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const TSharedPtr<SInvestigationScreen> Screen = SInvestigationScreen::GetActive();
			const bool bDone = Screen.IsValid() && Screen->DevCommand(Args);
			UE_LOG(LogInvestigation, Display, TEXT("ColdCase.UI %s: %s"), *FString::Join(Args, TEXT(" ")), bDone ? TEXT("done") : TEXT("not possible here"));
		}));
}
#endif

#pragma once

#include "CoreMinimal.h"
#include "CaseDataTypes.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UCaseMediaPlayback;
class UGameInstance;
class UCaseFileSubsystem;
class UCaseResolutionSubsystem;
class UInterviewSubsystem;
class UInvestigationSaveSubsystem;
class UInvestigationSubsystem;
class UTheorySubsystem;

/**
 * The prototype's whole interface: a list of choices on the left, text on the right.
 *
 * It is presentation only. It holds no investigation state of its own: every choice calls one of the
 * investigation subsystems, then the screen is rebuilt from what they report.
 *
 * Built in Slate (Unreal's C++ UI layer) so the prototype needs no editor-made assets. It works with
 * mouse, keyboard (arrows, Enter, Esc) and gamepad (d-pad or left stick, A to choose, B to go back).
 * CommonUI screens will replace it later without touching the systems underneath.
 */
class SInvestigationScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInvestigationScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UGameInstance>, GameInstance)
		SLATE_ARGUMENT(TWeakObjectPtr<UCaseMediaPlayback>, MediaPlayback)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	enum class EPage : uint8
	{
		Home,
		Exhibits,
		Exhibit,
		Statements,
		People,
		Interview,
		PresentExhibit,
		Theory,
		ChooseAnswer,
		Review,
		ConfirmConclusion,
		CaseReview,
		SaveLoad,
		Clip
	};

	/** One choice in the left-hand list. */
	struct FMenuItem
	{
		FText Label;
		TFunction<void()> Action;
	};

	/** Moves to a page and rebuilds the screen on the next frame. */
	void GoTo(EPage NewPage);
	void GoBack();

	/** Fills Title, Body and Items for the current page from the investigation subsystems. */
	void BuildPage();
	void BuildHome();
	void BuildExhibits();
	void BuildExhibit();
	void BuildStatements();
	void BuildPeople();
	void BuildInterview();
	void BuildPresentExhibit();
	void BuildTheory();
	void BuildChooseAnswer();
	void BuildReview();
	void BuildConfirmConclusion();
	void BuildCaseReview();
	void BuildSaveLoad();
	void BuildClip();

	/** Starts a video or sound clip and shows it. ReturnTo is where Back leads afterwards. */
	void OpenClip(const FCaseMediaClip& Clip, EPage ReturnTo);

	/** Replaces the widgets on screen with ones for Title, Body and Items. */
	void RebuildWidgets();

	void AddItem(const FText& Label, TFunction<void()> Action);
	void AddBackItem();
	void AskOrPresent(FName Id, bool bPresent);

	const FCaseContent* GetCase() const;

	TWeakObjectPtr<UGameInstance> GameInstance;
	TWeakObjectPtr<UInvestigationSubsystem> Investigation;
	TWeakObjectPtr<UCaseFileSubsystem> CaseFile;
	TWeakObjectPtr<UInterviewSubsystem> Interviews;
	TWeakObjectPtr<UTheorySubsystem> Theory;
	TWeakObjectPtr<UCaseResolutionSubsystem> Resolution;
	TWeakObjectPtr<UInvestigationSaveSubsystem> Saves;

	TWeakObjectPtr<UCaseMediaPlayback> MediaPlayback;

	/** The clip on screen, and where to go when the player leaves it. */
	FCaseMediaClip CurrentClip;
	EPage ClipReturnPage = EPage::Home;
	bool bClipOpened = false;
	bool bOpeningFilmOffered = false;
	FSlateBrush PictureBrush;

	EPage Page = EPage::Home;
	bool bRebuildPending = true;

	/** What the current page is about, where a page needs it. */
	FName SelectedEvidence;
	FName SelectedQuestion;
	EInterviewApproach Approach = EInterviewApproach::Neutral;

	/** What has been said in the interview on screen. Display only; the case file keeps the real record. */
	TArray<FString> Transcript;

	/** A one-off message shown at the top of the next page (for example a refusal). */
	FString Notice;

	/** The top button of the current page, so focus can be put back on the list. */
	TWeakPtr<SWidget> FirstButtonWeak;

	FText Title;
	FString Body;
	TArray<FMenuItem> Items;

	FSlateColorBrush BackgroundBrush = FSlateColorBrush(FLinearColor(0.012f, 0.014f, 0.018f, 1.f));
	FSlateColorBrush FilmBackgroundBrush = FSlateColorBrush(FLinearColor::Black);
	FSlateColorBrush PanelBrush = FSlateColorBrush(FLinearColor(0.03f, 0.035f, 0.045f, 1.f));
};

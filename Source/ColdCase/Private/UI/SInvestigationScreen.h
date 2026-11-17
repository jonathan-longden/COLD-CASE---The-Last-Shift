#pragma once

#include "CoreMinimal.h"
#include "CaseDataTypes.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;

class UCaseMediaPlayback;
class UGameInstance;
class UCaseFileSubsystem;
class UCaseResolutionSubsystem;
class UInterviewSubsystem;
class UInvestigationSaveSubsystem;
class UInvestigationSubsystem;
class UTheorySubsystem;

/**
 * The prototype's whole interface: a list of choices on the left, the case file on the right.
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
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/**
	 * Development only: jumps the screen to a page, for unattended screenshots and tests.
	 * Args: <Page> [Id], e.g. "Exhibit TillRoll.E.TillRoll", "Interview TillRoll.C.Lorraine", "Clip TillRoll.M.TillRollDesk".
	 * "Ask <TopicId>" / "Present <EvidenceId>" act inside the interview on screen. Returns false if it could not.
	 */
	bool DevCommand(const TArray<FString>& Args);

	/** The screen currently shown, if any (development commands use it). */
	static TSharedPtr<SInvestigationScreen> GetActive();

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
		Clip,
		Cases
	};

	/** One choice in the left-hand list. A second line (after a line break) is shown smaller, as detail. */
	struct FMenuItem
	{
		FText Label;
		TFunction<void()> Action;
	};

	/** Colour coding for tags and card edges. Teal informs, amber asks for attention, red marks a problem. */
	enum class ETone : uint8 { Neutral, Teal, Amber, Red, Dim };

	/** One labelled line inside a card or document (a transcript line, a review field, a figure). */
	struct FRow
	{
		FString Label;
		FString Text;
		FString Tag;
		ETone Tone = ETone::Neutral;
		/** Typewriter face: for words quoted from a document or a recording. */
		bool bTyped = false;
		/** Shown quieter (for example the interviewer's own question in a transcript). */
		bool bQuiet = false;
		/** A thin rule above the row, to separate one exchange from the next. */
		bool bRuleAbove = false;
	};

	/** The kinds of part a page is made of. Each has its own look; see MakeBlock. */
	enum class EBlock : uint8
	{
		Lead,       // a plain paragraph introducing the page
		Section,    // a small heading with a rule
		Document,   // a typed sheet of paper: briefings, statements, transcripts
		Stats,      // small figures: the state of the file
		Card,       // a dark card: an exhibit, a person, a question, a save
		Picture,    // a photograph, given the room it needs
		Video,      // the clip playing now
		Banner      // a wide cropped photograph at the top of a page
	};

	struct FBlock
	{
		EBlock Kind = EBlock::Lead;
		FString Label;
		FString Text;
		FString Meta;
		FString Tag;
		FString Stamp;
		ETone Tone = ETone::Neutral;
		bool bTyped = false;
		TArray<FRow> Rows;
		/** Owned by the image cache, or the clip's picture brush. */
		const FSlateBrush* Image = nullptr;
		FVector2D ImageSize = FVector2D::ZeroVector;
		float ImageHeight = 0.f;
	};

	/** Moves to a page and rebuilds the screen on the next frame. */
	void GoTo(EPage NewPage);
	void GoBack();

	/** Fills Title, Blocks and Items for the current page from the investigation subsystems. */
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
	void BuildCases();

	/**
	 * New lines of enquiry (records that can now be requested, people who can now be found, things that
	 * can now be asked about) are announced once, at the top of the page, the moment they open up.
	 */
	void AnnounceNewLeads();
	TSet<FName> KnownLeads;
	FName LeadsCase;

	/** Starts a video or sound clip and shows it. ReturnTo is where Back leads afterwards. */
	void OpenClip(const FCaseMediaClip& Clip, EPage ReturnTo);

	/** Replaces the widgets on screen with ones for Title, Blocks and Items. */
	void RebuildWidgets();

	/** Page parts. */
	FBlock& AddBlock(EBlock Kind);
	void AddLead(const FString& Text, ETone Tone = ETone::Neutral);
	void AddSection(const FString& Label);
	FBlock& AddDocument(const FString& Label, const FString& Text);
	FBlock& AddCard(const FString& Label, const FString& Text);
	TSharedRef<SWidget> MakeBlock(const FBlock& Block);
	TSharedRef<SWidget> MakeRows(const TArray<FRow>& Rows, bool bOnPaper);
	TSharedRef<SWidget> MakeImage(const FSlateBrush* Image, float Height, bool bFill);
	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeFooter();
	FString GetMenuHeading() const;

	/** A picture file as a brush, loaded once. Null (and nothing drawn) when the file is not there. */
	const FSlateBrush* GetImage(const FString& Path);
	/** A still that sits beside a case's clips, e.g. Media/TillRollDesk.png for TillRollDesk.mp4. */
	const FSlateBrush* GetCaseImage(FName CasePlugin, const FString& File);
	const FSlateBrush* GetClipStill(const FCaseMediaClip& Clip);
	TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> ImageCache;

	/** Scrolls the case-file side with Page Up / Page Down or the shoulder buttons. */
	TWeakPtr<SScrollBox> ContentScrollWeak;

	/** A short fade when a new page comes up. */
	double PageShownAt = 0.0;
	EPage LastShownPage = EPage::Home;
	float GetPageFade() const;

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

	/** The case the screen is showing. If the active case changes underneath it (for example from the console), the screen starts afresh. */
	FName ShownCasePlugin;

	/** The opening film's buttons fade away while it plays and come back on any key or mouse movement. */
	bool IsFullScreenFilm() const;
	float GetFilmMenuOpacity() const;
	double FilmMenuShownAt = 0.0;
	FSlateBrush PictureBrush;

	EPage Page = EPage::Home;
	bool bRebuildPending = true;

	/** What the current page is about, where a page needs it. */
	FName SelectedEvidence;
	FName SelectedQuestion;
	EInterviewApproach Approach = EInterviewApproach::Neutral;

	/** One question and answer in the interview on screen. */
	struct FExchange
	{
		FString Asked;
		FString Speaker;
		FString Line;
		bool bEnded = false;
	};

	/** What has been said in the interview on screen. Display only; the case file keeps the real record. */
	TArray<FExchange> Transcript;

	/** A one-off message shown at the top of the next page (for example a refusal). */
	FString Notice;
	/** Messages shown at the top of the page on screen: the one-off notice and any new lines of enquiry. */
	TArray<FString> Notices;

	/** The top button of the current page, so focus can be put back on the list. */
	TWeakPtr<SWidget> FirstButtonWeak;

	FText Title;
	TArray<FBlock> Blocks;
	TArray<FMenuItem> Items;

	FSlateColorBrush BackgroundBrush = FSlateColorBrush(FLinearColor(0.0034f, 0.0044f, 0.0060f, 1.f));

	/** The case-file look: a dim photograph of the office behind everything, and paper for documents. */
	TSharedPtr<FSlateDynamicImageBrush> BackdropBrush;
	TSharedPtr<FSlateDynamicImageBrush> PaperBrush;
	FSlateColorBrush FilmBackgroundBrush = FSlateColorBrush(FLinearColor::Black);
};

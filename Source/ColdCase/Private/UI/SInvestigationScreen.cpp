#include "UI/SInvestigationScreen.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "CaseDataLoader.h"
#include "CaseFileSubsystem.h"
#include "CaseMediaPlayback.h"
#include "CaseResolutionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "InvestigationLog.h"
#include "InvestigationSettings.h"
#include "InputCoreTypes.h"
#include "InterviewSubsystem.h"
#include "InvestigationSaveSubsystem.h"
#include "InvestigationSubsystem.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"
#include "TheorySubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

/*
 * The look, in one place.
 *
 * An evidence room at night: deep charcoal and navy surfaces, muted teal for information, amber only for
 * what has focus or needs attention, muted red for problems. Interface text is a clean sans-serif (Roboto,
 * the engine's own); the typewriter face is kept for words that come from a document or a recording.
 * Paper appears only as document cards. Sizes are for a 1920 x 1080 screen; Slate scales them for others.
 */
namespace InvestigationScreenPrivate
{
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B, float Alpha = 1.f)
	{
		FLinearColor Colour = FLinearColor::FromSRGBColor(FColor(R, G, B));
		Colour.A = Alpha;
		return Colour;
	}

	// Surfaces
	const FLinearColor Surface = Srgb(18, 23, 30, 0.94f);
	const FLinearColor SurfaceRaised = Srgb(26, 33, 42, 0.96f);
	const FLinearColor Line = Srgb(52, 63, 76);
	const FLinearColor Shade = Srgb(8, 10, 13, 0.72f);

	// Text
	const FLinearColor TextMain = Srgb(232, 234, 236);
	const FLinearColor TextBody = Srgb(196, 202, 208);
	const FLinearColor TextDim = Srgb(138, 149, 160);
	const FLinearColor Ink = Srgb(34, 31, 27);
	const FLinearColor InkDim = Srgb(96, 88, 76);

	// Accents
	const FLinearColor Teal = Srgb(111, 163, 166);
	const FLinearColor Amber = Srgb(212, 157, 78);
	const FLinearColor Red = Srgb(184, 92, 78);
	const FLinearColor StampRed = Srgb(150, 42, 34, 0.85f);

	FLinearColor WithAlpha(FLinearColor Colour, float Alpha)
	{
		Colour.A = Alpha;
		return Colour;
	}

	FLinearColor ToneColour(uint8 Tone)
	{
		switch (Tone)
		{
		case 1: return Teal;
		case 2: return Amber;
		case 3: return Red;
		case 4: return TextDim;
		default: return TextBody;
		}
	}

	// Type
	FSlateFontInfo Sans(int32 Size, const TCHAR* Face = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Face, Size);
	}

	/** Small capitals labels: bold, a little letter-spaced, always paired with plenty of contrast. */
	FSlateFontInfo Caps(int32 Size)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Font.LetterSpacing = 160;
		return Font;
	}

	FSlateFontInfo TypedFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Mono", Size); }

	// Brushes. Built once; widgets keep pointers to them.
	const FSlateBrush* CardBrush()
	{
		static FSlateRoundedBoxBrush Brush(Surface, 3.f, WithAlpha(Line, 0.9f), 1.f);
		return &Brush;
	}
	const FSlateBrush* RaisedBrush()
	{
		static FSlateRoundedBoxBrush Brush(SurfaceRaised, 3.f, WithAlpha(Line, 0.9f), 1.f);
		return &Brush;
	}
	const FSlateBrush* ShadeBrush()
	{
		static FSlateColorBrush Brush(Shade);
		return &Brush;
	}
	const FSlateBrush* LineBrush()
	{
		static FSlateColorBrush Brush(Line);
		return &Brush;
	}
	const FSlateBrush* InkRuleBrush()
	{
		static FSlateColorBrush Brush(WithAlpha(InkDim, 0.35f));
		return &Brush;
	}
	const FSlateBrush* PaperShadowBrush()
	{
		static FSlateRoundedBoxBrush Brush(FLinearColor(0.f, 0.f, 0.f, 0.55f), 2.f);
		return &Brush;
	}
	const FSlateBrush* MenuBrush(bool bLit)
	{
		static FSlateRoundedBoxBrush Normal(FLinearColor(1.f, 1.f, 1.f, 0.025f), 2.f);
		static FSlateRoundedBoxBrush Lit(WithAlpha(Amber, 0.13f), 2.f, WithAlpha(Amber, 0.55f), 1.f);
		return bLit ? &Lit : &Normal;
	}
	const FSlateBrush* KeyBrush()
	{
		static FSlateRoundedBoxBrush Brush(FLinearColor(1.f, 1.f, 1.f, 0.04f), 3.f, Line, 1.f);
		return &Brush;
	}
	const FSlateBrush* ToneBrush(uint8 Tone)
	{
		static FSlateRoundedBoxBrush Neutral(FLinearColor(1.f, 1.f, 1.f, 0.05f), 2.f, WithAlpha(TextDim, 0.6f), 1.f);
		static FSlateRoundedBoxBrush TealBrush(WithAlpha(Teal, 0.14f), 2.f, WithAlpha(Teal, 0.7f), 1.f);
		static FSlateRoundedBoxBrush AmberBrush(WithAlpha(Amber, 0.14f), 2.f, WithAlpha(Amber, 0.7f), 1.f);
		static FSlateRoundedBoxBrush RedBrush(WithAlpha(Red, 0.16f), 2.f, WithAlpha(Red, 0.75f), 1.f);
		switch (Tone)
		{
		case 1: return &TealBrush;
		case 2: return &AmberBrush;
		case 3: return &RedBrush;
		default: return &Neutral;
		}
	}
	const FSlateBrush* StampBrush()
	{
		static FSlateRoundedBoxBrush Brush(FLinearColor::Transparent, 2.f, StampRed, 1.5f);
		return &Brush;
	}
	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("GenericWhiteBox");
	}

	/** The default scroll box draws a dark shadow line at its edge when text overflows; here it looks like a rule through the text. */
	const FScrollBoxStyle* QuietScrollStyle()
	{
		static FScrollBoxStyle Style = []()
		{
			FScrollBoxStyle Copy = FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox");
			Copy.SetTopShadowBrush(*FStyleDefaults::GetNoBrush());
			Copy.SetBottomShadowBrush(*FStyleDefaults::GetNoBrush());
			Copy.SetLeftShadowBrush(*FStyleDefaults::GetNoBrush());
			Copy.SetRightShadowBrush(*FStyleDefaults::GetNoBrush());
			return Copy;
		}();
		return &Style;
	}

	/** A small coloured label: SUPPORTED, NOT YET EXAMINED, and so on. */
	TSharedRef<SWidget> MakeTag(const FString& Text, uint8 Tone)
	{
		return SNew(SBorder)
			.BorderImage(ToneBrush(Tone))
			.Padding(FMargin(8.f, 3.f))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Text.ToUpper()))
				.Font(Caps(10))
				.ColorAndOpacity(FSlateColor(Tone == 0 ? TextBody : ToneColour(Tone)))
			];
	}

	/** An outlined, slightly turned stamp: RESTRICTED on documents, EVIDENCE on exhibits. */
	TSharedRef<SWidget> MakeStamp(const FString& Text)
	{
		return SNew(SBorder)
			.BorderImage(StampBrush())
			.Padding(FMargin(10.f, 4.f))
			.Visibility(EVisibility::HitTestInvisible)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-4.f))))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Text.ToUpper()))
				.Font(Caps(12))
				.ColorAndOpacity(FSlateColor(StampRed))
			];
	}

	/** A one-pixel rule. */
	TSharedRef<SWidget> MakeRule(const FSlateBrush* Brush)
	{
		return SNew(SBox)
			.HeightOverride(1.f)
			[
				SNew(SImage).Image(Brush)
			];
	}

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
			// "Load slot 1\nCase 1, 17/11/2026" reads as a line and a smaller line of detail.
			const FString Full = InArgs._Label.ToString();
			FString Main = Full;
			FString Detail;
			if (!Full.Split(TEXT("\n"), &Main, &Detail))
			{
				Main = Full;
				Detail.Reset();
			}

			const TSharedRef<SVerticalBox> Text = SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Main))
					.Font(Sans(16))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(IsLit() ? TextMain : TextBody); })
					.AutoWrapText(true)
				];
			if (!Detail.IsEmpty())
			{
				Text->AddSlot()
					.AutoHeight()
					.Padding(0.f, 3.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Detail))
						.Font(Sans(13))
						.ColorAndOpacity(FSlateColor(TextDim))
						.AutoWrapText(true)
					];
			}

			SButton::Construct(SButton::FArguments()
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
				.OnClicked(InArgs._OnClicked)
				.ContentPadding(FMargin(0.f))
				[
					SNew(SBorder)
					.BorderImage_Lambda([this]() { return MenuBrush(IsLit()); })
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
								.Image(WhiteBrush())
								.ColorAndOpacity_Lambda([this]() { return FSlateColor(IsLit() ? Amber : FLinearColor::Transparent); })
							]
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						.Padding(FMargin(14.f, 11.f, 8.f, 11.f))
						[
							Text
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.f, 0.f, 14.f, 0.f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("\u203A")))
							.Font(Sans(18))
							.ColorAndOpacity_Lambda([this]() { return FSlateColor(IsLit() ? Amber : FLinearColor::Transparent); })
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

	FString StrengthName(EEvidentialStrength Strength)
	{
		switch (Strength)
		{
		case EEvidentialStrength::Unsupported: return TEXT("Unsupported");
		case EEvidentialStrength::Thin: return TEXT("Thin");
		case EEvidentialStrength::Contested: return TEXT("Contested");
		case EEvidentialStrength::Supported: return TEXT("Supported");
		default: return TEXT("No answer");
		}
	}

	uint8 StrengthTone(EEvidentialStrength Strength)
	{
		switch (Strength)
		{
		case EEvidentialStrength::Supported: return 1;
		case EEvidentialStrength::Contested:
		case EEvidentialStrength::Thin: return 2;
		case EEvidentialStrength::Unsupported: return 3;
		default: return 4;
		}
	}

	/** Reads a PNG's size from its header, so a picture is never drawn squashed. */
	bool ReadPngSize(const FString& Path, FVector2D& OutSize)
	{
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));
		if (!Reader || Reader->TotalSize() < 24)
		{
			return false;
		}
		uint8 Header[24];
		Reader->Serialize(Header, 24);
		if (Header[0] != 0x89 || Header[1] != 'P' || Header[2] != 'N' || Header[3] != 'G')
		{
			return false;
		}
		const uint32 Width = (uint32(Header[16]) << 24) | (uint32(Header[17]) << 16) | (uint32(Header[18]) << 8) | uint32(Header[19]);
		const uint32 Height = (uint32(Header[20]) << 24) | (uint32(Header[21]) << 16) | (uint32(Header[22]) << 8) | uint32(Header[23]);
		if (Width == 0 || Height == 0)
		{
			return false;
		}
		OutSize = FVector2D(static_cast<double>(Width), static_cast<double>(Height));
		return true;
	}
}

namespace InvestigationScreenPrivate
{
	TWeakPtr<SInvestigationScreen> ActiveScreen;

	/** How long the opening film's buttons stay up after the film starts or the player touches a control. */
	constexpr double FilmMenuHoldSeconds = 3.0;
	constexpr double FilmMenuFadeSeconds = 0.6;

	/** A new page fades in over this long. Short enough never to slow anyone down. */
	constexpr double PageFadeSeconds = 0.18;

	/** How far one press of Page Up / Page Down (or a shoulder button) moves the case-file side. */
	constexpr float ContentScrollStep = 320.f;

	/** The case-file side never grows wider than this, so lines stay comfortable to read on wide screens. */
	constexpr float ContentMaxWidth = 1120.f;
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

const FSlateBrush* SInvestigationScreen::GetImage(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	if (const TSharedPtr<FSlateDynamicImageBrush>* Cached = ImageCache.Find(Path))
	{
		return Cached->Get();
	}
	TSharedPtr<FSlateDynamicImageBrush> Brush;
	FVector2D Size;
	if (FPaths::FileExists(Path) && InvestigationScreenPrivate::ReadPngSize(Path, Size))
	{
		Brush = MakeShared<FSlateDynamicImageBrush>(FName(*Path), Size);
	}
	ImageCache.Add(Path, Brush);   // a miss is remembered too, so a missing file is only looked for once
	return Brush.Get();
}

const FSlateBrush* SInvestigationScreen::GetCaseImage(FName CasePlugin, const FString& File)
{
	return GetImage(FCaseDataLoader::GetMediaPath(CasePlugin, File));
}

const FSlateBrush* SInvestigationScreen::GetClipStill(const FCaseMediaClip& Clip)
{
	if (Clip.File.IsEmpty() || !Investigation.IsValid())
	{
		return nullptr;
	}
	return GetCaseImage(Investigation->GetActiveCasePlugin(), FPaths::GetBaseFilename(Clip.File) + TEXT(".png"));
}

SInvestigationScreen::FBlock& SInvestigationScreen::AddBlock(EBlock Kind)
{
	FBlock& Block = Blocks.AddDefaulted_GetRef();
	Block.Kind = Kind;
	return Block;
}

void SInvestigationScreen::AddLead(const FString& Text, ETone Tone)
{
	FBlock& Block = AddBlock(EBlock::Lead);
	Block.Text = Text;
	Block.Tone = Tone;
}

void SInvestigationScreen::AddSection(const FString& Label)
{
	AddBlock(EBlock::Section).Label = Label;
}

SInvestigationScreen::FBlock& SInvestigationScreen::AddDocument(const FString& Label, const FString& Text)
{
	FBlock& Block = AddBlock(EBlock::Document);
	Block.Label = Label;
	Block.Text = Text;
	return Block;
}

SInvestigationScreen::FBlock& SInvestigationScreen::AddCard(const FString& Label, const FString& Text)
{
	FBlock& Block = AddBlock(EBlock::Card);
	Block.Label = Label;
	Block.Text = Text;
	return Block;
}

void SInvestigationScreen::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// A different case is now open (from the case list or the console): leave any clip and start on the new case's front page.
	// Its opening film plays if the case was opened from the beginning.
	if (Investigation.IsValid() && Investigation->IsCaseActive())
	{
		const FName Active = Investigation->GetActiveCasePlugin();
		if (ShownCasePlugin.IsNone())
		{
			ShownCasePlugin = Active;
		}
		else if (Active != ShownCasePlugin)
		{
			ShownCasePlugin = Active;
			Transcript.Reset();
			LeadsCase = NAME_None;
			bOpeningFilmOffered = !(Saves.IsValid() && Saves->WasLastOpenFresh());
			GoTo(EPage::Home);
		}
	}

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

	// The list keeps the focus; the case-file side scrolls on its own keys, so long reports can be read with any control.
	const bool bDown = Key == EKeys::PageDown || Key == EKeys::Gamepad_RightShoulder || Key == EKeys::Gamepad_RightStick_Down;
	const bool bUp = Key == EKeys::PageUp || Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightStick_Up;
	if (bDown || bUp)
	{
		if (const TSharedPtr<SScrollBox> Scroll = ContentScrollWeak.Pin())
		{
			const float Offset = Scroll->GetScrollOffset() + (bDown ? 1.f : -1.f) * InvestigationScreenPrivate::ContentScrollStep;
			Scroll->SetScrollOffset(FMath::Clamp(Offset, 0.f, Scroll->GetScrollOffsetOfEnd()));
		}
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

float SInvestigationScreen::GetPageFade() const
{
	const double Since = FPlatformTime::Seconds() - PageShownAt;
	const float Linear = static_cast<float>(FMath::Clamp(Since / InvestigationScreenPrivate::PageFadeSeconds, 0.0, 1.0));
	return 1.f - (1.f - Linear) * (1.f - Linear);   // eases out
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
	Blocks.Reset();
	Notices.Reset();
	Items.Reset();

	if (!GetCase() || !CaseFile.IsValid() || !Interviews.IsValid() || !Theory.IsValid() || !Resolution.IsValid() || !Saves.IsValid())
	{
		Title = FText::FromString(TEXT("COLD CASE"));
		AddLead(TEXT("No case is loaded. Check the Output Log (LogInvestigation) for the reason."));
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
	case EPage::Cases: BuildCases(); break;
	default: BuildHome(); break;
	}

	AnnounceNewLeads();

	if (!Notice.IsEmpty())
	{
		Notices.Insert(Notice, 0);
		Notice.Reset();
	}
}

void SInvestigationScreen::BuildHome()
{
	const FCaseContent& Case = *GetCase();
	Title = Case.Title;

	// The case's own cover picture across the top (a still from its film), or Nottingham at night.
	{
		const FSlateBrush* Cover = GetCaseImage(Investigation->GetActiveCasePlugin(), TEXT("Cover.png"));
		if (!Cover)
		{
			Cover = GetImage(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Art"), TEXT("UI"), TEXT("Nottingham.png"))));
		}
		if (Cover)
		{
			FBlock& Banner = AddBlock(EBlock::Banner);
			Banner.Image = Cover;
			Banner.ImageHeight = 250.f;
		}
	}

	if (Investigation->IsCaseConcluded())
	{
		AddLead(TEXT("The investigation is concluded and the file is closed."));
		AddItem(FText::FromString(TEXT("Read the case review")), [this]() { GoTo(EPage::CaseReview); });
		AddItem(FText::FromString(TEXT("Look back through the exhibits")), [this]() { GoTo(EPage::Exhibits); });
		AddItem(FText::FromString(TEXT("Look back through the statements")), [this]() { GoTo(EPage::Statements); });
		AddItem(FText::FromString(TEXT("Save or load")), [this]() { GoTo(EPage::SaveLoad); });
		AddItem(FText::FromString(TEXT("Other case files")), [this]() { GoTo(EPage::Cases); });
		return;
	}

	AddDocument(TEXT("Briefing"), Case.Briefing.ToString()).Stamp = TEXT("Restricted");

	// The state of the file, as a few figures.
	{
		int32 Held = 0, Unexamined = 0;
		for (const FHeldEvidence& Item : CaseFile->GetHeldEvidence())
		{
			++Held;
			Unexamined += Item.bExamined ? 0 : 1;
		}
		int32 Spoken = 0, Available = 0;
		for (const FCaseCharacter& Character : Case.Characters)
		{
			if (!Interviews.IsValid() || !Interviews->IsCharacterAvailable(Character.Id))
			{
				continue;
			}
			++Available;
			const FCharacterRuntimeState* State = Interviews.IsValid() ? Interviews->GetCharacterState(Character.Id) : nullptr;
			Spoken += (State && State->bInterviewed) ? 1 : 0;
		}
		int32 Answered = 0;
		for (const FName QuestionId : Case.KeyQuestions)
		{
			Answered += Theory.IsValid() && !Theory->GetAnswer(QuestionId).IsNone() ? 1 : 0;
		}
		const int32 Pending = CaseFile->GetPendingRequests().Num();

		AddSection(TEXT("State of the file"));
		FBlock& Stats = AddBlock(EBlock::Stats);
		auto Stat = [&Stats](const FString& Label, const FString& Value, const FString& Detail, ETone Tone)
		{
			FRow& Row = Stats.Rows.AddDefaulted_GetRef();
			Row.Label = Label;
			Row.Text = Value;
			Row.Tag = Detail;
			Row.Tone = Tone;
		};
		Stat(TEXT("Exhibits held"), FString::FromInt(Held), Unexamined > 0 ? FString::Printf(TEXT("%d not yet examined"), Unexamined) : FString(), Unexamined > 0 ? ETone::Amber : ETone::Neutral);
		Stat(TEXT("Requests outstanding"), FString::FromInt(Pending), FString(), ETone::Neutral);
		Stat(TEXT("Statements taken"), FString::FromInt(CaseFile->GetStatements().Num()), FString(), ETone::Neutral);
		Stat(TEXT("People spoken to"), FString::Printf(TEXT("%d of %d"), Spoken, Available), FString(), ETone::Neutral);
		Stat(TEXT("Key questions answered"), FString::Printf(TEXT("%d of %d"), Answered, Case.KeyQuestions.Num()), FString(), ETone::Neutral);

		AddLead(TEXT("Nothing in this file has been tested in court. Treat every page as a claim, not a fact."), ETone::Dim);
	}

	AddItem(FText::FromString(TEXT("Exhibits")), [this]() { GoTo(EPage::Exhibits); });
	AddItem(FText::FromString(TEXT("Statements taken")), [this]() { GoTo(EPage::Statements); });
	AddItem(FText::FromString(TEXT("Interview someone")), [this]() { GoTo(EPage::People); });
	AddItem(FText::FromString(TEXT("Your theory")), [this]() { GoTo(EPage::Theory); });
	AddItem(FText::FromString(TEXT("Save or load")), [this]() { GoTo(EPage::SaveLoad); });
	AddItem(FText::FromString(TEXT("Other case files")), [this]() { GoTo(EPage::Cases); });
}

void SInvestigationScreen::BuildExhibits()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("Exhibits"));
	AddLead(TEXT("Everything in the file. Open an exhibit to examine it."));

	for (const FHeldEvidence& Held : CaseFile->GetHeldEvidence())
	{
		const FEvidenceItem* Item = Case.FindEvidence(Held.EvidenceId);
		if (!Item)
		{
			continue;
		}

		// The exhibit's label in the file, with a small picture where there is one.
		FBlock& Card = AddCard(Item->Name.ToString(), FString());
		Card.Meta = FString::Printf(TEXT("Exhibit %s"), *Item->ExhibitRef);
		Card.Tag = Held.bExamined ? TEXT("Examined") : TEXT("Not yet examined");
		Card.Tone = Held.bExamined ? ETone::Dim : ETone::Amber;
		for (const FCaseMediaClip& Clip : Item->Media)
		{
			if (const FSlateBrush* Still = GetClipStill(Clip))
			{
				Card.Image = Still;
				Card.ImageHeight = 72.f;
				break;
			}
		}

		const FName EvidenceId = Item->Id;
		const FString Label = FString::Printf(TEXT("%s  %s%s"), *Item->ExhibitRef, *Item->Name.ToString(), Held.bExamined ? TEXT("") : TEXT("\nNot yet examined"));
		AddItem(FText::FromString(Label), [this, EvidenceId]()
		{
			SelectedEvidence = EvidenceId;
			GoTo(EPage::Exhibit);
		});
	}

	// Records that can be asked for. They arrive after the next interview.
	if (!Investigation->IsCaseConcluded())
	{
		bool bFirstRequest = true;
		for (const FEvidenceItem& Item : Case.Evidence)
		{
			if (Item.Availability != EEvidenceAvailability::OnRequest || CaseFile->HasEvidence(Item.Id))
			{
				continue;
			}
			const bool bPending = CaseFile->GetPendingRequests().Contains(Item.Id);
			if (!bPending && !CaseFile->CanRequestEvidence(Item.Id))
			{
				// Nobody knows to ask for this yet.
				continue;
			}
			if (bFirstRequest)
			{
				AddSection(TEXT("Records"));
				bFirstRequest = false;
			}
			if (bPending)
			{
				FBlock& Card = AddCard(Item.Name.ToString(), FString::Printf(TEXT("Requested and awaited: %s."), *Item.Name.ToString()));
				Card.Tag = TEXT("Requested");
				Card.Tone = ETone::Teal;
				continue;
			}
			FBlock& Card = AddCard(Item.Name.ToString(), FString());
			Card.Tag = TEXT("Can be requested");
			Card.Tone = ETone::Neutral;

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

	// The exhibit itself first: its photograph, where the file has one.
	for (const FCaseMediaClip& Clip : Item->Media)
	{
		if (const FSlateBrush* Still = GetClipStill(Clip))
		{
			FBlock& Picture = AddBlock(EBlock::Picture);
			Picture.Image = Still;
			Picture.Label = FString::Printf(TEXT("Exhibit %s"), *Item->ExhibitRef);
			Picture.Text = Item->Name.ToString();
			Picture.Stamp = TEXT("Evidence");
			break;
		}
	}

	FBlock& Description = AddDocument(FString::Printf(TEXT("Exhibit %s"), *Item->ExhibitRef), Item->Description.ToString());
	Description.Meta = Item->Name.ToString();

	for (const FCaseMediaClip& Clip : Item->Media)
	{
		const FCaseMediaClip ClipCopy = Clip;
		const FText Label = Clip.Caption.IsEmpty() ? FText::FromString(TEXT("Play the recording")) : Clip.Caption;
		AddItem(Label, [this, ClipCopy]() { OpenClip(ClipCopy, EPage::Exhibit); });
	}

	const FHeldEvidence* Held = CaseFile->GetHeldEvidence().FindByPredicate([this](const FHeldEvidence& H) { return H.EvidenceId == SelectedEvidence; });
	if (Held && Held->bExamined)
	{
		bool bFirstFinding = true;
		for (const FEvidenceFinding& Finding : Item->Findings)
		{
			if (bFirstFinding)
			{
				AddSection(TEXT("What you notice"));
				bFirstFinding = false;
			}
			FBlock& Card = AddCard(FString(), Finding.Text.ToString());
			if (Finding.Interpretations.Num() == 0)
			{
				continue;
			}

			const FName FindingId = Finding.Id;
			const FName ChosenId = CaseFile->GetChosenInterpretation(FindingId);
			const FEvidenceInterpretation* Chosen = Finding.Interpretations.FindByPredicate([ChosenId](const FEvidenceInterpretation& I) { return I.Id == ChosenId; });
			FRow& Reading = Card.Rows.AddDefaulted_GetRef();
			Reading.Label = TEXT("Your reading");
			Reading.Text = Chosen ? Chosen->Text.ToString() : FString(TEXT("You have not taken a view on what this shows."));
			Reading.bQuiet = Chosen == nullptr;
			Card.Tone = Chosen ? ETone::Teal : ETone::Neutral;

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
			FBlock& Card = AddCard(Speaker->Name.ToString(), FString::Printf(TEXT("\"%s\""), *Response->Line.ToString()));
			Card.Meta = FString::Printf(TEXT("Statement %d"), Statement.Number);
			Card.bTyped = true;
			if (Response->Certainty == EStatementCertainty::Unsure)
			{
				Card.Tag = TEXT("Not sure");
				Card.Tone = ETone::Amber;
			}
		}
	}
	if (Blocks.Num() == 0)
	{
		AddLead(TEXT("Nothing yet. What people tell you in interview is recorded here."));
	}

	AddBackItem();
}

void SInvestigationScreen::BuildPeople()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("Interview someone"));

	for (const FCaseCharacter& Character : Case.Characters)
	{
		const bool bAvailable = Interviews->IsCharacterAvailable(Character.Id);
		if (Character.CanBeInterviewed() && !bAvailable)
		{
			// Someone the investigation has not found yet.
			continue;
		}
		FBlock& Card = AddCard(Character.Name.ToString(), Character.Role.ToString());
		if (const FCharacterRuntimeState* State = Interviews->GetCharacterState(Character.Id))
		{
			if (State->bInterviewed)
			{
				Card.Tag = TEXT("Spoken to");
				Card.Tone = ETone::Teal;
			}
		}
		if (!bAvailable || Investigation->IsCaseConcluded())
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
	const bool bActive = Interviews->IsInterviewActive() && Character;

	if (bActive)
	{
		// Who is in the room, and how you are putting your questions.
		FBlock& Subject = AddCard(Character->Name.ToString(), Character->Role.ToString());
		Subject.Tag = FString::Printf(TEXT("Approach: %s"), *InvestigationScreenPrivate::ApproachName(Approach));
		Subject.Tone = ETone::Teal;
	}

	// The record of the interview, newest exchange first, so the latest answer is always in view.
	if (Transcript.Num() > 0)
	{
		FBlock& Record = AddDocument(TEXT("Record of interview"), FString());
		Record.Meta = Character ? Character->Name.ToString() : FString();
		for (int32 Index = Transcript.Num() - 1; Index >= 0; --Index)
		{
			const FExchange& Exchange = Transcript[Index];
			FRow& Asked = Record.Rows.AddDefaulted_GetRef();
			Asked.Label = FString::Printf(TEXT("%02d"), Index + 1);
			Asked.Text = Exchange.Asked + TEXT(".");
			Asked.bQuiet = true;
			Asked.bRuleAbove = Index != Transcript.Num() - 1;

			FRow& Answer = Record.Rows.AddDefaulted_GetRef();
			Answer.Label = Exchange.Speaker;
			Answer.Text = FString::Printf(TEXT("\"%s\""), *Exchange.Line);
			Answer.bTyped = true;
			if (Exchange.bEnded)
			{
				FRow& Ended = Record.Rows.AddDefaulted_GetRef();
				Ended.Text = FString::Printf(TEXT("%s ends the interview."), *Exchange.Speaker);
				Ended.bQuiet = true;
			}
		}
	}

	if (!bActive)
	{
		Title = FText::FromString(TEXT("Interview over"));
		AddItem(FText::FromString(TEXT("Leave")), [this]() { GoTo(EPage::People); });
		return;
	}

	Title = FText::FromString(FString::Printf(TEXT("Interview: %s"), *Character->Name.ToString()));
	if (Transcript.Num() == 0)
	{
		AddLead(TEXT("Ask about anything, or show them anything from the file. How you put it matters."));
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
		if (!Interviews->IsTopicAvailable(Topic.Id))
		{
			continue;
		}
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
	AddLead(TEXT("Anything in the file can be put to anyone."));

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
		FExchange& Exchange = Transcript.AddDefaulted_GetRef();
		Exchange.Asked = bPresent
			? FString::Printf(TEXT("You show %s %s%s"), *FirstName, *Subject, Manner)
			: FString::Printf(TEXT("You ask about %s%s"), *Subject, Manner);
		Exchange.Speaker = Name;
		Exchange.Line = Reply.Line.ToString();
		Exchange.bEnded = Reply.bInterviewEnded;
	}
	GoTo(EPage::Interview);
}

void SInvestigationScreen::BuildTheory()
{
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("Your theory"));
	AddLead(TEXT("Say what you think happened. You can change your answers until you submit. Your DCI will tell you how well the file supports them."));

	AddSection(TEXT("Key questions"));
	for (const FName QuestionId : Case.KeyQuestions)
	{
		const FCaseQuestion* Question = Case.FindQuestion(QuestionId);
		if (!Question)
		{
			continue;
		}
		const FCaseFact* Answer = Case.FindFact(Theory->GetAnswer(QuestionId));
		FBlock& Card = AddCard(Question->Text.ToString(), FString());
		FRow& Row = Card.Rows.AddDefaulted_GetRef();
		Row.Label = TEXT("Your answer");
		Row.Text = Answer ? Answer->Text.ToString() : FString(TEXT("No answer yet"));
		Row.bQuiet = Answer == nullptr;
		Card.Tone = Answer ? ETone::Teal : ETone::Amber;

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
	AddLead(TEXT("Choose the answer you are prepared to stand behind."));

	if (const FCaseFact* Current = Case.FindFact(Theory->GetAnswer(Question->Id)))
	{
		FBlock& Card = AddCard(FString(), FString());
		FRow& Row = Card.Rows.AddDefaulted_GetRef();
		Row.Label = TEXT("Your answer");
		Row.Text = Current->Text.ToString();
		Card.Tone = ETone::Teal;
	}

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
	using namespace InvestigationScreenPrivate;
	const FCaseContent& Case = *GetCase();
	Title = FText::FromString(TEXT("DCI review"));

	for (const FAnswerAssessment& Assessment : Theory->AssessTheory())
	{
		const FCaseQuestion* Question = Case.FindQuestion(Assessment.QuestionId);
		const FCaseFact* Fact = Case.FindFact(Assessment.FactId);
		FBlock& Card = AddCard(Question ? Question->Text.ToString() : TEXT("?"), FString());

		FRow& Said = Card.Rows.AddDefaulted_GetRef();
		Said.Label = TEXT("You say");
		Said.Text = Fact ? Fact->Text.ToString() : TEXT("(no answer)");
		Said.bQuiet = Fact == nullptr;

		FRow& Dci = Card.Rows.AddDefaulted_GetRef();
		Dci.Label = TEXT("DCI");
		Dci.Text = FString::Printf(TEXT("\"%s\""), *UTheorySubsystem::DescribeStrength(Assessment.Strength).ToString());
		Dci.Tag = StrengthName(Assessment.Strength);
		Dci.Tone = static_cast<ETone>(StrengthTone(Assessment.Strength));
	}

	AddBackItem();
}

void SInvestigationScreen::BuildConfirmConclusion()
{
	Title = FText::FromString(TEXT("Submit your conclusion"));
	FBlock& Card = AddCard(FString(), TEXT("This closes the investigation. There will be no more interviews and the file cannot be changed. A case review will follow."));
	Card.Tone = ETone::Red;

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
	using namespace InvestigationScreenPrivate;
	Title = FText::FromString(TEXT("Case review"));
	const FCaseReviewReport Report = Resolution.IsValid() ? Resolution->BuildReport() : FCaseReviewReport();
	const FCaseContent* Case = GetCase();
	if (!Report.bValid || !Case)
	{
		AddLead(TEXT("There is no case review until the investigation has been concluded."));
		AddBackItem();
		return;
	}

	auto Sentence = [](FString Text)
	{
		Text.TrimStartAndEndInline();
		if (!Text.IsEmpty() && !FChar::IsPunct(Text[Text.Len() - 1]))
		{
			Text += TEXT(".");
		}
		return Text;
	};

	// A reviewing officer's report. Each answer is kept in three clearly separate parts: what the
	// investigation concluded, what the file actually supports, and what the review found.
	FBlock& Heading = AddDocument(TEXT("Review of the investigation"), Case->Title.ToString());
	Heading.Stamp = TEXT("Restricted");

	AddSection(TEXT("What the investigation concluded"));
	int32 Number = 0;
	for (const FCaseReviewAnswer& Answer : Report.Answers)
	{
		const FCaseQuestion* Question = Case->FindQuestion(Answer.Assessment.QuestionId);
		const FCaseFact* PlayerFact = Case->FindFact(Answer.Assessment.FactId);
		const FCaseFact* TrueFact = Case->FindFact(Answer.TrueFactId);

		FBlock& Card = AddCard(FString::Printf(TEXT("%d. %s"), ++Number, Question ? *Question->Text.ToString() : TEXT("?")), FString());
		Card.Tone = Answer.bMatchesTruth ? ETone::Teal : ETone::Red;

		FRow& Concluded = Card.Rows.AddDefaulted_GetRef();
		Concluded.Label = TEXT("Concluded");
		Concluded.Text = Sentence(PlayerFact ? PlayerFact->Text.ToString() : TEXT("no answer"));

		FRow& Support = Card.Rows.AddDefaulted_GetRef();
		Support.Label = TEXT("Support");
		Support.Text = Sentence(UTheorySubsystem::DescribeStrength(Answer.Assessment.Strength).ToString());
		Support.Tag = StrengthName(Answer.Assessment.Strength);
		Support.Tone = static_cast<ETone>(StrengthTone(Answer.Assessment.Strength));

		FRow& Finding = Card.Rows.AddDefaulted_GetRef();
		Finding.Label = TEXT("Finding");
		Finding.Text = Answer.bMatchesTruth
			? FString(TEXT("This is what happened."))
			: FString::Printf(TEXT("This is not what happened. In fact: %s"), *Sentence(TrueFact ? TrueFact->Text.ToString() : TEXT("not recorded")));
		Finding.Tag = Answer.bMatchesTruth ? TEXT("What happened") : TEXT("Not what happened");
		Finding.Tone = Answer.bMatchesTruth ? ETone::Teal : ETone::Red;
	}

	AddSection(TEXT("Lines of enquiry not followed"));
	{
		FBlock& Missed = AddCard(FString(), FString());
		if (Report.Missed.Num() == 0)
		{
			Missed.Text = TEXT("None identified.");
		}
		for (const FText& MissedLine : Report.Missed)
		{
			FRow& Row = Missed.Rows.AddDefaulted_GetRef();
			Row.Text = FString::Printf(TEXT("\u2022  %s"), *MissedLine.ToString());
		}
		Missed.Tone = Report.Missed.Num() > 0 ? ETone::Amber : ETone::Neutral;
	}

	AddSection(TEXT("What happened"));
	AddDocument(TEXT("What happened"), Report.TruthSummary.ToString());

	if (Report.Afterwards.Num() > 0)
	{
		AddSection(TEXT("Afterwards"));
		for (const FText& Paragraph : Report.Afterwards)
		{
			AddLead(Paragraph.ToString());
		}
	}
	AddBackItem();
}

void SInvestigationScreen::BuildSaveLoad()
{
	Title = FText::FromString(TEXT("Save or load"));
	AddLead(TEXT("The game also saves by itself after each interview and when you submit your conclusion."));

	auto Describe = [](const FSaveSlotHeader& Header)
	{
		return FString::Printf(TEXT("%s, %s\n%s"), *Header.CaseTitle.ToString(), *Header.SavedAt.ToString(TEXT("%d/%m/%Y %H:%M")), *Header.Summary);
	};
	auto SlotCard = [this](const FString& Name, bool bUsed, const FSaveSlotHeader& Header)
	{
		FBlock& Card = AddCard(Name, bUsed ? Header.Summary : FString(TEXT("Empty")));
		if (bUsed)
		{
			Card.Meta = FString::Printf(TEXT("%s  \u00B7  %s"), *Header.CaseTitle.ToString(), *Header.SavedAt.ToString(TEXT("%d/%m/%Y %H:%M")));
			Card.Tone = ETone::Teal;
		}
		else
		{
			Card.Tone = ETone::Dim;
		}
	};

	AddSection(TEXT("Slots"));
	for (int32 Slot = 1; Slot <= UInvestigationSaveSubsystem::NumManualSlots; ++Slot)
	{
		FSaveSlotHeader Header;
		const bool bUsed = Saves->GetManualSlotHeader(Slot, Header);
		SlotCard(FString::Printf(TEXT("Slot %d"), Slot), bUsed, Header);

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
				LeadsCase = NAME_None;
				Notice = bLoaded ? FString::Printf(TEXT("Loaded slot %d."), Slot) : FString(TEXT("That save could not be loaded."));
				GoTo(bLoaded ? EPage::Home : EPage::SaveLoad);
			});
		}
	}

	FSaveSlotHeader AutoHeader;
	if (Saves->GetAutosaveHeader(AutoHeader))
	{
		SlotCard(TEXT("Autosave"), true, AutoHeader);
		AddItem(FText::FromString(FString::Printf(TEXT("Load the autosave\n%s"), *Describe(AutoHeader))), [this]()
		{
			const bool bLoaded = Saves.IsValid() && Saves->LoadAutosave();
			Transcript.Reset();
			LeadsCase = NAME_None;
			Notice = bLoaded ? FString(TEXT("Loaded the autosave.")) : FString(TEXT("The autosave could not be loaded."));
			GoTo(bLoaded ? EPage::Home : EPage::SaveLoad);
		});
	}

	AddBackItem();
}

void SInvestigationScreen::BuildCases()
{
	Title = FText::FromString(TEXT("Case files"));

	if (const FSlateBrush* Night = GetImage(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Art"), TEXT("UI"), TEXT("Nottingham.png")))))
	{
		FBlock& Banner = AddBlock(EBlock::Banner);
		Banner.Image = Night;
		Banner.ImageHeight = 200.f;
	}
	AddLead(TEXT("Every file here was closed without an answer. Each one keeps its own place: you can put one down and pick it up again where you left it."));

	const UInvestigationSettings* Settings = GetDefault<UInvestigationSettings>();
	TArray<FName> Plugins = Settings->CasePlugins;
	if (Plugins.Num() == 0)
	{
		Plugins.Add(Settings->StartupCasePlugin);
	}

	for (const FName Plugin : Plugins)
	{
		FCaseContent Content;
		FString Error;
		if (!FCaseDataLoader::LoadContent(Plugin, Content, Error))
		{
			UE_LOG(LogInvestigation, Warning, TEXT("Case list: %s"), *Error);
			continue;
		}

		const bool bCurrent = Investigation->GetActiveCasePlugin() == Plugin;
		FString Status;
		FSaveSlotHeader Header;
		if (bCurrent)
		{
			Status = Investigation->IsCaseConcluded() ? TEXT("Open on your desk. Concluded.") : TEXT("Open on your desk.");
		}
		else if (Saves->GetAutosaveHeaderFor(Plugin, Header))
		{
			Status = FString::Printf(TEXT("Last worked on %s: %s."), *Header.SavedAt.ToString(TEXT("%d/%m/%Y")), *Header.Summary);
		}
		else
		{
			Status = TEXT("Not yet opened.");
		}

		// The first sentence of the briefing is enough to say what the case is.
		FString Logline = Content.Briefing.ToString();
		int32 Stop = INDEX_NONE;
		if (Logline.FindChar(TEXT('.'), Stop))
		{
			Logline.LeftInline(Stop + 1);
		}
		FBlock& Card = AddCard(Content.Title.ToString(), Logline);
		FRow& StatusRow = Card.Rows.AddDefaulted_GetRef();
		StatusRow.Text = Status;
		StatusRow.bQuiet = true;
		Card.Image = GetCaseImage(Plugin, TEXT("Cover.png"));
		Card.ImageHeight = 104.f;
		if (bCurrent)
		{
			Card.Tag = TEXT("On your desk");
			Card.Tone = ETone::Amber;
		}

		if (bCurrent)
		{
			continue;
		}
		const FName PluginCopy = Plugin;
		const FString CaseTitle = Content.Title.ToString();
		AddItem(FText::FromString(FString::Printf(TEXT("Open %s"), *CaseTitle)), [this, PluginCopy, CaseTitle]()
		{
			const bool bOpened = Saves.IsValid() && Saves->OpenCase(PluginCopy);
			Transcript.Reset();
			LeadsCase = NAME_None;
			if (bOpened)
			{
				// A new case on the desk: its opening film (if it has one) plays the first time it is opened.
				ShownCasePlugin = PluginCopy;
				bOpeningFilmOffered = !Saves->WasLastOpenFresh();
				Notice = FString::Printf(TEXT("You take out the file: %s."), *CaseTitle);
			}
			else
			{
				Notice = TEXT("That file could not be opened. See the Output Log (LogInvestigation).");
			}
			GoTo(bOpened ? EPage::Home : EPage::Cases);
		});
	}

	AddBackItem();
}

void SInvestigationScreen::AnnounceNewLeads()
{
	const FCaseContent* Case = GetCase();
	if (!Case || !CaseFile.IsValid() || !Interviews.IsValid())
	{
		return;
	}

	// Every lead open right now, in the order the case lists them.
	TArray<TPair<FName, FString>> Open;
	for (const FEvidenceItem& Item : Case->Evidence)
	{
		if (Item.RequiresFlags.Num() > 0 && CaseFile->CanRequestEvidence(Item.Id))
		{
			Open.Emplace(Item.Id, FString::Printf(TEXT("New line of enquiry: %s can now be requested (Exhibits)."), *Item.Name.ToString()));
		}
	}
	for (const FCaseCharacter& Character : Case->Characters)
	{
		if (Character.RequiresFlags.Num() > 0 && Interviews->IsCharacterAvailable(Character.Id))
		{
			Open.Emplace(Character.Id, FString::Printf(TEXT("New line of enquiry: %s can now be interviewed."), *Character.Name.ToString()));
		}
	}
	for (const FInterviewTopic& Topic : Case->Topics)
	{
		if (Topic.RequiresFlags.Num() > 0 && Interviews->IsTopicAvailable(Topic.Id))
		{
			Open.Emplace(Topic.Id, FString::Printf(TEXT("New line of enquiry: you can now ask about %s."), *Topic.Text.ToString()));
		}
	}

	// A different case, or a save just loaded: take stock quietly, without announcing what was already known.
	const bool bTakeStock = LeadsCase != Case->CaseId;
	LeadsCase = Case->CaseId;
	if (bTakeStock)
	{
		KnownLeads.Reset();
	}

	for (const TPair<FName, FString>& Lead : Open)
	{
		bool bAlreadyKnown = false;
		KnownLeads.Add(Lead.Key, &bAlreadyKnown);
		if (!bAlreadyKnown && !bTakeStock)
		{
			Notices.Add(Lead.Value);
		}
	}
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

	UMediaTexture* Picture = MediaPlayback.IsValid() ? MediaPlayback->GetPicture() : nullptr;
	if (!bClipOpened)
	{
		Notices.Add(TEXT("This recording could not be played."));
	}
	else if (CurrentClip.Kind == ECaseMediaKind::Video && Picture)
	{
		PictureBrush = FSlateBrush();
		PictureBrush.SetResourceObject(Picture);
		PictureBrush.ImageSize = FVector2D(1280.f, 720.f);
		PictureBrush.DrawAs = ESlateBrushDrawType::Image;

		FBlock& Video = AddBlock(EBlock::Video);
		Video.Image = &PictureBrush;
	}

	if (!CurrentClip.Transcript.IsEmpty())
	{
		AddDocument(TEXT("Transcript"), CurrentClip.Transcript.ToString());
	}

	AddItem(FText::FromString(ClipReturnPage == EPage::Home ? TEXT("Continue") : TEXT("Back")), [this]() { GoBack(); });
	AddItem(FText::FromString(TEXT("Play again")), [this]()
	{
		const FCaseMediaClip Again = CurrentClip;
		OpenClip(Again, ClipReturnPage);
	});
}

FString SInvestigationScreen::GetMenuHeading() const
{
	switch (Page)
	{
	case EPage::Home: return TEXT("Case file");
	case EPage::Exhibits:
	case EPage::Exhibit: return TEXT("Exhibits");
	case EPage::Statements: return TEXT("Statements");
	case EPage::People: return TEXT("People");
	case EPage::Interview: return TEXT("In the interview room");
	case EPage::PresentExhibit: return TEXT("Show an exhibit");
	case EPage::Theory:
	case EPage::ChooseAnswer: return TEXT("Your theory");
	case EPage::Review: return TEXT("DCI review");
	case EPage::ConfirmConclusion: return TEXT("Conclusion");
	case EPage::CaseReview: return TEXT("Case review");
	case EPage::SaveLoad: return TEXT("Save or load");
	case EPage::Clip: return TEXT("Recording");
	case EPage::Cases: return TEXT("Case files");
	default: return TEXT("Options");
	}
}

TSharedRef<SWidget> SInvestigationScreen::MakeImage(const FSlateBrush* Image, float Height, bool bFill)
{
	// A photograph cropped to the space it is given, never stretched: it is scaled evenly to cover the
	// box and the overflow is clipped. Without a fixed height it is fitted whole instead.
	if (Height > 0.f)
	{
		return SNew(SBox)
			.HeightOverride(Height)
			.Clipping(EWidgetClipping::ClipToBounds)
			[
				SNew(SScaleBox)
				.Stretch(bFill ? EStretch::ScaleToFill : EStretch::ScaleToFit)
				[
					SNew(SImage).Image(Image)
				]
			];
	}
	return SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		[
			SNew(SImage).Image(Image)
		];
}

TSharedRef<SWidget> SInvestigationScreen::MakeRows(const TArray<FRow>& Rows, bool bOnPaper)
{
	using namespace InvestigationScreenPrivate;
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FRow& Row = Rows[Index];
		const FLinearColor TextColour = bOnPaper ? (Row.bQuiet ? InkDim : Ink) : (Row.bQuiet ? TextDim : TextBody);
		const FLinearColor LabelColour = bOnPaper ? InkDim : TextDim;
		const FSlateFontInfo Font = Row.bTyped ? TypedFont(bOnPaper ? 16 : 15) : Sans(16);

		if (Row.bRuleAbove)
		{
			Box->AddSlot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 12.f)
				[
					MakeRule(bOnPaper ? InkRuleBrush() : LineBrush())
				];
		}

		if (bOnPaper)
		{
			// On paper, rows read as a transcript: a narrow column for who is speaking, then what was said.
			Box->AddSlot()
				.AutoHeight()
				.Padding(0.f, Index == 0 ? 0.f : 6.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(170.f)
						.Padding(FMargin(0.f, 2.f, 16.f, 0.f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(Row.Label.ToUpper()))
							.Font(TypedFont(13))
							.ColorAndOpacity(FSlateColor(LabelColour))
							.AutoWrapText(true)
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Row.Text))
						.Font(Font)
						.LineHeightPercentage(1.15f)
						.ColorAndOpacity(FSlateColor(TextColour))
						.AutoWrapText(true)
					]
				];
			continue;
		}

		// On a card, a row is a small label (with its tag beside it) above its text.
		const TSharedRef<SVerticalBox> Field = SNew(SVerticalBox);
		if (!Row.Label.IsEmpty() || !Row.Tag.IsEmpty())
		{
			const TSharedRef<SHorizontalBox> LabelLine = SNew(SHorizontalBox);
			if (!Row.Label.IsEmpty())
			{
				LabelLine->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 10.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Row.Label.ToUpper()))
						.Font(Caps(11))
						.ColorAndOpacity(FSlateColor(LabelColour))
					];
			}
			if (!Row.Tag.IsEmpty())
			{
				LabelLine->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						MakeTag(Row.Tag, static_cast<uint8>(Row.Tone))
					];
			}
			Field->AddSlot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 6.f)
				[
					LabelLine
				];
		}
		if (!Row.Text.IsEmpty())
		{
			Field->AddSlot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Row.Text))
					.Font(Font)
					.LineHeightPercentage(1.15f)
					.ColorAndOpacity(FSlateColor(TextColour))
					.AutoWrapText(true)
				];
		}
		Box->AddSlot()
			.AutoHeight()
			.Padding(0.f, Index == 0 ? 0.f : 14.f, 0.f, 0.f)
			[
				Field
			];
	}
	return Box;
}

TSharedRef<SWidget> SInvestigationScreen::MakeBlock(const FBlock& Block)
{
	using namespace InvestigationScreenPrivate;
	const uint8 Tone = static_cast<uint8>(Block.Tone);

	switch (Block.Kind)
	{
	case EBlock::Section:
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Block.Label.ToUpper()))
				.Font(Caps(12))
				.ColorAndOpacity(FSlateColor(Teal))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				MakeRule(LineBrush())
			];

	case EBlock::Banner:
		return SNew(SBorder)
			.BorderImage(CardBrush())
			.Padding(FMargin(1.f))
			[
				MakeImage(Block.Image, Block.ImageHeight, true)
			];

	case EBlock::Picture:
	case EBlock::Video:
	{
		// The exhibit (or the clip) is the focus: as large as the column allows, on a dark mount,
		// with its label tucked under it like a card in an evidence bag.
		const TSharedRef<SVerticalBox> Mount = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.MaxDesiredHeight(Block.Kind == EBlock::Video ? 560.f : 400.f)
				[
					MakeImage(Block.Image, 0.f, false)
				]
			];
		if (!Block.Label.IsEmpty() || !Block.Text.IsEmpty())
		{
			Mount->AddSlot()
				.AutoHeight()
				.Padding(4.f, 12.f, 4.f, 2.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 14.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Block.Label.ToUpper()))
						.Font(Caps(12))
						.ColorAndOpacity(FSlateColor(Amber))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Block.Text))
						.Font(Sans(15))
						.ColorAndOpacity(FSlateColor(TextBody))
						.AutoWrapText(true)
					]
				];
		}
		return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(RaisedBrush())
				.Padding(FMargin(14.f))
				[
					Mount
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, 26.f, 28.f, 0.f))
			[
				Block.Stamp.IsEmpty() ? SNullWidget::NullWidget : MakeStamp(Block.Stamp)
			];
	}

	case EBlock::Document:
	{
		// A typed sheet: paper, a label line, the text in the typewriter face.
		const bool bPaper = PaperBrush.IsValid();
		const FLinearColor InkColour = bPaper ? Ink : TextMain;
		const FLinearColor LabelColour = bPaper ? InkDim : TextDim;

		const TSharedRef<SVerticalBox> Sheet = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Block.Label.ToUpper()))
					.Font(TypedFont(13))
					.ColorAndOpacity(FSlateColor(LabelColour))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(16.f, 0.f, Block.Stamp.IsEmpty() ? 0.f : 130.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Block.Meta))
					.Font(TypedFont(13))
					.ColorAndOpacity(FSlateColor(LabelColour))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 16.f)
			[
				MakeRule(bPaper ? InkRuleBrush() : LineBrush())
			];
		if (!Block.Text.IsEmpty())
		{
			Sheet->AddSlot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Block.Text))
					.Font(TypedFont(16))
					.LineHeightPercentage(1.2f)
					.ColorAndOpacity(FSlateColor(InkColour))
					.AutoWrapText(true)
				];
		}
		if (Block.Rows.Num() > 0)
		{
			Sheet->AddSlot()
				.AutoHeight()
				.Padding(0.f, Block.Text.IsEmpty() ? 0.f : 14.f, 0.f, 0.f)
				[
					MakeRows(Block.Rows, bPaper)
				];
		}

		return SNew(SOverlay)
			// A soft shadow under the sheet so it sits on the desk rather than floating.
			+ SOverlay::Slot()
			.Padding(FMargin(4.f, 6.f, 0.f, 0.f))
			[
				SNew(SBorder).BorderImage(PaperShadowBrush())
			]
			+ SOverlay::Slot()
			.Padding(FMargin(0.f, 0.f, 4.f, 6.f))
			[
				SNew(SBorder)
				.BorderImage(bPaper ? static_cast<const FSlateBrush*>(PaperBrush.Get()) : CardBrush())
				.Padding(FMargin(36.f, 26.f, 36.f, 30.f))
				[
					Sheet
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, 16.f, 26.f, 0.f))
			[
				Block.Stamp.IsEmpty() ? SNullWidget::NullWidget : MakeStamp(Block.Stamp)
			];
	}

	case EBlock::Stats:
	{
		// Small figures, side by side, wrapping onto a second line on narrow screens.
		const TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true);
		for (const FRow& Row : Block.Rows)
		{
			Wrap->AddSlot()
				.Padding(FMargin(0.f, 0.f, 12.f, 12.f))
				[
					SNew(SBox)
					.WidthOverride(200.f)
					[
						SNew(SBorder)
						.BorderImage(CardBrush())
						.Padding(FMargin(18.f, 14.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Row.Text))
								.Font(Sans(30, TEXT("Light")))
								.ColorAndOpacity(FSlateColor(TextMain))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 4.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(Row.Label.ToUpper()))
								.Font(Caps(10))
								.ColorAndOpacity(FSlateColor(TextDim))
								.AutoWrapText(true)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 4.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Visibility(Row.Tag.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
								.Text(FText::FromString(Row.Tag))
								.Font(Sans(13))
								.ColorAndOpacity(FSlateColor(Row.Tone == ETone::Neutral ? TextDim : ToneColour(static_cast<uint8>(Row.Tone))))
								.AutoWrapText(true)
							]
						]
					]
				];
		}
		return Wrap;
	}

	case EBlock::Card:
	{
		const TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

		// Title line: the card's name, with its tag at the right.
		if (!Block.Label.IsEmpty() || !Block.Tag.IsEmpty())
		{
			Content->AddSlot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Block.Label))
						.Font(Sans(17, TEXT("Bold")))
						.ColorAndOpacity(FSlateColor(TextMain))
						.AutoWrapText(true)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					.Padding(12.f, 0.f, 0.f, 0.f)
					[
						Block.Tag.IsEmpty() ? SNullWidget::NullWidget : MakeTag(Block.Tag, Tone)
					]
				];
		}
		if (!Block.Meta.IsEmpty())
		{
			Content->AddSlot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Block.Meta.ToUpper()))
					.Font(TypedFont(12))
					.ColorAndOpacity(FSlateColor(TextDim))
				];
		}
		const bool bHasHeading = !Block.Label.IsEmpty() || !Block.Tag.IsEmpty() || !Block.Meta.IsEmpty();
		if (!Block.Text.IsEmpty())
		{
			Content->AddSlot()
				.AutoHeight()
				.Padding(0.f, bHasHeading ? 10.f : 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Block.Text))
					.Font(Block.bTyped ? TypedFont(15) : Sans(16))
					.LineHeightPercentage(1.15f)
					.ColorAndOpacity(FSlateColor(TextBody))
					.AutoWrapText(true)
				];
		}
		if (Block.Rows.Num() > 0)
		{
			Content->AddSlot()
				.AutoHeight()
				.Padding(0.f, (bHasHeading || !Block.Text.IsEmpty()) ? 14.f : 0.f, 0.f, 0.f)
				[
					MakeRows(Block.Rows, false)
				];
		}

		const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		if (Block.Image)
		{
			// A thumbnail of the exhibit or case, cropped to a fixed frame.
			const float ThumbHeight = Block.ImageHeight > 0.f ? Block.ImageHeight : 72.f;
			Row->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(0.f, 0.f, 18.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(ThumbHeight * 1.78f)
					[
						SNew(SBorder)
						.BorderImage(RaisedBrush())
						.Padding(FMargin(1.f))
						[
							MakeImage(Block.Image, ThumbHeight, true)
						]
					]
				];
		}
		Row->AddSlot()
			.FillWidth(1.f)
			[
				Content
			];

		// A coloured edge on the left says at a glance what kind of card it is.
		const bool bEdge = Block.Tone == ETone::Teal || Block.Tone == ETone::Amber || Block.Tone == ETone::Red;
		return SNew(SBorder)
			.BorderImage(CardBrush())
			.Padding(FMargin(0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(3.f)
					[
						SNew(SImage)
						.Image(WhiteBrush())
						.ColorAndOpacity(FSlateColor(bEdge ? WithAlpha(ToneColour(Tone), 0.85f) : FLinearColor::Transparent))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(FMargin(20.f, 16.f, 22.f, 18.f))
				[
					Row
				]
			];
	}

	default:   // Lead
		return SNew(STextBlock)
			.Text(FText::FromString(Block.Text))
			.Font(Sans(17))
			.LineHeightPercentage(1.25f)
			.ColorAndOpacity(FSlateColor(Block.Tone == ETone::Dim ? TextDim : TextBody))
			.AutoWrapText(true);
	}
}

TSharedRef<SWidget> SInvestigationScreen::MakeHeader()
{
	using namespace InvestigationScreenPrivate;
	const FCaseContent* Case = GetCase();
	const FString CaseName = Case ? Case->Title.ToString() : FString();

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Bottom)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("MAJOR CRIME REVIEW  \u00B7  CASE FILE")))
				.Font(Caps(12))
				.ColorAndOpacity(FSlateColor(Amber))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(Sans(32, TEXT("Light")))
				.ColorAndOpacity(FSlateColor(TextMain))
				.AutoWrapText(true)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Bottom)
		.Padding(24.f, 0.f, 0.f, 4.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Visibility(CaseName.IsEmpty() || Page == EPage::Home ? EVisibility::Collapsed : EVisibility::Visible)
				.Text(FText::FromString(CaseName.ToUpper()))
				.Font(Caps(11))
				.ColorAndOpacity(FSlateColor(TextDim))
			]
		];
}

TSharedRef<SWidget> SInvestigationScreen::MakeFooter()
{
	using namespace InvestigationScreenPrivate;
	const TSharedRef<SHorizontalBox> Hints = SNew(SHorizontalBox);
	auto Hint = [&Hints](const TCHAR* Action, const TCHAR* Keys)
	{
		Hints->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Action))
				.Font(Caps(11))
				.ColorAndOpacity(FSlateColor(TextDim))
			];
		Hints->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 30.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(KeyBrush())
				.Padding(FMargin(9.f, 4.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Keys))
					.Font(Sans(13))
					.ColorAndOpacity(FSlateColor(TextMain))
				]
			];
	};
	Hint(TEXT("MOVE"), TEXT("arrows / d-pad"));
	Hint(TEXT("CHOOSE"), TEXT("Enter / A"));
	Hint(TEXT("BACK"), TEXT("Esc / B"));
	Hint(TEXT("SCROLL FILE"), TEXT("PgUp PgDn / LB RB"));
	return Hints;
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
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				Button
			];

		if (!FirstButton.IsValid())
		{
			FirstButton = Button;
		}
	}

	// The opening film fills the whole screen, with its choices tucked into a corner.
	if (IsFullScreenFilm() && MediaPlayback.IsValid() && MediaPlayback->GetPicture())
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
						.WidthOverride(220.f)
						[
							Menu
						]
					]
				]
			]
		];

		FirstButtonWeak = FirstButton;
		ContentScrollWeak.Reset();
		if (FirstButton.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(FirstButton, EFocusCause::SetDirectly);
		}
		return;
	}

	// The case-file side: notices first, then the page's parts, top to bottom.
	const TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	for (const FString& Message : Notices)
	{
		Content->AddSlot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(SBorder)
				.BorderImage(ToneBrush(2))
				.Padding(FMargin(16.f, 10.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Message))
					.Font(Sans(16))
					.ColorAndOpacity(FSlateColor(TextMain))
					.AutoWrapText(true)
				]
			];
	}
	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		const FBlock& Block = Blocks[Index];
		// Sections get more air above them than the parts inside a section get between each other.
		const float Above = (Index == 0 && Notices.Num() == 0) ? 0.f : (Block.Kind == EBlock::Section ? 26.f : 14.f);
		Content->AddSlot()
			.AutoHeight()
			.Padding(0.f, Above, 0.f, 0.f)
			[
				MakeBlock(Block)
			];
	}

	const TSharedRef<SScrollBox> ContentScroll = SNew(SScrollBox)
		.Style(QuietScrollStyle())
		+ SScrollBox::Slot()
		.Padding(FMargin(0.f, 0.f, 20.f, 24.f))   // keeps cards clear of the scroll bar
		[
			SNew(SBox)
			.MaxDesiredWidth(ContentMaxWidth)
			.HAlign(HAlign_Fill)
			[
				Content
			]
		];
	ContentScrollWeak = ContentScroll;

	// A new page eases in; acting on the same page (asking another question) just updates it.
	if (Page != LastShownPage || PageShownAt == 0.0)
	{
		PageShownAt = FPlatformTime::Seconds();
	}
	LastShownPage = Page;

	ChildSlot
	[
		SNew(SOverlay)

		// Background: the office at night, blurred and kept well down so it never competes with the file.
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
					.ColorAndOpacity(FLinearColor(0.42f, 0.45f, 0.5f, 1.f))
				]
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(ShadeBrush())
		]

		+ SOverlay::Slot()
		.Padding(FMargin(56.f, 34.f, 56.f, 26.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeHeader()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 18.f, 0.f, 22.f)
			[
				MakeRule(LineBrush())
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SHorizontalBox)

				// The choices.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(400.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2.f, 0.f, 0.f, 12.f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(GetMenuHeading().ToUpper()))
							.Font(Caps(11))
							.ColorAndOpacity(FSlateColor(TextDim))
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SNew(SScrollBox)
							.Style(QuietScrollStyle())
							.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
							+ SScrollBox::Slot()
							.Padding(FMargin(0.f, 0.f, 10.f, 0.f))
							[
								Menu
							]
						]
					]
				]

				// The file.
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(40.f, 0.f, 0.f, 0.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
					.Padding(FMargin(0.f))
					.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, GetPageFade()); })
					.RenderTransform_Lambda([this]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(0.f, (1.f - GetPageFade()) * 10.f))); })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 12.f)
						[
							// Lines up the top of the file with the first button, under the list's own heading.
							SNew(SBox).HeightOverride(14.f)
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							ContentScroll
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 16.f, 0.f, 12.f)
			[
				MakeRule(LineBrush())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeFooter()
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
	if (What == TEXT("Review")) { GoTo(EPage::Review); return true; }
	if (What == TEXT("SaveLoad")) { GoTo(EPage::SaveLoad); return true; }
	if (What == TEXT("Cases")) { GoTo(EPage::Cases); return true; }
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
			const FExchange& Last = Transcript.Last();
			UE_LOG(LogInvestigation, Display, TEXT("%s.  /  %s: \"%s\"%s"), *Last.Asked, *Last.Speaker, *Last.Line,
				Last.bEnded ? TEXT("  /  (interview ended)") : TEXT(""));   // one log line, so it can be searched
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
		TEXT("Development only. ColdCase.UI <Home|Exhibits|Exhibit|Statements|People|Interview|Theory|Review|CaseReview|SaveLoad|Cases|Clip|Ask|Present> [Id]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const TSharedPtr<SInvestigationScreen> Screen = SInvestigationScreen::GetActive();
			const bool bDone = Screen.IsValid() && Screen->DevCommand(Args);
			UE_LOG(LogInvestigation, Display, TEXT("ColdCase.UI %s: %s"), *FString::Join(Args, TEXT(" ")), bDone ? TEXT("done") : TEXT("not possible here"));
		}));
}
#endif

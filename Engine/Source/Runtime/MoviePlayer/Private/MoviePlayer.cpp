// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "MoviePlayer.h"

#include "Engine.h"
#include "Slate.h"
#include "SpinLock.h"
#include "NullMoviePlayer.h"
#include "DefaultGameMoviePlayer.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MoviePlayer);


/** A very simple loading screen sample test widget to use */
class SLoadingScreenTestWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLoadingScreenTestWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SThrobber)
				.Visibility(this, &SLoadingScreenTestWidget::GetLoadIndicatorVisibility)
			]
			+SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("MoviePlayerTestLoadingScreen", "LoadingComplete", "Loading complete!"))
				.Visibility(this, &SLoadingScreenTestWidget::GetMessageIndicatorVisibility)
			]
		];
	}

private:
	EVisibility GetLoadIndicatorVisibility() const
	{
		return GetMoviePlayer()->IsLoadingFinished() ? EVisibility::Collapsed : EVisibility::Visible;
	}
	
	EVisibility GetMessageIndicatorVisibility() const
	{
		return GetMoviePlayer()->IsLoadingFinished() ? EVisibility::Visible : EVisibility::Collapsed;
	}
};


bool FLoadingScreenAttributes::IsValid() const {return WidgetLoadingScreen.IsValid() || MoviePaths.Num() > 0;}

TSharedRef<SWidget> FLoadingScreenAttributes::NewTestLoadingScreenWidget()
{
	return SNew(SLoadingScreenTestWidget);
}


TSharedPtr<IGameMoviePlayer> GetMoviePlayer()
{
	if (!IsMoviePlayerEnabled() || GUsingNullRHI )
	{
		return FNullGameMoviePlayer::Get();
	}
	else
	{
		return FDefaultGameMoviePlayer::Get();
	}
}

bool IsMoviePlayerEnabled()
{
	return !GIsEditor && !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

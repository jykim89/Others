// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SSessionFrontend.cpp: Implements the SSessionFrontend class.
=============================================================================*/

#include "SessionFrontendPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SSessionFrontend"


/* Local constants
 *****************************************************************************/

static const FName AutomationTabId("AutomationPanel");
static const FName SessionBrowserTabId("SessionBrowser");
static const FName SessionConsoleTabId("SessionConsole");
static const FName SessionScreenTabId("ScreenComparison");
static const FName ProfilerTabId("Profiler");


/* SSessionFrontend interface
 *****************************************************************************/

void SSessionFrontend::Construct( const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow )
{
	InitializeControllers();

	// create & initialize tab manager
	TabManager = FGlobalTabmanager::Get()->NewTabManager(ConstructUnderMajorTab);

	TSharedRef<FWorkspaceItem> RootMenuGroup = FWorkspaceItem::NewGroup(LOCTEXT("RootMenuGroupName", "Root"));
	TSharedRef<FWorkspaceItem> AppMenuGroup = RootMenuGroup->AddGroup(LOCTEXT("SessionFrontendMenuGroupName", "Session Frontend"));
	
	TabManager->RegisterTabSpawner(AutomationTabId, FOnSpawnTab::CreateRaw(this, &SSessionFrontend::HandleTabManagerSpawnTab, AutomationTabId))
		.SetDisplayName(LOCTEXT("AutomationTabTitle", "Automation"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "SessionFrontEnd.Tabs.Tools"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(SessionBrowserTabId, FOnSpawnTab::CreateRaw(this, &SSessionFrontend::HandleTabManagerSpawnTab, SessionBrowserTabId))
		.SetDisplayName(LOCTEXT("SessionBrowserTitle", "Session Browser"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "SessionFrontEnd.Tabs.Tools"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(SessionConsoleTabId, FOnSpawnTab::CreateRaw(this, &SSessionFrontend::HandleTabManagerSpawnTab, SessionConsoleTabId))
		.SetDisplayName(LOCTEXT("ConsoleTabTitle", "Console"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "SessionFrontEnd.Tabs.Tools"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(SessionScreenTabId, FOnSpawnTab::CreateRaw(this, &SSessionFrontend::HandleTabManagerSpawnTab, SessionScreenTabId))
		.SetDisplayName(LOCTEXT("ScreenTabTitle", "Screen Comparison"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "SessionFrontEnd.Tabs.Tools"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(ProfilerTabId, FOnSpawnTab::CreateRaw(this, &SSessionFrontend::HandleTabManagerSpawnTab, ProfilerTabId))
		.SetDisplayName(LOCTEXT("ProfilerTabTitle", "Profiler"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Tab"))
		.SetGroup(AppMenuGroup);
	
	// create tab layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("SessionFrontendLayout_v1.1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					// session browser
					FTabManager::NewStack()
						->AddTab(SessionBrowserTabId, ETabState::OpenedTab)
						->SetHideTabWell(true)
						->SetSizeCoefficient(0.15f)
				)
				->Split
				(
					// applications
					FTabManager::NewStack()
						->AddTab(SessionConsoleTabId, ETabState::OpenedTab)
						->AddTab(AutomationTabId, ETabState::OpenedTab)
						->AddTab(SessionScreenTabId, ETabState::ClosedTab)
						->AddTab(ProfilerTabId, ETabState::OpenedTab)
						->SetSizeCoefficient(0.85f)
						->SetForegroundTab(SessionConsoleTabId)
				)							
		);

	// create & initialize main menu
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());

	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateStatic(&SSessionFrontend::FillWindowMenu, RootMenuGroup, AppMenuGroup, TabManager),
		"Window"
	);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MenuBarBuilder.MakeWidget()
			]
	
		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				TabManager->RestoreFrom(Layout, ConstructUnderWindow).ToSharedRef()
			]
	];
}


/* SSessionFrontend implementation
 *****************************************************************************/

void SSessionFrontend::FillWindowMenu( FMenuBuilder& MenuBuilder, TSharedRef<FWorkspaceItem> RootMenuGroup, TSharedRef<FWorkspaceItem> AppMenuGroup, const TSharedPtr<FTabManager> TabManager )
{
	if (!TabManager.IsValid())
	{
		return;
	}

#if !WITH_EDITOR
	MenuBuilder.BeginSection("WindowGlobalTabSpawners", LOCTEXT("UfeMenuGroup", "Unreal Frontend"));
	{
		FGlobalTabmanager::Get()->PopulateTabSpawnerMenu(MenuBuilder, RootMenuGroup);
	}
	MenuBuilder.EndSection();
#endif //!WITH_EDITOR

	MenuBuilder.BeginSection("WindowLocalTabSpawners", LOCTEXT("SessionFrontendMenuGroup", "Session Frontend"));
	{
		TabManager->PopulateTabSpawnerMenu(MenuBuilder, AppMenuGroup);
	}
	MenuBuilder.EndSection();
}


void SSessionFrontend::InitializeControllers( )
{
	// load required modules and objects
	ISessionServicesModule& SessionServicesModule = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices");
	ITargetDeviceServicesModule& TargetDeviceServicesModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>("TargetDeviceServices");
	IScreenShotToolsModule& ScreenShotModule = FModuleManager::LoadModuleChecked<IScreenShotToolsModule>("ScreenShotComparisonTools");

	// create controllers
	DeviceProxyManager = TargetDeviceServicesModule.GetDeviceProxyManager();
	SessionManager = SessionServicesModule.GetSessionManager();
	ScreenShotManager = ScreenShotModule.GetScreenShotManager();
}


/* SSessionFrontend callbacks
 *****************************************************************************/

void SSessionFrontend::HandleAutomationModuleShutdown()
{
	IAutomationWindowModule& AutomationWindowModule = FModuleManager::LoadModuleChecked<IAutomationWindowModule>("AutomationWindow");
	TSharedPtr<SDockTab> AutomationWindowModuleTab = AutomationWindowModule.GetAutomationWindowTab().Pin();
	if (AutomationWindowModuleTab.IsValid())
	{
		AutomationWindowModuleTab->RequestCloseTab();
	}
}


TSharedRef<SDockTab> SSessionFrontend::HandleTabManagerSpawnTab( const FSpawnTabArgs& Args, FName TabIdentifier ) const
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::PanelTab);

	if (TabIdentifier == AutomationTabId)
	{
		// create a controller every time a tab is created
		IAutomationControllerModule& AutomationControllerModule = FModuleManager::LoadModuleChecked<IAutomationControllerModule>(TEXT("AutomationController"));
		IAutomationControllerManagerPtr AutomationController = AutomationControllerModule.GetAutomationController();
		IAutomationWindowModule& AutomationWindowModule = FModuleManager::LoadModuleChecked<IAutomationWindowModule>("AutomationWindow");

		AutomationController->OnShutdown().BindRaw(this, &SSessionFrontend::HandleAutomationModuleShutdown);

		TabWidget = AutomationWindowModule.CreateAutomationWindow(
			AutomationController.ToSharedRef(),
			SessionManager.ToSharedRef()
		);

		AutomationWindowModule.OnShutdown().BindRaw(this, &SSessionFrontend::HandleAutomationModuleShutdown);
	}
	else if (TabIdentifier == ProfilerTabId)
	{
		IProfilerModule& ProfilerModule = FModuleManager::LoadModuleChecked<IProfilerModule>(TEXT("Profiler"));
		TabWidget = ProfilerModule.CreateProfilerWindow(SessionManager.ToSharedRef(), DockTab);
	}
	else if (TabIdentifier == SessionBrowserTabId)
	{
		TabWidget = SNew(SSessionBrowser, SessionManager.ToSharedRef());
	}
	else if (TabIdentifier == SessionConsoleTabId)
	{
		TabWidget = SNew(SSessionConsole, SessionManager.ToSharedRef());
	}
	else if (TabIdentifier == SessionScreenTabId)
	{
		TabWidget = FModuleManager::LoadModuleChecked<IScreenShotComparisonModule>("ScreenShotComparison").CreateScreenShotComparison(
			ScreenShotManager.ToSharedRef()
		);
	}

	DockTab->SetContent(TabWidget.ToSharedRef());

	// save the Automation Window Dock Tab so that we can close it on required module being shutdown or recompiled.
	if (TabIdentifier == AutomationTabId)
	{
		FModuleManager::LoadModuleChecked<IAutomationWindowModule>("AutomationWindow").SetAutomationWindowTab(DockTab);
	}

	return DockTab;
}


#undef LOCTEXT_NAMESPACE

// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "WorldBrowserPrivatePCH.h"
#include "STiledLandscapeImportDlg.h"
#include "SVectorInputBox.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

static int32 CalcLandscapeSquareResolution(int32 ComponetsNumX, int32 SectionNumX, int32 SectionQuadsNumX)
{
	return ComponetsNumX*SectionNumX*SectionQuadsNumX+1;
}

/**
 *	Returns heightmap tile coordinates extracted from a specified tile filename
 */
static FIntPoint ExtractHeighmapTileCoordinates(FString BaseFilename)
{
	//We expect file name in form: <tilename>_x<number>_y<number>
	FIntPoint ResultPosition(-1,-1);
	
	int32 XPos = BaseFilename.Find(TEXT("_x"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 YPos = BaseFilename.Find(TEXT("_y"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (XPos != INDEX_NONE && YPos != INDEX_NONE && XPos < YPos)
	{
		FString XCoord = BaseFilename.Mid(XPos+2, YPos-(XPos+2));
		FString YCoord = BaseFilename.Mid(YPos+2, BaseFilename.Len()-(YPos+2));

		if (XCoord.IsNumeric() && YCoord.IsNumeric())
		{
			TTypeFromString<int32>::FromString(ResultPosition.X, *XCoord);
			TTypeFromString<int32>::FromString(ResultPosition.Y, *YCoord);
		}
	}

	return ResultPosition;
}

void STiledLandcapeImportDlg::Construct(const FArguments& InArgs, TSharedPtr<SWindow> InParentWindow)
{
	ParentWindow = InParentWindow;
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.FillHeight(1)
			.Padding(0,10,0,10)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				
				// Select tiles
				+SUniformGridPanel::Slot(0, 0)

				+SUniformGridPanel::Slot(1, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &STiledLandcapeImportDlg::OnClickedSelectHeightmapTiles)
					.Text(LOCTEXT("TiledLandscapeImport_SelectButtonText", "Select Heightmap Tiles..."))
				]
				
				// Tiles origin offset
				+SUniformGridPanel::Slot(0, 1)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ToolTipText(LOCTEXT("TiledLandscapeImport_TilesOffsetTooltip", "For example: tile x0_y0 will be treated as x(0+offsetX)_y(0+offsetY)"))
					.Text(LOCTEXT("TiledLandscapeImport_TilesOffsetText", "Tile Coordinates Offset"))
				]

				+SUniformGridPanel::Slot(1, 1)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					
					+SHorizontalBox::Slot()
					.Padding(0.0f, 1.0f, 2.0f, 1.0f)
					.FillWidth(1.f)
					[
						SNew(SNumericEntryBox<int32>)
						.Value(this, &STiledLandcapeImportDlg::GetTileOffsetX)
						.OnValueChanged(this, &STiledLandcapeImportDlg::SetTileOffsetX)
						.LabelPadding(0)
						.Label()
						[
							SNumericEntryBox<int>::BuildLabel( LOCTEXT("X_Label", "X"), FLinearColor::White, SNumericEntryBox<int>::RedLabelBackgroundColor )
						]
					]
					
					+SHorizontalBox::Slot()
					.Padding(0.0f, 1.0f, 2.0f, 1.0f)
					.FillWidth(1.f)
					[
						SNew(SNumericEntryBox<int32>)
						.Value(this, &STiledLandcapeImportDlg::GetTileOffsetY)
						.OnValueChanged(this, &STiledLandcapeImportDlg::SetTileOffsetY)
						.LabelPadding(0)
						.Label()
						[
							SNumericEntryBox<float>::BuildLabel( LOCTEXT("Y_Label", "Y"), FLinearColor::White, SNumericEntryBox<int>::GreenLabelBackgroundColor )
						]
					]
				]

				// Tile configuration
				+SUniformGridPanel::Slot(0, 2)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TiledLandscapeImport_ConfigurationText", "Import Configuration"))
				]

				+SUniformGridPanel::Slot(1, 2)
				.VAlign(VAlign_Center)
				[
					SAssignNew(TileConfigurationComboBox, SComboBox<TSharedPtr<FTileImportConfiguration>>)
					.OptionsSource(&ActiveConfigurations)
					.OnSelectionChanged(this, &STiledLandcapeImportDlg::OnSetImportConfiguration)
					.OnGenerateWidget(this, &STiledLandcapeImportDlg::HandleTileConfigurationComboBoxGenarateWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text(this, &STiledLandcapeImportDlg::GetTileConfigurationText)
					]
				]

				// Scale
				+SUniformGridPanel::Slot(0, 3)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TiledLandscapeImport_ScaleText", "Landscape Scale"))
				]
			
				+SUniformGridPanel::Slot(1, 3)
				.VAlign(VAlign_Center)
				[
					SNew( SVectorInputBox )
					.bColorAxisLabels( true )
					.X( this, &STiledLandcapeImportDlg::GetScaleX )
					.Y( this, &STiledLandcapeImportDlg::GetScaleY )
					.Z( this, &STiledLandcapeImportDlg::GetScaleZ )
					.OnXCommitted( this, &STiledLandcapeImportDlg::OnSetScale, 0 )
					.OnYCommitted( this, &STiledLandcapeImportDlg::OnSetScale, 1 )
					.OnZCommitted( this, &STiledLandcapeImportDlg::OnSetScale, 2 )
				]

				// Landcape material
				+SUniformGridPanel::Slot(0, 4)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TiledLandscapeImport_MaterialText", "Material"))
				]
			
				+SUniformGridPanel::Slot(1, 4)
				.VAlign(VAlign_Center)
				[
					SAssignNew(LandscapeMaterialComboButton, SComboButton)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.ComboButtonStyle(FEditorStyle::Get(), "ToolbarComboButton")
					.ForegroundColor(FLinearColor::White)
					.ContentPadding(3)
					.MenuPlacement(EMenuPlacement::MenuPlacement_BelowAnchor)
					.OnGetMenuContent(this, &STiledLandcapeImportDlg::CreateLandscapeMaterialPicker)
					.ButtonContent()
					[
						SNew( STextBlock )
						.Text(this, &STiledLandcapeImportDlg::GetLandscapeMaterialName)
					]
				]
			]

			// Layers
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
			[
				SAssignNew(LayerDataListView, SListView<TSharedPtr<FLandscapeImportLayerData>>)
				.ListItemsSource( &LayerDataList )
				.OnGenerateRow( this, &STiledLandcapeImportDlg::OnGenerateWidgetForLayerDataListView )
				.SelectionMode(ESelectionMode::None)
			]

			// Import summary
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
			[
				SNew(STextBlock)
				.Text(this, &STiledLandcapeImportDlg::GetImportSummaryText)
			]

			// Import, Cancel
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(0,10,0,10)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.IsEnabled(this, &STiledLandcapeImportDlg::IsImportEnabled)
					.OnClicked(this, &STiledLandcapeImportDlg::OnClickedImport)
					.Text(LOCTEXT("TiledLandscapeImport_ImportButtonText", "Import"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &STiledLandcapeImportDlg::OnClickedCancel)
					.Text(LOCTEXT("TiledLandscapeImport_CancelButtonText", "Cancel"))
				]
			]
		]
	];


	GenerateAllPossibleTileConfigurations();
	SetPossibleConfigurationsForResolution(0);
}

TSharedRef<SWidget> STiledLandcapeImportDlg::CreateLandscapeMaterialPicker()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(UMaterial::StaticClass()->GetFName());
	AssetPickerConfig.Filter.ClassNames.Add(UMaterialInstance::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &STiledLandcapeImportDlg::OnLandscapeMaterialChanged);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.ThumbnailScale = 0.0f;
	
	return SNew(SBox)
		.WidthOverride(250)
		.HeightOverride(300)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

FText STiledLandcapeImportDlg::GetLandscapeMaterialName() const
{
	if (ImportSettings.LandscapeMaterial.IsValid())
	{
		return FText::FromString(ImportSettings.LandscapeMaterial->GetName());
	}

	return LOCTEXT("TiledLandscapeImport_NoLandscapeMaterialText", "None");
}

TSharedRef<SWidget> STiledLandcapeImportDlg::HandleTileConfigurationComboBoxGenarateWidget(TSharedPtr<FTileImportConfiguration> InItem) const
{
	const FText ItemText = GenerateConfigurationText(InItem->NumComponents, InItem->NumSectionsPerComponent, InItem->NumQuadsPerSection);
		
	return SNew(SBox)
	.Padding(4)
	[
		SNew(STextBlock).Text(ItemText)
	];
}

FText STiledLandcapeImportDlg::GetTileConfigurationText() const
{
	if (ImportSettings.HeightmapFileList.Num() == 0)
	{
		return LOCTEXT("TiledLandscapeImport_NoTilesText", "No tiles selected");
	}

	if (ImportSettings.SectionsPerComponent <= 0)
	{
		return LOCTEXT("TiledLandscapeImport_InvalidTileResolutuinText", "Selected tiles have unsupported resolutuion");
	}
	
	return GenerateConfigurationText(ImportSettings.ComponentsNum, ImportSettings.SectionsPerComponent, ImportSettings.QuadsPerSection);
}

TSharedRef<ITableRow> STiledLandcapeImportDlg::OnGenerateWidgetForLayerDataListView(TSharedPtr<FLandscapeImportLayerData> InLayerData, const TSharedRef<STableViewBase>& OwnerTable)
{
	return 	SNew(STableRow< TSharedRef<FLandscapeImportLayerData> >, OwnerTable)
			[
				SNew(SBorder)
				[
					SNew(SHorizontalBox)
				
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.FillWidth(1)
					[
						SNew(STextBlock).Text(InLayerData->LayerName)
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding(2)
					.AutoWidth()
					[
						SNew(STextBlock).Text(this, &STiledLandcapeImportDlg::GetWeightmapCountText, InLayerData)
					]
					
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &STiledLandcapeImportDlg::OnClickedSelectWeighmapTiles, InLayerData)
						.Text(LOCTEXT("TiledLandscapeImport_SelectWeightmapButtonText", "Select Weightmap Tiles..."))
					]
				]
			];
	
}

const FTiledLandscapeImportSettings& STiledLandcapeImportDlg::GetImportSettings() const
{
	return ImportSettings;
}

TOptional<float> STiledLandcapeImportDlg::GetScaleX() const
{
	return ImportSettings.Scale3D.X;
}

TOptional<float> STiledLandcapeImportDlg::GetScaleY() const
{
	return ImportSettings.Scale3D.Y;
}

TOptional<float> STiledLandcapeImportDlg::GetScaleZ() const
{
	return ImportSettings.Scale3D.Z;
}

void STiledLandcapeImportDlg::OnSetScale(float InValue, ETextCommit::Type CommitType, int32 InAxis)
{
	if (InAxis < 2) //XY uniform
	{
		ImportSettings.Scale3D.X = FMath::Abs(InValue);
		ImportSettings.Scale3D.Y = FMath::Abs(InValue);
	}
	else //Z
	{
		ImportSettings.Scale3D.Z = FMath::Abs(InValue);
	}
}

TOptional<int32> STiledLandcapeImportDlg::GetTileOffsetX() const
{
	return ImportSettings.TilesCoordinatesOffset.X;
}

void STiledLandcapeImportDlg::SetTileOffsetX(int32 InValue)
{
	ImportSettings.TilesCoordinatesOffset.X = InValue;
}

TOptional<int32> STiledLandcapeImportDlg::GetTileOffsetY() const
{
	return ImportSettings.TilesCoordinatesOffset.Y;
}

void STiledLandcapeImportDlg::SetTileOffsetY(int32 InValue)
{
	ImportSettings.TilesCoordinatesOffset.Y = InValue;
}

void STiledLandcapeImportDlg::OnSetImportConfiguration(TSharedPtr<FTileImportConfiguration> InTileConfig, ESelectInfo::Type SelectInfo)
{
	if (InTileConfig.IsValid())
	{
		ImportSettings.ComponentsNum = InTileConfig->NumComponents;
		ImportSettings.QuadsPerSection = InTileConfig->NumQuadsPerSection;
		ImportSettings.SectionsPerComponent = InTileConfig->NumSectionsPerComponent;
	}
	else
	{
		ImportSettings.ComponentsNum = 0;
		ImportSettings.HeightmapFileList.Empty();
	}
}

FReply STiledLandcapeImportDlg::OnClickedSelectHeightmapTiles()
{
	TotalLandscapeRect = FIntRect(MAX_int32, MAX_int32, MIN_int32, MIN_int32);
	ImportSettings.HeightmapFileList.Empty();
	ImportSettings.TileCoordinates.Empty();
	
	SetPossibleConfigurationsForResolution(0);
	
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		if (ParentWindow->GetNativeWindow().IsValid())
		{
			void* ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();

			const FString ImportFileTypes = TEXT("Raw heightmap tiles (*.r16)|*.r16");
			bool bOpened = DesktopPlatform->OpenFileDialog(
								ParentWindowWindowHandle,
								LOCTEXT("SelectHeightmapTiles", "Select heightmap tiles").ToString(),
								*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::UNR),
								TEXT(""),
								*ImportFileTypes,
								EFileDialogFlags::Multiple,
								ImportSettings.HeightmapFileList);

			if (bOpened	&& ImportSettings.HeightmapFileList.Num())
			{
				IFileManager& FileManager = IFileManager::Get();
				bool bValidTiles = true;

				// All heightmap tiles have to be the same size and have correct tile position encoded into filename
				const int64 TargetFileSize = FileManager.FileSize(*ImportSettings.HeightmapFileList[0]);
				for (const FString& Filename : ImportSettings.HeightmapFileList)
				{
					int64 FileSize = FileManager.FileSize(*Filename);
					if (FileSize != TargetFileSize)
					{
						bValidTiles = false;
						break;
					}

					FIntPoint TileCoordinate = ExtractHeighmapTileCoordinates(FPaths::GetBaseFilename(Filename));
					if (TileCoordinate.GetMin() < 0)
					{
						bValidTiles = false;
						break;
					}

					TotalLandscapeRect.Include(TileCoordinate);
					ImportSettings.TileCoordinates.Add(TileCoordinate);
					
				}
				
				if (bValidTiles)
				{
					ImportSettings.TileResolution = FMath::Sqrt(TargetFileSize/2.0);
					SetPossibleConfigurationsForResolution(ImportSettings.TileResolution);
				}
			}
		}
	}
		
	return FReply::Handled();
}

FReply STiledLandcapeImportDlg::OnClickedSelectWeighmapTiles(TSharedPtr<FLandscapeImportLayerData> InLayerData)
{
	InLayerData->WeighmapFileList.Empty();

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		if (ParentWindow->GetNativeWindow().IsValid())
		{
			void* ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();

			const FString ImportFileTypes = TEXT("Raw weigtmap tiles (*.raw)|*.raw");
			bool bOpened = DesktopPlatform->OpenFileDialog(
								ParentWindowWindowHandle,
								LOCTEXT("SelectHeightmapTiles", "Select weightmap tiles").ToString(),
								*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::UNR),
								TEXT(""),
								*ImportFileTypes,
								EFileDialogFlags::Multiple,
								InLayerData->WeighmapFileList);

			if (bOpened	&& InLayerData->WeighmapFileList.Num())
			{
				// TODO: check if it's a valid weightmaps
			}
		}
	}
	
	return FReply::Handled();
}

bool STiledLandcapeImportDlg::IsImportEnabled() const
{
	return ImportSettings.HeightmapFileList.Num() && ImportSettings.ComponentsNum > 0;
}

FReply STiledLandcapeImportDlg::OnClickedImport()
{
	// copy weightmaps list data to an import structure  
	ImportSettings.WeightmapFileList.Empty();
	ImportSettings.WeightmapFileList.SetNum(ImportSettings.LandscapeLayerNameList.Num());
	check(ImportSettings.WeightmapFileList.Num() == LayerDataList.Num());
	for (int32 LayerIdx = 0; LayerIdx < LayerDataList.Num(); ++LayerIdx)
	{
		ImportSettings.WeightmapFileList[LayerIdx] = LayerDataList[LayerIdx]->WeighmapFileList;
		
		// we assume weightmaps tiles will match heightmap tiles 
		ImportSettings.WeightmapFileList[LayerIdx].Sort();
	}

	ImportSettings.HeightmapFileList.Sort();
			
	ParentWindow->RequestDestroyWindow();
	return FReply::Handled();	
}

FReply STiledLandcapeImportDlg::OnClickedCancel()
{
	ParentWindow->RequestDestroyWindow();
	
	ImportSettings.HeightmapFileList.Empty();
	return FReply::Handled();	
}

void STiledLandcapeImportDlg::OnLandscapeMaterialChanged(const FAssetData& AssetData)
{
	ImportSettings.LandscapeMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
	LandscapeMaterialComboButton->SetIsOpen(false);

	// pull landscape layers from a chosen material
	UpdateLandscapeLayerList();
}

void STiledLandcapeImportDlg::SetPossibleConfigurationsForResolution(int32 TargetResolutuion)
{
	int32 Idx = AllConfigurations.IndexOfByPredicate([&](const FTileImportConfiguration& A){
		return TargetResolutuion == A.Resolutuion;
	});

	ActiveConfigurations.Empty();
	ImportSettings.ComponentsNum = 0; // Set invalid options

	// AllConfigurations is sorted by resolution
	while(AllConfigurations.IsValidIndex(Idx) && AllConfigurations[Idx].Resolutuion == TargetResolutuion)
	{
		TSharedPtr<FTileImportConfiguration> TileConfig = MakeShareable(new FTileImportConfiguration(AllConfigurations[Idx++]));
		ActiveConfigurations.Add(TileConfig);
	}

	// Refresh combo box with active configurations
	TileConfigurationComboBox->RefreshOptions();
	// Set first configurationion as active
	if (ActiveConfigurations.Num())
	{
		TileConfigurationComboBox->SetSelectedItem(ActiveConfigurations[0]);
	}
}

void STiledLandcapeImportDlg::GenerateAllPossibleTileConfigurations()
{
	AllConfigurations.Empty();
	for (int32 ComponentsNum = 1; ComponentsNum <= 32; ComponentsNum++)
	{
		for (int32 SectionsPerComponent = 1; SectionsPerComponent <= 2; SectionsPerComponent++)
		{
			for (int32 QuadsPerSection = 3; QuadsPerSection <= 8; QuadsPerSection++)
			{
				FTileImportConfiguration Entry;
				Entry.NumComponents				= ComponentsNum;
				Entry.NumSectionsPerComponent	= SectionsPerComponent;
				Entry.NumQuadsPerSection		= (1 << QuadsPerSection) - 1;
				Entry.Resolutuion				= CalcLandscapeSquareResolution(Entry.NumComponents, Entry.NumSectionsPerComponent, Entry.NumQuadsPerSection);

				AllConfigurations.Add(Entry);
			}
		}
	}
	
	// Sort by resolution
	AllConfigurations.Sort([](const FTileImportConfiguration& A, const FTileImportConfiguration& B){
		if (A.Resolutuion == B.Resolutuion)
		{
			return A.NumComponents < B.NumComponents;
		}
		return A.Resolutuion < B.Resolutuion;
	});
}

FText STiledLandcapeImportDlg::GetImportSummaryText() const
{
	if (ImportSettings.HeightmapFileList.Num() == 0 || ImportSettings.ComponentsNum <= 0)
	{
		return FText();
	}
	
	// Tile information(num, resolution)
	const FString TilesSummary = FString::Printf(TEXT("%d - %dx%d"), ImportSettings.HeightmapFileList.Num(), ImportSettings.TileResolution, ImportSettings.TileResolution);
	
	// Total landscape size(NxN km)
	int32 WidthInTilesX = TotalLandscapeRect.Width() + 1;
	int32 WidthInTilesY = TotalLandscapeRect.Height() + 1;
	float WidthX = 0.00001f*ImportSettings.Scale3D.X*WidthInTilesX*ImportSettings.TileResolution;
	float WidthY = 0.00001f*ImportSettings.Scale3D.Y*WidthInTilesY*ImportSettings.TileResolution;
	const FString LandscapeSummary = FString::Printf(TEXT("%.3fx%.3f"), WidthX, WidthY);
	
	return FText::Format(
		LOCTEXT("TiledLandscapeImport_SummaryText", "{0} tiles, {1}km landscape"), 
		FText::FromString(TilesSummary),
		FText::FromString(LandscapeSummary)
		);
}

FText STiledLandcapeImportDlg::GetWeightmapCountText(TSharedPtr<FLandscapeImportLayerData> InLayerData) const
{
	int32 NumWeighmaps = InLayerData.IsValid() ? InLayerData->WeighmapFileList.Num() : 0;
	return FText::AsNumber(NumWeighmaps);
}

FText STiledLandcapeImportDlg::GenerateConfigurationText(int32 NumComponents, int32 NumSectionsPerComponent, int32 NumQuadsPerSection) const
{
	const FString ComponentsStr = FString::Printf(TEXT("%dx%d"), NumComponents, NumComponents);
	const FString SectionsStr = FString::Printf(TEXT("%dx%d"), NumSectionsPerComponent, NumSectionsPerComponent);
	const FString QuadsStr = FString::Printf(TEXT("%dx%d"), NumQuadsPerSection, NumQuadsPerSection);
	
	return FText::Format(
		LOCTEXT("TiledLandscapeImport_ConfigurationText", "Components: {0} Sections: {1} Quads: {2}"), 
		FText::FromString(ComponentsStr),
		FText::FromString(SectionsStr),
		FText::FromString(QuadsStr)
		);
}

void STiledLandcapeImportDlg::UpdateLandscapeLayerList()
{
	ImportSettings.LandscapeLayerNameList = ALandscapeProxy::GetLayersFromMaterial(ImportSettings.LandscapeMaterial.Get());
	//
	ImportSettings.WeightmapFileList.Empty();
	ImportSettings.WeightmapFileList.SetNum(ImportSettings.LandscapeLayerNameList.Num());

	//	
	LayerDataList.Empty();
	for (FName LayerName  : ImportSettings.LandscapeLayerNameList)
	{
		TSharedPtr<FLandscapeImportLayerData> LayerData = MakeShareable(new FLandscapeImportLayerData());
		LayerData->LayerName = LayerName.ToString();
		LayerData->bBlend = true;

		LayerDataList.Add(LayerData);
	}

	LayerDataListView->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE

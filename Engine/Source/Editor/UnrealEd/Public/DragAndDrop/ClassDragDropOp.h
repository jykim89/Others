// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

class FClassDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FClassDragDropOp, FDragDropOperation)

	/** The classes to be dropped. */
	TArray< TWeakObjectPtr<UClass> > ClassesToDrop;
	
	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const OVERRIDE
	{
		// Just use the first class for the cursor decorator.
		const FSlateBrush* ClassIcon = FEditorStyle::GetOptionalBrush(*FString::Printf( TEXT( "ClassIcon.%s" ), *ClassesToDrop[0]->GetName() ), nullptr, nullptr );

		// If the class icon is the default brush, do not put it in the cursor decoration window.
		if(ClassIcon)
		{
			return SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
				.Content()
				[			
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SImage )
						.Image( ClassIcon )
					]
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock) 
						.Text( ClassesToDrop[0]->GetName() )
					]
				];
		}

		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Content()
			[			
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock) 
					.Text( ClassesToDrop[0]->GetName() )
				]
			];
	}

	static TSharedRef<FClassDragDropOp> New(TWeakObjectPtr<UClass> ClassToDrop)
	{
		TSharedRef<FClassDragDropOp> Operation = MakeShareable(new FClassDragDropOp);
		Operation->ClassesToDrop.Add(ClassToDrop);
		Operation->Construct();
		return Operation;
	}

protected:
	/** Adding hint text to all drag drop operations that drop locations could set, and the decorator could report. */
	FText HintText;
};

struct FClassPackageData
{
	FString AssetName;
	FString GeneratedPackageName;

	FClassPackageData(const FString& InAssetName, const FString& InGeneratedPackageName)
	{
		AssetName = InAssetName;
		GeneratedPackageName = InGeneratedPackageName;
	}
};

class FUnloadedClassDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FUnloadedClassDragDropOp, FDragDropOperation)

	/** The assets to be dropped. */
	TSharedPtr< TArray< FClassPackageData > >	AssetsToDrop;
	
	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const OVERRIDE
	{
		// Create hover widget
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Content()
			[			
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock) 
					.Text( (*AssetsToDrop.Get())[0].AssetName )
				]
			];
	}

	static TSharedRef<FUnloadedClassDragDropOp> New(FClassPackageData AssetToDrop)
	{
		TSharedRef<FUnloadedClassDragDropOp> Operation = MakeShareable(new FUnloadedClassDragDropOp);
		Operation->AssetsToDrop = MakeShareable(new TArray<FClassPackageData>);
		Operation->AssetsToDrop->Add(AssetToDrop);
		Operation->Construct();
		return Operation;
	}
};

// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "EnvDirectionCustomization.h"

#define LOCTEXT_NAMESPACE "FEnvQueryCustomization"

TSharedRef<IStructCustomization> FEnvDirectionCustomization::MakeInstance( )
{
	return MakeShareable(new FEnvDirectionCustomization);
}

void FEnvDirectionCustomization::CustomizeStructHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IStructCustomizationUtils& StructCustomizationUtils )
{
	// create struct header
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(this, &FEnvDirectionCustomization::GetShortDescription)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	ModeProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvDirection,DirMode));
	if (ModeProp.IsValid())
	{
		FSimpleDelegate OnModeChangedDelegate = FSimpleDelegate::CreateSP(this, &FEnvDirectionCustomization::OnModeChanged);
		ModeProp->SetOnPropertyValueChanged(OnModeChangedDelegate);
	}
	
	OnModeChanged();
}

void FEnvDirectionCustomization::CustomizeStructChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IStructCustomizationUtils& StructCustomizationUtils )
{
	StructBuilder.AddChildProperty(ModeProp.ToSharedRef());

	TSharedPtr<IPropertyHandle> PropFrom = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvDirection,LineFrom));
	StructBuilder.AddChildProperty(PropFrom.ToSharedRef())
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvDirectionCustomization::GetTwoPointsVisibility)));

	TSharedPtr<IPropertyHandle> PropTo = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvDirection,LineTo));
	StructBuilder.AddChildProperty(PropTo.ToSharedRef())
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvDirectionCustomization::GetTwoPointsVisibility)));

	TSharedPtr<IPropertyHandle> PropRot = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvDirection,Rotation));
	StructBuilder.AddChildProperty(PropRot.ToSharedRef())
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvDirectionCustomization::GetRotationVisibility)));
}

FString FEnvDirectionCustomization::GetShortDescription() const
{
	return bIsRotation ? TEXT("context's rotation...") : TEXT("between two contexts...");
}

EVisibility FEnvDirectionCustomization::GetTwoPointsVisibility() const
{
	return bIsRotation ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FEnvDirectionCustomization::GetRotationVisibility() const
{
	return bIsRotation ? EVisibility::Visible : EVisibility::Collapsed;
}

void FEnvDirectionCustomization::OnModeChanged()
{
	uint8 EnumValue = 0;
	ModeProp->GetValue(EnumValue);

	bIsRotation = (EnumValue == EEnvDirection::Rotation);
}

#undef LOCTEXT_NAMESPACE

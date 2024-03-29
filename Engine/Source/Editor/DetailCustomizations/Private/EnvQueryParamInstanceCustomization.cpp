// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "EnvQueryParamInstanceCustomization.h"

#define LOCTEXT_NAMESPACE "FEnvQueryCustomization"

TSharedRef<IStructCustomization> FEnvQueryParamInstanceCustomization::MakeInstance( )
{
	return MakeShareable(new FEnvQueryParamInstanceCustomization);
}

void FEnvQueryParamInstanceCustomization::CustomizeStructHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IStructCustomizationUtils& StructCustomizationUtils )
{
	NameProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvNamedValue,ParamName));
	TypeProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvNamedValue,ParamType));
	ValueProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvNamedValue,Value));

	FSimpleDelegate OnTypeChangedDelegate = FSimpleDelegate::CreateSP( this, &FEnvQueryParamInstanceCustomization::OnTypeChanged );
	TypeProp->SetOnPropertyValueChanged(OnTypeChangedDelegate);
	InitCachedTypes();
	OnTypeChanged();

	// create struct header
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(STextBlock) 
		.Text(this, &FEnvQueryParamInstanceCustomization::GetHeaderDesc)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
}

void FEnvQueryParamInstanceCustomization::CustomizeStructChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IStructCustomizationUtils& StructCustomizationUtils )
{
	StructBuilder.AddChildProperty(NameProp.ToSharedRef());
	StructBuilder.AddChildProperty(TypeProp.ToSharedRef());
	
	StructBuilder.AddChildContent("Value")
		.NameContent()
		[
			ValueProp->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(0.0f, 2.0f, 5.0f, 2.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(false)
				.Visibility(this, &FEnvQueryParamInstanceCustomization::GetParamNumValueVisibility)
				.Value(this, &FEnvQueryParamInstanceCustomization::GetParamNumValue)
				.OnValueChanged(this, &FEnvQueryParamInstanceCustomization::OnParamNumValueChanged)
			]
			+SHorizontalBox::Slot()
			.Padding(0.0f, 2.0f, 5.0f, 2.0f)
			[
				SNew(SCheckBox)
				.Visibility(this, &FEnvQueryParamInstanceCustomization::GetParamBoolValueVisibility)
				.IsChecked(this, &FEnvQueryParamInstanceCustomization::GetParamBoolValue )
				.OnCheckStateChanged(this, &FEnvQueryParamInstanceCustomization::OnParamBoolValueChanged )
			]
		];
}

TOptional<float> FEnvQueryParamInstanceCustomization::GetParamNumValue() const
{
	// Gets the actual aspect ratio property value
	if (ParamType == EEnvQueryParam::Float)
	{
		float FloatValue = 0.0f;
		FPropertyAccess::Result PropResult = ValueProp->GetValue(FloatValue);
		if (PropResult == FPropertyAccess::Success)
		{
			return FloatValue;
		}
	}
	else if (ParamType == EEnvQueryParam::Int)
	{
		float StoreValue = 0.0f;
		FPropertyAccess::Result PropResult = ValueProp->GetValue(StoreValue);
		if (PropResult == FPropertyAccess::Success)
		{
			const int32 IntValue = *((int32*)&StoreValue);
			return IntValue;
		}
	}

	return TOptional<float>();
}

void FEnvQueryParamInstanceCustomization::OnParamNumValueChanged(float FloatValue) const
{
	if (ParamType == EEnvQueryParam::Float)
	{
		ValueProp->SetValue(FloatValue);
		CachedFloat = FloatValue;
	}
	else if (ParamType == EEnvQueryParam::Int)
	{
		const int32 IntValue = FMath::TruncToInt(FloatValue);
		const float StoreValue = *((float*)&IntValue);
		ValueProp->SetValue(StoreValue);
		CachedInt = IntValue;
	}
}

ESlateCheckBoxState::Type FEnvQueryParamInstanceCustomization::GetParamBoolValue() const
{
	if (ParamType == EEnvQueryParam::Bool)
	{
		float StoreValue = 0.0f;
		FPropertyAccess::Result PropResult = ValueProp->GetValue(StoreValue);
		if (PropResult == FPropertyAccess::Success)
		{
			return (StoreValue > 0.0f) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
		}
	}
	
	return ESlateCheckBoxState::Undetermined;
}

void FEnvQueryParamInstanceCustomization::OnParamBoolValueChanged(ESlateCheckBoxState::Type BoolValue) const
{
	if (ParamType == EEnvQueryParam::Bool)
	{
		const float StoreValue = (BoolValue == ESlateCheckBoxState::Checked) ? 1.0f : -1.0f;
		ValueProp->SetValue(StoreValue);
		CachedBool = (BoolValue == ESlateCheckBoxState::Checked);
	}
}

EVisibility FEnvQueryParamInstanceCustomization::GetParamNumValueVisibility() const
{
	return (ParamType == EEnvQueryParam::Int || ParamType == EEnvQueryParam::Float) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FEnvQueryParamInstanceCustomization::GetParamBoolValueVisibility() const
{
	return (ParamType == EEnvQueryParam::Bool) ? EVisibility::Visible : EVisibility::Collapsed;
}

FString FEnvQueryParamInstanceCustomization::GetHeaderDesc() const
{
	FString ParamName;
	if (NameProp->GetValue(ParamName) == FPropertyAccess::Success)
	{
		switch (ParamType)
		{
		case EEnvQueryParam::Float:	return FString::Printf(TEXT("%s = %s"), *ParamName, *FString::SanitizeFloat(CachedFloat));
		case EEnvQueryParam::Int:	return FString::Printf(TEXT("%s = %d"), *ParamName, CachedInt);
		case EEnvQueryParam::Bool:	return FString::Printf(TEXT("%s = %s"), *ParamName, CachedBool ? TEXT("true") : TEXT("false"));
		}
	}

	return FString();
}

void FEnvQueryParamInstanceCustomization::InitCachedTypes()
{
	CachedBool = false;
	CachedFloat = 0.0f;
	CachedInt = 0;

	uint8 EnumValue = 0;
	FPropertyAccess::Result PropResult = TypeProp->GetValue(EnumValue);
	if (PropResult == FPropertyAccess::Success)
	{
		ParamType = (EEnvQueryParam::Type)EnumValue;
		switch (ParamType)
		{
		case EEnvQueryParam::Float:	CachedFloat = GetParamNumValue().GetValue(); break;
		case EEnvQueryParam::Int:	CachedInt = FMath::TruncToInt(GetParamNumValue().GetValue()); break;
		case EEnvQueryParam::Bool:	CachedBool = (GetParamBoolValue() == ESlateCheckBoxState::Checked); break;
		}
	}
}

void FEnvQueryParamInstanceCustomization::OnTypeChanged()
{
	uint8 EnumValue = 0;
	FPropertyAccess::Result PropResult = TypeProp->GetValue(EnumValue);
	if (PropResult == FPropertyAccess::Success)
	{
		ParamType = (EEnvQueryParam::Type)EnumValue;
		switch (ParamType)
		{
		case EEnvQueryParam::Float:	OnParamNumValueChanged(CachedFloat); break;
		case EEnvQueryParam::Int:	OnParamNumValueChanged(CachedInt); break;
		case EEnvQueryParam::Bool:	OnParamBoolValueChanged(CachedBool ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked); break;
		}
	}
}

#undef LOCTEXT_NAMESPACE

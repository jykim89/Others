﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "LightComponentDetails.h"

#define LOCTEXT_NAMESPACE "LightComponentDetails"

TSharedRef<IDetailCustomization> FLightComponentDetails::MakeInstance()
{
	return MakeShareable( new FLightComponentDetails );
}

void FLightComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Mobility property is on the scene component base class not the light component and that is why we have to use USceneComponent::StaticClass
	TSharedRef<IPropertyHandle> MobilityHandle = DetailBuilder.GetProperty("Mobility", USceneComponent::StaticClass());
	// Set a mobility tooltip specific to lights
	MobilityHandle->SetToolTipText(LOCTEXT("LightMobilityTooltip", "Mobility for lights controls what the light is allowed to do at runtime and therefore what rendering methods are used.\n● A movable light uses fully dynamic lighting and anything can change in game, however it has a large performance cost, typically proportional to the light's influence size.\n● A stationary light will only have its shadowing and bounced lighting from static geometry baked by Lightmass, all other lighting will be dynamic.  It can change color and intensity in game. \n● A static light is fully baked into lightmaps and therefore has no performance cost, but also can't change in game.").ToString());

	IDetailCategoryBuilder& LightCategory = DetailBuilder.EditCategory( "Light", TEXT(""), ECategoryPriority::TypeSpecific );

	LightIntensityProperty = DetailBuilder.GetProperty("Intensity", ULightComponentBase::StaticClass());
	IESBrightnessTextureProperty = DetailBuilder.GetProperty("IESTexture");
	IESBrightnessEnabledProperty = DetailBuilder.GetProperty("bUseIESBrightness");
	IESBrightnessScaleProperty = DetailBuilder.GetProperty("IESBrightnessScale");

	if( !IESBrightnessEnabledProperty->IsValidHandle() )
	{
		// Brightness and color should be listed first
		LightCategory.AddProperty( LightIntensityProperty );
		LightCategory.AddProperty( DetailBuilder.GetProperty("LightColor", ULightComponentBase::StaticClass() ) );
	}
	else
	{
		IDetailCategoryBuilder& LightProfilesCategory = DetailBuilder.EditCategory( "Light Profiles", TEXT(""), ECategoryPriority::TypeSpecific );

		LightCategory.AddProperty( LightIntensityProperty )
			.IsEnabled( TAttribute<bool>( this, &FLightComponentDetails::IsLightBrightnessEnabled ) );

		LightCategory.AddProperty( DetailBuilder.GetProperty("LightColor", ULightComponentBase::StaticClass() ) );

		LightProfilesCategory.AddProperty( IESBrightnessTextureProperty );

		LightProfilesCategory.AddProperty( IESBrightnessEnabledProperty )
			.IsEnabled( TAttribute<bool>( this, &FLightComponentDetails::IsUseIESBrightnessEnabled ) );

		LightProfilesCategory.AddProperty( IESBrightnessScaleProperty)
			.IsEnabled( TAttribute<bool>( this, &FLightComponentDetails::IsIESBrightnessScaleEnabled ) );
	}
}

bool FLightComponentDetails::IsLightBrightnessEnabled() const
{
	return !IsIESBrightnessScaleEnabled();
}

bool FLightComponentDetails::IsUseIESBrightnessEnabled() const
{
	UObject* IESTexture;
	IESBrightnessTextureProperty->GetValue(IESTexture);
	return (IESTexture != NULL);
}

bool FLightComponentDetails::IsIESBrightnessScaleEnabled() const
{
	bool Enabled;
	IESBrightnessEnabledProperty->GetValue(Enabled);
	return IsUseIESBrightnessEnabled() && Enabled;
}


#undef LOCTEXT_NAMESPACE

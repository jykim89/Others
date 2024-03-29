// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateWidgetStyleContainerBase.h"
#include "SlateWidgetStyleAsset.generated.h"

/**
 * Just a wrapper for the struct with real data in it.
 */
UCLASS(hidecategories=Object)
class SLATECORE_API USlateWidgetStyleAsset : public UObject
{
	GENERATED_UCLASS_BODY()
		  
public:  
	/**  */
	UPROPERTY(Category=Appearance, EditAnywhere, EditInline)
	USlateWidgetStyleContainerBase* CustomStyle;

	template< class WidgetStyleType >            
	const WidgetStyleType* GetStyle() const 
	{
		return static_cast< const WidgetStyleType* >( GetStyle( WidgetStyleType::TypeName ) );
	}

	template< class WidgetStyleType >            
	const WidgetStyleType* GetStyleChecked() const 
	{
		return static_cast< const WidgetStyleType* >( GetStyleChecked( WidgetStyleType::TypeName ) );
	}

	const FSlateWidgetStyle* GetStyle( const FName DesiredTypeName ) const 
	{
		if ( CustomStyle == nullptr )
		{
			return nullptr;
		}

		const FSlateWidgetStyle* Style = CustomStyle->GetStyle();

		if ( (Style == nullptr) || (Style->GetTypeName() != DesiredTypeName) )
		{
			return nullptr;
		}

		return Style;
	}

	const FSlateWidgetStyle* GetStyleChecked( const FName DesiredTypeName ) const 
	{
		if ( CustomStyle == nullptr )
		{
			UE_LOG( LogSlateStyle, Error, TEXT("USlateWidgetStyleAsset::GetStyle : No custom style set for '%s'."), *GetPathName() );
			return nullptr;
		}

		const FSlateWidgetStyle* Style = CustomStyle->GetStyle();

		if ( Style == nullptr )
		{
			UE_LOG( LogSlateStyle, Error, TEXT("USlateWidgetStyleAsset::GetStyle : No style found in custom style set for '%s'."), *GetPathName() );
			return nullptr;
		}

		if ( Style->GetTypeName() != DesiredTypeName )
		{
			UE_LOG( LogSlateStyle, Error, TEXT("USlateWidgetStyleAsset::GetStyle : The custom style is not of the desired type. Desired: '%s', Actual: '%s'"), *DesiredTypeName.ToString(), *Style->GetTypeName().ToString() );
			return nullptr;
		}

		return Style;
	}
};

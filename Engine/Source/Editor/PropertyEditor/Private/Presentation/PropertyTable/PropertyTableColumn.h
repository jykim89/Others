// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IPropertyTableColumn.h"

class FPropertyTableColumn : public TSharedFromThis< FPropertyTableColumn >, public IPropertyTableColumn
{
public:

	FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject );

	FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TSharedRef< FPropertyPath >& InPropertyPath );

	FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject, const TSharedRef< FPropertyPath >& InPartialPropertyPath );

	// Begin IPropertyTable Interface

	virtual FName GetId() const OVERRIDE;

	virtual FText GetDisplayName() const OVERRIDE;

	virtual TSharedRef< IDataSource > GetDataSource() const OVERRIDE { return DataSource; }

	virtual TSharedRef< class FPropertyPath > GetPartialPath() const OVERRIDE { return PartialPath; }

	virtual TSharedRef< class IPropertyTableCell > GetCell( const TSharedRef< class IPropertyTableRow >& Row ) OVERRIDE;

	virtual void RemoveCellsForRow( const TSharedRef< class IPropertyTableRow >& Row ) OVERRIDE;

	virtual TSharedRef< class IPropertyTable > GetTable() const OVERRIDE;

	virtual bool CanSelectCells() const OVERRIDE { return !IsHidden(); }

	virtual EPropertyTableColumnSizeMode::Type GetSizeMode() const OVERRIDE { return SizeMode; }

	virtual void SetSizeMode(EPropertyTableColumnSizeMode::Type InSizeMode) OVERRIDE{ SizeMode = InSizeMode; }

	virtual float GetWidth() const OVERRIDE { return Width; } 

	virtual void SetWidth( float InWidth ) OVERRIDE { Width = InWidth; }

	virtual bool IsHidden() const OVERRIDE { return bIsHidden; }

	virtual void SetHidden( bool InIsHidden ) OVERRIDE { bIsHidden = InIsHidden; }

	virtual bool IsFrozen() const OVERRIDE { return bIsFrozen; }

	virtual void SetFrozen( bool InIsFrozen ) OVERRIDE;

	virtual bool CanSortBy() const OVERRIDE;

	virtual void Sort( TArray< TSharedRef< class IPropertyTableRow > >& Rows, const EColumnSortMode::Type SortMode ) OVERRIDE;

	virtual void Tick() OVERRIDE;

	DECLARE_DERIVED_EVENT( FPropertyTableColumn, IPropertyTableColumn::FFrozenStateChanged, FFrozenStateChanged );
	FFrozenStateChanged* OnFrozenStateChanged() OVERRIDE { return &FrozenStateChanged; }

	// End IPropertyTable Interface

private:

	void GenerateColumnId();

	void GenerateColumnDisplayName();


private:

	TMap< TSharedRef< IPropertyTableRow >, TSharedRef< class IPropertyTableCell > > Cells;

	TSharedRef< IDataSource > DataSource;
	TWeakPtr< IPropertyTable > Table;

	FName Id;
	FText DisplayName;

	float Width;

	bool bIsHidden;
	bool bIsFrozen;

	FFrozenStateChanged FrozenStateChanged;

	TSharedRef< class FPropertyPath > PartialPath;

	EPropertyTableColumnSizeMode::Type SizeMode;
};
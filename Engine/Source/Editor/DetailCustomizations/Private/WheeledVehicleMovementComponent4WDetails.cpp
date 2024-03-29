﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "WheeledVehicleMovementComponent4WDetails.h"
#include "ScopedTransaction.h"
#include "ObjectEditorUtils.h"
#include "Vehicles/WheeledVehicleMovementComponent4W.h"
#include "IDocumentation.h"
#include "SCurveEditor.h"

#define LOCTEXT_NAMESPACE "WheeledVehicleMovementComponent4WDetails"

//////////////////////////////////////////////////////////////
// This class customizes various settings in WheeledVehicleMovementComponent4W
//////////////////////////////////////////////////////////////

TSharedRef<IDetailCustomization> FWheeledVehicleMovementComponent4WDetails::MakeInstance()
{
	return MakeShareable(new FWheeledVehicleMovementComponent4WDetails);
}

void FWheeledVehicleMovementComponent4WDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);

	//we only do fancy customization if we have one vehicle component selected
	if (SelectedObjects.Num() != 1)
	{
		return;
	}
	else if (UWheeledVehicleMovementComponent4W * VehicleComponent = Cast<UWheeledVehicleMovementComponent4W>(SelectedObjects[0].Get()))
	{
		SteeringCurveEditor = FSteeringCurveEditor(VehicleComponent);
		TorqueCurveEditor = FTorqueCurveEditor(VehicleComponent);
	}
	else
	{
		return;
	}

	//Torque curve
	{
		IDetailCategoryBuilder& MechanicalCategory = DetailBuilder.EditCategory("MechanicalSetup");
		TSharedRef<IPropertyHandle> TorqueCurveHandle = DetailBuilder.GetProperty("EngineSetup.TorqueCurve");

		MechanicalCategory.AddProperty(TorqueCurveHandle).CustomWidget()
		.NameContent()
			[
				TorqueCurveHandle->CreatePropertyNameWidget()
			]
		.ValueContent()
			.MinDesiredWidth(125.f * 3.f)
			[
				SAssignNew(TorqueCurveWidget, SCurveEditor)
				.ViewMinInput(0.f)
				.ViewMaxInput(70000.f)
				.ViewMinOutput(0.f)
				.ViewMaxOutput(1.f)
				.TimelineLength(7000.f)
				.HideUI(false)
				.DesiredSize(FVector2D(512, 128))
			];

		TorqueCurveWidget->SetCurveOwner(&TorqueCurveEditor);
	}
	
	//Steering curve
	{
		IDetailCategoryBuilder& SteeringCategory = DetailBuilder.EditCategory("SteeringSetup");
		TSharedRef<IPropertyHandle> SteeringCurveHandle = DetailBuilder.GetProperty("SteeringCurve");
		SteeringCategory.AddProperty(SteeringCurveHandle).CustomWidget()
			.NameContent()
			[
				SteeringCurveHandle->CreatePropertyNameWidget()
			]
		.ValueContent()
			.MinDesiredWidth(125.f * 3.f)
			[
				SAssignNew(SteeringCurveWidget, SCurveEditor)
				.ViewMinInput(0.f)
				.ViewMaxInput(150.f)
				.ViewMinOutput(0.f)
				.ViewMaxOutput(1.f)
				.TimelineLength(150)
				.HideUI(false)
				.ZoomToFit(false)
				.DesiredSize(FVector2D(512, 128))
			];

		SteeringCurveWidget->SetCurveOwner(&SteeringCurveEditor);
	}
}

TArray<FRichCurveEditInfoConst> FWheeledVehicleMovementComponent4WDetails::FSteeringCurveEditor::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(&VehicleComponent->SteeringCurve.EditorCurveData);

	return Curves;
}

TArray<FRichCurveEditInfo> FWheeledVehicleMovementComponent4WDetails::FSteeringCurveEditor::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(&VehicleComponent->SteeringCurve.EditorCurveData);
	
	return Curves;
}

UObject * FWheeledVehicleMovementComponent4WDetails::FSteeringCurveEditor::GetOwner()
{
	return Owner;
}

void FWheeledVehicleMovementComponent4WDetails::FSteeringCurveEditor::ModifyOwner()
{
	if (Owner)
	{
		Owner->Modify();
	}
}

void FWheeledVehicleMovementComponent4WDetails::FSteeringCurveEditor::MakeTransactional()
{
	if (Owner)
	{
		Owner->SetFlags(Owner->GetFlags() | RF_Transactional);
	}
}

FWheeledVehicleMovementComponent4WDetails::FSteeringCurveEditor::FSteeringCurveEditor(UWheeledVehicleMovementComponent4W * InVehicle)
{
	VehicleComponent = InVehicle;
	Owner = VehicleComponent;
}


TArray<FRichCurveEditInfoConst> FWheeledVehicleMovementComponent4WDetails::FTorqueCurveEditor::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(&VehicleComponent->EngineSetup.TorqueCurve.EditorCurveData);

	return Curves;
}

TArray<FRichCurveEditInfo> FWheeledVehicleMovementComponent4WDetails::FTorqueCurveEditor::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(&VehicleComponent->EngineSetup.TorqueCurve.EditorCurveData);

	return Curves;
}

UObject * FWheeledVehicleMovementComponent4WDetails::FTorqueCurveEditor::GetOwner()
{
	return Owner;
}

void FWheeledVehicleMovementComponent4WDetails::FTorqueCurveEditor::ModifyOwner()
{
	if (Owner)
	{
		Owner->Modify();
	}
}

void FWheeledVehicleMovementComponent4WDetails::FTorqueCurveEditor::MakeTransactional()
{
	if (Owner)
	{
		Owner->SetFlags(Owner->GetFlags() | RF_Transactional);
	}
}

FWheeledVehicleMovementComponent4WDetails::FTorqueCurveEditor::FTorqueCurveEditor(UWheeledVehicleMovementComponent4W * InVehicle)
{
	VehicleComponent = InVehicle;
	Owner = VehicleComponent;
}

#undef LOCTEXT_NAMESPACE


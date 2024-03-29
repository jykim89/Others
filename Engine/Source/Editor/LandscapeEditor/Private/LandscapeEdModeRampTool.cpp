// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
//#include "ObjectTools.h"
#include "LandscapeEdMode.h"
#include "ScopedTransaction.h"
//#include "EngineFoliageClasses.h"
#include "Landscape/LandscapeEdit.h"
#include "Landscape/LandscapeRender.h"
#include "Landscape/LandscapeDataAccess.h"
//#include "Landscape/LandscapeSplineProxies.h"
//#include "LandscapeEditorModule.h"
//#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
//#include "LandscapeEdModeTools.h"
#include "Raster.h"

#define LOCTEXT_NAMESPACE "Landscape"

class FLandscapeRampToolHeightRasterPolicy
{
public:
	// X = Side Falloff Alpha, Y = Height
	typedef FVector2D InterpolantType;

	/** Initialization constructor. */
	FLandscapeRampToolHeightRasterPolicy(TArray<uint16>& InData, int32 InMinX, int32 InMinY, int32 InMaxX, int32 InMaxY, bool InbRaiseTerrain, bool InbLowerTerrain):
		Data(InData),
		MinX(InMinX),
		MinY(InMinY),
		MaxX(InMaxX),
		MaxY(InMaxY),
		bRaiseTerrain(InbRaiseTerrain),
		bLowerTerrain(InbLowerTerrain)
	{
	}

protected:

	// FTriangleRasterizer policy interface.

	int32 GetMinX() const { return MinX; }
	int32 GetMaxX() const { return MaxX; }
	int32 GetMinY() const { return MinY; }
	int32 GetMaxY() const { return MaxY; }

	void ProcessPixel(int32 X, int32 Y, const InterpolantType& Interpolant, bool BackFacing)
	{
		const float CosInterpX = (Interpolant.X >= 1 ? 1 : 0.5f - 0.5f * FMath::Cos(Interpolant.X * PI) );
		const float Alpha = CosInterpX;
		uint16& Dest = Data[(Y-MinY)*(1+MaxX-MinX) + X-MinX];
		float Value = FMath::Lerp((float)Dest, Interpolant.Y, Alpha);
		uint16 DValue = (uint32)FMath::Clamp<float>(Value, 0, LandscapeDataAccess::MaxValue);
		if ( (bRaiseTerrain && DValue > Dest) ||
			(bLowerTerrain && DValue < Dest) )
		{
			Dest = DValue;
		}
	}

private:
	TArray<uint16>& Data;
	int32 MinX, MinY, MaxX, MaxY;
	uint32 bRaiseTerrain:1, bLowerTerrain:1;
};

class HLandscapeRampToolPointHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( );

	int8 Point;

	HLandscapeRampToolPointHitProxy(int8 InPoint) :
		HHitProxy(HPP_Foreground),
		Point(InPoint)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor()
	{
		return EMouseCursor::Crosshairs;
	}
};

IMPLEMENT_HIT_PROXY(HLandscapeRampToolPointHitProxy, HHitProxy)

class FLandscapeToolRamp : public FLandscapeTool
{
protected:
	class FEdModeLandscape* EdMode;
	UTexture2D* SpriteTexture;
	FVector Points[2];
	int8 NumPoints;
	int8 SelectedPoint;
	bool bMovingPoint;

public:
	FLandscapeToolRamp(FEdModeLandscape* InEdMode)
		: EdMode(InEdMode)
		, SpriteTexture(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_Terrain.S_Terrain")))
		, NumPoints(0)
		, SelectedPoint(INDEX_NONE)
		, bMovingPoint(false)
	{
		check(SpriteTexture);
	}

	virtual const TCHAR* GetToolName() OVERRIDE { return TEXT("Ramp"); }
	virtual FText GetDisplayName() OVERRIDE { return NSLOCTEXT("UnrealEd", "LandscapeMode_Ramp", "Ramp"); };

	virtual void SetEditRenderType() OVERRIDE { GLandscapeEditRenderMode = ELandscapeEditRenderMode::None | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }
	virtual bool SupportsMask() OVERRIDE { return false; }

	virtual bool IsValidForTarget(const FLandscapeToolTarget& Target) OVERRIDE
	{
		return Target.TargetType == ELandscapeToolTargetType::Heightmap;
	}

	virtual void EnterTool() OVERRIDE
	{
		NumPoints = 0;
		SelectedPoint = INDEX_NONE;
		GEditorModeTools().SetWidgetMode(FWidget::WM_Translate);
	}

	virtual bool BeginTool(FLevelEditorViewportClient* ViewportClient, const FLandscapeToolTarget& Target, const FVector& InHitLocation) OVERRIDE
	{
		if (NumPoints < 2)
		{
			Points[NumPoints] = InHitLocation;
			SelectedPoint = NumPoints;
			NumPoints++;
			bMovingPoint = true;
			GEditorModeTools().SetWidgetMode(FWidget::WM_Translate);
		}
		else
		{
			if (SelectedPoint != INDEX_NONE)
			{
				Points[SelectedPoint] = InHitLocation;
				bMovingPoint = true;
				GEditorModeTools().SetWidgetMode(FWidget::WM_Translate);
			}
		}

		GUnrealEd->RedrawLevelEditingViewports();

		return true;
	}

	virtual void EndTool(FLevelEditorViewportClient* ViewportClient) OVERRIDE
	{
		bMovingPoint = false;
	}

	virtual bool MouseMove(FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) OVERRIDE
	{
		if (bMovingPoint)
		{
			FVector HitLocation;
			if (EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitLocation))
			{
				if (NumPoints == 1)
				{
					SelectedPoint = NumPoints;
					NumPoints++;
				}

				Points[SelectedPoint] = HitLocation;
			}
		}

		return true;
	}

	virtual bool HandleClick(HHitProxy* HitProxy, const FViewportClick& Click) OVERRIDE
	{
		if (HitProxy)
		{
			if (HitProxy->IsA(HLandscapeRampToolPointHitProxy::StaticGetType()) )
			{
				HLandscapeRampToolPointHitProxy* PointHitProxy = (HLandscapeRampToolPointHitProxy*)HitProxy;
				SelectedPoint = PointHitProxy->Point;
				GEditorModeTools().SetWidgetMode(FWidget::WM_Translate);
				GUnrealEd->RedrawLevelEditingViewports();

				return true;
			}
		}

		return false;
	}

	virtual bool InputKey(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) OVERRIDE
	{
		if (InKey == EKeys::Enter && InEvent == IE_Pressed)
		{
			if (CanApplyRamp())
			{
				ApplyRamp();
			}
		}

		if (InKey == EKeys::Escape && InEvent == IE_Pressed)
		{
			ResetRamp();
		}

		if (SelectedPoint != INDEX_NONE)
		{
			if (InKey == EKeys::End && InEvent == IE_Pressed)
			{
				const int32 MinX = FMath::FloorToInt(Points[SelectedPoint].X);
				const int32 MinY = FMath::FloorToInt(Points[SelectedPoint].Y);
				const int32 MaxX = MinX + 1;
				const int32 MaxY = MinY + 1;

				FLandscapeEditDataInterface LandscapeEdit(EdMode->CurrentToolTarget.LandscapeInfo.Get());

				TArray<uint16> Data;
				Data.AddZeroed(4);

				int32 ValidMinX = MinX;
				int32 ValidMinY = MinY;
				int32 ValidMaxX = MaxX;
				int32 ValidMaxY = MaxY;
				LandscapeEdit.GetHeightData(ValidMinX, ValidMinY, ValidMaxX, ValidMaxY, Data.GetTypedData(), 0);

				if (ValidMaxX - ValidMinX != 1 && ValidMaxY - ValidMinY != 1)
				{
					// If we didn't read 4 values then we're partly off the edge of the landscape
					return true;
				}

				checkSlow(ValidMinX == MinX);
				checkSlow(ValidMinY == MinY);
				checkSlow(ValidMaxX == MaxX);
				checkSlow(ValidMaxY == MaxY);

				Points[SelectedPoint].Z = (FMath::BiLerp<float>(Data[0], Data[1], Data[2], Data[3], FMath::Frac(Points[SelectedPoint].X), FMath::Frac(Points[SelectedPoint].Y)) - LandscapeDataAccess::MidValue) * LANDSCAPE_ZSCALE;

				return true;
			}
		}

		// Change Ramp Width
		if ((InEvent == IE_Pressed || InEvent == IE_Repeat) && (InKey == EKeys::LeftBracket || InKey == EKeys::RightBracket))
		{
			const float OldValue = EdMode->UISettings->RampWidth;
			const float SliderMin = 0.0f;
			const float SliderMax = 8192.0f;
			const float Diff = 0.05f;

			float NewValue;
			if (InKey == EKeys::LeftBracket)
			{
				NewValue = OldValue - OldValue * Diff;
				NewValue = FMath::Min(NewValue, OldValue - 1.f);
			}
			else
			{
				NewValue = OldValue + OldValue * Diff;
				NewValue = FMath::Max(NewValue, OldValue + 1.f);
			}

			NewValue = FMath::RoundToFloat(FMath::Clamp(NewValue, SliderMin, SliderMax));

			EdMode->UISettings->RampWidth = NewValue;

			return true;
		}

		return false;
	}

	virtual bool InputDelta(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) OVERRIDE
	{
		if (SelectedPoint != INDEX_NONE && InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
		{
			const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			Points[SelectedPoint] += LandscapeToWorld.InverseTransformVector(InDrag);

			return true;
		}

		return false;
	}

	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) OVERRIDE
	{
		if (NumPoints > 0)
		{
			const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			const FLinearColor SelectedSpriteColor = FLinearColor::White + (GEngine->GetSelectedMaterialColor() * GEngine->SelectionHighlightIntensity * 10); // copied from FSpriteSceneProxy::DrawDynamicElements()

			FVector WorldPoints[2];
			for (int32 i = 0; i < NumPoints; i++)
			{
				WorldPoints[i] = LandscapeToWorld.TransformPosition(Points[i]);

				const FLinearColor SpriteColor = (i == SelectedPoint) ? SelectedSpriteColor : FLinearColor::White;

				PDI->SetHitProxy(new HLandscapeRampToolPointHitProxy(i));
				PDI->DrawSprite(WorldPoints[i],
					SpriteTexture->Resource->GetSizeX() * 2,
					SpriteTexture->Resource->GetSizeY() * 2,
					SpriteTexture->Resource,
					SpriteColor,
					SDPG_Foreground,
					0, SpriteTexture->Resource->GetSizeX(),
					0, SpriteTexture->Resource->GetSizeY(),
					SE_BLEND_Masked);
			}
			PDI->SetHitProxy(NULL);

			if (NumPoints == 2)
			{
				const FVector Side = FVector::CrossProduct(Points[1] - Points[0], FVector(0, 0, 1)).SafeNormal2D();
				const FVector InnerSide = Side * (EdMode->UISettings->RampWidth * 0.5f * (1 - EdMode->UISettings->RampSideFalloff));
				const FVector OuterSide = Side * (EdMode->UISettings->RampWidth * 0.5f);
				FVector InnerVerts[2][2];
				InnerVerts[0][0] = WorldPoints[0] - InnerSide;
				InnerVerts[0][1] = WorldPoints[0] + InnerSide;
				InnerVerts[1][0] = WorldPoints[1] - InnerSide;
				InnerVerts[1][1] = WorldPoints[1] + InnerSide;

				FVector OuterVerts[2][2];
				OuterVerts[0][0] = WorldPoints[0] - OuterSide;
				OuterVerts[0][1] = WorldPoints[0] + OuterSide;
				OuterVerts[1][0] = WorldPoints[1] - OuterSide;
				OuterVerts[1][1] = WorldPoints[1] + OuterSide;

				// Left
				DrawDashedLine(PDI, OuterVerts[0][0], OuterVerts[1][0], FColor::White, 50, SDPG_Foreground);

				// Center
				DrawDashedLine(PDI, InnerVerts[0][0], InnerVerts[0][1], FColor::White, 50, SDPG_Foreground);
				PDI->DrawLine(InnerVerts[0][0], InnerVerts[0][1], FLinearColor::White, SDPG_World);
				DrawDashedLine(PDI, InnerVerts[0][0], InnerVerts[1][0], FColor::White, 50, SDPG_Foreground);
				PDI->DrawLine(InnerVerts[0][0], InnerVerts[1][0], FLinearColor::White, SDPG_World);
				DrawDashedLine(PDI, InnerVerts[0][1], InnerVerts[1][1], FColor::White, 50, SDPG_Foreground);
				PDI->DrawLine(InnerVerts[0][1], InnerVerts[1][1], FLinearColor::White, SDPG_World);
				DrawDashedLine(PDI, InnerVerts[1][0], InnerVerts[1][1], FColor::White, 50, SDPG_Foreground);
				PDI->DrawLine(InnerVerts[1][0], InnerVerts[1][1], FLinearColor::White, SDPG_World);

				// Right
				DrawDashedLine(PDI, OuterVerts[0][1], OuterVerts[1][1], FColor::White, 50, SDPG_Foreground);
			}
		}
	}

	virtual bool OverrideSelection() const OVERRIDE
	{
		return true;
	}

	virtual bool IsSelectionAllowed( AActor* InActor, bool bInSelection ) const OVERRIDE
	{
		// Only filter selection not deselection
		if (bInSelection)
		{
			return false;
		}

		return true;
	}

	virtual bool UsesTransformWidget() const OVERRIDE
	{
		if (SelectedPoint != INDEX_NONE)
		{
			return true;
		}

		return false;
	}

	virtual EAxisList::Type GetWidgetAxisToDraw(FWidget::EWidgetMode CheckMode) const OVERRIDE
	{
		if (SelectedPoint != INDEX_NONE)
		{
			if (CheckMode == FWidget::WM_Translate)
			{
				return EAxisList::XYZ;
			}
			else
			{
				return EAxisList::None;
			}
		}

		return EAxisList::None;
	}

	virtual FVector GetWidgetLocation() const OVERRIDE
	{
		if (SelectedPoint != INDEX_NONE)
		{
			const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			return LandscapeToWorld.TransformPosition(Points[SelectedPoint]);
		}

		return FVector::ZeroVector;
	}

	virtual FMatrix GetWidgetRotation() const OVERRIDE
	{
		if (SelectedPoint != INDEX_NONE)
		{
			const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			return FQuatRotationTranslationMatrix(LandscapeToWorld.GetRotation(), FVector::ZeroVector);
		}

		return FMatrix::Identity;
	}

	virtual void ApplyRamp()
	{
		FScopedTransaction Transaction(LOCTEXT("Ramp_Apply", "Landscape Editing: Add ramp") );

		const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
		const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

		const FVector2D Side = FVector2D(FVector::CrossProduct(Points[1] - Points[0], FVector(0, 0, 1))).SafeNormal();
		const FVector2D InnerSide = Side * (EdMode->UISettings->RampWidth * 0.5f * (1 - EdMode->UISettings->RampSideFalloff)) / LandscapeToWorld.GetScale3D().X;
		const FVector2D OuterSide = Side * (EdMode->UISettings->RampWidth * 0.5f) / LandscapeToWorld.GetScale3D().X;

		FVector2D InnerVerts[2][2];
		InnerVerts[0][0] = FVector2D(Points[0]) - InnerSide;
		InnerVerts[0][1] = FVector2D(Points[0]) + InnerSide;
		InnerVerts[1][0] = FVector2D(Points[1]) - InnerSide;
		InnerVerts[1][1] = FVector2D(Points[1]) + InnerSide;

		FVector2D OuterVerts[2][2];
		OuterVerts[0][0] = FVector2D(Points[0]) - OuterSide;
		OuterVerts[0][1] = FVector2D(Points[0]) + OuterSide;
		OuterVerts[1][0] = FVector2D(Points[1]) - OuterSide;
		OuterVerts[1][1] = FVector2D(Points[1]) + OuterSide;

		float Heights[2];
		Heights[0] = Points[0].Z * LANDSCAPE_INV_ZSCALE + LandscapeDataAccess::MidValue;
		Heights[1] = Points[1].Z * LANDSCAPE_INV_ZSCALE + LandscapeDataAccess::MidValue;

		int32 MinX = FMath::CeilToInt( FMath::Min(FMath::Min(OuterVerts[0][0].X, OuterVerts[0][1].X),FMath::Min(OuterVerts[1][0].X, OuterVerts[1][1].X))) - 1; // +/- 1 to make sure we have enough data for calculating correct normals
		int32 MinY = FMath::CeilToInt( FMath::Min(FMath::Min(OuterVerts[0][0].Y, OuterVerts[0][1].Y),FMath::Min(OuterVerts[1][0].Y, OuterVerts[1][1].Y))) - 1;
		int32 MaxX = FMath::FloorToInt(FMath::Max(FMath::Max(OuterVerts[0][0].X, OuterVerts[0][1].X),FMath::Max(OuterVerts[1][0].X, OuterVerts[1][1].X))) + 1;
		int32 MaxY = FMath::FloorToInt(FMath::Max(FMath::Max(OuterVerts[0][0].Y, OuterVerts[0][1].Y),FMath::Max(OuterVerts[1][0].Y, OuterVerts[1][1].Y))) + 1;

		FLandscapeEditDataInterface LandscapeEdit(EdMode->CurrentToolTarget.LandscapeInfo.Get());

		// Heights raster
		bool bRaiseTerrain = true; //EdMode->UISettings->Ramp_bRaiseTerrain;
		bool bLowerTerrain = true; //EdMode->UISettings->Ramp_bLowerTerrain;
		if (bRaiseTerrain || bLowerTerrain)
		{
			TArray<uint16> Data;
			Data.AddZeroed( (1+MaxY-MinY) * (1+MaxX-MinX) );

			int32 ValidMinX = MinX;
			int32 ValidMinY = MinY;
			int32 ValidMaxX = MaxX;
			int32 ValidMaxY = MaxY;
			LandscapeEdit.GetHeightData(ValidMinX, ValidMinY, ValidMaxX, ValidMaxY, Data.GetTypedData(), 0);

			if (ValidMinX > ValidMaxX || ValidMinY > ValidMaxY)
			{
				// The bounds don't intersect any data, so we skip it entirely
				return;
			}

			//ShrinkData(Data, MinX, MinY, MaxX, MaxY, ValidMinX, ValidMinY, ValidMaxX, ValidMaxY);

			MinX = ValidMinX;
			MinY = ValidMinY;
			MaxX = ValidMaxX;
			MaxY = ValidMaxY;

			FTriangleRasterizer<FLandscapeRampToolHeightRasterPolicy> Rasterizer(
				FLandscapeRampToolHeightRasterPolicy(Data, MinX, MinY, MaxX, MaxY, bRaiseTerrain, bLowerTerrain) );

			// Left
			Rasterizer.DrawTriangle(FVector2D(0, Heights[0]), FVector2D(1, Heights[0]), FVector2D(0, Heights[1]), OuterVerts[0][0], InnerVerts[0][0], OuterVerts[1][0], false);
			Rasterizer.DrawTriangle(FVector2D(1, Heights[0]), FVector2D(0, Heights[1]), FVector2D(1, Heights[1]), InnerVerts[0][0], OuterVerts[1][0], InnerVerts[1][0], false);

			// Center
			Rasterizer.DrawTriangle(FVector2D(1, Heights[0]), FVector2D(1, Heights[0]), FVector2D(1, Heights[1]), InnerVerts[0][0], InnerVerts[0][1], InnerVerts[1][0], false);
			Rasterizer.DrawTriangle(FVector2D(1, Heights[0]), FVector2D(1, Heights[1]), FVector2D(1, Heights[1]), InnerVerts[0][1], InnerVerts[1][0], InnerVerts[1][1], false);

			// Right
			Rasterizer.DrawTriangle(FVector2D(1, Heights[0]), FVector2D(0, Heights[0]), FVector2D(1, Heights[1]), InnerVerts[0][1], OuterVerts[0][1], InnerVerts[1][1], false);
			Rasterizer.DrawTriangle(FVector2D(0, Heights[0]), FVector2D(1, Heights[1]), FVector2D(0, Heights[1]), OuterVerts[0][1], InnerVerts[1][1], OuterVerts[1][1], false);

			LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, Data.GetTypedData(), 0, true);
			LandscapeEdit.Flush();

			TSet<ULandscapeComponent*> Components;
			if (LandscapeEdit.GetComponentsInRegion(MinX, MinY, MaxX, MaxY, &Components))
			{
				for (ULandscapeComponent* Component : Components)
				{
					// Recreate collision for modified components and update the navmesh
					ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->CollisionComponent.Get();
					if (CollisionComponent)
					{
						CollisionComponent->RecreateCollision(false);
						UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(Component);
						if (NavSys)
						{
							NavSys->UpdateNavOctree(CollisionComponent);
						}
					}
				}
			}
		}
	}

	bool CanApplyRamp()
	{
		return NumPoints == 2;
	}

	void ResetRamp()
	{
		NumPoints = 0;
		SelectedPoint = INDEX_NONE;
	}
};

void FEdModeLandscape::ApplyRampTool()
{
	if (CurrentToolSet->GetToolSetName() == FName("ToolSet_Ramp"))
	{
		FLandscapeToolRamp* RampTool = (FLandscapeToolRamp*)CurrentToolSet->GetTool();
		RampTool->ApplyRamp();
		GEditor->RedrawLevelEditingViewports();
	}
}

bool FEdModeLandscape::CanApplyRampTool()
{
	if (CurrentToolSet->GetToolSetName() == FName("ToolSet_Ramp"))
	{
		FLandscapeToolRamp* RampTool = (FLandscapeToolRamp*)CurrentToolSet->GetTool();

		return RampTool->CanApplyRamp();
	}
	return false;
}

void FEdModeLandscape::ResetRampTool()
{
	if (CurrentToolSet->GetToolSetName() == FName("ToolSet_Ramp"))
	{
		FLandscapeToolRamp* RampTool = (FLandscapeToolRamp*)CurrentToolSet->GetTool();
		RampTool->ResetRamp();
		GEditor->RedrawLevelEditingViewports();
	}
}

//
// Toolset initialization
//
void FEdModeLandscape::InitializeToolSet_Ramp()
{
	FLandscapeToolSet* ToolSet_Ramp = new(LandscapeToolSets) FLandscapeToolSet(TEXT("ToolSet_Ramp"));
	ToolSet_Ramp->AddTool(new FLandscapeToolRamp(this));
	ToolSet_Ramp->ValidBrushes.Add("BrushSet_Dummy");
}

#undef LOCTEXT_NAMESPACE
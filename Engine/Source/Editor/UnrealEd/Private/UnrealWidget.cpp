// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "SnappingUtils.h"

IMPLEMENT_HIT_PROXY(HWidgetAxis,HHitProxy);

static const float AXIS_LENGTH = 35.0f;
static const float TRANSLATE_ROTATE_AXIS_CIRCLE_RADIUS = 20.0f;
static const float INNER_AXIS_CIRCLE_RADIUS = 48.0f;
static const float OUTER_AXIS_CIRCLE_RADIUS = 56.0f;
static const float ROTATION_TEXT_RADIUS = 75.0f;
static const int32 AXIS_CIRCLE_SIDES = 24;

FWidget::FWidget()
{
	EditorModeTools = NULL;
	TotalDeltaRotation = 0;
	CurrentDeltaRotation = 0;

	AxisColorX = FLinearColor(0.594f,0.0197f,0.0f);
	AxisColorY = FLinearColor(0.1349f,0.3959f,0.0f);
	AxisColorZ = FLinearColor(0.0251f,0.207f,0.85f);
	PlaneColorXY = FColor(255,255,0);
	ScreenSpaceColor  = FColor(196, 196, 196);
	CurrentColor = FColor(255,255,0);

	UMaterial* AxisMaterialBase = GEngine->ArrowMaterial;

	AxisMaterialX = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	AxisMaterialX->SetVectorParameterValue( "GizmoColor", AxisColorX );

	AxisMaterialY = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	AxisMaterialY->SetVectorParameterValue( "GizmoColor", AxisColorY );

	AxisMaterialZ = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	AxisMaterialZ->SetVectorParameterValue( "GizmoColor", AxisColorZ );

	CurrentAxisMaterial = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	CurrentAxisMaterial->SetVectorParameterValue("GizmoColor", CurrentColor );

	OpaquePlaneMaterialXY = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	OpaquePlaneMaterialXY->SetVectorParameterValue( "GizmoColor", FLinearColor::White );

	TransparentPlaneMaterialXY = (UMaterial*)StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("/Engine/EditorMaterials/WidgetVertexColorMaterial.WidgetVertexColorMaterial"),NULL,LOAD_None,NULL );
	
	GridMaterial = (UMaterial*)StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("/Engine/EditorMaterials/WidgetGridVertexColorMaterial_Ma.WidgetGridVertexColorMaterial_Ma"),NULL,LOAD_None,NULL );
	if (!GridMaterial)
	{
		GridMaterial = TransparentPlaneMaterialXY;
	}

	CurrentAxis = EAxisList::None;

	CustomCoordSystem = FMatrix::Identity;
	CustomCoordSystemSpace = COORD_World;

	bAbsoluteTranslationInitialOffsetCached = false;
	InitialTranslationOffset = FVector::ZeroVector;
	InitialTranslationPosition = FVector(0, 0, 0);

	bDragging = false;
	bSnapEnabled = false;
}

extern ENGINE_API void StringSize(UFont* Font,int32& XL,int32& YL,const TCHAR* Text, FCanvas* Canvas);

void FWidget::SetUsesEditorModeTools( FEditorModeTools* InEditorModeTools )
{	
	EditorModeTools = InEditorModeTools;
}

/**
 * Renders any widget specific HUD text
 * @param Canvas - Canvas to use for 2d rendering
 */
void FWidget::DrawHUD (FCanvas* Canvas)
{
	if (HUDString.Len())
	{
		int32 StringPosX = FMath::FloorToInt(HUDInfoPos.X);
		int32 StringPosY = FMath::FloorToInt(HUDInfoPos.Y);

		//measure string size
		int32 StringSizeX, StringSizeY;
		StringSize(GEngine->GetSmallFont(), StringSizeX, StringSizeY, *HUDString);
		
		//add some padding to the outside
		const int32 Border = 5;
		int32 FillMinX = StringPosX - Border - (StringSizeX>>1);
		int32 FillMinY = StringPosY - Border;// - (StringSizeY>>1);
		StringSizeX += 2*Border;
		StringSizeY += 2*Border;

		//mostly alpha'ed black
		FCanvasTileItem TileItem( FVector2D( FillMinX, FillMinY), GWhiteTexture, FVector2D( StringSizeX, StringSizeY ), FLinearColor( 0.0f, 0.0f, 0.0f, .25f ) );
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem( TileItem );
		FCanvasTextItem TextItem( FVector2D( StringPosX, StringPosY), FText::FromString( HUDString ), GEngine->GetSmallFont(), FLinearColor::White );
		TextItem.bCentreX = true;
		Canvas->DrawItem( TextItem );	
	}
}

void FWidget::Render( const FSceneView* View,FPrimitiveDrawInterface* PDI, FEditorViewportClient* ViewportClient )
{
	check(ViewportClient);

	TArray<FEdMode*> ActiveModes;
	if( EditorModeTools )
	{
		EditorModeTools->GetActiveModes( ActiveModes );
	}

	//reset HUD text
	HUDString.Empty();

	bool bDrawModeSupportsWidgetDrawing = true;

	if( EditorModeTools )
	{
		bDrawModeSupportsWidgetDrawing = false;
		// Check to see of any active modes support widget drawing
		for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
		{
			bDrawModeSupportsWidgetDrawing |= ActiveModes[ModeIndex]->ShouldDrawWidget();
		}
	}


	const bool bShowFlagsSupportsWidgetDrawing = View->Family->EngineShowFlags.ModeWidgets;
	const bool bEditorModeToolsSupportsWidgetDrawing = EditorModeTools ? EditorModeTools->GetShowWidget() : true;
	bool bDrawWidget;

	// Because the movement routines use the widget axis to determine how to transform mouse movement into
	// editor object movement, we need to still run through the Render routine even though widget drawing may be
	// disabled.  So we keep a flag that is used to determine whether or not to actually render anything.  This way
	// we can still update the widget axis' based on the Context's transform matrices, even though drawing is disabled.
	if(bDrawModeSupportsWidgetDrawing && bShowFlagsSupportsWidgetDrawing && bEditorModeToolsSupportsWidgetDrawing)
	{
		bDrawWidget = true;

		// See if there is a custom coordinate system we should be using, only change it if we are drawing widgets.
		CustomCoordSystem = ViewportClient->GetWidgetCoordSystem();
	}
	else
	{
		bDrawWidget = false;
	}

	CustomCoordSystemSpace = ViewportClient->GetWidgetCoordSystemSpace();

	// If the current modes don't want to use the widget, don't draw it.
	if( EditorModeTools && !EditorModeTools->UsesTransformWidget() )
	{
		CurrentAxis = EAxisList::None;
		return;
	}

	FVector Loc = ViewportClient->GetWidgetLocation();
	if(!View->ScreenToPixel(View->WorldToScreen(Loc),Origin))
	{
		Origin.X = Origin.Y = 0;
	}

	switch( ViewportClient->GetWidgetMode() )
	{
		case WM_Translate:
			Render_Translate( View, PDI, ViewportClient, Loc, bDrawWidget );
			break;

		case WM_Rotate:
			Render_Rotate( View, PDI, ViewportClient, Loc, bDrawWidget );
			break;

		case WM_Scale:
			Render_Scale( View, PDI, ViewportClient, Loc, bDrawWidget );
			break;

		case WM_TranslateRotateZ:
			Render_TranslateRotateZ( View, PDI, ViewportClient, Loc, bDrawWidget );
			break;

		default:
			break;
	}
};

/**
 * Draws an arrow head line for a specific axis.
 */
void FWidget::Render_Axis( const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, FMatrix& InMatrix, UMaterialInterface* InMaterial, const FLinearColor& InColor, FVector2D& OutAxisEnd, const FVector& InScale, bool bDrawWidget, bool bCubeHead )
{
	FMatrix AxisRotation = FMatrix::Identity;
	if( InAxis == EAxisList::Y )
	{
		AxisRotation = FRotationMatrix( FRotator(0,90.f,0) );
	}
	else if( InAxis == EAxisList::Z )
	{
		AxisRotation = FRotationMatrix( FRotator(90.f,0,0) );
	}

	FMatrix ArrowToWorld = AxisRotation * InMatrix;

	// The scale that is passed in potentially leaves one component with a scale of 1, if that happens
	// we need to extract the inform scale and use it to construct the scale that transforms the primitives
	float UniformScale = InScale.GetMax() > 1.0f ? InScale.GetMax() : InScale.GetMin() < 1.0f ? InScale.GetMin() : 1.0f;
	// After the primitives have been scaled and transformed, we apply this inverse scale that flattens the dimension
	// that was scaled up to prevent it from intersecting with the near plane.  In perspective this won't have any effect,
	// but in the ortho viewports it will prevent scaling in the direction of the camera and thus intersecting the near plane.
	FVector FlattenScale = FVector(InScale.Component(0) == 1.0f ? 1.0f / UniformScale : 1.0f, InScale.Component(1) == 1.0f ? 1.0f / UniformScale : 1.0f, InScale.Component(2) == 1.0f ? 1.0f / UniformScale : 1.0f);

	FScaleMatrix Scale(UniformScale);
	ArrowToWorld = Scale * ArrowToWorld;

	if( bDrawWidget )
	{
		const bool bDisabled = EditorModeTools ? (EditorModeTools->IsModeActive(FBuiltinEditorModes::EM_Default) && GEditor->HasLockedActors() ) : false;
		PDI->SetHitProxy( new HWidgetAxis( InAxis, bDisabled) );

		const float AxisLength = AXIS_LENGTH + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;
		const float HalfHeight = AxisLength/2.0f;
		const float CylinderRadius = 1.2f;
		const FVector Offset( 0,0,HalfHeight );

		switch( InAxis )
		{
			case EAxisList::X:
			{
				DrawCylinder(PDI, ( Scale * FRotationMatrix(FRotator(-90, 0.f, 0)) * InMatrix ) * FScaleMatrix(FlattenScale), Offset, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), CylinderRadius, HalfHeight, 16, InMaterial->GetRenderProxy(false), SDPG_Foreground);
				break;
			}
			case EAxisList::Y:
			{
				DrawCylinder(PDI, (Scale * FRotationMatrix(FRotator(0, 0, 90)) * InMatrix)* FScaleMatrix(FlattenScale), Offset, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), CylinderRadius, HalfHeight, 16, InMaterial->GetRenderProxy(false), SDPG_Foreground );
				break;
			}
			case EAxisList::Z:
			{
				DrawCylinder(PDI, ( Scale * InMatrix ) * FScaleMatrix(FlattenScale), Offset, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), CylinderRadius, HalfHeight, 16, InMaterial->GetRenderProxy(false), SDPG_Foreground);
				break;
			}
		}

		if ( bCubeHead )
		{
			const float CubeHeadOffset = 3.0f;
			FVector RootPos(AxisLength + CubeHeadOffset, 0, 0);

			Render_Cube(PDI, (FTranslationMatrix(RootPos) * ArrowToWorld) * FScaleMatrix(FlattenScale), InMaterial, FVector(4.0f));
		}
		else
		{
			const float ConeHeadOffset = 12.0f;
			FVector RootPos(AxisLength + ConeHeadOffset, 0, 0);

			float Angle = FMath::DegreesToRadians( PI * 5 );
			DrawCone(PDI, ( FScaleMatrix(-13) * FTranslationMatrix(RootPos) * ArrowToWorld ) * FScaleMatrix(FlattenScale), Angle, Angle, 32, false, FColor::White, InMaterial->GetRenderProxy(false), SDPG_Foreground);
		}
	
		PDI->SetHitProxy( NULL );
	}

	if(!View->ScreenToPixel(View->WorldToScreen(ArrowToWorld.TransformPosition(FVector(64,0,0))),OutAxisEnd))
	{
		OutAxisEnd.X = OutAxisEnd.Y = 0;
	}
}

void FWidget::Render_Cube( FPrimitiveDrawInterface* PDI, const FMatrix& InMatrix, const UMaterialInterface* InMaterial, const FVector& InScale )
{
	const FMatrix CubeToWorld = FScaleMatrix(InScale) * InMatrix;
	DrawBox( PDI, CubeToWorld, FVector(1,1,1), InMaterial->GetRenderProxy( false ), SDPG_Foreground );
}

void DrawCornerHelper( FPrimitiveDrawInterface* PDI, const FMatrix& LocalToWorld, const FVector& Length, float Thickness, const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriorityGroup  )
{
	const float TH = Thickness;

	float TX = Length.X/2;
	float TY = Length.Y/2;
	float TZ = Length.Z/2;

	FDynamicMeshBuilder MeshBuilder;

	// Top
	{
		int32 VertexIndices[4];
		VertexIndices[0] = MeshBuilder.AddVertex( FVector(-TX, -TY, +TZ), FVector2D::ZeroVector, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor::White );
		VertexIndices[1] = MeshBuilder.AddVertex( FVector(-TX, +TY, +TZ), FVector2D::ZeroVector, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor::White );
		VertexIndices[2] = MeshBuilder.AddVertex( FVector(+TX, +TY, +TZ), FVector2D::ZeroVector, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor::White );
		VertexIndices[3] = MeshBuilder.AddVertex( FVector(+TX, -TY, +TZ), FVector2D::ZeroVector, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor::White );

		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2]);
		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[2],VertexIndices[3]);
	}

	//Left
	{
		int32 VertexIndices[4];
		VertexIndices[0] = MeshBuilder.AddVertex( FVector(-TX,  -TY, TZ-TH),	FVector2D::ZeroVector, FVector(0,0,1), FVector(0,1,0), FVector(-1,0,0), FColor::White );
		VertexIndices[1] = MeshBuilder.AddVertex( FVector(-TX, -TY, TZ),		FVector2D::ZeroVector, FVector(0,0,1), FVector(0,1,0), FVector(-1,0,0), FColor::White );
		VertexIndices[2] = MeshBuilder.AddVertex( FVector(-TX, +TY, TZ),		FVector2D::ZeroVector, FVector(0,0,1), FVector(0,1,0), FVector(-1,0,0), FColor::White );
		VertexIndices[3] = MeshBuilder.AddVertex( FVector(-TX, +TY, TZ-TH),		FVector2D::ZeroVector, FVector(0,0,1), FVector(0,1,0), FVector(-1,0,0), FColor::White );


		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2]);
		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[2],VertexIndices[3]);
	}

	// Front
	{
		int32 VertexIndices[5];
		VertexIndices[0] = MeshBuilder.AddVertex( FVector(-TX,	+TY, TZ-TH),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0), FColor::White );
		VertexIndices[1] = MeshBuilder.AddVertex( FVector(-TX,	+TY, +TZ  ),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0), FColor::White );
		VertexIndices[2] = MeshBuilder.AddVertex( FVector(+TX-TH, +TY, +TX  ),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0), FColor::White );
		VertexIndices[3] = MeshBuilder.AddVertex( FVector(+TX,	+TY, +TZ  ),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0), FColor::White );
		VertexIndices[4] = MeshBuilder.AddVertex( FVector(+TX-TH, +TY, TZ-TH),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0), FColor::White );

		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2]);
		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[2],VertexIndices[4]);
		MeshBuilder.AddTriangle(VertexIndices[4],VertexIndices[2],VertexIndices[3]);
	}

	// Back
	{
		int32 VertexIndices[5];
		VertexIndices[0] = MeshBuilder.AddVertex( FVector(-TX,	-TY, TZ-TH),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0), FColor::White );
		VertexIndices[1] = MeshBuilder.AddVertex( FVector(-TX,	-TY, +TZ),		FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0), FColor::White );
		VertexIndices[2] = MeshBuilder.AddVertex( FVector(+TX-TH, -TY, +TX),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0), FColor::White );
		VertexIndices[3] = MeshBuilder.AddVertex( FVector(+TX,	-TY, +TZ),		FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0), FColor::White );
		VertexIndices[4] = MeshBuilder.AddVertex( FVector(+TX-TH, -TY, TZ-TH),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0), FColor::White );

		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2]);
		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[2],VertexIndices[4]);
		MeshBuilder.AddTriangle(VertexIndices[4],VertexIndices[2],VertexIndices[3]);
	}
	// Bottom
	{
		int32 VertexIndices[4];
		VertexIndices[0] = MeshBuilder.AddVertex( FVector(-TX, -TY, TZ-TH),		FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,0,1), FColor::White );
		VertexIndices[1] = MeshBuilder.AddVertex( FVector(-TX, +TY, TZ-TH),		FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,0,1), FColor::White );
		VertexIndices[2] = MeshBuilder.AddVertex( FVector(+TX-TH, +TY, TZ-TH),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,0,1), FColor::White );
		VertexIndices[3] = MeshBuilder.AddVertex( FVector(+TX-TH, -TY, TZ-TH),	FVector2D::ZeroVector, FVector(1,0,0), FVector(0,0,-1), FVector(0,0,1), FColor::White );

		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2]);
		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[2],VertexIndices[3]);
	}
	MeshBuilder.Draw(PDI,LocalToWorld,MaterialRenderProxy,DepthPriorityGroup,0.f);
}

void DrawDualAxis( FPrimitiveDrawInterface* PDI, const FMatrix& BoxToWorld,const FVector& Length, float Thickness, const FMaterialRenderProxy* AxisMat,const FMaterialRenderProxy* Axis2Mat )
{
	DrawCornerHelper(PDI, BoxToWorld, Length, Thickness, Axis2Mat, SDPG_Foreground);
	DrawCornerHelper(PDI, FScaleMatrix(FVector(-1, 1, 1)) * FRotationMatrix(FRotator(-90, 0, 0)) * BoxToWorld, Length, Thickness, AxisMat, SDPG_Foreground);
}
/**
 * Draws the translation widget.
 */
void FWidget::Render_Translate( const FSceneView* View, FPrimitiveDrawInterface* PDI, FEditorViewportClient* ViewportClient, const FVector& InLocation, bool bDrawWidget )
{
	// Figure out axis colors
	const FLinearColor& XColor = ( CurrentAxis&EAxisList::X ? (FLinearColor)CurrentColor : AxisColorX );
	const FLinearColor& YColor = ( CurrentAxis&EAxisList::Y ? (FLinearColor)CurrentColor : AxisColorY );
	const FLinearColor& ZColor = ( CurrentAxis&EAxisList::Z ? (FLinearColor)CurrentColor : AxisColorZ );
	FColor CurrentScreenColor = ( CurrentAxis & EAxisList::Screen ? CurrentColor : ScreenSpaceColor );

	// Figure out axis matrices
	FMatrix WidgetMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );

	bool bIsPerspective = ( View->ViewMatrices.ProjMatrix.M[3][3] < 1.0f );
	const bool bIsOrthoXY = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[2][2]) > 0.0f;
	const bool bIsOrthoXZ = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[1][2]) > 0.0f;
	const bool bIsOrthoYZ = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[0][2]) > 0.0f;

	// For local space widgets, we always want to draw all three axis, since they may not be aligned with
	// the orthographic projection anyway.
	const bool bIsLocalSpace = ( ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local );

	const EAxisList::Type DrawAxis = GetAxisToDraw( ViewportClient->GetWidgetMode() );

	const bool bDisabled = IsWidgetDisabled();

	FVector Scale;
	float UniformScale = View->WorldToScreen(InLocation).W * ( 4.0f / View->ViewRect.Width() / View->ViewMatrices.ProjMatrix.M[0][0] );

	if ( bIsOrthoXY )
	{
		Scale = FVector(UniformScale, UniformScale, 1.0f);
	}
	else if ( bIsOrthoXZ )
	{
		Scale = FVector(UniformScale, 1.0f, UniformScale);
	}
	else if ( bIsOrthoYZ )
	{
		Scale = FVector(1.0f, UniformScale, UniformScale);
	}
	else
	{
		Scale = FVector(UniformScale, UniformScale, UniformScale);
	}

	// Draw the axis lines with arrow heads
	if( DrawAxis&EAxisList::X && (bIsPerspective || bIsLocalSpace || !bIsOrthoYZ) )
	{
		UMaterialInstanceDynamic* XMaterial = ( CurrentAxis&EAxisList::X ? CurrentAxisMaterial : AxisMaterialX );
		Render_Axis( View, PDI, EAxisList::X, WidgetMatrix, XMaterial, XColor, XAxisEnd, Scale, bDrawWidget );
	}

	if( DrawAxis&EAxisList::Y && (bIsPerspective || bIsLocalSpace || !bIsOrthoXZ) )
	{
		UMaterialInstanceDynamic* YMaterial = ( CurrentAxis&EAxisList::Y ? CurrentAxisMaterial : AxisMaterialY );
		Render_Axis( View, PDI, EAxisList::Y, WidgetMatrix, YMaterial, YColor, YAxisEnd, Scale, bDrawWidget );
	}

	if( DrawAxis&EAxisList::Z && (bIsPerspective || bIsLocalSpace || !bIsOrthoXY) )
	{
		UMaterialInstanceDynamic* ZMaterial = ( CurrentAxis&EAxisList::Z ? CurrentAxisMaterial : AxisMaterialZ );
		Render_Axis( View, PDI, EAxisList::Z, WidgetMatrix, ZMaterial, ZColor, ZAxisEnd, Scale, bDrawWidget );
	}

	// Draw the grabbers
	if( bDrawWidget )
	{
		FVector CornerPos = FVector(7, 0, 7) * UniformScale;
		FVector AxisSize = FVector(12, 1.2, 12) * UniformScale;
		float CornerLength = 1.2f * UniformScale;

		// After the primitives have been scaled and transformed, we apply this inverse scale that flattens the dimension
		// that was scaled up to prevent it from intersecting with the near plane.  In perspective this won't have any effect,
		// but in the ortho viewports it will prevent scaling in the direction of the camera and thus intersecting the near plane.
		FVector FlattenScale = FVector(Scale.Component(0) == 1.0f ? 1.0f / UniformScale : 1.0f, Scale.Component(1) == 1.0f ? 1.0f / UniformScale : 1.0f, Scale.Component(2) == 1.0f ? 1.0f / UniformScale : 1.0f);

		if( bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[2][1] == 0.f )
		{
			if( (DrawAxis&EAxisList::XY) == EAxisList::XY )							// Top
			{
				UMaterialInstanceDynamic* XMaterial = ( (CurrentAxis&EAxisList::XY) == EAxisList::XY ? CurrentAxisMaterial : AxisMaterialX );
				UMaterialInstanceDynamic* YMaterial = ( (CurrentAxis&EAxisList::XY) == EAxisList::XY? CurrentAxisMaterial : AxisMaterialY );

				PDI->SetHitProxy( new HWidgetAxis(EAxisList::XY, bDisabled) );
				{
					DrawDualAxis(PDI, ( FTranslationMatrix(CornerPos) * FRotationMatrix(FRotator(0, 0, 90)) * WidgetMatrix ) * FScaleMatrix(FlattenScale), AxisSize, CornerLength, XMaterial->GetRenderProxy(false), YMaterial->GetRenderProxy(false));
				}
				PDI->SetHitProxy( NULL );
			}
		}

		if( bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[1][2] == -1.f )		// Front
		{
			if( (DrawAxis&EAxisList::XZ) == EAxisList::XZ ) 
			{
				UMaterialInstanceDynamic* XMaterial = ( (CurrentAxis&EAxisList::XZ) == EAxisList::XZ ? CurrentAxisMaterial : AxisMaterialX );
				UMaterialInstanceDynamic* ZMaterial = ( (CurrentAxis&EAxisList::XZ) == EAxisList::XZ ? CurrentAxisMaterial : AxisMaterialZ );

				PDI->SetHitProxy( new HWidgetAxis(EAxisList::XZ, bDisabled) );
				{
					DrawDualAxis(PDI, (FTranslationMatrix(CornerPos) * WidgetMatrix) * FScaleMatrix(FlattenScale), AxisSize, CornerLength, XMaterial->GetRenderProxy(false), ZMaterial->GetRenderProxy(false) );
				}
				PDI->SetHitProxy( NULL );
			}
		}

		if( bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[1][0] == 1.f )		// Side
		{
			if( (DrawAxis&EAxisList::YZ) == EAxisList::YZ ) 
			{
				UMaterialInstanceDynamic* YMaterial = ( (CurrentAxis&EAxisList::YZ) == EAxisList::YZ ? CurrentAxisMaterial : AxisMaterialY );
				UMaterialInstanceDynamic* ZMaterial = ( (CurrentAxis&EAxisList::YZ) == EAxisList::YZ ? CurrentAxisMaterial : AxisMaterialZ );

				PDI->SetHitProxy( new HWidgetAxis(EAxisList::YZ, bDisabled) );
				{
					DrawDualAxis(PDI, (FTranslationMatrix(CornerPos) * FRotationMatrix(FRotator(0, 90, 0)) * WidgetMatrix) * FScaleMatrix(FlattenScale), AxisSize, CornerLength, YMaterial->GetRenderProxy(false), ZMaterial->GetRenderProxy(false) );
				}
				PDI->SetHitProxy( NULL );
			}
		}
	}

	// Draw screen-space movement handle (circle)
	if( bDrawWidget && ( DrawAxis & EAxisList::Screen ) && bIsPerspective )
	{
		PDI->SetHitProxy( new HWidgetAxis(EAxisList::Screen, bDisabled) );
		const FVector CameraXAxis = View->ViewMatrices.ViewMatrix.GetColumn(0);
		const FVector CameraYAxis = View->ViewMatrices.ViewMatrix.GetColumn(1);
		const FVector CameraZAxis = View->ViewMatrices.ViewMatrix.GetColumn(2);

		UMaterialInstanceDynamic* XYZMaterial = ( CurrentAxis&EAxisList::Screen) ? CurrentAxisMaterial : OpaquePlaneMaterialXY;
		DrawSphere( PDI, InLocation, 4.0f * Scale, 10, 5, XYZMaterial->GetRenderProxy(false), SDPG_Foreground );

		PDI->SetHitProxy( NULL );
	}
}

/**
 * Draws the rotation widget.
 */
void FWidget::Render_Rotate( const FSceneView* View,FPrimitiveDrawInterface* PDI, FEditorViewportClient* ViewportClient, const FVector& InLocation, bool bDrawWidget )
{
	float Scale = View->WorldToScreen( InLocation ).W * ( 4.0f / View->ViewRect.Width() / View->ViewMatrices.ProjMatrix.M[0][0] );

	//get the axes 
	FVector XAxis = CustomCoordSystem.TransformVector(FVector(-1, 0, 0));
	FVector YAxis = CustomCoordSystem.TransformVector(FVector(0, -1, 0));
	FVector ZAxis = CustomCoordSystem.TransformVector(FVector(0, 0, 1));

	EAxisList::Type DrawAxis = GetAxisToDraw( ViewportClient->GetWidgetMode() );

	FMatrix XMatrix = FRotationMatrix( FRotator(0,90.f,0) ) * FTranslationMatrix( InLocation );

	FVector DirectionToWidget = View->IsPerspectiveProjection() ? (InLocation - View->ViewMatrices.ViewOrigin) : -View->GetViewDirection();
	DirectionToWidget.Normalize();

	// Draw a circle for each axis
	if (bDrawWidget || bDragging)
	{
		//no draw the arc segments
		if( DrawAxis&EAxisList::X )
		{
			DrawRotationArc(View, PDI, EAxisList::X, InLocation, YAxis, ZAxis, DirectionToWidget, AxisColorX, Scale);
		}

		if( DrawAxis&EAxisList::Y )
		{
			DrawRotationArc(View, PDI, EAxisList::Y, InLocation, ZAxis, XAxis, DirectionToWidget, AxisColorY, Scale);
		}

		if( DrawAxis&EAxisList::Z )
		{
			DrawRotationArc(View, PDI, EAxisList::Z, InLocation, XAxis, YAxis, DirectionToWidget, AxisColorZ, Scale);
		}
	}

	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformPosition( FVector(96,0,0) ) ), XAxisEnd);
	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformPosition( FVector(0,96,0) ) ), YAxisEnd);
	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformPosition( FVector(0,0,96) ) ), ZAxisEnd);
}

/**
 * Draws the scaling widget.
 */
void FWidget::Render_Scale( const FSceneView* View,FPrimitiveDrawInterface* PDI, FEditorViewportClient* ViewportClient, const FVector& InLocation, bool bDrawWidget )
{
	// Figure out axis colors
	const FLinearColor& XColor = ( CurrentAxis&EAxisList::X ? (FLinearColor)CurrentColor : AxisColorX );
	const FLinearColor& YColor = ( CurrentAxis&EAxisList::Y ? (FLinearColor)CurrentColor : AxisColorY );
	const FLinearColor& ZColor = ( CurrentAxis&EAxisList::Z ? (FLinearColor)CurrentColor : AxisColorZ );
	FColor CurrentScreenColor = ( CurrentAxis & EAxisList::Screen ? CurrentColor : ScreenSpaceColor );

	// Figure out axis materials

	UMaterialInstanceDynamic* XMaterial = ( CurrentAxis&EAxisList::X ? CurrentAxisMaterial : AxisMaterialX );
	UMaterialInstanceDynamic* YMaterial = ( CurrentAxis&EAxisList::Y ? CurrentAxisMaterial : AxisMaterialY );
	UMaterialInstanceDynamic* ZMaterial = ( CurrentAxis&EAxisList::Z ? CurrentAxisMaterial : AxisMaterialZ );
	UMaterialInstanceDynamic* XYZMaterial = ( CurrentAxis&EAxisList::XYZ ? CurrentAxisMaterial : OpaquePlaneMaterialXY );

	// Figure out axis matrices

	FMatrix WidgetMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );
	// Determine viewport

	const EAxisList::Type DrawAxis = GetAxisToDraw( ViewportClient->GetWidgetMode() );
	const bool bIsPerspective = ( View->ViewMatrices.ProjMatrix.M[3][3] < 1.0f );
	const bool bIsOrthoXY = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[2][2]) > 0.0f;
	const bool bIsOrthoXZ = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[1][2]) > 0.0f;
	const bool bIsOrthoYZ = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[0][2]) > 0.0f;

	FVector Scale;
	const float UniformScale = View->WorldToScreen(InLocation).W * ( 4.0f / View->ViewRect.Width() / View->ViewMatrices.ProjMatrix.M[0][0] );

	if ( bIsOrthoXY )
	{
		Scale = FVector(UniformScale, UniformScale, 1.0f);
	}
	else if ( bIsOrthoXZ )
	{
		Scale = FVector(UniformScale, 1.0f, UniformScale);
	}
	else if ( bIsOrthoYZ )
	{
		Scale = FVector(1.0f, UniformScale, UniformScale);
	}
	else
	{
		Scale = FVector(UniformScale, UniformScale, UniformScale);
	}

	// Draw the axis lines with cube heads	
	if( !bIsOrthoYZ && DrawAxis&EAxisList::X )
	{
		Render_Axis( View, PDI, EAxisList::X, WidgetMatrix, XMaterial, XColor, XAxisEnd, Scale, bDrawWidget, true );
	}

	if( !bIsOrthoXZ && DrawAxis&EAxisList::Y )
	{
		Render_Axis( View, PDI, EAxisList::Y, WidgetMatrix, YMaterial, YColor, YAxisEnd, Scale, bDrawWidget, true );
	}

	if( !bIsOrthoXY &&  DrawAxis&EAxisList::Z )
	{
		Render_Axis( View, PDI, EAxisList::Z, WidgetMatrix, ZMaterial, ZColor, ZAxisEnd, Scale, bDrawWidget, true );
	}

	// Draw grabber handles and center cube
	if ( bDrawWidget )
	{
		const bool bDisabled = IsWidgetDisabled();

		// Grabber handles
		if( !bIsOrthoYZ && !bIsOrthoXZ && ((DrawAxis&(EAxisList::X|EAxisList::Y)) == (EAxisList::X|EAxisList::Y)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::XY, bDisabled) );
			{
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(24,0,0) * Scale), WidgetMatrix.TransformPosition(FVector(12,12,0) * Scale), XColor, SDPG_Foreground );
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(12,12,0) * Scale), WidgetMatrix.TransformPosition(FVector(0,24,0) * Scale), YColor, SDPG_Foreground );
			}
			PDI->SetHitProxy( NULL );
		}

		if( !bIsOrthoYZ && !bIsOrthoXY && ((DrawAxis&(EAxisList::X|EAxisList::Z)) == (EAxisList::X|EAxisList::Z)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::XZ, bDisabled) );
			{
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(24,0,0) * Scale), WidgetMatrix.TransformPosition(FVector(12,0,12) * Scale), XColor, SDPG_Foreground );
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(12,0,12) * Scale), WidgetMatrix.TransformPosition(FVector(0,0,24) * Scale), ZColor, SDPG_Foreground );
			}
			PDI->SetHitProxy( NULL );
		}

		if( !bIsOrthoXY && !bIsOrthoXZ && ((DrawAxis&(EAxisList::Y|EAxisList::Z)) == (EAxisList::Y|EAxisList::Z)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::YZ, bDisabled) );
			{
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(0,24,0) * Scale), WidgetMatrix.TransformPosition(FVector(0,12,12) * Scale), YColor, SDPG_Foreground );
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(0,12,12) * Scale), WidgetMatrix.TransformPosition(FVector(0,0,24) * Scale), ZColor, SDPG_Foreground );
			}
			PDI->SetHitProxy( NULL );
		}

		// Center cube
		if( (DrawAxis&(EAxisList::XYZ)) == EAxisList::XYZ )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::XYZ, bDisabled) );

			Render_Cube(PDI, WidgetMatrix, XYZMaterial, Scale * 4 );

			PDI->SetHitProxy( NULL );
		}
	}
}

/**
* Draws the Translate & Rotate Z widget.
*/

void FWidget::Render_TranslateRotateZ( const FSceneView* View, FPrimitiveDrawInterface* PDI, FEditorViewportClient* ViewportClient, const FVector& InLocation, bool bDrawWidget )
{
	// Figure out axis colors

	FColor XYPlaneColor  = ( (CurrentAxis&EAxisList::XY) ==  EAxisList::XY) ? CurrentColor : PlaneColorXY;
	FColor ZRotateColor  = ( (CurrentAxis&EAxisList::ZRotation) == EAxisList::ZRotation ) ? CurrentColor : (FColor)AxisColorZ ;
	FColor XColor        = ( (CurrentAxis&EAxisList::X ) == EAxisList::X )? CurrentColor : (FColor)AxisColorX;
	FColor YColor        = ( (CurrentAxis&EAxisList::Y) == EAxisList::Y && CurrentAxis != EAxisList::ZRotation ) ? CurrentColor : (FColor)AxisColorY;
	FColor ZColor        = ( (CurrentAxis&EAxisList::Z) == EAxisList::Z) ? CurrentColor : (FColor)AxisColorZ;

	// Figure out axis materials
	UMaterialInterface* ZRotateMaterial = (CurrentAxis&EAxisList::ZRotation) == EAxisList::ZRotation? CurrentAxisMaterial : AxisMaterialZ;
	UMaterialInterface* XMaterial = CurrentAxis&EAxisList::X? CurrentAxisMaterial : AxisMaterialX;
	UMaterialInterface* YMaterial = ( CurrentAxis&EAxisList::Y && CurrentAxis != EAxisList::ZRotation ) ? CurrentAxisMaterial : AxisMaterialY;
	UMaterialInterface* ZMaterial = CurrentAxis&EAxisList::Z ? CurrentAxisMaterial : AxisMaterialZ;

	// Figure out axis matrices
	FMatrix AxisMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );

	bool bIsPerspective = ( View->ViewMatrices.ProjMatrix.M[3][3] < 1.0f );
	const bool bIsOrthoXY = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[2][2]) > 0.0f;
	const bool bIsOrthoXZ = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[1][2]) > 0.0f;
	const bool bIsOrthoYZ = !bIsPerspective && FMath::Abs(View->ViewMatrices.ViewMatrix.M[0][2]) > 0.0f;

	// For local space widgets, we always want to draw all three axis, since they may not be aligned with
	// the orthographic projection anyway.
	bool bIsLocalSpace = ( ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local );

	EAxisList::Type DrawAxis = GetAxisToDraw( ViewportClient->GetWidgetMode() );

	FVector Scale;
	float UniformScale = View->WorldToScreen(InLocation).W * ( 4.0f / View->ViewRect.Width() / View->ViewMatrices.ProjMatrix.M[0][0] );

	if ( bIsOrthoXY )
	{
		Scale = FVector(UniformScale, UniformScale, 1.0f);
	}
	else if ( bIsOrthoXZ )
	{
		Scale = FVector(UniformScale, 1.0f, UniformScale);
	}
	else if ( bIsOrthoYZ )
	{
		Scale = FVector(1.0f, UniformScale, UniformScale);
	}
	else
	{
		Scale = FVector(UniformScale, UniformScale, UniformScale);
	}

	// Draw the grabbers
	if( bDrawWidget )
	{
		// Draw the axis lines with arrow heads
		if( DrawAxis&EAxisList::X && (bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[0][2] != -1.f) )
		{
			Render_Axis( View, PDI, EAxisList::X, AxisMatrix, XMaterial, XColor, XAxisEnd, Scale, bDrawWidget );
		}

		if( DrawAxis&EAxisList::Y && (bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[1][2] != -1.f) )
		{
			Render_Axis( View, PDI, EAxisList::Y, AxisMatrix, YMaterial, YColor, YAxisEnd, Scale, bDrawWidget );
		}

		if( DrawAxis&EAxisList::Z && (bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[0][1] != 1.f) )
		{
			Render_Axis( View, PDI, EAxisList::Z, AxisMatrix, ZMaterial, ZColor, ZAxisEnd, Scale, bDrawWidget );
		}

		const bool bDisabled = IsWidgetDisabled();

		const float ScaledRadius = (TRANSLATE_ROTATE_AXIS_CIRCLE_RADIUS * UniformScale) + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;

		//ZRotation
		if( DrawAxis&EAxisList::ZRotation && (bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[0][2] != -1.f) )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::ZRotation, bDisabled) );
			{
				FVector XAxis = CustomCoordSystem.TransformPosition( FVector(1,0,0).RotateAngleAxis( (EditorModeTools ? EditorModeTools->TranslateRotateXAxisAngle : 0 ), FVector(0,0,1)) );
				FVector YAxis = CustomCoordSystem.TransformPosition( FVector(0,1,0).RotateAngleAxis( (EditorModeTools ? EditorModeTools->TranslateRotateXAxisAngle : 0 ), FVector(0,0,1)) );
				FVector BaseArrowPoint = InLocation + XAxis * ScaledRadius;
				DrawFlatArrow(PDI, BaseArrowPoint, XAxis, YAxis, ZRotateColor, ScaledRadius, ScaledRadius*.5f, ZRotateMaterial->GetRenderProxy(false), SDPG_Foreground);
			}
			PDI->SetHitProxy( NULL );
		}

		//XY Plane
		if( bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[0][1] != 1.f )
		{
			if( (DrawAxis & EAxisList::XY) == EAxisList::XY ) 
			{
				// Add more sides to the circle if we've been scaled up to keep the circle looking circular
				// An extra side for every 5 extra unreal units seems to produce a nice result
				const int32 CircleSides = (GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment > 0) 
					? AXIS_CIRCLE_SIDES + (GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment / 5)
					: AXIS_CIRCLE_SIDES;

				PDI->SetHitProxy( new HWidgetAxis(EAxisList::XY, bDisabled) );
				{
					DrawCircle( PDI, InLocation, CustomCoordSystem.TransformPosition( FVector(1,0,0) ), CustomCoordSystem.TransformPosition( FVector(0,1,0) ), XYPlaneColor, ScaledRadius, CircleSides, SDPG_Foreground );
					XYPlaneColor.A = ((CurrentAxis&EAxisList::XY) == EAxisList::XY) ? 0x3f : 0x0f;	//make the disc transparent
					DrawDisc  ( PDI, InLocation, CustomCoordSystem.TransformPosition( FVector(1,0,0) ), CustomCoordSystem.TransformPosition( FVector(0,1,0) ), XYPlaneColor, ScaledRadius, CircleSides, TransparentPlaneMaterialXY->GetRenderProxy(false), SDPG_Foreground );
				}
				PDI->SetHitProxy( NULL );
			}
		}
	}
}

/**
 * Converts mouse movement on the screen to widget axis movement/rotation.
 */
void FWidget::ConvertMouseMovementToAxisMovement( FEditorViewportClient* InViewportClient, const FVector& InLocation, const FVector& InDiff, FVector& InDrag, FRotator& InRotation, FVector& InScale )
{
	FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( InViewportClient->Viewport, InViewportClient->GetScene(), InViewportClient->EngineShowFlags ));
	FSceneView* View = InViewportClient->CalcSceneView(&ViewFamily);

	FPlane Wk;
	FVector2D AxisEnd;
	FVector Diff = InDiff;

	InDrag = FVector::ZeroVector;
	InRotation = FRotator::ZeroRotator;
	InScale = FVector::ZeroVector; 

	// Get the end of the axis (in screen space) based on which axis is being pulled

	switch( CurrentAxis )
	{
		case EAxisList::X:	AxisEnd = XAxisEnd;		break;
		case EAxisList::Y:	AxisEnd = YAxisEnd;		break;
		case EAxisList::Z:	AxisEnd = ZAxisEnd;		break;
		case EAxisList::XY:	AxisEnd = Diff.X != 0 ? XAxisEnd : YAxisEnd;		break;
		case EAxisList::XZ:	AxisEnd = Diff.X != 0 ? XAxisEnd : ZAxisEnd;		break;
		case EAxisList::YZ:	AxisEnd = Diff.X != 0 ? YAxisEnd : ZAxisEnd;		break;
		case EAxisList::XYZ:	AxisEnd = Diff.X != 0 ? YAxisEnd : ZAxisEnd;		break;
		default:
			break;
	}

	// Screen space Y axis is inverted

	Diff.Y *= -1;

	// Get the directions of the axis (on the screen) and the mouse drag direction (in screen space also).
	if(!View->ScreenToPixel(View->WorldToScreen(InLocation),Origin))
	{
		Origin.X = Origin.Y = 0;
	}


	FVector2D AxisDir = AxisEnd - Origin;
	AxisDir.Normalize();

	FVector2D DragDir( Diff.X, Diff.Y );
	DragDir.Normalize();


	// Use the most dominant axis the mouse is being dragged along -
	// unless we are modifying a single axis in an Ortho viewport.
	uint32 idx = GetDominantAxisIndex( Diff, InViewportClient );

	float Val = Diff[idx];

	FMatrix InputCoordSystem = InViewportClient->GetWidgetCoordSystem();

	const int32 WidgetMode = InViewportClient->GetWidgetMode();


	if( WidgetMode == FWidget::WM_Rotate && InViewportClient->IsPerspective() )
	{
		FVector DirectionToWidget = InLocation - View->ViewMatrices.ViewOrigin;

		FVector XAxis = InputCoordSystem.TransformVector(FVector(1, 0, 0));
		FVector YAxis = InputCoordSystem.TransformVector(FVector(0, 1, 0));
		FVector ZAxis = InputCoordSystem.TransformVector(FVector(0, 0, 1));

		float XDot = XAxis|DirectionToWidget;
		float YDot = YAxis|DirectionToWidget;
		float ZDot = ZAxis|DirectionToWidget;

		switch(CurrentAxis)
		{
		case EAxisList::X:
			if( FMath::IsNegativeFloat(XDot ) )
			{
				Val *= -1;
			}
			break;
		case EAxisList::Y:
			if( FMath::IsNegativeFloat(YDot) )
			{
				Val *= -1;
			}
			break;
		case EAxisList::Z:
			if( FMath::IsNegativeFloat(ZDot) )
			{
				Val *= -1;
			}
			break;
		}
	}
	else
	{

		// If the axis dir is negative, it is pointing in the negative screen direction.  In this situation, the mouse
		// drag must be inverted so that you are still dragging in the right logical direction.
		//
		// For example, if the X axis is pointing left and you drag left, this will ensure that the widget moves left.
		//Only valid for single axis movement.  For planar movement, this widget gets caught up at the origin and oscillates
		
		if(( AxisDir[idx] < 0 ) && ((CurrentAxis == EAxisList::X) || (CurrentAxis == EAxisList::Y) || (CurrentAxis == EAxisList::Z)))
		{
			Val *= -1;
		}
	}


	// Honor INI option to invert Z axis movement on the widget

	if( idx == 1 && (CurrentAxis&EAxisList::Z) && GEditor->InvertwidgetZAxis && ((WidgetMode==WM_Translate) || (WidgetMode==WM_Rotate) || (WidgetMode==WM_TranslateRotateZ)) &&
		// Don't apply this if the origin and the AxisEnd are the same
		AxisDir.IsNearlyZero() == false)
	{
		Val *= -1;
	}



	switch( WidgetMode )
	{
		case WM_Translate:
			switch( CurrentAxis )
			{
				case EAxisList::X:	InDrag = FVector( Val, 0, 0 );	break;
				case EAxisList::Y:	InDrag = FVector( 0, Val, 0 );	break;
				case EAxisList::Z:	InDrag = FVector( 0, 0, -Val );	break;
				case EAxisList::XY:	InDrag = ( InDiff.X != 0 ? FVector( Val, 0, 0 ) : FVector( 0, Val, 0 ) );	break;
				case EAxisList::XZ:	InDrag = ( InDiff.X != 0 ? FVector( Val, 0, 0 ) : FVector( 0, 0, Val ) );	break;
				case EAxisList::YZ:	InDrag = ( InDiff.X != 0 ? FVector( 0, Val, 0 ) : FVector( 0, 0, Val ) );	break;
			}

			InDrag = InputCoordSystem.TransformPosition( InDrag );
			break;

		case WM_Rotate:
			{
				FVector WorldManDir;
				FVector Axis;
				switch( CurrentAxis )
				{
					case EAxisList::X:	
						Axis = FVector( -1, 0, 0 );
						break;
					case EAxisList::Y:	
						Axis = FVector( 0, -1, 0 );	
						break;
					case EAxisList::Z:	
						Axis = FVector( 0, 0, 1 );	
						break;
					default:
						// Prevent this from crashing when axis incorrect and
						// make sure Axis is set to something sensible.
						ensureMsgf( 0, TEXT("Axis not correctly set while rotating! Axis value was %d"), (uint32)CurrentAxis );
						Axis = FVector( -1, 0, 0 );
						break;
				}
			
				Axis = InputCoordSystem.TransformVector( Axis );

				const float RotationSpeed = GetRotationSpeed();
				const FQuat DeltaQ( Axis, Val*RotationSpeed );
				CurrentDeltaRotation = Val;
				
				InRotation = FRotator(DeltaQ);
			}
			break;

		case WM_Scale:
			{
				FVector Axis;
				Axis.X = (CurrentAxis & EAxisList::X) == 0 ? 0 : 1;
				Axis.Y = (CurrentAxis & EAxisList::Y) == 0 ? 0 : 1;
				Axis.Z = (CurrentAxis & EAxisList::Z) == 0 ? 0 : 1;

				InScale = Axis * Val;
			}
			break;

		case WM_TranslateRotateZ:
			{
				if( CurrentAxis == EAxisList::ZRotation )
				{
					FVector Axis = Axis = FVector( 0, 0, 1 );
					Axis = InputCoordSystem.TransformVector( Axis );

					const float RotationSpeed = GetRotationSpeed();
					const FQuat DeltaQ( Axis, Val*RotationSpeed );
					CurrentDeltaRotation = Val;

					InRotation = FRotator(DeltaQ);
				}
				else
				{
					switch( CurrentAxis )
					{
						case EAxisList::X:	InDrag = FVector( Val, 0, 0 );	break;
						case EAxisList::Y:	InDrag = FVector( 0, Val, 0 );	break;
						case EAxisList::Z:	InDrag = FVector( 0, 0, -Val );	break;
						case EAxisList::XY:	InDrag = ( InDiff.X != 0 ? FVector( Val, 0, 0 ) : FVector( 0, Val, 0 ) );	break;
					}

					InDrag = InputCoordSystem.TransformPosition( InDrag );
				}
			}
			break;
		default:
			break;
	}
}

/**
 * For axis movement, get the "best" planar normal and axis mask
 * @param InAxis - Axis of movement
 * @param InDirToPixel - 
 * @param OutPlaneNormal - Normal of the plane to project the mouse onto
 * @param OutMask - Used to mask out the component of the planar movement we want
 */
void GetAxisPlaneNormalAndMask(const FMatrix& InCoordSystem, const FVector& InAxis, const FVector& InDirToPixel, FVector& OutPlaneNormal, FVector& NormalToRemove)
{
	FVector XAxis = InCoordSystem.TransformVector(FVector(1, 0, 0));
	FVector YAxis = InCoordSystem.TransformVector(FVector(0, 1, 0));
	FVector ZAxis = InCoordSystem.TransformVector(FVector(0, 0, 1));

	float XDot = FMath::Abs(InDirToPixel | XAxis);
	float YDot = FMath::Abs(InDirToPixel | YAxis);
	float ZDot = FMath::Abs(InDirToPixel | ZAxis);

	if ((InAxis|XAxis) > .1f)
	{
		OutPlaneNormal = (YDot > ZDot) ? YAxis : ZAxis;
		NormalToRemove = (YDot > ZDot) ? ZAxis : YAxis;
	}
	else if ((InAxis|YAxis) > .1f)
	{
		OutPlaneNormal = (XDot > ZDot) ? XAxis : ZAxis;
		NormalToRemove = (XDot > ZDot) ? ZAxis : XAxis;
	}
	else
	{
		OutPlaneNormal = (XDot > YDot) ? XAxis : YAxis;
		NormalToRemove = (XDot > YDot) ? YAxis : XAxis;
	}
}

/**
 * For planar movement, get the "best" planar normal and axis mask
 * @param InAxis - Axis of movement
 * @param OutPlaneNormal - Normal of the plane to project the mouse onto
 * @param OutMask - Used to mask out the component of the planar movement we want
 */
void GetPlaneNormalAndMask(const FVector& InAxis, FVector& OutPlaneNormal, FVector& NormalToRemove)
{
	OutPlaneNormal = InAxis;
	NormalToRemove = InAxis;
}

/**
 * Absolute Translation conversion from mouse movement on the screen to widget axis movement/rotation.
 */
void FWidget::AbsoluteTranslationConvertMouseMovementToAxisMovement(FSceneView* InView, FEditorViewportClient* InViewportClient, const FVector& InLocation, const FVector2D& InMousePosition, FVector& OutDrag, FRotator& OutRotation, FVector& OutScale )
{
	//reset all output variables
	//OutDrag = FVector::ZeroVector;
	//OutScale = FVector::ZeroVector;
	//OutRotation = FRotator::ZeroRotator;

	// Compute a world space ray from the screen space mouse coordinates
	FViewportCursorLocation MouseViewportRay( InView, InViewportClient, InMousePosition.X, InMousePosition.Y );

	FAbsoluteMovementParams Params;
	Params.EyePos = MouseViewportRay.GetOrigin();
	Params.PixelDir = MouseViewportRay.GetDirection();
	Params.CameraDir = InView->GetViewDirection();
	Params.Position = InLocation;
	//dampen by 
	Params.bMovementLockedToCamera = InViewportClient->IsShiftPressed();
	Params.bPositionSnapping = true;

	FMatrix InputCoordSystem = InViewportClient->GetWidgetCoordSystem();

	Params.XAxis = InputCoordSystem.TransformVector(FVector(1, 0, 0));
	Params.YAxis = InputCoordSystem.TransformVector(FVector(0, 1, 0));
	Params.ZAxis = InputCoordSystem.TransformVector(FVector(0, 0, 1));

	switch( InViewportClient->GetWidgetMode() )
	{
		case WM_Translate:
		{
			switch( CurrentAxis )
			{
			case EAxisList::X:  GetAxisPlaneNormalAndMask(InputCoordSystem, Params.XAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove); break;
			case EAxisList::Y:  GetAxisPlaneNormalAndMask(InputCoordSystem, Params.YAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove); break;
			case EAxisList::Z:  GetAxisPlaneNormalAndMask(InputCoordSystem, Params.ZAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove); break;
			case EAxisList::XY: GetPlaneNormalAndMask(Params.ZAxis, Params.PlaneNormal, Params.NormalToRemove); break;
			case EAxisList::XZ: GetPlaneNormalAndMask(Params.YAxis, Params.PlaneNormal, Params.NormalToRemove); break;
			case EAxisList::YZ: GetPlaneNormalAndMask(Params.XAxis, Params.PlaneNormal, Params.NormalToRemove); break;
			case EAxisList::Screen:
				Params.XAxis = InView->ViewMatrices.ViewMatrix.GetColumn(0);
				Params.YAxis = InView->ViewMatrices.ViewMatrix.GetColumn(1);
				Params.ZAxis = InView->ViewMatrices.ViewMatrix.GetColumn(2);
				GetPlaneNormalAndMask(Params.ZAxis, Params.PlaneNormal, Params.NormalToRemove); break;
				break;
			}

			OutDrag = GetAbsoluteTranslationDelta(Params);

			break;
		}

		case WM_TranslateRotateZ:
		{
			FVector LineToUse;
			switch( CurrentAxis )
			{
				case EAxisList::X:	
					{
						GetAxisPlaneNormalAndMask(InputCoordSystem, Params.XAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove);
						OutDrag = GetAbsoluteTranslationDelta (Params);
						break;
					}
				case EAxisList::Y:	
					{
						GetAxisPlaneNormalAndMask(InputCoordSystem, Params.YAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove);
						OutDrag = GetAbsoluteTranslationDelta (Params);
						break;
					}
				case EAxisList::Z:	
				{
					GetAxisPlaneNormalAndMask(InputCoordSystem, Params.ZAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove);
					OutDrag = GetAbsoluteTranslationDelta (Params);
					break;
				}
				case EAxisList::XY:
				{
					GetPlaneNormalAndMask(Params.ZAxis, Params.PlaneNormal, Params.NormalToRemove);
					OutDrag = GetAbsoluteTranslationDelta (Params);
					break;
				}
				//Rotate about the z-axis
				case EAxisList::ZRotation:
				{
					//no position snapping, we'll handle the rotation snapping elsewhere
					Params.bPositionSnapping = false;

					//find new point on the 
					GetPlaneNormalAndMask(Params.ZAxis, Params.PlaneNormal, Params.NormalToRemove);
					//No DAMPING
					Params.bMovementLockedToCamera = false;
					//this is the one movement type where we want to always use the widget origin and 
					//NOT the "first click" origin
					FVector XYPlaneProjectedPosition = GetAbsoluteTranslationDelta (Params) + InitialTranslationOffset;

					//remove the component along the normal we want to mute
					float MovementAlongMutedAxis = XYPlaneProjectedPosition|Params.NormalToRemove;
					XYPlaneProjectedPosition = XYPlaneProjectedPosition - (Params.NormalToRemove*MovementAlongMutedAxis);

					if (!XYPlaneProjectedPosition.Normalize())
					{
						XYPlaneProjectedPosition = Params.XAxis;
					}

					//NOW, find the rotation around the PlaneNormal to make the xaxis point at InDrag
					OutRotation = FRotator::ZeroRotator;

					OutRotation.Yaw = XYPlaneProjectedPosition.Rotation().Yaw - (EditorModeTools ? EditorModeTools->TranslateRotateXAxisAngle : 0 );

					if (bSnapEnabled)
					{
						FSnappingUtils::SnapRotatorToGrid( OutRotation );
					}

					break;
				}
				default:
					break;
			}
		}
	}
}

/** Only some modes support Absolute Translation Movement */
bool FWidget::AllowsAbsoluteTranslationMovement(EWidgetMode WidgetMode)
{
	if ((WidgetMode == WM_Translate) || (WidgetMode == WM_TranslateRotateZ))
	{
		return true;
	}
	return false;
}

/** 
 * Serializes the widget references so they don't get garbage collected.
 *
 * @param Ar	FArchive to serialize with
 */
void FWidget::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( AxisMaterialX );
	Collector.AddReferencedObject( AxisMaterialY );
	Collector.AddReferencedObject( AxisMaterialZ );
	Collector.AddReferencedObject( OpaquePlaneMaterialXY );
	Collector.AddReferencedObject( TransparentPlaneMaterialXY );
	Collector.AddReferencedObject( GridMaterial );
	Collector.AddReferencedObject( CurrentAxisMaterial );
}

#define CAMERA_LOCK_DAMPING_FACTOR .1f
#define MAX_CAMERA_MOVEMENT_SPEED 512.0f
/**
 * Returns the Delta from the current position that the absolute movement system wants the object to be at
 * @param InParams - Structure containing all the information needed for absolute movement
 * @return - The requested delta from the current position
 */
FVector FWidget::GetAbsoluteTranslationDelta(const FAbsoluteMovementParams& InParams)
{
	FPlane MovementPlane(InParams.Position, InParams.PlaneNormal);
	FVector ProposedEndofEyeVector = InParams.EyePos + (InParams.PixelDir * (InParams.Position - InParams.EyePos).Size());

	//default to not moving
	FVector RequestedPosition = InParams.Position;

	float DotProductWithPlaneNormal = InParams.PixelDir|InParams.PlaneNormal;
	//check to make sure we're not co-planar
	if (FMath::Abs(DotProductWithPlaneNormal) > DELTA)
	{
		//Get closest point on plane
		RequestedPosition = FMath::LinePlaneIntersection(InParams.EyePos, ProposedEndofEyeVector, MovementPlane);
	}

	//drag is a delta position, so just update the different between the previous position and the new position
	FVector DeltaPosition = RequestedPosition - InParams.Position;

	//Retrieve the initial offset, passing in the current requested position and the current position
	FVector InitialOffset = GetAbsoluteTranslationInitialOffset(RequestedPosition, InParams.Position);

	//subtract off the initial offset (where the widget was clicked) to prevent popping
	DeltaPosition -= InitialOffset;

	//remove the component along the normal we want to mute
	float MovementAlongMutedAxis = DeltaPosition|InParams.NormalToRemove;
	FVector OutDrag = DeltaPosition - (InParams.NormalToRemove*MovementAlongMutedAxis);

	if (InParams.bMovementLockedToCamera)
	{
		//DAMPEN ABSOLUTE MOVEMENT when the camera is locked to the object
		OutDrag *= CAMERA_LOCK_DAMPING_FACTOR;
		OutDrag.X = FMath::Clamp(OutDrag.X, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
		OutDrag.Y = FMath::Clamp(OutDrag.Y, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
		OutDrag.Z = FMath::Clamp(OutDrag.Z, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
	}

	//the they requested position snapping and we're not moving with the camera
	if (InParams.bPositionSnapping && !InParams.bMovementLockedToCamera && bSnapEnabled)
	{
		FVector MovementAlongAxis = FVector(OutDrag|InParams.XAxis, OutDrag|InParams.YAxis, OutDrag|InParams.ZAxis);
		//translation (either xy plane or z)
		FSnappingUtils::SnapPointToGrid( MovementAlongAxis, FVector(GEditor->GetGridSize(),GEditor->GetGridSize(),GEditor->GetGridSize()) );
		OutDrag = MovementAlongAxis.X*InParams.XAxis + MovementAlongAxis.Y*InParams.YAxis + MovementAlongAxis.Z*InParams.ZAxis;
	}

	//get the distance from the original position to the new proposed position 
	FVector DeltaFromStart = InParams.Position + OutDrag - InitialTranslationPosition;

	//Get the vector from the eye to the proposed new position (to make sure it's not behind the camera
	FVector EyeToNewPosition = (InParams.Position + OutDrag) - InParams.EyePos;
	float BehindTheCameraDotProduct = EyeToNewPosition|InParams.CameraDir;

	//Don't let the requested position go behind the camera
	if ( BehindTheCameraDotProduct <= 0 )
	{
		OutDrag = OutDrag.ZeroVector;
	}
	return OutDrag;
}

/**
 * Returns the offset from the initial selection point
 */
FVector FWidget::GetAbsoluteTranslationInitialOffset(const FVector& InNewPosition, const FVector& InCurrentPosition)
{
	if (!bAbsoluteTranslationInitialOffsetCached)
	{
		bAbsoluteTranslationInitialOffsetCached = true;
		InitialTranslationOffset = InNewPosition - InCurrentPosition;
		InitialTranslationPosition = InCurrentPosition;
	}
	return InitialTranslationOffset;
}



/**
 * Returns true if we're in Local Space editing mode or editing BSP (which uses the World axes anyway
 */
bool FWidget::IsRotationLocalSpace() const
{
	bool bIsLocalSpace = ( CustomCoordSystemSpace == COORD_Local );
	//for bsp and things that don't have a "true" local space, they will always use world.  So do NOT invert.
	if (bIsLocalSpace && CustomCoordSystem.Equals(FMatrix::Identity))
	{
		bIsLocalSpace = false;
	}
	return bIsLocalSpace;
}

void FWidget::UpdateDeltaRotation()
{
	TotalDeltaRotation += CurrentDeltaRotation;
	if ( (TotalDeltaRotation <= -360.f) || (TotalDeltaRotation >= 360.f) )
	{
		TotalDeltaRotation = FRotator::ClampAxis(TotalDeltaRotation);
	}
}

/**
 * Returns the angle in degrees representation of how far we have just rotated
 */
float FWidget::GetDeltaRotation() const
{
	bool bIsLocalSpace = IsRotationLocalSpace();
	return (bIsLocalSpace ? -1 : 1)*TotalDeltaRotation;
}


uint8 LargeInnerAlpha = 0x3f;
uint8 SmallInnerAlpha = 0x0f;
uint8 LargeOuterAlpha = 0x7f;
uint8 SmallOuterAlpha = 0x0f;

/**
 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
 * @param View - Information about the scene/camera/etc
 * @param PDI - Drawing interface
 * @param InAxis - Enumeration of axis to rotate about
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InDirectionToWidget - Direction from camera to the widget
 * @param InColor - The color associated with the axis of rotation
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 */
void FWidget::DrawRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FVector& InDirectionToWidget, const FColor& InColor, const float InScale)
{
	bool bIsPerspective = ( View->ViewMatrices.ProjMatrix.M[3][3] < 1.0f );
	bool bIsOrtho = !bIsPerspective;
	//if we're in an ortho viewport and the ring is perpendicular to the camera (both Axis0 & Axis1 are perpendicular)
	bool bIsOrthoDrawingFullRing = bIsOrtho && (FMath::Abs(Axis0|InDirectionToWidget) < KINDA_SMALL_NUMBER) && (FMath::Abs(Axis1|InDirectionToWidget) < KINDA_SMALL_NUMBER);

	FColor ArcColor = InColor;
	ArcColor.A = LargeOuterAlpha;

	if (bDragging || (bIsOrthoDrawingFullRing))
	{
		if ((CurrentAxis&InAxis) || (bIsOrthoDrawingFullRing))
		{
			float DeltaRotation = GetDeltaRotation();
			float AbsRotation = FRotator::ClampAxis(fabs(DeltaRotation));
			float AngleOfChangeRadians (AbsRotation * PI / 180.f);

			//always draw clockwise, so if we're negative we need to flip the angle
			float StartAngle = DeltaRotation < 0.0f ? -AngleOfChangeRadians : 0.0f;
			float FilledAngle = AngleOfChangeRadians;

			//the axis of rotation
			FVector ZAxis = Axis0 ^ Axis1;

			ArcColor.A = LargeOuterAlpha;
			DrawPartialRotationArc(View, PDI, InAxis, InLocation,  Axis0, Axis1, StartAngle, StartAngle + FilledAngle, ArcColor, InScale, InDirectionToWidget);
			ArcColor.A = SmallOuterAlpha;
			DrawPartialRotationArc(View, PDI, InAxis, InLocation,  Axis0, Axis1, StartAngle + FilledAngle, StartAngle + 2*PI, ArcColor, InScale, InDirectionToWidget);

			ArcColor = (CurrentAxis&InAxis) ? CurrentColor : ArcColor;
			//Hallow Arrow
			ArcColor.A = 0;
			DrawStartStopMarker(PDI, InLocation, Axis0, Axis1, 0, ArcColor, InScale);
			//Filled Arrow
			ArcColor.A = LargeOuterAlpha;
			DrawStartStopMarker(PDI, InLocation, Axis0, Axis1, DeltaRotation, ArcColor, InScale);

			ArcColor.A = 255;

			FVector SnapLocation = InLocation;

			if (GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled)
			{
				float DeltaAngle = GEditor->GetRotGridSize().Yaw;
				//every 22.5 degrees
				float TickMarker = 22.5f;
				for (float Angle = 0; Angle < 360.f; Angle+=DeltaAngle)
				{ 
					FVector GridAxis = Axis0.RotateAngleAxis(Angle, ZAxis);
					float PercentSize = (FMath::Fmod(Angle, TickMarker)==0) ? .75f : .25f;
					if (FMath::Fmod(Angle, 90.f) != 0)
					{
						DrawSnapMarker(PDI, SnapLocation,  GridAxis,  FVector::ZeroVector, ArcColor, InScale, 0.0f, PercentSize);
					}
				}
			}

			//draw axis tick marks
			FColor AxisColor = InColor;
			//Rotate Colors to match Axis 0
			Swap(AxisColor.R, AxisColor.G);
			Swap(AxisColor.B, AxisColor.R);
			AxisColor.A = (DeltaRotation == 0) ? MAX_uint8 : LargeOuterAlpha;
			DrawSnapMarker(PDI, SnapLocation,  Axis0,  Axis1, AxisColor, InScale, .25f);
			AxisColor.A = (DeltaRotation == 180.f) ? MAX_uint8 : LargeOuterAlpha;
			DrawSnapMarker(PDI, SnapLocation, -Axis0, -Axis1, AxisColor, InScale, .25f);

			//Rotate Colors to match Axis 1
			Swap(AxisColor.R, AxisColor.G);
			Swap(AxisColor.B, AxisColor.R);
			AxisColor.A = (DeltaRotation == 90.f) ? MAX_uint8 : LargeOuterAlpha;
			DrawSnapMarker(PDI, SnapLocation,  Axis1, -Axis0, AxisColor, InScale, .25f);
			AxisColor.A = (DeltaRotation == 270.f) ? MAX_uint8 : LargeOuterAlpha;
			DrawSnapMarker(PDI, SnapLocation, -Axis1,  Axis0, AxisColor, InScale, .25f);

			if (bDragging)
			{
				float OffsetAngle = IsRotationLocalSpace() ? 0 : DeltaRotation;

				CacheRotationHUDText(View, PDI, InLocation, Axis0.RotateAngleAxis(OffsetAngle, ZAxis), Axis1.RotateAngleAxis(OffsetAngle, ZAxis), DeltaRotation, InScale);
			}
		}
	}
	else
	{
		//Reverse the axes based on camera view
		FVector RenderAxis0 = ((Axis0|InDirectionToWidget) <= 0.0f) ? Axis0 : -Axis0;
		FVector RenderAxis1 = ((Axis1|InDirectionToWidget) <= 0.0f) ? Axis1 : -Axis1;

		DrawPartialRotationArc(View, PDI, InAxis, InLocation, RenderAxis0, RenderAxis1, 0, PI/2, ArcColor, InScale, InDirectionToWidget);
	}
}

/**
 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
 * @param View - Information about the scene/camera/etc
 * @param PDI - Drawing interface
 * @param InAxis - Enumeration of axis to rotate about
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InStartAngle - The starting angle about (Axis0^Axis1) to render the arc, in radians
 * @param InEndAngle - The ending angle about (Axis0^Axis1) to render the arc, in radians
 * @param InColor - The color associated with the axis of rotation
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 */
void FWidget::DrawPartialRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const float InStartAngle, const float InEndAngle, const FColor& InColor, const float InScale, const FVector& InDirectionToWidget )
{
	const float InnerRadius = (INNER_AXIS_CIRCLE_RADIUS * InScale) + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;
	const float OuterRadius = (OUTER_AXIS_CIRCLE_RADIUS * InScale) + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;

	bool bIsPerspective = ( View->ViewMatrices.ProjMatrix.M[3][3] < 1.0f );
	PDI->SetHitProxy( new HWidgetAxis( InAxis ) );
	{
		FThickArcParams OuterArcParams(PDI, InLocation, TransparentPlaneMaterialXY, InnerRadius, OuterRadius);
		FColor OuterColor = ( CurrentAxis&InAxis ? CurrentColor : InColor );
		//Pass through alpha
		OuterColor.A = InColor.A;
		DrawThickArc(OuterArcParams, Axis0, Axis1, InStartAngle, InEndAngle, OuterColor, InDirectionToWidget, !bIsPerspective );
	}
	PDI->SetHitProxy( NULL );

	if (bIsPerspective)
	{
		FThickArcParams InnerArcParams(PDI, InLocation, GridMaterial, 0.0f, InnerRadius);
		FColor InnerColor = InColor;
		//if something is selected and it's not this
		InnerColor.A = ((CurrentAxis & InAxis) && !bDragging) ? LargeInnerAlpha : SmallInnerAlpha;
		DrawThickArc(InnerArcParams, Axis0, Axis1, InStartAngle, InEndAngle, InnerColor, InDirectionToWidget, false );
	}
}

/**
 * Renders a portion of an arc for the rotation widget
 * @param InParams - Material, Radii, etc
 * @param InStartAxis - Start of the arc, in radians
 * @param InEndAxis - End of the arc, in radians
 * @param InColor - Color to use for the arc
 */
void FWidget::DrawThickArc (const FThickArcParams& InParams, const FVector& Axis0, const FVector& Axis1, const float InStartAngle, const float InEndAngle, const FColor& InColor, const FVector& InDirectionToWidget, bool bIsOrtho )
{
	if (InColor.A == 0)
	{
		return;
	}

	// Add more sides to the circle if we've been scaled up to keep the circle looking circular
	// An extra side for every 5 extra unreal units seems to produce a nice result
	const int32 CircleSides = (GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment > 0) 
		? AXIS_CIRCLE_SIDES + (GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment / 5)
		: AXIS_CIRCLE_SIDES;
	const int32 NumPoints = FMath::TruncToInt(CircleSides * (InEndAngle-InStartAngle)/(PI/2)) + 1;

	FColor TriangleColor = InColor;
	FColor RingColor = InColor;
	RingColor.A = MAX_uint8;

	FVector ZAxis = Axis0 ^ Axis1;
	FVector LastVertex;

	FDynamicMeshBuilder MeshBuilder;

	for (int32 RadiusIndex = 0; RadiusIndex < 2; ++RadiusIndex)
	{
		float Radius = (RadiusIndex == 0) ? InParams.OuterRadius : InParams.InnerRadius;
		float TCRadius = Radius / (float) InParams.OuterRadius;
		//Compute vertices for base circle.
		for(int32 VertexIndex = 0;VertexIndex <= NumPoints;VertexIndex++)
		{
			float Percent = VertexIndex/(float)NumPoints;
			float Angle = FMath::Lerp(InStartAngle, InEndAngle, Percent);
			float AngleDeg = FRotator::ClampAxis(Angle * 180.f / PI);

			FVector VertexDir = Axis0.RotateAngleAxis(AngleDeg, ZAxis);
			VertexDir.Normalize();

			float TCAngle = Percent*(PI/2);
			FVector2D TC(TCRadius*FMath::Cos(Angle), TCRadius*FMath::Sin(Angle));

			const FVector VertexPosition = InParams.Position + VertexDir*Radius;
			FVector Normal = VertexPosition - InParams.Position;
			Normal.Normalize();

			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = VertexPosition;
			MeshVertex.Color = TriangleColor;
			MeshVertex.TextureCoordinate = TC;

			MeshVertex.SetTangents(
				-ZAxis,
				(-ZAxis) ^ Normal,
				Normal
				);

			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex

			// Push out the arc line borders so they dont z-fight with the mesh arcs
			FVector StartLinePos = LastVertex;
			FVector EndLinePos = VertexPosition;
			if (VertexIndex != 0)
			{
				InParams.PDI->DrawLine(StartLinePos,EndLinePos,RingColor,SDPG_Foreground);
			}
			LastVertex = VertexPosition;
		}
	}

	//Add top/bottom triangles, in the style of a fan.
	int32 InnerVertexStartIndex = NumPoints + 1;
	for(int32 VertexIndex = 0; VertexIndex < NumPoints; VertexIndex++)
	{
		MeshBuilder.AddTriangle(VertexIndex, VertexIndex+1, InnerVertexStartIndex+VertexIndex);
		MeshBuilder.AddTriangle(VertexIndex+1, InnerVertexStartIndex+VertexIndex+1, InnerVertexStartIndex+VertexIndex);
	}

	MeshBuilder.Draw(InParams.PDI, FMatrix::Identity, InParams.Material->GetRenderProxy(false),SDPG_Foreground,0.f);
}

/**
 * Draws protractor like ticks where the rotation widget would snap too.
 * Also, used to draw the wider axis tick marks
 * @param PDI - Drawing interface
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1)
 * @param InColor - The color to use for line/poly drawing
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 * @param InWidthPercent - The percent of the distance between the outer ring and inner ring to use for tangential thickness
 * @param InPercentSize - The percent of the distance between the outer ring and inner ring to use for radial distance
 */
void FWidget::DrawSnapMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FColor& InColor, const float InScale, const float InWidthPercent, const float InPercentSize)
{
	const float InnerDistance = (INNER_AXIS_CIRCLE_RADIUS * InScale) + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;
	const float OuterDistance = (OUTER_AXIS_CIRCLE_RADIUS * InScale) + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;
	const float MaxMarkerHeight = OuterDistance - InnerDistance;
	const float MarkerWidth = MaxMarkerHeight*InWidthPercent;
	const float MarkerHeight = MaxMarkerHeight*InPercentSize;

	FVector Vertices[4];
	Vertices[0] = InLocation + (OuterDistance)*Axis0 - (MarkerWidth*.5)*Axis1;
	Vertices[1] = Vertices[0] + (MarkerWidth)*Axis1;
	Vertices[2] = InLocation + (OuterDistance-MarkerHeight)*Axis0 - (MarkerWidth*.5)*Axis1;
	Vertices[3] = Vertices[2] + (MarkerWidth)*Axis1;

	//draw at least one line
	PDI->DrawLine(Vertices[0], Vertices[2], InColor, SDPG_Foreground);

	//if there should be thickness, draw the other lines
	if (InWidthPercent > 0.0f)
	{
		PDI->DrawLine(Vertices[0], Vertices[1], InColor, SDPG_Foreground);
		PDI->DrawLine(Vertices[1], Vertices[3], InColor, SDPG_Foreground);
		PDI->DrawLine(Vertices[2], Vertices[3], InColor, SDPG_Foreground);

		//fill in the box
		FDynamicMeshBuilder MeshBuilder;

		for(int32 VertexIndex = 0;VertexIndex < 4; VertexIndex++)
		{
			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = Vertices[VertexIndex];
			MeshVertex.Color = InColor;
			MeshVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);
			MeshVertex.SetTangents(
				Axis0,
				Axis1,
				(Axis0) ^ Axis1
				);
			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
		}

		MeshBuilder.AddTriangle(0, 1, 2);
		MeshBuilder.AddTriangle(1, 3, 2);
		MeshBuilder.Draw(PDI, FMatrix::Identity, TransparentPlaneMaterialXY->GetRenderProxy(false),SDPG_Foreground,0.f);
	}
}

/**
 * Draw Start/Stop Marker to show delta rotations along the arc of rotation
 * @param PDI - Drawing interface
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1), units are degrees
 * @param InColor - The color to use for line/poly drawing
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 */
void FWidget::DrawStartStopMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const float InAngle, const FColor& InColor, const float InScale)
{
	const float ArrowHeightPercent = .8f;
	const float InnerDistance = (INNER_AXIS_CIRCLE_RADIUS * InScale) + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;
	const float OuterDistance = (OUTER_AXIS_CIRCLE_RADIUS * InScale) + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;
	const float RingHeight = OuterDistance - InnerDistance;
	const float ArrowHeight = RingHeight*ArrowHeightPercent;
	const float ThirtyDegrees = PI / 6.0f;
	const float HalfArrowidth = ArrowHeight*FMath::Tan(ThirtyDegrees);

	FVector ZAxis = Axis0 ^ Axis1;
	FVector RotatedAxis0 = Axis0.RotateAngleAxis(InAngle, ZAxis);
	FVector RotatedAxis1 = Axis1.RotateAngleAxis(InAngle, ZAxis);

	FVector Vertices[3];
	Vertices[0] = InLocation + (OuterDistance)*RotatedAxis0;
	Vertices[1] = Vertices[0] + (ArrowHeight)*RotatedAxis0 - HalfArrowidth*RotatedAxis1;
	Vertices[2] = Vertices[1] + (2*HalfArrowidth)*RotatedAxis1;

	PDI->DrawLine(Vertices[0], Vertices[1], InColor, SDPG_Foreground);
	PDI->DrawLine(Vertices[1], Vertices[2], InColor, SDPG_Foreground);
	PDI->DrawLine(Vertices[0], Vertices[2], InColor, SDPG_Foreground);

	if (InColor.A > 0)
	{
		//fill in the box
		FDynamicMeshBuilder MeshBuilder;

		for(int32 VertexIndex = 0;VertexIndex < 3; VertexIndex++)
		{
			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = Vertices[VertexIndex];
			MeshVertex.Color = InColor;
			MeshVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);
			MeshVertex.SetTangents(
				RotatedAxis0,
				RotatedAxis1,
				(RotatedAxis0) ^ RotatedAxis1
				);
			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
		}

		MeshBuilder.AddTriangle(0, 1, 2);
		MeshBuilder.Draw(PDI, FMatrix::Identity, TransparentPlaneMaterialXY->GetRenderProxy(false),SDPG_Foreground,0.f);
	}
}

/**
 * Caches off HUD text to display after 3d rendering is complete
 * @param View - Information about the scene/camera/etc
 * @param PDI - Drawing interface
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param AngleOfAngle - angle we've rotated so far (in degrees)
 */
void FWidget::CacheRotationHUDText(const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const float AngleOfChange, const float InScale)
{
	const float TextDistance = (ROTATION_TEXT_RADIUS * InScale) + GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;

	FVector AxisVectors[4] = { Axis0, Axis1, -Axis0, -Axis1};

	for (int i = 0 ; i < 4; ++i)
	{
		FVector PotentialTextPosition = InLocation + (TextDistance)*AxisVectors[i];
		if(View->ScreenToPixel(View->WorldToScreen(PotentialTextPosition), HUDInfoPos))
		{
			if (FMath::IsWithin<float>(HUDInfoPos.X, 0, View->ViewRect.Width()) && FMath::IsWithin<float>(HUDInfoPos.Y, 0, View->ViewRect.Height()))
			{
				//only valid screen locations get a valid string
				HUDString = FString::Printf(TEXT("%3.2f"), AngleOfChange);
				break;
			}
		}
	}
}

uint32 FWidget::GetDominantAxisIndex( const FVector& InDiff, FEditorViewportClient* ViewportClient ) const
{
	uint32 DominantIndex = 0;
	if( FMath::Abs(InDiff.X) < FMath::Abs(InDiff.Y) )
	{
		DominantIndex = 1;
	}

	const int32 WidgetMode = ViewportClient->GetWidgetMode();

	switch( WidgetMode )
	{
		case WM_Translate:
			switch( ViewportClient->ViewportType )
			{
				case LVT_OrthoXY:
					if( CurrentAxis == EAxisList::X )
					{
						DominantIndex = 0;
					}
					else if( CurrentAxis == EAxisList::Y )
					{
						DominantIndex = 1;
					}
					break;
				case LVT_OrthoXZ:
					if( CurrentAxis == EAxisList::X )
					{
						DominantIndex = 0;
					}
					else if( CurrentAxis == EAxisList::Z )
					{
						DominantIndex = 1;
					}
					break;
				case LVT_OrthoYZ:
					if( CurrentAxis == EAxisList::Y )
					{
						DominantIndex = 0;
					}
					else if( CurrentAxis == EAxisList::Z )
					{
						DominantIndex = 1;
					}
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	return DominantIndex;
}


EAxisList::Type FWidget::GetAxisToDraw( EWidgetMode WidgetMode ) const
{
	return EditorModeTools ? EditorModeTools->GetWidgetAxisToDraw( WidgetMode ) : EAxisList::All;
}

bool FWidget::IsWidgetDisabled() const
{
	return EditorModeTools ? (EditorModeTools->IsModeActive(FBuiltinEditorModes::EM_Default) && GEditor->HasLockedActors()) : false;
}


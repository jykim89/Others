// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "DebugRenderSceneProxy.h"
#include "Debug/DebugDrawService.h"
#include "EnvQueryTypes.h"
#include "EQSRenderingComponent.generated.h"

class FEQSSceneProxy : public FDebugRenderSceneProxy
{
public:
	FEQSSceneProxy(const UPrimitiveComponent* InComponent, const FString& ViewFlagName = TEXT("DebugAI"), bool bDrawOnlyWhenSelected = true);
	FEQSSceneProxy(const UPrimitiveComponent* InComponent, const FString& ViewFlagName, bool bDrawOnlyWhenSelected, const TArray<FSphere>& Spheres, const TArray<FText3d>& Texts);

	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController*) OVERRIDE;
	
	//virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) OVERRIDE;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) OVERRIDE;

	static void CollectEQSData(const UPrimitiveComponent* InComponent, const class IEQSQueryResultSourceInterface* QueryDataSource, TArray<FSphere>& Spheres, TArray<FText3d>& Texts);
private:
	FEnvQueryResult QueryResult;	
	// can be 0
	AActor* ActorOwner;
	const class IEQSQueryResultSourceInterface* QueryDataSource;
	bool bDrawOnlyWhenSelected;

	static const FVector ItemDrawRadius;

	bool SafeIsActorSelected() const;
};

UCLASS(hidecategories=Object)
class ENGINE_API UEQSRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	FString DrawFlagName;
	bool bDrawOnlyWhenSelected;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const OVERRIDE;
	virtual void CreateRenderState_Concurrent() OVERRIDE;
	virtual void DestroyRenderState_Concurrent() OVERRIDE;
};

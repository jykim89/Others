// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Renderer.cpp: Renderer module implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "AssertionMacros.h"
#include "GPUBenchmark.h"

DEFINE_LOG_CATEGORY(LogRenderer);

IMPLEMENT_MODULE(FRendererModule, Renderer);

#if !IS_MONOLITHIC
	// visual studio cannot find cross dll data for visualizers
	// thus as a workaround for now, copy and paste this into every module
	// where we need to visualize SystemSettings
	FSystemSettings* GSystemSettingsForVisualizers = &GSystemSettings;
#endif

void FRendererModule::ReallocateSceneRenderTargets()
{
	FLightPrimitiveInteraction::InitializeMemoryPool();
	GSceneRenderTargets.UpdateRHI();
}

void FRendererModule::SceneRenderTargetsSetBufferSize(uint32 SizeX, uint32 SizeY)
{
	GSceneRenderTargets.SetBufferSize(SizeX, SizeY);
	GSceneRenderTargets.UpdateRHI();
}

void FRendererModule::DrawTileMesh(const FSceneView& SceneView, const FMeshBatch& Mesh, bool bIsHitTesting, const FHitProxyId& HitProxyId)
{
	// Create an FViewInfo so we can initialize its RHI resources
	//@todo - reuse this view for multiple tiles, this is going to be slow for each tile
	FViewInfo View(&SceneView);
	View.InitRHIResources();

	const auto FeatureLevel = View.GetFeatureLevel();

	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel);

	//get the blend mode of the material
	const EBlendMode MaterialBlendMode = Material->GetBlendMode();

	if (!GUsingNullRHI)
	{
		// handle translucent material blend modes
		if (IsTranslucentBlendMode(MaterialBlendMode))
		{
			if (FeatureLevel >= ERHIFeatureLevel::SM3)
			{
				FTranslucencyDrawingPolicyFactory::DrawDynamicMesh(View, FTranslucencyDrawingPolicyFactory::ContextType(), Mesh, false, false, NULL, HitProxyId);
			}
			else
			{
				FTranslucencyForwardShadingDrawingPolicyFactory::DrawDynamicMesh(View, FTranslucencyForwardShadingDrawingPolicyFactory::ContextType(), Mesh, false, false, NULL, HitProxyId);
			}
		}
		// handle opaque materials
		else
		{
			// make sure we are doing opaque drawing
			RHISetBlendState(TStaticBlendState<>::GetRHI());

			// draw the mesh
			if (bIsHitTesting)
			{
				FHitProxyDrawingPolicyFactory::DrawDynamicMesh(View, FHitProxyDrawingPolicyFactory::ContextType(), Mesh, false, false, NULL, HitProxyId);
			}
			else
			{
				if (FeatureLevel >= ERHIFeatureLevel::SM3)
				{
					FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(View, FBasePassOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::SetTextures), Mesh, false, false, NULL, HitProxyId);
				}
				else
				{
					FBasePassForwardOpaqueDrawingPolicyFactory::DrawDynamicMesh(View, FBasePassForwardOpaqueDrawingPolicyFactory::ContextType(ESceneRenderTargetsMode::SetTextures), Mesh, false, false, NULL, HitProxyId);
				}
			}
		}	
	}
}

void FRendererModule::RenderTargetPoolFindFreeElement(const FPooledRenderTargetDesc& Desc, TRefCountPtr<IPooledRenderTarget> &Out, const TCHAR* InDebugName)
{
	GRenderTargetPool.FindFreeElement(Desc, Out, InDebugName);
}

void FRendererModule::TickRenderTargetPool()
{
	GRenderTargetPool.TickPoolElements();
}

void FRendererModule::DebugLogOnCrash()
{
	GRenderTargetPool.VisualizeTexture.SortOrder = 1;
	GRenderTargetPool.VisualizeTexture.bFullList = true;
	GRenderTargetPool.VisualizeTexture.DebugLog(false);

	// execute on main thread
	{
		struct FTest
		{
			void Thread()
			{
				GEngine->Exec(NULL, TEXT("Mem FromReport"), *GLog);
				GEngine->Exec(NULL, TEXT("rhi.DumpMemory"), *GLog);
			}
		} Test;

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
			(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(&Test, &FTest::Thread)
			, TEXT("DumpDataAfterCrash")
			, nullptr, ENamedThreads::GameThread
			);
	}
}

void FRendererModule::GPUBenchmark(FSynthBenchmarkResults& InOut, uint32 WorkScale, bool bDebugOut)
{
	check(IsInGameThread());

	FSceneViewInitOptions ViewInitOptions;
	FIntRect ViewRect(0, 0, 1, 1);

	FBox LevelBox(FVector(-WORLD_MAX), FVector(+WORLD_MAX));
	ViewInitOptions.SetViewRectangle(ViewRect);

	// Initialize Projection Matrix and ViewMatrix since FSceneView initialization is doing some math on them.
	// Otherwise it trips NaN checks.
	const FVector ViewPoint = LevelBox.GetCenter();
	ViewInitOptions.ViewMatrix = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, -1, 0, 0),
		FPlane(0, 0, -1, 0),
		FPlane(-ViewPoint.X, ViewPoint.Y, 0, 1));

	const float ZOffset = WORLD_MAX;
	ViewInitOptions.ProjectionMatrix = FReversedZOrthoMatrix(
		LevelBox.GetSize().X / 2.f,
		LevelBox.GetSize().Y / 2.f,
		0.5f / ZOffset,
		ZOffset
		);

	FSceneView DummyView(ViewInitOptions);

	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
	  RendererGPUBenchmarkCommand,
	  FSceneView, DummyView, DummyView,
	  uint32, WorkScale, WorkScale,
	  bool, bDebugOut, bDebugOut,
	  FSynthBenchmarkResults&, InOut, InOut,
	{
		RendererGPUBenchmark(InOut, DummyView, WorkScale, bDebugOut);
	});
	FlushRenderingCommands();
}

void FRendererModule::QueryVisualizeTexture(FQueryVisualizeTexureInfo& Out)
{
	check(IsInGameThread());
	FlushRenderingCommands();

	GRenderTargetPool.VisualizeTexture.QueryInfo(Out);
}

static void VisualizeTextureExec( const TCHAR* Cmd, FOutputDevice &Ar )
{
	check(IsInGameThread());

	FlushRenderingCommands();

	uint32 ParameterCount = 0;

	// parse parameters
	for(;;)
	{
		FString Parameter = FParse::Token(Cmd, 0);

		if(Parameter.IsEmpty())
		{
			break;
		}

		Parameter.ToLower();

		// FULL flag
		if(Parameter == TEXT("fulllist") || Parameter == TEXT("full"))
		{
			GRenderTargetPool.VisualizeTexture.bFullList = true;
			// this one doesn't count as parameter so we can do "vis full"
			continue;
		}
		// SORT0 flag
		else if(Parameter == TEXT("sort0"))
		{
			GRenderTargetPool.VisualizeTexture.SortOrder = 0;
			// this one doesn't count as parameter so we can do "vis full"
			continue;
		}
		// SORT1 flag
		else if(Parameter == TEXT("sort1"))
		{
			GRenderTargetPool.VisualizeTexture.SortOrder = 1;
			// this one doesn't count as parameter so we can do "vis full"
			continue;
		}
		else if(ParameterCount == 0)
		{
			// Init
			GRenderTargetPool.VisualizeTexture.RGBMul = 1;
			GRenderTargetPool.VisualizeTexture.AMul = 0;
			GRenderTargetPool.VisualizeTexture.UVInputMapping = 3;
			GRenderTargetPool.VisualizeTexture.Flags = 0;
			GRenderTargetPool.VisualizeTexture.Mode = 0;
			GRenderTargetPool.VisualizeTexture.CustomMip = 0;
			GRenderTargetPool.VisualizeTexture.ArrayIndex = 0;
			GRenderTargetPool.VisualizeTexture.bOutputStencil = false;

			// e.g. "VisualizeTexture Name" or "VisualizeTexture 5"
			bool bIsDigit = FChar::IsDigit(**Parameter);

			if (bIsDigit)
			{
				GRenderTargetPool.VisualizeTexture.Mode = FCString::Atoi(*Parameter);
			}

			if(!bIsDigit)
			{
				// the name was specified as string
				const TCHAR* AfterAt = *Parameter;

				while(*AfterAt != 0 && *AfterAt != TCHAR('@'))
				{
					++AfterAt;
				}

				if(*AfterAt == TCHAR('@'))
				{
					// user specified a reuse goal
					FString NameWithoutAt = Parameter.Left(AfterAt - *Parameter);
					GRenderTargetPool.VisualizeTexture.SetObserveTarget(*NameWithoutAt, FCString::Atoi(AfterAt + 1));
				}
				else
				{
					// we take the last one
					GRenderTargetPool.VisualizeTexture.SetObserveTarget(*Parameter);
				}
			}
			else
			{
				// the index was used
				GRenderTargetPool.VisualizeTexture.SetObserveTarget(TEXT(""));
			}
		}
		// GRenderTargetPoolInputMapping mode
		else if(Parameter == TEXT("uv0"))
		{
			GRenderTargetPool.VisualizeTexture.UVInputMapping = 0;
		}
		else if(Parameter == TEXT("uv1"))
		{
			GRenderTargetPool.VisualizeTexture.UVInputMapping = 1;
		}
		else if(Parameter == TEXT("uv2"))
		{
			GRenderTargetPool.VisualizeTexture.UVInputMapping = 2;
		}
		else if(Parameter == TEXT("pip"))
		{
			GRenderTargetPool.VisualizeTexture.UVInputMapping = 3;
		}
		// BMP flag
		else if(Parameter == TEXT("bmp"))
		{
			GRenderTargetPool.VisualizeTexture.bSaveBitmap = true;
		}
		else if (Parameter == TEXT("stencil"))
		{
			GRenderTargetPool.VisualizeTexture.bOutputStencil = true;
		}
		// saturate flag
		else if(Parameter == TEXT("frac"))
		{
			// default already covers this
		}
		// saturate flag
		else if(Parameter == TEXT("sat"))
		{
			GRenderTargetPool.VisualizeTexture.Flags |= 0x1;
		}
		// e.g. mip2 or mip0
		else if(Parameter.Left(3) == TEXT("mip"))
		{
			Parameter = Parameter.Right(Parameter.Len() - 3);
			GRenderTargetPool.VisualizeTexture.CustomMip = FCString::Atoi(*Parameter);
		}
		// e.g. [0] or [2]
		else if(Parameter.Left(5) == TEXT("index"))
		{
			Parameter = Parameter.Right(Parameter.Len() - 5);
			GRenderTargetPool.VisualizeTexture.ArrayIndex = FCString::Atoi(*Parameter);
		}
		// e.g. RGB*6, A, *22, /2.7, A*7
		else if(Parameter.Left(3) == TEXT("rgb")
			|| Parameter.Left(1) == TEXT("a")
			|| Parameter.Left(1) == TEXT("*")
			|| Parameter.Left(1) == TEXT("/"))
		{
			if(Parameter.Left(3) == TEXT("rgb"))
			{
				Parameter = Parameter.Right(Parameter.Len() - 3);
			}
			else if(Parameter.Left(1) == TEXT("a"))
			{
				Parameter = Parameter.Right(Parameter.Len() - 1);
				GRenderTargetPool.VisualizeTexture.RGBMul = 0;
				GRenderTargetPool.VisualizeTexture.AMul = 1;
			}

			float Mul = 1.0f;

			// * or /
			if(Parameter.Left(1) == TEXT("*"))
			{
				Parameter = Parameter.Right(Parameter.Len() - 1);
				Mul = FCString::Atof(*Parameter);
			}
			else if(Parameter.Left(1) == TEXT("/"))
			{
				Parameter = Parameter.Right(Parameter.Len() - 1);
				Mul = 1.0f / FCString::Atof(*Parameter);
			}
			GRenderTargetPool.VisualizeTexture.RGBMul *= Mul;
			GRenderTargetPool.VisualizeTexture.AMul *= Mul;
		}
		else
		{
			Ar.Logf(TEXT("Error: parameter \"%s\" not recognized"), *Parameter);
		}

		++ParameterCount;
	}

	if(!ParameterCount)
	{
		// show help
		Ar.Logf(TEXT("VisualizeTexture/Vis <TextureId/CheckpointName> [<Mode>] [PIP/UV0/UV1/UV2] [BMP] [FRAC/SAT] [FULL]:"));

		Ar.Logf(TEXT("Mode (examples):"));
		Ar.Logf(TEXT("  RGB      = RGB in range 0..1 (default)"));
		Ar.Logf(TEXT("  *8       = RGB * 8"));
		Ar.Logf(TEXT("  A        = alpha channel in range 0..1"));
		Ar.Logf(TEXT("  A*16     = Alpha * 16"));
		Ar.Logf(TEXT("  RGB/2    = RGB / 2"));
		Ar.Logf(TEXT("SubResource:"));
		Ar.Logf(TEXT("  MIP5     = Mip level 5 (0 is default)"));
		Ar.Logf(TEXT("  INDEX5   = Array Element 5 (0 is default)"));
		Ar.Logf(TEXT("InputMapping:"));
		Ar.Logf(TEXT("  PIP      = like UV1 but as picture in picture with normal rendering  (default)"));
		Ar.Logf(TEXT("  UV0      = UV in left top"));
		Ar.Logf(TEXT("  UV1      = full texture"));
		Ar.Logf(TEXT("  UV2      = pixel perfect centered"));
		Ar.Logf(TEXT("Flags:"));
		Ar.Logf(TEXT("  BMP      = save out bitmap to the screenshots folder (not on console, normalized)"));
		Ar.Logf(TEXT("STENCIL    = Stencil normally displayed in alpha channel of depth.  This option is used for BMP to get a stencil only BMP."));
		Ar.Logf(TEXT("  FRAC     = use frac() in shader (default)"));
		Ar.Logf(TEXT("  SAT      = use saturate() in shader"));
		Ar.Logf(TEXT("  FULLLIST = show full list, otherwise we hide some textures in the printout"));
		Ar.Logf(TEXT("  SORT0    = sort list by name"));
		Ar.Logf(TEXT("  SORT1    = show list by size"));
		Ar.Logf(TEXT("TextureId:"));
		Ar.Logf(TEXT("  0        = <off>"));

		GRenderTargetPool.VisualizeTexture.DebugLog(true);
	}
	//		Ar.Logf(TEXT("VisualizeTexture %d"), GRenderTargetPool.VisualizeTexture.Mode);
}

static bool RendererExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FParse::Command(&Cmd, TEXT("VisualizeTexture")) || FParse::Command(&Cmd, TEXT("Vis")))
	{
		VisualizeTextureExec(Cmd, Ar);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("ShowMipLevels")))
	{
		extern bool GVisualizeMipLevels;
		GVisualizeMipLevels = !GVisualizeMipLevels;
		Ar.Logf( TEXT( "Showing mip levels: %s" ), GVisualizeMipLevels ? TEXT("ENABLED") : TEXT("DISABLED") );
		return true;
	}
	else if(FParse::Command(&Cmd,TEXT("DumpUnbuiltLightInteractions")))
	{
		InWorld->Scene->DumpUnbuiltLightIteractions(Ar);
		return true;
	}
#endif

	return false;
}
FStaticSelfRegisteringExec RendererExecRegistration(RendererExec);

void FRendererModule::ExecVisualizeTextureCmd( const FString& Cmd )
{
	// @todo: Find a nicer way to call this
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	VisualizeTextureExec(*Cmd, *GLog);
#endif
}

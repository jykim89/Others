// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ProfilerPrivatePCH.h: Pre-compiled header file for the Profiler module.
=============================================================================*/

#ifndef __ProfilerPrivatePCH_h__
#define __ProfilerPrivatePCH_h__

/*-----------------------------------------------------------------------------
	Public included
-----------------------------------------------------------------------------*/

#include "../Public/Profiler.h"
#include "StatsData.h"
#include "StatsFile.h"

/*-----------------------------------------------------------------------------
	Dependencies
-----------------------------------------------------------------------------*/

#include "Messaging.h"
#include "ProfilerClient.h"

/*-----------------------------------------------------------------------------
	Private includes
-----------------------------------------------------------------------------*/

// TODO: Remove dependencies between generic profiler code and profiler ui code

// Common headers and managers.
#include "ProfilerStream.h"

#include "ProfilerSample.h"
#include "ProfilerFPSAnalyzer.h"
#include "ProfilerDataProvider.h"
#include "ProfilerDataSource.h"
#include "ProfilerSession.h"
#include "ProfilerCommands.h"
#include "ProfilerManager.h"

// Slate related headers.
#include "SProfilerToolbar.h"
#include "SFiltersAndPresets.h"
#include "SEventGraph.h"
#include "SDataGraph.h"
#include "SProfilerMiniView.h"
#include "SProfilerThreadView.h"
#include "SProfilerGraphPanel.h"
#include "SProfilerFPSChartPanel.h"
#include "SProfilerWindow.h"
#include "SHistogram.h"
#include "StatDragDropOp.h"

#endif // __ProfilerPrivatePCH_h__

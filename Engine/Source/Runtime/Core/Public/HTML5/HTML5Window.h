// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericWindow.h"

/**
 * A platform specific implementation of FNativeWindow.
 * Native windows provide platform-specific backing for and are always owned by an SWindow.
 */
 
class CORE_API FHTML5Window : public FGenericWindow
{
public:
	~FHTML5Window();

	/** Create a new FAndroidWindow.
	 *
	 * @param OwnerWindow		The SlateWindow for which we are crating a backing AndroidWindow
	 * @param InParent			Parent iOS window; usually NULL.
	 */
	static TSharedRef<FHTML5Window> Make();

	
	virtual void* GetOSWindowHandle() const OVERRIDE { return NULL; } //can be null.

	void Initialize( class FHTML5Application* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FHTML5Window >& InParent, const bool bShowImmediately );

	/** Returns the rectangle of the screen the window is associated with */
	virtual bool GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const OVERRIDE;

	virtual void ReshapeWindow(int32 X, int32 Y, int32 Width, int32 Height);

	virtual void SetOSWindowHandle(void*);

	static FPlatformRect GetScreenRect();

	static void CalculateSurfaceSize(void* InWindow, int32_t& SurfaceWidth, int32_t& SurfaceHeight);


protected:
	/** @return true if the native window is currently in fullscreen mode, false otherwise */
	virtual EWindowMode::Type GetWindowMode() const OVERRIDE;

private:
	/**
	 * Protect the constructor; only TSharedRefs of this class can be made.
	 */
	FHTML5Window();


	FHTML5Application* OwningApplication;

	/** Store the window region size for querying whether a point lies within the window */
	int32 RegionX;
	int32 RegionY;
};

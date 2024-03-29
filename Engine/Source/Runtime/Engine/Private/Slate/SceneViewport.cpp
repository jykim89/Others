// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "Slate.h"
#include "Slate/SlateTextures.h"
#include "Slate/SceneViewport.h"
#include "DebugCanvas.h"

#include "IHeadMountedDisplay.h"

extern int32 GetBoundFullScreenModeCVar();
extern EWindowMode::Type GetWindowModeType(EWindowMode::Type WindowMode);

FSceneViewport::FSceneViewport( FViewportClient* InViewportClient, TSharedPtr<SViewport> InViewportWidget )
	: FViewport( InViewportClient )
	, CurrentReplyState( FReply::Unhandled() )
	, CachedMousePos(-1, -1)
	, PreCaptureMousePos(-1, -1)
	, SoftwareCursorPosition( 0, 0 )
	, bIsSoftwareCursorVisible( false )
	, SlateRenderTargetHandle( NULL )
	, DebugCanvasDrawer( new FDebugCanvasDrawer )
	, ViewportWidget( InViewportWidget )
	, NumMouseSamplesX( 0 )
	, NumMouseSamplesY( 0 )
	, MouseDelta( 0, 0 )
	, bIsCursorVisible( true )
	, bRequiresVsync( false )
	, bUseSeparateRenderTarget( InViewportWidget.IsValid() ? !InViewportWidget->ShouldRenderDirectly() : true )
	, bIsResizing( false )
	, bPlayInEditorGetsMouseControl( true )
	, bPlayInEditorIsSimulate( false )
{
	bIsSlateViewport = true;
}

FSceneViewport::~FSceneViewport()
{
	Destroy();
	// Wait for resources to be deleted
	FlushRenderingCommands();
}

bool FSceneViewport::HasMouseCapture() const
{
	return FSlateApplication::Get().GetMouseCaptor() == ViewportWidget.Pin();
}

bool FSceneViewport::HasFocus() const
{
	return FSlateApplication::Get().GetKeyboardFocusedWidget() == ViewportWidget.Pin();
}

void FSceneViewport::CaptureMouse( bool bCapture )
{
	if( bCapture )
	{
		CurrentReplyState.UseHighPrecisionMouseMovement( ViewportWidget.Pin().ToSharedRef() );
	}
	else
	{
		CurrentReplyState.ReleaseMouseCapture();
	}
}

void FSceneViewport::LockMouseToViewport( bool bLock )
{
	if( bLock )
	{
		CurrentReplyState.LockMouseToWidget( ViewportWidget.Pin().ToSharedRef() );
	}
	else
	{
		CurrentReplyState.ReleaseMouseLock();
	}
}

void FSceneViewport::ShowCursor( bool bVisible )
{
	if ( bVisible && !bIsCursorVisible )
	{
		if( bIsSoftwareCursorVisible )
		{
			const int32 ClampedMouseX = FMath::Clamp<int32>(SoftwareCursorPosition.X, 0, SizeX);
			const int32 ClampedMouseY = FMath::Clamp<int32>(SoftwareCursorPosition.Y, 0, SizeY);

			CurrentReplyState.SetMousePos( CachedGeometry.LocalToAbsolute( FVector2D(ClampedMouseX, ClampedMouseY) ).IntPoint() );
		}
		else
		{
			// Restore the old mouse position when we show the cursor.
			CurrentReplyState.SetMousePos( PreCaptureMousePos );
		}

		SetPreCaptureMousePosFromSlateCursor();
		bIsCursorVisible = true;
	}
	else if ( !bVisible && bIsCursorVisible )
	{
		// Remember the current mouse position when we hide the cursor.
		SetPreCaptureMousePosFromSlateCursor();
		bIsCursorVisible = false;
	}
}

bool FSceneViewport::CaptureJoystickInput(bool Capture)
{
	if( Capture )
	{
		CurrentReplyState.CaptureJoystick( ViewportWidget.Pin().ToSharedRef(), true );
	}
	else
	{
		CurrentReplyState.ReleaseJoystickCapture(true);
	}

	return Capture;
}

bool FSceneViewport::KeyState( FKey Key ) const
{
	return KeyStateMap.FindRef( Key );
}

void FSceneViewport::Destroy()
{
	ViewportClient = NULL;

	UpdateViewportRHI( true, 0, 0, EWindowMode::Windowed );
}

int32 FSceneViewport::GetMouseX() const
{
	return CachedMousePos.X;
}

int32 FSceneViewport::GetMouseY() const
{
	return CachedMousePos.Y;
}

void FSceneViewport::GetMousePos( FIntPoint& MousePosition, const bool bLocalPosition )
{
	if (bLocalPosition)
	{
		MousePosition = CachedMousePos;
	}
	else
	{
		const FVector2D AbsoluteMousePos = CachedGeometry.LocalToAbsolute(FVector2D(CachedMousePos.X, CachedMousePos.Y));
		MousePosition.X = AbsoluteMousePos.X;
		MousePosition.Y = AbsoluteMousePos.Y;
	}
}

void FSceneViewport::SetMouse( int32 X, int32 Y )
{
	FVector2D AbsolutePos = CachedGeometry.LocalToAbsolute(FVector2D(X, Y));
	FSlateApplication::Get().SetCursorPos( AbsolutePos );
	CachedMousePos = FIntPoint(X, Y);
}

void FSceneViewport::UpdateCachedMousePos( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	CachedMousePos = InGeometry.AbsoluteToLocal( InMouseEvent.GetScreenSpacePosition() ).IntPoint();
}

void FSceneViewport::UpdateCachedGeometry( const FGeometry& InGeometry )
{
	CachedGeometry = InGeometry;
}

void FSceneViewport::UpdateModifierKeys( const FPointerEvent& InMouseEvent )
{
	KeyStateMap.Add( EKeys::LeftAlt, InMouseEvent.IsLeftAltDown() );
	KeyStateMap.Add( EKeys::RightAlt, InMouseEvent.IsRightAltDown() );
	KeyStateMap.Add(EKeys::LeftControl, InMouseEvent.IsLeftControlDown());
	KeyStateMap.Add(EKeys::RightControl, InMouseEvent.IsRightControlDown());
	KeyStateMap.Add(EKeys::LeftShift, InMouseEvent.IsLeftShiftDown());
	KeyStateMap.Add(EKeys::RightShift, InMouseEvent.IsRightShiftDown());
}

void FSceneViewport::ApplyModifierKeys( const FModifierKeysState& InKeysState )
{
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if ( InKeysState.IsLeftAltDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::LeftAlt, IE_Pressed );
		}
		if ( InKeysState.IsRightAltDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::RightAlt, IE_Pressed );
		}
		if ( InKeysState.IsLeftControlDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::LeftControl, IE_Pressed );
		}
		if ( InKeysState.IsRightControlDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::RightControl, IE_Pressed );
		}
		if ( InKeysState.IsLeftShiftDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::LeftShift, IE_Pressed );
		}
		if ( InKeysState.IsRightShiftDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::RightShift, IE_Pressed );
		}
	}
}

void FSceneViewport::ProcessInput( float DeltaTime )
{
	if( !ViewportClient )
	{
		return;
	}

	// Switch to the viewport clients world before processing input
	FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

	const bool bViewportHasCapture = ViewportWidget.IsValid() && ViewportWidget.Pin()->HasMouseCapture();

	if (NumMouseSamplesX > 0 || NumMouseSamplesY > 0)
	{
		ViewportClient->InputAxis( this, 0, EKeys::MouseX, MouseDelta.X, DeltaTime, NumMouseSamplesX );
		ViewportClient->InputAxis( this, 0, EKeys::MouseY, MouseDelta.Y, DeltaTime, NumMouseSamplesY );
	}

	MouseDelta = FIntPoint::ZeroValue;
	NumMouseSamplesX = 0;
	NumMouseSamplesY = 0;

}

void FSceneViewport::OnDrawViewport( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled )
{
	// Switch to the viewport clients world before resizing
	FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

	/** Check to see if the viewport should be resized */
	FIntPoint DrawSize = FIntPoint( FMath::TruncToInt( AllottedGeometry.GetDrawSize().X ), FMath::TruncToInt( AllottedGeometry.GetDrawSize().Y ) );
	if( GetSizeXY() != DrawSize )
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef() );

		check(Window.IsValid());
		ResizeViewport(FMath::Max(0, DrawSize.X), FMath::Max(0, DrawSize.Y), Window->GetWindowMode(), 0, 0);
	}	
	
	// Cannot pass negative canvas positions
	float CanvasMinX = FMath::Max(0.0f, AllottedGeometry.AbsolutePosition.X);
	float CanvasMinY = FMath::Max(0.0f, AllottedGeometry.AbsolutePosition.Y);
	FIntRect CanvasRect(
		FMath::TruncToInt( CanvasMinX ),
		FMath::TruncToInt( CanvasMinY ),
		FMath::TruncToInt( CanvasMinX + AllottedGeometry.Size.X * AllottedGeometry.Scale ), 
		FMath::TruncToInt( CanvasMinY + AllottedGeometry.Size.Y * AllottedGeometry.Scale ) );


	DebugCanvasDrawer->BeginRenderingCanvas( CanvasRect );

	// Draw above everything else
	uint32 MaxLayer = MAX_uint32;
	FSlateDrawElement::MakeCustom( OutDrawElements, MAX_uint32, DebugCanvasDrawer );

}

bool FSceneViewport::IsForegroundWindow() const
{
	bool bIsForeground = false;
	if( ViewportWidget.IsValid() )
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef() );
		if( Window.IsValid() )
		{
			bIsForeground = Window->GetNativeWindow()->IsForegroundWindow();
		}
	}

	return bIsForeground;
}

FCursorReply FSceneViewport::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent )
{
	EMouseCursor::Type MouseCursorToUse = EMouseCursor::Default;

	// If the cursor should be hidden, use EMouseCursor::None,
	// only when in the foreground, or we'll hide the mouse in the window/program above us.
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		MouseCursorToUse = ViewportClient->GetCursor( this, GetMouseX(), GetMouseY() );
	}

	// Use the default cursor if there is no viewport client or we dont have focus
	return FCursorReply::Cursor(MouseCursorToUse);
}



FReply FSceneViewport::OnMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	// Prevent throttling when interacting with the viewport so we can move around in it
	CurrentReplyState = FReply::Handled().PreventThrottling();
	
	KeyStateMap.Add( InMouseEvent.GetEffectingButton(), true );
	UpdateModifierKeys( InMouseEvent );
	UpdateCachedMousePos( InGeometry, InMouseEvent );
	UpdateCachedGeometry(InGeometry);

	// Switch to the viewport clients world before processing input
	FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue )
	{
		// If we're obtaining focus, we have to copy the modifier key states prior to processing this mouse button event, as this is the only point at which the mouse down
		// event is processed when focus initially changes and the modifier keys need to be in-place to detect any unique drag-like events.
		if ( !HasFocus() )
		{
			FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
			ApplyModifierKeys( KeysState );
		}

		// Process the mouse event
		if( !ViewportClient->InputKey( this, 0, InMouseEvent.GetEffectingButton(), IE_Pressed ) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	// Mouse down should focus viewport for keyboard input
	CurrentReplyState.SetKeyboardFocus( ViewportWidget.Pin().ToSharedRef(), EKeyboardFocusCause::Mouse );
	CurrentReplyState.UseHighPrecisionMouseMovement( ViewportWidget.Pin().ToSharedRef() );
	
	// Re-set prevent throttling here as it can get reset when inside of InputKey()
	CurrentReplyState.PreventThrottling();
	
	return CurrentReplyState;
}

FReply FSceneViewport::OnMouseButtonUp( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	KeyStateMap.Add( InMouseEvent.GetEffectingButton(), false );
	UpdateModifierKeys( InMouseEvent );
	UpdateCachedMousePos( InGeometry, InMouseEvent );
	UpdateCachedGeometry(InGeometry);

	// Switch to the viewport clients world before processing input
	FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );
	bool bIsCursorForcedVisible = true;
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		if( !ViewportClient->InputKey(this,0,InMouseEvent.GetEffectingButton(), IE_Released ) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
		bIsCursorForcedVisible = ViewportClient->GetCursor( this, GetMouseX(), GetMouseY() ) != EMouseCursor::None;
	}
	if( !( ( FApp::IsGame() && !GIsEditor ) || bIsPlayInEditorViewport ) || bIsCursorForcedVisible )
	{
		// On mouse up outside of the game (editor viewport) or if the cursor is visible in game, we should make sure the mouse is no longer captured
		// as long as the left or right mouse buttons are not still down
		if( !InMouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) && !InMouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ))
		{
			CurrentReplyState.ReleaseMouseCapture();
		}
	}
	return CurrentReplyState;
}

void FSceneViewport::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	UpdateCachedMousePos( MyGeometry, MouseEvent );
	ViewportClient->MouseEnter( this, GetMouseX(), GetMouseY() );
}

void FSceneViewport::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	ViewportClient->MouseLeave( this );
	
	if ( IsPlayInEditorViewport() )
	{
		CachedMousePos = FIntPoint(-1, -1);
	}
}

FReply FSceneViewport::OnMouseMove( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	if( !InMouseEvent.GetCursorDelta().IsZero() )
	{
		UpdateCachedMousePos( InGeometry, InMouseEvent );
		UpdateCachedGeometry(InGeometry);

		const bool bViewportHasCapture = ViewportWidget.IsValid() && ViewportWidget.Pin()->HasMouseCapture();
		if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue )
		{
			// Switch to the viewport clients world before processing input
			FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

			if( bViewportHasCapture )
			{
				ViewportClient->CapturedMouseMove( this, GetMouseX(), GetMouseY() );
			}
			else
			{
				ViewportClient->MouseMove( this, GetMouseX(), GetMouseY() );
			}
		
			if( bViewportHasCapture )
			{
				// Accumulate delta changes to mouse movment.  Depending on the sample frequency of a mouse we may get many per frame.
				//@todo Slate: In directinput, number of samples in x/y could be different...
				const FVector2D CursorDelta = InMouseEvent.GetCursorDelta();
				MouseDelta.X += CursorDelta.X;
				++NumMouseSamplesX;

				MouseDelta.Y -= CursorDelta.Y;
				++NumMouseSamplesY;
			}
		}
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnMouseWheel( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	UpdateCachedMousePos( InGeometry, InMouseEvent );
	UpdateCachedGeometry(InGeometry);

	if( ViewportClient  && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		// The viewport client accepts two different keys depending on the direction of scroll.  
		FKey const ViewportClientKey = InMouseEvent.GetWheelDelta() < 0 ? EKeys::MouseScrollDown : EKeys::MouseScrollUp;

		// Pressed and released should be sent
		ViewportClient->InputKey( this, 0, ViewportClientKey, IE_Pressed );
		ViewportClient->InputKey( this, 0, ViewportClientKey, IE_Released );
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnMouseButtonDoubleClick( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	// Note: When double-clicking, the following message sequence is sent:
	//	WM_*BUTTONDOWN
	//	WM_*BUTTONUP
	//	WM_*BUTTONDBLCLK	(Needs to set the KeyStates[*] to true)
	//	WM_*BUTTONUP
	KeyStateMap.Add( InMouseEvent.GetEffectingButton(), true );
	UpdateCachedMousePos( InGeometry, InMouseEvent );
	UpdateCachedGeometry(InGeometry);

	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputKey( this, 0, InMouseEvent.GetEffectingButton(), IE_DoubleClick ) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnControllerButtonPressed( const FGeometry& MyGeometry, const FControllerEvent& ControllerEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	KeyStateMap.Add( ControllerEvent.GetEffectingButton(), true );

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputKey( this, ControllerEvent.GetUserIndex(), ControllerEvent.GetEffectingButton(), ControllerEvent.IsRepeat() ? IE_Repeat : IE_Pressed, 1.0f, true ) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnControllerButtonReleased( const FGeometry& MyGeometry, const FControllerEvent& ControllerEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	KeyStateMap.Add( ControllerEvent.GetEffectingButton(), false );

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputKey( this, ControllerEvent.GetUserIndex(), ControllerEvent.GetEffectingButton(), IE_Released, 1.0f, true ) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnControllerAnalogValueChanged( const FGeometry& MyGeometry, const FControllerEvent& ControllerEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	KeyStateMap.Add( ControllerEvent.GetEffectingButton(), true );

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if (!ViewportClient->InputAxis(this, ControllerEvent.GetUserIndex(), ControllerEvent.GetEffectingButton(), ControllerEvent.GetEffectingButton() == EKeys::Gamepad_RightY ? -ControllerEvent.GetAnalogValue() : ControllerEvent.GetAnalogValue(), FApp::GetDeltaTime(), 1, true))
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& TouchEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled().PreventThrottling(); 

	UpdateCachedMousePos(MyGeometry, TouchEvent);
	UpdateCachedGeometry(MyGeometry);

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		const FVector2D TouchPosition = MyGeometry.AbsoluteToLocal(TouchEvent.GetLastScreenSpacePosition());

		if( !ViewportClient->InputTouch( this, TouchEvent.GetUserIndex(), TouchEvent.GetPointerIndex(), ETouchType::Began, TouchPosition, FDateTime::Now(), TouchEvent.GetTouchpadIndex()) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& TouchEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	UpdateCachedMousePos(MyGeometry, TouchEvent);
	UpdateCachedGeometry(MyGeometry);

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		const FVector2D TouchPosition = MyGeometry.AbsoluteToLocal(TouchEvent.GetLastScreenSpacePosition());

		if( !ViewportClient->InputTouch( this, TouchEvent.GetUserIndex(), TouchEvent.GetPointerIndex(), ETouchType::Moved, TouchPosition, FDateTime::Now(), TouchEvent.GetTouchpadIndex()) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& TouchEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	UpdateCachedMousePos(MyGeometry, TouchEvent);
	UpdateCachedGeometry(MyGeometry);

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		const FVector2D TouchPosition = MyGeometry.AbsoluteToLocal(TouchEvent.GetLastScreenSpacePosition());

		if( !ViewportClient->InputTouch( this, TouchEvent.GetUserIndex(), TouchEvent.GetPointerIndex(), ETouchType::Ended, TouchPosition, FDateTime::Now(), TouchEvent.GetTouchpadIndex()) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& GestureEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	UpdateCachedMousePos( MyGeometry, GestureEvent );
	UpdateCachedGeometry( MyGeometry );

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputGesture( this, GestureEvent.GetGestureType(), GestureEvent.GetGestureDelta() ) )
		{
			CurrentReplyState = FReply::Unhandled();
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& MotionEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputMotion( this, MotionEvent.GetUserIndex(), MotionEvent.GetTilt(), MotionEvent.GetRotationRate(), MotionEvent.GetGravity(), MotionEvent.GetAcceleration()) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnKeyDown( const FGeometry& InGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	FKey Key = InKeyboardEvent.GetKey();
	KeyStateMap.Add( Key, true );

	//@todo Slate Viewports: FWindowsViewport checks for Alt+Enter or F11 and toggles fullscreen.  Unknown if fullscreen via this method will be needed for slate viewports. 
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputKey( this, 0, Key, InKeyboardEvent.IsRepeat() ? IE_Repeat : IE_Pressed ) )
		{
			CurrentReplyState = FReply::Unhandled();
		}
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnKeyUp( const FGeometry& InGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	FKey Key = InKeyboardEvent.GetKey();
	KeyStateMap.Add( Key, false );
	
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputKey(this, 0, Key, IE_Released) )
		{
			CurrentReplyState = FReply::Unhandled();
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnKeyChar( const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputChar( this,0, InCharacterEvent.GetCharacter() ) )
		{
			CurrentReplyState = FReply::Unhandled();
		}
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnKeyboardFocusReceived( const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	CurrentReplyState = FReply::Handled(); 

	if( ViewportClient )
	{
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );
		ViewportClient->ReceivedFocus( this );

		if( ( FApp::IsGame() && !GIsEditor ) || bIsPlayInEditorViewport )
		{
			if( IsForegroundWindow() )
			{
				const bool bIsCursorForcedVisible =
					( ViewportClient != NULL && ViewportClient->GetCursor( this, GetMouseX(), GetMouseY() ) != EMouseCursor::None );

				const bool bPlayInEditorCapture = !bIsPlayInEditorViewport || InKeyboardFocusEvent.GetCause() != EKeyboardFocusCause::SetDirectly || bPlayInEditorGetsMouseControl;

				// capturing the mouse interferes with slate UI (like the virtual joysticks)
				if (FPlatformProperties::SupportsWindowedMode() && bPlayInEditorCapture && !bIsCursorForcedVisible && !FSlateApplication::Get().IsFakingTouchEvents())
				{
					// Only require the user to click in the window the first time - after that return focus to the game so long as it was the last focused widget.
					// Means that tabbing in/out will return the mouse control to where it was & the in-game console won't leave the mouse under editor control.
					bPlayInEditorGetsMouseControl = true;
					CurrentReplyState.UseHighPrecisionMouseMovement( ViewportWidget.Pin().ToSharedRef() );
					CurrentReplyState.LockMouseToWidget( ViewportWidget.Pin().ToSharedRef() );
				}
				else if(!bPlayInEditorCapture)
				{
					FSlateApplication::Get().ClearKeyboardFocus( EKeyboardFocusCause::SetDirectly );
					FSlateApplication::Get().ResetToDefaultInputSettings();
				}
			}
			else
			{
				FSlateApplication::Get().ClearKeyboardFocus( EKeyboardFocusCause::Cleared );
			}
		}
	}

	return CurrentReplyState;
}

void FSceneViewport::OnKeyboardFocusLost( const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	KeyStateMap.Empty();
	if( ViewportClient )
	{
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );
		ViewportClient->LostFocus( this );

		TSharedPtr<SWidget> ViewportWidgetPin = ViewportWidget.Pin();
		if( ViewportWidgetPin.IsValid() )
		{
			for( int32 UserIndex = 0; UserIndex < SlateApplicationDefs::MaxUsers; ++UserIndex )
			{
				if( FSlateApplication::Get().GetJoystickCaptor(UserIndex) == ViewportWidgetPin )
				{
					FSlateApplication::Get().ReleaseJoystickCapture( UserIndex );
				}
			}
		}
	}
}

void FSceneViewport::OnViewportClosed()
{
	if( ViewportClient )
	{
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );
		ViewportClient->CloseRequested( this );
	}
}

FSlateShaderResource* FSceneViewport::GetViewportRenderTargetTexture() const
{ 
	return SlateRenderTargetHandle; 
}

void FSceneViewport::ResizeFrame(uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, int32 InPosX, int32 InPosY)
{
	// Resizing the window directly is only supported in the game
	if( FApp::IsGame() && NewSizeX > 0 && NewSizeY > 0 )
	{		
		FWidgetPath WidgetPath;
		TSharedPtr<SWindow> WindowToResize = FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef(), WidgetPath );

		if( WindowToResize.IsValid() )
		{
			int32 CVarValue = GetBoundFullScreenModeCVar();
			EWindowMode::Type DesiredWindowMode = GetWindowModeType(NewWindowMode);
			
			// Avoid resizing if nothing changes.
			bool bNeedsResize = SizeX != NewSizeX || SizeY != NewSizeY || NewWindowMode != DesiredWindowMode || DesiredWindowMode != WindowToResize->GetWindowMode();

			if (bNeedsResize)
			{
				if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHMDEnabled())
				{
					// Resize & move only if moving to a fullscreen mode
					if (NewWindowMode != EWindowMode::Windowed)
					{
						FSlateRect PreFullScreenRect = WindowToResize->GetRectInScreen();

						IHeadMountedDisplay::MonitorInfo MonitorInfo;
						GEngine->HMDDevice->GetHMDMonitorInfo(MonitorInfo);
						NewSizeX = MonitorInfo.ResolutionX;
						NewSizeY = MonitorInfo.ResolutionY;
						WindowToResize->ReshapeWindow(FVector2D(MonitorInfo.DesktopX, MonitorInfo.DesktopY), FVector2D(MonitorInfo.ResolutionX, MonitorInfo.ResolutionY));

						GEngine->HMDDevice->PushPreFullScreenRect(PreFullScreenRect);
					}
				}

				// Toggle fullscreen and resize
				WindowToResize->SetWindowMode(DesiredWindowMode);

				if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHMDEnabled())
				{
					if (NewWindowMode == EWindowMode::Windowed)
					{
						FSlateRect PreFullScreenRect;
						GEngine->HMDDevice->PopPreFullScreenRect(PreFullScreenRect);
						if (PreFullScreenRect.GetSize().X > 0 && PreFullScreenRect.GetSize().Y > 0)
						{
							NewSizeX = PreFullScreenRect.GetSize().X;
							NewSizeY = PreFullScreenRect.GetSize().Y;
							WindowToResize->MoveWindowTo (FVector2D(PreFullScreenRect.Left, PreFullScreenRect.Top));
						}
					}

					if (NewWindowMode != WindowMode)
					{
						// Only notify the HMD if we've actually changed modes
						GEngine->HMDDevice->OnScreenModeChange(NewWindowMode);
					}
				}

				LockMouseToViewport(!CurrentReplyState.ShouldReleaseMouseLock());

				int32 NewWindowSizeX = NewSizeX;
				int32 NewWindowSizeY = NewSizeY;

				if (DesiredWindowMode != EWindowMode::Windowed && CVarValue != 0)
				{
					FSlateRect Rect = WindowToResize->GetFullScreenInfo();

					// needs to be implemented
					if( Rect.IsValid() )
					{
						NewWindowSizeX = Rect.GetSize().X;
						NewWindowSizeY = Rect.GetSize().Y;
					}
				}

				WindowToResize->Resize( FVector2D(NewWindowSizeX, NewWindowSizeY) );

				ResizeViewport(NewWindowSizeX, NewWindowSizeY, NewWindowMode, InPosX, InPosY);
			}
			UCanvas::UpdateAllCanvasSafeZoneData();
		}		
	}
}

void FSceneViewport::ResizeViewport(uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, int32 InPosX, int32 InPosY)
{
	// Do not resize if the viewport is an invalid size or our UI should be responsive
	if( NewSizeX > 0 && NewSizeY > 0 && FSlateThrottleManager::Get().IsAllowingExpensiveTasks() )
	{
		bIsResizing = true;

		UpdateViewportRHI(false, NewSizeX, NewSizeY, NewWindowMode);

		if (ViewportClient)
		{
			// Invalidate, then redraw immediately so the user isn't left looking at an empty black viewport
			// as they continue to resize the window.
			Invalidate();

			if ( ViewportClient->GetWorld() )
			{
				Draw();
			}
		}

		//if we have a delegate, fire it off
		if(FApp::IsGame() && OnSceneViewportResizeDel.IsBound())
		{
			OnSceneViewportResizeDel.Execute(FVector2D(NewSizeX, NewSizeY));
		}

		bIsResizing = false;
	}
}

void FSceneViewport::InvalidateDisplay()
{
	// Dirty the viewport.  It will be redrawn next time the editor ticks.
	if( ViewportClient != NULL )
	{
		ViewportClient->RedrawRequested( this );
	}
}

void FSceneViewport::DeferInvalidateHitProxy()
{
	if( ViewportClient != NULL )
	{
		ViewportClient->RequestInvalidateHitProxy( this );
	}
}

FCanvas* FSceneViewport::GetDebugCanvas()
{
	return DebugCanvasDrawer->GetGameThreadDebugCanvas();
}

void FSceneViewport::UpdateViewportRHI(bool bDestroyed, uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode)
{
	// Make sure we're not in the middle of streaming textures.
	(*GFlushStreamingFunc)();

	{
		SCOPED_SUSPEND_RENDERING_THREAD(true);

		// Update the viewport attributes.
		// This is done AFTER the command flush done by UpdateViewportRHI, to avoid disrupting rendering thread accesses to the old viewport size.
		SizeX = NewSizeX;
		SizeY = NewSizeY;
		WindowMode = NewWindowMode;

		// Release the viewport's resources.
		BeginReleaseResource(this);

		if( !bDestroyed )
		{
			BeginInitResource(this);
				
			if( !bUseSeparateRenderTarget )
			{
				// Get the viewport for this window from the renderer so we can render directly to the backbuffer
				TSharedPtr<FSlateRenderer> Renderer = FSlateApplication::Get().GetRenderer();
				FWidgetPath WidgetPath;
				void* ViewportResource = Renderer->GetViewportResource( *FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef(), WidgetPath ) );
				if( ViewportResource )
				{
					ViewportRHI = *((FViewportRHIRef*)ViewportResource);
				}
			}

			ViewportResizedEvent.Broadcast(this, 0);
		}
		else
		{
			// Enqueue a render command to delete the handle.  It must be deleted on the render thread after the resource is released
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(DeleteSlateRenderTarget, FSlateRenderTargetRHI*&, SlateRenderTargetHandle, SlateRenderTargetHandle,
			{
				delete SlateRenderTargetHandle;
				SlateRenderTargetHandle = NULL;
			});

		}
	}
}

void FSceneViewport::EnqueueBeginRenderFrame()
{
	check( IsInGameThread() );

	DebugCanvasDrawer->InitDebugCanvas(GetClient()->GetWorld());

	// Note: ViewportRHI is only updated on the game thread

	// If we dont have the ViewportRHI then we need to get it before rendering
	if( !bUseSeparateRenderTarget && !IsValidRef(ViewportRHI) )
	{
		// Get the viewport for this window from the renderer so we can render directly to the backbuffer
		TSharedPtr<FSlateRenderer> Renderer = FSlateApplication::Get().GetRenderer();
		FWidgetPath WidgetPath;
		void* ViewportResource = Renderer->GetViewportResource( *FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef(), WidgetPath ) );
		if( ViewportResource )
		{
			ViewportRHI = *((FViewportRHIRef*)ViewportResource);
		}

	}

	FViewport::EnqueueBeginRenderFrame();
}

void FSceneViewport::BeginRenderFrame()
{
	check( IsInRenderingThread() );
	
	RHIBeginScene();

	if( bUseSeparateRenderTarget )
	{
		RHISetRenderTarget( RenderTargetTextureRHI,  FTexture2DRHIRef() );
	}
	else if( IsValidRef( ViewportRHI ) ) 
	{
		// Get the backbuffer render target to render directly to it
		RenderTargetTextureRHI = RHIGetViewportBackBuffer( ViewportRHI );
		RHISetRenderTarget( RenderTargetTextureRHI,  FTexture2DRHIRef() );
	}
}

void FSceneViewport::EndRenderFrame( bool bPresent, bool bLockToVsync )
{
	check( IsInRenderingThread() );
	if( bUseSeparateRenderTarget )
	{
		// @todo-mobile
		if (GRHIShaderPlatform == SP_OPENGL_ES2)
		{
			check(0);
		}
		RHICopyToResolveTarget( RenderTargetTextureRHI, SlateRenderTargetHandle->GetRHIRef(), false, FResolveParams() ); 
	}
	else
	{
		// Set the active render target(s) to nothing to release references in the case that the viewport is resized by slate before we draw again
		RHISetRenderTarget( FTexture2DRHIRef(), FTexture2DRHIRef() );
		// Note: this releases our reference but does not release the resource as it is owned by slate (this is intended)
		RenderTargetTextureRHI.SafeRelease();
	}

	RHIEndScene();
}

void FSceneViewport::Tick( float DeltaTime )
{
	ProcessInput( DeltaTime );
}

void FSceneViewport::OnPlayWorldViewportSwapped( const FSceneViewport& OtherViewport )
{
	// Play world viewports should always be the same size.  Resize to other viewports size
	if( GetSizeXY() != OtherViewport.GetSizeXY() )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		UpdateViewportRHI( false, OtherViewport.GetSizeXY().X, OtherViewport.GetSizeXY().Y, EWindowMode::Windowed );

		// Invalidate, then redraw immediately so the user isn't left looking at an empty black viewport
		// as they continue to resize the window.
		Invalidate();
	}

	// Play world viewports should transfer active stats so it doesn't appear like a seperate viewport
	SwapStatCommands(OtherViewport);
}


void FSceneViewport::SwapStatCommands( const FSceneViewport& OtherViewport )
{
	FViewportClient* ClientA = GetClient();
	FViewportClient* ClientB = OtherViewport.GetClient();
	check(ClientA && ClientB);
	// Only swap if both viewports have stats
	const TArray<FString>* StatsA = ClientA->GetEnabledStats();
	const TArray<FString>* StatsB = ClientB->GetEnabledStats();
	if (StatsA && StatsB)
	{
		const TArray<FString> StatsCopy = *StatsA;
		ClientA->SetEnabledStats(*StatsB);
		ClientB->SetEnabledStats(StatsCopy);
	}
}

void FSceneViewport::InitDynamicRHI()
{
	if(bRequiresHitProxyStorage)
	{
		// Initialize the hit proxy map.
		HitProxyMap.Init(SizeX,SizeY);
	}

	if( bUseSeparateRenderTarget )
	{
		FTexture2DRHIRef ShaderResourceTextureRHI;

		RHICreateTargetableShaderResource2D( SizeX, SizeY, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, RenderTargetTextureRHI, ShaderResourceTextureRHI );

		if( !SlateRenderTargetHandle )
		{
			SlateRenderTargetHandle = new FSlateRenderTargetRHI( ShaderResourceTextureRHI, SizeX, SizeY );
		}
		else
		{
			SlateRenderTargetHandle->SetRHIRef( ShaderResourceTextureRHI, SizeX, SizeY );
		}
	}
}

void FSceneViewport::ReleaseDynamicRHI()
{
	FViewport::ReleaseDynamicRHI();

	ViewportRHI.SafeRelease();

	if( SlateRenderTargetHandle )
	{
		SlateRenderTargetHandle->ReleaseDynamicRHI();
	}
}

void FSceneViewport::SetPreCaptureMousePosFromSlateCursor()
{
	PreCaptureMousePos = FSlateApplication::Get().GetCursorPos().IntPoint();
}


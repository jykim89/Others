// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "MaterialExpression.generated.h"

//@warning: FExpressionInput is mirrored in MaterialShared.h and manually "subclassed" in Material.h (FMaterialInput)
#if !CPP      //noexport struct
USTRUCT(noexport)
struct FExpressionInput
{
	/** UMaterial expression that this input is connected to, or NULL if not connected. */
	UPROPERTY()
	class UMaterialExpression* Expression;

	/** Index into Expression's outputs array that this input is connected to. */
	UPROPERTY()
	int32 OutputIndex;

	/** 
	 * optional FName of the input.  
	 * Note that this is the only member which is not derived from the output currently connected. 
	 */
	UPROPERTY()
	FString InputName;

	UPROPERTY()
	int32 Mask;

	UPROPERTY()
	int32 MaskR;

	UPROPERTY()
	int32 MaskG;

	UPROPERTY()
	int32 MaskB;

	UPROPERTY()
	int32 MaskA;

	UPROPERTY()
	int32 GCC64_Padding;    // @todo 64: if the C++ didn't mismirror this structure (with MaterialInput), we might not need this

};

USTRUCT(noexport)
struct FMaterialAttributesInput : public FExpressionInput
{
};

#endif

/** Struct that represents an expression's output. */
#if !CPP      //noexport struct
USTRUCT(noexport)
struct FExpressionOutput
{
	UPROPERTY()
	FString OutputName;

	UPROPERTY()
	int32 Mask;

	UPROPERTY()
	int32 MaskR;

	UPROPERTY()
	int32 MaskG;

	UPROPERTY()
	int32 MaskB;

	UPROPERTY()
	int32 MaskA;

};
#endif

UCLASS(abstract, hidecategories=Object, MinimalAPI)
class UMaterialExpression : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	int32 MaterialExpressionEditorX;

	UPROPERTY()
	int32 MaterialExpressionEditorY;

	/** Expression's Graph representation */
	class UEdGraphNode*	GraphNode;

	/** Text of last error for this expression */
	FString LastErrorText;

#endif // WITH_EDITORONLY_DATA
	/** Set to true by RecursiveUpdateRealtimePreview() if the expression's preview needs to be updated in realtime in the material editor. */
	UPROPERTY()
	uint32 bRealtimePreview:1;

	/** If true, we should update the preview next render. This is set when changing bRealtimePreview. */
	UPROPERTY(transient)
	uint32 bNeedToUpdatePreview:1;

	/** Indicates that this is a 'parameter' type of expression and should always be loaded (ie not cooked away) because we might want the default parameter. */
	UPROPERTY()
	uint32 bIsParameterExpression:1;

	/** 
	 * The material that this expression is currently being compiled in.  
	 * This is not necessarily the object which owns this expression, for example a preview material compiling a material function's expressions.
	 */
	UPROPERTY()
	class UMaterial* Material;

	/** 
	 * The material function that this expression is being used with, if any.
	 * This will be NULL if the expression belongs to a function that is currently being edited, 
	 */
	UPROPERTY()
	class UMaterialFunction* Function;

	/** A description that level designers can add (shows in the material editor UI). */
	UPROPERTY(EditAnywhere, Category=MaterialExpression)
	FString Desc;

	/** Color of the expression's border outline. */
	UPROPERTY()
	FColor BorderColor;

	/** If true, use the output name as the label for the pin */
	UPROPERTY()
	uint32 bShowOutputNameOnPin:1;

	/** If true, do not render the preview window for the expression */
	UPROPERTY()
	uint32 bHidePreviewWindow:1;

	/** If true, show a collapsed version of the node */
	UPROPERTY()
	uint32 bCollapsed:1;

	/** Whether the node represents an input to the shader or not.  Used to color the node's background. */
	UPROPERTY()
	uint32 bShaderInputData:1;

	/** Whether to draw the expression's inputs. */
	UPROPERTY()
	uint32 bShowInputs:1;

	/** Whether to draw the expression's outputs. */
	UPROPERTY()
	uint32 bShowOutputs:1;

	/** Localized categories to sort this expression into... */
	UPROPERTY()
	TArray<FString> MenuCategories;

	/** The expression's outputs, which are set in default properties by derived classes. */
	UPROPERTY()
	TArray<FExpressionOutput> Outputs;

	// Begin UObject interface.
	virtual void PostInitProperties() OVERRIDE;
	virtual void PostLoad() OVERRIDE;
	virtual void PostDuplicate(bool bDuplicateForPIE) OVERRIDE;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	virtual void PostEditImport() OVERRIDE;
	virtual bool CanEditChange( const UProperty* InProperty ) const OVERRIDE;
#endif // WITH_EDITOR

	virtual bool Modify( bool bAlwaysMarkDirty=true ) OVERRIDE;
	virtual void Serialize( FArchive& Ar );
	// End UObject interface.

	/**
	 * Create the new shader code chunk needed for the Abs expression
	 *
	 * @param	Compiler - UMaterial compiler that knows how to handle this expression.
	 * @param	MultiplexIndex - An index used by some expressions to send multiple values across a single connection.
	 * @return	Index to the new FMaterialCompiler::CodeChunk entry for this expression
	 */	
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) { return INDEX_NONE; }
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) { return Compile(Compiler, OutputIndex, MultiplexIndex); }

	/** 
	 * Callback to get any texture reference this expression emits.
	 * This is used to link the compiled uniform expressions with their default texture values. 
	 * Any UMaterialExpression whose compilation creates a texture uniform expression (eg Compiler->Texture, Compiler->TextureParameter) must implement this.
	 */
	virtual UTexture* GetReferencedTexture() 
	{
		return NULL;
	}

	/**
	 *	Get the outputs supported by this expression.
	 *
	 *	@param	Outputs		The TArray of outputs to fill in.
	 */
	virtual TArray<FExpressionOutput>& GetOutputs();
	virtual const TArray<FExpressionInput*> GetInputs();
	virtual FExpressionInput* GetInput(int32 InputIndex);
	virtual FString GetInputName(int32 InputIndex) const;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex);
	virtual uint32 GetOutputType(int32 OutputIndex);
#endif

	/**
	 *	Get the width required by this expression (in the material editor).
	 *
	 *	@return	int32			The width in pixels.
	 */
	virtual int32 GetWidth() const;
	virtual int32 GetHeight() const;
	virtual bool UsesLeftGutter() const;
	virtual bool UsesRightGutter() const;

	/**
	 *	Returns the text to display on the material expression (in the material editor).
	 *
	 *	@return	FString		The text to display.
	 */
	virtual void GetCaption(TArray<FString>& OutCaptions) const;
#if WITH_EDITOR
	/** Get a single line description of the material expression (used for lists) */
	virtual FString GetDescription() const;
	/** Get a tooltip for the specified connector. */
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip);

	/** Get a tooltip for the expression itself. */
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) {}
#endif
	/**
	 *	Returns the amount of padding to use for the label.
	 *
	 *	@return int32			The padding (in pixels).
	 */
	virtual int GetLabelPadding() { return 0; }
	virtual int32 CompilerError(class FMaterialCompiler* Compiler, const TCHAR* pcMessage);


	/**
	 * @return whether the expression preview needs realtime update
	 */
	virtual bool NeedsRealtimePreview() { return false; }

	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return true if the expression matches the search query
	 */
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery );

	/**
	 * Copy the SrcExpressions into the specified material, preserving internal references.
	 * New material expressions are created within the specified material.
	 */
	ENGINE_API static void CopyMaterialExpressions(const TArray<class UMaterialExpression*>& SrcExpressions, const TArray<class UMaterialExpressionComment*>& SrcExpressionComments, 
		class UMaterial* Material, class UMaterialFunction* Function, TArray<class UMaterialExpression*>& OutNewExpressions, TArray<class UMaterialExpression*>& OutNewComments);

	/**
	 * Marks certain expression types as outputting material attributes. Allows the material editor preview material to know if it should use its material attributes pin.
	 */
	virtual bool IsResultMaterialAttributes(int32 OutputIndex){return false;}

	/**
	 * Connects the specified output to the passed material for previewing. 
	 */
	ENGINE_API void ConnectToPreviewMaterial(UMaterial* Material, int32 OutputIndex);

	/**
	 * Connects the specified input expression to the specified output of this expression.
	 */
	ENGINE_API void ConnectExpression(FExpressionInput* Input, int32 OutputIndex);

	/** 
	* Generates a GUID for this expression if one doesn't already exist. 
	*
	* @param bForceGeneration	Whether we should generate a GUID even if it is already valid.
	*/
	ENGINE_API void UpdateParameterGuid(bool bForceGeneration, bool bAllowMarkingPackageDirty);

	/** Callback to access derived classes' parameter expression id. */
	ENGINE_API virtual FGuid& GetParameterExpressionId()
	{
		checkf(!bIsParameterExpression, TEXT("Expressions with bIsParameterExpression==true must implement their own GetParameterExpressionId!"));
		static FGuid Dummy;
		return Dummy;
	}

	/** Asserts if the expression is not contained by its Material or Function's expressions array. */
	ENGINE_API void ValidateState();

#if WITH_EDITOR
	/** Returns the keywords that should be used when searching for this expression */
	virtual FString GetKeywords() const {return TEXT("");}

	/**
	 * Recursively gets a list of all expressions that are connected to this
	 * Checks for repeats so that it can't end up in an infinite loop
	 *
	 * @param InputExpressions Array to contain/pass on expressions
	 *
	 * @return Whether a repeat was found while getting expressions
	 */
	ENGINE_API bool GetAllInputExpressions(TArray<UMaterialExpression*>& InputExpressions);

#endif // WITH_EDITOR

	/** Checks whether any inputs to this expression create a loop */
	ENGINE_API bool ContainsInputLoop();

protected:
	/**
	 * Checks whether any inputs to this expression create a loop by recursively
	 * calling itself and keeping a list of inputs as expression keys.
	 *
	 * @param ExpressionStack List of expression keys that have been checked already
	 */
	bool ContainsInputLoopInternal(TArray<FMaterialExpressionKey>& ExpressionStack);
};




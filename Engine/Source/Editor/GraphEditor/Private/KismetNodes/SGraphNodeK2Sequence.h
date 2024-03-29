// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#ifndef __SGraphNodeK2Sequence_h__
#define __SGraphNodeK2Sequence_h__

class SGraphNodeK2Sequence : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Sequence){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, UK2Node* InNode );

protected:
	// SGraphNode interface
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) OVERRIDE;
	virtual FReply OnAddPin() OVERRIDE;
	// End of SGraphNode interface
};

#endif // __SGraphNodeK2Sequence_h__
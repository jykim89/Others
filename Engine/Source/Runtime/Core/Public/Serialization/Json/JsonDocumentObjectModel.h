// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

class FJsonObject;

/**
 * A Json Value is a structure that can be any of the Json Types.
 * It should never be used on its, only its derived types should be used.
 */
class CORE_API FJsonValue
{
public:
	/** Returns this value as a double, throwing an error if this is not an Json Number */
	virtual double AsNumber() const {ErrorMessage(TEXT("Number")); return 0.f;}

	/** Returns this value as a number, throwing an error if this is not an Json String */
	virtual FString AsString() const {ErrorMessage(TEXT("String")); return FString();}

	/** Returns this value as a boolean, throwing an error if this is not an Json Bool */
	virtual bool AsBool() const {ErrorMessage(TEXT("Boolean")); return false;}

	/** Returns this value as an array, throwing an error if this is not an Json Array */
	virtual const TArray< TSharedPtr<FJsonValue> >& AsArray() const {ErrorMessage(TEXT("Array")); return EMPTY_ARRAY;}

	/** Returns this value as an object, throwing an error if this is not an Json Object */
	virtual const TSharedPtr<FJsonObject>& AsObject() const {ErrorMessage(TEXT("Object")); return EMPTY_OBJECT;}

	/** Returns true if this value is a 'null' */
	bool IsNull() const { return Type == EJson::Null || Type == EJson::None; }

	/** Get a field of the same type as the argument */
	void AsArgumentType(double                          & Value) { Value = AsNumber(); }
	void AsArgumentType(FString                         & Value) { Value = AsString(); }
	void AsArgumentType(bool                            & Value) { Value = AsBool  (); }
	void AsArgumentType(TArray< TSharedPtr<FJsonValue> >& Value) { Value = AsArray (); }
	void AsArgumentType(TSharedPtr<FJsonObject>         & Value) { Value = AsObject(); }

	EJson::Type Type;

	static bool CompareEqual(const FJsonValue& Lhs, const FJsonValue& Rhs);
	bool operator==(const FJsonValue& Rhs) const { return CompareEqual(*this, Rhs); }

protected:

	static const TArray< TSharedPtr<FJsonValue> > EMPTY_ARRAY;
	static const TSharedPtr<FJsonObject> EMPTY_OBJECT;

	FJsonValue() : Type(EJson::None) {}
	virtual ~FJsonValue() {}

	virtual FString GetType() const = 0;

	void ErrorMessage(const FString& InType) const {UE_LOG(LogJson, Error, TEXT("Json Value of type '%s' used as a '%s'."), *GetType(), *InType);}
};

/** A Json String Value. */
class CORE_API FJsonValueString : public FJsonValue
{
public:
	FJsonValueString(const FString& InString) : Value(InString) {Type = EJson::String;}

	virtual FString AsString() const {return Value;}
	virtual double AsNumber() const {return Value.IsNumeric() ? FCString::Atod(*Value) : FJsonValue::AsNumber();}
	virtual bool AsBool() const {return Value.ToBool();}

protected:
	FString Value;

	virtual FString GetType() const {return TEXT("String");}
};

/** A Json Number Value. */
class CORE_API FJsonValueNumber : public FJsonValue
{
public:
	FJsonValueNumber(double InNumber) : Value(InNumber) {Type = EJson::Number;}
	virtual double AsNumber() const {return Value;}
	virtual bool AsBool() const {return Value != 0.0;}
	virtual FString AsString() const {return FString::SanitizeFloat(Value);}
	
protected:
	double Value;

	virtual FString GetType() const {return TEXT("Number");}
};

/** A Json Boolean Value. */
class CORE_API FJsonValueBoolean : public FJsonValue
{
public:
	FJsonValueBoolean(bool InBool) : Value(InBool) {Type = EJson::Boolean;}
	virtual double AsNumber() const {return Value ? 1 : 0;}
	virtual bool AsBool() const {return Value;}
	virtual FString AsString() const {return Value ? TEXT("true") : TEXT("false");}
	
protected:
	bool Value;

	virtual FString GetType() const {return TEXT("Boolean");}
};

/** A Json Array Value. */
class CORE_API FJsonValueArray : public FJsonValue
{
public:
	FJsonValueArray(const TArray< TSharedPtr<FJsonValue> >& InArray) : Value(InArray) {Type = EJson::Array;}
	virtual const TArray< TSharedPtr<FJsonValue> >& AsArray() const {return Value;}
	
protected:
	TArray< TSharedPtr<FJsonValue> > Value;

	virtual FString GetType() const {return TEXT("Array");}
};

/** A Json Object Value. */
class CORE_API FJsonValueObject : public FJsonValue
{
public:
	FJsonValueObject(TSharedPtr<FJsonObject> InObject) : Value(InObject) {Type = EJson::Object;}
	virtual const TSharedPtr<FJsonObject>& AsObject() const {return Value;}
	
protected:
	TSharedPtr<FJsonObject> Value;

	virtual FString GetType() const {return TEXT("Object");}
};

/** A Json Null Value. */
class CORE_API FJsonValueNull : public FJsonValue
{
public:
	FJsonValueNull() {Type = EJson::Null;}

protected:
	virtual FString GetType() const {return TEXT("Null");}
};


/**
 * A Json Object is a structure holding an unordered set of name/value pairs.
 * In a Json file, it is represented by everything between curly braces {}.
 */
class CORE_API FJsonObject
{
public:
	TMap< FString, TSharedPtr<FJsonValue> > Values;

	template<EJson::Type JsonType>
	TSharedPtr<FJsonValue> GetField( const FString& FieldName ) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
		if ( Field != NULL && Field->IsValid() )
		{
			if (JsonType == EJson::None || (*Field)->Type == JsonType)
			{
				return (*Field);
			}
			else
			{
				UE_LOG(LogJson, Warning, TEXT("Field %s is of the wrong type."), *FieldName);
			}
		}
		else
		{
			UE_LOG(LogJson, Warning, TEXT("Field %s was not found."), *FieldName);
		}

		return TSharedPtr<FJsonValue>(new FJsonValueNull());
	}

	TSharedPtr<FJsonValue> TryGetField( const FString& FieldName )
	{
		const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
		return (Field != NULL && Field->IsValid()) ? *Field : TSharedPtr<FJsonValue>();
	}

	/** Checks to see if the FieldName exists in the object. */
	bool HasField( const FString& FieldName) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
		if(Field && Field->IsValid())
		{
			return true;
		}

		return false;
	}
	
	/** Checks to see if the FieldName exists in the object, and has the specified type. */
	template<EJson::Type JsonType>
	bool HasTypedField(const FString& FieldName) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
		if(Field && Field->IsValid() && ((*Field)->Type == JsonType))
		{
			return true;
		}

		return false;
	}

	void SetField( const FString& FieldName, const TSharedPtr<FJsonValue>& Value );

	void RemoveField(const FString& FieldName);

	/** Get the field named FieldName as a number. Ensures that the field is present and is of type Json number. */
	double GetNumberField(const FString& FieldName) const;

	/** Add a field named FieldName with Number as value */
	void SetNumberField( const FString& FieldName, double Number );

	/** Get the field named FieldName as a string. */
	FString GetStringField(const FString& FieldName) const;

	/** Add a field named FieldName with value of StringValue */
	void SetStringField( const FString& FieldName, const FString& StringValue );

	/** Get the field named FieldName as a boolean. */
	bool GetBoolField(const FString& FieldName) const;

	/** Set a boolean field named FieldName and value of InValue */
	void SetBoolField( const FString& FieldName, bool InValue );

	/** Get the field named FieldName as an array. */
	const TArray< TSharedPtr<FJsonValue> >& GetArrayField(const FString& FieldName) const;

	/** Set an array field named FieldName and value of Array */
	void SetArrayField( const FString& FieldName, const TArray< TSharedPtr<FJsonValue> >& Array );

	/** Get the field named FieldName as a Json object. */
	const TSharedPtr<FJsonObject>& GetObjectField(const FString& FieldName) const;

	/** Set an ObjectField named FieldName and value of JsonObject */
	void SetObjectField( const FString& FieldName, const TSharedPtr<FJsonObject>& JsonObject );
};

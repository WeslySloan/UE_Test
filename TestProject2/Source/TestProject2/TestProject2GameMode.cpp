// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestProject2GameMode.h"
#include "TestProject2Character.h"
#include "UObject/ConstructorHelpers.h"

ATestProject2GameMode::ATestProject2GameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

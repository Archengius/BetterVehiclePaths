// Copyright Epic Games, Inc. All Rights Reserved.

#include "BetterVehiclePaths.h"
#include "BVPSettings.h"
#include "BVPSubsystem.h"
#include "EnhancedInputComponent.h"
#include "FGCharacterPlayer.h"

void FBetterVehiclePathsModule::StartupModule()
{
	OnInputInitializedHandle = AFGCharacterPlayer::OnPlayerInputInitialized.AddLambda( []( AFGCharacterPlayer* CharacterPlayer, UInputComponent* InputComponent )
	{
		if ( UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>( InputComponent ) )
		{
			if ( UBVPSubsystem* BVPSubsystem = CharacterPlayer->GetWorld()->GetSubsystem<UBVPSubsystem>() )
			{
				BVPSubsystem->BindPlayerActions( CharacterPlayer, EnhancedInputComponent );
			}
		}
	} );
}

void FBetterVehiclePathsModule::ShutdownModule()
{
	AFGCharacterPlayer::OnPlayerInputInitialized.Remove( OnInputInitializedHandle );
	OnInputInitializedHandle.Reset();
}

IMPLEMENT_MODULE(FBetterVehiclePathsModule, BetterVehiclePaths)
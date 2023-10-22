// Copyright Nikita Zolotukhin. All Rights Reserved.

#include "BVPPathVisualizationActor.h"

ABVPPathVisualizationActor::ABVPPathVisualizationActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>( TEXT("Root") );
	RootComponent->SetMobility( EComponentMobility::Movable );
}

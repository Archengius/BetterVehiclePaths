// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BVPPathVisualizationActor.generated.h"

class AFGDrivingTargetList;

UCLASS()
class BETTERVEHICLEPATHS_API ABVPPathVisualizationActor : public AActor
{
	GENERATED_BODY()
public:
	ABVPPathVisualizationActor();

	// The target list this actor belongs to.
	UPROPERTY( VisibleAnywhere, BlueprintReadOnly, Category = "BVP Subsystem" )
	AFGDrivingTargetList* OwnerTargetList;
};

// Copyright Nikita Zolotukhin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class UBVPSubsystem;
class AFGTargetPoint;
class AFGDrivingTargetList;
class UMaterialInstanceDynamic;
class FBVPVehiclePathSegmentVisualization;
class UMaterialInterface;
class AActor;

// Visualization of a vehicle path (e.g. target point list).
class BETTERVEHICLEPATHS_API FBVPVehiclePathVisualization
{
protected:
	UBVPSubsystem* OwnerSubsystem{};
	AFGDrivingTargetList* TargetPointList{};
	UMaterialInstanceDynamic* MaterialInstance{};
	AActor* VisualizationActor{};
	TArray<FBVPVehiclePathSegmentVisualization*> VisualizationSegments;
public:
	FBVPVehiclePathVisualization( UBVPSubsystem* InSubsystem, AFGDrivingTargetList* InTargetList );
	~FBVPVehiclePathVisualization();

	FORCEINLINE UBVPSubsystem* GetSubsystem() const { return OwnerSubsystem; }
	FORCEINLINE AFGDrivingTargetList* GetTargetList() const { return TargetPointList; }
	FORCEINLINE const TArray<FBVPVehiclePathSegmentVisualization*>& GetVisualizationSegments() const { return VisualizationSegments; }

	AActor* GetVisualizationActor();
	UMaterialInterface* GetOrCreateMaterialInstance();

	// Attempts to find a visualization segment by index. First segment is 0->1, second 1->2 and so on. The last one is Num->0
	FBVPVehiclePathSegmentVisualization* FindSegmentByIndex( int32 SegmentIndex ) const;

	// Attempts to find a path visualization segment that starts at the provided node
	FBVPVehiclePathSegmentVisualization* FindSegmentByStartPathNode( const AFGTargetPoint* InPathNodeAfter ) const;

	// Returns true if this visualization is valid. If not, it should not be used and should be destroyed.
	bool IsVisualizationValid() const;
	
	void UpdateVisualization();
	void DestroyVisualization();
	
	void AddStructReferencedObjects( FReferenceCollector& Collector );
};

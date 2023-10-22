// Copyright Nikita Zolotukhin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class FBVPVehiclePathVisualization;
class USplineMeshComponent;
class UBoxComponent;
struct FBVPSegmentColliderInfo;

// Visualization of a single segment of the path spline
class BETTERVEHICLEPATHS_API FBVPVehiclePathSegmentVisualization
{
protected:
	FBVPVehiclePathVisualization* OwnerVisualization{};
	int32 SegmentIndex{INDEX_NONE};
	FVector ArriveLocation;
	FVector ArriveTangent;
	FVector LeaveLocation;
	FVector LeaveTangent;
	
	USplineMeshComponent* VisualizationComponent{};
	TArray<UBoxComponent*> CollisionComponents;

	int32 VisualizationRequestCounter{};
	int32 CollisionRequestCounter{};
	bool bNeedsVisualizationRebuild{};
	bool bNeedsCollisionRebuild{};
public:
	FBVPVehiclePathSegmentVisualization( FBVPVehiclePathVisualization* InOwner, int32 InSegmentIndex );
	
	void AddRemoveVisualizationRequest( bool bRemove );
	void AddRemoveCollisionRequest( bool bRemove );

	// Updates the segment with the new spline data. The path must have a valid path spline built!
	void UpdateSegmentWithNewSpline();
	
	bool IsSegmentUpToDate() const;
	bool IsSegmentRelevantForObserver( const FVector& ObserverLocation, double RelevanceDistance ) const;
	
	void UpdateSegment();
	void DestroySegment();
	
	void AddReferencedObjects( FReferenceCollector& ReferenceCollector );
private:
	void GetSplinePointsForSegment( int32& OutStartPoint, int32& OutEndPoint ) const;
	
	void ForceUpdateVisualization();
	void ForceUpdateCollision();

	void RebuildCollidersForSpline( TArray<FBVPSegmentColliderInfo>& ColliderList ) const;
};

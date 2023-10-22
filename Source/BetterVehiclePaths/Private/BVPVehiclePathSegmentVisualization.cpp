// Copyright Nikita Zolotukhin. All Rights Reserved.

#include "BVPVehiclePathSegmentVisualization.h"

#include "BVPPlayerVisualizationTracker.h"
#include "BVPSettings.h"
#include "BVPSubsystem.h"
#include "BVPVehiclePathVisualization.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/World.h"
#include "WheeledVehicles/FGTargetPointLinkedList.h"

struct FBVPSegmentColliderInfo
{
	FVector ColliderLocation;
	FVector ColliderDirection;
	float ColliderLength{};
	bool bNeedsUpdate{false};
};

FBVPVehiclePathSegmentVisualization::FBVPVehiclePathSegmentVisualization( FBVPVehiclePathVisualization* InOwner, int32 InSegmentIndex ) : OwnerVisualization( InOwner ), SegmentIndex( InSegmentIndex )
{
}

void FBVPVehiclePathSegmentVisualization::AddRemoveVisualizationRequest( bool bRemove )
{
	const bool bWantedVisualizationBefore = VisualizationRequestCounter != 0;
	VisualizationRequestCounter += ( bRemove ? -1 : 1 );
	
	if ( ( VisualizationRequestCounter != 0 ) != bWantedVisualizationBefore )
	{
		bNeedsVisualizationRebuild = true;
	}
}

void FBVPVehiclePathSegmentVisualization::AddRemoveCollisionRequest( bool bRemove )
{
	const bool bWantedCollisionBefore = CollisionRequestCounter != 0;
	CollisionRequestCounter += ( bRemove ? -1 : 1 );

	if ( ( CollisionRequestCounter != 0 ) != bWantedCollisionBefore )
	{
		bNeedsCollisionRebuild = true;
	}
}

void FBVPVehiclePathSegmentVisualization::UpdateSegmentWithNewSpline()
{
	const USplineComponent* SplineComponent = OwnerVisualization->GetTargetList()->GetPath();
	fgcheck( SplineComponent );
	
	int32 StartSplinePoint = INDEX_NONE, EndSplineSplinePoint = INDEX_NONE;
	GetSplinePointsForSegment( StartSplinePoint, EndSplineSplinePoint );

	const FVector NewArriveLocation = SplineComponent->GetLocationAtSplinePoint( StartSplinePoint, ESplineCoordinateSpace::World );
	const FVector NewArriveTangent = SplineComponent->GetLeaveTangentAtSplinePoint( StartSplinePoint, ESplineCoordinateSpace::World );

	const FVector NewLeaveLocation = SplineComponent->GetLocationAtSplinePoint( EndSplineSplinePoint, ESplineCoordinateSpace::World );
	const FVector NewLeaveTangent = SplineComponent->GetArriveTangentAtSplinePoint( EndSplineSplinePoint, ESplineCoordinateSpace::World );

	constexpr float LocationTolerance = 1.0f;
	constexpr float TangentTolerance = 0.1f;

	if ( !ArriveLocation.Equals( NewArriveLocation, LocationTolerance ) || !LeaveLocation.Equals( NewLeaveLocation, LocationTolerance ) ||
		!ArriveTangent.Equals( NewArriveTangent, TangentTolerance ) || !LeaveTangent.Equals( NewLeaveTangent, TangentTolerance ) )
	{
		ArriveLocation = NewArriveLocation;
		ArriveTangent = NewArriveTangent;

		LeaveLocation = NewLeaveLocation;
		LeaveTangent = NewLeaveTangent;

		bNeedsVisualizationRebuild |= VisualizationRequestCounter != 0;
		bNeedsCollisionRebuild |= CollisionRequestCounter != 0;
	}
}

bool FBVPVehiclePathSegmentVisualization::IsSegmentUpToDate() const
{
	return !bNeedsVisualizationRebuild && !bNeedsCollisionRebuild;
}

bool FBVPVehiclePathSegmentVisualization::IsSegmentRelevantForObserver( const FVector& ObserverLocation, double RelevanceDistance ) const
{
	return FVector::Distance( ArriveLocation, ObserverLocation ) <= RelevanceDistance ||
		FVector::Distance( LeaveLocation, ObserverLocation ) <= RelevanceDistance;
}

void FBVPVehiclePathSegmentVisualization::UpdateSegment()
{
	if ( OwnerVisualization->GetTargetList()->GetPath() )
	{
		UpdateSegmentWithNewSpline();
		
		if ( bNeedsVisualizationRebuild )
		{
			ForceUpdateVisualization();
		}
		if ( bNeedsCollisionRebuild )
		{
			ForceUpdateCollision();
		}
	}
}

void FBVPVehiclePathSegmentVisualization::DestroySegment()
{
	if ( VisualizationComponent )
	{
		VisualizationComponent->DestroyComponent();
		VisualizationComponent = nullptr;
		bNeedsVisualizationRebuild = true;
	}
	
	if ( !CollisionComponents.IsEmpty() )
	{
		for ( UBoxComponent* BoxComponent : CollisionComponents )
		{
			fgcheck( BoxComponent );
			BoxComponent->DestroyComponent();
		}
		CollisionComponents.Empty();
		bNeedsCollisionRebuild = true;
	}

	if ( OwnerVisualization != nullptr )
	{
		if ( const UBVPSubsystem* OwnerSubsystem = OwnerVisualization->GetSubsystem() )
		{
			for ( FBVPPlayerVisualizationTracker* VisualizationTracker : OwnerSubsystem->GetAllPlayerTrackers() )
			{
				fgcheck( VisualizationTracker );
				VisualizationTracker->ClearSegmentVisualization( this );
			}
		}
	}
}

void FBVPVehiclePathSegmentVisualization::AddReferencedObjects( FReferenceCollector& ReferenceCollector )
{
	ReferenceCollector.AddReferencedObject( VisualizationComponent );
	ReferenceCollector.AddReferencedObjects( CollisionComponents );
}

void FBVPVehiclePathSegmentVisualization::ForceUpdateVisualization()
{
	const bool bWantsVisualization = VisualizationRequestCounter != 0;
	if ( !VisualizationComponent && bWantsVisualization )
	{
		fgcheck( OwnerVisualization->GetTargetList() );

		VisualizationComponent = NewObject<USplineMeshComponent>( OwnerVisualization->GetVisualizationActor(), NAME_None, RF_Transient );
		VisualizationComponent->SetupAttachment( OwnerVisualization->GetVisualizationActor()->GetRootComponent() );
		VisualizationComponent->SetMobility( EComponentMobility::Movable );

		VisualizationComponent->SetCollisionEnabled( ECollisionEnabled::NoCollision );
		VisualizationComponent->SetStaticMesh( OwnerVisualization->GetSubsystem()->PathVisualizationMesh );
		VisualizationComponent->SetMaterial( 0, OwnerVisualization->GetOrCreateMaterialInstance() );

		VisualizationComponent->SetStartAndEnd( ArriveLocation, ArriveTangent, LeaveLocation, LeaveTangent );
		VisualizationComponent->RegisterComponent();
	}

	if ( VisualizationComponent )
	{
		VisualizationComponent->SetStartAndEnd( ArriveLocation, ArriveTangent, LeaveLocation, LeaveTangent );
		VisualizationComponent->SetVisibility( bWantsVisualization );
	}
	bNeedsVisualizationRebuild = false;
}

void FBVPVehiclePathSegmentVisualization::ForceUpdateCollision()
{
	const UBVPSettings* BVPSettings = UBVPSettings::Get();
	const float CollisionThickness = BVPSettings->PathVisualizationCollisionThickness;

	// First fetch info from the already spawned colliders
	TArray<FBVPSegmentColliderInfo> SegmentColliders;
	for ( const UBoxComponent* BoxComponent : CollisionComponents )
	{
		fgcheck( BoxComponent );
		FBVPSegmentColliderInfo& NewCollider = SegmentColliders.AddDefaulted_GetRef();

		NewCollider.ColliderLocation = BoxComponent->GetComponentLocation();
		NewCollider.ColliderDirection = BoxComponent->GetComponentRotation().Vector();
		NewCollider.ColliderLength = BoxComponent->GetScaledBoxExtent().X * 2.0f;
	}

	// Only build the collision segments if we actually want them
	if ( CollisionRequestCounter != 0 )
	{
		RebuildCollidersForSpline( SegmentColliders );
	}
	else
	{
		SegmentColliders.Empty();
	}
	
	// Spawn new colliders (or remove existing ones)
	if ( SegmentColliders.Num() != CollisionComponents.Num() )
	{
		// Remove extra colliders that we do not need
		for ( int32 i = CollisionComponents.Num() - 1; i >= SegmentColliders.Num(); i-- )
		{
			fgcheck( CollisionComponents[i] );
			CollisionComponents[i]->DestroyComponent();

			CollisionComponents.RemoveAt( i );
		}
		
		// Spawn extra colliders
		for ( int32 i = CollisionComponents.Num(); i < SegmentColliders.Num(); i++ )
		{
			UBoxComponent* BoxComponent = NewObject<UBoxComponent>( OwnerVisualization->GetVisualizationActor(), NAME_None, RF_Transient );
			BoxComponent->SetupAttachment( OwnerVisualization->GetVisualizationActor()->GetRootComponent() );
			BoxComponent->SetMobility( EComponentMobility::Movable );
			
			BoxComponent->SetWorldLocationAndRotation( SegmentColliders[i].ColliderLocation, SegmentColliders[i].ColliderDirection.Rotation() );
			BoxComponent->SetBoxExtent( FVector( SegmentColliders[i].ColliderLength / 2.0f, CollisionThickness, CollisionThickness ) );
			SegmentColliders[i].bNeedsUpdate = false;

			// TC_Interact but it is not exported
			FCollisionResponseContainer CollisionResponseContainer( ECR_Ignore );
			CollisionResponseContainer.SetResponse( ECC_GameTraceChannel13, ECR_Overlap );
				
			BoxComponent->SetCollisionResponseToChannels( CollisionResponseContainer );
			BoxComponent->RegisterComponent();

			CollisionComponents.Add( BoxComponent );
		}
	}

	// Update existing colliders
	for ( int32 i = 0; i < SegmentColliders.Num(); i++ )
	{
		UBoxComponent* BoxComponent = CollisionComponents[i];
		fgcheck( BoxComponent );

		const FBVPSegmentColliderInfo& ColliderInfo = SegmentColliders[i];
		if ( ColliderInfo.bNeedsUpdate )
		{
			BoxComponent->SetWorldLocationAndRotation( ColliderInfo.ColliderLocation, ColliderInfo.ColliderDirection.Rotation() );
			const FVector BoxExtent = BoxComponent->GetUnscaledBoxExtent();
			BoxComponent->SetBoxExtent( FVector( ColliderInfo.ColliderLength / 2.0f, BoxExtent.Y, BoxExtent.Z ) );
		}
	}
	bNeedsCollisionRebuild = false;
}

void FBVPVehiclePathSegmentVisualization::RebuildCollidersForSpline( TArray<FBVPSegmentColliderInfo>& ColliderList ) const
{
	const UBVPSettings* BVPSettings = UBVPSettings::Get();
	const float CollisionStep = BVPSettings->PathVisualizationCollisionStep;
	const float MaxAngleDiff = BVPSettings->PathVisualizationCollisionMaximumAngleDifference;

	USplineComponent* SplineComponent = OwnerVisualization->GetTargetList()->GetPath();
	fgcheck( SplineComponent );
	
	int32 StartSplinePoint = INDEX_NONE, EndSplinePoint = INDEX_NONE;
	GetSplinePointsForSegment( StartSplinePoint, EndSplinePoint );

	SplineComponent->UpdateSpline();

	const float StartProgress = SplineComponent->SplineCurves.Position.Points[ StartSplinePoint ].InVal;
	const float EndProgress = SplineComponent->SplineCurves.Position.Points[ EndSplinePoint ].InVal;
	const float SegmentLengthDistance = FMath::Abs( SplineComponent->GetDistanceAlongSplineAtSplineInputKey( EndProgress ) - SplineComponent->GetDistanceAlongSplineAtSplineInputKey( StartProgress ) );
	
	float CurrentProgressAlongSpline = StartProgress;
	FVector CurrentDirectionAlongSpline = SplineComponent->GetDirectionAtSplineInputKey( CurrentProgressAlongSpline, ESplineCoordinateSpace::World );
	FVector CurrentLocationAlongSpline = SplineComponent->GetLocationAtSplineInputKey( CurrentProgressAlongSpline, ESplineCoordinateSpace::World );
	int32 CurrentSegmentIndex = 0;

	constexpr float LocationTolerance = 1.0f;
	constexpr float DirectionTolerance = 0.01f;
	constexpr float LengthTolerance = 1.0f;

	const float ProgressIncrementMax = ( EndProgress - StartProgress ) / ( SegmentLengthDistance / CollisionStep );
	while ( CurrentProgressAlongSpline < EndProgress )
	{
		const float ProgressIncrement = FMath::Min( ProgressIncrementMax, EndProgress - CurrentProgressAlongSpline );
		const FVector NextDistanceAlongSpline = SplineComponent->GetDirectionAtSplineInputKey( CurrentProgressAlongSpline + ProgressIncrement, ESplineCoordinateSpace::World );
		const FVector NextLocationAlongSpline = SplineComponent->GetLocationAtSplineInputKey( CurrentProgressAlongSpline + ProgressIncrement, ESplineCoordinateSpace::World );

		const float AngleDifFromPrev = FMath::RadiansToDegrees( FMath::Acos( FVector::DotProduct( CurrentDirectionAlongSpline, NextDistanceAlongSpline ) ) );
		if ( AngleDifFromPrev >= MaxAngleDiff || CurrentProgressAlongSpline + ProgressIncrement >= EndProgress )
		{
			FBVPSegmentColliderInfo& ColliderInfo = ColliderList.IsValidIndex( CurrentSegmentIndex ) ? ColliderList[CurrentSegmentIndex] :
				ColliderList.AddDefaulted_GetRef();

			const FVector NewColliderLocation = ( CurrentLocationAlongSpline + NextLocationAlongSpline ) * 0.5f;
			const FVector NewColliderDirection = ( NextLocationAlongSpline - CurrentLocationAlongSpline ).GetSafeNormal();
			const float NewColliderLength = FVector::Distance( CurrentLocationAlongSpline, NextLocationAlongSpline );
			
			if ( !ColliderInfo.ColliderLocation.Equals( NewColliderLocation, LocationTolerance ) || !ColliderInfo.ColliderDirection.Equals( NewColliderDirection, DirectionTolerance ) ||
				FMath::Abs( ColliderInfo.ColliderLength - NewColliderLength ) >= LengthTolerance )
			{
				ColliderInfo.ColliderLocation = NewColliderLocation;
				ColliderInfo.ColliderDirection = NewColliderDirection;
				ColliderInfo.ColliderLength = NewColliderLength;
				ColliderInfo.bNeedsUpdate = true;
			}

			CurrentDirectionAlongSpline = NextDistanceAlongSpline;
			CurrentLocationAlongSpline = NextLocationAlongSpline;
			CurrentSegmentIndex++;
		}
		CurrentProgressAlongSpline += ProgressIncrement;
	}
	ColliderList.RemoveAt( CurrentSegmentIndex, ColliderList.Num() - CurrentSegmentIndex );
}

void FBVPVehiclePathSegmentVisualization::GetSplinePointsForSegment( int32& OutStartPoint, int32& OutEndPoint ) const
{
	const USplineComponent* SplineComponent = OwnerVisualization->GetTargetList()->GetPath();
	fgcheck( SplineComponent );
	
	OutStartPoint = UBVPSubsystem::NumBacktrackSplinePoints + SegmentIndex;
	OutEndPoint = ( OutStartPoint + 1 < SplineComponent->GetNumberOfSplinePoints() ) ? OutStartPoint + 1 : UBVPSubsystem::NumBacktrackSplinePoints;
}


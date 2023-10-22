// Copyright Nikita Zolotukhin. All Rights Reserved.

#include "BVPPlayerVisualizationTracker.h"
#include "BVPSettings.h"
#include "BVPSubsystem.h"
#include "BVPVehiclePathSegmentVisualization.h"
#include "BVPVehiclePathVisualization.h"
#include "FGCharacterPlayer.h"

FBVPPlayerVisualizationTracker::FBVPPlayerVisualizationTracker( UBVPSubsystem* InSubsystem, APlayerController* InPlayer ) : OwnerSubsystem( InSubsystem ), OwnerPlayer( InPlayer )
{
	fgcheck( OwnerSubsystem );
	fgcheck( OwnerPlayer );
}

void FBVPPlayerVisualizationTracker::SetVisualizationBit( FName VisualizationId, EBVPPathVisualizationType VisualizationBit, bool bVisualizationEnabled )
{
	EBVPPathVisualizationType& ActiveVisualizationBitmask = EnabledVisualizationBits.FindOrAdd( VisualizationId );

	if ( bVisualizationEnabled )
	{
		ActiveVisualizationBitmask |= VisualizationBit;
	}
	else
	{
		ActiveVisualizationBitmask &= ~VisualizationBit;
	}
	if ( ActiveVisualizationBitmask == EBVPPathVisualizationType::None )
	{
		EnabledVisualizationBits.Remove( VisualizationId );
	}
}

bool FBVPPlayerVisualizationTracker::GetVisualizationBit( FName VisualizationId, EBVPPathVisualizationType VisualizationBit ) const
{
	if ( const EBVPPathVisualizationType* ActiveVisualizationBitmask = EnabledVisualizationBits.Find( VisualizationId ) )
	{
		return ( *ActiveVisualizationBitmask & VisualizationBit ) != EBVPPathVisualizationType::None;
	}
	return false;
}

bool FBVPPlayerVisualizationTracker::IsVisualizationTrackerValid() const
{
	return OwnerSubsystem && OwnerPlayer && OwnerPlayer->IsLocalController();
}

bool FBVPPlayerVisualizationTracker::IsVisualizationTrackerEmpty() const
{
	return EnabledVisualizationBits.IsEmpty();
}

void FBVPPlayerVisualizationTracker::ClearSegmentVisualization( const FBVPVehiclePathSegmentVisualization* SegmentVisualization )
{
	VisualizationSegmentToVisualizationBitMap.Remove( SegmentVisualization );
}

void FBVPPlayerVisualizationTracker::DestroyVisualizationTracker()
{
	if ( OwnerSubsystem )
	{
		const TMap<FBVPVehiclePathSegmentVisualization*, EBVPPathVisualizationType> EmptyVisualizationBits;

		TArray<FBVPVehiclePathSegmentVisualization*> AllPathVisualizationSegments;
		CollectAllVisualizationPaths( AllPathVisualizationSegments );
		
		ApplyVisualizationBits( AllPathVisualizationSegments, EmptyVisualizationBits );
	}
}

void FBVPPlayerVisualizationTracker::CollectAllVisualizationPaths( TArray<FBVPVehiclePathSegmentVisualization*>& OutAllVisualizations )
{
	OutAllVisualizations.Reserve( LastVisualizationSegmentArraySize );
	
	for ( const FBVPVehiclePathVisualization* Visualization : OwnerSubsystem->GetAllVisualizedPaths() )
	{
		OutAllVisualizations.Append( Visualization->GetVisualizationSegments() );
	}
	LastVisualizationSegmentArraySize = OutAllVisualizations.Num();
}

EBVPPathVisualizationType FBVPPlayerVisualizationTracker::GetCombinedVisualizationFlags() const
{
	EBVPPathVisualizationType CombinedWantedVisualizationBits = EBVPPathVisualizationType::None;
	for ( const TPair<FName, EBVPPathVisualizationType>& Pair : EnabledVisualizationBits )
	{
		CombinedWantedVisualizationBits |= Pair.Value;
	}
	return CombinedWantedVisualizationBits;
}

void FBVPPlayerVisualizationTracker::UpdateVisualizationTracker()
{
	fgcheck( OwnerSubsystem );
	const EBVPPathVisualizationType CombinedWantedVisualizationBits = GetCombinedVisualizationFlags();

	TArray<FBVPVehiclePathSegmentVisualization*> AllPathVisualizationSegments;
	CollectAllVisualizationPaths( AllPathVisualizationSegments );

	// Collect visualization distances for all supported visualization types
	const UBVPSettings* BVPSettings = UBVPSettings::Get();
	float VisualizationTypeViewDistances[UE_ARRAY_COUNT( AllVisualizationTypes )];

	for ( int32 i = 0; i < UE_ARRAY_COUNT( AllVisualizationTypes ); i++ )
	{
		const EBVPPathVisualizationType VisualizationType = AllVisualizationTypes[i];
		VisualizationTypeViewDistances[i] = BVPSettings->MaxPathVisualizationDistance.Contains( VisualizationType ) ?
			BVPSettings->MaxPathVisualizationDistance.FindChecked( VisualizationType ) : UE_BIG_NUMBER;
	}
	
	// Generate visualizations for each spline segment
	TMap<FBVPVehiclePathSegmentVisualization*, EBVPPathVisualizationType> NewPerSegmentVisualizationBits;

	if ( IsVisualizationTrackerValid() )
	{
		FVector ObserverLocation;
		FRotator ObserverRotation;
		OwnerPlayer->GetPlayerViewPoint( ObserverLocation, ObserverRotation );
		
		for ( FBVPVehiclePathSegmentVisualization* VisualizationSegment : AllPathVisualizationSegments )
		{
			for ( int32 i = 0; i < UE_ARRAY_COUNT( AllVisualizationTypes ); i++ )
			{
				const EBVPPathVisualizationType VisualizationType = AllVisualizationTypes[i];
				const float VisualizationDistance = VisualizationTypeViewDistances[i];
				
				if ( EnumHasAnyFlags( CombinedWantedVisualizationBits, VisualizationType ) && VisualizationSegment->IsSegmentRelevantForObserver( ObserverLocation, VisualizationDistance ) )
				{
					NewPerSegmentVisualizationBits.FindOrAdd( VisualizationSegment ) |= VisualizationType;
				}
			}
		}
	}

	// Apply visualizations to the resulting segments
	ApplyVisualizationBits( AllPathVisualizationSegments, NewPerSegmentVisualizationBits );
}

void FBVPPlayerVisualizationTracker::ApplyVisualizationBits( const TArray<FBVPVehiclePathSegmentVisualization*>& AllVisualizations, const TMap<FBVPVehiclePathSegmentVisualization*, EBVPPathVisualizationType>& NewVisualizationBits )
{
	for ( FBVPVehiclePathSegmentVisualization* Visualization : AllVisualizations )
	{
		EBVPPathVisualizationType& CurrentVisualization = VisualizationSegmentToVisualizationBitMap.FindOrAdd( Visualization );
		const EBVPPathVisualizationType DesiredVisualization = NewVisualizationBits.Contains( Visualization ) ? NewVisualizationBits.FindChecked( Visualization ) : EBVPPathVisualizationType::None;

		if ( CurrentVisualization != DesiredVisualization )
		{
			for ( const EBVPPathVisualizationType VisualizationType : AllVisualizationTypes )
			{
				const bool bCurrentVisualizationValue = EnumHasAnyFlags( CurrentVisualization, VisualizationType );
				const bool bNewVisualizationValue = EnumHasAnyFlags( DesiredVisualization, VisualizationType );

				if ( bCurrentVisualizationValue != bNewVisualizationValue )
				{
					SetVisualizationTypeEnabledForSegment( Visualization, VisualizationType, bNewVisualizationValue );
				}
			}
			CurrentVisualization = DesiredVisualization;
		}
	}
}

void FBVPPlayerVisualizationTracker::SetVisualizationTypeEnabledForSegment( FBVPVehiclePathSegmentVisualization* SegmentVisualization, EBVPPathVisualizationType VisualizationType, bool bEnabled )
{
	if ( EnumHasAnyFlags( VisualizationType, EBVPPathVisualizationType::SegmentVisualization ) )
	{
		SegmentVisualization->AddRemoveVisualizationRequest( !bEnabled );
	}
	if ( EnumHasAnyFlags( VisualizationType, EBVPPathVisualizationType::SegmentCollision ) )
	{
		SegmentVisualization->AddRemoveCollisionRequest( !bEnabled );
	}
}

void FBVPPlayerVisualizationTracker::AddReferencedObjects( FReferenceCollector& ReferenceCollector )
{
	ReferenceCollector.AddReferencedObject( OwnerSubsystem );
	ReferenceCollector.AddReferencedObject( OwnerPlayer );
}

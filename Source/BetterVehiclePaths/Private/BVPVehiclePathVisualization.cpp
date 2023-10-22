// Copyright Nikita Zolotukhin. All Rights Reserved.

#include "BVPVehiclePathVisualization.h"

#include "BVPPathVisualizationActor.h"
#include "BVPSettings.h"
#include "BVPSubsystem.h"
#include "BVPVehiclePathSegmentVisualization.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WheeledVehicles/FGTargetPoint.h"
#include "WheeledVehicles/FGTargetPointLinkedList.h"

FBVPVehiclePathVisualization::FBVPVehiclePathVisualization( UBVPSubsystem* InSubsystem, AFGDrivingTargetList* InTargetList ) : OwnerSubsystem( InSubsystem ), TargetPointList( InTargetList )
{
	fgcheck( OwnerSubsystem );
	fgcheck( TargetPointList );
}

FBVPVehiclePathVisualization::~FBVPVehiclePathVisualization()
{
	for ( const FBVPVehiclePathSegmentVisualization* SegmentVisualization : VisualizationSegments )
	{
		delete SegmentVisualization;
	}
	VisualizationSegments.Empty();
}

AActor* FBVPVehiclePathVisualization::GetVisualizationActor()
{
	if ( !VisualizationActor )
	{
		FActorSpawnParameters SpawnParameters{};
		SpawnParameters.Name = *FString::Printf( TEXT("VisualizationActor_%s"), *TargetPointList->GetName() );
		SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		SpawnParameters.Owner = TargetPointList;
		
		ABVPPathVisualizationActor* SpawnedActor = OwnerSubsystem->GetWorld()->SpawnActor<ABVPPathVisualizationActor>( SpawnParameters );
		SpawnedActor->OwnerTargetList = TargetPointList;
		VisualizationActor = SpawnedActor;
	}
	return VisualizationActor;
}

UMaterialInterface* FBVPVehiclePathVisualization::GetOrCreateMaterialInstance()
{
	if ( !MaterialInstance && OwnerSubsystem )
	{
		const UBVPSettings* BVPSettings = UBVPSettings::Get();
		MaterialInstance = UMaterialInstanceDynamic::Create( OwnerSubsystem->PathVisualizationMaterial, OwnerSubsystem );

		// Use the hash of the target point name here as it gives results consistent between game restarts
		const FRandomStream RandomStream( (int32) GetTypeHash( TargetPointList->GetName() ) );

		// Only randomize hue to avoid randomized colors appearing more bleak
		const FLinearColor LinearColor = FLinearColor( RandomStream.GetFraction(), 0.9f, 0.5f ).HSVToLinearRGB();
		MaterialInstance->SetVectorParameterValue( BVPSettings->PathVisualizationColorParameterName, LinearColor );
	}
	return MaterialInstance;
}

FBVPVehiclePathSegmentVisualization* FBVPVehiclePathVisualization::FindSegmentByIndex( int32 SegmentIndex ) const
{
	return VisualizationSegments.IsValidIndex( SegmentIndex ) ? VisualizationSegments[SegmentIndex] : nullptr;
}

FBVPVehiclePathSegmentVisualization* FBVPVehiclePathVisualization::FindSegmentByStartPathNode( const AFGTargetPoint* InPathNodeAfter ) const
{
	if ( TargetPointList )
	{
		int32 CurrentNodeIndex = 0;
		const AFGTargetPoint* CurrentNode = TargetPointList->GetFirstTarget();

		while ( CurrentNode != nullptr )
		{
			if ( CurrentNode == InPathNodeAfter )
			{
				return FindSegmentByIndex( CurrentNodeIndex );
			}
			CurrentNode = CurrentNode->GetNext();
			CurrentNodeIndex++;
		}
	}
	return nullptr;
}

bool FBVPVehiclePathVisualization::IsVisualizationValid() const
{
	return TargetPointList != nullptr && OwnerSubsystem != nullptr;
}

void FBVPVehiclePathVisualization::UpdateVisualization()
{
	if ( TargetPointList )
	{
		// Attempt to fetch the spline component data
		if ( TargetPointList->GetPath() == nullptr && TargetPointList->IsComplete() )
		{
			// Update target count if we have the authority but have no data
			if ( !TargetPointList->HasData() && TargetPointList->HasAuthority() )
			{
				TargetPointList->CalculateTargetCount();
			}

			// Build the spline component path if we have the data now
			if ( TargetPointList->HasData() )
			{
				TargetPointList->CreatePath();
			}
		}

		// Build the segments if we have a valid spline component
		if ( TargetPointList->GetPath() != nullptr )
		{
			// Spawn or remove segments as needed
			if ( TargetPointList->GetTargetCount() != VisualizationSegments.Num() )
			{
				// Remove additional segments that are not needed
				for ( int32 i = VisualizationSegments.Num() - 1; i >= TargetPointList->GetTargetCount(); i-- )
				{
					FBVPVehiclePathSegmentVisualization* SegmentToDelete = VisualizationSegments[i];
					fgcheck( SegmentToDelete );
					
					SegmentToDelete->DestroySegment();
					delete SegmentToDelete;
					
					VisualizationSegments.RemoveAt( i );
				}

				// Spawn new segments for additional target points
				for ( int32 i = VisualizationSegments.Num(); i < TargetPointList->GetTargetCount(); i++ )
				{
					FBVPVehiclePathSegmentVisualization* NewSegment = new FBVPVehiclePathSegmentVisualization( this, i );
					VisualizationSegments.Add( NewSegment );
				}
			}

			// Update segments with the new spline
			for ( FBVPVehiclePathSegmentVisualization* SegmentVisualization : VisualizationSegments )
			{
				fgcheck( SegmentVisualization );

				SegmentVisualization->UpdateSegmentWithNewSpline();
				if ( !SegmentVisualization->IsSegmentUpToDate() )
				{
					SegmentVisualization->UpdateSegment();
				}
			}
		}
		// Otherwise, kill the existing segments
		else if ( !VisualizationSegments.IsEmpty() )
		{
			DestroyVisualization();
		}
	}
	else if ( !VisualizationSegments.IsEmpty() )
	{
		DestroyVisualization();
	}
}

void FBVPVehiclePathVisualization::DestroyVisualization()
{
	for ( FBVPVehiclePathSegmentVisualization* SegmentVisualization : VisualizationSegments )
	{
		fgcheck( SegmentVisualization );
		SegmentVisualization->DestroySegment();
	}
	VisualizationSegments.Empty();

	if ( VisualizationActor != nullptr )
	{
		VisualizationActor->Destroy();
		VisualizationActor = nullptr;
	}
}

void FBVPVehiclePathVisualization::AddStructReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( OwnerSubsystem );
	Collector.AddReferencedObject( TargetPointList );

	for ( FBVPVehiclePathSegmentVisualization* SegmentVisualization : VisualizationSegments )
	{
		fgcheck( SegmentVisualization );
		SegmentVisualization->AddReferencedObjects( Collector );
	}
}

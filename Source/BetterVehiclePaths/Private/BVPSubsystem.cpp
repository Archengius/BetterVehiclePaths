// Copyright Nikita Zolotukhin. All Rights Reserved.

#include "BVPSubsystem.h"
#include "BVPPathVisualizationActor.h"
#include "BVPPlayerVisualizationTracker.h"
#include "BVPSettings.h"
#include "BVPVehiclePathVisualization.h"
#include "EnhancedInputComponent.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "FGSplineMeshGenerationLibrary.h"
#include "FGVehicleSubsystem.h"
#include "InstancedSplineMeshComponent.h"
#include "WheeledVehicles/FGTargetPoint.h"
#include "WheeledVehicles/FGTargetPointLinkedList.h"
#include "FGGameMode.h"
#include "Net/UnrealNetwork.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "BetterVehiclePaths"

class AFGDrivingTargetList;
DECLARE_CYCLE_STAT( TEXT( "Better Vehicles Subsystem" ), STAT_BVPSubsystem, STATGROUP_Game );

UBVPSubsystem::UBVPSubsystem()
{
}

UBVPSubsystem::~UBVPSubsystem()
{
	for ( const FBVPVehiclePathVisualization* PathVisualization : VisualizedPaths )
	{
		delete PathVisualization;
	}
	VisualizedPaths.Empty();

	for ( const FBVPPlayerVisualizationTracker* VisualizationTracker : VisualizationTrackers )
	{
		delete VisualizationTracker;
	}
	VisualizationTrackers.Empty();
}

bool UBVPSubsystem::DoesSupportWorldType( const EWorldType::Type WorldType ) const
{
	return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UBVPSubsystem::Initialize( FSubsystemCollectionBase& Collection )
{
	Super::Initialize( Collection );

	const UBVPSettings* BVPSettings = UBVPSettings::Get();
	
	PathVisualizationMesh = BVPSettings->PathVisualizationMesh.LoadSynchronous();
	PathVisualizationMaterial = BVPSettings->PathVisualizationMaterial.LoadSynchronous();
	PathEditorWidget = BVPSettings->PathEditorWidget.LoadSynchronous();
	PathNodeClass = BVPSettings->PathNodeClass.LoadSynchronous();
	PathEditorSelectedMaterial = BVPSettings->PathEditorSelectedMaterial.LoadSynchronous();
}

void UBVPSubsystem::OnWorldBeginPlay( UWorld& InWorld )
{
	Super::OnWorldBeginPlay( InWorld );

	if ( AFGGameMode* GameMode = InWorld.GetAuthGameMode<AFGGameMode>() )
	{
		GameMode->RegisterRemoteCallObjectClass( UBVPRemoteCallObject::StaticClass() );
	}
}

void UBVPSubsystem::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );
	
	TickActiveVisualizations();
	TickVisualizationTrackers();
}

TStatId UBVPSubsystem::GetStatId() const
{
	return GET_STATID( STAT_BVPSubsystem );
}

AFGTargetPoint* UBVPSubsystem::TraceForTargetPoint( APlayerController* PlayerController, const FVector2D& ScreenPosition )
{
	// Trace for the point
	TArray<FHitResult> HitResults;
	if ( !TraceForPathNodeChannelInternal( PlayerController, ScreenPosition, HitResults ) )
	{
		return nullptr;
	}
	
	for ( const FHitResult& HitResult : HitResults )
	{
		if ( AFGTargetPoint* HitTargetPoint = Cast<AFGTargetPoint>( HitResult.GetActor() ) )
		{
			return HitTargetPoint;
		}
	}
	return nullptr;
}

bool UBVPSubsystem::TraceForSplineSegment( APlayerController* PlayerController, const FVector2D& ScreenPosition, FBVPVehiclePathSegmentHit& OutHitResult )
{
	// Trace for the point
	TArray<FHitResult> HitResults;
	if ( !TraceForPathNodeChannelInternal( PlayerController, ScreenPosition, HitResults ) )
	{
		return false;
	}

	for ( const FHitResult& HitResult : HitResults )
	{
		// Only attempt to resolve hit with our visualization meshes
		const ABVPPathVisualizationActor* PathVisualizationActor = Cast<ABVPPathVisualizationActor>( HitResult.GetActor() );
		if ( PathVisualizationActor && PathVisualizationActor->OwnerTargetList )
		{
			if ( USplineComponent* SplineComponent = PathVisualizationActor->OwnerTargetList->GetPath() )
			{
				OutHitResult.ProgressAlongSpline = SplineComponent->FindInputKeyClosestToWorldLocation( HitResult.ImpactPoint );
				OutHitResult.SplineComponent = SplineComponent;

				const int32 SplinePointAtInput = SplineComponent->SplineCurves.Position.GetPointIndexForInputValue( OutHitResult.ProgressAlongSpline );
				OutHitResult.PointAfter = GetTargetPointAtSplinePoint( PathVisualizationActor->OwnerTargetList, SplinePointAtInput, SplineComponent->GetNumberOfSplinePoints() );
				return OutHitResult.PointAfter != nullptr;
			}
		}
	}
	return false;
}

bool UBVPSubsystem::TraceForSolidSurface( APlayerController* PlayerController, const FVector2D& ScreenPosition, FHitResult& OutHitResul )
{
	// Attempt to obtain the hit from the screen space first
	FVector HitWorldPosition, HitWorldDirection;
	if ( !UGameplayStatics::DeprojectScreenToWorld( PlayerController, ScreenPosition, HitWorldPosition, HitWorldDirection ) )
	{
		return false;
	}

	// Trace for the spline mesh component. We use Interact channel because our splines collide on the interact channel only
	const float TraceDistance = GetTraceDistanceForPlayer( PlayerController );
	
	FCollisionQueryParams CollisionQueryParams{};
	CollisionQueryParams.AddIgnoredActor( PlayerController->GetPawn() );

	FCollisionObjectQueryParams ObjectQueryParams{};
	ObjectQueryParams.AddObjectTypesToQuery( ECC_WorldStatic );
	
	return GetWorld()->LineTraceSingleByObjectType( OutHitResul, HitWorldPosition, HitWorldPosition + HitWorldDirection * TraceDistance, ObjectQueryParams, CollisionQueryParams );
}

AFGTargetPoint* UBVPSubsystem::GetTargetPointAtSplinePoint( const AFGDrivingTargetList* TargetPointList, int32 SplinePointIndex, int32 NumSplinePoints )
{
	// Backtrack to the correct spline point index
	if ( SplinePointIndex < NumBacktrackSplinePoints )
	{
		SplinePointIndex = NumSplinePoints - NumBacktrackSplinePoints + SplinePointIndex;
	}
	SplinePointIndex -= NumBacktrackSplinePoints;
	AFGTargetPoint* CurrentTargetPoint = TargetPointList->mFirst;

	while ( SplinePointIndex > 0 && CurrentTargetPoint->GetNext() != nullptr )
	{
		CurrentTargetPoint = CurrentTargetPoint->GetNext();
		SplinePointIndex--;
	}
	return CurrentTargetPoint;
}

float UBVPSubsystem::GetTraceDistanceForPlayer( const APlayerController* PlayerController )
{
	const UBVPSettings* BVPSettings = UBVPSettings::Get();
	float TraceDistance = BVPSettings->MinTraceDistanceForPathEditor;

	if ( AFGCharacterPlayer* CharacterPlayer = Cast<AFGCharacterPlayer>( PlayerController->GetPawn() ) )
	{
		TraceDistance = FMath::Max( CharacterPlayer->GetUseDistance(), TraceDistance );
	}
	return TraceDistance;
}

bool UBVPSubsystem::TraceForPathNodeChannelInternal( const APlayerController* PlayerController, const FVector2D& ScreenPosition, TArray<FHitResult>& OutHitResults ) const
{
	// Attempt to obtain the hit from the screen space first
	FVector HitWorldPosition, HitWorldDirection;
	if ( !UGameplayStatics::DeprojectScreenToWorld( PlayerController, ScreenPosition, HitWorldPosition, HitWorldDirection ) )
	{
		return false;
	}

	// Trace for the spline mesh component. We use Interact channel because our splines collide on the interact channel only
	const float TraceDistance = GetTraceDistanceForPlayer( PlayerController );
	
	FCollisionQueryParams CollisionQueryParams{};
	CollisionQueryParams.AddIgnoredActor( PlayerController->GetPawn() );

	// TC_Interact but it is not exported
	return GetWorld()->LineTraceMultiByChannel( OutHitResults, HitWorldPosition, HitWorldPosition + HitWorldDirection * TraceDistance, ECC_GameTraceChannel13, CollisionQueryParams );
}

bool UBVPSubsystem::CreateNewPathNode( AFGPlayerController* PlayerController, const FBVPVehiclePathSegmentHit& HitResult, FText& OutErrorMessage )
{
	if ( !HitResult.PointAfter || !HitResult.SplineComponent || !HitResult.PointAfter->GetOwningList() || !UBVPSubsystem::FindNextTargetPoint( HitResult.PointAfter ) )
	{
		return false;
	}

	const AFGTargetPoint* NextTargetPoint = UBVPSubsystem::FindNextTargetPoint( HitResult.PointAfter );
	const FVector WorldLocationAtHit = HitResult.SplineComponent->GetLocationAtSplineInputKey( HitResult.ProgressAlongSpline, ESplineCoordinateSpace::World );
	const FVector WorldDirectionAtHit = HitResult.SplineComponent->GetDirectionAtSplineInputKey( HitResult.ProgressAlongSpline, ESplineCoordinateSpace::World );

	const int32 ClosestPointAtProgress = HitResult.SplineComponent->SplineCurves.Position.GetPointIndexForInputValue( HitResult.ProgressAlongSpline );
	const float SegmentProgressStart = HitResult.SplineComponent->SplineCurves.Position.Points[ ClosestPointAtProgress ].InVal;
	const float SegmentProgressEnd = HitResult.SplineComponent->SplineCurves.Position.Points[ ClosestPointAtProgress + 1 ].InVal;
	
	const float InterpolatedSegmentProgress = ( HitResult.ProgressAlongSpline - SegmentProgressStart ) / ( SegmentProgressEnd - SegmentProgressStart );
	const int32 TargetSpeedAtPoint = FMath::Lerp( HitResult.PointAfter->GetTargetSpeed(), NextTargetPoint->GetTargetSpeed(), InterpolatedSegmentProgress );

	const FVector WorldLocationAtPointA = HitResult.PointAfter->GetActorLocation();
	const FVector WorldLocationAtPointB = NextTargetPoint->GetActorLocation();

	// Check the distance between the hit location and the adjacent points
	if ( !CheckDistanceBetweenTwoPoints( WorldLocationAtHit, WorldLocationAtPointA ) || !CheckDistanceBetweenTwoPoints( WorldLocationAtHit, WorldLocationAtPointB ) )
	{
		OutErrorMessage = LOCTEXT("CreateNewPathNode_TooFar", "Cannot create Path Node as it's Too Far from either of it's adjacent Path Nodes.");
		return false;
	}

	// As a client, ask the server to do the memes for us
	if ( PlayerController && GetWorld()->IsNetMode( NM_Client ) )
	{
		if ( UBVPRemoteCallObject* RemoteCallObject = PlayerController->GetRemoteCallObjectOfClass<UBVPRemoteCallObject>() )
		{
			RemoteCallObject->Server_CreatePathNode( HitResult.PointAfter, WorldLocationAtHit, WorldDirectionAtHit.Rotation(), TargetSpeedAtPoint );
			return true;
		}
		return false;
	}

	if ( AFGTargetPoint* NewTargetPoint = CreatePawnPathNodeInternal( HitResult.PointAfter, WorldLocationAtHit, WorldDirectionAtHit.Rotation(), TargetSpeedAtPoint ) )
	{
		OnNewPathNodeCreated.Broadcast( NewTargetPoint, PlayerController );
		return true;
	}
	return false;
}

AFGTargetPoint* UBVPSubsystem::CreatePawnPathNodeInternal( AFGTargetPoint* AfterPoint, const FVector& NewLocation, const FRotator& NewRotation, int32 TargetSpeed ) const
{
	AFGDrivingTargetList* OwnerTargetList = AfterPoint->GetOwningList();
	fgcheck( OwnerTargetList );
	
	const FTransform Transform( NewRotation, NewLocation );
	AFGTargetPoint* NewTargetPoint = GetWorld()->SpawnActorDeferred<AFGTargetPoint>( PathNodeClass, Transform, OwnerTargetList, nullptr );
	fgcheck( NewTargetPoint );

	const AFGTargetPoint* NextPoint = FindNextTargetPoint( AfterPoint );
	fgcheck( NextPoint );
	
	const int32 MaxSpeed = FMath::Max( AfterPoint->GetTargetSpeed(), NextPoint->GetTargetSpeed() );
	NewTargetPoint->SetTargetSpeed( FMath::Clamp( TargetSpeed, 0, MaxSpeed ) );
	
	OwnerTargetList->InsertItem( NewTargetPoint, AfterPoint );
	OwnerTargetList->CalculateTargetCount();
	
	NewTargetPoint->FinishSpawning( Transform, false );
	
	if ( OwnerTargetList->HasData() && OwnerTargetList->IsComplete() )
	{
		OwnerTargetList->CreatePath();
	}
	return NewTargetPoint;
}

bool UBVPSubsystem::RemovePathNode( AFGPlayerController* PlayerController, AFGTargetPoint* TargetPoint, FText& OutErrorMessage )
{
	if ( !CheckCanRemovePathNode( TargetPoint, OutErrorMessage ) )
	{
		return false;
	}

	// We cannot remove target points on the client, so ask the server to do so
	if ( PlayerController && GetWorld()->IsNetMode( NM_Client ) )
	{
		if ( UBVPRemoteCallObject* RemoteCallObject = PlayerController->GetRemoteCallObjectOfClass<UBVPRemoteCallObject>() )
		{
			RemoteCallObject->Server_RemovePathNode( TargetPoint );
			return true;
		}
		return false;
	}

	if ( AFGDrivingTargetList* OwnerTargetList = TargetPoint->GetOwningList() )
	{
		OwnerTargetList->RemoveItem( TargetPoint );
		OwnerTargetList->CalculateTargetCount();
		
		if ( OwnerTargetList->HasData() && OwnerTargetList->IsComplete() )
		{
			OwnerTargetList->CreatePath();
		}
		return true;
	}
	return false;
}

bool UBVPSubsystem::SetPathNodeTargetSpeed( AFGPlayerController* PlayerController, AFGTargetPoint* TargetPoint, int32 NewTargetSpeed )
{
	// Redirect the request to server if we are a client
	if ( PlayerController && GetWorld()->IsNetMode( NM_Client ) )
	{
		if ( UBVPRemoteCallObject* RemoteCallObject = PlayerController->GetRemoteCallObjectOfClass<UBVPRemoteCallObject>() )
		{
			RemoteCallObject->Server_SetPathNodeTargetSpeed( TargetPoint, NewTargetSpeed );
			return true;
		}
		return false;
	}

	if ( TargetPoint && TargetPoint->GetOwningList() )
	{
		TargetPoint->SetTargetSpeed( FMath::Clamp( NewTargetSpeed, 0, 200 ) );
		return true;
	}
	return false;
}

bool UBVPSubsystem::MovePathNode( AFGPlayerController* PlayerController, AFGTargetPoint* TargetPoint, const FVector& NewLocation, const FRotator& NewRotation, bool bClientPrediction, FText& OutErrorMessage )
{
	// Check that we can actually move the node to that position first
	if ( !CheckCanMovePathNode( TargetPoint, NewLocation, NewRotation, OutErrorMessage ) )
	{
		return false;
	}
	
	TargetPoint->SetActorLocationAndRotation( NewLocation, NewRotation );
	TargetPoint->FlushNetDormancy();

	// Rebuild the path on the owner target list now
	if ( AFGDrivingTargetList* OwnerTargetList = TargetPoint->GetOwningList() )
	{
		if ( OwnerTargetList->IsComplete() && OwnerTargetList->HasData() )
		{
			OwnerTargetList->CreatePath();
		}
	}

	// If we are not the authority, notify the server. It will rollback our node position if we fuck up as well.
	if ( !bClientPrediction && PlayerController && GetWorld()->IsNetMode( NM_Client ) )
	{
		if ( UBVPRemoteCallObject* RemoteCallObject = PlayerController->GetRemoteCallObjectOfClass<UBVPRemoteCallObject>() )
		{
			RemoteCallObject->Server_EditPathNode( TargetPoint, TargetPoint->GetActorLocation(), TargetPoint->GetActorRotation() );
		}
	}
	return true;
}

bool UBVPSubsystem::CheckCanMovePathNode( const AFGTargetPoint* TargetPoint, const FVector& NewLocation, const FRotator& NewRotation, FText& OutErrorMessage )
{
	if ( !TargetPoint || !TargetPoint->GetOwningList() )
	{
		return false;
	}
	
	const AFGTargetPoint* PrevTargetPoint = FindPrevTargetPoint( TargetPoint );
	const AFGTargetPoint* NextTargetPoint = FindNextTargetPoint( TargetPoint );
	
	if ( !NextTargetPoint || !PrevTargetPoint || !CheckDistanceBetweenTwoPoints( NewLocation, PrevTargetPoint->GetActorLocation() ) || !CheckDistanceBetweenTwoPoints( NewLocation, NextTargetPoint->GetActorLocation() ) )
	{
		OutErrorMessage = LOCTEXT("MovePathNode_TooFar", "Cannot move Path Node as it would be Too Far from it's adjacent Path Nodes.");
		return false;
	}
	return true;
}

bool UBVPSubsystem::CheckCanRemovePathNode( const AFGTargetPoint* TargetPoint, FText& OutErrorMessage )
{
	if ( !TargetPoint || !TargetPoint->GetOwningList() )
	{
		return false;
	}
	const AFGTargetPoint* PrevTargetPoint = FindPrevTargetPoint( TargetPoint );
	const AFGTargetPoint* NextTargetPoint = FindNextTargetPoint( TargetPoint );
	
	if ( !PrevTargetPoint || !NextTargetPoint || !CheckDistanceBetweenTwoPoints( PrevTargetPoint->GetActorLocation(), NextTargetPoint->GetActorLocation() ) )
	{
		OutErrorMessage = LOCTEXT("RemovePathNode_TooFar", "Cannot remove Path Node as it's adjacent Path Nodes would end up Too Far from each other.");
		return false;
	}
	return true;
}

AFGTargetPoint* UBVPSubsystem::FindPrevTargetPoint( const AFGTargetPoint* TargetPoint )
{
	fgcheck( TargetPoint );

	const AFGDrivingTargetList* TargetList = TargetPoint->GetOwningList();
	fgcheck( TargetList );

	// Previous point of the first target is the last point ;)
	if ( TargetPoint == TargetList->GetFirstTarget() )
	{
		return TargetList->GetLastTarget();
	}

	// Otherwise walk the linked list
	AFGTargetPoint* CurrentTargetPoint = TargetList->GetFirstTarget();
	while ( CurrentTargetPoint != nullptr )
	{
		if ( CurrentTargetPoint->GetNext() == TargetPoint )
		{
			return CurrentTargetPoint;
		}
		CurrentTargetPoint = CurrentTargetPoint->GetNext();
	}
	return nullptr;
}

AFGTargetPoint* UBVPSubsystem::FindNextTargetPoint( const AFGTargetPoint* TargetPoint )
{
	fgcheck( TargetPoint );

	const AFGDrivingTargetList* TargetList = TargetPoint->GetOwningList();
	fgcheck( TargetList );

	// Next point of the last target is the first point ;)
	if ( TargetPoint == TargetList->GetLastTarget() )
	{
		return TargetList->GetFirstTarget();
	}
	return TargetPoint->GetNext();
}

bool UBVPSubsystem::CheckDistanceBetweenTwoPoints( const FVector& LocationA, const FVector& LocationB )
{
	const UBVPSettings* BVPSettings = UBVPSettings::Get();
	return FVector::Distance( LocationA, LocationB ) <= BVPSettings->MaxDistanceBetweenPathNodes;
}

void UBVPSubsystem::SetVisualizationRequesterState( APlayerController* Requester, FName VisualizationId, EBVPPathVisualizationType VisualizationType, bool bVisualizationEnabled )
{
	if ( FBVPPlayerVisualizationTracker* VisualizationTracker = FindVisualizationTrackerForPlayer( Requester, true ) )
	{
		VisualizationTracker->SetVisualizationBit( VisualizationId, VisualizationType, bVisualizationEnabled );
	}
}

bool UBVPSubsystem::GetVisualizationRequesterState( APlayerController* Requester, FName VisualizationId, EBVPPathVisualizationType VisualizationType )
{
	if (const FBVPPlayerVisualizationTracker* VisualizationTracker = FindVisualizationTrackerForPlayer( Requester, false ) )
	{
		return VisualizationTracker->GetVisualizationBit( VisualizationId, VisualizationType );
	}
	return false;
}

void UBVPSubsystem::BindPlayerActions( AFGCharacterPlayer* CharacterPlayer, UEnhancedInputComponent* InputComponent )
{
	const UBVPSettings* BVPSettings = UBVPSettings::Get();
	InputComponent->BindAction( BVPSettings->InputActionVisualizePaths.LoadSynchronous(), ETriggerEvent::Triggered, this, &UBVPSubsystem::Input_ToggleVisualizePaths, CharacterPlayer );
	InputComponent->BindAction( BVPSettings->InputActionOpenPathEditor.LoadSynchronous(), ETriggerEvent::Triggered, this, &UBVPSubsystem::Input_OpenPathEditor, CharacterPlayer );
}

FBVPPlayerVisualizationTracker* UBVPSubsystem::FindVisualizationTrackerForPlayer( APlayerController* PlayerController, bool bCreateIfNotFound )
{
	for ( FBVPPlayerVisualizationTracker* VisualizationTracker : VisualizationTrackers )
	{
		if ( VisualizationTracker->IsVisualizationTrackerValid() && VisualizationTracker->GetOwner() == PlayerController )
		{
			return VisualizationTracker;
		}
	}

	if ( bCreateIfNotFound && PlayerController )
	{
		FBVPPlayerVisualizationTracker* NewPlayerTracker = new FBVPPlayerVisualizationTracker( this, PlayerController );
		VisualizationTrackers.Add( NewPlayerTracker );

		return NewPlayerTracker;
	}
	return nullptr;
}

void UBVPSubsystem::Input_ToggleVisualizePaths( const FInputActionValue& ActionValue, AFGCharacterPlayer* CharacterPlayer )
{
	if ( ActionValue.IsNonZero() )
	{
		if ( APlayerController* PlayerController = CharacterPlayer->GetController<APlayerController>() )
		{
			const FName VisualizationIdKeyBind( TEXT("BVP_InputToggleVisualizePaths") );
			constexpr EBVPPathVisualizationType VisualizationType = EBVPPathVisualizationType::SegmentVisualization;
			
			const bool bCurrentState = GetVisualizationRequesterState( PlayerController, VisualizationIdKeyBind, VisualizationType );
			SetVisualizationRequesterState( PlayerController, VisualizationIdKeyBind, VisualizationType, !bCurrentState );	
		}
	}
}

void UBVPSubsystem::Input_OpenPathEditor( const FInputActionValue& ActionValue, AFGCharacterPlayer* CharacterPlayer )
{
	if ( ActionValue.IsNonZero() )
	{
		if ( const AFGPlayerController* PlayerController = CharacterPlayer->GetFGPlayerController() )
		{
			if ( AFGHUD* HUD = PlayerController->GetHUD<AFGHUD>() )
			{
				HUD->OpenInteractUI( PathEditorWidget, this );
			}
		}
	}
}

void UBVPSubsystem::TickActiveVisualizations()
{
	const AFGVehicleSubsystem* VehicleSubsystem = AFGVehicleSubsystem::Get( this );
	if ( !VehicleSubsystem )
	{
		return;
	}

	// Cleanup stale visualization paths before we do anything
	for ( int32 i = VisualizedPaths.Num() - 1; i >= 0; i-- )
	{
		FBVPVehiclePathVisualization* PathVisualization = VisualizedPaths[i];
		fgcheck( PathVisualization );

		if ( !PathVisualization->IsVisualizationValid() )
		{
			PathVisualization->DestroyVisualization();
			delete PathVisualization;
			VisualizedPaths.RemoveAt( i );
		}
	}
	
	// Update path visualizations based on target lists
	TMap<AFGDrivingTargetList*, FBVPVehiclePathVisualization*> ExistingVisualizationPaths;
	for ( FBVPVehiclePathVisualization* VehiclePathVisualization : VisualizedPaths )
	{
		ExistingVisualizationPaths.Add( VehiclePathVisualization->GetTargetList(), VehiclePathVisualization );
	}

	// Clients do not populate mTargetLists, so we need to use TActorIterator instead.
	TArray<AFGDrivingTargetList*> AllTargetLists;
	if ( !GetWorld()->IsNetMode( NM_Client ) )
	{
		AllTargetLists.Append( VehicleSubsystem->mTargetLists );
	}
	else
	{
		for ( TActorIterator<AFGDrivingTargetList> It( GetWorld() ); It; ++It )
		{
			AFGDrivingTargetList* DrivingTargetList = *It;
			if ( DrivingTargetList && !DrivingTargetList->IsTemporary() && DrivingTargetList->HasData() )
			{
				AllTargetLists.Add( DrivingTargetList );
			}
		}
	}

	// If we are the client, we need to manually ensure that each target point has a valid cache owner list
	if ( GetWorld()->IsNetMode( NM_Client ) )
	{
		for ( AFGDrivingTargetList* DrivingTargetList : AllTargetLists )
		{
			for( AFGTargetPoint* Target = DrivingTargetList->GetFirstTarget(); Target; Target = Target->GetNext() )
			{
				Target->SetOwningList( DrivingTargetList );
				Target->SetVisibility( DrivingTargetList->IsPathVisible() );
			}
		}
	}

	// Update each target list have in the world
	for ( AFGDrivingTargetList* DrivingTargetList : AllTargetLists )
	{
		FBVPVehiclePathVisualization*& ExistingVisualization = ExistingVisualizationPaths.FindOrAdd( DrivingTargetList );
		if ( !ExistingVisualization )
		{
			ExistingVisualization = new FBVPVehiclePathVisualization( this, DrivingTargetList );
			VisualizedPaths.Add( ExistingVisualization );
		}

		fgcheck( ExistingVisualization->IsVisualizationValid() );
		ExistingVisualization->UpdateVisualization();
	}
}

void UBVPSubsystem::TickVisualizationTrackers()
{
	// Cleanup stale or empty visualization trackers, as they use up performance
	for ( int32 i = VisualizationTrackers.Num() - 1; i >= 0; i-- )
	{
		FBVPPlayerVisualizationTracker* VisualizationTracker = VisualizationTrackers[i];
		fgcheck( VisualizationTracker );

		if ( !VisualizationTracker->IsVisualizationTrackerValid() || VisualizationTracker->IsVisualizationTrackerEmpty() )
		{
			VisualizationTracker->DestroyVisualizationTracker();
			delete VisualizationTracker;
			VisualizationTrackers.RemoveAt( i );
		}
	}
	
	// Update visualization trackers
	for ( FBVPPlayerVisualizationTracker* VisualizationTracker : VisualizationTrackers )
	{
		fgcheck( VisualizationTracker );
		fgcheck( VisualizationTracker->IsVisualizationTrackerValid() );

		VisualizationTracker->UpdateVisualizationTracker();
	}
}

void UBVPRemoteCallObject::GetLifetimeReplicatedProps( TArray<FLifetimeProperty>& OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( ThisClass, ForceNetField_UBVPRemoteCallObject );
}

void UBVPRemoteCallObject::Server_EditPathNode_Implementation( AFGTargetPoint* TargetPoint, const FVector& NewLocation, const FRotator& NewRotation )
{
	UBVPSubsystem* BVPSubsystem = GetWorld()->GetSubsystem<UBVPSubsystem>();
	if ( BVPSubsystem && TargetPoint )
	{
		// If we have failed to update the path node, force the correct location back to the client
		FText IgnoredErrorMessage;
		if ( !BVPSubsystem->MovePathNode( nullptr, TargetPoint, NewLocation, NewRotation, false, IgnoredErrorMessage ) )
		{
			Client_ForcePathNodeUpdate( TargetPoint, TargetPoint->GetActorLocation(), TargetPoint->GetActorRotation() );
		}
	}
}

void UBVPRemoteCallObject::Client_ForcePathNodeUpdate_Implementation( AFGTargetPoint* TargetPoint, const FVector& NewLocation, const FRotator& NewRotation )
{
	if ( TargetPoint )
	{
		TargetPoint->SetActorLocationAndRotation( NewLocation, NewRotation );
		if ( AFGDrivingTargetList* OwnerTargetList = TargetPoint->GetOwningList() )
		{
			if ( OwnerTargetList->IsComplete() && OwnerTargetList->HasData() )
			{
				OwnerTargetList->CreatePath();
			}
		}
	}
}

void UBVPRemoteCallObject::Server_RemovePathNode_Implementation( AFGTargetPoint* TargetPoint )
{
	UBVPSubsystem* BVPSubsystem = GetWorld()->GetSubsystem<UBVPSubsystem>();
	if ( BVPSubsystem && TargetPoint )
	{
		FText IgnoredErrorMessage;
		BVPSubsystem->RemovePathNode( nullptr, TargetPoint, IgnoredErrorMessage );
	}
}

void UBVPRemoteCallObject::Server_CreatePathNode_Implementation( AFGTargetPoint* AfterPoint, const FVector& NewLocation, const FRotator& NewRotation, int32 TargetSpeed )
{
	UBVPSubsystem* BVPSubsystem = GetWorld()->GetSubsystem<UBVPSubsystem>();

	if ( BVPSubsystem && AfterPoint && UBVPSubsystem::FindNextTargetPoint( AfterPoint ) &&
		UBVPSubsystem::CheckDistanceBetweenTwoPoints( AfterPoint->GetActorLocation(), NewLocation ) &&
		UBVPSubsystem::CheckDistanceBetweenTwoPoints( UBVPSubsystem::FindNextTargetPoint( AfterPoint )->GetActorLocation(), NewLocation ) )
	{
		if ( AFGTargetPoint* NewTargetPoint = BVPSubsystem->CreatePawnPathNodeInternal( AfterPoint, NewLocation, NewRotation, TargetSpeed ) )
		{
			Client_NotifyPointSpawned( NewTargetPoint );
		}
	}
}

void UBVPRemoteCallObject::Client_NotifyPointSpawned_Implementation( AFGTargetPoint* NewPoint )
{
	const UBVPSubsystem* BVPSubsystem = GetWorld()->GetSubsystem<UBVPSubsystem>();
	if ( BVPSubsystem && NewPoint )
	{
		AFGPlayerController* PlayerController = GetOuterFGPlayerController();
		BVPSubsystem->OnNewPathNodeCreated.Broadcast( NewPoint, PlayerController );
	}
}

void UBVPRemoteCallObject::Server_SetPathNodeTargetSpeed_Implementation( AFGTargetPoint* TargetPoint, int32 TargetSpeed )
{
	UBVPSubsystem* BVPSubsystem = GetWorld()->GetSubsystem<UBVPSubsystem>();
	if ( BVPSubsystem && TargetPoint && TargetSpeed > 0 )
	{
		BVPSubsystem->SetPathNodeTargetSpeed( nullptr, TargetPoint, TargetSpeed );
	}
}

#undef LOCTEXT_NAMESPACE

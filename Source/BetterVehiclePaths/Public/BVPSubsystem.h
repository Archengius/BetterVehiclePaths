// Copyright Nikita Zolotukhin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "Subsystems/WorldSubsystem.h"
#include "BVPSubsystem.generated.h"

// Pre-declarations
class AFGDrivingTargetList;
class AFGTargetPoint;
class USplineComponent;
class AFGCharacterPlayer;
class UEnhancedInputComponent;
struct FInputActionValue;
class USplineMeshComponent;
class UFGInteractWidget;
struct FHitResult;
class FBVPVehiclePathVisualization;
class UBVPSubsystem;
class FBVPPlayerVisualizationTracker;

enum class EBVPPathVisualizationType : uint8;

USTRUCT( BlueprintType )
struct BETTERVEHICLEPATHS_API FBVPVehiclePathSegmentHit
{
	GENERATED_BODY()

	// Point that begins the path segment hit
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Vehicle Path" )
	AFGTargetPoint* PointAfter{};

	// Spline that the segment belongs to. Note that this will be the entire path spline, this segment is just a portion in it
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Vehicle Path" )
	USplineComponent* SplineComponent{};

	// Distance along the spline that got hit
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Vehicle Path" )
	float ProgressAlongSpline{};
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FBVPOnNewPathNodeCreated, AFGTargetPoint*, NewPathNode, AFGPlayerController*, OwnerPlayerController );

UCLASS( BlueprintType )
class BETTERVEHICLEPATHS_API UBVPSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	UBVPSubsystem();
	~UBVPSubsystem();

	// Begin UTickableWorldSubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// End UTickableWorldSubsystem interface
	
	// Traces from the specified screen position for the player, trying to find a hit with a vehicle waypoint
	UFUNCTION( BlueprintCallable, Category = "BVP Subsystem" )
	AFGTargetPoint* TraceForTargetPoint( APlayerController* PlayerController, const FVector2D& ScreenPosition );

	// Traces from the specified screen position for the player, trying to find a vehicle spline path segment hit. Path visualization should be enabled for the hit to succeed.
	UFUNCTION( BlueprintCallable, Category = "BVP Subsystem" )
	bool TraceForSplineSegment( APlayerController* PlayerController, const FVector2D& ScreenPosition, FBVPVehiclePathSegmentHit& OutHitResult );

	// Traces from the given viewport position to the world
	UFUNCTION( BlueprintCallable, Category = "BVP Subsystem" )
	bool TraceForSolidSurface( APlayerController* PlayerController, const FVector2D& ScreenPosition, FHitResult& OutHitResult );

	// Attempts to spawn a new target point at the hit result of the given segment. The delegate OnNewPointSpawned will be fired on success on the BVP subsystem
	UFUNCTION( BlueprintCallable, Category = "BVP Subsystem" )
	bool CreateNewPathNode( AFGPlayerController* PlayerController, const FBVPVehiclePathSegmentHit& HitResult, FText& OutErrorMessage );

	// Attempts to remove the given path node. Returns a failure reason on failure that can be brought back to the player.
	UFUNCTION( BlueprintCallable, Category = "BVP Subsystem" )
	bool RemovePathNode( AFGPlayerController* PlayerController, AFGTargetPoint* TargetPoint, FText& OutErrorMessage );

	// Updates the target speed value for the given node
	UFUNCTION( BlueprintCallable, Category = "BVP Subsystem" )
	bool SetPathNodeTargetSpeed( AFGPlayerController* PlayerController, AFGTargetPoint* TargetPoint, int32 NewTargetSpeed );

	// Checks if the given path node can be moved to the new location.
	UFUNCTION( BlueprintPure, Category = "BVP Subsystem" )
	static bool CheckCanMovePathNode( const AFGTargetPoint* TargetPoint, const FVector& NewLocation, const FRotator& NewRotation, FText& OutErrorMessage );

	UFUNCTION( BlueprintPure, Category = "BVP Subsystem" )
	static bool CheckCanRemovePathNode( const AFGTargetPoint* TargetPoint, FText& OutErrorMessage );

	// Moves the given path node to the new location. If client prediction is true, the node will not actually be moved on the server.
	// Keep in mind that it's your responsibility to keep the node position in sync with the server in that case.
	UFUNCTION( BlueprintCallable, Category = "BVP Subsystem" )
	bool MovePathNode( AFGPlayerController* PlayerController, AFGTargetPoint* TargetPoint, const FVector& NewLocation, const FRotator& NewRotation, bool bClientPrediction, FText& OutErrorMessage );
	
	UFUNCTION( BlueprintCallable, Category = "BVP Subsystem" )
	void SetVisualizationRequesterState( APlayerController* Requester, FName VisualizationId,
		UPARAM( meta = (Bitmask, BitmaskEnum = "/Script/BetterVehiclePaths.EBVPPathVisualizationType") ) EBVPPathVisualizationType VisualizationType, bool bVisualizationEnabled );

	UFUNCTION( BlueprintPure, Category = "BVP Subsystem" )
	bool GetVisualizationRequesterState( APlayerController* Requester, FName VisualizationId,
		UPARAM( meta = (Bitmask, BitmaskEnum = "/Script/BetterVehiclePaths.EBVPPathVisualizationType") ) EBVPPathVisualizationType VisualizationType );
	
	void BindPlayerActions( AFGCharacterPlayer* CharacterPlayer, UEnhancedInputComponent* InputComponent );

	FBVPPlayerVisualizationTracker* FindVisualizationTrackerForPlayer( APlayerController* PlayerController, bool bCreateIfNotFound = false );
	
	const TArray<FBVPVehiclePathVisualization*>& GetAllVisualizedPaths() const { return VisualizedPaths; }
	const TArray<FBVPPlayerVisualizationTracker*>& GetAllPlayerTrackers() const { return VisualizationTrackers; }

	static AFGTargetPoint* FindPrevTargetPoint( const AFGTargetPoint* TargetPoint );
	static AFGTargetPoint* FindNextTargetPoint( const AFGTargetPoint* TargetPoint );
	// This function is needed because the spline point mapping to target points is a bit weird and first 2 points are actually last 2 points of the list
	static AFGTargetPoint* GetTargetPointAtSplinePoint( const AFGDrivingTargetList* TargetPointList, int32 SplinePointIndex, int32 NumSplinePoints );
protected:
	friend class UBVPRemoteCallObject;
	
	AFGTargetPoint* CreatePawnPathNodeInternal( AFGTargetPoint* AfterPoint, const FVector& NewLocation, const FRotator& NewRotation, int32 TargetSpeed ) const;
	static bool CheckDistanceBetweenTwoPoints( const FVector& LocationA, const FVector& LocationB );
	static float GetTraceDistanceForPlayer( const APlayerController* PlayerController );

	bool TraceForPathNodeChannelInternal( const APlayerController* PlayerController, const FVector2D& ScreenPosition, TArray<FHitResult>& OutHitResults ) const;
	void Input_ToggleVisualizePaths( const FInputActionValue& ActionValue, AFGCharacterPlayer* CharacterPlayer );
	void Input_OpenPathEditor( const FInputActionValue& ActionValue, AFGCharacterPlayer* CharacterPlayer );
	
	void TickActiveVisualizations();
	void TickVisualizationTrackers();
public:
	// Called when a new path node has been created through the subsystem
	UPROPERTY( BlueprintAssignable, Transient, Category = "BVP Subsystem" )
	FBVPOnNewPathNodeCreated OnNewPathNodeCreated;

protected:
	// List of paths being currently visualized
	TArray<FBVPVehiclePathVisualization*> VisualizedPaths;

	// Visualization trackers for each player currently online
	TArray<FBVPPlayerVisualizationTracker*> VisualizationTrackers;

public:
	// Number of spline points in the vehicle path spline that have negative time
	static constexpr int32 NumBacktrackSplinePoints = 2;
	
	// Cached data from the BVPSettings to be available when needed without having to flush the streaming
	UPROPERTY( Transient )
	UStaticMesh* PathVisualizationMesh;
	
	UPROPERTY( Transient )
	UMaterialInterface* PathVisualizationMaterial;

	UPROPERTY( Transient )
	TSubclassOf<UFGInteractWidget> PathEditorWidget;

	UPROPERTY( Transient )
	TSubclassOf<AFGTargetPoint> PathNodeClass;

	UPROPERTY( Transient, BlueprintReadOnly, Category = "BVP Subsystem" )
	UMaterialInterface* PathEditorSelectedMaterial;
};

UCLASS()
class BETTERVEHICLEPATHS_API UBVPRemoteCallObject : public UFGRemoteCallObject
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps( TArray< FLifetimeProperty >& OutLifetimeProps ) const override;

	UFUNCTION( Server, Reliable )
	void Server_EditPathNode( AFGTargetPoint* TargetPoint, const FVector& NewLocation, const FRotator& NewRotation );

	UFUNCTION( Server, Reliable )
	void Server_RemovePathNode( AFGTargetPoint* TargetPoint );

	UFUNCTION( Server, Reliable )
	void Server_CreatePathNode( AFGTargetPoint* AfterPoint, const FVector& NewLocation, const FRotator& NewRotation, int32 TargetSpeed );
	
	UFUNCTION( Client, Reliable )
	void Client_ForcePathNodeUpdate( AFGTargetPoint* TargetPoint, const FVector& NewLocation, const FRotator& NewRotation );

	UFUNCTION( Client, Reliable )
	void Client_NotifyPointSpawned( AFGTargetPoint* NewPoint );

	UFUNCTION( Server, Reliable )
	void Server_SetPathNodeTargetSpeed( AFGTargetPoint* TargetPoint, int32 TargetSpeed );
private:
	UPROPERTY( Replicated, Meta = ( NoAutoJson ) )
	bool ForceNetField_UBVPRemoteCallObject = false;
};

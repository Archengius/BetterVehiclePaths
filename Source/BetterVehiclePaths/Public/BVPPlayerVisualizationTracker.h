// Copyright Nikita Zolotukhin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BVPPlayerVisualizationTracker.generated.h"

class UBVPSubsystem;
class AFGPlayerController;
class FBVPVehiclePathVisualization;
class FBVPVehiclePathSegmentVisualization;

// Possible bits that can be set on the visualization subsystem to enable various functionality
UENUM( BlueprintType, meta = ( Bitflags, UseEnumValuesAsMaskValuesInEditor ) )
enum class EBVPPathVisualizationType : uint8
{
	None = 0x00,
	SegmentVisualization = 0x01,
	SegmentCollision = 0x02
};
ENUM_CLASS_FLAGS( EBVPPathVisualizationType );

class BETTERVEHICLEPATHS_API FBVPPlayerVisualizationTracker
{
	// Maps of segments to the visualization bits set for these segments
	TMap<const FBVPVehiclePathSegmentVisualization*, EBVPPathVisualizationType> VisualizationSegmentToVisualizationBitMap;

	// Map of VisualizationId a bitmask of all visualization bits enabled on it
	TMap<FName, EBVPPathVisualizationType> EnabledVisualizationBits;

	UBVPSubsystem* OwnerSubsystem{};
	APlayerController* OwnerPlayer{};
	int32 LastVisualizationSegmentArraySize{};
public:
	FBVPPlayerVisualizationTracker( UBVPSubsystem* InSubsystem, APlayerController* InPlayer );

	FORCEINLINE APlayerController* GetOwner() const { return OwnerPlayer; }
	
	void SetVisualizationBit( FName VisualizationId, EBVPPathVisualizationType VisualizationBit, bool bVisualizationEnabled );
	bool GetVisualizationBit( FName VisualizationId, EBVPPathVisualizationType VisualizationBit ) const;

	bool IsVisualizationTrackerValid() const;
	bool IsVisualizationTrackerEmpty() const;
	
	void DestroyVisualizationTracker();
	void ClearSegmentVisualization( const FBVPVehiclePathSegmentVisualization* SegmentVisualization );
	
	void UpdateVisualizationTracker();

	void AddReferencedObjects( FReferenceCollector& ReferenceCollector );
private:
	static constexpr EBVPPathVisualizationType AllVisualizationTypes[] { EBVPPathVisualizationType::SegmentCollision, EBVPPathVisualizationType::SegmentVisualization };
	
	void CollectAllVisualizationPaths( TArray<FBVPVehiclePathSegmentVisualization*>& OutAllVisualizations );
	EBVPPathVisualizationType GetCombinedVisualizationFlags() const;
	
	void ApplyVisualizationBits( const TArray<FBVPVehiclePathSegmentVisualization*>& AllVisualizations, const TMap<FBVPVehiclePathSegmentVisualization*, EBVPPathVisualizationType>& NewVisualizationBits );
	static void SetVisualizationTypeEnabledForSegment( FBVPVehiclePathSegmentVisualization* SegmentVisualization, EBVPPathVisualizationType VisualizationType, bool bEnabled );
};
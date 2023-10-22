// Copyright Nikita Zolotukhin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BVPSettings.generated.h"

class UInputAction;
class UStaticMesh;
class UMaterialInterface;
class UFGInteractWidget;
class AFGTargetPoint;
class UFGInputMappingContext;

enum class EBVPPathVisualizationType : uint8;

UCLASS( Config = BetterVehiclePaths, DefaultConfig, meta = ( DisplayName = "Better Vehicle Paths" ) )
class BETTERVEHICLEPATHS_API UBVPSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	// Class of the path node actors that we will spawn into the world when creating new target points
	UPROPERTY( EditAnywhere, Category = "General", Config )
	TSoftClassPtr<AFGTargetPoint> PathNodeClass;

	// Maximum distance allowed between vehicle path nodes
	UPROPERTY( EditAnywhere, Category = "General", Config )
	float MaxDistanceBetweenPathNodes;
	
	// Maximum distance at which path visualizations will be rendered - per visualization bit
	UPROPERTY( EditAnywhere, Category = "General", Config )
	TMap<EBVPPathVisualizationType, float> MaxPathVisualizationDistance;

	// Minimum distance between path nodes before we are allowed to spawn new ones
	UPROPERTY( EditAnywhere, Category = "General", Config )
	float MinDistanceBetweenPathNodes;

	// Mapping contexts owned by the mod. Used to make sure they are referenced at all times and, as such, always cooked
	UPROPERTY( EditAnywhere, Category = "General", Config )
	TArray<TSoftObjectPtr<UFGInputMappingContext>> OwnedMappingContexts;
	
	// Input action to use to toggle the vehicle path visualization for paths with visible nodes
	UPROPERTY( EditAnywhere, Category = "Enhanced Input", Config )
	TSoftObjectPtr<UInputAction> InputActionVisualizePaths;

	// Input action to open the path editor in which you can drag and move nodes around or delete them
	UPROPERTY( EditAnywhere, Category = "Enhanced Input", Config )
	TSoftObjectPtr<UInputAction> InputActionOpenPathEditor;

	// Mesh to use as a segment for path visualization
	UPROPERTY( EditAnywhere, Category = "Path Visualization | Mesh", Config )
	TSoftObjectPtr<UStaticMesh> PathVisualizationMesh;
	
	// Length of one mesh segment used for path visualization
	UPROPERTY( EditAnywhere, Category = "Path Visualization | Mesh", Config )
	float PathVisualizationSegmentLength;

	// Material to use for path visualization
	UPROPERTY( EditAnywhere, Category = "Path Visualization | Mesh", Config )
	TSoftObjectPtr<UMaterialInterface> PathVisualizationMaterial;

	// Name of the parameter on the material instance to populate with a randomly selected color of a path
	UPROPERTY( EditAnywhere, Category = "Path Visualization | Mesh", Config )
	FName PathVisualizationColorParameterName;
	
	// The thickness of the collision box to use for the visualization
	UPROPERTY( EditAnywhere, Category = "Path Visualization | Collision", Config )
	float PathVisualizationCollisionThickness;

	// The step to use when building collision boxes for segments in the visualization
	UPROPERTY( EditAnywhere, Category = "Path Visualization | Collision", Config )
	float PathVisualizationCollisionStep;

	// Maximum angle between two directions across the spline to break down a collider into smaller ones
	UPROPERTY( EditAnywhere, Category = "Path Visualization | Collision", Config )
	float PathVisualizationCollisionMaximumAngleDifference;
	
	// Widget to use for the vehicle path editor
	UPROPERTY( EditAnywhere, Category = "Path Editor", Config )
	TSoftClassPtr<UFGInteractWidget> PathEditorWidget;

	// Minimum trace distance when tracing from the screen space
	UPROPERTY( EditAnywhere, Category = "Path Editor", Config )
	float MinTraceDistanceForPathEditor;

	// Material that will be used when the path node is selected in the editor. The path visualization will ignore selected node.
	UPROPERTY( EditAnywhere, Category = "Path Editor", Config )
	TSoftObjectPtr<UMaterialInterface> PathEditorSelectedMaterial;

	// Distance for a single nudge interaction, in world units, per axis
	UPROPERTY( EditAnywhere, Category = "Path Editor", BlueprintReadOnly, Config )
	float NudgeDistance;

	// Rotation step when rotating the path nodes with the scroll wheel, in degrees
	UPROPERTY( EditAnywhere, Category = "Path Editor", BlueprintReadOnly, Config )
	float PathNodeRotationStep;

	// Retrieves the global singleton of the settings
	UFUNCTION( BlueprintPure, Category = "BVP Subsystem", DisplayName = "Get BVP Settings" )
	static const UBVPSettings* Get()
	{
		return GetDefault<UBVPSettings>();
	}
};
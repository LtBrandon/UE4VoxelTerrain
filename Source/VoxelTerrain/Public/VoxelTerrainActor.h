// Copyright (c) 2016 Brandon Garvin

#pragma once

// Polyvox Includes
#include "PolyVox/PagedVolume.h"
#include "PolyVox/MaterialDensityPair.h"
#include "PolyVox/Vector.h"

#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTerrainActor.generated.h"

// Bridge between PolyVox Vector3DFloat and Unreal Engine 4 FVector
struct FPolyVoxVector : public FVector
{
	FORCEINLINE FPolyVoxVector()
	{}

	explicit FORCEINLINE FPolyVoxVector(EForceInit E)
		: FVector(E)
	{}

	FORCEINLINE FPolyVoxVector(float InX, float InY, float InZ)
		: FVector(InX, InY, InX)
	{}

	FORCEINLINE FPolyVoxVector(const FVector &InVec)
	{
		FVector::operator=(InVec);
	}

	FORCEINLINE FPolyVoxVector(const PolyVox::Vector3DFloat &InVec)
	{
		FPolyVoxVector::operator=(InVec);
	}

	FORCEINLINE FVector& operator=(const PolyVox::Vector3DFloat& Other)
	{
		this->X = Other.getX();
		this->Y = Other.getY();
		this->Z = Other.getZ();

		DiagnosticCheckNaN();

		return *this;
	}
};

class VoxelTerrainPager : public PolyVox::PagedVolume<PolyVox::MaterialDensityPair44>::Pager
{
public:
	// Constructor
	VoxelTerrainPager(uint32 NoiseSeed = 123, uint32 Octaves = 3, float Frequency = 0.01, float Scale = 32, float Offset = 0, float Height = 64);

	// Destructor
	virtual ~VoxelTerrainPager() {};

	// PagedVolume::Pager functions
	virtual void pageIn(const PolyVox::Region& region, PolyVox::PagedVolume<PolyVox::MaterialDensityPair44>::Chunk* pChunk);
	virtual void pageOut(const PolyVox::Region& region, PolyVox::PagedVolume<PolyVox::MaterialDensityPair44>::Chunk* pChunk);

private:
	// Some variables to control our terrain generator
	// The seed of our fractal
	uint32 Seed = 123;

	// The number of octaves that the noise generator will use
	uint32 NoiseOctaves = 3;

	// The frequency of the noise
	float NoiseFrequency = 0.01;

	// The scale of the noise. The output of the TerrainFractal is multiplied by this.
	float NoiseScale = 32;

	// The offset of the noise. This value is added to the output of the TerrainFractal.
	float NoiseOffset = 0;

	// The maximum height of the generated terrain in voxels. NOTE: Changing this will affect where the ground begins!
	float TerrainHeight = 64;
};

UCLASS()
class VOXELTERRAIN_API AVoxelTerrainActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelTerrainActor();

	// Called after the C++ constructor and after the properties have been initialized.
	virtual void PostInitializeComponents() override;

	// Called when the actor has begun playing in the level
	virtual void BeginPlay() override;

	// The procedurally generated mesh that represents our voxels
	UPROPERTY(Category = "Voxel Terrain", BlueprintReadWrite, VisibleAnywhere) class UProceduralMeshComponent* Mesh;

	// The material to apply to our voxel terrain
	UPROPERTY(Category = "Voxel Terrain", BlueprintReadWrite, EditAnywhere) TArray<UMaterialInterface*> TerrainMaterials;

	// Some variables to control our terrain generator
	// The seed of our fractal
	UPROPERTY(Category = "Voxel Terrain", BlueprintReadWrite, EditAnywhere) int32 Seed;

	// The number of octaves that the noise generator will use
	UPROPERTY(Category = "Voxel Terrain", BlueprintReadWrite, EditAnywhere) int32 NoiseOctaves;

	// The frequency of the noise
	UPROPERTY(Category = "Voxel Terrain", BlueprintReadWrite, EditAnywhere) float NoiseFrequency;

	// The scale of the noise. The output of the TerrainFractal is multiplied by this.
	UPROPERTY(Category = "Voxel Terrain", BlueprintReadWrite, EditAnywhere) float NoiseScale;

	// The offset of the noise. This value is added to the output of the TerrainFractal.
	UPROPERTY(Category = "Voxel Terrain", BlueprintReadWrite, EditAnywhere) float NoiseOffset;

	// The maximum height of the generated terrain in voxels. NOTE: Changing this will affect where the ground begins!
	UPROPERTY(Category = "Voxel Terrain", BlueprintReadWrite, EditAnywhere) float TerrainHeight;
	
private:
	TSharedPtr<PolyVox::PagedVolume<PolyVox::MaterialDensityPair44>> VoxelVolume;
};
// Copyright (c) 2016 Brandon Garvin

#include "VoxelTerrainActor.h"
#include "VoxelTerrain.h"

// PolyVox
#include "PolyVox/CubicSurfaceExtractor.h"
#include "PolyVox/MarchingCubesSurfaceExtractor.h"
#include "PolyVox/Mesh.h"
using namespace PolyVox;

// ANL
#include "VM/kernel.h"
using namespace anl;

// Sets default values
AVoxelTerrainActor::AVoxelTerrainActor()
{
	// Default values for our noise control variables.
	bIsSpherical = false;
	Seed = 123;
	NoiseOctaves = 3;
	NoiseFrequency = 0.01f;
	NoiseScale = 32.f;
	NoiseOffset = 0.f;
	TerrainHeight = 64.f;
	ChunksToGenerateX = 5;
	ChunksToGenerateY = 5;
	ChunksToGenerateZ = 5;
	MaxChunksX = 5;
	MaxChunksY = 5;
	MaxChunksZ = 5;

	Scene = CreateDefaultSubobject<USceneComponent>(FName(TEXT("Terrain")));
	SetRootComponent(Scene);

	Meshes.Empty();
}

// Called after the C++ constructor and after the properties have been initialized.
void AVoxelTerrainActor::PostInitializeComponents()
{
	// Initialize our paged volume.
	VoxelVolume = MakeShareable(new PagedVolume<MaterialDensityPair88>(new VoxelTerrainPager(bIsSpherical, Seed, NoiseOctaves, NoiseFrequency, NoiseScale, NoiseOffset, TerrainHeight)));

	// Clear all of the data in our array of meshes and recreate it with all zeros
	Meshes.Empty();
	Meshes.AddZeroed((MaxChunksX * 2) * (MaxChunksY * 2) * (MaxChunksZ * 2));

	// Call the base class's function.
	Super::PostInitializeComponents();
}

// Called when the actor has begun playing in the level
void AVoxelTerrainActor::BeginPlay()
{
	// This will generate all the chunks in the area specified by the ChunksToGenerate settings
	for (int32 X = -(ChunksToGenerateX - 1); X < ChunksToGenerateX; X++)
	{
		for (int32 Y = -(ChunksToGenerateY - 1); Y < ChunksToGenerateY; Y++)
		{
			for (int32 Z = -(ChunksToGenerateZ - 1); Z < ChunksToGenerateZ; Z++)
			{
				GenerateChunk(X, Y, Z);
			}
		}
	}
}

bool AVoxelTerrainActor::GenerateChunk(int32 X, int32 Y, int32 Z)
{
	// Extract the voxel mesh from PolyVox

	// Get the index of the new chunk. This is used to prevent duplicate chunks.
	int32 ChunkIndex = (Z + MaxChunksZ) * (MaxChunksX * 2) * (MaxChunksY * 2) + (Y + MaxChunksY) * (MaxChunksX * 2) + (X + MaxChunksX);

	// Make sure the chunk hasn't already been loaded.
	// This array should always fit a chunk index if a valid chunk was specified.
	if (Meshes[ChunkIndex] != nullptr)
		return false;

	// Convert to the actual location of the chunk
	// 32 is the size of each side of the chunk, this can be adjusted in the ToExtract region below.
	// If you change the chunk size make sure to adjust the OffsetLocation as well!
	int32 ChunkX = (X + MaxChunksX) * 32;
	int32 ChunkY = (Y + MaxChunksY) * 32;
	int32 ChunkZ = (Z + MaxChunksZ) * 32;

	// Spherical terrains need to be able to generate chunks on the Z axis.
	// If you're not working with spherical terrain in your project you might consider
	// removing the Z axis from this function.
	PolyVox::Region ToExtract(Vector3DInt32(ChunkX - 31, ChunkY - 31, ChunkZ - 31), Vector3DInt32(ChunkX + 31, ChunkY + 31, ChunkZ + 31));
	
	// Generate a blocky mesh from the voxels.
	//auto ExtractedMesh = extractCubicMesh(VoxelVolume.Get(), ToExtract);

	// Uncomment this line to generate a smooth mesh from the voxels.
	// This is mostly intended for use on spherical terrain.
	auto ExtractedMesh = extractMarchingCubesMesh(VoxelVolume.Get(), ToExtract);

	auto DecodedMesh = decodeMesh(ExtractedMesh);

	if (DecodedMesh.getNoOfIndices() == 0)
		return false;

	// Create a new mesh component to render and add it to the list of meshes.
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, FName(*FString::Printf(TEXT("VoxelMesh_%d"), ChunkIndex)));
	Mesh->RegisterComponent();
	Mesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
	Meshes[ChunkIndex] = Mesh;

	// Get the offset location of the chunk to apply to the mesh
	FVector OffsetLocation(X * 32, Y * 32,  Z * 32);

	// This isn't the most efficient way to handle this, but it works.
	// To improve the performance of this code, you'll want to modify 
	// the code so that you only run this section of code once.
	for (int32 Material = 0; Material < TerrainMaterials.Num(); Material++)
	{
		// Define variables to pass into the CreateMeshSection function
		auto Vertices = TArray<FVector>();
		auto Indices = TArray<int32>();
		auto Normals = TArray<FVector>();
		auto UV0 = TArray<FVector2D>();
		auto Colors = TArray<FColor>();
		auto Tangents = TArray<FProcMeshTangent>();

		// Loop over all of the triangle vertex indices
		for (uint32 i = 0; i < DecodedMesh.getNoOfIndices() - 2; i += 3)
		{
			// We need to add the vertices of each triangle in reverse or the mesh will be upside down
			auto Index = DecodedMesh.getIndex(i + 2);
			auto Vertex2 = DecodedMesh.getVertex(Index);
			auto TriangleMaterial = Vertex2.data.getMaterial();

			// Before we continue, we need to be sure that the triangle is the right material; we don't want to use verticies from other materials
			if (TriangleMaterial == (Material + 1))
			{
				// If it is of the same material, then we need to add the correct indices now
				Indices.Add(Vertices.Add((FPolyVoxVector(Vertex2.position) + OffsetLocation) * 100.f));

				Index = DecodedMesh.getIndex(i + 1);
				auto Vertex1 = DecodedMesh.getVertex(Index);
				Indices.Add(Vertices.Add((FPolyVoxVector(Vertex1.position) + OffsetLocation) * 100.f));

				Index = DecodedMesh.getIndex(i);
				auto Vertex0 = DecodedMesh.getVertex(Index);
				Indices.Add(Vertices.Add((FPolyVoxVector(Vertex0.position) + OffsetLocation) * 100.f));

				// Calculate the tangents of our triangle
				const FVector Edge01 = FPolyVoxVector(Vertex1.position - Vertex0.position);
				const FVector Edge02 = FPolyVoxVector(Vertex2.position - Vertex0.position);

				const FVector TangentX = Edge01.GetSafeNormal();
				FVector TangentZ = (Edge01 ^ Edge02).GetSafeNormal();

				for (int32 i = 0; i < 3; i++)
				{
					Tangents.Add(FProcMeshTangent(TangentX, false));
					Normals.Add(TangentZ);
				}
			}
		}

		// Finally create the mesh
		Mesh->CreateMeshSection(Material, Vertices, Indices, Normals, UV0, Colors, Tangents, true);
		Mesh->SetMaterial(Material, TerrainMaterials[Material]);
	}

	return true;
}

// VoxelTerrainPager Definitions
// Constructor
VoxelTerrainPager::VoxelTerrainPager(bool bIsSphericalTerrain, uint32 NoiseSeed, uint32 Octaves, float Frequency, float Scale, float Offset, float Height) : PagedVolume<MaterialDensityPair88>::Pager(), bIsSpherical(bIsSphericalTerrain), Seed(NoiseSeed), NoiseOctaves(Octaves), NoiseFrequency(Frequency), NoiseScale(Scale), NoiseOffset(Offset), TerrainHeight(Height)
{

}

// Called when a new chunk is paged in
// This function will automatically generate our voxel-based terrain from simplex noise
void VoxelTerrainPager::pageIn(const PolyVox::Region& region, PagedVolume<MaterialDensityPair88>::Chunk* Chunk)
{
	// This is our kernel. It is responsible for generating our noise.
	CKernel NoiseKernel;

	// Commonly used constants
	auto Zero = NoiseKernel.constant(0);
	auto One = NoiseKernel.constant(1);
	auto VerticalHeight = NoiseKernel.constant(TerrainHeight);
	auto HalfVerticalHeight = NoiseKernel.constant(TerrainHeight / 2.f);
	
	// This if statement has a lot of copy pasting, and could definitely be done better...
	// It works though, so we're just going to go with it to keep things simple.
	if (bIsSpherical)
	{
		// Select an area around 0, 0, 0 to become our sphere
		//	- This is done by comparing the result of NoiseKernel.radial() with the radius of our sphere
		//	- NoiseKernel.radial() returns the distance from 0, 0, 0 in case you don't know!
		auto SphereSelect = NoiseKernel.select(One, Zero, NoiseKernel.radial(), HalfVerticalHeight, Zero);

		// This is the actual noise generator we'll be using.
		// In this case I've gone with a simple fBm generator, which will create terrain that looks like smooth, rolling hills.
		auto TerrainFractal = NoiseKernel.simplefBm(BasisTypes::BASIS_SIMPLEX, InterpolationTypes::INTERP_LINEAR, NoiseOctaves, NoiseFrequency, Seed);

		// Scale and offset the generated noise value. 
		// Scaling the noise makes the features bigger or smaller, and offsetting it will move the terrain up and down.
		// 
		// Generally speaking it is probably better to avoid using NoiseOffset with a spherical terrain, it might not do what you expect!
		//	- You should probably just change the TerrainHeight from the editor instead.
		auto TerrainScale = NoiseKernel.scaleOffset(TerrainFractal, NoiseScale, NoiseOffset);

		// Finally, apply the offset we just calculated from the fractal to our sphere.
		auto PerturbGradient = NoiseKernel.translateDomain(SphereSelect, TerrainScale);

		CNoiseExecutor TerrainExecutor(NoiseKernel);

		// Now that we have our noise setup, let's loop over our chunk and apply it.
		for (int x = region.getLowerX(); x <= region.getUpperX(); x++)
		{
			for (int y = region.getLowerY(); y <= region.getUpperY(); y++)
			{
				for (int z = region.getLowerZ(); z <= region.getUpperZ(); z++)
				{
					// Evaluate the noise
					auto EvaluatedNoise = TerrainExecutor.evaluateScalar(x, y, z, PerturbGradient);
					MaterialDensityPair88 Voxel;

					// For the sake of making this code shorter, I've opted to only use two materials: Air and Stone
					bool bSolid = EvaluatedNoise > 0.5;
					Voxel.setDensity(FMath::FloorToInt(FMath::Clamp(EvaluatedNoise, 0.0, 1.0) * Voxel.getMaxDensity()));
					Voxel.setMaterial(bSolid ? 1 : 0);

					// Voxel position within a chunk always start from zero. So if a chunk represents region (4, 8, 12) to (11, 19, 15)
					// then the valid chunk voxels are from (0, 0, 0) to (7, 11, 3). Hence we subtract the lower corner position of the
					// region from the volume space position in order to get the chunk space position.
					Chunk->setVoxel(x - region.getLowerX(), y - region.getLowerY(), z - region.getLowerZ(), Voxel);
				}
			}
		}
	}
	else
	{
		// Create a gradient on the vertical axis to form our ground plane.
		auto VerticalGradient = NoiseKernel.divide(NoiseKernel.clamp(NoiseKernel.subtract(VerticalHeight, NoiseKernel.z()), Zero, VerticalHeight), VerticalHeight);

		// Turn our gradient into two solids that represent the ground and air. This prevents floating terrain from forming later.
		auto VerticalSelect = NoiseKernel.select(Zero, One, VerticalGradient, NoiseKernel.constant(0.5), Zero);

		// This is the actual noise generator we'll be using.
		// In this case I've gone with a simple fBm generator, which will create terrain that looks like smooth, rolling hills.
		auto TerrainFractal = NoiseKernel.simplefBm(BasisTypes::BASIS_SIMPLEX, InterpolationTypes::INTERP_LINEAR, NoiseOctaves, NoiseFrequency, Seed);

		// Scale and offset the generated noise value. 
		// Scaling the noise makes the features bigger or smaller, and offsetting it will move the terrain up and down.
		auto TerrainScale = NoiseKernel.scaleOffset(TerrainFractal, NoiseScale, NoiseOffset);

		// Setting the Z scale of the fractal to 0 will effectively turn the fractal into a heightmap.
		auto TerrainZScale = NoiseKernel.scaleZ(TerrainScale, Zero);

		// Finally, apply the Z offset we just calculated from the fractal to our ground plane.
		auto PerturbGradient = NoiseKernel.translateZ(VerticalSelect, TerrainZScale);

		// Now we want to determine different materials based on a variety of factors.
		// This is made easier by the fact that we're basically generating a heightmap.

		// For now our grass is always going to appear at the top level, so we don't need to do anything fancy.
		auto GrassZ = NoiseKernel.subtract(HalfVerticalHeight, TerrainZScale);

		// To generate pockets of ore we're going to need another noise generator.
		auto OreFractal = NoiseKernel.simpleRidgedMultifractal(BasisTypes::BASIS_SIMPLEX, InterpolationTypes::INTERP_LINEAR, 2, 1.2 * NoiseFrequency, Seed);
		auto FractalGradient = NoiseKernel.multiply(OreFractal, NoiseKernel.bias(VerticalGradient, NoiseKernel.constant(1.015)));

		CNoiseExecutor TerrainExecutor(NoiseKernel);

		// Now that we have our noise setup, let's loop over our chunk and apply it.
		for (int x = region.getLowerX(); x <= region.getUpperX(); x++)
		{
			for (int y = region.getLowerY(); y <= region.getUpperY(); y++)
			{
				for (int z = region.getLowerZ(); z <= region.getUpperZ(); z++)
				{
					// Evaluate the noise
					auto EvaluatedNoise = TerrainExecutor.evaluateScalar(x, y, z, PerturbGradient);
					MaterialDensityPair88 Voxel;

					// Determine the solidity of the voxel
					bool bSolid = EvaluatedNoise > 0.5;

					// Determine what material should be set on the voxel
					// Air = 0
					// Stone = 1
					// Dirt = 2
					// Grass = 3
					// Ore = 4

					int ActualGrassZ = FMath::FloorToInt(TerrainExecutor.evaluateScalar(x, y, z, GrassZ));
					int DirtZ = ActualGrassZ - 1;
					int DirtThickness = 10;

					// Set all the solid blocks to something
					if (bSolid)
					{
						auto EvaluatedOreFractal = TerrainExecutor.evaluateScalar(x, y, z, OreFractal);
						auto EvaluatedFractalGradient = TerrainExecutor.evaluateScalar(x, y, z, FractalGradient);

						// Place a cave
						if (EvaluatedOreFractal > 1.88 && EvaluatedFractalGradient > 0.875)
							bSolid = false;

						// Make the top layer into grass
						else if (z >= ActualGrassZ)
						{
							Voxel.setMaterial(3);
						}

						// Make a layer of dirt below the grass
						else if (z <= DirtZ && z > (DirtZ - DirtThickness))
						{
							Voxel.setMaterial(2);
						}

						// Make the stone below the dirt
						else
						{
							// Place an air pocket
							if (EvaluatedOreFractal > 1.85)
								bSolid = false;

							// Place an underground dirt pocket
							else if (EvaluatedOreFractal > 1.6 && EvaluatedOreFractal < 1.8)
								Voxel.setMaterial(2);

							// Place an ore deposit
							else if (EvaluatedOreFractal < 1.5 || EvaluatedOreFractal > 1.848)
								Voxel.setMaterial(4);

							// Place stone
							else
								Voxel.setMaterial(1);
						}
					}

					Voxel.setDensity(FMath::FloorToInt(FMath::Clamp(bSolid ? EvaluatedNoise : 0.0, 0.0, 1.0) * Voxel.getMaxDensity()));

					// Voxel position within a chunk always start from zero. So if a chunk represents region (4, 8, 12) to (11, 19, 15)
					// then the valid chunk voxels are from (0, 0, 0) to (7, 11, 3). Hence we subtract the lower corner position of the
					// region from the volume space position in order to get the chunk space position.
					Chunk->setVoxel(x - region.getLowerX(), y - region.getLowerY(), z - region.getLowerZ(), Voxel);
				}
			}
		}
	}
}

// Called when a chunk is paged out
void VoxelTerrainPager::pageOut(const PolyVox::Region& region, PagedVolume<MaterialDensityPair88>::Chunk* Chunk)
{
}
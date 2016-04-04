// Copyright (c) 2016 Brandon Garvin

#include "VoxelTerrain.h"
#include "VoxelTerrainActor.h"

// PolyVox
#include "PolyVox/CubicSurfaceExtractor.h"
#include "PolyVox/Mesh.h"
using namespace PolyVox;

// ANL
#include "VM/kernel.h"
using namespace anl;

// Sets default values
AVoxelTerrainActor::AVoxelTerrainActor()
{
	// Initialize our mesh component
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain Mesh"));

	// Default values for our noise control variables.
	Seed = 123;
	NoiseOctaves = 3;
	NoiseFrequency = 0.01f;
	NoiseScale = 32.f;
	NoiseOffset = 0.f;
	TerrainHeight = 64.f;
}

// Called after the C++ constructor and after the properties have been initialized.
void AVoxelTerrainActor::PostInitializeComponents()
{
	// Initialize our paged volume.
	VoxelVolume = MakeShareable(new PagedVolume<MaterialDensityPair44>(new VoxelTerrainPager(Seed, NoiseOctaves, NoiseFrequency, NoiseScale, NoiseOffset, TerrainHeight)));

	// Call the base class's function.
	Super::PostInitializeComponents();
}

// Called when the actor has begun playing in the level
void AVoxelTerrainActor::BeginPlay()
{
	// Extract the voxel mesh from PolyVox
	PolyVox::Region ToExtract(Vector3DInt32(0, 0, 0), Vector3DInt32(127, 127, 63));
	auto ExtractedMesh = extractCubicMesh(VoxelVolume.Get(), ToExtract);
	auto DecodedMesh = decodeMesh(ExtractedMesh);

	// Define variables to pass into the CreateMeshSection function
	auto Vertices = TArray<FVector>();
	auto Indices = TArray<int32>();
	auto Normals = TArray<FVector>();
	auto UV0 = TArray<FVector2D>();
	auto Colors = TArray<FColor>();
	auto Tangents = TArray<FProcMeshTangent>();

	// Loop over all of the triangle vertex indices
	for (uint32 i = 0; i < DecodedMesh.getNoOfIndices() - 2; i+=3)
	{
		// We need to add the vertices of each triangle in reverse or the mesh will be upside down
		auto Index = DecodedMesh.getIndex(i + 2);
		auto Vertex2 = DecodedMesh.getVertex(Index);
		Indices.Add(Vertices.Add(FPolyVoxVector(Vertex2.position) * 100.f));
		
		Index = DecodedMesh.getIndex(i + 1);
		auto Vertex1 = DecodedMesh.getVertex(Index);
		Indices.Add(Vertices.Add(FPolyVoxVector(Vertex1.position) * 100.f));
		
		Index = DecodedMesh.getIndex(i);
		auto Vertex0 = DecodedMesh.getVertex(Index);
		Indices.Add(Vertices.Add(FPolyVoxVector(Vertex0.position) * 100.f));
		
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

	// Finally create the mesh
	Mesh->CreateMeshSection(0, Vertices, Indices, Normals, UV0, Colors, Tangents, true);
}

// VoxelTerrainPager Definitions
// Constructor
VoxelTerrainPager::VoxelTerrainPager(uint32 NoiseSeed, uint32 Octaves, float Frequency, float Scale, float Offset, float Height) : PagedVolume<MaterialDensityPair44>::Pager(), Seed(NoiseSeed), NoiseOctaves(Octaves), NoiseFrequency(Frequency), NoiseScale(Scale), NoiseOffset(Offset), TerrainHeight(Height)
{

}

// Called when a new chunk is paged in
// This function will automatically generate our voxel-based terrain from simplex noise
void VoxelTerrainPager::pageIn(const PolyVox::Region& region, PagedVolume<MaterialDensityPair44>::Chunk* Chunk)
{
	// This is our kernel. It is responsible for generating our noise.
	CKernel NoiseKernel;

	// Commonly used constants
	auto Zero = NoiseKernel.constant(0);
	auto One = NoiseKernel.constant(1);
	auto VerticalHeight = NoiseKernel.constant(TerrainHeight);

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
				MaterialDensityPair44 Voxel;

				bool bSolid = EvaluatedNoise > 0.5;
				Voxel.setDensity(bSolid ? 255 : 0);
				Voxel.setMaterial(bSolid ? 1 : 0);

				// Voxel position within a chunk always start from zero. So if a chunk represents region (4, 8, 12) to (11, 19, 15)
				// then the valid chunk voxels are from (0, 0, 0) to (7, 11, 3). Hence we subtract the lower corner position of the
				// region from the volume space position in order to get the chunk space position.
				Chunk->setVoxel(x - region.getLowerX(), y - region.getLowerY(), z - region.getLowerZ(), Voxel);
			}
		}
	}
}

// Called when a chunk is paged out
void VoxelTerrainPager::pageOut(const PolyVox::Region& region, PagedVolume<MaterialDensityPair44>::Chunk* Chunk)
{
}
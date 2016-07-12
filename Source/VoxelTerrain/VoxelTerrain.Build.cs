// Fill out your copyright notice in the Description page of Project Settings.
using System.IO; // Make sure you are using System.IO! Its not in here by default!
using UnrealBuildTool;

public class VoxelTerrain : ModuleRules
{
    // The path to our Third Party code assets; e.g. ProjectFolder/ThirdParty/
    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty/")); }
    }

    public VoxelTerrain(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent" });


        PrivateDependencyModuleNames.AddRange(new string[] {  });

        ////////////////////// Custom Voxel Terrain Stuff Starts Here //////////////////////////////////////
        // You will need to compile and add additional libraries if you want to use this on platforms not listed below!
        switch (Target.Platform)
        {
            // 64-bit Windows
            case UnrealTargetPlatform.Win64:
                PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "build", "ANL", "x64", "ANL.lib"));
                break;

            // 32-bit Windows
            case UnrealTargetPlatform.Win32:
                PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "build", "ANL", "x86", "ANL.lib"));
                break;

            // Mac
            case UnrealTargetPlatform.Mac:
                PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "build", "ANL", "Universal", "libANL.a"));
                break;
            
            // Linux
            case UnrealTargetPlatform.Linux:
                PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "build", "ANL", "x64", "libANL.a"));
                break;

            default:
                break;
        }

        // Include the headers for PolyVox and ANL so we can access them later.
        PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "polyvox", "include"));
        PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library"));
        ////////////////////// End Voxel Terrain Stuff //////////////////////////////////////////////////////
    }
}

// Fill out your copyright notice in the Description page of Project Settings.
using System.IO; // Make sure you are using System.IO! Its not in here by default!
using UnrealBuildTool;

public class VoxelTerrain : ModuleRules
{
    // Some versions of the engine might have the ModulePath and ThirdPartyPath variables defined already
    // 4.10.4 doesn't so we need to add them!

    // The path to our game module; e.g. ProjectFolder/Source/ModuleName/
    private string ModulePath
    {
        get { return Path.GetDirectoryName(RulesCompiler.GetModuleFilename(this.GetType().Name)); }
    }

    // The path to our Third Party code assets; e.g. ProjectFolder/ThirdParty/
    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/")); }
    }

    public VoxelTerrain(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        ////////////////////// Custom Voxel Terrain Stuff Starts Here //////////////////////////////////////
        // You will need to compile and add additional libraries if you want to use this on other platforms!
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "Builder", "RelWithDebInfo", "Builder.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "Framework", "RelWithDebInfo", "Framework.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "Imaging", "RelWithDebInfo", "Imaging.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "Module", "RelWithDebInfo", "Module.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "Noise", "RelWithDebInfo", "Noise.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library", "RGBA", "RelWithDebInfo", "RGBA.lib"));
        }

        // Add the headers for PolyVox and ANL so we can access them later.
        PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "polyvox", "include"));
        PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "accidental-noise-library"));
        ////////////////////// End Voxel Terrain Stuff //////////////////////////////////////////////////////
    }
}

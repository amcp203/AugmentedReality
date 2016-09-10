// Fill out your copyright notice in the Description page of Project Settings.
using System.IO;
using UnrealBuildTool;

public class AugmentedReality : ModuleRules
{	
	private string ModulePath
    {
        get { return Path.GetDirectoryName( RulesCompiler.GetModuleFilename( this.GetType().Name ) ); }
    }
 
    private string ThirdPartyPath
    {
        get { return Path.GetFullPath( Path.Combine( ModulePath, "../../ThirdParty/" ) ); }
    }
	
	public AugmentedReality(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore", "ShaderCore", "OpenCV" });
		PrivateDependencyModuleNames.AddRange(new string[] {  });
		//PublicIncludePaths.Add("D:/Games/VS/Projects/AugmentedReality/AugmentedReality/ThirdParty/OpenCV/Includes");
		//PublicIncludePaths.Add("D:/Games/VS/Projects/AugmentedReality/AugmentedReality/ThirdParty/lsd/Includes");
		//PublicAdditionalLibraries.Add("D:/Games/VS/Projects/AugmentedReality/AugmentedReality/ThirdParty/lsd/Libraries/lsd.lib"); 
		LoadLSD(Target);
		
	}
	
	public bool LoadLSD(TargetInfo Target)
    {
        bool isLibrarySupported = false;
 
        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
        {
            isLibrarySupported = true;
 
            string PlatformString = (Target.Platform == UnrealTargetPlatform.Win64) ? "x64" : "x86";
            string LibrariesPath = Path.Combine(ThirdPartyPath, "lsd", "Libraries");
 
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "lsd." + PlatformString + ".lib")); 
        }
 
        if (isLibrarySupported)
        {
            PublicIncludePaths.Add( Path.Combine( ThirdPartyPath, "lsd", "Includes" ) );
        }
 
        Definitions.Add(string.Format( "WITH_LSD_BINDING={0}", isLibrarySupported ? 1 : 0 ) );
 
        return isLibrarySupported;
    }
}

using UnrealBuildTool;

public class ProperFootball : ModuleRules
{
	public ProperFootball(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "ImageWrapper", "RenderCore", "RHI", "Slate", "SlateCore", "ApplicationCore"
		});

		// Everything the game reads from disk at run time lives under GameData/ (generated) or ArtSource/Packs/ (the owner's packs).
		// NonUFS = loose files next to the pak at the same relative path, read through FPaths::ProjectDir().
		RuntimeDependencies.Add("$(ProjectDir)/GameData/...", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Clubs/...", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Goals/...", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Projection.json", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Gameplay_Scale.json", StagedFileType.NonUFS);
	}
}

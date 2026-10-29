using UnrealBuildTool;

public class MWFrontendAuthoring : ModuleRules
{
	public MWFrontendAuthoring(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new[] { "MW", "UMG", "UMGEditor", "UnrealEd", "Slate", "SlateCore", "AssetTools", "MovieScene", "MovieSceneTracks" });
	}
}

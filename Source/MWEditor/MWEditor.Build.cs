using UnrealBuildTool;

public class MWEditor : ModuleRules
{
	public MWEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "UMGEditor" });
		PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "BlueprintGraph", "KismetCompiler", "Slate", "SlateCore" });
	}
}

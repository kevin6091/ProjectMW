using UnrealBuildTool;
public class MWUCMFrontend : ModuleRules
{
    public MWUCMFrontend(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "MW", "UMG", "UCMTitlePreview" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "LevelSequence", "MovieScene", "InputCore" });
    }
}

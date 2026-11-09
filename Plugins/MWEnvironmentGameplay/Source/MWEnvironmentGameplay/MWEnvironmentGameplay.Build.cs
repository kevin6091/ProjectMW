using UnrealBuildTool;
public class MWEnvironmentGameplay : ModuleRules
{
    public MWEnvironmentGameplay(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "MWEnvironmentRestore" });
        PrivateDependencyModuleNames.AddRange(new[] { "MW", "GameplayTags", "Json" });
    }
}

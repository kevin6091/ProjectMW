using UnrealBuildTool;

public class MWEnvironmentAcceptance : ModuleRules
{
    public MWEnvironmentAcceptance(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json", "MWEnvironmentRestore", "MW", "GameplayTags", "MWEnvironmentGameplay" });
    }
}

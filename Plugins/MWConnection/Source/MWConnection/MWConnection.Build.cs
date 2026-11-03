using UnrealBuildTool;

public class MWConnection : ModuleRules
{
    public MWConnection(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "OnlineSubsystem", "OnlineSubsystemUtils", "OnlineBase", "EngineSettings" });
    }
}

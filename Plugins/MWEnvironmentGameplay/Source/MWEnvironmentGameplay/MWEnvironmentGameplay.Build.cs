// 이 모듈이 사용하는 기존 런타임 의존성을 선언한다. 게임 코드에 에디터 전용 의존성을 추가하지 않는다.

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

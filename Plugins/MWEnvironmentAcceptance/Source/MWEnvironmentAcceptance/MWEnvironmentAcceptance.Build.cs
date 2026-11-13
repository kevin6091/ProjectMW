// 이 모듈이 사용하는 기존 런타임 의존성을 선언한다. 게임 코드에 에디터 전용 의존성을 추가하지 않는다.

using UnrealBuildTool;

public class MWEnvironmentAcceptance : ModuleRules
{
    public MWEnvironmentAcceptance(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json", "MWEnvironmentRestore",
                                                      "MW", "GameplayTags", "MWEnvironmentGameplay" });
    }
}

// 이 모듈이 사용하는 기존 런타임 의존성을 선언한다. 게임 코드에 에디터 전용 의존성을 추가하지 않는다.

using UnrealBuildTool;

public class MWEnvironmentRestore : ModuleRules
{
    public MWEnvironmentRestore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "Json" });
    }
}

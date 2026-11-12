// 이 모듈이 사용하는 기존 런타임 의존성을 선언한다. 게임 코드에 에디터 전용 의존성을 추가하지 않는다.

using UnrealBuildTool;

public class MWUCMFrontend : ModuleRules
{
    public MWUCMFrontend(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "MW", "UMG", "UCMTitlePreview" });
        PrivateDependencyModuleNames.AddRange(
            new[] { "Slate", "SlateCore", "LevelSequence", "MovieScene", "InputCore" });
    }
}

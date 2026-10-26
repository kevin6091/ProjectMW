// 저작권은 Epic Games, Inc.에 있으며, 모든 권리를 보유합니다.

using UnrealBuildTool;

// MW 게임 모듈을 컴파일할 때 필요한 헤더 경로와 다른 모듈을 정합니다.
public class MW : ModuleRules
{
	public MW(ReadOnlyTargetRules Target) : base(Target)
	{
		// 자주 쓰는 헤더를 미리 처리해 두는 공통 헤더 사용 방식을 정합니다.
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// Data/..., System/...처럼 모듈 폴더를 기준으로 헤더를 포함할 수 있게 합니다.
		PublicIncludePaths.Add(ModuleDirectory);
		// 공개 헤더에서 필요한 기본 엔진 기능, 향상된 입력 처리, 게임플레이 태그 모듈을 연결합니다.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags",
			"UMG", "OnlineSubsystem", "OnlineSubsystemUtils"
		});

		// 이 모듈 내부에서만 사용할 추가 모듈을 등록하는 자리입니다.
		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Slate 화면 위젯을 사용하면 아래 줄의 주석을 지웁니다.
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// 온라인 기능을 사용하면 아래 줄의 주석을 지웁니다.
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// Steam 온라인 기능을 사용하려면 MW.uproject의 Plugins 항목에 OnlineSubsystemSteam을 추가하고
		// Enabled를 true로 설정합니다.
	}
}

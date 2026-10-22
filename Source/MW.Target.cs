// 저작권은 Epic Games, Inc.에 있으며, 모든 권리를 보유합니다.

using UnrealBuildTool;
using System.Collections.Generic;

// 에디터 없이 실행하는 MW 게임 프로그램의 빌드 설정입니다.
public class MWTarget : TargetRules
{
	public MWTarget(TargetInfo Target) : base(Target)
	{
		// 게임 실행용 프로그램을 만들도록 지정합니다.
		Type = TargetType.Game;
		// 현재 프로젝트가 사용하는 엔진의 기본 빌드 규칙과 헤더 포함 순서를 따릅니다.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		// 이 프로그램에 프로젝트의 MW 게임 모듈을 포함합니다.
		ExtraModuleNames.Add("MW");
	}
}

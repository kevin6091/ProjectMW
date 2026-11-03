

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MWGameplayGameMode.generated.h"

// 테마 레벨의 서버 초기화입니다. 로비 규칙을 상속하지 않습니다.
UCLASS()
class MW_API AMWGameplayGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AMWGameplayGameMode();
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& Error) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* Player) override;

private:
	// 기존 GameMode의 테마 초기화 분기만 이전했습니다. 건설/주행 진행 기능은 아직 없습니다.
	void TryInitializeGameplay();
	bool bRoomSettingsApplied = false;
};

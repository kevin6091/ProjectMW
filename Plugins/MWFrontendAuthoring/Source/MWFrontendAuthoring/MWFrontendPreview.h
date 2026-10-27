#pragma once

#include "Frontend/MWFrontendConnection.h"
#include "Frontend/MWFrontendWidget.h"
#include "MWFrontendPreview.generated.h"

/** Editor-only fixtures. Never installed by the runtime menu or included in the Game target. */
UCLASS()
class UMWFrontendPreviewWidget : public UMWFrontendWidget
{
	GENERATED_BODY()
protected:
	virtual bool IsAuthenticated() const override { return true; }
};

UCLASS()
class UMWFrontendPreviewConnection : public UMWFrontendConnection
{
	GENERATED_BODY()
public:
	virtual EMWFrontendOperation GetOperation_Implementation() const override { return Operation; }
	virtual TArray<FMWFrontendTheme> GetThemes_Implementation() const override;
	virtual TArray<FMWRoomSearchResult> GetRooms_Implementation() const override;
	virtual FMWLobbyView GetLobby_Implementation() const override { return Lobby; }
	virtual bool CreateRoom_Implementation(const FMWCreateRoomRequest& Request) override;
	virtual bool SearchRooms_Implementation() override;
	virtual bool JoinRoom_Implementation(const FString& SessionId) override;
	virtual bool LeaveRoom_Implementation() override;
	virtual bool SetReady_Implementation(bool bReady) override;
	virtual bool StartMatch_Implementation() override;
	virtual bool CancelOperation_Implementation() override;
	void PrepareLobby(APlayerController* Controller);
	void CompleteSearch();
	EMWFrontendOperation Operation = EMWFrontendOperation::Idle;
	int32 SearchCount = 0;
	int32 CreateCount = 0;
	bool bSearchFinished = false;
	bool bCompleteSearchImmediately = false;
	UPROPERTY() FMWLobbyView Lobby;
	TWeakObjectPtr<APlayerController> OwnerController;
};

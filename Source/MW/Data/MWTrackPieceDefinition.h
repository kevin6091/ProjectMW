#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWTrackPieceDefinition.generated.h"

class UStaticMesh;
class AMWTrackOfferPreview;

// 트랙 종류별 표시 설정. 후보에는 이 객체 대신 카탈로그 조회용 태그를 전달한다.
UCLASS(BlueprintType)
class MW_API UMWTrackPieceDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
	// 트랙 정의 자체의 ID. 선택마다 다시 부여하는 OfferId와 구분한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Track")
    FName PieceId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Track")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Track")
    TObjectPtr<UStaticMesh> Mesh;

    // 선택판에 생성할 표현 클래스. 미지정 시 기본 Preview Actor를 사용한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview")
    TSubclassOf<AMWTrackOfferPreview> PreviewActorClass;

    // AuthoredMesh 표시용 상대 변환. 실제 코스 설치 위치나 소켓 정렬 결과가 아니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview")
    FTransform PreviewRelativeTransform;
};

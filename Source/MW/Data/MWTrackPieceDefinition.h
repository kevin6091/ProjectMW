#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWTrackPieceDefinition.generated.h"

class UStaticMesh;
class AMWTrackOfferPreview;

// 개별 트랙의 표시 설정. 같은 유형이어도 길이·곡률·외형이 다르면 별도 정의를 만든다.
// 각 정의는 MWAssetData에 고유 AssetId로 등록하고, 후보에는 그 조회 태그를 전달한다.
UCLASS(BlueprintType)
class MW_API UMWTrackPieceDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
	// 정의 내부 식별용 이름. 카탈로그 조회·선택 요청에는 FMWAssetEntry.AssetId를 사용한다.
	// 같은 유형의 변형도 이름을 구분한다. 예: Track.Curve90.Short / Track.Curve90.Wide.
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

#include "UI/Gameplay/MWWinnerVehicleShowcase.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
    struct FTemplateAttachment
    {
        const USceneComponent* Parent = nullptr;
        FName Socket;
    };

    TMap<const USceneComponent*, FTemplateAttachment> ReadTemplateAttachments(UClass& VehicleClass)
    {
        TMap<const USceneComponent*, FTemplateAttachment> Attachments;
        auto* BlueprintClass = Cast<UBlueprintGeneratedClass>(&VehicleClass);
        if (!BlueprintClass) return Attachments;

        TArray<const UBlueprintGeneratedClass*> Hierarchy;
        UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(BlueprintClass, Hierarchy);
        for (const UBlueprintGeneratedClass* Class : Hierarchy)
        {
            const USimpleConstructionScript* Script = Class->SimpleConstructionScript;
            if (!Script) continue;
            for (USCS_Node* Node : Script->GetAllNodes())
            {
                auto* Component = Cast<USceneComponent>(Node->GetActualComponentTemplate(BlueprintClass));
                if (!Component) continue;
                const USCS_Node* ParentNode = Script->FindParentNode(Node);
                const auto* Parent = ParentNode
                    ? Cast<USceneComponent>(ParentNode->GetActualComponentTemplate(BlueprintClass))
                    : Node->GetParentComponentTemplate(BlueprintClass);
                Attachments.Add(Component, {Parent, Node->AttachToName});
            }
        }
        return Attachments;
    }

    FTransform GetTemplateSocket(const USceneComponent& Parent, FName Socket)
    {
        if (Socket.IsNone()) return FTransform::Identity;
        if (const auto* Skeletal = Cast<USkeletalMeshComponent>(&Parent))
        {
            const USkeletalMesh* Mesh = Skeletal->GetSkeletalMeshAsset();
            if (!Mesh) return FTransform::Identity;
            const USkeletalMeshSocket* MeshSocket = Mesh->FindSocket(Socket);
            const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
            FTransform Transform = MeshSocket ? MeshSocket->GetSocketLocalTransform() : FTransform::Identity;
            for (int32 Index = Skeleton.FindBoneIndex(MeshSocket ? MeshSocket->BoneName : Socket);
                Index != INDEX_NONE; Index = Skeleton.GetParentIndex(Index))
            {
                Transform = Transform * Skeleton.GetRefBonePose()[Index];
            }
            return Transform;
        }
        return Parent.GetSocketTransform(Socket, RTS_Component);
    }

    FTransform GetTemplatePose(const USceneComponent& Part,
        const TMap<const USceneComponent*, FTemplateAttachment>& Attachments)
    {
        FTransform Pose = Part.GetRelativeTransform();
        const USceneComponent* Current = &Part;
        // SCS의 BP 부모와 네이티브 AttachParent는 같은 상대 변환 체인으로 합성한다.
        for (const auto* Link = Attachments.Find(Current); Current; Link = Attachments.Find(Current))
        {
            const FTemplateAttachment Attachment = Link && Link->Parent ? *Link
                : FTemplateAttachment{ Current->GetAttachParent(), Current->GetAttachSocketName() };
            Current = Attachment.Parent;
            if (Current) Pose = Pose * GetTemplateSocket(*Current, Attachment.Socket) * Current->GetRelativeTransform();
        }
        return Pose;
    }
}

bool FMWWinnerVehicleShot::IsValid() const
{
    if (Keys.Num() < 2 || !FMath::IsNearlyZero(Keys[0].Time) || !FMath::IsNearlyEqual(Keys.Last().Time, 1.f)
        || !FMath::IsFinite(LeadSeconds) || LeadSeconds <= 0.f)
    {
        return false;
    }
    float Previous = -1.f;
    for (const FMWWinnerVehicleKey& Key : Keys)
    {
        if (!FMath::IsFinite(Key.Time) || Key.Time <= Previous || !FMath::IsFinite(Key.WidthFraction)
            || Key.WidthFraction <= UE_SMALL_NUMBER || Key.ScreenAnchor.ContainsNaN() || Key.ViewRotation.ContainsNaN())
        {
            return false;
        }
        Previous = Key.Time;
    }
    return true;
}

AMWWinnerVehicleShowcase::AMWWinnerVehicleShowcase()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot")));
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    PrimaryActorTick.bCanEverTick = false;
}

bool AMWWinnerVehicleShowcase::Initialize(APlayerController& Controller, AMWVehiclePawn* Vehicle,
    TSubclassOf<AMWVehiclePawn> VehicleClass)
{
    if (!Controller.IsLocalController() || Controller.GetWorld() != GetWorld()) return false;
    if (Vehicle && Vehicle->GetWorld() == GetWorld())
    {
        const FTransform RacerTransform = Vehicle->GetActorTransform();
        TInlineComponentArray<UMeshComponent*> Meshes(Vehicle);
        for (UMeshComponent* Source : Meshes)
        {
            if (!Source->IsVisible() || Source->bHiddenInGame) continue;
            // 바퀴와 장식이 따로 움직여도 승자 Pawn의 현재 자세를 보존한다.
            AddVisual(*Source, Source->GetComponentTransform().GetRelativeTransform(RacerTransform),
                Cast<USkeletalMeshComponent>(Source));
        }
    }
    if (!ModelBounds.IsValid && VehicleClass)
    {
        // 원격 Pawn이 시야 밖에서 정리됐어도 확정 차량 BP의 메시 템플릿을 사용한다.
        const auto Attachments = ReadTemplateAttachments(*VehicleClass.Get());
        TArray<const UMeshComponent*> Templates;
        AActor::GetActorClassDefaultComponents(VehicleClass, Templates);
        for (const UMeshComponent* Template : Templates)
        {
            if (Template->IsVisible() && !Template->bHiddenInGame)
                AddVisual(*Template, GetTemplatePose(*Template, Attachments), nullptr);
        }
    }
    if (!ModelBounds.IsValid || ModelBounds.GetExtent().IsNearlyZero()) return false;

    Viewer = &Controller;
    if (Vehicle && !Controller.HiddenActors.Contains(Vehicle))
    {
        // 시상 중 이 뷰에서만 실제 경주차를 감춘다. 차량의 게임 상태는 계속 서버가 소유한다.
        Controller.HiddenActors.Add(Vehicle);
        RacerHiddenByShowcase = Vehicle;
    }
    return true;
}

void AMWWinnerVehicleShowcase::AddVisual(const UMeshComponent& Source, const FTransform& Part,
    USkeletalMeshComponent* LiveSkeletal)
{
    UMeshComponent* Visual = nullptr;
    if (const auto* Static = Cast<UStaticMeshComponent>(&Source); Static && Static->GetStaticMesh())
    {
        auto* Copy = NewObject<UStaticMeshComponent>(this);
        Copy->SetStaticMesh(Static->GetStaticMesh());
        Visual = Copy;
    }
    else if (const auto* Skeletal = Cast<USkeletalMeshComponent>(&Source); Skeletal && Skeletal->GetSkeletalMeshAsset())
    {
        auto* Copy = NewObject<UPoseableMeshComponent>(this);
        Copy->SetSkinnedAssetAndUpdate(Skeletal->GetSkeletalMeshAsset());
        Visual = Copy;
    }
    if (!Visual) return;
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetGenerateOverlapEvents(false);
    Visual->SetCanEverAffectNavigation(false);
    Visual->SetCastShadow(false);
    for (int32 Index = 0; Index < Source.GetNumMaterials(); ++Index) Visual->SetMaterial(Index, Source.GetMaterial(Index));
    Visual->SetupAttachment(GetRootComponent());
    Visual->SetRelativeTransform(Part);
    AddInstanceComponent(Visual);
    Visual->RegisterComponent();
    if (LiveSkeletal) CastChecked<UPoseableMeshComponent>(Visual)->CopyPoseFromSkeletalComponent(LiveSkeletal);
    ModelBounds += Visual->CalcBounds(Part).GetBox();
}

bool AMWWinnerVehicleShowcase::Present(const FMWWinnerVehicleShot& Shot, float Alpha)
{
    const APlayerController* Controller = Viewer.Get();
    if (!Controller || !Controller->PlayerCameraManager) return false;
    int32 Width = 0;
    int32 Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return false;

    const int32 NextIndex = Shot.Keys.IndexOfByPredicate([Alpha](const FMWWinnerVehicleKey& Key) { return Key.Time >= Alpha; });
    const int32 End = FMath::Clamp(NextIndex == INDEX_NONE ? Shot.Keys.Num() - 1 : NextIndex, 1, Shot.Keys.Num() - 1);
    const FMWWinnerVehicleKey& From = Shot.Keys[End - 1];
    const FMWWinnerVehicleKey& To = Shot.Keys[End];
    const float Amount = FMath::SmoothStep(From.Time, To.Time, Alpha);
    const FVector2D Anchor = FMath::Lerp(From.ScreenAnchor, To.ScreenAnchor, Amount);
    FVector RayOrigin, RayDirection;
    if (!Controller->DeprojectScreenPositionToWorld(Anchor.X * Width, Anchor.Y * Height, RayOrigin, RayDirection)) return false;

    const auto& View = Controller->PlayerCameraManager->GetCameraCacheView();
    const FQuat Rotation = View.Rotation.Quaternion() * FQuat::Slerp(From.ViewRotation.Quaternion(), To.ViewRotation.Quaternion(), Amount);
    const double ProjectedWidth = FMath::Lerp(From.WidthFraction, To.WidthFraction, Amount);
    const double HalfFovTangent = FMath::Tan(FMath::DegreesToRadians(View.FOV * .5));
    const double Facing = FVector::DotProduct(RayDirection, View.Rotation.Vector());
    if (HalfFovTangent <= UE_SMALL_NUMBER || Facing <= UE_SMALL_NUMBER) return false;
    // 외형의 원래 크기를 유지하고 카메라까지의 거리를 바꿔 화면에서 다가오게 한다.
    const double Distance = ModelBounds.GetExtent().Size() / (HalfFovTangent * ProjectedWidth * Facing);
    const FVector Center = View.Location + RayDirection * Distance;
    SetActorLocationAndRotation(Center - Rotation.RotateVector(ModelBounds.GetCenter()), Rotation);
    SetActorHiddenInGame(false);
    UpdateTrail(Shot, Alpha);
    return true;
}

void AMWWinnerVehicleShowcase::UpdateTrail(const FMWWinnerVehicleShot& Shot, float Alpha)
{
    if (!bTrailStarted && Shot.Trail && Alpha >= Shot.TrailTime.X && Alpha < Shot.TrailTime.Y)
    {
        bTrailStarted = true;
        const FVector Tail(ModelBounds.Min.X, ModelBounds.GetCenter().Y, ModelBounds.GetCenter().Z);
        ActiveTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(Shot.Trail, GetRootComponent(), NAME_None,
            Tail, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
    }
    if (Alpha >= Shot.TrailTime.Y)
    {
        if (UNiagaraComponent* Trail = ActiveTrail.Get(); Trail && Trail->IsActive())
        {
            Trail->Deactivate();
        }
    }
}

void AMWWinnerVehicleShowcase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (APlayerController* Controller = Viewer.Get(); Controller && RacerHiddenByShowcase.IsValid())
    {
        Controller->HiddenActors.RemoveSingle(RacerHiddenByShowcase.Get());
    }
    if (UNiagaraComponent* Trail = ActiveTrail.Get()) Trail->DestroyComponent();
    Super::EndPlay(EndPlayReason);
}

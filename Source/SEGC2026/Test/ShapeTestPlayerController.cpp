#include "Test/ShapeTestPlayerController.h"
#include "UI/SStrokeCanvas.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/SpectatorPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogShapeTest, Log, All);

AShapeTestPlayerController::AShapeTestPlayerController()
{
	bShowMouseCursor = true;
}

void AShapeTestPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!IsLocalController() || !Viewport)
	{
		return;
	}

	Canvas = SNew(SStrokeCanvas)
		.OnShapeResult(FOnShapeResultNative::CreateUObject(this, &AShapeTestPlayerController::HandleShape));
	Viewport->AddViewportWidgetContent(Canvas.ToSharedRef(), 100);

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Canvas);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void AShapeTestPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Canvas.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Canvas.ToSharedRef());
		}
		Canvas.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AShapeTestPlayerController::HandleShape(const FShapeResult& Result)
{
	UE_LOG(LogShapeTest, Log, TEXT("shape=%d conf=%.2f closure=%.2f circle=%.2f tri=%.2f dir=(%.2f, %.2f) [%s]"),
		(int32)Result.Shape, Result.Confidence, Result.ClosureRatio, Result.CircleScore, Result.TriangleScore,
		Result.Direction.X, Result.Direction.Y, *Result.Reason);
}

AShapeTestGameMode::AShapeTestGameMode()
{
	PlayerControllerClass = AShapeTestPlayerController::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

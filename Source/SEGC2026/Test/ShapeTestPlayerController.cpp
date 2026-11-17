#include "Test/ShapeTestPlayerController.h"
#include "UI/SStrokeCanvas.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/SpectatorPawn.h"
#include "Blueprint/WidgetLayoutLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogShapeTest, Log, All);

namespace
{
	/** デモ用: 下から始まる記号を、下をくぐる U 字のつなぎ線でつないだ一筆書き（円・三角・星・ハート） */
	TArray<FVector2D> MakeDemoStroke(const FVector2D& Size)
	{
		const double R = FMath::Min(Size.X / 9.0, Size.Y / 5.0);
		const double Y = Size.Y * 0.45;
		TArray<FVector2D> Out;

		auto Connect = [&Out](const FVector2D& B)
		{
			if (Out.Num() == 0) { return; }
			const FVector2D A = Out.Last();
			const double Yl = FMath::Max(A.Y, B.Y) + 40.0;
			const FVector2D Way[] = { A, FVector2D(A.X, Yl), FVector2D(B.X, Yl), B };
			for (int32 w = 0; w < 3; ++w)
			{
				for (int32 i = 1; i <= 20; ++i) { Out.Add(FMath::Lerp(Way[w], Way[w + 1], i / 20.0)); }
			}
		};
		auto AddPolygon = [&](TArray<FVector2D> V)
		{
			int32 Bottom = 0;
			for (int32 i = 1; i < V.Num(); ++i) { if (V[i].Y > V[Bottom].Y) { Bottom = i; } }
			TArray<FVector2D> Ring;
			for (int32 i = 0; i < V.Num(); ++i)
			{
				const FVector2D A = V[(Bottom + i) % V.Num()], B = V[(Bottom + i + 1) % V.Num()];
				for (int32 k = 0; k < 20; ++k) { Ring.Add(FMath::Lerp(A, B, k / 20.0)); }
			}
			Connect(Ring[0]);
			Out.Append(Ring);
			Out.Add(Ring[0]);
		};

		// 円
		{
			const FVector2D C(Size.X * 0.14, Y);
			TArray<FVector2D> Pts;
			for (int32 i = 0; i <= 64; ++i)
			{
				const double A = UE_DOUBLE_PI * 0.5 + 2.0 * UE_DOUBLE_PI * 1.02 * i / 64;
				Pts.Add(C + FVector2D(R * 0.9 * FMath::Cos(A), R * 0.9 * FMath::Sin(A)));
			}
			Connect(Pts[0]);
			Out.Append(Pts);
		}
		// 三角形（右向き）
		{
			const FVector2D C(Size.X * 0.38, Y);
			AddPolygon({ C + FVector2D(-R * 0.6, -R), C + FVector2D(-R * 0.6, R), C + FVector2D(R, 0) });
		}
		// 星
		{
			const FVector2D C(Size.X * 0.62, Y);
			TArray<FVector2D> V;
			for (int32 i = 0; i < 10; ++i)
			{
				const double A = FMath::DegreesToRadians(-90.0 + 36.0 * i);
				const double Rd = (i % 2 == 0) ? R : R * 0.45;
				V.Add(C + FVector2D(Rd * FMath::Cos(A), Rd * FMath::Sin(A)));
			}
			AddPolygon(V);
		}
		// ハート
		{
			const FVector2D C(Size.X * 0.86, Y);
			TArray<FVector2D> Pts;
			for (int32 i = 0; i <= 100; ++i)
			{
				const double T = 2.0 * UE_DOUBLE_PI * (0.5 + (double)i / 100);
				const double S = FMath::Sin(T);
				const double X = 16.0 * S * S * S;
				const double Yh = -(13.0 * FMath::Cos(T) - 5.0 * FMath::Cos(2 * T) - 2.0 * FMath::Cos(3 * T) - FMath::Cos(4 * T));
				Pts.Add(C + FVector2D(X, Yh) * (R / 16.0));
			}
			Connect(Pts[0]);
			Out.Append(Pts);
		}
		return Out;
	}
}

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
		.OnStrokeResult(FOnStrokeResultNative::CreateUObject(this, &AShapeTestPlayerController::HandleStroke));
	Viewport->AddViewportWidgetContent(Canvas.ToSharedRef(), 100);

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Canvas);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	// 起動オプション -ShapeDemo があれば、デモの一筆書き（円・三角・星・ハート）を自動で流し込む
	if (FParse::Param(FCommandLine::Get(), TEXT("ShapeDemo")))
	{
		int32 W = 0, H = 0;
		GetViewportSize(W, H);
		// キャンバスのローカル座標は、ビューポートのピクセル数を DPI スケールで割ったもの
		const float Dpi = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.1f);
		const FVector2D Size((W > 0 ? W : 1280) / Dpi, (H > 0 ? H : 720) / Dpi);
		Canvas->SetStrokeAndRecognize(MakeDemoStroke(Size));
	}
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

void AShapeTestPlayerController::HandleStroke(const FStrokeResult& Result)
{
	UE_LOG(LogShapeTest, Log, TEXT("stroke valid=%d symbols=%d [%s]"), Result.bValid ? 1 : 0, Result.Symbols.Num(), *Result.Reason);
	for (int32 i = 0; i < Result.Symbols.Num(); ++i)
	{
		const FShapeResult& S = Result.Symbols[i];
		UE_LOG(LogShapeTest, Log, TEXT("  #%d %s conf=%.2f center=(%.0f, %.0f) r=%.0f dir=(%.2f, %.2f) [%s]"),
			i + 1, GetShapeName(S.Shape), S.Confidence, S.Center.X, S.Center.Y, S.Radius, S.Direction.X, S.Direction.Y, *S.Reason);
	}
}

AShapeTestGameMode::AShapeTestGameMode()
{
	PlayerControllerClass = AShapeTestPlayerController::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

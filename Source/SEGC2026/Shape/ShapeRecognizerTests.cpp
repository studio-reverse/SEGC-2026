#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "Shape/ShapeRecognizer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ShapeTestUtil
{
	/** 手描きっぽい円。SweepDeg < 360 で少し閉じ切らない */
	static TArray<FVector2D> MakeCircle(float Radius, float StartDeg, float SweepDeg, float Jitter, int32 Seed)
	{
		FRandomStream Rand(Seed);
		TArray<FVector2D> Pts;
		const int32 N = 80;
		for (int32 i = 0; i <= N; ++i)
		{
			const float A = FMath::DegreesToRadians(StartDeg + SweepDeg * i / N);
			const float R = Radius + Rand.FRandRange(-Jitter, Jitter);
			Pts.Add(FVector2D(300.0 + R * FMath::Cos(A), 300.0 + R * FMath::Sin(A)));
		}
		return Pts;
	}

	/** 頂点列(閉じる)に沿ってサンプリング */
	static TArray<FVector2D> MakePolygon(const TArray<FVector2D>& Verts, float Jitter, int32 Seed)
	{
		FRandomStream Rand(Seed);
		TArray<FVector2D> Pts;
		for (int32 e = 0; e < Verts.Num(); ++e)
		{
			const FVector2D A = Verts[e], B = Verts[(e + 1) % Verts.Num()];
			for (int32 i = 0; i < 30; ++i)
			{
				const FVector2D P = FMath::Lerp(A, B, i / 30.0);
				Pts.Add(P + FVector2D(Rand.FRandRange(-Jitter, Jitter), Rand.FRandRange(-Jitter, Jitter)));
			}
		}
		Pts.Add(Verts[0]);
		return Pts;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeCircleTest, "SEGC2026.Shape.Circle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeCircleTest::RunTest(const FString& Parameters)
{
	for (int32 Seed = 1; Seed <= 5; ++Seed)
	{
		// 開始角・掃引角・ジッタを変えて複数回（掃引 350° は少し閉じ切らない）
		const TArray<FVector2D> Pts = ShapeTestUtil::MakeCircle(100.f, Seed * 70.f, 350.f, 4.f, Seed);
		const FShapeResult R = FShapeRecognizer::Recognize(Pts);
		TestEqual(*FString::Printf(TEXT("circle seed %d (%s)"), Seed, *R.Reason), R.Shape, EShapeType::Circle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeTriangleTest, "SEGC2026.Shape.Triangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeTriangleTest::RunTest(const FString& Parameters)
{
	// 右向き ▷（二等辺: 先端が右）
	const TArray<FVector2D> Right = { FVector2D(200, 100), FVector2D(200, 300), FVector2D(450, 200) };
	for (int32 Seed = 1; Seed <= 5; ++Seed)
	{
		const FShapeResult R = FShapeRecognizer::Recognize(ShapeTestUtil::MakePolygon(Right, 3.f, Seed));
		TestEqual(*FString::Printf(TEXT("triangle seed %d (%s)"), Seed, *R.Reason), R.Shape, EShapeType::Triangle);
		TestTrue(TEXT("apex points right"), R.Direction.X > 0.8);
	}

	// 上向き △（回転しても Triangle として認識される）
	const TArray<FVector2D> Up = { FVector2D(300, 100), FVector2D(200, 350), FVector2D(400, 350) };
	const FShapeResult RUp = FShapeRecognizer::Recognize(ShapeTestUtil::MakePolygon(Up, 3.f, 9));
	TestEqual(TEXT("triangle (up)"), RUp.Shape, EShapeType::Triangle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeRejectTest, "SEGC2026.Shape.Reject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeRejectTest::RunTest(const FString& Parameters)
{
	// 直線（閉じていない）
	TArray<FVector2D> Line;
	for (int32 i = 0; i < 50; ++i) { Line.Add(FVector2D(100 + i * 5, 200 + i)); }
	TestEqual(TEXT("line"), FShapeRecognizer::Recognize(Line).Shape, EShapeType::Unknown);

	// 半円（閉じていない）
	TestEqual(TEXT("half circle"),
		FShapeRecognizer::Recognize(ShapeTestUtil::MakeCircle(100.f, 0.f, 180.f, 2.f, 1)).Shape, EShapeType::Unknown);

	// 点が少なすぎる
	TArray<FVector2D> Few = { FVector2D(0, 0), FVector2D(10, 10) };
	TestEqual(TEXT("few points"), FShapeRecognizer::Recognize(Few).Shape, EShapeType::Unknown);

	// 小さすぎる円
	TestEqual(TEXT("tiny circle"),
		FShapeRecognizer::Recognize(ShapeTestUtil::MakeCircle(5.f, 0.f, 350.f, 0.f, 1)).Shape, EShapeType::Unknown);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

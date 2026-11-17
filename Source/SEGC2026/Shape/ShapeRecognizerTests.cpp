#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "Shape/ShapeRecognizer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ShapeTestUtil
{
	using FPts = TArray<FVector2D>;

	/** 円。StartDeg=90 で真下から始める（画面座標では +Y が下）。SweepDeg > 360 で少し行き過ぎる */
	static FPts Circle(const FVector2D& C, float Radius, float StartDeg, float SweepDeg, float Jitter, int32 Seed)
	{
		FRandomStream Rand(Seed);
		FPts Pts;
		const int32 N = 96;
		for (int32 i = 0; i <= N; ++i)
		{
			const float A = FMath::DegreesToRadians(StartDeg + SweepDeg * i / N);
			const float R = Radius + Rand.FRandRange(-Jitter, Jitter);
			Pts.Add(C + FVector2D(R * FMath::Cos(A), R * FMath::Sin(A)));
		}
		return Pts;
	}

	/** 最も下（Y 最大）の頂点が先頭になるよう、頂点列を回す */
	static FPts RotateToBottom(const FPts& Verts)
	{
		int32 Bottom = 0;
		for (int32 i = 1; i < Verts.Num(); ++i)
		{
			if (Verts[i].Y > Verts[Bottom].Y) { Bottom = i; }
		}
		FPts Out;
		for (int32 i = 0; i < Verts.Num(); ++i) { Out.Add(Verts[(Bottom + i) % Verts.Num()]); }
		return Out;
	}

	/** 頂点列（閉じる）に沿ってサンプリング。SmoothPasses > 0 で角を丸める（手描きっぽくなる） */
	static FPts Polygon(const FPts& Verts, float Jitter, int32 Seed, int32 SmoothPasses = 0, int32 StartShift = 0)
	{
		FRandomStream Rand(Seed);
		FPts Ring;
		for (int32 e = 0; e < Verts.Num(); ++e)
		{
			const FVector2D A = Verts[e], B = Verts[(e + 1) % Verts.Num()];
			for (int32 i = 0; i < 30; ++i) { Ring.Add(FMath::Lerp(A, B, i / 30.0)); }
		}
		if (StartShift != 0)
		{
			FPts Shifted;
			for (int32 i = 0; i < Ring.Num(); ++i) { Shifted.Add(Ring[(i + StartShift) % Ring.Num()]); }
			Ring = Shifted;
		}
		for (int32 Pass = 0; Pass < SmoothPasses; ++Pass)
		{
			FPts T = Ring;
			for (int32 i = 0; i < Ring.Num(); ++i)
			{
				T[i] = (Ring[(i - 1 + Ring.Num()) % Ring.Num()] + Ring[i] + Ring[(i + 1) % Ring.Num()]) / 3.0;
			}
			Ring = T;
		}
		FPts Pts;
		for (const FVector2D& P : Ring)
		{
			Pts.Add(P + FVector2D(Rand.FRandRange(-Jitter, Jitter), Rand.FRandRange(-Jitter, Jitter)));
		}
		const FVector2D First = Pts[0];
		Pts.Add(First);
		return Pts;
	}

	static FPts RegularPolygonVerts(const FVector2D& C, float R, int32 K, float RotDeg)
	{
		FPts V;
		for (int32 i = 0; i < K; ++i)
		{
			const float A = FMath::DegreesToRadians(-90.f + RotDeg + 360.f * i / K);
			V.Add(C + FVector2D(R * FMath::Cos(A), R * FMath::Sin(A)));
		}
		return V;
	}

	/** 輪郭の星（外 R・内 r の 10 頂点） */
	static FPts StarOutlineVerts(const FVector2D& C, float R, float r)
	{
		FPts V;
		for (int32 i = 0; i < 10; ++i)
		{
			const float A = FMath::DegreesToRadians(-90.f + 36.f * i);
			const float Rd = (i % 2 == 0) ? R : r;
			V.Add(C + FVector2D(Rd * FMath::Cos(A), Rd * FMath::Sin(A)));
		}
		return V;
	}

	/** 五芒星（一筆書きの星）: 先端を 1 つ飛ばしで結ぶ */
	static FPts PentagramVerts(const FVector2D& C, float R)
	{
		const FPts T = RegularPolygonVerts(C, R, 5, 0.f);
		return { T[0], T[2], T[4], T[1], T[3] };
	}

	/** ハート。真下の先端から始まる。StartFrac で開始位置をずらす（0..1） */
	static FPts Heart(const FVector2D& C, float Scale, float StartFrac, float Jitter, int32 Seed)
	{
		FRandomStream Rand(Seed);
		FPts Pts;
		const int32 N = 120;
		for (int32 i = 0; i <= N; ++i)
		{
			const double T = 2.0 * UE_DOUBLE_PI * (0.5 + StartFrac + (double)i / N);
			const double S = FMath::Sin(T);
			const double X = 16.0 * S * S * S;
			const double Y = -(13.0 * FMath::Cos(T) - 5.0 * FMath::Cos(2 * T) - 2.0 * FMath::Cos(3 * T) - FMath::Cos(4 * T));
			Pts.Add(C + FVector2D(X, Y) * Scale + FVector2D(Rand.FRandRange(-Jitter, Jitter), Rand.FRandRange(-Jitter, Jitter)));
		}
		return Pts;
	}

	static FPts Line(const FVector2D& A, const FVector2D& B, int32 Count = 24)
	{
		FPts Pts;
		for (int32 i = 1; i <= Count; ++i) { Pts.Add(FMath::Lerp(A, B, (double)i / Count)); }
		return Pts;
	}

	/** つなぎ線を引いてから記号を足す。つなぎ線は記号の下をくぐる U 字（記号の始点・終点は下側） */
	static void AppendSymbol(FPts& Out, const FPts& Symbol, bool bStraightConnector = false)
	{
		if (Out.Num() > 0)
		{
			const FVector2D A = Out.Last();
			const FVector2D B = Symbol[0];
			if (bStraightConnector)
			{
				Out.Append(Line(A, B, 40));
			}
			else
			{
				const double Y = FMath::Max(A.Y, B.Y) + 50.0;
				Out.Append(Line(A, FVector2D(A.X, Y)));
				Out.Append(Line(FVector2D(A.X, Y), FVector2D(B.X, Y), 40));
				Out.Append(Line(FVector2D(B.X, Y), B));
			}
		}
		Out.Append(Symbol);
	}

	/** 下から始まる各記号（スタンドアロンの手描き風） */
	static FPts SymCircle(const FVector2D& C, float R, int32 Seed) { return Circle(C, R, 90.f, 365.f, 2.f, Seed); }
	static FPts SymSquare(const FVector2D& C, float H, int32 Seed)
	{
		return Polygon(RotateToBottom({ C + FVector2D(-H, -H), C + FVector2D(H, -H), C + FVector2D(H, H), C + FVector2D(-H, H) }), 1.5f, Seed, 3);
	}
	static FPts SymTriangle(const FVector2D& C, float R, int32 Seed)
	{
		return Polygon(RotateToBottom(RegularPolygonVerts(C, R, 3, 0.f)), 1.5f, Seed, 3);
	}
	static FPts SymStar(const FVector2D& C, float R, int32 Seed)
	{
		return Polygon(RotateToBottom(StarOutlineVerts(C, R, R * 0.45f)), 1.0f, Seed, 1);
	}

	static FString Describe(const FStrokeResult& R)
	{
		FString S = FString::Printf(TEXT("valid=%d n=%d [%s]"), R.bValid ? 1 : 0, R.Symbols.Num(), *R.Reason);
		for (const FShapeResult& Sym : R.Symbols)
		{
			S += FString::Printf(TEXT(" {%s %.2f: %s}"), GetShapeName(Sym.Shape), Sym.Confidence, *Sym.Reason);
		}
		return S;
	}

	static void ExpectSingle(FAutomationTestBase& T, const FString& Label, const FPts& Pts, EShapeType Expected)
	{
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		const bool bOk = R.bValid && R.Symbols.Num() == 1 && R.Symbols[0].Shape == Expected;
		T.TestTrue(FString::Printf(TEXT("%s: expected %s, got %s"), *Label, GetShapeName(Expected), *Describe(R)), bOk);
	}

	static void ExpectSequence(FAutomationTestBase& T, const FString& Label, const FPts& Pts, const TArray<EShapeType>& Expected)
	{
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		bool bOk = R.bValid && R.Symbols.Num() == Expected.Num();
		for (int32 i = 0; bOk && i < Expected.Num(); ++i) { bOk = R.Symbols[i].Shape == Expected[i]; }
		T.TestTrue(FString::Printf(TEXT("%s: %s"), *Label, *Describe(R)), bOk);
	}
}

using namespace ShapeTestUtil;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeCircleTest, "SEGC2026.Shape.Circle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeCircleTest::RunTest(const FString& Parameters)
{
	for (int32 Seed = 1; Seed <= 5; ++Seed)
	{
		// 開始角・掃引角・ジッタを変えて複数回（掃引 350° は少し閉じ切らない）
		ExpectSingle(*this, FString::Printf(TEXT("circle seed %d"), Seed),
			Circle(FVector2D(300, 300), 100.f, Seed * 70.f, 350.f, 4.f, Seed), EShapeType::Circle);
	}
	ExpectSingle(*this, TEXT("circle overshoot"), Circle(FVector2D(300, 300), 100.f, 30.f, 372.f, 3.f, 9), EShapeType::Circle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeTriangleTest, "SEGC2026.Shape.Triangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeTriangleTest::RunTest(const FString& Parameters)
{
	// 右向き ▷（二等辺: 先端が右）
	const FPts Right = { FVector2D(200, 100), FVector2D(200, 300), FVector2D(450, 200) };
	for (int32 Seed = 1; Seed <= 5; ++Seed)
	{
		const FPts Pts = Polygon(Right, 3.f, Seed);
		ExpectSingle(*this, FString::Printf(TEXT("triangle seed %d"), Seed), Pts, EShapeType::Triangle);
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		if (R.Symbols.Num() == 1) { TestTrue(TEXT("apex points right"), R.Symbols[0].Direction.X > 0.8); }
	}

	// 上向き △ と、角が丸い三角形
	ExpectSingle(*this, TEXT("triangle up"), Polygon({ FVector2D(300, 100), FVector2D(200, 350), FVector2D(400, 350) }, 3.f, 9), EShapeType::Triangle);
	ExpectSingle(*this, TEXT("triangle rounded"), Polygon(Right, 1.5f, 4, 3), EShapeType::Triangle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapePolygonTest, "SEGC2026.Shape.Polygon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapePolygonTest::RunTest(const FString& Parameters)
{
	const FVector2D C(300, 300);
	for (int32 Seed = 1; Seed <= 4; ++Seed)
	{
		// 軸に沿った四角形（角は丸め）、長方形、斜めの四角形（◇）
		ExpectSingle(*this, FString::Printf(TEXT("square %d"), Seed),
			Polygon({ FVector2D(200, 200), FVector2D(400, 200), FVector2D(400, 400), FVector2D(200, 400) }, 2.f, Seed, 3), EShapeType::Square);
		ExpectSingle(*this, FString::Printf(TEXT("rectangle %d"), Seed),
			Polygon({ FVector2D(150, 220), FVector2D(450, 220), FVector2D(450, 380), FVector2D(150, 380) }, 2.f, Seed, 3), EShapeType::Square);
		// 45 度傾けた四角形（◇）も、四角形として認識される
		ExpectSingle(*this, FString::Printf(TEXT("tilted square %d"), Seed),
			Polygon({ FVector2D(300, 150), FVector2D(450, 300), FVector2D(300, 450), FVector2D(150, 300) }, 2.f, Seed, 3), EShapeType::Square);
		ExpectSingle(*this, FString::Printf(TEXT("pentagon %d"), Seed),
			Polygon(RegularPolygonVerts(C, 150.f, 5, 0.f), 2.f, Seed, 3), EShapeType::Pentagon);
	}
	ExpectSingle(*this, TEXT("pentagon rotated"), Polygon(RegularPolygonVerts(C, 150.f, 5, 30.f), 2.f, 7, 3), EShapeType::Pentagon);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeStarHeartTest, "SEGC2026.Shape.StarHeart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeStarHeartTest::RunTest(const FString& Parameters)
{
	const FVector2D C(300, 300);
	for (int32 Seed = 1; Seed <= 3; ++Seed)
	{
		ExpectSingle(*this, FString::Printf(TEXT("star outline %d"), Seed),
			Polygon(StarOutlineVerts(C, 160.f, 70.f), 1.5f, Seed, 1), EShapeType::Star);
		ExpectSingle(*this, FString::Printf(TEXT("pentagram %d"), Seed),
			Polygon(PentagramVerts(C, 160.f), 1.5f, Seed, 1), EShapeType::Star);
		// ハート: 開始位置を変える
		ExpectSingle(*this, FString::Printf(TEXT("heart %d"), Seed),
			Heart(C, 7.f, Seed * 0.23f, 2.f, Seed), EShapeType::Heart);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeRejectTest, "SEGC2026.Shape.Reject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeRejectTest::RunTest(const FString& Parameters)
{
	// 直線（閉じていない）
	FPts Line;
	for (int32 i = 0; i < 50; ++i) { Line.Add(FVector2D(100 + i * 5, 200 + i)); }
	TestFalse(TEXT("line"), FShapeRecognizer::RecognizeStroke(Line).bValid);

	// 半円（閉じていない）
	TestFalse(TEXT("half circle"), FShapeRecognizer::RecognizeStroke(Circle(FVector2D(300, 300), 100.f, 0.f, 180.f, 2.f, 1)).bValid);

	// 点が少なすぎる
	const FPts Few = { FVector2D(0, 0), FVector2D(10, 10) };
	TestFalse(TEXT("few points"), FShapeRecognizer::RecognizeStroke(Few).bValid);

	// 小さすぎる円
	TestFalse(TEXT("tiny circle"), FShapeRecognizer::RecognizeStroke(Circle(FVector2D(300, 300), 5.f, 0.f, 360.f, 0.f, 1)).bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeMultiTest, "SEGC2026.Shape.Multi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeMultiTest::RunTest(const FString& Parameters)
{
	// 円 → 四角 の一筆書き
	{
		FPts Pts;
		AppendSymbol(Pts, SymCircle(FVector2D(150, 300), 60.f, 1));
		AppendSymbol(Pts, SymSquare(FVector2D(400, 300), 60.f, 2));
		ExpectSequence(*this, TEXT("circle,square"), Pts, { EShapeType::Circle, EShapeType::Square });
	}
	// 三角 → 円 → 星
	{
		FPts Pts;
		AppendSymbol(Pts, SymTriangle(FVector2D(150, 300), 80.f, 3));
		AppendSymbol(Pts, SymCircle(FVector2D(400, 300), 60.f, 4));
		AppendSymbol(Pts, SymStar(FVector2D(660, 300), 90.f, 5));
		ExpectSequence(*this, TEXT("triangle,circle,star"), Pts, { EShapeType::Triangle, EShapeType::Circle, EShapeType::Star });
	}
	// 五芒星は 1 つの星として認識され、先端の小さな三角形に分割されない
	{
		FPts Pts = Polygon(PentagramVerts(FVector2D(300, 300), 160.f), 1.5f, 6, 1);
		ExpectSingle(*this, TEXT("pentagram is one star"), Pts, EShapeType::Star);
	}
	// 四角 → ハート
	{
		FPts Pts;
		AppendSymbol(Pts, SymSquare(FVector2D(150, 300), 60.f, 7));
		AppendSymbol(Pts, Heart(FVector2D(420, 300), 6.f, 0.f, 1.5f, 8));
		ExpectSequence(*this, TEXT("square,heart"), Pts, { EShapeType::Square, EShapeType::Heart });
	}
	return true;
}

namespace ShapeTestUtil
{
	/** 手描きらしい強い歪み（拡大縮小・縦横比・頂点のぶれ・角の丸め・開始位置・行き過ぎ/閉じ残し）をかけた図形 */
	/** bVariant: Star なら五芒星（一筆書きの星）、Square なら 45 度傾けた四角形 */
	static FPts HandDraw(EShapeType Shape, int32 Seed, bool bVariant,
		const FVector2D& C = FVector2D(300, 300), bool bBottomStart = false)
	{
		FRandomStream Rand(Seed);
		const float Scale = Rand.FRandRange(0.7f, 1.3f);
		const float Aspect = Rand.FRandRange(0.8f, 1.25f);
		const float R = 120.f * Scale;

		if (Shape == EShapeType::Circle)
		{
			FPts Pts;
			const int32 N = 110;
			const float Sweep = Rand.FRandRange(345.f, 378.f);
			const float Start = bBottomStart ? 90.f + Rand.FRandRange(-15.f, 15.f) : Rand.FRandRange(0.f, 360.f);
			const float Ph1 = Rand.FRandRange(0.f, 6.28f), Ph2 = Rand.FRandRange(0.f, 6.28f);
			for (int32 i = 0; i <= N; ++i)
			{
				const float A = FMath::DegreesToRadians(Start + Sweep * i / N);
				const float Wob = 1.f + 0.05f * FMath::Sin(2.f * A + Ph1) + 0.035f * FMath::Sin(3.f * A + Ph2);
				const float Rd = R * Wob + Rand.FRandRange(-2.5f, 2.5f);
				Pts.Add(C + FVector2D(Rd * Aspect * FMath::Cos(A), Rd * FMath::Sin(A)));
			}
			return Pts;
		}
		if (Shape == EShapeType::Heart)
		{
			const float Tilt = FMath::DegreesToRadians(Rand.FRandRange(-12.f, 12.f));
			FPts Pts = Heart(FVector2D::ZeroVector, R / 16.f, bBottomStart ? Rand.FRandRange(-0.04f, 0.04f) : Rand.FRand(), 2.5f, Seed);
			for (FVector2D& P : Pts)
			{
				const FVector2D Q(P.X * Aspect, P.Y);
				P = C + FVector2D(Q.X * FMath::Cos(Tilt) - Q.Y * FMath::Sin(Tilt), Q.X * FMath::Sin(Tilt) + Q.Y * FMath::Cos(Tilt));
			}
			return Pts;
		}

		FPts Verts;
		switch (Shape)
		{
		case EShapeType::Triangle: Verts = RegularPolygonVerts(C, R, 3, Rand.FRandRange(0.f, 360.f)); break;
		case EShapeType::Square:   Verts = RegularPolygonVerts(C, R, 4, (bVariant ? 0.f : 45.f) + Rand.FRandRange(-10.f, 10.f)); break;
		case EShapeType::Pentagon: Verts = RegularPolygonVerts(C, R, 5, Rand.FRandRange(0.f, 72.f)); break;
		case EShapeType::Star:
		{
			const float Rot = Rand.FRandRange(0.f, 72.f);
			Verts = bVariant ? PentagramVerts(C, R) : StarOutlineVerts(C, R, R * 0.45f);
			const FVector2D Pivot = C;
			for (FVector2D& V : Verts)
			{
				const FVector2D D = V - Pivot;
				const float A = FMath::DegreesToRadians(Rot);
				V = Pivot + FVector2D(D.X * FMath::Cos(A) - D.Y * FMath::Sin(A), D.X * FMath::Sin(A) + D.Y * FMath::Cos(A));
			}
			break;
		}
		default: break;
		}
		for (FVector2D& V : Verts)
		{
			V = C + FVector2D((V.X - C.X) * Aspect, V.Y - C.Y);
			V += FVector2D(Rand.FRandRange(-0.05f, 0.05f), Rand.FRandRange(-0.05f, 0.05f)) * R;
		}
		const int32 Passes = (Shape == EShapeType::Star) ? 1 : 3;
		if (bBottomStart)
		{
			return Polygon(RotateToBottom(Verts), 2.5f, Seed, Passes, Rand.RandRange(0, 4));
		}
		return Polygon(Verts, 2.5f, Seed, Passes, Rand.RandRange(0, 29 * Verts.Num()));
	}
}

namespace ShapeTestUtil
{
	/** マウスを速く動かしたときのように、点を間引く（間隔は Spacing の 0.5〜1.5 倍でばらつく）。最初と最後は残す */
	static FPts Sparse(const FPts& In, float Spacing, int32 Seed)
	{
		FRandomStream Rand(Seed);
		FPts Out;
		if (In.Num() == 0) { return Out; }
		Out.Add(In[0]);
		double Acc = 0.0;
		double Next = Spacing * Rand.FRandRange(0.5f, 1.5f);
		for (int32 i = 1; i < In.Num(); ++i)
		{
			Acc += FVector2D::Distance(In[i - 1], In[i]);
			if (Acc >= Next)
			{
				Out.Add(In[i]);
				Acc = 0.0;
				Next = Spacing * Rand.FRandRange(0.5f, 1.5f);
			}
		}
		if (FVector2D::Distance(Out.Last(), In.Last()) > 0.5) { Out.Add(In.Last()); }
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeSparseInputTest, "SEGC2026.Shape.SparseInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeSparseInputTest::RunTest(const FString& Parameters)
{
	// 速くドラッグすると、マウスイベントの間隔が空いて、点が疎になる（数十 px おき）
	struct FCase { const TCHAR* Name; EShapeType Shape; bool bVariant; };
	const FCase Cases[] = {
		{ TEXT("circle"),   EShapeType::Circle,   false },
		{ TEXT("triangle"), EShapeType::Triangle, false },
		{ TEXT("square"),   EShapeType::Square,   false },
		{ TEXT("pentagon"), EShapeType::Pentagon, false },
		{ TEXT("star"),     EShapeType::Star,     false },
		{ TEXT("pentagram"),EShapeType::Star,     true  },
		{ TEXT("heart"),    EShapeType::Heart,    false },
	};
	const float Spacings[] = { 15.f, 30.f, 60.f };
	const int32 Trials = 20;
	for (float Spacing : Spacings)
	{
		FString Row = FString::Printf(TEXT("spacing %.0fpx: "), Spacing);
		int32 Total = 0;
		for (const FCase& Case : Cases)
		{
			int32 Ok = 0;
			FString FirstFail;
			for (int32 t = 0; t < Trials; ++t)
			{
				const FPts Dense = HandDraw(Case.Shape, 3000 + t, Case.bVariant);
				const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Sparse(Dense, Spacing, 9000 + t));
				if (R.bValid && R.Symbols.Num() == 1 && R.Symbols[0].Shape == Case.Shape) { ++Ok; }
				else if (FirstFail.IsEmpty()) { FirstFail = Describe(R); }
			}
			Total += Ok;
			Row += FString::Printf(TEXT("%s %d/%d  "), Case.Name, Ok, Trials);
			if (Ok < Trials * 8 / 10) { AddInfo(FString::Printf(TEXT("  %.0fpx %s first fail: %s"), Spacing, Case.Name, *FirstFail)); }
		}
		AddInfo(Row);
		// 30px 間隔（速いドラッグ）までは 8 割以上を要求する。60px は、サンプルの間に角が隠れて
		// 復元できないことがあるので、参考値の表示だけにする
		if (Spacing <= 30.f)
		{
			TestTrue(FString::Printf(TEXT("sparse input (%.0fpx) overall: %s"), Spacing, *Row), Total >= UE_ARRAY_COUNT(Cases) * Trials * 80 / 100);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeMultiRobustnessTest, "SEGC2026.Shape.MultiRobustness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeMultiRobustnessTest::RunTest(const FString& Parameters)
{
	// 手描き風に歪ませた記号を 2〜4 個、下をくぐるつなぎ線で一筆書きにつなぐ。
	// 記号どうしの間隔を十分に空けているので、重ならず、すべて順番どおりに認識されるはず
	const EShapeType Pool[] = { EShapeType::Circle, EShapeType::Triangle, EShapeType::Square,
		EShapeType::Pentagon, EShapeType::Star, EShapeType::Heart };
	const int32 Trials = 30;
	int32 Ok = 0;
	FString FirstFail;
	for (int32 t = 0; t < Trials; ++t)
	{
		FRandomStream Rand(5000 + t);
		const int32 Count = Rand.RandRange(2, 4);
		FPts Pts;
		TArray<EShapeType> Expected;
		for (int32 k = 0; k < Count; ++k)
		{
			const EShapeType Shape = Pool[Rand.RandRange(0, UE_ARRAY_COUNT(Pool) - 1)];
			Expected.Add(Shape);
			AppendSymbol(Pts, HandDraw(Shape, 7000 + t * 10 + k, false, FVector2D(250 + 480 * k, 300), true));
		}
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		bool bMatch = R.bValid && R.Symbols.Num() == Expected.Num();
		for (int32 i = 0; bMatch && i < Expected.Num(); ++i) { bMatch = R.Symbols[i].Shape == Expected[i]; }
		if (bMatch) { ++Ok; }
		else if (FirstFail.IsEmpty())
		{
			FString Want;
			for (EShapeType E : Expected) { Want += FString(GetShapeName(E)) + TEXT(","); }
			FirstFail = FString::Printf(TEXT("trial %d want [%s]: %s"), t, *Want, *Describe(R));
		}
	}
	AddInfo(FString::Printf(TEXT("multi-symbol strokes: %d/%d%s%s"), Ok, Trials, FirstFail.IsEmpty() ? TEXT("") : TEXT("  first fail -> "), *FirstFail));
	TestTrue(FString::Printf(TEXT("multi-symbol rate %d/%d (first fail: %s)"), Ok, Trials, *FirstFail), Ok >= Trials * 85 / 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeRobustnessTest, "SEGC2026.Shape.Robustness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeRobustnessTest::RunTest(const FString& Parameters)
{
	struct FCase { const TCHAR* Name; EShapeType Shape; bool bVariant; };
	const FCase Cases[] = {
		{ TEXT("circle"),   EShapeType::Circle,   false },
		{ TEXT("triangle"), EShapeType::Triangle, false },
		{ TEXT("square"),   EShapeType::Square,   false },
		{ TEXT("square (tilted 45)"), EShapeType::Square, true },
		{ TEXT("pentagon"), EShapeType::Pentagon, false },
		{ TEXT("star"),     EShapeType::Star,     false },
		{ TEXT("pentagram"),EShapeType::Star,     true  },
		{ TEXT("heart"),    EShapeType::Heart,    false },
	};
	const int32 Trials = 40;
	for (const FCase& Case : Cases)
	{
		int32 Ok = 0;
		FString FirstFail;
		for (int32 t = 0; t < Trials; ++t)
		{
			const FStrokeResult R = FShapeRecognizer::RecognizeStroke(HandDraw(Case.Shape, 1000 + t, Case.bVariant));
			if (R.bValid && R.Symbols.Num() == 1 && R.Symbols[0].Shape == Case.Shape) { ++Ok; }
			else if (FirstFail.IsEmpty()) { FirstFail = FString::Printf(TEXT("seed %d: %s"), 1000 + t, *Describe(R)); }
		}
		AddInfo(FString::Printf(TEXT("%s: %d/%d%s%s"), Case.Name, Ok, Trials, FirstFail.IsEmpty() ? TEXT("") : TEXT("  first fail -> "), *FirstFail));
		TestTrue(FString::Printf(TEXT("%s recognition rate %d/%d (first fail: %s)"), Case.Name, Ok, Trials, *FirstFail), Ok >= Trials * 85 / 100);
	}

	// 処理時間: 3 記号の一筆書き
	{
		FPts Pts;
		AppendSymbol(Pts, SymTriangle(FVector2D(150, 300), 80.f, 3));
		AppendSymbol(Pts, SymCircle(FVector2D(400, 300), 60.f, 4));
		AppendSymbol(Pts, SymStar(FVector2D(660, 300), 90.f, 5));
		const double T0 = FPlatformTime::Seconds();
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		AddInfo(FString::Printf(TEXT("3 symbols, %d points: %.1f ms (valid=%d)"), Pts.Num(), Ms, R.bValid ? 1 : 0));
		TestTrue(TEXT("recognition under 500 ms"), Ms < 500.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeSmallLoopTest, "SEGC2026.Shape.SmallLoops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeSmallLoopTest::RunTest(const FString& Parameters)
{
	// 大きな円 → 大きな四角 の間のつなぎ線に、折り返しでできた小さな円（半径 36）がある。
	// 小さな円は記号として数えず、2 個の記号として有効になる
	{
		FPts Pts;
		AppendSymbol(Pts, SymCircle(FVector2D(160, 300), 100.f, 1));
		const FPts Sq = SymSquare(FVector2D(560, 300), 100.f, 2);
		const FVector2D A = Pts.Last();
		const FVector2D B = Sq[0];
		const double Y = FMath::Max(A.Y, B.Y) + 50.0;
		Pts.Append(Line(A, FVector2D(A.X, Y)));
		Pts.Append(Line(FVector2D(A.X, Y), FVector2D(360, Y), 30));
		Pts.Append(Circle(FVector2D(360, Y - 36), 36.f, 90.f, 360.f, 0.f, 3)); // 折り返しの小ループ
		Pts.Append(Line(FVector2D(360, Y), FVector2D(B.X, Y), 30));
		Pts.Append(Line(FVector2D(B.X, Y), B));
		Pts.Append(Sq);

		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		bool bOk = R.bValid && R.Symbols.Num() == 2 && R.Symbols[0].Shape == EShapeType::Circle && R.Symbols[1].Shape == EShapeType::Square;
		TestTrue(FString::Printf(TEXT("small curl is ignored: %s"), *Describe(R)), bOk);
		TestTrue(TEXT("ignored count is reported"), R.Reason.Contains(TEXT("無視")));
	}
	// 同じくらいの大きさ（半径比 0.45）の円は、ちゃんと記号として数える
	{
		FPts Pts;
		AppendSymbol(Pts, SymCircle(FVector2D(160, 300), 100.f, 4));
		AppendSymbol(Pts, SymCircle(FVector2D(460, 300), 45.f, 5));
		ExpectSequence(*this, TEXT("0.45x circle is a symbol"), Pts, { EShapeType::Circle, EShapeType::Circle });
	}
	// 1 つの大きな円の描き終わりに、小さな折り返しがあっても 1 個の円
	{
		FPts Pts = SymCircle(FVector2D(300, 300), 120.f, 6);
		const FVector2D End = Pts.Last();
		Pts.Append(Line(End, End + FVector2D(70, 30), 12));
		Pts.Append(Circle(End + FVector2D(70 + 20, 30), 20.f, 180.f, 360.f, 0.f, 7));
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		TestTrue(FString::Printf(TEXT("curl after a big circle: %s"), *Describe(R)),
			R.Symbols.Num() == 1 && R.Symbols[0].Shape == EShapeType::Circle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeConstraintTest, "SEGC2026.Shape.Constraints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShapeConstraintTest::RunTest(const FString& Parameters)
{
	// 重なった 2 つの円は無効
	{
		FPts Pts;
		AppendSymbol(Pts, SymCircle(FVector2D(300, 300), 80.f, 1));
		AppendSymbol(Pts, SymCircle(FVector2D(360, 300), 80.f, 2));
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		TestFalse(FString::Printf(TEXT("overlap must be invalid: %s"), *Describe(R)), R.bValid);
		TestTrue(TEXT("overlap reason"), R.Reason.Contains(TEXT("重なって")));
	}
	// 入れ子（大きな円の中に小さな円）は無効
	{
		FPts Pts;
		AppendSymbol(Pts, SymCircle(FVector2D(300, 300), 100.f, 3), true);
		AppendSymbol(Pts, SymCircle(FVector2D(300, 300), 45.f, 4), true);
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		TestFalse(FString::Printf(TEXT("nested must be invalid: %s"), *Describe(R)), R.bValid);
	}
	// つなぎ線が、あとで描く記号を横切る場合は無効（A → B の直線が C の底を通る）
	{
		FPts Pts;
		AppendSymbol(Pts, SymCircle(FVector2D(120, 300), 50.f, 5));
		AppendSymbol(Pts, SymCircle(FVector2D(520, 300), 50.f, 6), true);
		AppendSymbol(Pts, SymCircle(FVector2D(320, 300), 50.f, 7), true);
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		TestFalse(FString::Printf(TEXT("connector crossing must be invalid: %s"), *Describe(R)), R.bValid);
	}
	// 記号が多すぎる（7 個）
	{
		FPts Pts;
		for (int32 i = 0; i < 7; ++i) { AppendSymbol(Pts, SymCircle(FVector2D(100 + 130 * i, 300), 45.f, 10 + i)); }
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts);
		TestFalse(FString::Printf(TEXT("too many symbols must be invalid: %s"), *Describe(R)), R.bValid);
	}
	// 重なりを許可すれば有効
	{
		FPts Pts;
		AppendSymbol(Pts, SymCircle(FVector2D(300, 300), 80.f, 1));
		AppendSymbol(Pts, SymCircle(FVector2D(360, 300), 80.f, 2));
		FShapeRecognizerParams Pm;
		Pm.bAllowOverlap = true;
		const FStrokeResult R = FShapeRecognizer::RecognizeStroke(Pts, Pm);
		TestTrue(FString::Printf(TEXT("overlap allowed: %s"), *Describe(R)), R.bValid && R.Symbols.Num() == 2);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

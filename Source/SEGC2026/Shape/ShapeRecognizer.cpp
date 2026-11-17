#include "Shape/ShapeRecognizer.h"

namespace
{
	/** 閉じたパス（最後の点 == 最初の点）を N 点に等間隔リサンプリング */
	void ResampleClosed(const TArray<FVector2D>& ClosedPath, int32 N, TArray<FVector2D>& Out)
	{
		Out.Reset();
		double Total = 0.0;
		for (int32 i = 1; i < ClosedPath.Num(); ++i)
		{
			Total += FVector2D::Distance(ClosedPath[i - 1], ClosedPath[i]);
		}
		if (Total <= UE_DOUBLE_SMALL_NUMBER || N < 3)
		{
			return;
		}

		const double Interval = Total / N;
		double Accum = 0.0;
		Out.Add(ClosedPath[0]);

		for (int32 i = 1; i < ClosedPath.Num() && Out.Num() < N; ++i)
		{
			FVector2D Prev = ClosedPath[i - 1];
			const FVector2D Cur = ClosedPath[i];
			double SegLen = FVector2D::Distance(Prev, Cur);

			while (SegLen > UE_DOUBLE_SMALL_NUMBER && Accum + SegLen >= Interval && Out.Num() < N)
			{
				const double T = (Interval - Accum) / SegLen;
				const FVector2D NewPt = Prev + (Cur - Prev) * T;
				Out.Add(NewPt);
				Prev = NewPt;
				SegLen = FVector2D::Distance(Prev, Cur);
				Accum = 0.0;
			}
			Accum += SegLen;
		}
		// 浮動小数誤差で足りなければ最後の点で埋める
		while (Out.Num() < N)
		{
			Out.Add(ClosedPath.Last());
		}
	}

	double DistToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double Len2 = AB.SizeSquared();
		if (Len2 <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D::Distance(P, A);
		}
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Len2, 0.0, 1.0);
		return FVector2D::Distance(P, A + AB * T);
	}

	int32 FarthestIndexFrom(const TArray<FVector2D>& Pts, const FVector2D& From)
	{
		int32 Best = 0;
		double BestD = -1.0;
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			const double D = FVector2D::DistSquared(Pts[i], From);
			if (D > BestD) { BestD = D; Best = i; }
		}
		return Best;
	}

	float ScoreFromError(double Err, double ErrAtZero)
	{
		return (float)FMath::Clamp(1.0 - Err / FMath::Max(ErrAtZero, 1e-6), 0.0, 1.0);
	}
}

FShapeResult FShapeRecognizer::Recognize(const TArray<FVector2D>& RawPoints, const FShapeRecognizerParams& Params)
{
	FShapeResult R;

	if (RawPoints.Num() < Params.MinPoints)
	{
		R.Reason = TEXT("too few points");
		return R;
	}

	// 1. 近接重複点の除去
	TArray<FVector2D> Pts;
	Pts.Reserve(RawPoints.Num() + 1);
	for (const FVector2D& P : RawPoints)
	{
		if (Pts.Num() == 0 || FVector2D::Distance(Pts.Last(), P) >= 0.5)
		{
			Pts.Add(P);
		}
	}
	if (Pts.Num() < Params.MinPoints)
	{
		R.Reason = TEXT("too few distinct points");
		return R;
	}

	// 2. サイズ・全長・閉じ判定
	FVector2D Min = Pts[0], Max = Pts[0];
	double Length = 0.0;
	for (int32 i = 0; i < Pts.Num(); ++i)
	{
		Min = FVector2D(FMath::Min(Min.X, Pts[i].X), FMath::Min(Min.Y, Pts[i].Y));
		Max = FVector2D(FMath::Max(Max.X, Pts[i].X), FMath::Max(Max.Y, Pts[i].Y));
		if (i > 0) { Length += FVector2D::Distance(Pts[i - 1], Pts[i]); }
	}
	const double Diag = FVector2D::Distance(Min, Max);
	if (Diag < Params.MinSizePx || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		R.Reason = TEXT("too small");
		return R;
	}

	R.ClosureRatio = (float)(FVector2D::Distance(Pts[0], Pts.Last()) / Length);
	if (R.ClosureRatio > Params.MaxClosureRatio)
	{
		R.Reason = FString::Printf(TEXT("not closed (closure %.2f > %.2f)"), R.ClosureRatio, Params.MaxClosureRatio);
		return R;
	}

	// 3. 閉曲線としてリサンプリング
	const FVector2D FirstPt = Pts[0];
	Pts.Add(FirstPt);
	TArray<FVector2D> S;
	ResampleClosed(Pts, Params.ResampleCount, S);
	if (S.Num() < 3)
	{
		R.Reason = TEXT("resample failed");
		return R;
	}

	FVector2D Centroid = FVector2D::ZeroVector;
	for (const FVector2D& P : S) { Centroid += P; }
	Centroid /= (double)S.Num();

	FVector2D SMin = S[0], SMax = S[0];
	for (const FVector2D& P : S)
	{
		SMin = FVector2D(FMath::Min(SMin.X, P.X), FMath::Min(SMin.Y, P.Y));
		SMax = FVector2D(FMath::Max(SMax.X, P.X), FMath::Max(SMax.Y, P.Y));
	}
	const double SDiag = FMath::Max(FVector2D::Distance(SMin, SMax), 1e-6);

	// 4. 円スコア: 重心からの半径の変動係数
	double MeanR = 0.0;
	for (const FVector2D& P : S) { MeanR += FVector2D::Distance(P, Centroid); }
	MeanR /= S.Num();
	double VarR = 0.0;
	for (const FVector2D& P : S)
	{
		const double d = FVector2D::Distance(P, Centroid) - MeanR;
		VarR += d * d;
	}
	const double CircleErr = MeanR > UE_DOUBLE_SMALL_NUMBER ? FMath::Sqrt(VarR / S.Num()) / MeanR : 1.0;
	R.CircleScore = ScoreFromError(CircleErr, Params.CircleErrAtZeroScore);

	// 5. 三角形スコア: 3頂点フィット
	const int32 IA = FarthestIndexFrom(S, Centroid);
	const int32 IB = FarthestIndexFrom(S, S[IA]);
	const FVector2D A = S[IA], B = S[IB];
	const FVector2D AB = B - A;
	const double ABLen = AB.Size();

	int32 IC = 0;
	double BestPerp = -1.0;
	for (int32 i = 0; i < S.Num(); ++i)
	{
		const double Perp = ABLen > UE_DOUBLE_SMALL_NUMBER ? FMath::Abs(FVector2D::CrossProduct(AB, S[i] - A)) / ABLen : 0.0;
		if (Perp > BestPerp) { BestPerp = Perp; IC = i; }
	}
	const FVector2D C = S[IC];

	if (ABLen > UE_DOUBLE_SMALL_NUMBER && BestPerp > 0.05 * SDiag)
	{
		double SumD = 0.0;
		for (const FVector2D& P : S)
		{
			SumD += FMath::Min3(DistToSegment(P, A, B), DistToSegment(P, B, C), DistToSegment(P, C, A));
		}
		const double TriErr = (SumD / S.Num()) / SDiag;
		R.TriangleScore = ScoreFromError(TriErr, Params.TriangleErrAtZeroScore);
		R.DebugVertices = { A, B, C };

		// 先端 = 内角が最小の頂点
		const FVector2D V[3] = { A, B, C };
		double MinAngle = TNumericLimits<double>::Max();
		int32 Apex = 0;
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector2D U = (V[(k + 1) % 3] - V[k]).GetSafeNormal();
			const FVector2D W = (V[(k + 2) % 3] - V[k]).GetSafeNormal();
			const double Ang = FMath::Acos(FMath::Clamp(FVector2D::DotProduct(U, W), -1.0, 1.0));
			if (Ang < MinAngle) { MinAngle = Ang; Apex = k; }
		}
		const FVector2D TriCenter = (A + B + C) / 3.0;
		R.Direction = (V[Apex] - TriCenter).GetSafeNormal();
	}

	// 6. 判定
	const bool bCircle = R.CircleScore >= R.TriangleScore;
	R.Confidence = bCircle ? R.CircleScore : R.TriangleScore;
	if (R.Confidence < Params.MinConfidence)
	{
		R.Shape = EShapeType::Unknown;
		R.Reason = FString::Printf(TEXT("low confidence (circle %.2f, tri %.2f)"), R.CircleScore, R.TriangleScore);
		return R;
	}
	R.Shape = bCircle ? EShapeType::Circle : EShapeType::Triangle;
	R.Reason = FString::Printf(TEXT("circle %.2f, tri %.2f, closure %.2f"), R.CircleScore, R.TriangleScore, R.ClosureRatio);
	if (bCircle) { R.Direction = FVector2D::ZeroVector; R.DebugVertices.Reset(); }
	return R;
}

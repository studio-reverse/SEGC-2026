#include "Shape/ShapeRecognizer.h"

const TCHAR* GetShapeName(EShapeType Shape)
{
	switch (Shape)
	{
	case EShapeType::Circle:   return TEXT("Circle");
	case EShapeType::Triangle: return TEXT("Triangle");
	case EShapeType::Square:   return TEXT("Square");
	case EShapeType::Pentagon: return TEXT("Pentagon");
	case EShapeType::Star:     return TEXT("Star");
	case EShapeType::Heart:    return TEXT("Heart");
	default:                   return TEXT("Unknown");
	}
}

namespace
{
	// ------------------------------------------------------------------
	// 幾何ユーティリティ
	// ------------------------------------------------------------------

	double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }

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

	bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		const double O1 = Cross(B - A, C - A);
		const double O2 = Cross(B - A, D - A);
		const double O3 = Cross(D - C, A - C);
		const double O4 = Cross(D - C, B - C);
		return ((O1 > 0) != (O2 > 0)) && ((O3 > 0) != (O4 > 0));
	}

	double SegSegDistance(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		if (SegmentsIntersect(A, B, C, D))
		{
			return 0.0;
		}
		return FMath::Min(FMath::Min(DistToSegment(A, C, D), DistToSegment(B, C, D)),
			FMath::Min(DistToSegment(C, A, B), DistToSegment(D, A, B)));
	}

	/** 閉ポリライン同士の最短距離（交差していれば 0） */
	double ClosedPolylineDistance(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const FVector2D& A0 = A[i];
			const FVector2D& A1 = A[(i + 1) % A.Num()];
			for (int32 j = 0; j < B.Num(); ++j)
			{
				Best = FMath::Min(Best, SegSegDistance(A0, A1, B[j], B[(j + 1) % B.Num()]));
				if (Best <= 0.0)
				{
					return 0.0;
				}
			}
		}
		return Best;
	}

	double PointToClosedPolylineDistance(const FVector2D& P, const TArray<FVector2D>& Poly)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			Best = FMath::Min(Best, DistToSegment(P, Poly[i], Poly[(i + 1) % Poly.Num()]));
		}
		return Best;
	}

	bool PointInPolygon(const FVector2D& P, const TArray<FVector2D>& Poly)
	{
		bool bInside = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
		{
			const FVector2D& A = Poly[i];
			const FVector2D& B = Poly[j];
			if (((A.Y > P.Y) != (B.Y > P.Y)) && (P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X))
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	/** 閉多角形の面積重心。符号付き面積も返す */
	FVector2D PolygonCentroid(const TArray<FVector2D>& S, double& OutArea)
	{
		double A2 = 0.0, Cx = 0.0, Cy = 0.0;
		FVector2D Mean = FVector2D::ZeroVector;
		for (int32 i = 0; i < S.Num(); ++i)
		{
			const FVector2D& P = S[i];
			const FVector2D& Q = S[(i + 1) % S.Num()];
			const double Cr = P.X * Q.Y - Q.X * P.Y;
			A2 += Cr;
			Cx += (P.X + Q.X) * Cr;
			Cy += (P.Y + Q.Y) * Cr;
			Mean += P;
		}
		OutArea = A2 * 0.5;
		if (FMath::Abs(A2) < 1e-6)
		{
			return Mean / FMath::Max(1, S.Num());
		}
		return FVector2D(Cx, Cy) / (3.0 * A2);
	}

	// ------------------------------------------------------------------
	// 前処理
	// ------------------------------------------------------------------

	/** 近接する重複点を除く。Map[新インデックス] = 元インデックス */
	void Dedupe(const TArray<FVector2D>& Raw, TArray<FVector2D>& OutP, TArray<int32>& OutMap)
	{
		OutP.Reset();
		OutMap.Reset();
		OutP.Reserve(Raw.Num());
		for (int32 i = 0; i < Raw.Num(); ++i)
		{
			if (OutP.Num() == 0 || FVector2D::Distance(OutP.Last(), Raw[i]) >= 0.5)
			{
				OutP.Add(Raw[i]);
				OutMap.Add(i);
			}
		}
	}

	/**
	 * 点が疎な区間（速くドラッグしたときなど）を、centripetal Catmull-Rom スプラインで補間して密にする。
	 * 点を結ぶ折れ線のままだと、円が多角形に見えて角が大量に検出されるため。
	 * 間隔が詰まっている区間は、そのまま残す。Map[新インデックス] = 元インデックス（その区間の始点）
	 */
	void Densify(TArray<FVector2D>& P, TArray<int32>& Map, double Step)
	{
		const int32 N = P.Num();
		if (N < 2) { return; }

		TArray<FVector2D> OutP;
		TArray<int32> OutMap;
		OutP.Reserve(N * 2);
		OutMap.Reserve(N * 2);

		for (int32 i = 0; i < N - 1; ++i)
		{
			OutP.Add(P[i]);
			OutMap.Add(Map[i]);

			const FVector2D P1 = P[i], P2 = P[i + 1];
			const double Dist = FVector2D::Distance(P1, P2);
			if (Dist <= Step * 1.5) { continue; }

			const FVector2D P0 = (i > 0) ? P[i - 1] : P1 - (P2 - P1);
			const FVector2D P3 = (i + 2 < N) ? P[i + 2] : P2 + (P2 - P1);
			const double T0 = 0.0;
			const double T1 = T0 + FMath::Sqrt(FMath::Max(FVector2D::Distance(P0, P1), 1e-3));
			const double T2 = T1 + FMath::Sqrt(FMath::Max(Dist, 1e-3));
			const double T3 = T2 + FMath::Sqrt(FMath::Max(FVector2D::Distance(P2, P3), 1e-3));

			const int32 Steps = FMath::CeilToInt(Dist / Step);
			for (int32 s = 1; s < Steps; ++s)
			{
				const double T = T1 + (T2 - T1) * s / Steps;
				const FVector2D A1 = P0 * ((T1 - T) / (T1 - T0)) + P1 * ((T - T0) / (T1 - T0));
				const FVector2D A2 = P1 * ((T2 - T) / (T2 - T1)) + P2 * ((T - T1) / (T2 - T1));
				const FVector2D A3 = P2 * ((T3 - T) / (T3 - T2)) + P3 * ((T - T2) / (T3 - T2));
				const FVector2D B1 = A1 * ((T2 - T) / (T2 - T0)) + A2 * ((T - T0) / (T2 - T0));
				const FVector2D B2 = A2 * ((T3 - T) / (T3 - T1)) + A3 * ((T - T1) / (T3 - T1));
				OutP.Add(B1 * ((T2 - T) / (T2 - T1)) + B2 * ((T - T1) / (T2 - T1)));
				OutMap.Add(Map[i]);
			}
		}
		OutP.Add(P.Last());
		OutMap.Add(Map.Last());
		P = MoveTemp(OutP);
		Map = MoveTemp(OutMap);
	}

	void BuildCumulative(const TArray<FVector2D>& P, TArray<double>& OutL)
	{
		OutL.SetNumUninitialized(P.Num());
		double Acc = 0.0;
		for (int32 i = 0; i < P.Num(); ++i)
		{
			if (i > 0)
			{
				Acc += FVector2D::Distance(P[i - 1], P[i]);
			}
			OutL[i] = Acc;
		}
	}

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
		while (Out.Num() < N)
		{
			Out.Add(ClosedPath.Last());
		}
	}

	void SmoothClosed(TArray<FVector2D>& S, int32 Passes)
	{
		const int32 N = S.Num();
		TArray<FVector2D> T;
		T.SetNum(N);
		for (int32 Pass = 0; Pass < Passes; ++Pass)
		{
			for (int32 i = 0; i < N; ++i)
			{
				T[i] = (S[(i - 1 + N) % N] + S[i] * 2.0 + S[(i + 1) % N]) * 0.25;
			}
			S = T;
		}
	}

	// ------------------------------------------------------------------
	// 角検出
	// ------------------------------------------------------------------

	struct FCorner
	{
		int32 Index = 0;
		double Angle = 0.0; // 度。凸が正、凹が負
	};

	void DetectCorners(const TArray<FVector2D>& S, double Sign, const FShapeRecognizerParams& Pm,
		TArray<double>& OutAngles, TArray<FCorner>& OutCorners)
	{
		const int32 N = S.Num();
		const int32 W = FMath::Max(1, Pm.CornerWindow);
		OutAngles.SetNumUninitialized(N);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D V1 = S[i] - S[(i - W + N) % N];
			const FVector2D V2 = S[(i + W) % N] - S[i];
			OutAngles[i] = FMath::RadiansToDegrees(FMath::Atan2(Cross(V1, V2), FVector2D::DotProduct(V1, V2))) * Sign;
		}

		TArray<FCorner> Raw;
		for (int32 i = 0; i < N; ++i)
		{
			const double A = OutAngles[i];
			const double Thr = A > 0 ? Pm.ConvexCornerDeg : Pm.ConcaveCornerDeg;
			if (FMath::Abs(A) < Thr)
			{
				continue;
			}
			bool bLocalMax = true;
			for (int32 d = 1; d <= W && bLocalMax; ++d)
			{
				if (FMath::Abs(OutAngles[(i - d + N) % N]) > FMath::Abs(A) || FMath::Abs(OutAngles[(i + d) % N]) > FMath::Abs(A))
				{
					bLocalMax = false;
				}
			}
			if (bLocalMax)
			{
				Raw.Add({ i, A });
			}
		}

		// 近接する同符号の角（平らな頂上で重複したもの）を統合
		OutCorners.Reset();
		for (const FCorner& C : Raw)
		{
			if (OutCorners.Num() > 0 && (C.Index - OutCorners.Last().Index) <= W && (C.Angle > 0) == (OutCorners.Last().Angle > 0))
			{
				if (FMath::Abs(C.Angle) > FMath::Abs(OutCorners.Last().Angle))
				{
					OutCorners.Last() = C;
				}
			}
			else
			{
				OutCorners.Add(C);
			}
		}
		if (OutCorners.Num() > 1)
		{
			const FCorner& First = OutCorners[0];
			const FCorner& Last = OutCorners.Last();
			if ((First.Index + N - Last.Index) <= W && (First.Angle > 0) == (Last.Angle > 0))
			{
				if (FMath::Abs(First.Angle) > FMath::Abs(Last.Angle))
				{
					OutCorners.Pop();
				}
				else
				{
					OutCorners.RemoveAt(0);
				}
			}
		}
	}

	// ------------------------------------------------------------------
	// スコア計算
	// ------------------------------------------------------------------

	float ScoreFromError(double Err, double ErrAtZero)
	{
		return (float)FMath::Clamp(1.0 - Err / FMath::Max(ErrAtZero, 1e-6), 0.0, 1.0);
	}

	double CoefVar(const TArray<double>& V)
	{
		if (V.Num() == 0) { return 1.0; }
		double Mean = 0.0;
		for (double X : V) { Mean += X; }
		Mean /= V.Num();
		if (Mean <= UE_DOUBLE_SMALL_NUMBER) { return 1.0; }
		double Var = 0.0;
		for (double X : V) { Var += (X - Mean) * (X - Mean); }
		return FMath::Sqrt(Var / V.Num()) / Mean;
	}

	double MeanOf(const TArray<double>& V)
	{
		if (V.Num() == 0) { return 0.0; }
		double M = 0.0;
		for (double X : V) { M += X; }
		return M / V.Num();
	}

	/** 頂点 V の閉多角形に、点列 S がどれだけ沿うか（平均距離 / 対角線） */
	double PolygonFitError(const TArray<FVector2D>& S, const TArray<FVector2D>& V, double Diag)
	{
		double Sum = 0.0;
		for (const FVector2D& P : S)
		{
			Sum += PointToClosedPolylineDistance(P, V);
		}
		return (Sum / FMath::Max(1, S.Num())) / FMath::Max(Diag, 1e-6);
	}

	double InteriorAngleDeg(const TArray<FVector2D>& V, int32 K)
	{
		const int32 N = V.Num();
		const FVector2D A = (V[(K - 1 + N) % N] - V[K]).GetSafeNormal();
		const FVector2D B = (V[(K + 1) % N] - V[K]).GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(A, B), -1.0, 1.0)));
	}

	/** 先端（尖り）の極角が 72 度ずつ並んでいるか（0..1） */
	double TipSpacingScore(const TArray<FVector2D>& Tips, const FVector2D& Center)
	{
		if (Tips.Num() != 5) { return 0.0; }
		TArray<double> Angles;
		for (const FVector2D& T : Tips)
		{
			Angles.Add(FMath::RadiansToDegrees(FMath::Atan2(T.Y - Center.Y, T.X - Center.X)));
		}
		Angles.Sort();
		double MaxDev = 0.0;
		for (int32 i = 0; i < 5; ++i)
		{
			double Gap = Angles[(i + 1) % 5] - Angles[i];
			if (Gap < 0) { Gap += 360.0; }
			MaxDev = FMath::Max(MaxDev, FMath::Abs(Gap - 72.0));
		}
		return FMath::Clamp(1.0 - MaxDev / 40.0, 0.0, 1.0);
	}

	/** ハートのテンプレート（画面座標、上向き）を M 点で作る。重心 0・RMS 半径 1 に正規化 */
	void BuildHeartTemplate(int32 M, TArray<FVector2D>& OutT)
	{
		// 媒介変数で細かく描いてから、入力と同じ「弧長で等間隔」にリサンプリングする
		// （媒介変数のままだと点の間隔が一定でなく、完全なハートでも誤差が出る）
		TArray<FVector2D> Fine;
		const int32 FineN = 720;
		for (int32 i = 0; i <= FineN; ++i)
		{
			const double T = 2.0 * UE_DOUBLE_PI * i / FineN;
			const double S = FMath::Sin(T);
			const double X = 16.0 * S * S * S;
			const double Y = -(13.0 * FMath::Cos(T) - 5.0 * FMath::Cos(2 * T) - 2.0 * FMath::Cos(3 * T) - FMath::Cos(4 * T));
			Fine.Add(FVector2D(X, Y));
		}
		ResampleClosed(Fine, M, OutT);
		FVector2D Mean = FVector2D::ZeroVector;
		for (const FVector2D& P : OutT) { Mean += P; }
		Mean /= M;
		double Rms = 0.0;
		for (FVector2D& P : OutT) { P -= Mean; Rms += P.SizeSquared(); }
		Rms = FMath::Sqrt(Rms / M);
		for (FVector2D& P : OutT) { P /= Rms; }
	}

	/** ハートのテンプレートとの最小の平均距離（開始位置・回り方向・±20 度の傾きを許容） */
	double HeartError(const TArray<FVector2D>& S)
	{
		const int32 M = 64;
		TArray<FVector2D> A;
		A.SetNum(M);
		FVector2D Mean = FVector2D::ZeroVector;
		for (int32 i = 0; i < M; ++i)
		{
			A[i] = S[(i * S.Num() / M) % S.Num()];
			Mean += A[i];
		}
		Mean /= M;
		double Rms = 0.0;
		for (FVector2D& P : A) { P -= Mean; Rms += P.SizeSquared(); }
		Rms = FMath::Sqrt(Rms / M);
		if (Rms <= UE_DOUBLE_SMALL_NUMBER) { return 1e9; }
		for (FVector2D& P : A) { P /= Rms; }

		TArray<FVector2D> T;
		BuildHeartTemplate(M, T);

		double Best = TNumericLimits<double>::Max();
		TArray<FVector2D> B;
		B.SetNum(M);
		for (int32 RotDeg = -20; RotDeg <= 20; RotDeg += 10)
		{
			const double C = FMath::Cos(FMath::DegreesToRadians((double)RotDeg));
			const double Sn = FMath::Sin(FMath::DegreesToRadians((double)RotDeg));
			for (int32 i = 0; i < M; ++i)
			{
				B[i] = FVector2D(A[i].X * C - A[i].Y * Sn, A[i].X * Sn + A[i].Y * C);
			}
			for (int32 Dir = -1; Dir <= 1; Dir += 2)
			{
				for (int32 Shift = 0; Shift < M; ++Shift)
				{
					double Sum = 0.0;
					for (int32 i = 0; i < M; ++i)
					{
						const int32 Idx = (((Dir * i + Shift) % M) + M) % M;
						Sum += FVector2D::Distance(B[i], T[Idx]);
					}
					Best = FMath::Min(Best, Sum / M);
				}
			}
		}
		return Best;
	}

	// ------------------------------------------------------------------
	// 1 つの閉じたループの分類
	// ------------------------------------------------------------------

	FShapeResult ClassifyLoop(const TArray<FVector2D>& Loop, const FShapeRecognizerParams& Pm)
	{
		FShapeResult R;
		const int32 Num = Loop.Num();
		if (Num < Pm.MinPoints)
		{
			R.Reason = TEXT("too few points");
			return R;
		}

		FVector2D Min = Loop[0], Max = Loop[0];
		double Length = 0.0;
		for (int32 i = 0; i < Num; ++i)
		{
			Min = FVector2D(FMath::Min(Min.X, Loop[i].X), FMath::Min(Min.Y, Loop[i].Y));
			Max = FVector2D(FMath::Max(Max.X, Loop[i].X), FMath::Max(Max.Y, Loop[i].Y));
			if (i > 0) { Length += FVector2D::Distance(Loop[i - 1], Loop[i]); }
		}
		const double Diag = FVector2D::Distance(Min, Max);
		if (Diag < Pm.MinSizePx || Length <= UE_DOUBLE_SMALL_NUMBER)
		{
			R.Reason = TEXT("too small");
			return R;
		}

		R.ClosureRatio = (float)(FVector2D::Distance(Loop[0], Loop.Last()) / Length);
		if (R.ClosureRatio > Pm.MaxClosureRatio)
		{
			R.Reason = FString::Printf(TEXT("not closed (closure %.2f > %.2f)"), R.ClosureRatio, Pm.MaxClosureRatio);
			return R;
		}

		TArray<FVector2D> Closed = Loop;
		const FVector2D FirstPt = Loop[0];
		Closed.Add(FirstPt);
		TArray<FVector2D> S;
		ResampleClosed(Closed, Pm.ResampleCount, S);
		if (S.Num() < 16)
		{
			R.Reason = TEXT("resample failed");
			return R;
		}
		SmoothClosed(S, 2);
		const int32 N = S.Num();

		double Area = 0.0;
		const FVector2D Center = PolygonCentroid(S, Area);
		R.Center = Center;
		const double Sign = Area >= 0.0 ? 1.0 : -1.0;

		FVector2D SMin = S[0], SMax = S[0];
		for (const FVector2D& P : S)
		{
			SMin = FVector2D(FMath::Min(SMin.X, P.X), FMath::Min(SMin.Y, P.Y));
			SMax = FVector2D(FMath::Max(SMax.X, P.X), FMath::Max(SMax.Y, P.Y));
		}
		const double SDiag = FMath::Max(FVector2D::Distance(SMin, SMax), 1e-6);

		// 半径
		TArray<double> Radii;
		for (const FVector2D& P : S) { Radii.Add(FVector2D::Distance(P, Center)); }
		const double MeanR = MeanOf(Radii);
		R.Radius = (float)MeanR;
		const double CV = CoefVar(Radii);

		// 角
		TArray<double> Angles;
		TArray<FCorner> Corners;
		DetectCorners(S, Sign, Pm, Angles, Corners);
		int32 NConvex = 0, NConcave = 0;
		double MaxAbsAngle = 0.0, MinAnyAngle = 0.0;
		for (const FCorner& C : Corners)
		{
			(C.Angle > 0 ? NConvex : NConcave)++;
			MaxAbsAngle = FMath::Max(MaxAbsAngle, FMath::Abs(C.Angle));
		}
		for (double A : Angles) { MinAnyAngle = FMath::Min(MinAnyAngle, A); }

		TMap<EShapeType, float>& Sc = R.Scores;
		for (int32 e = 1; e <= (int32)EShapeType::Heart; ++e) { Sc.Add((EShapeType)e, 0.f); }

		// --- 円 ---
		{
			const float Radial = ScoreFromError(CV, Pm.CircleErrAtZeroScore);
			float Penalty = 1.f;
			if (Corners.Num() == 1 && MaxAbsAngle < 70.0) { Penalty = 0.8f; }
			else if (Corners.Num() >= 1) { Penalty = 0.15f; }
			Sc[EShapeType::Circle] = Radial * Penalty;
		}

		// --- 凸の多角形（△ □ ◇ 五角形） ---
		if (NConcave == 0 && NConvex >= 3 && NConvex <= 5 && Corners.Num() == NConvex)
		{
			TArray<FVector2D> V;
			for (const FCorner& C : Corners) { V.Add(S[C.Index]); }
			const float Fit = ScoreFromError(PolygonFitError(S, V, SDiag), Pm.PolyErrAtZeroScore);

			if (NConvex == 3)
			{
				Sc[EShapeType::Triangle] = Fit;
				// 先端 = 隣り合う 2 辺の長さが最も近い頂点（二等辺三角形の頂角）。
				// 「最も鋭い角」だと、頂角が底角より大きい三角形で底角を選んでしまう
				double MinDiff = 1e9;
				int32 Apex = 0;
				for (int32 k = 0; k < 3; ++k)
				{
					const double S1 = FVector2D::Distance(V[k], V[(k + 2) % 3]);
					const double S2 = FVector2D::Distance(V[k], V[(k + 1) % 3]);
					const double Diff = FMath::Abs(S1 - S2) / FMath::Max(S1 + S2, 1e-6);
					if (Diff < MinDiff) { MinDiff = Diff; Apex = k; }
				}
				const FVector2D TriCenter = (V[0] + V[1] + V[2]) / 3.0;
				R.Direction = (V[Apex] - TriCenter).GetSafeNormal();
			}
			else if (NConvex == 4)
			{
				double MaxDev = 0.0;
				for (int32 k = 0; k < 4; ++k) { MaxDev = FMath::Max(MaxDev, FMath::Abs(InteriorAngleDeg(V, k) - 90.0)); }
				const float AngleScore = (float)FMath::Clamp(1.0 - MaxDev / 35.0, 0.0, 1.0);

				// 四つの角がほぼ直角なら四角形。向きは問わない（回転した ◇ も □ として扱う）
				Sc[EShapeType::Square] = Fit * AngleScore;
			}
			else
			{
				double MaxDev = 0.0;
				for (int32 k = 0; k < 5; ++k) { MaxDev = FMath::Max(MaxDev, FMath::Abs(InteriorAngleDeg(V, k) - 108.0)); }
				const float AngleScore = (float)FMath::Clamp(1.0 - MaxDev / 40.0, 0.0, 1.0);
				Sc[EShapeType::Pentagon] = Fit * AngleScore;
			}
			R.DebugVertices = V;
		}

		// --- 星（五芒星: 鋭い凸の角 5 つ） ---
		if (NConcave == 0 && NConvex == 5 && Corners.Num() == 5)
		{
			double MinAng = 1e9;
			TArray<double> TipR;
			TArray<FVector2D> Tips;
			for (const FCorner& C : Corners)
			{
				MinAng = FMath::Min(MinAng, C.Angle);
				TipR.Add(FVector2D::Distance(S[C.Index], Center));
				Tips.Add(S[C.Index]);
			}
			if (MinAng >= Pm.PentagramMinDeg)
			{
				// 鋭い凸の角がちょうど 5 つあること自体が星の強い特徴。正則性は加点に使う
				const double Regular = FMath::Clamp(1.0 - CoefVar(TipR) / 0.3, 0.0, 1.0);
				Sc[EShapeType::Star] = (float)(0.6 + 0.4 * Regular * TipSpacingScore(Tips, Center));
				R.DebugVertices = Tips;
			}
		}

		// --- 星（輪郭: 凸 5 + 凹 3〜5） ---
		if (NConvex == 5 && NConcave >= 3 && NConcave <= 5)
		{
			TArray<double> TipR, InnerR;
			TArray<FVector2D> Tips, All;
			for (const FCorner& C : Corners)
			{
				const double Rd = FVector2D::Distance(S[C.Index], Center);
				All.Add(S[C.Index]);
				if (C.Angle > 0) { TipR.Add(Rd); Tips.Add(S[C.Index]); }
				else { InnerR.Add(Rd); }
			}
			const double Ratio = MeanOf(InnerR) / FMath::Max(MeanOf(TipR), 1e-6);
			if (Ratio >= 0.25 && Ratio <= 0.8)
			{
				const double TipReg = FMath::Clamp(1.0 - CoefVar(TipR) / 0.3, 0.0, 1.0);
				const double InnerReg = FMath::Clamp(1.0 - CoefVar(InnerR) / 0.4, 0.0, 1.0);
				// 凸 5 + 凹 3〜5 の交互の構造が、星の強い特徴。正則性は加点に使う
				const float Outline = (float)(0.6 + 0.4 * TipReg * InnerReg * TipSpacingScore(Tips, Center));
				if (Outline > Sc[EShapeType::Star])
				{
					Sc[EShapeType::Star] = Outline;
					R.DebugVertices = All;
				}
			}
		}

		// --- ハート: はっきりした凹みがあり、凸の角が 3 つ以下で、テンプレートに近い ---
		// （凸の角が 4 つ以上の四角形などを除くため、構造でも絞る）
		if (MinAnyAngle <= -28.0 && NConvex <= 3)
		{
			Sc[EShapeType::Heart] = ScoreFromError(HeartError(S), Pm.HeartErrAtZeroScore);
		}

		// --- 判定 ---
		EShapeType Best = EShapeType::Unknown;
		float BestScore = 0.f;
		for (const TPair<EShapeType, float>& Pair : Sc)
		{
			if (Pair.Value > BestScore) { BestScore = Pair.Value; Best = Pair.Key; }
		}
		R.Confidence = BestScore;
		R.Reason = FString::Printf(TEXT("corners %d(+%d/-%d) cv %.2f best %s %.2f"),
			Corners.Num(), NConvex, NConcave, CV, GetShapeName(Best), BestScore);
		if (BestScore < Pm.MinConfidence)
		{
			R.Shape = EShapeType::Unknown;
			return R;
		}
		R.Shape = Best;
		if (Best != EShapeType::Triangle) { R.Direction = FVector2D::ZeroVector; }
		if (Best == EShapeType::Circle || Best == EShapeType::Heart) { R.DebugVertices.Reset(); }
		return R;
	}

	// ------------------------------------------------------------------
	// 一筆書きの分割
	// ------------------------------------------------------------------

	struct FLoopPair
	{
		int32 I = 0;
		int32 J = 0;
		double Closure = 0.0;
	};

	/** P[A..B] の中から、閉じたループの候補（始点終点が近く、面積がある区間）を列挙する */
	void FindLoopPairs(const TArray<FVector2D>& P, const TArray<double>& L, int32 A, int32 B,
		double MinLoopLen, double MaxClosure, TArray<FLoopPair>& Out)
	{
		const int32 Count = B - A + 1;
		if (Count < 3) { return; }
		// 通常の長さ（800 点以下）なら全点を端の候補にする。端が実際の始点・終点からずれると、
		// 記号の輪郭の一部が「つなぎ線」に混ざり、横切りの誤検出につながる
		const int32 Stride = FMath::Max(1, Count / 800);
		TArray<int32> Grid;
		for (int32 i = A; i <= B; i += Stride) { Grid.Add(i); }
		if (Grid.Last() != B) { Grid.Add(B); }

		for (int32 a = 0; a < Grid.Num(); ++a)
		{
			for (int32 b = a + 1; b < Grid.Num(); ++b)
			{
				const int32 I = Grid[a], J = Grid[b];
				const double Len = L[J] - L[I];
				if (Len < MinLoopLen) { continue; }
				const double Closure = FVector2D::Distance(P[I], P[J]) / Len;
				if (Closure > MaxClosure) { continue; }

				double A2 = 0.0;
				for (int32 k = I; k <= J; ++k)
				{
					const FVector2D& Pk = P[k];
					const FVector2D& Qk = P[k == J ? I : k + 1];
					A2 += Pk.X * Qk.Y - Qk.X * Pk.Y;
				}
				// 面積がほとんどない往復（折り返し）は閉じたループではない。
				// 目安: 円 0.080 / 四角 0.063 / 三角 0.048 / 五芒星 0.016（自己交差で小さい）
				if (FMath::Abs(A2) * 0.5 / (Len * Len) < 0.01) { continue; }

				Out.Add({ I, J, Closure });
			}
		}
	}

	void CopyRange(const TArray<FVector2D>& P, int32 I, int32 J, TArray<FVector2D>& Out)
	{
		Out.Reset();
		Out.Reserve(J - I + 1);
		for (int32 k = I; k <= J; ++k) { Out.Add(P[k]); }
	}
}

// ----------------------------------------------------------------------
// 公開 API
// ----------------------------------------------------------------------

FShapeResult FShapeRecognizer::Recognize(const TArray<FVector2D>& RawPoints, const FShapeRecognizerParams& Params)
{
	TArray<FVector2D> P;
	TArray<int32> Map;
	Dedupe(RawPoints, P, Map);
	Densify(P, Map, 4.0);
	FShapeResult R = ClassifyLoop(P, Params);
	R.StartIndex = 0;
	R.EndIndex = FMath::Max(0, RawPoints.Num() - 1);
	return R;
}

FStrokeResult FShapeRecognizer::RecognizeStroke(const TArray<FVector2D>& RawPoints, const FShapeRecognizerParams& Pm)
{
	FStrokeResult Out;

	TArray<FVector2D> P;
	TArray<int32> Map;
	Dedupe(RawPoints, P, Map);
	if (P.Num() < Pm.MinPoints)
	{
		Out.Reason = TEXT("点が少なすぎます");
		return Out;
	}
	// 点が疎な区間は、滑らかに補間して密にする（速くドラッグしたとき）
	Densify(P, Map, 4.0);
	const int32 n = P.Num();
	TArray<double> L;
	BuildCumulative(P, L);
	const double MinLoopLen = Pm.MinSizePx * 2.5;

	FShapeRecognizerParams SegPm = Pm;
	SegPm.MaxClosureRatio = Pm.SegClosureRatio;

	// 1. 閉じたループの候補を列挙して分類する
	TArray<FLoopPair> Pairs;
	FindLoopPairs(P, L, 0, n - 1, MinLoopLen, SegPm.MaxClosureRatio, Pairs);
	Pairs.Sort([](const FLoopPair& A, const FLoopPair& B) { return A.Closure < B.Closure; });
	if (Pairs.Num() > Pm.MaxCandidates) { Pairs.SetNum(Pm.MaxCandidates); }

	struct FCand
	{
		int32 I, J;
		FShapeResult R;
		double W;
	};
	TArray<FCand> Cands;
	TArray<FVector2D> Loop;
	for (const FLoopPair& Pair : Pairs)
	{
		CopyRange(P, Pair.I, Pair.J, Loop);
		FShapeResult R = ClassifyLoop(Loop, SegPm);
		if (R.Shape == EShapeType::Unknown) { continue; }
		const double Len = L[Pair.J] - L[Pair.I];
		const double W = R.Confidence * FMath::Pow(Len, 1.5) * FMath::Max(0.2, 1.0 - 3.0 * R.ClosureRatio);
		Cands.Add({ Pair.I, Pair.J, MoveTemp(R), W });
	}

	// 2. 重ならない区間の組で、重みの合計が最大になるものを選ぶ（動的計画法）
	TArray<double> Best;
	Best.Init(0.0, n);
	TArray<int32> Choice;
	Choice.Init(-1, n);
	for (int32 j = 1; j < n; ++j)
	{
		Best[j] = Best[j - 1];
		for (int32 c = 0; c < Cands.Num(); ++c)
		{
			if (Cands[c].J != j) { continue; }
			const double V = Best[Cands[c].I] + Cands[c].W;
			if (V > Best[j]) { Best[j] = V; Choice[j] = c; }
		}
	}
	TArray<int32> Chosen;
	for (int32 k = n - 1; k > 0;)
	{
		if (Choice[k] >= 0)
		{
			Chosen.Insert(Choice[k], 0);
			k = Cands[Choice[k]].I;
		}
		else
		{
			--k;
		}
	}

	// 3. ループが見つからなければ、全体を 1 記号として判定（閉じ方が緩い場合の救済）
	if (Chosen.Num() == 0)
	{
		FShapeResult Whole = ClassifyLoop(P, Pm);
		if (Whole.Shape == EShapeType::Unknown)
		{
			Out.Reason = FString::Printf(TEXT("記号として認識できません（%s）"), *Whole.Reason);
			return Out;
		}
		Cands.Add({ 0, n - 1, MoveTemp(Whole), 1.0 });
		Chosen.Add(Cands.Num() - 1);
	}

	// 3b. 小さすぎるループ（線の折り返しでできた小さな円や三角形など）は、記号として数えず無視する。
	// 基準は、同じ一筆書きの中で一番大きい記号の半径
	double MaxRadius = 0.0;
	for (int32 CIdx : Chosen) { MaxRadius = FMath::Max(MaxRadius, (double)Cands[CIdx].R.Radius); }
	const double MinRadius = Pm.MinRelativeSize * MaxRadius;
	int32 IgnoredSmall = 0;
	{
		TArray<int32> Kept;
		for (int32 CIdx : Chosen)
		{
			if (Cands[CIdx].R.Radius >= MinRadius) { Kept.Add(CIdx); }
			else { ++IgnoredSmall; }
		}
		Chosen = Kept;
	}

	// 4. 記号と、つなぎ線の区間を作る
	TArray<int32> SymI, SymJ;
	for (int32 CIdx : Chosen)
	{
		FShapeResult R = Cands[CIdx].R;
		R.StartIndex = Map[Cands[CIdx].I];
		R.EndIndex = Map[Cands[CIdx].J];
		SymI.Add(Cands[CIdx].I);
		SymJ.Add(Cands[CIdx].J);
		Out.Symbols.Add(MoveTemp(R));
	}

	// SymA = この区間の直前の記号、SymB = 直後の記号（なければ -1）
	struct FGap { int32 A, B; int32 SymA, SymB; };
	TArray<FGap> Gaps;
	{
		int32 Cur = 0;
		int32 PrevSym = -1;
		for (int32 s = 0; s < SymI.Num(); ++s)
		{
			if (SymI[s] > Cur) { Gaps.Add({ Cur, SymI[s], PrevSym, s }); }
			Cur = SymJ[s];
			PrevSym = s;
		}
		if (Cur < n - 1) { Gaps.Add({ Cur, n - 1, PrevSym, -1 }); }
	}
	for (const FGap& G : Gaps) { Out.Connectors.Add(FIntPoint(Map[G.A], Map[G.B])); }

	// 5. 制約の検査
	TArray<FString> Problems;
	auto Flag = [&Out](int32 S) { Out.ViolatingSymbols.AddUnique(S); };

	if (Out.Symbols.Num() > Pm.MaxSymbols)
	{
		Problems.Add(FString::Printf(TEXT("記号が多すぎます（最大 %d 個）"), Pm.MaxSymbols));
	}

	// 5a. つなぎ線の中に、認識できない閉じた線がないか
	for (const FGap& G : Gaps)
	{
		// 小さな閉じた線（折り返しなど）は無視する。記号と同じくらいの大きさのものだけを問題にする
		TArray<FLoopPair> GapLoops;
		FindLoopPairs(P, L, G.A, G.B, MinLoopLen, SegPm.MaxClosureRatio, GapLoops);
		bool bBigLoop = false;
		for (const FLoopPair& GL : GapLoops)
		{
			FVector2D BMin = P[GL.I], BMax = P[GL.I];
			for (int32 k = GL.I; k <= GL.J; ++k)
			{
				BMin = FVector2D(FMath::Min(BMin.X, P[k].X), FMath::Min(BMin.Y, P[k].Y));
				BMax = FVector2D(FMath::Max(BMax.X, P[k].X), FMath::Max(BMax.Y, P[k].Y));
			}
			// 外接矩形の対角線は、円なら半径の約 2.83 倍（2√2）。半径の基準 MinRadius に合わせて比べる
			if (FVector2D::Distance(BMin, BMax) >= FMath::Max((double)Pm.MinSizePx, 2.83 * MinRadius))
			{
				bBigLoop = true;
				break;
			}
		}
		if (bBigLoop)
		{
			Problems.Add(TEXT("認識できない閉じた線があります"));
			break;
		}
	}

	if (!Pm.bAllowOverlap)
	{
		TArray<TArray<FVector2D>> Poly;
		Poly.SetNum(SymI.Num());
		for (int32 s = 0; s < SymI.Num(); ++s) { CopyRange(P, SymI[s], SymJ[s], Poly[s]); }

		// 5b. 記号どうしの重なり・接触・入れ子
		for (int32 a = 0; a < Poly.Num(); ++a)
		{
			for (int32 b = a + 1; b < Poly.Num(); ++b)
			{
				const bool bTouch = ClosedPolylineDistance(Poly[a], Poly[b]) < Pm.OverlapMarginPx;
				const bool bNested = PointInPolygon(Out.Symbols[a].Center, Poly[b]) || PointInPolygon(Out.Symbols[b].Center, Poly[a]);
				if (bTouch || bNested)
				{
					Problems.Add(FString::Printf(TEXT("記号 %d と %d が重なっています"), a + 1, b + 1));
					Flag(a);
					Flag(b);
				}
			}
		}

		// 5c. つなぎ線が記号を横切っていないか
		for (const FGap& G : Gaps)
		{
			// 記号の輪郭をなぞって行き過ぎた尾（描き終わりが始点を通り過ぎた分など）は、
			// 記号の一部として許す。輪郭から離れたあとに、また近づいたら横切りとみなす
			int32 M0 = G.A, M1 = G.B;
			const double Hug = 2.0 * Pm.OverlapMarginPx;
			if (G.SymA >= 0)
			{
				while (M0 <= M1 && PointToClosedPolylineDistance(P[M0], Poly[G.SymA]) < Hug) { ++M0; }
			}
			if (G.SymB >= 0)
			{
				while (M1 >= M0 && PointToClosedPolylineDistance(P[M1], Poly[G.SymB]) < Hug) { --M1; }
			}

			for (int32 m = M0; m <= M1; m += 2)
			{
				if (G.SymA >= 0 && (L[m] - L[G.A]) < Pm.ConnectorEndMarginPx) { continue; }
				if (G.SymB >= 0 && (L[G.B] - L[m]) < Pm.ConnectorEndMarginPx) { continue; }
				for (int32 s = 0; s < Poly.Num(); ++s)
				{
					if (PointToClosedPolylineDistance(P[m], Poly[s]) < Pm.OverlapMarginPx)
					{
						Problems.Add(FString::Printf(TEXT("つなぎ線が記号 %d を横切っています"), s + 1));
						Flag(s);
					}
				}
			}
		}
	}

	// 6. まとめ
	Out.bValid = Out.Symbols.Num() > 0 && Problems.Num() == 0;
	if (Out.bValid)
	{
		Out.Reason = FString::Printf(TEXT("%d 個の記号"), Out.Symbols.Num());
		if (IgnoredSmall > 0)
		{
			Out.Reason += FString::Printf(TEXT("（小さなループ %d 個は無視）"), IgnoredSmall);
		}
	}
	else
	{
		TArray<FString> Unique;
		for (const FString& S : Problems) { Unique.AddUnique(S); }
		Out.Reason = FString::Join(Unique, TEXT(" / "));
	}
	return Out;
}

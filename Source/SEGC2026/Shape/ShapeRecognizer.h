#pragma once

#include "CoreMinimal.h"
#include "ShapeTypes.h"

/** 判定パラメータ。すべて調整可能。 */
struct SEGC2026_API FShapeRecognizerParams
{
	/** これ未満の点数は判定しない */
	int32 MinPoints = 8;
	/** バウンディングボックス対角線がこれ未満（px）なら小さすぎる */
	float MinSizePx = 40.f;
	/** 始点終点の距離 / 全長 の上限。超えたら「閉じていない」 */
	float MaxClosureRatio = 0.25f;
	/** リサンプリング点数 */
	int32 ResampleCount = 64;
	/** 円: 重心からの半径の変動係数(std/mean)がこの値で円スコア 0 */
	float CircleErrAtZeroScore = 0.25f;
	/** 三角形: 3頂点フィット誤差（bbox対角線比）がこの値でスコア 0 */
	float TriangleErrAtZeroScore = 0.12f;
	/** 勝者スコアがこれ未満なら Unknown */
	float MinConfidence = 0.45f;
};

/**
 * 純 C++ の図形判定器（UObject / エンジン状態に非依存。単体テスト可能）。
 *
 * 手順: 重複点除去 → 閉じ判定 → 閉曲線として等間隔リサンプリング →
 *   円スコア  = 重心からの半径のばらつきの小ささ
 *   三角スコア = 3頂点フィット（重心から最遠点A, Aから最遠点B, 直線ABから最遠点C）への距離の小ささ
 * → 高い方を採用し、MinConfidence 未満は Unknown。
 */
class SEGC2026_API FShapeRecognizer
{
public:
	static FShapeResult Recognize(const TArray<FVector2D>& RawPoints,
		const FShapeRecognizerParams& Params = FShapeRecognizerParams());
};

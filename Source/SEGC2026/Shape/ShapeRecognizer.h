#pragma once

#include "CoreMinimal.h"
#include "ShapeTypes.h"

/** 判定パラメータ。すべて調整可能。 */
struct SEGC2026_API FShapeRecognizerParams
{
	// ---- 1 つの閉じたループの判定 ----
	/** これ未満の点数は判定しない */
	int32 MinPoints = 8;
	/** バウンディングボックス対角線がこれ未満（px）なら小さすぎる */
	float MinSizePx = 60.f;
	/** 始点終点の距離 / 全長 の上限（ストローク全体を 1 記号として判定するとき） */
	float MaxClosureRatio = 0.25f;
	/** リサンプリング点数（偶数） */
	int32 ResampleCount = 128;

	/** 角検出: 曲率を測る窓の幅（リサンプリング点数での前後幅） */
	int32 CornerWindow = 4;
	/** 角検出: 凸の角と見なす最小の曲がり角（度） */
	float ConvexCornerDeg = 34.f;
	/** 角検出: 凹の角と見なす最小の曲がり角（度） */
	float ConcaveCornerDeg = 30.f;
	/** 五芒星（一筆書きの星）の先端と見なす最小の曲がり角（度） */
	float PentagramMinDeg = 100.f;

	/** 円: 重心からの半径の変動係数(std/mean)がこの値でスコア 0 */
	float CircleErrAtZeroScore = 0.25f;
	/** 多角形: 頂点フィット誤差（bbox対角線比）がこの値でスコア 0 */
	float PolyErrAtZeroScore = 0.12f;
	/** ハート: テンプレートとの平均距離（RMS 半径 = 1 に正規化）がこの値でスコア 0 */
	float HeartErrAtZeroScore = 0.4f;
	/** 勝者スコアがこれ未満なら Unknown */
	float MinConfidence = 0.45f;

	// ---- 一筆書きの分割と制約 ----
	/** 一筆書きから記号を切り出すときの、始点終点の距離 / 全長 の上限（全体判定より厳しい） */
	float SegClosureRatio = 0.12f;
	/** 記号の候補として判定する区間の最大数 */
	int32 MaxCandidates = 1500;
	/**
	 * 同じ一筆書きの中で一番大きい記号の半径に対して、この割合未満の半径のループは、
	 * 記号として数えず無視する（線の折り返しでできた小さなループなど）
	 */
	float MinRelativeSize = 0.35f;
	/** 1 本の一筆書きに含められる記号の最大数 */
	int32 MaxSymbols = 6;
	/** true なら、重なり・横切りの制約を無視する */
	bool bAllowOverlap = false;
	/** 記号どうし／つなぎ線と記号が、この距離（px）より近いと「重なり」とみなす */
	float OverlapMarginPx = 4.f;
	/** つなぎ線の両端のうち、記号につながる側のこの長さ（px）は横切り判定から除く */
	float ConnectorEndMarginPx = 14.f;
};

/**
 * 純 C++ の図形判定器（UObject / エンジン状態に非依存。単体テスト可能）。
 *
 * 1 記号の判定: 閉曲線として等間隔リサンプリング → 曲率から角を検出 →
 *   ○ : 角がなく、重心からの半径が一定
 *   ▷ : 凸の角が 3 つ（先端の向きも返す）
 *   □ : 凸の角が 4 つでほぼ直角（向きは問わない。回転した ◇ も □）
 *   五角形 : 凸の角が 5 つでほぼ 108 度
 *   ☆ : 凸 5 + 凹 3〜5 の輪郭の星、または 5 つの鋭い角の五芒星（一筆書きの星）
 *   ♡ : 凹みがあり、ハートのテンプレートに近い
 * 一筆書きの判定: 閉じたループの候補を列挙し、動的計画法で「信頼度×長さ」が最大になる
 * 重ならない区間の組を選ぶ。その後、制約（個数・記号どうしの重なり・入れ子・つなぎ線の横切り・
 * 認識できない閉じた線）を検査する。
 */
class SEGC2026_API FShapeRecognizer
{
public:
	/** ストローク全体を 1 つの閉じたループとして判定する */
	static FShapeResult Recognize(const TArray<FVector2D>& RawPoints,
		const FShapeRecognizerParams& Params = FShapeRecognizerParams());

	/** 一筆書き 1 本から、複数の記号を切り出して判定する */
	static FStrokeResult RecognizeStroke(const TArray<FVector2D>& RawPoints,
		const FShapeRecognizerParams& Params = FShapeRecognizerParams());
};

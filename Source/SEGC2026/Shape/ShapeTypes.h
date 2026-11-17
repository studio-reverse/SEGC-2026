#pragma once

#include "CoreMinimal.h"
#include "ShapeTypes.generated.h"

/** 判定される図形 */
UENUM(BlueprintType)
enum class EShapeType : uint8
{
	Unknown  UMETA(DisplayName = "Unknown"),
	Circle   UMETA(DisplayName = "Circle (○)"),
	Triangle UMETA(DisplayName = "Triangle (▷)"),
	Square   UMETA(DisplayName = "Square (□)"), // 回転した四角形（◇）も含む

	Pentagon UMETA(DisplayName = "Pentagon"),
	Star     UMETA(DisplayName = "Star (☆)"),
	Heart    UMETA(DisplayName = "Heart (♡)"),
};

/** 図形の英語名（ログ用） */
SEGC2026_API const TCHAR* GetShapeName(EShapeType Shape);

/** 1 つの記号（閉じた 1 ループ）の判定結果 */
USTRUCT(BlueprintType)
struct SEGC2026_API FShapeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	EShapeType Shape = EShapeType::Unknown;

	/** 0..1。MinConfidence 未満なら Shape は Unknown になる */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	float Confidence = 0.f;

	/** 図形ごとのスコア（0..1） */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TMap<EShapeType, float> Scores;

	/** 始点と終点の距離 / ループの全長 */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	float ClosureRatio = 0.f;

	/** 記号の中心（面積重心。入力と同じ座標系） */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	FVector2D Center = FVector2D::ZeroVector;

	/** 中心からの平均半径 */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	float Radius = 0.f;

	/**
	 * Triangle のとき、最も鋭い頂点（先端）の方向（重心→頂点の単位ベクトル）。
	 * 画面座標系 (X右, Y下)。右向き ▷ なら (1, 0) 付近。他の図形では 0。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	FVector2D Direction = FVector2D::ZeroVector;

	/** この記号が占める、入力ストロークの点インデックス範囲 */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	int32 StartIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	int32 EndIndex = 0;

	/** Unknown になった理由、または判定の要約（デバッグ用） */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	FString Reason;

	/** 判定に使った角（頂点）。デバッグ描画用。入力と同じ座標系 */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FVector2D> DebugVertices;
};

/** 一筆書き 1 本ぶんの判定結果（複数の記号を含みうる） */
USTRUCT(BlueprintType)
struct SEGC2026_API FStrokeResult
{
	GENERATED_BODY()

	/** 描いた順の記号 */
	UPROPERTY(BlueprintReadOnly, Category = "Stroke")
	TArray<FShapeResult> Symbols;

	/** 記号と記号の間（および前後）の「つなぎ線」の点インデックス範囲 (X=開始, Y=終了) */
	UPROPERTY(BlueprintReadOnly, Category = "Stroke")
	TArray<FIntPoint> Connectors;

	/** 記号が 1 つ以上あり、かつ制約（重なり・横切り・個数など）をすべて満たす */
	UPROPERTY(BlueprintReadOnly, Category = "Stroke")
	bool bValid = false;

	/** 制約違反に関わった記号（Symbols のインデックス） */
	UPROPERTY(BlueprintReadOnly, Category = "Stroke")
	TArray<int32> ViolatingSymbols;

	/** 結果の要約、または無効だった理由 */
	UPROPERTY(BlueprintReadOnly, Category = "Stroke")
	FString Reason;
};

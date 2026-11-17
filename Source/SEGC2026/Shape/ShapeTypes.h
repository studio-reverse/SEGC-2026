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
};

/** 図形判定の結果 */
USTRUCT(BlueprintType)
struct SEGC2026_API FShapeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	EShapeType Shape = EShapeType::Unknown;

	/** 0..1。MinConfidence 未満なら Shape は Unknown になる */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	float Confidence = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	float CircleScore = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	float TriangleScore = 0.f;

	/** 始点と終点の距離 / ストローク全長 */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	float ClosureRatio = 0.f;

	/**
	 * Triangle のとき、最も鋭い頂点（先端）の方向（重心→頂点の単位ベクトル）。
	 * 画面座標系 (X右, Y下)。右向き ▷ なら (1, 0) 付近。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	FVector2D Direction = FVector2D::ZeroVector;

	/** Unknown になった理由、または判定の要約（デバッグ用） */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	FString Reason;

	/** Triangle 判定に使った 3 頂点（デバッグ描画用。入力と同じ座標系） */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FVector2D> DebugVertices;
};

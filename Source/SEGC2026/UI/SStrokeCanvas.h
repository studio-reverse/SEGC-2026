#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Shape/ShapeTypes.h"

DECLARE_DELEGATE_OneParam(FOnShapeResultNative, const FShapeResult&);

/**
 * ストローク描画 + 図形判定キャンバス（純 Slate）。
 * 不透明な暗い背景で画面を覆い、説明文・ストローク・判定結果を自前で描画する。
 * 左ドラッグで描いて離すと判定、右クリックでクリア。
 */
class SEGC2026_API SStrokeCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SStrokeCanvas) {}
		SLATE_EVENT(FOnShapeResultNative, OnShapeResult)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void Clear();

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D(200.0, 200.0); }
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	void AddPoint(const FGeometry& Geo, const FPointerEvent& Event);

	FOnShapeResultNative OnShapeResult;

	TArray<FVector2D> Stroke; // ローカル座標
	bool bDrawing = false;
	bool bHasResult = false;
	FShapeResult LastResult;

	float MinPointDistance = 4.f;
	float Thickness = 5.f;
};

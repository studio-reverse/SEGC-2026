#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Shape/ShapeTypes.h"

DECLARE_DELEGATE_OneParam(FOnStrokeResultNative, const FStrokeResult&);

/**
 * ストローク描画 + 図形判定キャンバス（純 Slate）。
 * 不透明な暗い背景で画面を覆い、説明文・ストローク・判定結果を自前で描画する。
 * 左ドラッグで一筆書きし、離すと判定する（1 本の一筆書きに複数の記号を描ける）。右クリックでクリア。
 * 検知した各記号の中心に点を表示する。
 */
class SEGC2026_API SStrokeCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SStrokeCanvas) {}
		SLATE_EVENT(FOnStrokeResultNative, OnStrokeResult)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void Clear();

	/** マウス入力を介さず、点列を直接渡して判定・表示する（デモや自動確認用） */
	void SetStrokeAndRecognize(const TArray<FVector2D>& Points);

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

	FOnStrokeResultNative OnStrokeResult;

	TArray<FVector2D> Stroke; // ローカル座標
	bool bDrawing = false;
	bool bHasResult = false;
	FStrokeResult LastResult;

	float MinPointDistance = 4.f;
	float Thickness = 5.f;

	/** 中心点（丸）用のブラシ。直径 14px */
	FSlateRoundedBoxBrush DotBrush = FSlateRoundedBoxBrush(FLinearColor::White, 7.f);
};

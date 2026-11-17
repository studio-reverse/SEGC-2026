#include "UI/SStrokeCanvas.h"
#include "Shape/ShapeRecognizer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"

namespace
{
	FLinearColor ShapeColor(EShapeType Shape)
	{
		switch (Shape)
		{
		case EShapeType::Circle:   return FLinearColor(0.10f, 0.90f, 1.00f);
		case EShapeType::Triangle: return FLinearColor(1.00f, 0.60f, 0.10f);
		case EShapeType::Square:   return FLinearColor(0.40f, 1.00f, 0.40f);
		case EShapeType::Pentagon: return FLinearColor(1.00f, 0.90f, 0.20f);
		case EShapeType::Star:     return FLinearColor(1.00f, 0.95f, 0.45f);
		case EShapeType::Heart:    return FLinearColor(1.00f, 0.40f, 0.65f);
		default:                   return FLinearColor(1.00f, 0.25f, 0.25f);
		}
	}

	/** 画面表示用の名前（記号つき） */
	FString ShapeLabel(EShapeType Shape)
	{
		switch (Shape)
		{
		case EShapeType::Circle:   return TEXT("○ 円");
		case EShapeType::Triangle: return TEXT("▷ 三角形");
		case EShapeType::Square:   return TEXT("□ 四角形");
		case EShapeType::Pentagon: return TEXT("五角形");
		case EShapeType::Star:     return TEXT("☆ 星");
		case EShapeType::Heart:    return TEXT("♡ ハート");
		default:                   return TEXT("？ 不明");
		}
	}

	/** 画面上の (X, Y) に文字列を描く。bCenterX なら X を中心として描く */
	void DrawText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text,
		const FSlateFontInfo& Font, const FLinearColor& Color, float X, float Y, bool bCenterX)
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D Size(Measure->Measure(Text, Font));
		const float DrawX = bCenterX ? X - (float)Size.X * 0.5f : X;
		FSlateDrawElement::MakeText(Out, Layer,
			Geo.ToPaintGeometry(FVector2f((float)Size.X, (float)Size.Y), FSlateLayoutTransform(FVector2f(DrawX, Y))),
			Text, Font, ESlateDrawEffect::None, Color);
	}

	void DrawLines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const TArray<FVector2D>& Pts,
		const FLinearColor& Color, float Thickness)
	{
		if (Pts.Num() >= 2)
		{
			FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, Thickness);
		}
	}
}

void SStrokeCanvas::Construct(const FArguments& InArgs)
{
	OnStrokeResult = InArgs._OnStrokeResult;
	SetCursor(EMouseCursor::Crosshairs);
}

void SStrokeCanvas::Clear()
{
	Stroke.Reset();
	bDrawing = false;
	bHasResult = false;
	LastResult = FStrokeResult();
}

void SStrokeCanvas::SetStrokeAndRecognize(const TArray<FVector2D>& Points)
{
	Stroke = Points;
	bDrawing = false;
	LastResult = FShapeRecognizer::RecognizeStroke(Stroke);
	bHasResult = true;
	OnStrokeResult.ExecuteIfBound(LastResult);
}

void SStrokeCanvas::AddPoint(const FGeometry& Geo, const FPointerEvent& Event)
{
	const FVector2D Local(Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (Stroke.Num() == 0 || FVector2D::Distance(Stroke.Last(), Local) >= MinPointDistance)
	{
		Stroke.Add(Local);
	}
}

FReply SStrokeCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Clear();
		bDrawing = true;
		AddPoint(MyGeometry, MouseEvent);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Clear();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SStrokeCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDrawing)
	{
		AddPoint(MyGeometry, MouseEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SStrokeCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDrawing && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		AddPoint(MyGeometry, MouseEvent);
		bDrawing = false;

		LastResult = FShapeRecognizer::RecognizeStroke(Stroke);
		bHasResult = true;
		OnStrokeResult.ExecuteIfBound(LastResult);

		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

int32 SStrokeCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size(AllottedGeometry.GetLocalSize());
	const float CX = (float)Size.X * 0.5f;

	// 1. 背景（不透明。後ろの 3D シーンを隠す）
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.035f, 0.045f, 0.07f, 1.f));
	++LayerId;

	// 2. 説明
	DrawText(OutDrawElements, LayerId, AllottedGeometry, TEXT("左ドラッグで一筆書き。○ ▷ □ 五角形 ☆ ♡ を、1 本の線に複数描けます"),
		FCoreStyle::GetDefaultFontStyle("Regular", 20), FLinearColor(0.75f, 0.8f, 0.9f, 1.f), CX, 20.f, true);
	DrawText(OutDrawElements, LayerId, AllottedGeometry, TEXT("各記号は閉じて描き、重ならないようにしてください。右クリック: 消去     Esc: 終了"),
		FCoreStyle::GetDefaultFontStyle("Regular", 14), FLinearColor(0.5f, 0.55f, 0.65f, 1.f), CX, 52.f, true);
	++LayerId;

	if (Stroke.Num() < 2)
	{
		return LayerId;
	}

	// 3. ストローク。判定前は白。判定後は全体を灰色（つなぎ線）にして、記号の区間だけ色を付ける
	if (!bHasResult)
	{
		DrawLines(OutDrawElements, LayerId, AllottedGeometry, Stroke, FLinearColor::White, Thickness);
		return LayerId + 1;
	}

	DrawLines(OutDrawElements, LayerId, AllottedGeometry, Stroke, FLinearColor(0.45f, 0.48f, 0.55f), 3.f);
	++LayerId;

	const FStrokeResult& Res = LastResult;
	for (int32 s = 0; s < Res.Symbols.Num(); ++s)
	{
		const FShapeResult& Sym = Res.Symbols[s];
		const bool bBad = Res.ViolatingSymbols.Contains(s);
		const FLinearColor Color = bBad ? FLinearColor(1.f, 0.25f, 0.25f) : ShapeColor(Sym.Shape);

		TArray<FVector2D> Seg;
		for (int32 k = Sym.StartIndex; k <= Sym.EndIndex && Stroke.IsValidIndex(k); ++k) { Seg.Add(Stroke[k]); }
		DrawLines(OutDrawElements, LayerId, AllottedGeometry, Seg, Color, Thickness);

		// デバッグ: 角を結ぶ線
		if (Sym.DebugVertices.Num() >= 3)
		{
			TArray<FVector2D> Poly = Sym.DebugVertices;
			const FVector2D First = Poly[0];
			Poly.Add(First);
			DrawLines(OutDrawElements, LayerId + 1, AllottedGeometry, Poly, FLinearColor(1.f, 1.f, 0.2f, 0.45f), 1.5f);
		}

		// 三角形: 先端の向き
		if (Sym.Shape == EShapeType::Triangle && !Sym.Direction.IsNearlyZero())
		{
			TArray<FVector2D> Arrow = { Sym.Center, Sym.Center + Sym.Direction * (double)Sym.Radius };
			DrawLines(OutDrawElements, LayerId + 1, AllottedGeometry, Arrow, FLinearColor(1.f, 1.f, 0.2f, 0.9f), 2.f);
		}
	}
	++LayerId;
	++LayerId;

	// 4. 各記号の中心の点と名前
	for (int32 s = 0; s < Res.Symbols.Num(); ++s)
	{
		const FShapeResult& Sym = Res.Symbols[s];
		const bool bBad = Res.ViolatingSymbols.Contains(s);
		const FLinearColor Color = bBad ? FLinearColor(1.f, 0.25f, 0.25f) : ShapeColor(Sym.Shape);

		const float Cx = (float)Sym.Center.X;
		const float Cy = (float)Sym.Center.Y;
		const float OutlineSize = 20.f;
		const float DotSize = 14.f;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(OutlineSize, OutlineSize), FSlateLayoutTransform(FVector2f(Cx - OutlineSize * 0.5f, Cy - OutlineSize * 0.5f))),
			&DotBrush, ESlateDrawEffect::None, FLinearColor(0.035f, 0.045f, 0.07f, 1.f));
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(DotSize, DotSize), FSlateLayoutTransform(FVector2f(Cx - DotSize * 0.5f, Cy - DotSize * 0.5f))),
			&DotBrush, ESlateDrawEffect::None, Color);

		const FString Label = FString::Printf(TEXT("%d  %s  %d%%"), s + 1, *ShapeLabel(Sym.Shape), FMath::RoundToInt(Sym.Confidence * 100.f));
		DrawText(OutDrawElements, LayerId + 2, AllottedGeometry, Label, FCoreStyle::GetDefaultFontStyle("Bold", 16), Color,
			Cx, Cy - 34.f, true);
	}
	LayerId += 3;

	// 5. 結果（下部）
	FString Title;
	for (int32 s = 0; s < Res.Symbols.Num(); ++s)
	{
		if (s > 0) { Title += TEXT("  →  "); }
		Title += ShapeLabel(Res.Symbols[s].Shape);
	}
	if (Title.IsEmpty()) { Title = TEXT("？ 認識できません"); }

	const FLinearColor TitleColor = Res.bValid ? FLinearColor(0.55f, 1.f, 0.65f) : FLinearColor(1.f, 0.35f, 0.35f);
	const float Y = (float)Size.Y - 130.f;
	DrawText(OutDrawElements, LayerId, AllottedGeometry, Title,
		FCoreStyle::GetDefaultFontStyle("Bold", 36), TitleColor, CX, Y, true);
	DrawText(OutDrawElements, LayerId, AllottedGeometry,
		(Res.bValid ? TEXT("OK: ") : TEXT("NG: ")) + Res.Reason,
		FCoreStyle::GetDefaultFontStyle("Regular", 16), TitleColor, CX, Y + 58.f, true);

	return LayerId + 1;
}

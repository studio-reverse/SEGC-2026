#include "UI/SStrokeCanvas.h"
#include "Shape/ShapeRecognizer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"

void SStrokeCanvas::Construct(const FArguments& InArgs)
{
	OnShapeResult = InArgs._OnShapeResult;
	SetCursor(EMouseCursor::Crosshairs);
}

void SStrokeCanvas::Clear()
{
	Stroke.Reset();
	bDrawing = false;
	bHasResult = false;
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

		LastResult = FShapeRecognizer::Recognize(Stroke);
		bHasResult = true;
		OnShapeResult.ExecuteIfBound(LastResult);

		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

namespace
{
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
	DrawText(OutDrawElements, LayerId, AllottedGeometry, TEXT("マウスの左ボタンを押しながら ○ か ▷ を描いて、離してください"),
		FCoreStyle::GetDefaultFontStyle("Regular", 22), FLinearColor(0.75f, 0.8f, 0.9f, 1.f), CX, 24.f, true);
	DrawText(OutDrawElements, LayerId, AllottedGeometry, TEXT("右クリック: 消去     Esc: 終了"),
		FCoreStyle::GetDefaultFontStyle("Regular", 14), FLinearColor(0.5f, 0.55f, 0.65f, 1.f), CX, 58.f, true);
	++LayerId;

	// 3. ストローク（判定前=白 / 円=シアン / 三角=オレンジ / 不明=赤）
	FLinearColor Color = FLinearColor::White;
	if (bHasResult)
	{
		switch (LastResult.Shape)
		{
		case EShapeType::Circle:   Color = FLinearColor(0.1f, 0.9f, 1.f); break;
		case EShapeType::Triangle: Color = FLinearColor(1.f, 0.6f, 0.1f); break;
		default:                   Color = FLinearColor(1.f, 0.25f, 0.25f); break;
		}
	}
	if (Stroke.Num() >= 2)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
			Stroke, ESlateDrawEffect::None, Color, true, Thickness);
	}
	++LayerId;

	// 4. 判定に使った三角形（デバッグ）
	if (bHasResult && LastResult.DebugVertices.Num() == 3 && LastResult.Shape == EShapeType::Triangle)
	{
		TArray<FVector2D> Tri = LastResult.DebugVertices;
		const FVector2D First = Tri[0];
		Tri.Add(First);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
			Tri, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 0.2f, 0.7f), true, 2.f);
	}
	++LayerId;

	// 5. 判定結果（下部に大きく）
	if (bHasResult)
	{
		FString Title;
		switch (LastResult.Shape)
		{
		case EShapeType::Circle:   Title = FString::Printf(TEXT("○  Circle   %d%%"), FMath::RoundToInt(LastResult.Confidence * 100.f)); break;
		case EShapeType::Triangle: Title = FString::Printf(TEXT("▷  Triangle   %d%%"), FMath::RoundToInt(LastResult.Confidence * 100.f)); break;
		default:                   Title = TEXT("？  Unknown"); break;
		}
		const float Y = (float)Size.Y - 150.f;
		DrawText(OutDrawElements, LayerId, AllottedGeometry, Title,
			FCoreStyle::GetDefaultFontStyle("Bold", 48), Color, CX, Y, true);

		FString Detail = LastResult.Reason;
		if (LastResult.Shape == EShapeType::Triangle)
		{
			Detail += FString::Printf(TEXT("   dir=(%.2f, %.2f)"), LastResult.Direction.X, LastResult.Direction.Y);
		}
		DrawText(OutDrawElements, LayerId, AllottedGeometry, Detail,
			FCoreStyle::GetDefaultFontStyle("Regular", 16), FLinearColor(0.6f, 0.65f, 0.75f, 1.f), CX, Y + 70.f, true);
	}

	return LayerId + 1;
}

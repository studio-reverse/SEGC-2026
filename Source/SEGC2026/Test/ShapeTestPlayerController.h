#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Shape/ShapeTypes.h"
#include "ShapeTestPlayerController.generated.h"

class SStrokeCanvas;

/** テスト用: BeginPlay で全画面のストローク キャンバスを出し、判定結果をログにも出す */
UCLASS()
class SEGC2026_API AShapeTestPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShapeTestPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleStroke(const FStrokeResult& Result);

	TSharedPtr<SStrokeCanvas> Canvas;
};

/** テストレベルの World Settings で GameMode Override に指定する */
UCLASS()
class SEGC2026_API AShapeTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShapeTestGameMode();
};

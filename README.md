# SQUARE ENIX GAME CONTEST 2026

## 動作環境

| 項目 | 内容 |
|---|---|
| OS | Windows 11 |
| Unreal Engine | **5.8**（Epic Games Launcher などでインストール） |
| IDE | **Visual Studio 2022 17.14 以降**。2022 の 17.13 以前は使えません（下記） |
| GPU / RHI | DirectX 12 |

- 別バージョンのエンジンでは開かないでください。アセットが新しい形式に書き換わり、元に戻せません。
- レイトレーシングは `Config/DefaultEngine.ini` で **オフ**（`r.RayTracing=False`）にしています。有効だと、環境（確認したのは AMD RX 7600）によっては PIE 開始直後に D3D12 の GPU メモリ確保エラーでエディタが落ちます。

## Git LFS

バイナリファイルは **Git LFS** で管理しています。clone する前に、`git` と `git-lfs` を入れてください（Git for Windows に `git-lfs` が同梱されています）。

LFS の対象は `.gitattributes` に書かれています。

| 種類 | 拡張子 |
|---|---|
| UE のアセット | `.uasset` `.umap` `.upk` `.udk` |
| 3D・画像 | `.fbx` `.obj` `.blend` `.psd` `.png` `.jpg` `.jpeg` `.tga` `.exr` `.hdr` |
| 音声・動画 | `.wav` `.mp3` `.ogg` `.mp4` `.mov` |

- コマンドラインで clone する場合は、初回に一度 `git lfs install` を実行してください。
- `git-lfs` が入っていないと、対象ファイルは本体ではなく、数行のテキスト（ポインタ）のまま取得されます。この状態でエディタを開くと、アセットを読み込めません。
- そうなったときは、`git-lfs` を入れたうえで、リポジトリのフォルダで次を実行してください。

  ```powershell
  git lfs install
  git lfs pull
  ```

## セットアップ

1. リポジトリを clone します。
2. `SEGC2026.uproject` を右クリックして **Generate Visual Studio project files** を実行します（`.sln` は git 管理していません）。
3. 初回は C++ のビルドが必要です。次のどちらかで行います。
   - `SEGC2026.uproject` をダブルクリックし、「ビルドしますか？」に **はい** と答えます。
   - コマンドラインから実行します。**エディタを閉じてから**実行してください。

   ```powershell
   & "<UE_5.8のパス>\Engine\Build\BatchFiles\Build.bat" SEGC2026Editor Win64 Development "-Project=<リポジトリ>\SEGC2026.uproject" -WaitMutex
   ```
4. エディタが開いたら `Content/ShapeTest/Lvl_ShapeTest` を開き、PIE（再生）します。左ドラッグで図形を描き、離すと判定されます。右クリックで消去、Esc で終了です。

## ディレクトリ構成

| パス | 役割 | git |
|---|---|---|
| `SEGC2026.uproject` | プロジェクト本体（エンジン、プラグイン、C++ モジュールの宣言） | 管理する |
| `Source/` | C++ コード（`SEGC2026` モジュール） | 管理する |
| `Content/` | アセット（レベル、Blueprint、マテリアルなど。`.uasset` はバイナリ） | 管理する |
| `Config/` | 設定（`.ini`） | 管理する |
| `.mcp.json` | Claude Code から Unreal Editor へ接続する MCP 設定 | 管理する |
| `Binaries/` `Intermediate/` `DerivedDataCache/` `Saved/` | ビルド成果物、キャッシュ、ログ | 管理しない |

`Content/__ExternalActors__/` と `Content/__ExternalObjects__/` は、One File Per Actor 機能が自動生成するフォルダです。手で触らないでください。

### C++ の構成（`Source/SEGC2026/`）

| パス | 内容 |
|---|---|
| `Shape/ShapeRecognizer.*` | 図形判定器（純 C++。エンジンの状態に依存しません） |
| `Shape/ShapeTypes.h` | `EShapeType`（Unknown / Circle / Triangle）と `FShapeResult` |
| `Shape/ShapeRecognizerTests.cpp` | 判定器の Automation Test |
| `UI/SStrokeCanvas.*` | 全画面の描画キャンバス（Slate） |
| `Test/ShapeTestPlayerController.*` | テストレベル用の PlayerController / GameMode |

### テストの実行

エディタを閉じた状態で、次のコマンドを実行します。

```powershell
& "<UE_5.8のパス>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<リポジトリ>\SEGC2026.uproject" "-ExecCmds=Automation RunTests SEGC2026.Shape; Quit" -unattended -nullrhi -nosplash "-abslog=<ログの出力先>.log"
```

エディタ内で実行する場合は、**ツール > テスト オートメーション** で `SEGC2026.Shape` を選びます。

## 命名規則と大文字小文字

方針は Epic 公式のスタイルに合わせます。

### アセット名
`プレフィックス_PascalCase` の形式にします。

| 種類 | プレフィックス | 例 |
|---|---|---|
| Blueprint | `BP_` | `BP_TopDownCharacter` |
| Widget Blueprint | `WBP_` | `WBP_Result` |
| Static Mesh | `SM_` | `SM_Cube` |
| Material / Material Instance | `M_` / `MI_` | `M_Cursor` / `MI_Colorway` |
| Texture | `T_` | `T_Arrow` |
| Niagara System | `NS_` | `NS_Cast` |
| レベル | `Lvl_` | `Lvl_ShapeTest` |
| Input Action / Mapping Context | `IA_` / `IMC_` | `IA_SetDestination_Click` |

フォルダ名は PascalCase にし、スペースや日本語は使いません。

### C++
- クラス名の接頭辞は、UObject 系が `U`、Actor 系が `A`、構造体が `F`、enum が `E`、インターフェースが `I`、Slate ウィジェットが `S` です（例: `UStrokeComponent`、`FShapeResult`、`SStrokeCanvas`）。
- 型名、関数名、変数名は PascalCase にします。bool の変数は `b` を付けます（`bDrawing`）。
- ファイル名は、そのファイルが定義するクラスから接頭辞を除いた名前にします（`FShapeRecognizer` は `ShapeRecognizer.h`）。
- `#include "Shape/ShapeRecognizer.h"` のように、パスの**大文字小文字をファイル名と一致**させます。Windows では動いても、他の OS ではビルドが通らなくなります。

### Windows と git の注意
- Windows のファイルシステムは大文字小文字を区別しませんが、git は区別します。**大文字小文字だけが違う名前のファイルやフォルダを作らないでください。**
- 大文字小文字だけを変えるリネームは、エクスプローラーではなく、**エディタのコンテンツブラウザ**（アセット）か `git mv`（C++ など）で行います。エクスプローラーで変えると、git が変更を拾えないことがあります。
- アセットの移動やリネームも、エクスプローラーではなくコンテンツブラウザで行います（参照が壊れるのを防ぐため）。

## Claude Code から Unreal Editor を操作する（MCP）

エディタ内の MCP サーバーに Claude Code が接続すると、アクタの配置、アセットの編集、PIE の制御などをチャットから実行できます。

### 前提
- `SEGC2026.uproject` で、次のプラグインを有効にしています。追加の作業は不要です。
  - `ModelContextProtocol`
  - `AllToolsets`
- `.mcp.json`（プロジェクト直下）に、接続先として `http://127.0.0.1:8000/mcp` を書いています。

### 手順
1. **Unreal Editor を起動します。** 起動すると、ログに `Starting MCP server on port 8000` と出て、ポート 8000 でサーバーが立ち上がります。エディタが起動していない間は、MCP は接続できません。
2. このリポジトリのフォルダで Claude Code を起動します（`.mcp.json` が自動で読まれます）。
3. 初回は、プロジェクトの MCP サーバーを信頼するかを聞かれるので承認します。
4. 接続を確認します。Claude Code の `/mcp` で `unreal-mcp` が `connected` になっていれば成功です。

### 使い方
- 公開されているのは `list_toolsets`、`describe_toolset`、`call_tool` の 3 つです。
  - まず `list_toolsets` でツールセットの一覧を見ます。
  - 次に `describe_toolset` で個々のツールと引数を調べます。
  - 最後に `call_tool` で実行します。`tool_name` には**ツールセット名を除いた名前**を渡します。
- 主なツールセット: シーン（アクタの配置）、アセット、Blueprint、オブジェクトのプロパティ、Material、UMG、Niagara、Sequencer、エディタ操作（PIE、ビューポート）など。

### 注意
- **エディタを閉じたり、落ちたりすると接続が切れます。** 再起動後は、数十秒待ってから再接続されます。
- C++ をビルドするときはエディタを閉じる必要があるため、その間は MCP を使えません。
- ポートを変えるときは、エディタの起動引数に `-ModelContextProtocolPort=N` を付け、`.mcp.json` の URL も合わせます。
- 接続先は `127.0.0.1`（自分の PC）だけです。外部のネットワークには公開しないでください。
- プラグインのログに出る通り、MCP 経由で LLM に送られるデータは UE EULA 第 6(e) 条の対象です。利用する LLM プロバイダーの学習利用の設定を確認してください。

## トラブルシューティング

| 症状 | 対処 |
|---|---|
| PIE を始めるとすぐエディタが落ちる（D3D12 `CreateHeap`） | レイトレーシングが有効になっていないか確認します（`r.RayTracing`） |
| ビルドで `include ファイルを開けません` と出る | `Source/SEGC2026/SEGC2026.Build.cs` の `PublicIncludePaths` と、`#include` のパスの大文字小文字を確認します |
| ビルドが「ファイルがロックされている」で失敗する | 起動中のエディタを閉じます |
| 原因不明のビルドエラー | `Binaries/` と `Intermediate/` を削除して、プロジェクトファイルを再生成し、再ビルドします |
| MCP が `failed` のまま | エディタが起動し、ログに `Starting MCP server on port 8000` が出ているか確認します |

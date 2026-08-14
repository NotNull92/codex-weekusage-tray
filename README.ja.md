<p align="center">
  <img src="docs/logo/logo.png" width="160" alt="Codex Weekly Limit Tray ロゴ">
</p>

<p align="center">
  <a href="README.md">English</a> | <a href="README.ko.md">한국어</a> | <b>日本語</b> | <a href="README.zh-CN.md">简体中文</a> | <a href="README.ru.md">Русский</a>
</p>

# Codex Weekly Limit Tray

Codex の週間利用上限を Windows の通知領域（システムトレイ）に表示する、小さなネイティブアプリです。

トレイアイコンには `73` のような大きな太字の数字が表示されます。上限の情報をまだ取得できていない場合は `--` が表示されます。アイコンをクリックすると Codex パネルが開きます。パネルには残量、リセット時刻、残り時間が表示されます。

<p align="center">
  <img src="docs/screenshot/tray.png" width="220" alt="Windows のシステムトレイに数字 39 が表示されたアイコン"><br>
  <sub><em>トレイアイコン</em></sub>
  <br><br>
  <img src="docs/screenshot/panel.png" width="320" alt="残り 39%、使用量、リセット日、残り時間を表示する詳細パネル"><br>
  <sub><em>詳細パネル</em></sub>
</p>

## 必要なもの

- x64 版の Windows 10 または Windows 11
- 公式の [Codex CLI](https://developers.openai.com/codex/cli/)

ソースからビルドする場合は、MSYS2 UCRT64 の C++ コンパイラをインストールしてください。ビルドスクリプトはまず `%USERPROFILE%\msys64\ucrt64\bin\g++.exe` を確認し、次に `C:\msys64\ucrt64\bin\g++.exe` を確認します。`CXX` 環境変数でフルパスを指定することもできます。

アプリは Windows のユーザーごとの Local AppData 既知フォルダーを通じて Codex を検出します。通常の場所は次のとおりです。

```text
%LOCALAPPDATA%\Programs\OpenAI\Codex\bin\codex.exe
```

## アプリの実行

公開リリースを使う場合は、ZIP を展開して `CodexWeekUsageTray.exe` を実行します。

ソースからビルドして実行する場合は次のようにします。

```powershell
native\build.cmd
native\out\CodexWeekUsageTray.exe
```

アイコンは通常の Windows システムトレイに表示されます。時計や日付を覆うことはありません。

### 初回起動時の SmartScreen 警告

リリースの EXE はまだ Authenticode 署名されていないため、初回起動時に **Windows によって PC が保護されました** という警告が表示されます。信頼する前にファイルを確認してください。

```powershell
Get-FileHash .\CodexWeekUsageTray.exe -Algorithm SHA256
```

結果を、同じリリースの `SHA256SUMS` ファイル内の該当する行と照合します。値が一致したら **詳細情報 > 実行** を選択してください。署名されていない実行ファイルをそもそも実行したくない場合は、上の手順でソースからビルドしてください。

### トレイアイコンを表示する

1. `CodexWeekUsageTray.exe` を実行します。
2. **設定 > 個人用設定 > タスク バー > 他のシステム トレイ アイコン** を開きます。
3. **CodexWeekUsageTray** を見つけて **オン** にします。

これで Windows はアイコンをオーバーフロー（隠し）リストに置かず、システムトレイに表示します。

## パネルの使い方

- **トレイアイコンを左クリック**: 詳細パネルを開く／隠す。
- **トレイアイコンを右クリック** して **Show panel** を選択: トレイメニューから詳細パネルを開く。
- **Refresh**: 上限を今すぐ確認します。
- **Close**: パネルだけを隠します。トレイアプリは動作し続けます。
- **Sign in**: 必要なときに、既定のブラウザーで公式の Codex ChatGPT サインインページを開きます。
- **Check**: サインイン後にアカウントを再確認します。

Codex から上限の更新が届くと、アプリはすぐに反映します。またフォールバックとして、最短 5 秒間隔で確認します。

## ビルドの確認

```powershell
native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
native\out\CodexWeekUsageTray.exe --self-test
```

## 保存されたトレイ項目の削除

テストの結果 **他のシステム トレイ アイコン** に古い項目が残っている場合は、`CodexWeekUsageTray.exe` と同じフォルダーにある `uninstall.cmd` をダブルクリックしてください。クリーンアップはすぐに始まり、入力する語句や確認のダイアログはありません。

`uninstall.cmd` は一致する `CodexWeekUsageTray.exe` のプロセスを停止し、その現在のユーザーのトレイ設定のみを削除します。古いリリースを置き換える前や、重複したトレイ項目が残っている場合に役立ちます。EXE ファイルを削除したり、他のアプリのトレイ設定を変更したりすることはありません。

リリースフォルダーのターミナルから、次のように実行することもできます。

```powershell
.\CodexWeekUsageTray.exe --uninstall-dry-run
.\CodexWeekUsageTray.exe --uninstall
```

ドライランは完全に一致する `CodexWeekUsageTray.exe` の項目を一覧表示するだけで、何も変更しません。

## セキュリティとプライバシー

- このアプリは、パスワード、API キー、リフレッシュトークン、利用履歴を読み取り・表示・保存・送信しません。
- サインイン URL は HTTPS で、ホストが厳密に `chatgpt.com` または `auth.openai.com` の場合にのみ、既定のブラウザーで開かれます。
- ネイティブホストにはアップデーター、ダウンローダー、スタートアップ登録、アプリ独自のネットワーククライアントがありません。アカウント関連の通信は Codex CLI がローカルの stdio 接続経由で自ら処理します。
- リリースの EXE はまだ Authenticode 署名されていません。ダウンロードしたファイルを実行する前に、公開されている SHA-256 の値を確認してください。

v2.0.1 以降のリリースには、EXE、`uninstall.cmd`、および対応する `SHA256SUMS` マニフェストが含まれます。アーカイブに .NET ランタイム、DLL、PDB は含まれません。v2.0.0 には EXE のみが含まれていました。

アプリは現在の上限をメモリー上にのみ保持します。Windows が通常の通知アイコン設定を保持する場合があり、Codex CLI は独自のセッションデータを保持します。

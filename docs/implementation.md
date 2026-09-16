# OmaTextの設計・検証記録

2026-09-17、ローカル実装。公開・デプロイはしていない。

## 実装を読んで確認したこと

- [Percius04/omafiles](https://github.com/Percius04/omafiles/tree/d8bde32df743b9a9fa3e64ed728f636837d6f0f3)（調査時のHEAD）: `CMakeLists.txt`はQt Quickの独立実行ファイルとC++ QMLバックエンドを構築する。`app/qml_modules/qs`に部品を持ち、`Commons/ThemeSource.qml`はQuickshell.Ioを使わずにテーマファイルをポーリングしている。独立したQtアプリがOmarchyのモジュールをそのままインポートできることの証拠ではなく、移植・調整した実装だった。
- Omawriteの調査コミットは`8f98892b26768236b2c20f4e637cf4b102d898bf`。`omawrite.pro`、`src/main.cpp`、`src/Main.qml`でQt QuickとC++の分担を確認した。OmaTextにOmawriteの機能や独自フォントを移植してはいない。
- インストール済みOmarchyの`/usr/share/omarchy/shell/{Commons,Ui}`とプラグイン契約を読んだ。最終的なユーザーの指定は「プラグインとして作る、バーには置かない」なので、独立したQt実行ファイルではなく、既存シェル内で通常のQt Quick Windowを表示する設計にした。

## 構成

`ui/Editor.qml`は`panel`エントリポイントで、ホストの`open`/`close`を受ける。`ui/EditorView.qml`は通常ウィンドウであり、バーやlayer-shellのパネルではない。`keepLoaded`により閉じた後も文書状態を保持する。

本番UIは`qs.Commons`のColor/Style/Borderと`qs.Ui`のButton/BorderSurface/SearchableDropdown/Dropdownを直接使う。色・フォントの独自コピーはない。四角い角、細い枠、Omarchyのボタンを使用する。ファイル選択はQt Quick DialogsからネイティブGTKのピッカーを開く。

`OmaText.Backend`はC++のQMLプラグイン。DocumentはUTF-8読込、QSaveFileによる原子的保存、変更状態、未保存確認後の保留操作を扱う。エディタ本文はPlainText。外部入力をコマンドとして実行しない。通常ファイル以外と2 MiB超の読み込みを拒否し、FIFO等でシェルがブロックするのを避ける。

`tests/support/vendor`はヘッドレスUIテスト専用のOmarchy部品とテーマアダプタ。本番にはコピーしない。元ファイルのハッシュを`upstream-sha256.json`、MITライセンスを`OMARCHY-LICENSE`に保存している。

## 検証環境と結果

- Omarchy settings 4.0.3-1、Quickshell 0.3.1-1、Qt 6.11.2-3。
- Fcitx5 5.1.22-1、fcitx5-qt 5.1.15-1、Mozc。
- Hyprland、3840×2160・スケール1.5。現在の黒・緑テーマとOmarchyのフォント指定を使用。
- CMakeビルドと`ctest --test-dir build --output-on-failure`でdocument/uiの2スイートが成功。
- インストーラは一時HOMEと疑似CLIで、検出の遅延待機・ファイル配置・desktopエントリ・既存インストールの上書き拒否も検証した。
- `omarchy plugin validate .`とスキルの`validate_plugin.py`が成功。
- `qmllint`はインポート・構文を解決したが、動的なStyle.font/Qt.inputMethod等の型情報と非修飾参照について警告が残る。警告ゼロの検証とは扱わない。該当UIは実ホストで動作確認した。

実ホストで以下を確認した。UIテスト用の代替テーマだけで成功と判断していない。

1. プラグインを検出・有効化し、通常のウィンドウを表示。プロセスは既存のomarchy-shellと同じPID 1568。バーウィジェットは追加されない。
2. Mozcで`nihongo`を入力し、`日本語`へ変換。変換中はDocumentの確定文字数0、確定後に本文へ反映。
3. 日本語42文字・104 UTF-8バイトのデモ文を、日本語ファイル名で保存。名前を付けて保存と、`.txt`を明示した名前も確認。
4. 新規文書で文字数0になった後、GTKの開くダイアログで日本語ファイルを選び、42文字を再読込。
5. 未保存確認のキャンセルで44文字を維持。「保存しない」は新規に進み、ディスクは変更されない。「保存」は44文字を保存して新規に進む。開き直して保存内容を確認し、デモ本文へ戻した。
6. 元に戻す、上書き保存、Ctrl+Qで閉じる、再表示で文書を保持することを確認。
7. 未保存確認中に再度summonしても、確認画面のフォーカスを維持する。回帰テストの失敗を確認して修正し、実ホストでも`modalOpen=true`かつ本文の`activeFocus=false`を確認した。
8. スクリーンショットを目視確認。`artifacts/omatext.png`と`artifacts/unsaved-confirmation.png`。撮影時だけ当該ウィンドウの不透明度を1にし、他アプリが背景に透けないようにした。テーマやHyprland設定ファイルは変更していない。

実UI操作にはwtype、Wayland仮想ポインタ、GTKのAT-SPIを使用した。wtypeのCtrl+AがGTKで効かず、元のファイル名に文字列を追記した失敗があった。AT-SPIで入力欄を全置換して実際のSaveボタンを押す方法で切り分け、保存処理の不具合ではないことを確認した。

## 制限

- タブ、Markdownプレビュー、自動保存、外部ファイル変更の監視は範囲外。
- シェル終了・プラグイン無効化時の未保存文書の復元はない。
- UTF-8のみ。CRLF/BOMは維持するが、混在改行の完全なバイト保存は対象外。
- ファイル読込は2 MiB以下。巨大な貼付けや極端に長い行に対する性能保証はしていない。
- ネイティブQMLバックエンド更新は、必要な文書を保存して再ログインする。開発中にシェルを再起動していない。
- テーマ変更への追従はホストのColor/Styleへの直接バインドによる。ユーザーの現在のテーマを別テーマへ切り替える実験はしていない。

## 開発中の再読込

このホストでは`rescanPlugins`だけでは変更済みQMLがキャッシュから再利用された。UIを`ui/`へ整理して新しいURLで読み込み、修正後の診断値を実ホストで確認した。再読込で文書状態は失われるため、インストール・更新作業前には保存が必要。旧名Oma Editorの試作版は無効化し、ローカルの一時バックアップへ退避した。

最終ソースのSHA-256と検証範囲は`artifacts/verification.json`に保存した。

## 最小UIとフォント設定の追加検証

通常は左下の歯車のみ、ホバーすると設定・新規・開く・保存の4アイコンを表示する。上部ツールバー、ステータス行、スクロールバーは表示しない。設定はモーダルで、フォント検索と文字サイズ選択、✓（Apply）と×（Cancel）を持つ。本文に選択をプレビューし、キャンセルでは以前の設定に戻す。QtCore.Settingsで`~/.config/omatext/editor.ini`に保存する。

自動テストでホバー展開、保存アイコン、モーダルのフォーカス、プレビューと取り消し、プラグインを作り直した際の設定保持を確認した。実ホストではLiberation Mono / 20pxの適用、22pxのプレビューを×で取り消す操作、新規・開く・保存アイコン、日本語ファイル`artifacts/設定確認.txt`の保存再読込を確認した。検証後はOmarchyの既定フォント・サイズに戻した。

最終画面は`artifacts/omatext.png`、`hover-actions.png`、`font-settings.png`。`unsaved-confirmation.png`と`ime-preedit.png`は初期UI時点の検証画像である。日本語IMEと未保存確認の初期検証は前節に記載し、この変更では設定とアイコン操作を重点的に再確認した。

## 英語ラベル

操作ツールチップ、アクセシビリティ名、設定ラベル、未保存確認、ファイルダイアログのタイトル・フィルタを英語へ統一。document/uiテスト成功。実行中のエディタに未保存の文章があるため、この変更のインストール・再読込は保留した。既存スクリーンショットは英語化前のもの。

## ショートカット

New: Ctrl+N、Open: Ctrl+O、Save: Ctrl+S、Settings: Ctrl+,。ツールチップに表示。設定モーダル中は文書操作のショートカットを無効にする。自動テストと実ホストで確認済み。ユーザー文書が保存済みであることを確認後、英語ラベルも反映した。実ホストのQMLキャッシュ回避のため、今回のローカル配置のみ`ui-shortcuts/Editor.qml`をエントリに指定。ソースと新規インストールのエントリは`ui/Editor.qml`。

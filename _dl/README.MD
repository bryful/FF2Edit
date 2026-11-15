# FF2 セーブエディタ 配布ファイル

ファミコン日本版『ファイナルファンタジーII』の非圧縮 SRAM セーブ（8,192 / 8,193 バイト）に対応します。所持金、名前、HP・MP、基礎能力値、武器・魔法の熟練度を編集できます。

## Windows x64

[FF2Edit-windows-x64.zip](windows/FF2Edit-windows-x64.zip) を展開し、`ff2edit_gui.exe` を起動してください。Qt の DLL、プラグイン、Visual C++ ランタイムを含みます。展開後はフォルダーごと移動してください。コマンドライン版の `ff2edit.exe` も同梱しています。

## Android arm64

Android 9 以降の arm64-v8a 端末向けです。[FF2Edit-android-arm64-release.apk](android/FF2Edit-android-arm64-release.apk) を端末にダウンロードしてインストールしてください。

## 使い方

1. 「開く…」からセーブファイルを選びます。
2. セーブ枠とキャラクターを選び、数値や名前を編集します。
3. 「上書き保存」または「別名で保存…」を選びます。

上書き保存時のバックアップは、Windows では元ファイルと同じフォルダーに `.bak` として作成されます。Android ではアプリ内に作成され、「バックアップを書き出す…」からコピーできます。

## ライセンス

FF2Edit は [MIT ライセンス](../LICENSE) です。Windows 版は Qt Base 6.8.3、Android 版は Qt Base 6.10.3 を使用しています。Qt のライセンスと対応するソースは以下を参照してください。

- Windows [ライセンス](https://doc.qt.io/qt-6.8/licensing.html) · [Qt Base 6.8.3 ソース](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/)
- Android [ライセンス](https://doc.qt.io/qt-6.10/licensing.html) · [Qt Base 6.10.3 ソース](https://download.qt.io/archive/qt/6.10/6.10.3/submodules/)

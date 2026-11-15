# FF2 セーブエディタ

ファミコン日本版『ファイナルファンタジーII』の非圧縮 SRAM セーブ（8,192 / 8,193 バイト）用エディタです。所持金、名前、HP/MP、基礎能力値、武器・魔法熟練度を編集できます。

## Windows

MSVC と Qt 6.5 以上の Windows 用 MSVC キットを使用します。コンパイラと Qt のアーキテクチャは揃えてください。

```powershell
$qt = 'C:/Qt/6.8.3/msvc2022_64'
cmake -S . -B build/windows -DFF2EDIT_BUILD_GUI=ON "-DCMAKE_PREFIX_PATH=$qt"
cmake --build build/windows --config Release
$env:Path = "$qt/bin;$env:Path"
.\build\windows\Release\ff2edit_gui.exe
```

`FF2EDIT_BUILD_GUI` を指定せずに構成すると、Qt なしで CLI をビルドできます。

## Android

Qt Creator で `CMakeLists.txt` を開き、16 KBページサイズに対応する Qt 6.10 以上の Android キットを選択してビルド・実行します。選択した Qt に対応する Android SDK、NDK、JDK を設定してください。Android キットでは GUI が既定で有効です。

## 操作

「開く…」でセーブを選択し、セーブ枠とキャラクターを選んで編集します。「上書き保存」で保存直前の内容をバックアップし、元の名前に保存します。「別名で保存…」では新しいファイルに保存します。

Windows のバックアップは元ファイルと同じフォルダーに `.bak` として残ります。Android のバックアップはアプリ内に残り、「バックアップを書き出す…」から任意の場所にコピーできます。

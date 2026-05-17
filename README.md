# nitic_mobile_device_application_experiment / icon
## 方針
アイコンを作って設定した！
<br>

## 変更概要
1. 2048_game/app/src/main/res内フォルダの【mipmap-hdpi】【mipmap-mdpi】【mipmap-xhdpi】【mipmap-xxhdpi】【mipmap-xxxhdpi】のフォルダに、画像a.pngをコピペ

2. 2048_game/app/src/main/AndroidManifest.xmlの一部を下記の通りに書き換える
```
<application
    android:icon="@mipmap/ic_launcher"
    android:roundIcon="@mipmap/ic_launcher_round"
```
↓
```
<application
    android:icon="@mipmap/a"
    android:roundIcon="@mipmap/a"
```
<br>

## レポート
1. 目的<br>
スマートフォン用アプリケーションソフトウェアを開発するために必要な基礎知識・技術を学ぶ。
<br>

2. 実験<br>
開発環境構築・サンプル実行・開発ソフトの提案と基本設計を行い、指導教員の評価を受ける。評価で合格した後に実装を行い、基本的な事項をまとめる。
<br>

3. 実験結果の報告<br>
　3-1 開発ソフトの提案<br>
　　2048ゲーム<br>
   - 2のべき乗の数字が4×4のマス上に表示される。画面は四方にスライドすることで
数字を動かせる。スライドする度に2か4がランダムに空きマスに新規生成される。
   - 同じ数字を2つ合わせると、1つになって数字が変化する。2と2で4、4と4で8、
8と8で16…のように大きくなる。
   - 数字でマス全体が埋まり、かつどの方向にスライドしても数字が合わない場合ゲーム
オーバーとなる。
<br>
　　3-2 班内分担<br>
　　3-3 個人成果報告<br>
  　　　略<br>
　　3-4 共通成果報告<br>
  　　　略<br>
<br>
4. まとめ<br>
今回の実験では、スマートフォン用アプリケーションを開発するために必要な知識・技術を、Android Studioを使用した共同開発を通して学ぶ事ができた。

# gakumas-ai

学園アイドルマスターの試験を再現するシミュレーターと、スコアを最大化する AI を作るプロジェクト。

## 今できること
- センスの15枚のデッキ(`data/deck_sense_first.csv`)で、試験をブラウザまたはターミナルで遊べる
- 計算式の確認用の例を、自動テストで確かめられる

## フォルダ構成
```
data/    カード・効果・デッキ・試験の設定(CSV)
docs/    ルール(rules.md)、設計(design.md)、方針(decisions.md)、TODO
src/     シミュレーター本体
app/     遊ぶためのプログラム(server.cpp はブラウザ用、main.cpp はターミナル用)
web/     ブラウザの画面
tests/   自動テスト
```

## ビルドと実行
C++17 に対応した g++ が必要。`gakumas_ai` フォルダで実行する。

```bash
# テスト
g++ -std=c++17 -Wall -Wextra -O2 -Isrc src/*.cpp tests/test_examples.cpp -o test_examples
./test_examples

# ブラウザで遊ぶ(Windows は -lws2_32 が必要)。起動したら http://localhost:8080 を開く
g++ -std=c++17 -Wall -Wextra -O2 -Isrc src/*.cpp app/server.cpp -o server -lws2_32
./server

# ターミナルで遊ぶ(シード値は省略可。同じ値なら同じ展開を再現できる)
g++ -std=c++17 -Wall -Wextra -O2 -Isrc src/*.cpp app/main.cpp -o gakumas
./gakumas 7
```

// CSV からカード・デッキ・試験の設定を読み込む
#pragma once

#include <map>
#include <string>
#include <vector>

#include "card.hpp"
#include "game.hpp"

// id → カードの定義。std::map なので、あとから要素を足しても CardDef のアドレスは変わらない
using CardDb = std::map<std::string, CardDef>;

// 1. cards.csv を読む
CardDb load_cards(const std::string& path);
// 2. effects.csv を読み、効果と使用条件を CardDb に入れる
void load_effects(const std::string& path, CardDb& db);
// 3. デッキの csv を読み、枚数分の DeckCard を作る
std::vector<DeckCard> load_deck(const std::string& path, const CardDb& db);
// 試験の設定(key,value 形式)を読む
Exam load_exam(const std::string& path);

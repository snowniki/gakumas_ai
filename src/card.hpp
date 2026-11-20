// カードの定義(試験中に変わらないデータ)
#pragma once

#include <string>
#include <vector>

enum class CardType { Active, Mental, Trouble };

// コストの払い方
enum class CostType {
    Normal,            // 元気から払い、足りない分を体力から払う
    Hp,                // 元気を無視して体力から払う
    GoodCondition,     // 好調のターン数を消費する
    Concentration,     // 集中を消費する
    PerfectCondition,  // 絶好調のターン数を消費する(未対応)
    Motivation,        // やる気を消費する(未対応)
    GoodImpression,    // 好印象を消費する(未対応)
    FullPower,         // 全力値を消費する(未対応)
};

// 効果の部品の種類(effects.csv の kind)
enum class EffectKind { Param, Genki, GoodCondition, Concentration };

// 効果の部品が発動する条件(effects.csv の condition)
enum class Condition { None, GoodCondition };

// カードを使うための条件(effects.csv の kind=use_condition)
enum class UseConditionKind { GoodCondition };

// 効果の部品(effects.csv の1行)
struct Effect {
    EffectKind kind = EffectKind::Param;
    int value = 0;
    int times = 1;
    int concentration_rate10 = 10;  // 集中を何倍で足すか × 10(1.5倍なら15)。小数の誤差を避けるため整数で持つ
    Condition condition = Condition::None;
};

struct UseCondition {
    UseConditionKind kind = UseConditionKind::GoodCondition;
};

// 無印版・+版それぞれの中身
struct CardVersion {
    bool exists = false;       // cards.csv にコストが書かれているか(LR の+版などは存在しない)
    CostType cost_type = CostType::Normal;
    int cost = 0;
    std::string text;          // 表示用の効果の文章(cards.csv の effect / effect_plus)
    bool has_effects = false;  // effects.csv に効果が書かれているか
    std::vector<UseCondition> use_conditions;
    std::vector<Effect> effects;
};

// カード1種類の定義
struct CardDef {
    std::string id;
    std::string name;
    std::string plan;
    CardType type = CardType::Active;
    bool once_per_lesson = false;
    bool no_duplicate = false;
    CardVersion base;
    CardVersion plus;

    const CardVersion& version(bool is_plus) const { return is_plus ? plus : base; }
};

// デッキの中のカード1枚
struct DeckCard {
    int uid = 0;                    // 試験の中で1枚ごとに違う番号
    std::string card_id;            // どの CardDef か
    const CardDef* def = nullptr;   // card_id で探した CardDef(毎回探さなくて済むように持っておく)
    bool plus = false;

    const CardVersion& version() const { return def->version(plus); }
    std::string display_name() const { return def->name + (plus ? "+" : ""); }
};

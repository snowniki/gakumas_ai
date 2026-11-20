// ゲームの状態と、状態を変える関数
#pragma once

#include <random>
#include <vector>

#include "card.hpp"

// 試験の設定
struct Exam {
    int max_turn = 0;
    int max_hp = 0;
    int start_hp = 0;
    int score_bonus_percent = 100;  // スコアボーナス × 100(1.0倍なら100)
};

// ターン数つきのバフ・デバフ(今の15枚では使わない。後から種類を足す)
enum class BuffKind { ParamUp };

struct Buff {
    BuffKind kind = BuffKind::ParamUp;
    int value = 0;
    int turns = 0;       // 残りターン数。-1 は試験の最後まで続く
    bool fresh = false;  // プレイヤーの操作中に付いたか(付いたターンの終了時には減らさない)
};

// ゲームの状態(変わるもの)
struct State {
    int turn = 1;
    int hp = 0;
    int genki = 0;
    int good_condition = 0;          // 好調の残りターン数
    bool good_condition_fresh = false;
    int perfect_condition = 0;       // 絶好調の残りターン数
    bool perfect_condition_fresh = false;
    int concentration = 0;
    int score = 0;
    int uses_left = 0;               // このターンにあと何枚使えるか
    std::vector<Buff> buffs;
    std::vector<DeckCard> cards;     // デッキの全カード。cards[uid] がその uid のカード
    std::vector<int> deck;           // 山札(先頭が次に引くカード)
    std::vector<int> hand;           // 手札
    std::vector<int> discard;        // 捨て札
    std::vector<int> exile;          // 除外
};

struct Action {
    enum class Type { UseCard, Rest };
    Type type = Type::Rest;
    int uid = -1;  // UseCard のとき、使うカードの uid

    static Action use(int uid) { return {Type::UseCard, uid}; }
    static Action rest() { return {Type::Rest, -1}; }
};

constexpr int kDrawPerTurn = 3;
constexpr int kHandLimit = 5;   // 手札の上限(要確認)
constexpr int kRestHeal = 2;

using Rng = std::mt19937;

State init_state(const std::vector<DeckCard>& cards, const Exam& exam, Rng& rng);
void start_turn(State& s, Rng& rng);
void draw(State& s, int n, Rng& rng);
bool can_use(const State& s, int uid);
std::vector<Action> legal_actions(const State& s);
void pay_cost(State& s, const CardVersion& v);
int calc_param(const State& s, const Effect& e);
void apply_effect(State& s, const Effect& e, const Exam& exam);
void use_card(State& s, int uid, const Exam& exam);
void rest(State& s, const Exam& exam);
void end_turn(State& s);
bool is_end(const State& s, const Exam& exam);

// 行動を1つ実行する。ターンが終わったら次のターンを始める
void step(State& s, const Action& a, const Exam& exam, Rng& rng);

#include "game.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

// 正の整数どうしの割り算を切り上げる
int ceil_div(int a, int b) { return (a + b - 1) / b; }

bool satisfies(const State& s, Condition c) {
    switch (c) {
        case Condition::None: return true;
        case Condition::GoodCondition: return s.good_condition > 0;
    }
    return false;
}

bool can_pay(const State& s, const CardVersion& v) {
    switch (v.cost_type) {
        case CostType::Normal: return s.genki + s.hp >= v.cost;  // 体力がマイナスになるなら使えない
        case CostType::Hp: return s.hp >= v.cost;
        case CostType::GoodCondition: return s.good_condition >= v.cost;
        case CostType::Concentration: return s.concentration >= v.cost;
        default: return false;  // まだ対応していないコスト
    }
}

void remove_from(std::vector<int>& zone, int uid) {
    auto it = std::find(zone.begin(), zone.end(), uid);
    if (it == zone.end()) throw std::logic_error("カードが見つかりません: uid " + std::to_string(uid));
    zone.erase(it);
}

// 好調・絶好調のように「ターン数が値」の効果を1ターン分減らす
void tick(int& turns, bool& fresh) {
    if (turns <= 0) return;
    if (fresh) {
        fresh = false;  // 付いたターンは減らさない(要確認)
    } else {
        --turns;
    }
}

}  // namespace

State init_state(const std::vector<DeckCard>& cards, const Exam& exam, Rng& rng) {
    State s;
    s.turn = 1;
    s.hp = exam.start_hp;
    s.cards = cards;
    for (const DeckCard& c : cards) s.deck.push_back(c.uid);
    std::shuffle(s.deck.begin(), s.deck.end(), rng);
    return s;
}

void start_turn(State& s, Rng& rng) {
    s.uses_left = 1;
    int n = kDrawPerTurn;  // ドロー枚数を増減する効果は、ここで足し引きする
    if (n < 0) n = 0;
    draw(s, n, rng);
}

void draw(State& s, int n, Rng& rng) {
    for (int i = 0; i < n; ++i) {
        if (s.deck.empty()) {
            if (s.discard.empty()) return;  // 山札も捨て札も空なら引けない
            s.deck = s.discard;
            s.discard.clear();
            std::shuffle(s.deck.begin(), s.deck.end(), rng);
        }
        if (static_cast<int>(s.hand.size()) >= kHandLimit) return;
        s.hand.push_back(s.deck.front());
        s.deck.erase(s.deck.begin());
    }
}

bool can_use(const State& s, int uid) {
    if (s.uses_left <= 0) return false;
    if (std::find(s.hand.begin(), s.hand.end(), uid) == s.hand.end()) return false;
    const CardVersion& v = s.cards[uid].version();
    if (!v.exists || !v.has_effects) return false;
    for (const UseCondition& u : v.use_conditions) {
        switch (u.kind) {
            case UseConditionKind::GoodCondition:
                if (s.good_condition <= 0) return false;
                break;
        }
    }
    return can_pay(s, v);
}

std::vector<Action> legal_actions(const State& s) {
    std::vector<Action> actions;
    for (int uid : s.hand) {
        if (can_use(s, uid)) actions.push_back(Action::use(uid));
    }
    actions.push_back(Action::rest());
    return actions;
}

void pay_cost(State& s, const CardVersion& v) {
    switch (v.cost_type) {
        case CostType::Normal:
            if (s.genki >= v.cost) {
                s.genki -= v.cost;
            } else {
                s.hp -= v.cost - s.genki;
                s.genki = 0;
            }
            break;
        case CostType::Hp: s.hp -= v.cost; break;
        case CostType::GoodCondition: s.good_condition -= v.cost; break;
        case CostType::Concentration: s.concentration -= v.cost; break;
        default: throw std::logic_error("まだ対応していないコストです");
    }
}

int calc_param(const State& s, const Effect& e) {
    // 1. 集中の部分は先に切り上げる(レッスンで確認済み)
    int concentration_part = ceil_div(s.concentration * e.concentration_rate10, 10);
    // 2. カードのパラメータ値(最終)
    int base = e.value + concentration_part;
    // 3. 好調・絶好調の倍率 × 10
    int rate10 = 10;
    if (s.good_condition > 0) {
        rate10 = 15;
        if (s.perfect_condition > 0) rate10 += s.good_condition;  // 1.5 + 0.1 × 好調のターン数
    }
    // 4. 倍率をかけて切り上げる
    return ceil_div(base * rate10, 10);
}

void apply_effect(State& s, const Effect& e, const Exam& exam) {
    for (int t = 0; t < e.times; ++t) {
        if (!satisfies(s, e.condition)) return;
        switch (e.kind) {
            case EffectKind::Param: {
                int param = calc_param(s, e);
                s.score += ceil_div(param * exam.score_bonus_percent, 100);
                break;
            }
            case EffectKind::Genki:
                s.genki += e.value;
                break;
            case EffectKind::GoodCondition:
                if (s.good_condition == 0) s.good_condition_fresh = true;
                s.good_condition += e.value;
                break;
            case EffectKind::Concentration:
                s.concentration += e.value;
                break;
        }
    }
}

void use_card(State& s, int uid, const Exam& exam) {
    if (!can_use(s, uid)) throw std::logic_error("使えないカードです: uid " + std::to_string(uid));
    const DeckCard& card = s.cards[uid];
    const CardVersion& v = card.version();

    pay_cost(s, v);
    for (const Effect& e : v.effects) apply_effect(s, e, exam);

    remove_from(s.hand, uid);
    if (card.def->once_per_lesson) {
        s.exile.push_back(uid);
    } else {
        s.discard.push_back(uid);
    }
    --s.uses_left;
}

void rest(State& s, const Exam& exam) {
    s.hp = std::min(s.hp + kRestHeal, exam.max_hp);
}

void end_turn(State& s) {
    // 使わなかった手札は捨て札へ(要確認)
    for (int uid : s.hand) s.discard.push_back(uid);
    s.hand.clear();

    tick(s.good_condition, s.good_condition_fresh);
    tick(s.perfect_condition, s.perfect_condition_fresh);

    for (Buff& b : s.buffs) {
        if (b.turns < 0) continue;  // 試験の最後まで続く
        if (b.fresh) {
            b.fresh = false;
        } else {
            --b.turns;
        }
    }
    s.buffs.erase(std::remove_if(s.buffs.begin(), s.buffs.end(),
                                 [](const Buff& b) { return b.turns == 0; }),
                  s.buffs.end());

    s.uses_left = 0;
    ++s.turn;
}

bool is_end(const State& s, const Exam& exam) { return s.turn > exam.max_turn; }

void step(State& s, const Action& a, const Exam& exam, Rng& rng) {
    if (a.type == Action::Type::Rest) {
        rest(s, exam);
        s.uses_left = 0;
    } else {
        use_card(s, a.uid, exam);
    }
    if (s.uses_left > 0) return;  // まだ使える(スキルカード使用数追加があったとき)

    end_turn(s);
    if (!is_end(s, exam)) start_turn(s, rng);
}

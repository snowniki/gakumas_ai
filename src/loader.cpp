#include "loader.hpp"

#include <algorithm>
#include <stdexcept>

#include "csv.hpp"

namespace {

std::runtime_error error(const CsvTable& t, const CsvRow& r, const std::string& msg) {
    return std::runtime_error(t.where(r) + msg);
}

int parse_int(const CsvTable& t, const CsvRow& r, const std::string& col) {
    const std::string& s = t.get(r, col);
    if (s.empty()) throw error(t, r, col + " が空です");
    size_t pos = 0;
    int v = 0;
    try {
        v = std::stoi(s, &pos);
    } catch (...) {
        pos = 0;
    }
    if (pos != s.size()) throw error(t, r, col + " が整数として読めません: " + s);
    return v;
}

// "1.5" → 15、"2" → 20 のように、10倍した整数にする(小数第1位まで)
int parse_tenths(const CsvTable& t, const CsvRow& r, const std::string& col) {
    const std::string& s = t.get(r, col);
    size_t dot = s.find('.');
    std::string whole = s.substr(0, dot);
    std::string frac = dot == std::string::npos ? "0" : s.substr(dot + 1);
    bool ok = !whole.empty() && frac.size() == 1 &&
              std::all_of(whole.begin(), whole.end(), ::isdigit) && ::isdigit(frac[0]);
    if (!ok) throw error(t, r, col + " が小数第1位までの数として読めません: " + s);
    return std::stoi(whole) * 10 + (frac[0] - '0');
}

bool parse_yes_no(const CsvTable& t, const CsvRow& r, const std::string& col) {
    const std::string& s = t.get(r, col);
    if (s == "yes") return true;
    if (s == "no") return false;
    throw error(t, r, col + " は yes か no にしてください: " + s);
}

CardType parse_card_type(const CsvTable& t, const CsvRow& r) {
    const std::string& s = t.get(r, "type");
    if (s == "active") return CardType::Active;
    if (s == "mental") return CardType::Mental;
    if (s == "trouble") return CardType::Trouble;
    throw error(t, r, "type が知らない値です: " + s);
}

CostType parse_cost_type(const CsvTable& t, const CsvRow& r, const std::string& col) {
    const std::string& s = t.get(r, col);
    if (s == "normal") return CostType::Normal;
    if (s == "hp") return CostType::Hp;
    if (s == "good_condition") return CostType::GoodCondition;
    if (s == "concentration") return CostType::Concentration;
    if (s == "perfect_condition") return CostType::PerfectCondition;
    if (s == "motivation") return CostType::Motivation;
    if (s == "good_impression") return CostType::GoodImpression;
    if (s == "full_power") return CostType::FullPower;
    throw error(t, r, col + " が知らない値です: " + s);
}

CardVersion read_version(const CsvTable& t, const CsvRow& r, const std::string& suffix) {
    CardVersion v;
    v.text = t.get(r, suffix.empty() ? "effect" : "effect" + suffix);
    if (t.get(r, "cost" + suffix).empty()) return v;  // この版は存在しない(LR の+版など)
    v.exists = true;
    v.cost_type = parse_cost_type(t, r, "cost_type" + suffix);
    v.cost = parse_int(t, r, "cost" + suffix);
    return v;
}

}  // namespace

CardDb load_cards(const std::string& path) {
    CsvTable t = read_csv(path);
    CardDb db;
    for (const CsvRow& r : t.rows) {
        CardDef c;
        c.id = t.get(r, "id");
        c.name = t.get(r, "name");
        c.plan = t.get(r, "plan");
        c.type = parse_card_type(t, r);
        c.once_per_lesson = parse_yes_no(t, r, "once_per_lesson");
        c.no_duplicate = parse_yes_no(t, r, "no_duplicate");
        c.base = read_version(t, r, "");
        c.plus = read_version(t, r, "_plus");
        if (db.count(c.id)) throw error(t, r, "id が重複しています: " + c.id);
        db[c.id] = c;
    }
    return db;
}

void load_effects(const std::string& path, CardDb& db) {
    CsvTable t = read_csv(path);

    struct Item {
        int order;
        const CsvRow* row;
    };
    // (id, plus) ごとに行を集めて、order 順に並べてから入れる
    std::map<std::pair<std::string, bool>, std::vector<Item>> groups;
    for (const CsvRow& r : t.rows) {
        const std::string& id = t.get(r, "id");
        if (!db.count(id)) throw error(t, r, "id が cards.csv にありません: " + id);
        bool plus = parse_yes_no(t, r, "plus");
        groups[{id, plus}].push_back({parse_int(t, r, "order"), &r});
    }

    for (auto& [key, items] : groups) {
        std::stable_sort(items.begin(), items.end(),
                         [](const Item& a, const Item& b) { return a.order < b.order; });
        CardDef& card = db.at(key.first);
        CardVersion& v = key.second ? card.plus : card.base;
        if (!v.exists) {
            throw error(t, *items.front().row, card.name + (key.second ? "+" : "") +
                                                   " は cards.csv にコストがありません");
        }
        v.has_effects = true;

        for (const Item& item : items) {
            const CsvRow& r = *item.row;
            const std::string& kind = t.get(r, "kind");
            const std::string& cond = t.get(r, "condition");

            if (kind == "use_condition") {
                if (cond != "good_condition") throw error(t, r, "知らない使用条件です: " + cond);
                v.use_conditions.push_back({UseConditionKind::GoodCondition});
                continue;
            }

            Effect e;
            if (kind == "param") e.kind = EffectKind::Param;
            else if (kind == "genki") e.kind = EffectKind::Genki;
            else if (kind == "good_condition") e.kind = EffectKind::GoodCondition;
            else if (kind == "concentration") e.kind = EffectKind::Concentration;
            else throw error(t, r, "kind が知らない値です: " + kind);

            if (cond == "none") e.condition = Condition::None;
            else if (cond == "good_condition") e.condition = Condition::GoodCondition;
            else throw error(t, r, "condition が知らない値です: " + cond);

            e.value = parse_int(t, r, "value");
            e.times = parse_int(t, r, "times");
            e.concentration_rate10 = parse_tenths(t, r, "concentration_rate");
            if (e.times < 1) throw error(t, r, "times は1以上にしてください");
            v.effects.push_back(e);
        }
    }
}

std::vector<DeckCard> load_deck(const std::string& path, const CardDb& db) {
    CsvTable t = read_csv(path);
    std::vector<DeckCard> deck;
    std::map<std::string, int> counts;
    for (const CsvRow& r : t.rows) {
        const std::string& id = t.get(r, "id");
        auto it = db.find(id);
        if (it == db.end()) throw error(t, r, "id が cards.csv にありません: " + id);
        const CardDef& def = it->second;
        bool plus = parse_yes_no(t, r, "plus");
        const CardVersion& v = def.version(plus);
        std::string name = def.name + (plus ? "+" : "");
        if (!v.exists) throw error(t, r, name + " は存在しない版です");
        if (!v.has_effects) throw error(t, r, name + " の効果が effects.csv にありません");

        int count = parse_int(t, r, "count");
        if (count < 1) throw error(t, r, "count は1以上にしてください");
        counts[id] += count;
        if (def.no_duplicate && counts[id] > 1) throw error(t, r, def.name + " は重複不可です");

        for (int i = 0; i < count; ++i) {
            DeckCard c;
            c.uid = static_cast<int>(deck.size());
            c.card_id = id;
            c.def = &def;
            c.plus = plus;
            deck.push_back(c);
        }
    }
    if (deck.empty()) throw std::runtime_error(path + ": デッキが空です");
    return deck;
}

Exam load_exam(const std::string& path) {
    CsvTable t = read_csv(path);
    std::map<std::string, int> kv;
    for (const CsvRow& r : t.rows) kv[t.get(r, "key")] = parse_int(t, r, "value");
    auto need = [&](const std::string& key) {
        if (!kv.count(key)) throw std::runtime_error(path + ": " + key + " がありません");
        return kv[key];
    };
    Exam e;
    e.max_turn = need("max_turn");
    e.max_hp = need("max_hp");
    e.start_hp = need("start_hp");
    e.score_bonus_percent = need("score_bonus_percent");
    if (e.start_hp > e.max_hp) throw std::runtime_error(path + ": start_hp が max_hp より大きいです");
    return e;
}

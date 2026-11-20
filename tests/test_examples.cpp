// design.md の「確認用の例」を自動で確かめる
//   使い方: ./test_examples(gakumas_ai フォルダで実行する)
#include <iostream>
#include <string>

#include "game.hpp"
#include "loader.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

int g_failed = 0;
int g_passed = 0;

void check(bool ok, const std::string& name, const std::string& detail = "") {
    if (ok) {
        ++g_passed;
        std::cout << "  OK   " << name << "\n";
    } else {
        ++g_failed;
        std::cout << "  NG   " << name << "  " << detail << "\n";
    }
}

void check_eq(int actual, int expected, const std::string& name) {
    check(actual == expected, name,
          "(期待 " + std::to_string(expected) + "、実際 " + std::to_string(actual) + ")");
}

struct Fixture {
    CardDb db;
    std::vector<DeckCard> deck;
    Exam exam;

    Fixture() {
        db = load_cards("data/cards.csv");
        load_effects("data/effects.csv", db);
        deck = load_deck("data/deck_sense_first.csv", db);
        exam.max_turn = 8;
        exam.max_hp = 30;
        exam.start_hp = 30;
        exam.score_bonus_percent = 100;
    }

    int uid_of(const std::string& name) const {
        for (const DeckCard& c : deck) {
            if (c.def->name == name) return c.uid;
        }
        throw std::runtime_error("デッキにないカード: " + name);
    }

    // 指定したカードだけを手札に持った状態を作る
    State state_with(const std::string& card_name) const {
        State s;
        s.cards = deck;
        s.hp = 30;
        s.uses_left = 1;
        s.hand.push_back(uid_of(card_name));
        return s;
    }
};

}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        Fixture f;
        std::cout << "デッキ読み込み\n";
        check_eq(static_cast<int>(f.deck.size()), 15, "デッキは15枚");

        std::cout << "design.md の確認用の例\n";
        {  // 例1
            State s = f.state_with("アピールの基本");
            use_card(s, s.hand[0], f.exam);
            check_eq(s.hp, 26, "例1 体力30・元気0でアピールの基本 → 体力26");
            check_eq(s.score, 9, "例1 スコア+9");
        }
        {  // 例2
            State s = f.state_with("アピールの基本");
            s.genki = 2;
            use_card(s, s.hand[0], f.exam);
            check_eq(s.genki, 0, "例2 元気2でアピールの基本 → 元気0");
            check_eq(s.hp, 28, "例2 体力28");
            check_eq(s.score, 9, "例2 スコア+9");
        }
        {  // 例3
            State s = f.state_with("ハイタッチ");
            s.good_condition = 2;
            s.concentration = 3;
            use_card(s, s.hand[0], f.exam);
            check_eq(s.score, 33, "例3 好調2・集中3でハイタッチ → スコア+33");
        }
        {  // 例3b(レッスンで確認済み)
            State s = f.state_with("ハイタッチ");
            s.good_condition = 10;
            s.perfect_condition = 4;
            s.concentration = 1;
            use_card(s, s.hand[0], f.exam);
            check_eq(s.score, 48, "例3b 好調10・絶好調4・集中1でハイタッチ → スコア+48");
        }
        {  // 例4
            State s = f.state_with("挑戦");
            check(!can_use(s, s.hand[0]), "例4 好調0では挑戦を使えない");
            check(legal_actions(s).size() == 1, "例4 選べる行動は休むだけ");
        }
        {  // 例5
            State s = f.state_with("勢い任せ");
            use_card(s, s.hand[0], f.exam);
            check_eq(s.score, 6, "例5 好調0で勢い任せ → スコア+6");
            check_eq(s.concentration, 0, "例5 集中は増えない");
        }
        {  // 例6
            State s = f.state_with("勢い任せ");
            s.good_condition = 1;
            use_card(s, s.hand[0], f.exam);
            check_eq(s.score, 9, "例6 好調1で勢い任せ → スコア+9");
            check_eq(s.concentration, 3, "例6 集中+3");
        }
        {  // 例7
            State s = f.state_with("試行錯誤");
            s.good_condition = 2;
            s.concentration = 1;
            use_card(s, s.hand[0], f.exam);
            check_eq(s.score, 28, "例7 好調2・集中1で試行錯誤 → スコア+28");
        }
        {  // 例8
            State s = f.state_with("アピールの基本");
            s.hp = 29;
            rest(s, f.exam);
            check_eq(s.hp, 30, "例8 体力29で休む → 体力30(最大体力を超えない)");
        }

        std::cout << "カードの移動とターンの処理\n";
        {
            State s = f.state_with("表現の基本");
            int uid = s.hand[0];
            use_card(s, uid, f.exam);
            check(s.exile.size() == 1 && s.exile[0] == uid, "レッスン中1回のカードは除外へ");
        }
        {
            State s = f.state_with("アピールの基本");
            int uid = s.hand[0];
            use_card(s, uid, f.exam);
            check(s.discard.size() == 1 && s.discard[0] == uid, "それ以外のカードは捨て札へ");
        }
        {
            State s = f.state_with("振る舞いの基本");
            use_card(s, s.hand[0], f.exam);
            check_eq(s.good_condition, 2, "振る舞いの基本で好調2");
            end_turn(s);
            check_eq(s.good_condition, 2, "付いたターンの終了時には減らない(要確認)");
            end_turn(s);
            check_eq(s.good_condition, 1, "次のターンの終了時に1減る");
        }
        {
            State s = f.state_with("アピールの基本");
            s.genki = 3;
            s.hp = 0;
            check(!can_use(s, s.hand[0]), "元気3・体力0ではコスト4のカードを使えない");
        }
        {
            Rng rng(1);
            State s = init_state(f.deck, f.exam, rng);
            start_turn(s, rng);
            check_eq(static_cast<int>(s.hand.size()), 3, "1ターン目は3枚引く");
            check_eq(static_cast<int>(s.deck.size()), 12, "山札は残り12枚");
            s.discard = s.deck;  // 山札をすべて捨て札に移す
            s.deck.clear();
            s.hand.clear();
            draw(s, 3, rng);
            check(s.hand.size() == 3 && s.deck.size() == 9 && s.discard.empty(),
                  "山札が空なら捨て札をシャッフルして山札にする");
        }
        {  // 最後まで休み続けても止まらないこと
            Rng rng(42);
            State s = init_state(f.deck, f.exam, rng);
            start_turn(s, rng);
            int guard = 0;
            while (!is_end(s, f.exam) && guard++ < 100) step(s, Action::rest(), f.exam, rng);
            check(is_end(s, f.exam) && s.turn == f.exam.max_turn + 1, "8ターンで試験が終わる");
        }
    } catch (const std::exception& e) {
        std::cout << "エラー: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n" << g_passed << " 件成功、" << g_failed << " 件失敗\n";
    return g_failed == 0 ? 0 : 1;
}

// ターミナルで自分が操作して試験を遊ぶ
//   使い方: ./gakumas [シード値]
//   gakumas_ai フォルダ(data/ がある場所)で実行する
#include <iostream>
#include <stdexcept>
#include <string>

#include "game.hpp"
#include "loader.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void print_state(const State& s, const Exam& exam) {
    std::cout << "\n========== ターン " << s.turn << " / " << exam.max_turn << " ==========\n";
    std::cout << "スコア " << s.score << "  体力 " << s.hp << "/" << exam.max_hp << "  元気 " << s.genki
              << "  好調 " << s.good_condition << "  絶好調 " << s.perfect_condition << "  集中 "
              << s.concentration << "\n";
    std::cout << "山札 " << s.deck.size() << "枚  捨て札 " << s.discard.size() << "枚  除外 "
              << s.exile.size() << "枚\n";
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // 日本語の文字化け対策
    SetConsoleCP(CP_UTF8);
#endif
    try {
        unsigned seed = argc > 1 ? static_cast<unsigned>(std::stoul(argv[1])) : std::random_device{}();

        CardDb db = load_cards("data/cards.csv");
        load_effects("data/effects.csv", db);
        std::vector<DeckCard> deck = load_deck("data/deck_sense_first.csv", db);
        Exam exam = load_exam("data/exam_sample.csv");

        Rng rng(seed);
        State s = init_state(deck, exam, rng);
        start_turn(s, rng);
        std::cout << "シード値: " << seed << "(同じ値を指定すると同じ展開を再現できます)\n";

        while (!is_end(s, exam)) {
            print_state(s, exam);
            std::cout << "手札:\n";
            for (size_t i = 0; i < s.hand.size(); ++i) {
                const DeckCard& c = s.cards[s.hand[i]];
                std::cout << "  " << i + 1 << ". " << c.display_name() << "(コスト "
                          << c.version().cost << ")" << (can_use(s, c.uid) ? "" : " [使えない]")
                          << "\n      " << c.version().text << "\n";
            }
            std::cout << "  0. 休む(体力+" << kRestHeal << ")\n";
            std::cout << "番号を入力 > " << std::flush;

            std::string line;
            if (!std::getline(std::cin, line)) return 0;
            int choice = -1;
            try {
                choice = std::stoi(line);
            } catch (...) {
            }

            if (choice == 0) {
                step(s, Action::rest(), exam, rng);
            } else if (choice >= 1 && choice <= static_cast<int>(s.hand.size()) &&
                       can_use(s, s.hand[choice - 1])) {
                step(s, Action::use(s.hand[choice - 1]), exam, rng);
            } else {
                std::cout << "その番号は選べません\n";
            }
        }

        std::cout << "\n========== 試験終了 ==========\n";
        std::cout << "最終スコア: " << s.score << "\n";
    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

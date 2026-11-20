// ブラウザで遊ぶためのサーバー
//   使い方: ./server [ポート番号(省略時 8080)]
//   gakumas_ai フォルダ(data/ と web/ がある場所)で実行し、ブラウザで http://localhost:8080 を開く
//
// ルールの計算はすべて src/ の C++ で行い、ブラウザは表示と操作だけを担当する。
// 外部ライブラリを使わないよう、localhost 専用の最小限の HTTP サーバーを自前で書いている。
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
using socket_t = SOCKET;
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define CLOSE_SOCKET close
#endif

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "game.hpp"
#include "loader.hpp"

namespace {

// ---------- JSON を組み立てる小さな道具 ----------

std::string json_str(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out + "\"";
}

std::string json_bool(bool b) { return b ? "true" : "false"; }

// ---------- 試験の進行を管理する ----------

struct Game {
    CardDb db;
    std::vector<DeckCard> deck;
    Exam exam;
    State state;
    Rng rng;
    unsigned seed = 0;
    std::vector<std::string> log;

    void load() {
        db = load_cards("data/cards.csv");
        load_effects("data/effects.csv", db);
        deck = load_deck("data/deck_sense_first.csv", db);
        exam = load_exam("data/exam_sample.csv");
    }

    void new_game(unsigned s) {
        seed = s;
        rng.seed(seed);
        state = init_state(deck, exam, rng);
        start_turn(state, rng);
        log.clear();
    }
};

const char* cost_type_name(CostType t) {
    switch (t) {
        case CostType::Normal: return "normal";
        case CostType::Hp: return "hp";
        case CostType::GoodCondition: return "good_condition";
        case CostType::Concentration: return "concentration";
        default: return "other";
    }
}

// カードが使えない理由(使えるなら空文字)
std::string unusable_reason(const State& s, int uid) {
    if (can_use(s, uid)) return "";
    const CardVersion& v = s.cards[uid].version();
    for (const UseCondition& u : v.use_conditions) {
        if (u.kind == UseConditionKind::GoodCondition && s.good_condition <= 0) {
            return "好調のときだけ使えます";
        }
    }
    switch (v.cost_type) {
        case CostType::Normal:
        case CostType::Hp: return "体力が足りません";
        case CostType::GoodCondition: return "好調のターン数が足りません";
        case CostType::Concentration: return "集中が足りません";
        default: return "使えません";
    }
}

// カードを使ったらどう変わるかを、状態のコピーで試して返す
std::string preview_json(const Game& g, int uid) {
    State after = g.state;
    use_card(after, uid, g.exam);
    const State& b = g.state;
    std::ostringstream o;
    o << "{\"score\":" << after.score - b.score << ",\"hp\":" << after.hp - b.hp
      << ",\"genki\":" << after.genki - b.genki
      << ",\"good_condition\":" << after.good_condition - b.good_condition
      << ",\"perfect_condition\":" << after.perfect_condition - b.perfect_condition
      << ",\"concentration\":" << after.concentration - b.concentration << "}";
    return o.str();
}

std::string card_json(const Game& g, int uid, bool with_preview) {
    const DeckCard& c = g.state.cards[uid];
    const CardVersion& v = c.version();
    std::string reason = with_preview ? unusable_reason(g.state, uid) : "";
    std::ostringstream o;
    o << "{\"uid\":" << uid << ",\"name\":" << json_str(c.display_name())
      << ",\"type\":" << json_str(c.def->type == CardType::Active ? "active" : "mental")
      << ",\"cost\":" << v.cost << ",\"cost_type\":" << json_str(cost_type_name(v.cost_type))
      << ",\"text\":" << json_str(v.text) << ",\"once\":" << json_bool(c.def->once_per_lesson);
    if (with_preview) {
        o << ",\"usable\":" << json_bool(reason.empty()) << ",\"reason\":" << json_str(reason);
        if (reason.empty()) o << ",\"preview\":" << preview_json(g, uid);
    }
    o << "}";
    return o.str();
}

std::string zone_json(const Game& g, const std::vector<int>& zone) {
    std::string out = "[";
    for (size_t i = 0; i < zone.size(); ++i) {
        if (i) out += ",";
        out += card_json(g, zone[i], false);
    }
    return out + "]";
}

std::string state_json(const Game& g) {
    const State& s = g.state;
    bool ended = is_end(s, g.exam);
    std::ostringstream o;
    o << "{\"seed\":" << g.seed << ",\"ended\":" << json_bool(ended) << ",\"turn\":" << s.turn
      << ",\"max_turn\":" << g.exam.max_turn << ",\"score\":" << s.score << ",\"hp\":" << s.hp
      << ",\"max_hp\":" << g.exam.max_hp << ",\"genki\":" << s.genki
      << ",\"good_condition\":" << s.good_condition << ",\"perfect_condition\":" << s.perfect_condition
      << ",\"concentration\":" << s.concentration << ",\"rest_heal\":" << kRestHeal << ",\"hand\":[";
    for (size_t i = 0; i < s.hand.size(); ++i) {
        if (i) o << ",";
        o << card_json(g, s.hand[i], !ended);
    }
    // 山札は順番を見せないよう、uid の順に並べ替えて返す
    std::vector<int> deck_sorted = s.deck;
    std::sort(deck_sorted.begin(), deck_sorted.end());
    o << "],\"deck\":" << zone_json(g, deck_sorted) << ",\"discard\":" << zone_json(g, s.discard)
      << ",\"exile\":" << zone_json(g, s.exile) << ",\"log\":[";
    for (size_t i = 0; i < g.log.size(); ++i) {
        if (i) o << ",";
        o << json_str(g.log[i]);
    }
    o << "]}";
    return o.str();
}

// ---------- 操作 ----------

std::string sign(int v) { return (v >= 0 ? "+" : "") + std::to_string(v); }

void do_use(Game& g, int uid) {
    if (is_end(g.state, g.exam)) throw std::runtime_error("試験は終わっています");
    if (uid < 0 || uid >= static_cast<int>(g.state.cards.size()) || !can_use(g.state, uid)) {
        throw std::runtime_error("そのカードは使えません");
    }
    int turn = g.state.turn;
    int score = g.state.score;
    std::string name = g.state.cards[uid].display_name();
    step(g.state, Action::use(uid), g.exam, g.rng);
    g.log.push_back("ターン" + std::to_string(turn) + " " + name + " スコア" + sign(g.state.score - score));
}

void do_rest(Game& g) {
    if (is_end(g.state, g.exam)) throw std::runtime_error("試験は終わっています");
    int turn = g.state.turn;
    int hp = g.state.hp;
    step(g.state, Action::rest(), g.exam, g.rng);
    g.log.push_back("ターン" + std::to_string(turn) + " 休む 体力" + sign(g.state.hp - hp));
}

// ---------- HTTP ----------

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
};

Request parse_request(const std::string& raw) {
    Request r;
    std::istringstream in(raw);
    std::string target;
    in >> r.method >> target;
    size_t q = target.find('?');
    r.path = target.substr(0, q);
    if (q != std::string::npos) {
        std::istringstream qs(target.substr(q + 1));
        std::string pair;
        while (std::getline(qs, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq != std::string::npos) r.query[pair.substr(0, eq)] = pair.substr(eq + 1);
        }
    }
    return r;
}

std::string response(int code, const std::string& type, const std::string& body) {
    const char* text = code == 200 ? "OK" : code == 400 ? "Bad Request" : "Not Found";
    std::ostringstream o;
    o << "HTTP/1.1 " << code << " " << text << "\r\n"
      << "Content-Type: " << type << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Cache-Control: no-store\r\n"
      << "Connection: close\r\n\r\n"
      << body;
    return o.str();
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error(path + " を開けません");
    std::ostringstream o;
    o << in.rdbuf();
    return o.str();
}

int to_int(const std::map<std::string, std::string>& q, const std::string& key) {
    auto it = q.find(key);
    if (it == q.end()) throw std::runtime_error(key + " がありません");
    try {
        return std::stoi(it->second);
    } catch (...) {
        throw std::runtime_error(key + " が数値ではありません");
    }
}

std::string handle(Game& g, const Request& r) {
    const std::string kJson = "application/json; charset=utf-8";
    try {
        if (r.method == "GET" && (r.path == "/" || r.path == "/index.html")) {
            return response(200, "text/html; charset=utf-8", read_file("web/index.html"));
        }
        if (r.method == "GET" && r.path == "/api/state") return response(200, kJson, state_json(g));
        if (r.method == "POST" && r.path == "/api/new") {
            unsigned seed = r.query.count("seed") ? static_cast<unsigned>(to_int(r.query, "seed"))
                                                  : std::random_device{}();
            g.new_game(seed);
            return response(200, kJson, state_json(g));
        }
        if (r.method == "POST" && r.path == "/api/use") {
            do_use(g, to_int(r.query, "uid"));
            return response(200, kJson, state_json(g));
        }
        if (r.method == "POST" && r.path == "/api/rest") {
            do_rest(g);
            return response(200, kJson, state_json(g));
        }
        return response(404, "text/plain; charset=utf-8", "not found");
    } catch (const std::exception& e) {
        return response(400, kJson, "{\"error\":" + json_str(e.what()) + "}");
    }
}

std::string read_request(socket_t client) {
    std::string raw;
    char buf[4096];
    while (raw.find("\r\n\r\n") == std::string::npos && raw.size() < 65536) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, n);
    }
    return raw;
}

void send_all(socket_t client, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(client, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return;
        sent += n;
    }
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "エラー: ネットワークの初期化に失敗しました\n";
        return 1;
    }
#endif
    int port = argc > 1 ? std::stoi(argv[1]) : 8080;

    Game game;
    try {
        game.load();
        game.new_game(std::random_device{}());
        read_file("web/index.html");  // 起動時に画面のファイルがあるか確かめる
    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << "\n";
        return 1;
    }

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        std::cerr << "エラー: ソケットを作れません\n";
        return 1;
    }
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // このパソコンからだけ接続できる
    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(server, 8) != 0) {
        std::cerr << "エラー: ポート " << port << " を使えません(すでに使われている可能性があります)\n";
        return 1;
    }

    std::cout << "ブラウザで http://localhost:" << port << " を開いてください(終了は Ctrl + C)\n";
    while (true) {
        socket_t client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        std::string raw = read_request(client);
        if (!raw.empty()) send_all(client, handle(game, parse_request(raw)));
        CLOSE_SOCKET(client);
    }
}

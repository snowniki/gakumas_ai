#include "csv.hpp"

#include <fstream>
#include <stdexcept>

namespace {

// 1行をカンマで区切る。"" で囲まれた値の中のカンマでは区切らない
std::vector<std::string> split_line(const std::string& line, const std::string& path, int line_no) {
    std::vector<std::string> fields;
    std::string cur;
    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {  // "" は " 1文字として扱う
                    cur += '"';
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cur += c;
            }
        } else if (c == '"') {
            in_quotes = true;
        } else if (c == ',') {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (in_quotes) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) + ": \" が閉じられていません");
    }
    fields.push_back(cur);
    return fields;
}

}  // namespace

int CsvTable::column(const std::string& name) const {
    for (size_t i = 0; i < header.size(); ++i) {
        if (header[i] == name) return static_cast<int>(i);
    }
    throw std::runtime_error(path + ": 列 " + name + " がありません");
}

const std::string& CsvTable::get(const CsvRow& row, const std::string& name) const {
    return row.fields[column(name)];
}

std::string CsvTable::where(const CsvRow& row) const {
    return path + ":" + std::to_string(row.line) + ": ";
}

CsvTable read_csv(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error(path + ": ファイルを開けません");

    CsvTable table;
    table.path = path;
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') line.pop_back();     // Windows の改行
        if (line_no == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {  // BOM
            line.erase(0, 3);
        }
        if (line.empty()) continue;

        std::vector<std::string> fields = split_line(line, path, line_no);
        if (table.header.empty()) {
            table.header = fields;
            continue;
        }
        if (fields.size() != table.header.size()) {
            throw std::runtime_error(path + ":" + std::to_string(line_no) + ": 列の数が " +
                                     std::to_string(fields.size()) + " です(1行目は " +
                                     std::to_string(table.header.size()) + ")");
        }
        table.rows.push_back({line_no, fields});
    }
    if (table.header.empty()) throw std::runtime_error(path + ": 中身が空です");
    return table;
}

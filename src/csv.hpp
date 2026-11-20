// CSV ファイルの読み込み
#pragma once

#include <string>
#include <vector>

struct CsvRow {
    int line = 0;                     // ファイルの何行目か(エラー表示用)
    std::vector<std::string> fields;
};

struct CsvTable {
    std::string path;
    std::vector<std::string> header;
    std::vector<CsvRow> rows;

    // 列名から列の番号を探す。見つからなければエラー
    int column(const std::string& name) const;
    // row の name 列の値を返す
    const std::string& get(const CsvRow& row, const std::string& name) const;
    // エラーメッセージの頭に「ファイル名:行番号」を付ける
    std::string where(const CsvRow& row) const;
};

// CSV を読む。1行目は列名。BOM・行末の \r・"" で囲まれた値に対応
CsvTable read_csv(const std::string& path);

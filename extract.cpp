#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "alphabetparameters.h"
#include "board.h"
#include "bag.h"
#include "rack.h"
#include "move.h"

int encodeLetter(char ch) {
    if (ch == '.' || ch == ' ' || ch == '\0') return 0;

    ch = std::toupper(static_cast<unsigned char>(ch));

    if (ch == '?' || ch == '_') return 27;
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 1;

    return 0;
}

std::string joinInts(const std::vector<int>& values) {
    std::ostringstream out;

    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << ";";
        out << values[i];
    }

    return out.str();
}

std::pair<std::string, std::string> encodeMoveLetters(
    const std::string& word,
    int maxLen = 15
) {
    std::vector<int> ids(maxLen, 0);
    std::vector<int> mask(maxLen, 0);

    const int n = std::min(static_cast<int>(word.size()), maxLen);

    for (int i = 0; i < n; ++i) {
        ids[i] = encodeLetter(word[i]);
        mask[i] = 1;
    }

    return {joinInts(ids), joinInts(mask)};
}

void writeHeader(std::ofstream& file) {
    file
        << "game_id,"
        << "move_id,"
        << "board_letters,"
        << "board_premiums,"
        << "rack_counts,"
        << "bag_counts,"
        << "move_letters,"
        << "move_mask,"
        << "move_position,"
        << "move_dir,"
        << "move_score,"
        << "context,"
        << "target_equity\n";
}

// 0 = empty, 1-26 = A-Z, 27 = blank
int letterToNumber(Quackle::Letter l) {
    if (l == QUACKLE_NULL_MARK) return 0;
    if (l == QUACKLE_BLANK_MARK) return 27;

    char ch = QUACKLE_ALPHABET_PARAMETERS->letterToChar(l);
    ch = std::toupper(static_cast<unsigned char>(ch));

    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A' + 1;
    }

    return 0;
}

std::string letterStringToAscii(const Quackle::LetterString& letters) {
    std::string out;

    for (Quackle::Letter l : letters) {
        if (l == QUACKLE_NULL_MARK) continue;
        if (Quackle::Move::isAlreadyOnBoard(l)) continue;

        if (l == QUACKLE_BLANK_MARK) {
            out += '?';
            continue;
        }

        char ch = QUACKLE_ALPHABET_PARAMETERS->letterToChar(l);
        out += std::toupper(static_cast<unsigned char>(ch));
    }

    return out;
}

std::string buildCountString(const std::vector<Quackle::Letter>& tiles) {
    std::vector<int> counts(27, 0);

    for (const Quackle::Letter tile : tiles) {
        const int idx = letterToNumber(tile);

        if (idx >= 1 && idx <= 27) {
            counts[idx - 1]++;
        }
    }

    return joinInts(counts);
}

std::string boardToFlatString(const Quackle::Board& board) {
    std::vector<int> values;
    values.reserve(225);

    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            values.push_back(letterToNumber(board.letter(r, c)));
        }
    }

    return joinInts(values);
}

std::string rackToCountString(const Quackle::Rack& rack) {
    return buildCountString(rack.tiles());
}

std::string bagToCountString(const Quackle::Bag& bag) {
    return buildCountString(bag.tiles());
}

const std::string& boardPremiumsString() {
    static const std::string premiums =
        "4;0;0;1;0;0;0;4;0;0;0;1;0;0;4;"
        "0;3;0;0;0;2;0;0;0;2;0;0;0;3;0;"
        "0;0;3;0;0;0;1;0;1;0;0;0;3;0;0;"
        "1;0;0;3;0;0;0;1;0;0;0;3;0;0;1;"
        "0;0;0;0;3;0;0;0;0;0;3;0;0;0;0;"
        "0;2;0;0;0;2;0;0;0;2;0;0;0;2;0;"
        "0;0;1;0;0;0;1;0;1;0;0;0;1;0;0;"
        "4;0;0;1;0;0;0;3;0;0;0;1;0;0;4;"
        "0;0;1;0;0;0;1;0;1;0;0;0;1;0;0;"
        "0;2;0;0;0;2;0;0;0;2;0;0;0;2;0;"
        "0;0;0;0;3;0;0;0;0;0;3;0;0;0;0;"
        "1;0;0;3;0;0;0;1;0;0;0;3;0;0;1;"
        "0;0;3;0;0;0;1;0;1;0;0;0;3;0;0;"
        "0;3;0;0;0;2;0;0;0;2;0;0;0;3;0;"
        "4;0;0;1;0;0;0;4;0;0;0;1;0;0;4";

    return premiums;
}

void extract(
    const std::string& csvPath,
    const std::string& gameId,
    int moveId,
    const Quackle::Board& board,
    const Quackle::Rack& rack,
    const Quackle::Bag& bag,
    const Quackle::MoveList& candidateMoves,
    int yourScore,
    int opponentScore,
    int tilesRemaining,
    bool append = true
) {
    std::ofstream file(
        csvPath,
        append ? std::ios::app : std::ios::out
    );

    if (!file.is_open()) {
        throw std::runtime_error("Could not open CSV file: " + csvPath);
    }

    if (!append) {
        writeHeader(file);
    }

    const std::string boardLetters = boardToFlatString(board);
    const std::string rackCounts = rackToCountString(rack);
    const std::string bagCounts = bagToCountString(bag);

    const int scoreDiff = yourScore - opponentScore;
    const std::string context =
        std::to_string(scoreDiff) + ";" +
        std::to_string(tilesRemaining);

    int candidateId = moveId;

    for (const Quackle::Move& move : candidateMoves) {
        if (!move.isAMove()) continue;
        if (move.action != Quackle::Move::Place) continue;
        if (move.isChallengedPhoney()) continue;

        const std::string word = letterStringToAscii(move.wordTiles());
        const auto [moveLetters, moveMask] = encodeMoveLetters(word);

        const std::string movePosition =
            std::to_string(move.startrow) + ";" +
            std::to_string(move.startcol);

        const int moveDir = move.horizontal ? 0 : 1;

        file
            << gameId << ","
            << candidateId << ","
            << boardLetters << ","
            << boardPremiumsString() << ","
            << rackCounts << ","
            << bagCounts << ","
            << moveLetters << ","
            << moveMask << ","
            << movePosition << ","
            << moveDir << ","
            << move.effectiveScore() << ","
            << context << ","
            << move.equity
            << "\n";

        ++candidateId;
    }
}

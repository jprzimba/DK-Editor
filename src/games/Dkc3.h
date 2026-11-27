#pragma once

#include "games/Game.h"

namespace dk {

class Dkc3Game final : public Game {
public:
    GameKind kind() const override { return GameKind::Dkc3; }
    const char* name() const override { return "Donkey Kong Country 3"; }
    bool init(Rom& rom, std::string& error) override;
    const std::vector<LevelEntry>& levels() const override { return levels_; }
    bool loadLevel(Rom& rom, int levelId, LoadedLevel& out, std::string& error) override;
    bool saveLevel(Rom& rom, const LoadedLevel& level, std::string& error) override;
    bool supportsVisualEditing() const override { return false; }
    std::string summary() const override;

private:
    std::vector<LevelEntry> levels_;
    std::vector<std::size_t> addresses_;
    std::vector<int> types_;
};

} // namespace dk

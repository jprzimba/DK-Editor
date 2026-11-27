#pragma once

#include "games/Game.h"

namespace dk {

class Dkc2Game final : public Game {
public:
    GameKind kind() const override { return GameKind::Dkc2; }
    const char* name() const override { return "Donkey Kong Country 2"; }
    bool init(Rom& rom, std::string& error) override;
    const std::vector<LevelEntry>& levels() const override { return levels_; }
    bool loadLevel(Rom& rom, int levelId, LoadedLevel& out, std::string& error) override;
    bool saveLevel(Rom& rom, const LoadedLevel& level, std::string& error) override;
    std::string summary() const override;

private:
    std::vector<LevelEntry> levels_;
};

} // namespace dk

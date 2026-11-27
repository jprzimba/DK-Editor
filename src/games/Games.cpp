#include "games/Game.h"
#include "games/Dkc1.h"
#include "games/Dkc2.h"
#include "games/Dkc3.h"

namespace dk {

std::unique_ptr<Game> createGame(GameKind kind) {
    switch (kind) {
    case GameKind::Dkc1: return std::make_unique<Dkc1Game>();
    case GameKind::Dkc2: return std::make_unique<Dkc2Game>();
    case GameKind::Dkc3: return std::make_unique<Dkc3Game>();
    default: return nullptr;
    }
}

} // namespace dk

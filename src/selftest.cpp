#include "games/Game.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what.c_str());
    if (!ok) ++failures;
}

std::string findLevelName(dk::Game& game, const std::string& preferred) {
    for (const auto& e : game.levels()) {
        if (e.supported && e.name == preferred) return e.name;
    }
    for (const auto& e : game.levels()) {
        if (e.supported) return e.name;
    }
    return {};
}

int idOf(dk::Game& game, const std::string& name) {
    for (const auto& e : game.levels()) {
        if (e.name == name) return e.id;
    }
    return -1;
}

void testDkc1(const std::string& path) {
    std::printf("DKC1: %s\n", path.c_str());
    dk::Rom rom;
    std::string err;
    check(rom.load(path, err), "load ROM");
    if (!rom.valid()) return;
    check(rom.game == dk::GameKind::Dkc1, "detect DKC1");

    auto game = dk::createGame(rom.game);
    check(game != nullptr, "create game module");
    if (!game) return;
    check(game->init(rom, err), "init game module");

    const int id = idOf(*game, "Jungle Hijinxs");
    check(id >= 0, "find Jungle Hijinxs");
    dk::LoadedLevel level;
    check(game->loadLevel(rom, id, level, err), "load level");
    check(level.valid(), "level has tilemap");
    check(level.width == 168 && level.height == 16, "168x16 dimensions");
    check(!level.metas.empty() && !level.charset.chars.empty(), "metatiles + tileset");
    check(level.image.size() == static_cast<std::size_t>(level.pixelWidth()) * level.pixelHeight(),
          "image allocated");

    std::size_t nonBg = 0;
    for (dk::u32 p : level.image) nonBg += (p != dk::packColor(level.background));
    check(nonBg > 1000, "rendered image contains pixels");

    int loadFailures = 0;
    std::string firstFailure;
    for (const auto& e : game->levels()) {
        if (!e.supported) continue;
        dk::LoadedLevel probe;
        std::string le;
        if (!game->loadLevel(rom, e.id, probe, le)) {
            ++loadFailures;
            if (firstFailure.empty()) firstFailure = e.name + ": " + le;
        }
    }
    check(loadFailures == 0, "all DKC1 levels load");
    if (loadFailures) std::printf("    first failure: %s\n", firstFailure.c_str());

    const auto before = level.tiles;
    level.tiles[0] = static_cast<dk::u16>((level.tiles[0] + 1) & level.indexMask);
    check(game->saveLevel(rom, level, err), "save edited tilemap");
    dk::LoadedLevel reloaded;
    check(game->loadLevel(rom, id, reloaded, err), "reload level");
    check(reloaded.tiles[0] == level.tiles[0], "edit survived reload");

    const int coralId = idOf(*game, "Coral Capers");
    if (coralId >= 0) {
        dk::LoadedLevel coral;
        if (game->loadLevel(rom, coralId, coral, err) && coral.valid()) {
            check(coral.vertical, "Coral Capers is vertical");
            const std::size_t ci = static_cast<std::size_t>(coral.width * 3 + 3);
            if (ci < coral.tiles.size()) {
                coral.tiles[ci] = static_cast<dk::u16>((coral.tiles[ci] + 1) & coral.indexMask);
                check(game->saveLevel(rom, coral, err), "save vertical tilemap");
                dk::LoadedLevel cr;
                check(game->loadLevel(rom, coralId, cr, err), "reload vertical level");
                check(cr.tiles.size() > ci && cr.tiles[ci] == coral.tiles[ci],
                      "vertical edit survived reload");
            }
        }
    }

    const std::string out = "dkc1_selftest.smc";
    check(rom.save(out, err), "write ROM copy");
    dk::Rom reread;
    check(reread.load(out, err), "reopen ROM copy");
    check(reread.checksum() == rom.checksum(), "checksum preserved");
    std::filesystem::remove(out);
    (void)before;
}

void testDkc2(const std::string& path) {
    std::printf("DKC2: %s\n", path.c_str());
    dk::Rom rom;
    std::string err;
    check(rom.load(path, err), "load ROM");
    if (!rom.valid()) return;
    check(rom.game == dk::GameKind::Dkc2, "detect DKC2");

    auto game = dk::createGame(rom.game);
    check(game != nullptr, "create game module");
    if (!game) return;
    check(game->init(rom, err), "init game module");

    const int id = idOf(*game, "Pirate panic");
    check(id >= 0, "find Pirate Panic");
    dk::LoadedLevel level;
    check(game->loadLevel(rom, id, level, err), "load level");
    if (!level.valid()) {
        std::printf("    error: %s\n", err.c_str());
        return;
    }
    check(level.width == 223 && level.height == 16, "223x16 dimensions");
    check(!level.metas.empty() && !level.charset.chars.empty(), "map chips + tileset");
    check(!level.bgImage.empty() && level.bgWidth == 256 && level.bgHeight > 0,
          "Pirate Panic background decoded");
    std::size_t nonBg = 0;
    for (dk::u32 p : level.image) nonBg += (p != dk::packColor(level.background));
    check(nonBg > 1000, "rendered image contains pixels");

    // Repainting a single edited cell must reproduce a full rebuild exactly, so
    // terrain edits never damage the scenery drawn behind them.
    {
        int cellX = -1, cellY = -1;
        for (int y = 0; y < level.height && cellX < 0; ++y) {
            for (int x = 0; x < level.width; ++x) {
                if ((level.tiles[static_cast<std::size_t>(y) * level.width + x] & 0x0FFF) != 0) {
                    cellX = x;
                    cellY = y;
                    break;
                }
            }
        }
        check(cellX >= 0, "found a terrain cell to repaint");
        if (cellX >= 0) {
            std::vector<dk::u16> cleared = level.tiles;
            cleared[static_cast<std::size_t>(cellY) * level.width + cellX] = 0;
            std::vector<dk::u32> rebuilt;
            dk::buildLevelImage(rebuilt, level.width, level.height, cleared, level.indexMask,
                                level.metas, level.charset, level.palette, level.background,
                                level.backgroundBottom, level.bgImage, level.bgWidth,
                                level.bgHeight);
            dk::fillLevelBackground(level.image, level.pixelWidth(), level.pixelWidth(),
                                    level.pixelHeight(), cellX * 32, cellY * 32, 32, 32,
                                    level.background, level.backgroundBottom, level.bgImage,
                                    level.bgWidth, level.bgHeight);
            bool same = true;
            for (int y = 0; y < 32 && same; ++y) {
                for (int x = 0; x < 32 && same; ++x) {
                    const std::size_t o =
                        static_cast<std::size_t>(cellY * 32 + y) * level.pixelWidth() +
                        cellX * 32 + x;
                    if (level.image[o] != rebuilt[o]) same = false;
                }
            }
            check(same, "tile repaint preserves background");
        }
    }

    int loadFailures = 0;
    std::string firstFailure;
    for (const auto& e : game->levels()) {
        if (!e.supported) continue;
        dk::LoadedLevel probe;
        std::string le;
        if (!game->loadLevel(rom, e.id, probe, le)) {
            ++loadFailures;
            if (firstFailure.empty()) firstFailure = e.name + ": " + le;
        }
    }
    check(loadFailures == 0, "all DKC2 levels load");
    if (loadFailures) std::printf("    first failure: %s\n", firstFailure.c_str());

    const int tileIndex = level.width * 5 + 3;
    const dk::u16 original = level.tiles[tileIndex];
    level.tiles[tileIndex] = static_cast<dk::u16>((original + 1) & level.indexMask);
    check(game->saveLevel(rom, level, err), "save (recompress) tilemap");
    if (!err.empty()) std::printf("    %s\n", err.c_str());
    dk::LoadedLevel reloaded;
    check(game->loadLevel(rom, id, reloaded, err), "reload level");
    check(reloaded.tiles[tileIndex] == level.tiles[tileIndex], "edit survived reload");

    // Stress the recompression/relocation path with many changed tiles.
    std::vector<dk::u24> ptrBefore(32);
    for (int m = 0; m < 32; ++m) ptrBefore[m] = rom.longptr(0x35BAEF + m * 3);
    unsigned rng = 0x1234567;
    for (std::size_t i = 0; i < level.tiles.size(); ++i) {
        rng = rng * 1103515245u + 12345u;
        if ((i % 3) == 0) {
            level.tiles[i] = static_cast<dk::u16>((rng >> 8) % level.metas.size());
        }
    }
    check(game->saveLevel(rom, level, err), "stress save (recompress) tilemap");
    bool relocated = false;
    for (int m = 0; m < 32; ++m) {
        const dk::u24 now = rom.longptr(0x35BAEF + m * 3);
        if (now != ptrBefore[m]) {
            relocated = true;
            std::printf("    map %d pointer %06X -> %06X\n", m, ptrBefore[m], now);
        }
    }
    (void)relocated;
    dk::LoadedLevel stressReload;
    check(game->loadLevel(rom, id, stressReload, err), "reload after stress save");
    check(stressReload.tiles.size() == level.tiles.size(), "stress reload shape");
    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < level.tiles.size(); ++i) {
        if (stressReload.tiles[i] != level.tiles[i]) ++mismatches;
    }
    check(mismatches == 0, "stress edits survived reload");

    // Saving twice without reloading must keep using the relocated pointer.
    level.tiles[0] = static_cast<dk::u16>((level.tiles[0] + 3) & level.indexMask);
    check(game->saveLevel(rom, level, err), "second save without reload");
    dk::LoadedLevel secondReload;
    check(game->loadLevel(rom, id, secondReload, err), "reload after second save");
    check(!secondReload.tiles.empty() && secondReload.tiles[0] == level.tiles[0],
          "second save survived reload");

    // Vertical-layout round trip.
    const int vertId = idOf(*game, "Glimer galleon");
    if (vertId >= 0) {
        dk::LoadedLevel vert;
        if (game->loadLevel(rom, vertId, vert, err) && vert.valid()) {
            check(vert.vertical, "Glimmer's Galleon is vertical");
            const std::size_t vi = static_cast<std::size_t>(vert.width * 4 + 4);
            if (vi < vert.tiles.size()) {
                vert.tiles[vi] = static_cast<dk::u16>((vert.tiles[vi] + 1) & vert.indexMask);
                check(game->saveLevel(rom, vert, err), "save vertical tilemap");
                dk::LoadedLevel vr;
                check(game->loadLevel(rom, vertId, vr, err), "reload vertical level");
                check(vr.tiles.size() > vi && vr.tiles[vi] == vert.tiles[vi],
                      "vertical edit survived reload");
            }
        }
    }

    const std::string out = "dkc2_selftest.smc";
    check(rom.save(out, err), "write ROM copy");
    dk::Rom reread;
    check(reread.load(out, err), "reopen ROM copy");
    check(reread.checksum() == rom.checksum(), "checksum preserved");
    std::filesystem::remove(out);
}

void testDkc3(const std::string& path) {
    std::printf("DKC3: %s\n", path.c_str());
    dk::Rom rom;
    std::string err;
    check(rom.load(path, err), "load ROM");
    if (!rom.valid()) return;
    check(rom.game == dk::GameKind::Dkc3, "detect DKC3");

    auto game = dk::createGame(rom.game);
    check(game != nullptr, "create game module");
    if (!game) return;
    check(game->init(rom, err), "init game module");

    const int id = idOf(*game, "Lakeside Limbo");
    check(id >= 0, "find Lakeside Limbo");
    dk::LoadedLevel level;
    check(game->loadLevel(rom, id, level, err), "load level properties");
    check(!level.properties.empty(), "properties available");
    if (level.properties.empty()) return;

    auto& music = level.properties[0];
    const int old = music.value;
    music.value = (old + 1) & 0xFF;
    check(game->saveLevel(rom, level, err), "save properties");
    dk::LoadedLevel reloaded;
    check(game->loadLevel(rom, id, reloaded, err), "reload properties");
    check(!reloaded.properties.empty() && reloaded.properties[0].value == music.value,
          "property edit survived reload");

    const std::string out = "dkc3_selftest.smc";
    check(rom.save(out, err), "write ROM copy");
    dk::Rom reread;
    check(reread.load(out, err), "reopen ROM copy");
    check(reread.checksum() == rom.checksum(), "checksum preserved");
    std::filesystem::remove(out);
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> paths;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) paths.emplace_back(argv[i]);
    } else {
        for (const auto& e : std::filesystem::directory_iterator(".")) {
            if (!e.is_regular_file()) continue;
            const std::string name = e.path().string();
            if (name.size() > 4 &&
                (name.substr(name.size() - 4) == ".smc" || name.substr(name.size() - 4) == ".sfc")) {
                paths.push_back(name);
            }
        }
    }
    if (paths.empty()) {
        std::printf("Usage: DKEditorSelftest <rom...>\n");
        return 2;
    }

    for (const std::string& path : paths) {
        dk::Rom probe;
        std::string err;
        if (!probe.load(path, err)) {
            std::printf("Skipping %s: %s\n", path.c_str(), err.c_str());
            continue;
        }
        switch (probe.game) {
        case dk::GameKind::Dkc1: testDkc1(path); break;
        case dk::GameKind::Dkc2: testDkc2(path); break;
        case dk::GameKind::Dkc3: testDkc3(path); break;
        default: break;
        }
        std::printf("\n");
    }

    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures,
                failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}

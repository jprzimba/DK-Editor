#pragma once

#include "core/Compression.h"
#include "core/Graphics.h"
#include "core/Rom.h"

#include <memory>
#include <string>
#include <vector>

namespace dk {

struct LevelEntry {
    int id = 0;
    std::string name;
    std::string group;
    bool supported = true;
    bool visual = true;
    std::string note;
};

struct LevelContext {
    virtual ~LevelContext() = default;
};

struct LevelProperty {
    std::string label;
    int value = 0;
    int minimum = 0;
    int maximum = 255;
    std::size_t offset = 0; // file offset in ROM
    int bytes = 1;
    int bit = -1; // if >= 0, single bit inside the byte at offset
    int displayOffset = 0; // value shown = value + displayOffset
};

struct LoadedLevel {
    LevelEntry info;
    int width = 0;
    int height = 0;
    bool vertical = false;
    int indexMask = 0x3FF;
    std::vector<u16> tiles; // row-major, y * width + x
    std::vector<Metatile> metas;
    Charset charset;
    Palette palette;
    Color background{8, 10, 18, 255};
    std::vector<u32> image; // width*32 x height*32, RGBA
    std::vector<LevelProperty> properties;
    std::shared_ptr<LevelContext> context;

    int pixelWidth() const { return width * 32; }
    int pixelHeight() const { return height * 32; }
    bool valid() const { return width > 0 && height > 0 && !tiles.empty(); }
};

class Game {
public:
    virtual ~Game() = default;
    virtual GameKind kind() const = 0;
    virtual const char* name() const = 0;
    virtual bool init(Rom& rom, std::string& error) = 0;
    virtual const std::vector<LevelEntry>& levels() const = 0;
    virtual bool loadLevel(Rom& rom, int levelId, LoadedLevel& out,
                           std::string& error) = 0;
    virtual bool saveLevel(Rom& rom, const LoadedLevel& level,
                           std::string& error) = 0;
    virtual bool supportsVisualEditing() const { return true; }
    virtual std::string summary() const { return {}; }
};

std::unique_ptr<Game> createGame(GameKind kind);

} // namespace dk

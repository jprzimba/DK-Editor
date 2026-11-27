#pragma once

#include "core/Rom.h"
#include "games/Game.h"
#include "ui/Texture.h"

#include <d3d11.h>
#include <imgui.h>

#include <memory>
#include <string>
#include <vector>

namespace dk {

enum class Tool {
    Paint,
    Picker,
    Erase,
    Select,
    Sprites,
};

class EditorApp {
public:
    EditorApp(ID3D11Device* device, ID3D11DeviceContext* context);
    ~EditorApp();

    void setWindow(void* hwnd) { hwnd_ = hwnd; }
    bool openInitialRom(const std::string& path) { return openRom(path); }
    bool selectLevel(int id) { return loadLevel(id); }
    void draw();

private:
    void drawMenuBar();
    void drawToolbar();
    void drawItemBar();
    void drawLevelPanel();
    void drawCanvas();
    void drawToolPanel();
    void drawStatusBar();
    void drawDkc3Properties();

    void openRomDialog();
    void saveRom(bool saveAs);
    bool openRom(const std::string& path);
    bool loadLevel(int id);
    void fitView(const ImVec2& canvasSize);
    void fitHeightView(const ImVec2& canvasSize);
    void uploadLevelTexture();
    void uploadAtlasTexture();
    void rebuildLevelImage();
    void paintAt(int tx, int ty);
    void eraseAt(int tx, int ty);
    void pickAt(int tx, int ty);
    void beginStroke();
    void endStroke();
    void undo();
    void redo();
    void setStatus(const std::string& text, bool error = false);

    // Rectangle selection, Lunar Magic style.
    bool hasSelection() const;
    void clearSelection();
    void normalizeSelection();
    void deleteSelection();
    void fillSelection();
    void copySelection();
    void pasteClipboard(int tx, int ty);
    void pushUndoSnapshot(const std::vector<u16>& before);
    void requestFit();

    int atlasColumns() const { return 8; }
    int atlasRows() const;
    ImVec2 atlasUv(int index) const; // returns uv0, use +32px for uv1

    std::string openFileDialog(bool save);

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    void* hwnd_ = nullptr;

    Rom rom_;
    std::unique_ptr<Game> game_;
    std::vector<LevelEntry> emptyLevels_;
    int selectedLevel_ = -1;
    LoadedLevel level_;
    std::string levelError_;

    Texture levelTex_;
    Texture atlasTex_;
    Texture previewTex_;

    Tool tool_ = Tool::Paint;
    bool flipH_ = false;
    bool flipV_ = false;
    int selectedMeta_ = -1;
    bool showGrid_ = true;
    bool showBackground_ = true;
    bool showCollision_ = false; // reserved
    float zoom_ = 0.5f;
    int autoFitFrames_ = 2;
    bool fitWhole_ = false;
    bool fitRequest_ = false;
    bool fitted_ = false;
    float preFitZoom_ = 1.0f;
    bool pendingScroll_ = false;
    ImVec2 pendingScrollTarget_{0, 0};
    bool dirty_ = false;
    int scrollFrames_ = 0;
    int paletteScrollFrames_ = 0;

    // Selection (tile coordinates, inclusive).
    int selX0_ = -1, selY0_ = -1, selX1_ = -1, selY1_ = -1;
    bool selecting_ = false;
    std::vector<u16> clipboard_;
    int clipW_ = 0, clipH_ = 0;

    bool strokeActive_ = false;
    bool strokeChanged_ = false;
    std::vector<u16> strokeBefore_;
    std::vector<std::vector<u16>> undoStack_;
    std::vector<std::vector<u16>> redoStack_;

    std::string search_;
    std::string status_;
    bool statusError_ = false;
    double statusTime_ = 0.0;

    bool showAbout_ = false;
    bool openRequested_ = false;
    bool saveRequested_ = false;
    bool saveAsRequested_ = false;
    int cursorX_ = -1;
    int cursorY_ = -1;
    std::string lastRomDir_;
    int selectedItem_ = -1;
    int selectedSprite_ = -1;
    int draggingSprite_ = -1;
    bool spriteMoved_ = false;
};

} // namespace dk

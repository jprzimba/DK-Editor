#include "ui/EditorApp.h"

#include "games/Dkc1.h"
#include "games/Dkc2.h"
#include "games/Dkc3.h"

#include <windows.h>
#include <commdlg.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace dk {

namespace {
std::string narrowPath(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                                nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n,
                        nullptr, nullptr);
    return s;
}

std::wstring widenPath(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

const wchar_t kDialogFilter[] =
    L"SNES ROM (*.smc;*.sfc)\0*.smc;*.sfc\0All files (*.*)\0*.*\0";

ImU32 col(float r, float g, float b, float a = 1.0f) {
    return ImGui::GetColorU32(ImVec4(r, g, b, a));
}
} // namespace

EditorApp::EditorApp(ID3D11Device* device, ID3D11DeviceContext* context)
    : device_(device), context_(context) {
    (void)emptyLevels_;
    setStatus("Open a Donkey Kong Country ROM to begin.");
}

EditorApp::~EditorApp() = default;

void EditorApp::setStatus(const std::string& text, bool error) {
    status_ = text;
    statusError_ = error;
    statusTime_ = ImGui::GetTime();
}

std::string EditorApp::openFileDialog(bool save) {
    wchar_t filename[MAX_PATH] = {};
    if (!save && !lastRomDir_.empty()) {
        const std::wstring w = widenPath(lastRomDir_);
        wcsncpy(filename, w.c_str(), MAX_PATH - 1);
    }
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(hwnd_);
    ofn.lpstrFilter = kDialogFilter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"smc";
    ofn.Flags = OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    const BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    if (!ok) return {};
    std::string result = narrowPath(filename);
    const std::size_t slash = result.find_last_of("\\/");
    if (slash != std::string::npos) lastRomDir_ = result.substr(0, slash);
    return result;
}

void EditorApp::openRomDialog() {
    const std::string path = openFileDialog(false);
    if (!path.empty()) openRom(path);
}

bool EditorApp::openRom(const std::string& path) {
    std::string err;
    if (!rom_.load(path, err)) {
        setStatus(err.empty() ? "Could not open the ROM." : err, true);
        return false;
    }
    game_ = createGame(rom_.game);
    if (!game_) {
        setStatus("Unsupported ROM.", true);
        return false;
    }
    if (!game_->init(rom_, err)) {
        setStatus(err.empty() ? "Could not initialise the game module." : err, true);
        game_.reset();
        return false;
    }
    const std::size_t slash = path.find_last_of("\\/");
    if (slash != std::string::npos) lastRomDir_ = path.substr(0, slash);

    selectedLevel_ = -1;
    dirty_ = false;
    undoStack_.clear();
    redoStack_.clear();
    level_ = LoadedLevel{};
    levelError_.clear();

    // Pick a pleasant default level for each game.
    const char* preferred = nullptr;
    if (rom_.game == GameKind::Dkc1) preferred = "Jungle Hijinxs";
    else if (rom_.game == GameKind::Dkc2) preferred = "Pirate panic";
    else if (rom_.game == GameKind::Dkc3) preferred = "Lakeside Limbo";
    int pick = -1;
    for (const auto& e : game_->levels()) {
        if (e.supported && preferred && e.name == preferred) {
            pick = e.id;
            break;
        }
    }
    if (pick < 0) {
        for (const auto& e : game_->levels()) {
            if (e.supported) {
                pick = e.id;
                break;
            }
        }
    }
    if (pick >= 0) loadLevel(pick);

    setStatus("Loaded " + rom_.gameName() + " (" + rom_.versionName() + "). " +
              game_->summary());
    (void)err;
    return true;
}

bool EditorApp::loadLevel(int id) {
    if (!game_) return false;
    selectedLevel_ = id;
    level_ = LoadedLevel{};
    levelError_.clear();
    undoStack_.clear();
    redoStack_.clear();
    dirty_ = false;
    scrollFrames_ = 2;
    paletteScrollFrames_ = 2;
    selectedMeta_ = 0;
    flipH_ = flipV_ = false;
    autoFitFrames_ = 2;
    fitWhole_ = false;
    pendingScroll_ = false;

    std::string err;
    if (!game_->loadLevel(rom_, id, level_, err)) {
        levelError_ = err;
        setStatus(err.empty() ? "Could not load this level." : err, true);
        levelTex_.release();
        atlasTex_.release();
        previewTex_.release();
        return false;
    }
    uploadLevelTexture();
    uploadAtlasTexture();
    selectedMeta_ = 0;
    if (!level_.metas.empty()) {
        std::vector<int> counts(level_.metas.size(), 0);
        for (u16 t : level_.tiles) {
            const int mi = t & level_.indexMask;
            if (mi > 0 && mi < static_cast<int>(counts.size())) ++counts[mi];
        }
        int best = 0;
        for (int i = 1; i < static_cast<int>(counts.size()); ++i) {
            if (counts[i] > counts[best]) best = i;
        }
        selectedMeta_ = best;
    }
    if (level_.info.visual) {
        setStatus("Loaded " + level_.info.name + " (" +
                  std::to_string(level_.width) + " x " + std::to_string(level_.height) +
                  " tiles).");
    } else {
        setStatus(level_.info.name + ": " + level_.info.note);
    }
    return true;
}

void EditorApp::uploadLevelTexture() {
    if (!level_.valid() || level_.image.empty()) {
        levelTex_.release();
        return;
    }
    if (!levelTex_.ensure(device_, level_.pixelWidth(), level_.pixelHeight())) return;
    levelTex_.upload(context_, level_.image, level_.pixelWidth(), level_.pixelHeight());
}

void EditorApp::rebuildLevelImage() {
    if (!level_.valid()) return;
    buildLevelImage(level_.image, level_.width, level_.height, level_.tiles,
                    level_.indexMask, level_.metas, level_.charset, level_.palette,
                    level_.background);
    uploadLevelTexture();
}

int EditorApp::atlasRows() const {
    if (level_.metas.empty()) return 0;
    return (static_cast<int>(level_.metas.size()) + atlasColumns() - 1) / atlasColumns();
}

ImVec2 EditorApp::atlasUv(int index) const {
    const int rows = atlasRows();
    if (rows <= 0) return ImVec2(0, 0);
    const int col = index % atlasColumns();
    const int row = index / atlasColumns();
    return ImVec2(static_cast<float>(col * 32) / (atlasColumns() * 32),
                  static_cast<float>(row * 32) / (rows * 32));
}

void EditorApp::uploadAtlasTexture() {
    if (level_.metas.empty() || level_.charset.chars.empty()) {
        atlasTex_.release();
        return;
    }
    const int columns = atlasColumns();
    const int rows = atlasRows();
    const int w = columns * 32;
    const int h = rows * 32;
    std::vector<u32> pixels(static_cast<std::size_t>(w) * h, packColor(Color{24, 24, 28, 255}));
    for (int i = 0; i < static_cast<int>(level_.metas.size()); ++i) {
        const auto thumb = renderMetatileThumbnail(level_.metas[i], level_.charset,
                                                   level_.palette);
        const int cx = (i % columns) * 32;
        const int cy = (i / columns) * 32;
        for (int y = 0; y < 32; ++y) {
            std::memcpy(pixels.data() + static_cast<std::size_t>(cy + y) * w + cx,
                        thumb.data() + y * 32, 32 * sizeof(u32));
        }
    }
    if (atlasTex_.ensure(device_, w, h)) {
        atlasTex_.upload(context_, pixels, w, h);
    }
}

void EditorApp::fitView(const ImVec2& canvasSize) {
    if (!level_.valid()) return;
    const float pw = static_cast<float>(level_.pixelWidth());
    const float ph = static_cast<float>(level_.pixelHeight());
    if (pw <= 0 || ph <= 0) return;
    float z = std::min((canvasSize.x - 32.0f) / pw, (canvasSize.y - 32.0f) / ph);
    z = std::clamp(z, 0.05f, 1.0f);
    zoom_ = z;
    const float cw = std::max(canvasSize.x, pw * z + 32.0f);
    const float ch = std::max(canvasSize.y, ph * z + 32.0f);
    pendingScrollTarget_.x = std::max(0.0f, (cw - canvasSize.x) * 0.5f);
    pendingScrollTarget_.y = std::max(0.0f, (ch - canvasSize.y) * 0.5f);
    pendingScroll_ = true;
}

void EditorApp::fitHeightView(const ImVec2& canvasSize) {
    if (!level_.valid()) return;
    const float pw = static_cast<float>(level_.pixelWidth());
    const float ph = static_cast<float>(level_.pixelHeight());
    if (pw <= 0 || ph <= 0) return;
    float z = std::min(1.0f, (canvasSize.y - 32.0f) / ph);
    z = std::clamp(z, 0.05f, 1.0f);
    zoom_ = z;
    const float cw = std::max(canvasSize.x, pw * z + 32.0f);
    const float ch = std::max(canvasSize.y, ph * z + 32.0f);
    pendingScrollTarget_.x = 0.0f;
    pendingScrollTarget_.y = std::max(0.0f, (ch - canvasSize.y) * 0.5f);
    pendingScroll_ = true;
    (void)pw;
    (void)cw;
}

void EditorApp::beginStroke() {
    if (!level_.valid()) return;
    strokeBefore_ = level_.tiles;
    strokeActive_ = true;
    strokeChanged_ = false;
}

void EditorApp::endStroke() {
    if (!strokeActive_) return;
    strokeActive_ = false;
    if (!strokeChanged_) return;
    undoStack_.push_back(std::move(strokeBefore_));
    if (undoStack_.size() > 64) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
    dirty_ = true;
}

void EditorApp::paintAt(int tx, int ty) {
    if (!level_.valid() || tx < 0 || ty < 0 || tx >= level_.width || ty >= level_.height) return;
    if (selectedMeta_ < 0 || selectedMeta_ >= static_cast<int>(level_.metas.size())) return;
    u16 value = static_cast<u16>(selectedMeta_ & level_.indexMask);
    if (flipH_) value = static_cast<u16>(value | 0x4000);
    if (flipV_) value = static_cast<u16>(value | 0x8000);
    const std::size_t index = static_cast<std::size_t>(ty) * level_.width + tx;
    if (level_.tiles[index] == value) return;
    level_.tiles[index] = value;
    strokeChanged_ = true;

    // Repaint only the affected 32x32 area and upload it.
    const int pw = level_.pixelWidth();
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const std::size_t o = static_cast<std::size_t>(ty * 32 + y) * pw + tx * 32 + x;
            level_.image[o] = packColor(level_.background);
        }
    }
    const int mi = value & level_.indexMask;
    if (mi >= 0 && mi < static_cast<int>(level_.metas.size())) {
        renderMetatile(level_.image, pw, tx * 32, ty * 32, level_.metas[mi],
                       level_.charset, level_.palette, (value & 0x4000) != 0,
                       (value & 0x8000) != 0);
    }
    levelTex_.uploadRegion(context_, level_.image, pw, tx * 32, ty * 32, 32, 32);
}

void EditorApp::eraseAt(int tx, int ty) {
    if (!level_.valid() || tx < 0 || ty < 0 || tx >= level_.width || ty >= level_.height) return;
    const std::size_t index = static_cast<std::size_t>(ty) * level_.width + tx;
    if (level_.tiles[index] == 0) return;
    level_.tiles[index] = 0;
    strokeChanged_ = true;
    const int pw = level_.pixelWidth();
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const std::size_t o = static_cast<std::size_t>(ty * 32 + y) * pw + tx * 32 + x;
            level_.image[o] = packColor(level_.background);
        }
    }
    levelTex_.uploadRegion(context_, level_.image, pw, tx * 32, ty * 32, 32, 32);
}

void EditorApp::pickAt(int tx, int ty) {
    if (!level_.valid() || tx < 0 || ty < 0 || tx >= level_.width || ty >= level_.height) return;
    const u16 v = level_.tiles[static_cast<std::size_t>(ty) * level_.width + tx];
    selectedMeta_ = v & level_.indexMask;
    flipH_ = (v & 0x4000) != 0;
    flipV_ = (v & 0x8000) != 0;
    if (selectedMeta_ >= static_cast<int>(level_.metas.size())) selectedMeta_ = 0;
}

void EditorApp::undo() {
    if (undoStack_.empty() || !level_.valid()) return;
    redoStack_.push_back(level_.tiles);
    level_.tiles = std::move(undoStack_.back());
    undoStack_.pop_back();
    rebuildLevelImage();
    dirty_ = true;
}

void EditorApp::redo() {
    if (redoStack_.empty() || !level_.valid()) return;
    undoStack_.push_back(level_.tiles);
    level_.tiles = std::move(redoStack_.back());
    redoStack_.pop_back();
    rebuildLevelImage();
    dirty_ = true;
}

void EditorApp::saveRom(bool saveAs) {
    if (!game_ || selectedLevel_ < 0) {
        setStatus("No level is selected.", true);
        return;
    }
    std::string target = rom_.path;
    if (saveAs || target.empty()) {
        target = openFileDialog(true);
        if (target.empty()) return;
    }
    std::string err;
    if (!game_->saveLevel(rom_, level_, err)) {
        setStatus(err.empty() ? "Could not save this level." : err, true);
        return;
    }
    if (!rom_.save(target, err)) {
        setStatus(err.empty() ? "Could not save the ROM." : err, true);
        return;
    }
    dirty_ = false;
    setStatus("Saved " + target + "  (checksum " + hex(rom_.checksum(), 4) + ").");
}

void EditorApp::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open ROM...", "Ctrl+O")) openRequested_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Save ROM", "Ctrl+S", false, game_ && selectedLevel_ >= 0))
            saveRequested_ = true;
        if (ImGui::MenuItem("Save ROM As...", "Ctrl+Shift+S", false,
                            game_ && selectedLevel_ >= 0))
            saveAsRequested_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack_.empty())) undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !redoStack_.empty())) redo();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Tile grid", nullptr, &showGrid_);
        if (ImGui::MenuItem("Fit level to window", "F")) { autoFitFrames_ = 2; fitWhole_ = true; }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About DK Editor")) showAbout_ = true;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void EditorApp::drawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    if (ImGui::Button("Open")) openRequested_ = true;
    ImGui::SameLine();
    ImGui::BeginDisabled(!game_ || selectedLevel_ < 0);
    if (ImGui::Button("Save")) saveRequested_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Save As")) saveAsRequested_ = true;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(undoStack_.empty() || !level_.valid());
    if (ImGui::Button("Undo")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redoStack_.empty() || !level_.valid());
    if (ImGui::Button("Redo")) redo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &showGrid_);
    ImGui::SameLine();
    if (ImGui::Button("Fit")) { autoFitFrames_ = 2; fitWhole_ = true; }
    ImGui::SameLine();
    ImGui::TextDisabled("Zoom %.0f%%", zoom_ * 100.0f);
    ImGui::SameLine();
    if (dirty_) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "* modified");
    } else if (game_) {
        ImGui::TextDisabled("%s  %s", game_->name(), rom_.versionName().c_str());
    }
    ImGui::PopStyleVar();
}

void EditorApp::drawLevelPanel() {
    if (!game_) {
        ImGui::TextWrapped("Open a Donkey Kong Country 1, 2 or 3 ROM to start.");
        ImGui::Spacing();
        if (ImGui::Button("Open ROM...", ImVec2(-1, 0))) openRequested_ = true;
        return;
    }

    ImGui::TextColored(ImVec4(0.55f, 0.8f, 1.0f, 1.0f), "%s", game_->name());
    ImGui::TextDisabled("%s  |  %s", rom_.versionName().c_str(),
                        formatSize(rom_.data.size()).c_str());
    ImGui::TextDisabled("%s", rom_.path.c_str());
    ImGui::Spacing();

    ImGui::SetNextItemWidth(-1);
    char search[128] = {};
    std::strncpy(search, search_.c_str(), sizeof(search) - 1);
    if (ImGui::InputTextWithHint("##search", "Search levels...", search, sizeof(search))) {
        search_ = search;
    }

    ImGui::Spacing();
    ImGui::BeginChild("level-list", ImVec2(0, 0), ImGuiChildFlags_Borders);
    const std::string filter = lower(trim(search_));
    std::vector<const LevelEntry*> ordered;
    ordered.reserve(game_->levels().size());
    for (const LevelEntry& e : game_->levels()) {
        if (!filter.empty()) {
            const std::string hay = lower(e.name + " " + e.group + " " + hex(e.id, 2));
            if (hay.find(filter) == std::string::npos) continue;
        }
        ordered.push_back(&e);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const LevelEntry* a, const LevelEntry* b) {
        if (a->group != b->group) return a->group < b->group;
        return a->id < b->id;
    });
    std::string lastGroup;
    for (const LevelEntry* ep : ordered) {
        const LevelEntry& e = *ep;
        if (e.group != lastGroup) {
            lastGroup = e.group;
            ImGui::Spacing();
            ImGui::TextDisabled("-- %s --", lastGroup.c_str());
        }
        char label[256];
        std::snprintf(label, sizeof label, "%02X  %s", e.id, e.name.c_str());
        if (!e.supported) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
        if (ImGui::Selectable(label, selectedLevel_ == e.id)) {
            loadLevel(e.id);
        }
        if (!e.supported) ImGui::PopStyleColor();
        if (selectedLevel_ == e.id && scrollFrames_ > 0) {
            ImGui::SetScrollHereY(0.5f);
            --scrollFrames_;
        }
        if (!e.supported && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", e.note.c_str());
        }
    }
    ImGui::EndChild();
}

void EditorApp::drawCanvas() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("canvas", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 view = ImGui::GetContentRegionAvail();
    if (view.x < 64) view.x = 64;
    if (view.y < 64) view.y = 64;
    const ImVec2 clip0(windowPos.x + 1.0f, windowPos.y + 1.0f);
    const ImVec2 clip1(clip0.x + view.x, clip0.y + view.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (!level_.valid()) {
        dl->PushClipRect(clip0, clip1, true);
        dl->AddRectFilled(clip0, clip1, IM_COL32(16, 17, 22, 255));
        if (selectedLevel_ >= 0 && !level_.info.name.empty()) {
            const std::string title = level_.info.name;
            const std::string note = level_.info.note.empty()
                ? "This level has no visual tilemap in this build."
                : level_.info.note;
            const ImVec2 t1 = ImGui::CalcTextSize(title.c_str());
            dl->AddText(ImVec2(clip0.x + (view.x - t1.x) * 0.5f, clip0.y + view.y * 0.35f),
                        IM_COL32(255, 210, 90, 255), title.c_str());
            const ImVec2 t2 = ImGui::CalcTextSize(note.c_str(), nullptr, false, view.x - 80);
            dl->AddText(nullptr, 0,
                        ImVec2(clip0.x + (view.x - t2.x) * 0.5f, clip0.y + view.y * 0.35f + 28),
                        IM_COL32(210, 214, 225, 255), note.c_str(), nullptr, view.x - 80);
            const char* hint = "Use the Stage settings panel on the right.";
            const ImVec2 t3 = ImGui::CalcTextSize(hint);
            dl->AddText(ImVec2(clip0.x + (view.x - t3.x) * 0.5f, clip0.y + view.y * 0.35f + 80),
                        IM_COL32(140, 150, 165, 255), hint);
        } else {
            const char* msg = levelError_.empty() ? "Select a level to begin."
                                                  : levelError_.c_str();
            const ImVec2 ts = ImGui::CalcTextSize(msg, nullptr, false, view.x - 40);
            dl->AddText(nullptr, 0,
                        ImVec2(clip0.x + (view.x - ts.x) * 0.5f,
                               clip0.y + (view.y - ts.y) * 0.5f),
                        IM_COL32(220, 220, 230, 255), msg);
        }
        dl->PopClipRect();
        ImGui::EndChild();
        return;
    }

    if (autoFitFrames_ > 0) {
        if (fitWhole_) fitView(view);
        else fitHeightView(view);
        --autoFitFrames_;
    }

    const float margin = 32.0f;
    const float imgW = level_.pixelWidth() * zoom_;
    const float imgH = level_.pixelHeight() * zoom_;
    const ImVec2 content(std::max(view.x, imgW + margin), std::max(view.y, imgH + margin));

    ImGui::InvisibleButton("##canvas-area", content,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonRight |
                           ImGuiButtonFlags_MouseButtonMiddle);
    const bool itemHovered = ImGui::IsItemHovered();
    const bool inside = itemHovered &&
                        io.MousePos.x >= clip0.x && io.MousePos.y >= clip0.y &&
                        io.MousePos.x < clip1.x && io.MousePos.y < clip1.y;

    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 imagePos(itemMin.x + (content.x - imgW) * 0.5f,
                          itemMin.y + (content.y - imgH) * 0.5f);
    const ImVec2 imageEnd(imagePos.x + imgW, imagePos.y + imgH);

    // Zoom around the mouse.  The scroll target keeps the world point under the
    // cursor when the content is larger than the viewport.
    if (inside && io.MouseWheel != 0.0f) {
        autoFitFrames_ = 0;
        const float oldZoom = zoom_;
        float newZoom = zoom_ * std::pow(1.18f, io.MouseWheel);
        newZoom = std::clamp(newZoom, 0.05f, 4.0f);
        if (newZoom != oldZoom) {
            const ImVec2 world((io.MousePos.x - imagePos.x) / oldZoom,
                               (io.MousePos.y - imagePos.y) / oldZoom);
            zoom_ = newZoom;
            pendingScrollTarget_.x = ImGui::GetScrollX() + world.x * (newZoom - oldZoom);
            pendingScrollTarget_.y = ImGui::GetScrollY() + world.y * (newZoom - oldZoom);
            pendingScroll_ = true;
        }
    }

    // Middle-drag or Space+left-drag pans by moving the child scroll.
    const bool space = ImGui::IsKeyDown(ImGuiKey_Space);
    if ((ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
         (space && ImGui::IsMouseDragging(ImGuiMouseButton_Left))) &&
        (inside || ImGui::IsItemActive())) {
        autoFitFrames_ = 0;
        pendingScrollTarget_.x = ImGui::GetScrollX() - io.MouseDelta.x;
        pendingScrollTarget_.y = ImGui::GetScrollY() - io.MouseDelta.y;
        pendingScroll_ = true;
    }

    if (pendingScroll_) {
        ImGui::SetScrollX(pendingScrollTarget_.x);
        ImGui::SetScrollY(pendingScrollTarget_.y);
        pendingScroll_ = false;
    }

    dl->PushClipRect(clip0, clip1, true);
    dl->AddRectFilled(clip0, clip1, IM_COL32(14, 15, 20, 255));
    if (levelTex_.valid()) {
        dl->AddImage(levelTex_.imguiId(), imagePos, imageEnd);
    }

    // Cursor -> tile.
    int tx = -1, ty = -1;
    if (inside && zoom_ > 0.0f) {
        const float px = (io.MousePos.x - imagePos.x) / zoom_;
        const float py = (io.MousePos.y - imagePos.y) / zoom_;
        if (px >= 0 && py >= 0 && px < level_.pixelWidth() && py < level_.pixelHeight()) {
            tx = static_cast<int>(px) / 32;
            ty = static_cast<int>(py) / 32;
        }
    }
    cursorX_ = tx;
    cursorY_ = ty;

    // Grid.
    if (showGrid_ && zoom_ * 32 >= 6.0f) {
        const ImU32 grid = IM_COL32(255, 255, 255, 28);
        for (int x = 0; x <= level_.width; ++x) {
            const float px = imagePos.x + x * 32 * zoom_;
            dl->AddLine(ImVec2(px, imagePos.y), ImVec2(px, imageEnd.y), grid);
        }
        for (int y = 0; y <= level_.height; ++y) {
            const float py = imagePos.y + y * 32 * zoom_;
            dl->AddLine(ImVec2(imagePos.x, py), ImVec2(imageEnd.x, py), grid);
        }
    }

    // Hover rectangle.
    if (tx >= 0 && ty >= 0) {
        const ImVec2 a(imagePos.x + tx * 32 * zoom_, imagePos.y + ty * 32 * zoom_);
        const ImVec2 b(a.x + 32 * zoom_, a.y + 32 * zoom_);
        dl->AddRect(a, b, IM_COL32(255, 210, 90, 220), 0.0f, 0, 2.0f);
    }

    // Painting.
    const bool canEdit = level_.info.visual && !space;
    if (canEdit && inside && tx >= 0 && ty >= 0) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (io.KeyAlt || tool_ == Tool::Picker) {
                pickAt(tx, ty);
            } else {
                beginStroke();
                if (tool_ == Tool::Erase) eraseAt(tx, ty);
                else paintAt(tx, ty);
            }
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            beginStroke();
            eraseAt(tx, ty);
        }
        if (strokeActive_ && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            !io.KeyAlt && tool_ != Tool::Picker) {
            if (tool_ == Tool::Erase) eraseAt(tx, ty);
            else paintAt(tx, ty);
        }
        if (strokeActive_ && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            eraseAt(tx, ty);
        }
    }
    if (strokeActive_ && (!ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                         !inside) &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        endStroke();
    }
    if (strokeActive_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        endStroke();
    }

    dl->PopClipRect();
    ImGui::EndChild();
}

void EditorApp::drawDkc3Properties() {
    if (level_.properties.empty()) return;
    ImGui::TextWrapped("%s", level_.info.note.c_str());
    ImGui::Spacing();
    ImGui::SeparatorText("Stage settings");
    bool changed = false;
    for (LevelProperty& p : level_.properties) {
        int display = p.value + p.displayOffset;
        ImGui::PushID(&p);
        if (p.bit >= 0) {
            bool v = p.value != 0;
            if (ImGui::Checkbox(p.label.c_str(), &v)) {
                p.value = v ? 1 : 0;
                changed = true;
            }
        } else {
            ImGui::SetNextItemWidth(140);
            if (ImGui::InputInt(p.label.c_str(), &display, 1, 16)) {
                display = std::clamp(display, p.minimum + p.displayOffset,
                                     p.maximum + p.displayOffset);
                p.value = display - p.displayOffset;
                changed = true;
            }
        }
        ImGui::PopID();
    }
    if (changed) {
        dirty_ = true;
        setStatus("Stage settings changed. Press Save ROM to apply them.");
    }
}

void EditorApp::drawToolPanel() {
    ImGui::TextColored(ImVec4(0.55f, 0.8f, 1.0f, 1.0f), "Tools");
    ImGui::Separator();

    const bool visual = level_.valid() && level_.info.visual;
    ImGui::BeginDisabled(!visual);
    if (ImGui::RadioButton("Paint", tool_ == Tool::Paint)) tool_ = Tool::Paint;
    ImGui::SameLine();
    if (ImGui::RadioButton("Pick", tool_ == Tool::Picker)) tool_ = Tool::Picker;
    ImGui::SameLine();
    if (ImGui::RadioButton("Erase", tool_ == Tool::Erase)) tool_ = Tool::Erase;

    ImGui::Spacing();
    ImGui::Checkbox("Flip horizontally (X)", &flipH_);
    ImGui::Checkbox("Flip vertically (Y)", &flipV_);

    if (visual) {
        ImGui::Spacing();
        ImGui::Text("Selected tile: %03X", selectedMeta_);
        const int rows = atlasRows();
        if (atlasTex_.valid() && rows > 0) {
            const ImVec2 uv0 = atlasUv(selectedMeta_);
            const ImVec2 uv1 = ImVec2(uv0.x + 32.0f / (atlasColumns() * 32),
                                      uv0.y + 32.0f / (rows * 32));
            ImGui::Image(atlasTex_.imguiId(), ImVec2(96, 96), uv0, uv1);
        }
        ImGui::Spacing();
        ImGui::SeparatorText("Tile palette");

        const float panelHeight = std::max(140.0f, ImGui::GetContentRegionAvail().y - 190.0f);
        ImGui::BeginChild("tile-palette", ImVec2(0, panelHeight), ImGuiChildFlags_Borders);
        if (atlasTex_.valid()) {
            const float atlasW = static_cast<float>(atlasColumns() * 32);
            const float atlasH = static_cast<float>(rows * 32);
            ImGui::Image(atlasTex_.imguiId(), ImVec2(atlasW, atlasH));
            const ImVec2 a = ImGui::GetItemRectMin();
            const ImVec2 b = ImGui::GetItemRectMax();
            if (ImGui::IsItemHovered()) {
                const ImVec2 m = ImGui::GetIO().MousePos;
                const int col = static_cast<int>((m.x - a.x) / 32.0f);
                const int row = static_cast<int>((m.y - a.y) / 32.0f);
                const int idx = row * atlasColumns() + col;
                if (idx >= 0 && idx < static_cast<int>(level_.metas.size())) {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    dl->AddRect(ImVec2(a.x + col * 32, a.y + row * 32),
                                ImVec2(a.x + col * 32 + 32, a.y + row * 32 + 32),
                                IM_COL32(255, 210, 90, 230), 0, 0, 2.0f);
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        selectedMeta_ = idx;
                        tool_ = Tool::Paint;
                    }
                }
            }
            // Selection marker.
            const int selCol = selectedMeta_ % atlasColumns();
            const int selRow = selectedMeta_ / atlasColumns();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRect(ImVec2(a.x + selCol * 32, a.y + selRow * 32),
                        ImVec2(a.x + selCol * 32 + 32, a.y + selRow * 32 + 32),
                        IM_COL32(80, 200, 255, 255), 0, 0, 2.5f);
            if (paletteScrollFrames_ > 0) {
                const float target = selRow * 32.0f - panelHeight * 0.5f + 16.0f;
                ImGui::SetScrollY(std::clamp(target, 0.0f, atlasH));
                --paletteScrollFrames_;
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::SeparatorText("Level");
    if (!level_.info.name.empty()) {
        ImGui::Text("%s", level_.info.name.c_str());
        if (level_.valid()) {
            ImGui::TextDisabled("%d x %d tiles (%d x %d px)", level_.width, level_.height,
                                level_.pixelWidth(), level_.pixelHeight());
            ImGui::TextDisabled("%s  |  %d metatiles", level_.info.group.c_str(),
                                static_cast<int>(level_.metas.size()));
            if (cursorX_ >= 0 && cursorY_ >= 0) {
                const u16 v = level_.tiles[static_cast<std::size_t>(cursorY_) * level_.width + cursorX_];
                ImGui::Text("Cursor: %d,%d  tile %03X", cursorX_, cursorY_, v & level_.indexMask);
            }
        } else {
            ImGui::TextDisabled("%s", level_.info.group.c_str());
        }
    } else if (!levelError_.empty()) {
        ImGui::TextWrapped("%s", levelError_.c_str());
    } else {
        ImGui::TextDisabled("No level loaded.");
    }

    if (!level_.info.visual) {
        ImGui::Spacing();
        ImGui::SeparatorText("DKC3 stage settings");
        drawDkc3Properties();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Game");
    if (game_) {
        ImGui::TextWrapped("%s", game_->summary().c_str());
    }
}

void EditorApp::drawStatusBar() {
    ImGui::Separator();
    const ImVec4 color = statusError_ ? ImVec4(1.0f, 0.55f, 0.45f, 1.0f)
                                      : ImVec4(0.75f, 0.8f, 0.9f, 1.0f);
    ImGui::TextColored(color, "%s", status_.c_str());
    ImGui::SameLine();
    if (level_.valid()) {
        ImGui::TextDisabled("|  %d,%d  |  %s  |  zoom %.0f%%", cursorX_, cursorY_,
                            dirty_ ? "modified" : "saved", zoom_ * 100.0f);
    }
}

void EditorApp::draw() {
    ImGuiIO& io = ImGui::GetIO();

    if (!ImGui::GetIO().WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) openRequested_ = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            if (io.KeyShift) saveAsRequested_ = true;
            else saveRequested_ = true;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (io.KeyShift) redo();
            else undo();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) redo();
        if (ImGui::IsKeyPressed(ImGuiKey_F)) { autoFitFrames_ = 2; fitWhole_ = true; }
    }

    drawMenuBar();

    const ImVec2 display = io.DisplaySize;
    const float menuH = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(ImVec2(0, menuH));
    ImGui::SetNextWindowSize(ImVec2(display.x, display.y - menuH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    drawToolbar();
    const float statusH = ImGui::GetTextLineHeightWithSpacing() + 4;
    const float contentH = ImGui::GetContentRegionAvail().y - statusH;
    const float leftW = 300.0f;
    const float rightW = 340.0f;

    ImGui::BeginChild("left-panel", ImVec2(leftW, contentH), ImGuiChildFlags_Borders);
    drawLevelPanel();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("centre-panel", ImVec2(ImGui::GetContentRegionAvail().x - rightW - 8,
                                             contentH), ImGuiChildFlags_Borders);
    drawCanvas();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("right-panel", ImVec2(0, contentH), ImGuiChildFlags_Borders);
    drawToolPanel();
    ImGui::EndChild();

    drawStatusBar();
    ImGui::End();

    if (showAbout_) {
        ImGui::OpenPopup("About DK Editor");
        showAbout_ = false;
    }
    if (ImGui::BeginPopupModal("About DK Editor", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("DK Editor - Donkey Kong Country 1/2/3 level editor");
        ImGui::Separator();
        ImGui::Text("DKC1: visual tilemap editing, in-place save.");
        ImGui::Text("DKC2: visual tilemap editing, Big Data recompression and relocation.");
        ImGui::Text("DKC3: stage settings editing; terrain editing is planned.");
        ImGui::Spacing();
        ImGui::TextDisabled("Compression library: Kingizor's dkcomp (MIT).");
        ImGui::TextDisabled("UI: Dear ImGui (MIT).");
        ImGui::Spacing();
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (openRequested_) {
        openRequested_ = false;
        openRomDialog();
    }
    if (saveRequested_) {
        saveRequested_ = false;
        saveRom(false);
    }
    if (saveAsRequested_) {
        saveAsRequested_ = false;
        saveRom(true);
    }
}

} // namespace dk

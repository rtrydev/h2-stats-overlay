#include "overlay_renderer.h"

#include "config.h"
#include "rating_logic.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace h2stats::OverlayRenderer {
namespace {

struct Vertex {
    float x;
    float y;
    float z;
    float rhw;
    DWORD color;
};

using SetRenderStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, D3DRENDERSTATETYPE, DWORD);
using GetRenderStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, D3DRENDERSTATETYPE, DWORD*);
using SetTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, DWORD, IDirect3DBaseTexture8*);
using GetTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, DWORD, IDirect3DBaseTexture8**);
using SetTextureStageStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD);
using GetTextureStageStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD*);
using SetVertexShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, DWORD);
using GetVertexShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, DWORD*);
using SetPixelShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, DWORD);
using GetPixelShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, DWORD*);
using SetStreamSourceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, UINT, IDirect3DVertexBuffer8*, UINT);
using GetStreamSourceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, UINT, IDirect3DVertexBuffer8**, UINT*);
using GetViewportFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, D3DVIEWPORT8*);
using DrawPrimitiveUPFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
using ReleaseFn = ULONG(STDMETHODCALLTYPE*)(void*);

constexpr DWORD kGreen = 0xFF00D060;
constexpr DWORD kRed = 0xFFE04040;
constexpr DWORD kWhite = 0xFFE8E8E8;
constexpr DWORD kShadow = 0xCC000000;

template <typename T>
T VTable(IDirect3DDevice8* device, size_t index) {
    void** table = *reinterpret_cast<void***>(device);
    return reinterpret_cast<T>(table[index]);
}

const char* GlyphRows(char glyph, int row) {
    static constexpr const char* k0[] = {
        "01110",
        "10001",
        "10011",
        "10101",
        "11001",
        "10001",
        "01110",
    };
    static constexpr const char* k1[] = {
        "00100",
        "01100",
        "00100",
        "00100",
        "00100",
        "00100",
        "01110",
    };
    static constexpr const char* k2[] = {
        "01110",
        "10001",
        "00001",
        "00010",
        "00100",
        "01000",
        "11111",
    };
    static constexpr const char* k3[] = {
        "11110",
        "00001",
        "00001",
        "01110",
        "00001",
        "00001",
        "11110",
    };
    static constexpr const char* k4[] = {
        "00010",
        "00110",
        "01010",
        "10010",
        "11111",
        "00010",
        "00010",
    };
    static constexpr const char* k5[] = {
        "11111",
        "10000",
        "10000",
        "11110",
        "00001",
        "00001",
        "11110",
    };
    static constexpr const char* k6[] = {
        "01110",
        "10000",
        "10000",
        "11110",
        "10001",
        "10001",
        "01110",
    };
    static constexpr const char* k7[] = {
        "11111",
        "00001",
        "00010",
        "00100",
        "01000",
        "01000",
        "01000",
    };
    static constexpr const char* k8[] = {
        "01110",
        "10001",
        "10001",
        "01110",
        "10001",
        "10001",
        "01110",
    };
    static constexpr const char* k9[] = {
        "01110",
        "10001",
        "10001",
        "01111",
        "00001",
        "00001",
        "01110",
    };
    static constexpr const char* kS[] = {
        "01111",
        "10000",
        "10000",
        "01110",
        "00001",
        "00001",
        "11110",
    };
    static constexpr const char* kA[] = {
        "01110",
        "10001",
        "10001",
        "11111",
        "10001",
        "10001",
        "10001",
    };
    static constexpr const char* kC[] = {
        "01111",
        "10000",
        "10000",
        "10000",
        "10000",
        "10000",
        "01111",
    };
    static constexpr const char* kD[] = {
        "11110",
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "11110",
    };
    static constexpr const char* kE[] = {
        "11111",
        "10000",
        "10000",
        "11110",
        "10000",
        "10000",
        "11111",
    };
    static constexpr const char* kF[] = {
        "11111",
        "10000",
        "10000",
        "11110",
        "10000",
        "10000",
        "10000",
    };
    static constexpr const char* kG[] = {
        "01111",
        "10000",
        "10000",
        "10011",
        "10001",
        "10001",
        "01111",
    };
    static constexpr const char* kH[] = {
        "10001",
        "10001",
        "10001",
        "11111",
        "10001",
        "10001",
        "10001",
    };
    static constexpr const char* kI[] = {
        "11111",
        "00100",
        "00100",
        "00100",
        "00100",
        "00100",
        "11111",
    };
    static constexpr const char* kK[] = {
        "10001",
        "10010",
        "10100",
        "11000",
        "10100",
        "10010",
        "10001",
    };
    static constexpr const char* kL[] = {
        "10000",
        "10000",
        "10000",
        "10000",
        "10000",
        "10000",
        "11111",
    };
    static constexpr const char* kM[] = {
        "10001",
        "11011",
        "10101",
        "10101",
        "10001",
        "10001",
        "10001",
    };
    static constexpr const char* kN[] = {
        "10001",
        "11001",
        "10101",
        "10011",
        "10001",
        "10001",
        "10001",
    };
    static constexpr const char* kO[] = {
        "01110",
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "01110",
    };
    static constexpr const char* kR[] = {
        "11110",
        "10001",
        "10001",
        "11110",
        "10100",
        "10010",
        "10001",
    };
    static constexpr const char* kT[] = {
        "11111",
        "00100",
        "00100",
        "00100",
        "00100",
        "00100",
        "00100",
    };
    static constexpr const char* kU[] = {
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "01110",
    };
    static constexpr const char* kZ[] = {
        "11111",
        "00001",
        "00010",
        "00100",
        "01000",
        "10000",
        "11111",
    };
    static constexpr const char* kColon[] = {
        "00000",
        "00100",
        "00100",
        "00000",
        "00100",
        "00100",
        "00000",
    };
    static constexpr const char* kDot[] = {
        "00000",
        "00000",
        "00000",
        "00000",
        "00000",
        "01100",
        "01100",
    };

    switch (glyph) {
    case '0':
        return k0[row];
    case '1':
        return k1[row];
    case '2':
        return k2[row];
    case '3':
        return k3[row];
    case '4':
        return k4[row];
    case '5':
        return k5[row];
    case '6':
        return k6[row];
    case '7':
        return k7[row];
    case '8':
        return k8[row];
    case '9':
        return k9[row];
    case 'S':
        return kS[row];
    case 'A':
        return kA[row];
    case 'C':
        return kC[row];
    case 'D':
        return kD[row];
    case 'E':
        return kE[row];
    case 'F':
        return kF[row];
    case 'G':
        return kG[row];
    case 'H':
        return kH[row];
    case 'I':
        return kI[row];
    case 'K':
        return kK[row];
    case 'L':
        return kL[row];
    case 'M':
        return kM[row];
    case 'N':
        return kN[row];
    case 'O':
        return kO[row];
    case 'R':
        return kR[row];
    case 'T':
        return kT[row];
    case 'U':
        return kU[row];
    case 'Z':
        return kZ[row];
    case ':':
        return kColon[row];
    case '.':
        return kDot[row];
    default:
        return "00000";
    }
}

void AddRectangle(std::vector<Vertex>& vertices, float x, float y, float w, float h, DWORD color) {
    const float z = 0.0f;
    const float rhw = 1.0f;
    vertices.push_back({x, y, z, rhw, color});
    vertices.push_back({x + w, y, z, rhw, color});
    vertices.push_back({x + w, y + h, z, rhw, color});
    vertices.push_back({x, y, z, rhw, color});
    vertices.push_back({x + w, y + h, z, rhw, color});
    vertices.push_back({x, y + h, z, rhw, color});
}

void AddGlyph(std::vector<Vertex>& vertices, char glyph, float x, float y, float cell, DWORD color) {
    for (int row = 0; row < 7; ++row) {
        const char* rowData = GlyphRows(glyph, row);
        // Merge each horizontal run of lit cells into one rectangle. This draws
        // the exact same pixels with far fewer triangles than a quad per cell.
        int col = 0;
        while (col < 5) {
            if (rowData[col] != '1') {
                ++col;
                continue;
            }
            const int runStart = col;
            while (col < 5 && rowData[col] == '1') {
                ++col;
            }
            const int runLength = col - runStart;
            AddRectangle(vertices,
                         x + static_cast<float>(runStart) * cell,
                         y + static_cast<float>(row) * cell,
                         static_cast<float>(runLength) * cell,
                         cell,
                         color);
        }
    }
}

float AddText(std::vector<Vertex>& vertices, const char* text, float x, float y, float scale, DWORD color) {
    const float cell = std::max(1.0f, 2.0f * scale);
    const float glyphAdvance = 6.0f * cell;
    float cursorX = x;
    for (const char* ch = text; *ch != '\0'; ++ch) {
        if (*ch != ' ') {
            AddGlyph(vertices, *ch, cursorX, y, cell, color);
        }
        cursorX += glyphAdvance;
    }
    return cursorX - x;
}

// Exactly the device state the overlay touches. This used to be a
// CreateStateBlock(D3DSBT_ALL) capture/apply/delete every frame, but under
// wined3d that snapshots and replays the ENTIRE device state (hundreds of
// states, transforms, lights) each frame — measurable main-thread cost on
// CrossOver. Saving and restoring only what ApplyOverlayState changes (plus
// stream 0, which DrawPrimitiveUP invalidates) draws the same pixels for a
// fraction of the work.
constexpr D3DRENDERSTATETYPE kSavedRenderStates[] = {
    D3DRS_LIGHTING,
    D3DRS_ZENABLE,
    D3DRS_ZWRITEENABLE,
    D3DRS_CULLMODE,
    D3DRS_ALPHABLENDENABLE,
    D3DRS_SRCBLEND,
    D3DRS_DESTBLEND,
};
constexpr size_t kSavedRenderStateCount = sizeof(kSavedRenderStates) / sizeof(kSavedRenderStates[0]);

constexpr D3DTEXTURESTAGESTATETYPE kSavedStageStates[] = {
    D3DTSS_COLOROP,
    D3DTSS_COLORARG1,
    D3DTSS_ALPHAOP,
    D3DTSS_ALPHAARG1,
};
constexpr size_t kSavedStageStateCount = sizeof(kSavedStageStates) / sizeof(kSavedStageStates[0]);

struct SavedState {
    DWORD renderStates[kSavedRenderStateCount];
    DWORD stageStates[kSavedStageStateCount];
    IDirect3DBaseTexture8* texture;    // AddRef'd by GetTexture
    IDirect3DVertexBuffer8* stream0;   // AddRef'd by GetStreamSource
    UINT stream0Stride;
    DWORD vertexShader;
    DWORD pixelShader;
};

void ReleaseInterface(void* object) {
    if (object != nullptr) {
        void** table = *reinterpret_cast<void***>(object);
        reinterpret_cast<ReleaseFn>(table[2])(object);
    }
}

void SaveDeviceState(IDirect3DDevice8* device, SavedState& saved) {
    const auto getRenderState = VTable<GetRenderStateFn>(device, 51);
    const auto getTexture = VTable<GetTextureFn>(device, 60);
    const auto getTextureStageState = VTable<GetTextureStageStateFn>(device, 62);
    const auto getVertexShader = VTable<GetVertexShaderFn>(device, 77);
    const auto getPixelShader = VTable<GetPixelShaderFn>(device, 89);
    const auto getStreamSource = VTable<GetStreamSourceFn>(device, 84);

    for (size_t index = 0; index < kSavedRenderStateCount; ++index) {
        getRenderState(device, kSavedRenderStates[index], &saved.renderStates[index]);
    }
    for (size_t index = 0; index < kSavedStageStateCount; ++index) {
        getTextureStageState(device, 0, kSavedStageStates[index], &saved.stageStates[index]);
    }
    getTexture(device, 0, &saved.texture);
    getStreamSource(device, 0, &saved.stream0, &saved.stream0Stride);
    getVertexShader(device, &saved.vertexShader);
    getPixelShader(device, &saved.pixelShader);
}

void ApplyOverlayState(IDirect3DDevice8* device) {
    const auto setRenderState = VTable<SetRenderStateFn>(device, 50);
    const auto setTexture = VTable<SetTextureFn>(device, 61);
    const auto setTextureStageState = VTable<SetTextureStageStateFn>(device, 63);
    const auto setVertexShader = VTable<SetVertexShaderFn>(device, 76);
    const auto setPixelShader = VTable<SetPixelShaderFn>(device, 88);

    setRenderState(device, D3DRS_LIGHTING, FALSE);
    setRenderState(device, D3DRS_ZENABLE, D3DZB_FALSE);
    setRenderState(device, D3DRS_ZWRITEENABLE, FALSE);
    setRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE);
    setRenderState(device, D3DRS_ALPHABLENDENABLE, TRUE);
    setRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    setRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    setTexture(device, 0, nullptr);
    setTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    setTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    setTextureStageState(device, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    setTextureStageState(device, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    setPixelShader(device, 0);
    setVertexShader(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
}

void RestoreDeviceState(IDirect3DDevice8* device, SavedState& saved) {
    const auto setRenderState = VTable<SetRenderStateFn>(device, 50);
    const auto setTexture = VTable<SetTextureFn>(device, 61);
    const auto setTextureStageState = VTable<SetTextureStageStateFn>(device, 63);
    const auto setVertexShader = VTable<SetVertexShaderFn>(device, 76);
    const auto setPixelShader = VTable<SetPixelShaderFn>(device, 88);
    const auto setStreamSource = VTable<SetStreamSourceFn>(device, 83);

    for (size_t index = 0; index < kSavedRenderStateCount; ++index) {
        setRenderState(device, kSavedRenderStates[index], saved.renderStates[index]);
    }
    for (size_t index = 0; index < kSavedStageStateCount; ++index) {
        setTextureStageState(device, 0, kSavedStageStates[index], saved.stageStates[index]);
    }
    setTexture(device, 0, saved.texture);
    setStreamSource(device, 0, saved.stream0, saved.stream0Stride);
    setVertexShader(device, saved.vertexShader);
    setPixelShader(device, saved.pixelShader);

    // The Get* calls handed back AddRef'd interfaces.
    ReleaseInterface(saved.texture);
    ReleaseInterface(saved.stream0);
    saved.texture = nullptr;
    saved.stream0 = nullptr;
}

void DrawVertices(IDirect3DDevice8* device, const std::vector<Vertex>& vertices) {
    if (vertices.empty()) {
        return;
    }

    const auto drawPrimitiveUP = VTable<DrawPrimitiveUPFn>(device, 72);
    drawPrimitiveUP(device,
                    D3DPT_TRIANGLELIST,
                    static_cast<UINT>(vertices.size() / 3),
                    vertices.data(),
                    sizeof(Vertex));
}

void AddOutlinedText(std::vector<Vertex>& vertices, const char* text, float x, float y, float scale, DWORD color) {
    AddText(vertices, text, x + 1.0f, y + 1.0f, scale, kShadow);
    AddText(vertices, text, x, y, scale, color);
}

void FormatTimer(int ticks, char* buffer, size_t bufferSize) {
    if (ticks < 0) {
        ticks = 0;
    }

    const int minutes = ticks / 3600;
    const int seconds = (ticks / 60) % 60;
    const int milliseconds = ((ticks % 60) * 1000) / 60;
    snprintf(buffer, bufferSize, "TIME %02d:%02d.%03d", minutes, seconds, milliseconds);
}

void AddMetricLine(std::vector<Vertex>& vertices,
                   float x,
                   float y,
                   float scale,
                   const char* label,
                   int value) {
    char line[32] = {};
    snprintf(line, sizeof(line), "%s %d", label, value);
    AddOutlinedText(vertices, line, x, y, scale, kWhite);
}

} // namespace

void Render(IDirect3DDevice8* device) {
    if (device == nullptr) {
        return;
    }

    static DWORD lastConfigCheckTick = 0;
    static DWORD lastStatsReadTick = 0;
    static StatsSnapshot snapshot;

    const DWORD now = GetTickCount();
    if (now - lastConfigCheckTick > 1000) {
        lastConfigCheckTick = now;
        Config::ReloadIfChanged();
    }

    OverlayConfig config = Config::Get();
    if (!config.enabled) {
        return;
    }

    if (now - lastStatsReadTick > 50) {
        lastStatsReadTick = now;
        StatsReader::ReadSnapshot(snapshot);
    }

    if (!snapshot.missionStarted && !config.showInMenus) {
        return;
    }

    const bool silentAssassin = snapshot.missionStarted ? RatingLogic::IsSilentAssassin(snapshot.counters, snapshot.strictCloseEncounter) : true;
    const bool allZeros = snapshot.missionStarted ? RatingLogic::IsAllZeros(snapshot.counters) : true;

    D3DVIEWPORT8 viewport = {};
    const auto getViewport = VTable<GetViewportFn>(device, 41);
    if (FAILED(getViewport(device, &viewport))) {
        return;
    }

    const float x = static_cast<float>(viewport.X + config.offsetX);
    const float y = static_cast<float>(viewport.Y + config.offsetY);
    const float scale = config.scale;
    const float lineSpacing = static_cast<float>(config.lineSpacing);

    // Reused across frames so the vertex storage is not reallocated every frame.
    static std::vector<Vertex> vertices;
    vertices.clear();

    // "SA" and "AZ" share a row; offset AZ by the width of "SA" plus half a glyph advance.
    const float indicatorCell = std::max(1.0f, 2.0f * scale);
    AddOutlinedText(vertices, "SA", x, y, scale, silentAssassin ? kGreen : kRed);
    AddOutlinedText(vertices, "AZ", x + 2.5f * 6.0f * indicatorCell, y, scale, allZeros ? kGreen : kRed);

    float detailsY = y + lineSpacing;
    const float detailsScale = config.verboseScale;
    const float detailsLineSpacing = static_cast<float>(config.verboseLineSpacing);

    if (config.showTimer && snapshot.missionStarted) {
        char timerText[32] = {};
        FormatTimer(snapshot.missionTime, timerText, sizeof(timerText));
        AddOutlinedText(vertices, timerText, x, detailsY, detailsScale, kWhite);
        detailsY += detailsLineSpacing;
    }

    if (config.verbose && snapshot.missionStarted) {
        AddMetricLine(vertices, x, detailsY, detailsScale, "SHOTS FIRED", snapshot.counters.shotsFired);
        detailsY += detailsLineSpacing;
        AddMetricLine(vertices, x, detailsY, detailsScale, "CLOSE ENCOUNTERS", snapshot.counters.closeEncounters);
        detailsY += detailsLineSpacing;
        AddMetricLine(vertices, x, detailsY, detailsScale, "HEADSHOTS", snapshot.counters.headshots);
        detailsY += detailsLineSpacing;
        AddMetricLine(vertices, x, detailsY, detailsScale, "ALERTS", snapshot.counters.alerts);
        detailsY += detailsLineSpacing;
        AddMetricLine(vertices, x, detailsY, detailsScale, "ENEMIES KILLED", snapshot.counters.enemiesKilled);
        detailsY += detailsLineSpacing;
        AddMetricLine(vertices, x, detailsY, detailsScale, "ENEMIES HARMED", snapshot.counters.enemiesHarmed);
        detailsY += detailsLineSpacing;
        AddMetricLine(vertices, x, detailsY, detailsScale, "INNOCENTS KILLED", snapshot.counters.innocentsKilled);
        detailsY += detailsLineSpacing;
        AddMetricLine(vertices, x, detailsY, detailsScale, "INNOCENTS HARMED", snapshot.counters.innocentsHarmed);
    }

    SavedState saved = {};
    SaveDeviceState(device, saved);
    ApplyOverlayState(device);
    DrawVertices(device, vertices);
    RestoreDeviceState(device, saved);
}

} // namespace h2stats::OverlayRenderer

#include "syntax_highlighter.hpp"
#include <sstream>

namespace graphics {

static std::wstring s2ws(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], len);
    return wstr;
}

void SyntaxHighlighter::parse_inline_markdown_spans(
    const std::string& line,
    std::wstring& outCleanWStr,
    std::vector<DWRITE_TEXT_RANGE>& outCodeRanges,
    std::vector<DWRITE_TEXT_RANGE>& outBoldRanges
) {
    outCleanWStr.clear();
    outCodeRanges.clear();
    outBoldRanges.clear();
    size_t i = 0;
    while (i < line.size()) {
        if (line[i] == '`') {
            size_t endTick = line.find('`', i + 1);
            if (endTick != std::string::npos) {
                std::wstring wTok = L" " + s2ws(line.substr(i + 1, endTick - i - 1)) + L" ";
                DWRITE_TEXT_RANGE r = { (UINT32)outCleanWStr.size(), (UINT32)wTok.size() };
                outCodeRanges.push_back(r);
                outCleanWStr += wTok;
                i = endTick + 1;
                continue;
            }
        }
        if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
            size_t endBold = line.find("**", i + 2);
            if (endBold != std::string::npos) {
                std::wstring wTok = s2ws(line.substr(i + 2, endBold - i - 2));
                DWRITE_TEXT_RANGE r = { (UINT32)outCleanWStr.size(), (UINT32)wTok.size() };
                outBoldRanges.push_back(r);
                outCleanWStr += wTok;
                i = endBold + 2;
                continue;
            }
        }
        size_t nextSpec = line.find_first_of("`*", i);
        if (nextSpec == i) {
            outCleanWStr += s2ws(line.substr(i, 1));
            i++;
        } else if (nextSpec == std::string::npos) {
            outCleanWStr += s2ws(line.substr(i));
            break;
        } else {
            outCleanWStr += s2ws(line.substr(i, nextSpec - i));
            i = nextSpec;
        }
    }
}

float SyntaxHighlighter::measure_rich_text_line(D2DRenderer& r, const std::string& line, float maxW) {
    if (line.empty()) return 10.0f;
    std::wstring cleanWStr;
    std::vector<DWRITE_TEXT_RANGE> codeRanges;
    std::vector<DWRITE_TEXT_RANGE> boldRanges;
    parse_inline_markdown_spans(line, cleanWStr, codeRanges, boldRanges);

    IDWriteFactory* dwrite = r.get_dwrite_factory();
    if (!dwrite || cleanWStr.empty()) {
        int wrapped = max(1, (int)((cleanWStr.size() * 7.2f) / maxW) + 1);
        return (wrapped * 22.0f) + 4.0f;
    }

    IDWriteTextFormat* fmt = nullptr;
    HRESULT hr = dwrite->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 10.0f * (96.0f / 72.0f), L"vi-vn", &fmt
    );
    if (FAILED(hr) || !fmt) return 24.0f;

    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    fmt->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 21.5f, 21.5f * 0.8f);

    IDWriteTextLayout* layout = nullptr;
    float h = 24.0f;
    if (SUCCEEDED(dwrite->CreateTextLayout(cleanWStr.c_str(), (UINT32)cleanWStr.size(), fmt, maxW, 4000.0f, &layout)) && layout) {
        for (const auto& rng : boldRanges) layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, rng);
        for (const auto& rng : codeRanges) {
            layout->SetFontFamilyName(L"Consolas", rng);
            layout->SetFontSize(9.2f * (96.0f / 72.0f), rng);
        }
        DWRITE_TEXT_METRICS tm;
        if (SUCCEEDED(layout->GetMetrics(&tm))) {
            h = max(21.5f, tm.height + 3.0f);
        }
        layout->Release();
    }
    fmt->Release();
    return h;
}

void SyntaxHighlighter::draw_rich_text_line(
    D2DRenderer& r,
    const std::string& line,
    float x, float y, float maxW,
    const D2DColor& textCol,
    const D2DColor& codeCol,
    const D2DColor& codeBg
) {
    if (line.empty()) return;
    std::wstring cleanWStr;
    std::vector<DWRITE_TEXT_RANGE> codeRanges;
    std::vector<DWRITE_TEXT_RANGE> boldRanges;
    parse_inline_markdown_spans(line, cleanWStr, codeRanges, boldRanges);

    IDWriteFactory* dwrite = r.get_dwrite_factory();
    ID2D1DeviceContext* ctx = r.get_context();
    if (!dwrite || !ctx || cleanWStr.empty()) return;

    IDWriteTextFormat* fmt = nullptr;
    HRESULT hr = dwrite->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 10.0f * (96.0f / 72.0f), L"vi-vn", &fmt
    );
    if (FAILED(hr) || !fmt) return;

    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    fmt->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 21.5f, 21.5f * 0.8f);

    IDWriteTextLayout* layout = nullptr;
    if (SUCCEEDED(dwrite->CreateTextLayout(cleanWStr.c_str(), (UINT32)cleanWStr.size(), fmt, maxW, 4000.0f, &layout)) && layout) {
        for (const auto& rng : boldRanges) layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, rng);
        for (const auto& rng : codeRanges) {
            layout->SetFontFamilyName(L"Consolas", rng);
            layout->SetFontSize(9.2f * (96.0f / 72.0f), rng);

            // Draw inline code background highlight box
            DWRITE_HIT_TEST_METRICS htm[16];
            UINT32 actualCount = 0;
            if (SUCCEEDED(layout->HitTestTextRange(rng.startPosition, rng.length, 0.0f, 0.0f, htm, 16, &actualCount))) {
                for (UINT32 m = 0; m < actualCount; ++m) {
                    r.draw_rounded_box(
                        x + htm[m].left, y + htm[m].top - 0.5f,
                        htm[m].width, htm[m].height - 1.0f,
                        3.0f, codeBg, D2DColor(68, 71, 90), 0.8f
                    );
                }
            }
        }

        // Draw rich text layout
        ID2D1SolidColorBrush* textBrush = nullptr;
        if (SUCCEEDED(ctx->CreateSolidColorBrush(textCol.to_d2d(), &textBrush))) {
            ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush, static_cast<D2D1_DRAW_TEXT_OPTIONS>(0x00000004));
            textBrush->Release();
        }
        layout->Release();
    }
    fmt->Release();
}

void SyntaxHighlighter::draw_code_highlighted(
    D2DRenderer& r,
    const std::string& lang,
    const std::string& code,
    float x, float y, float w, float h,
    float viewBottomY
) {
    D2DColor defaultCol(212, 212, 216); // #d4d4d8
    D2DColor commentCol(113, 113, 122); // #71717a
    D2DColor kwCol(248, 113, 113);      // Coral Red #f87171
    D2DColor strCol(134, 239, 172);     // Mint Green #86efac
    D2DColor propCol(125, 211, 252);    // Sky Cyan #7dd3fc
    D2DColor valCol(196, 181, 253);     // Lavender #c4b5fd

    std::stringstream ss(code);
    std::string line;
    float curY = y;

    auto draw_code_token = [&](const std::wstring& wTok, float& xPos, const D2DColor& col) {
        if (wTok.empty()) return;
        D2D1_SIZE_F sz = r.measure_text(wTok, 2000.0f, L"Consolas", 9.5f, false, false);
        float adv = (sz.width > 0.0f) ? sz.width : (wTok.size() * 7.4f);
        r.draw_text(wTok, xPos, curY, adv + 20.0f, 20.0f, L"Consolas", 9.5f, false, col, 0, 0, false);
        xPos += adv;
    };

    while (std::getline(ss, line)) {
        if (curY + 20.0f < 40.0f || curY > viewBottomY) {
            curY += 19.0f;
            continue;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        float curX = x;

        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));

        if (trimmed.rfind("//", 0) == 0 || (trimmed.rfind("#", 0) == 0 && trimmed.rfind("#include", 0) != 0 && trimmed.rfind("#pragma", 0) != 0 && trimmed.rfind("#define", 0) != 0)) {
            r.draw_text(s2ws(line), curX, curY, w, 20.0f, L"Consolas", 9.5f, false, commentCol, 0, 0, false);
        } else if (trimmed.rfind("#", 0) == 0) {
            r.draw_text(s2ws(line), curX, curY, w, 20.0f, L"Consolas", 9.5f, false, kwCol, 0, 0, false);
        } else if (line.find('<') != std::string::npos && line.find('>') != std::string::npos) {
            size_t idx = 0;
            while (idx < line.size()) {
                if (line[idx] == '<') {
                    size_t closePos = line.find('>', idx);
                    if (closePos != std::string::npos) {
                        draw_code_token(s2ws(line.substr(idx, closePos - idx + 1)), curX, kwCol);
                        idx = closePos + 1;
                        continue;
                    }
                }
                size_t nextTag = line.find('<', idx);
                std::string plain = (nextTag == std::string::npos) ? line.substr(idx) : line.substr(idx, nextTag - idx);
                draw_code_token(s2ws(plain), curX, defaultCol);
                if (nextTag == std::string::npos) break;
                idx = nextTag;
            }
        } else if (line.find(':') != std::string::npos && line.find("::") == std::string::npos && line.find("http") == std::string::npos) {
            size_t colonPos = line.find(':');
            draw_code_token(s2ws(line.substr(0, colonPos + 1)), curX, propCol);
            draw_code_token(s2ws(line.substr(colonPos + 1)), curX, valCol);
        } else if (line.find('"') != std::string::npos) {
            size_t idx = 0;
            while (idx < line.size()) {
                size_t q1 = line.find('"', idx);
                if (q1 == std::string::npos) {
                    draw_code_token(s2ws(line.substr(idx)), curX, defaultCol);
                    break;
                }
                if (q1 > idx) {
                    draw_code_token(s2ws(line.substr(idx, q1 - idx)), curX, defaultCol);
                }
                size_t q2 = line.find('"', q1 + 1);
                if (q2 == std::string::npos) {
                    draw_code_token(s2ws(line.substr(q1)), curX, strCol);
                    break;
                }
                draw_code_token(s2ws(line.substr(q1, q2 - q1 + 1)), curX, strCol);
                idx = q2 + 1;
            }
        } else {
            r.draw_text(s2ws(line), curX, curY, w, 20.0f, L"Consolas", 9.5f, false, defaultCol, 0, 0, false);
        }
        curY += 19.0f;
    }
}

} // namespace graphics

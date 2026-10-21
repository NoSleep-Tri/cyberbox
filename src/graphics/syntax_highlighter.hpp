#pragma once
#ifndef CYBERBOX_SYNTAX_HIGHLIGHTER_HPP
#define CYBERBOX_SYNTAX_HIGHLIGHTER_HPP

#include "d2d_renderer.hpp"
#include <string>
#include <vector>

namespace graphics {

class SyntaxHighlighter {
public:
    static void parse_inline_markdown_spans(
        const std::string& line,
        std::wstring& outCleanWStr,
        std::vector<DWRITE_TEXT_RANGE>& outCodeRanges,
        std::vector<DWRITE_TEXT_RANGE>& outBoldRanges
    );

    static float measure_rich_text_line(D2DRenderer& r, const std::string& line, float maxW);

    static void draw_rich_text_line(
        D2DRenderer& r,
        const std::string& line,
        float x, float y, float maxW,
        const D2DColor& textCol = D2DColor(241, 245, 249),
        const D2DColor& codeCol = D2DColor(248, 113, 113),
        const D2DColor& codeBg = D2DColor(30, 32, 40)
    );

    static void draw_code_highlighted(
        D2DRenderer& r,
        const std::string& lang,
        const std::string& code,
        float x, float y, float w, float h,
        float viewBottomY
    );
};

} // namespace graphics

#endif // CYBERBOX_SYNTAX_HIGHLIGHTER_HPP

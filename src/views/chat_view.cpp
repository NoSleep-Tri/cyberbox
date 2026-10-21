#include "chat_view.hpp"
#include "../graphics/syntax_highlighter.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>

namespace views {

static std::wstring s2ws(const std::string& raw) {
    if (raw.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), NULL, 0);
    std::wstring w(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), &w[0], size);
    return w;
}

static std::vector<gui::TextPart> parse_markdown_blocks(const std::string& text) {
    std::vector<gui::TextPart> parts;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t code_start = text.find("```", pos);
        if (code_start == std::string::npos) {
            std::string rem = text.substr(pos);
            if (!rem.empty()) parts.push_back({ false, "", rem });
            break;
        }
        if (code_start > pos) {
            parts.push_back({ false, "", text.substr(pos, code_start - pos) });
        }
        size_t lang_start = code_start + 3;
        size_t lang_end = text.find('\n', lang_start);
        std::string lang = "code";
        size_t content_start = lang_start;
        if (lang_end != std::string::npos && lang_end - lang_start < 20) {
            lang = text.substr(lang_start, lang_end - lang_start);
            lang.erase(0, lang.find_first_not_of(" \r\n\t"));
            lang.erase(lang.find_last_not_of(" \r\n\t") + 1);
            if (lang.empty()) lang = "code";
            content_start = lang_end + 1;
        }
        size_t code_end = text.find("```", content_start);
        if (code_end == std::string::npos) {
            parts.push_back({ true, lang, text.substr(content_start) });
            break;
        } else {
            std::string c = text.substr(content_start, code_end - content_start);
            while (!c.empty() && (c.back() == '\n' || c.back() == '\r')) c.pop_back();
            parts.push_back({ true, lang, c });
            pos = code_end + 3;
            while (pos < text.size() && (text[pos] == '\n' || text[pos] == '\r')) pos++;
        }
    }
    return parts;
}

void ChatView::render(
    graphics::D2DRenderer& r,
    const RECT& rc,
    float rightPanelW,
    std::vector<gui::GuiMessage>& messages,
    std::mutex& chatMutex,
    bool isGenerating,
    int scrollY,
    int animFrame,
    float pulsePhase,
    int copiedBlockId,
    int downloadedBlockId,
    bool hasInputText,
    int& outContentHeight
) {
    int chat_left = 48;
    int right_panel_w = (int)rightPanelW;
    int chat_right = rc.right - right_panel_w;
    int chat_w = chat_right - chat_left;

    code_hitboxes_.clear();
    reasoning_hitboxes_.clear();
    int blockCounter = 0;

    r.set_clip((float)chat_left, 46.0f, (float)chat_w, (float)(rc.bottom - 46 - 72));

    float colW = (float)min(chat_w - 60, 620);
    float colX = (float)chat_left + ((float)chat_w - colW) / 2.0f;

    int startY = 54 - scrollY;
    int curY = startY;

    std::lock_guard<std::mutex> lock(chatMutex);

    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
        bool isUser = (msg.sender == "user");

        if (isUser) {
            std::wstring wText = s2ws(msg.text);
            if (std::abs(msg.cached_col_w - colW) > 0.5f || msg.cached_text_len != msg.text.size()) {
                D2D1_SIZE_F textSz = r.measure_text(wText, colW * 0.72f, L"Segoe UI", 10.0f, false, true);
                msg.cached_user_w = min(colW * 0.75f, max(160.0f, textSz.width + 34.0f));
                msg.cached_user_h = max(36.0f, textSz.height + 18.0f);
                msg.cached_col_w = colW;
                msg.cached_text_len = msg.text.size();
            }
            float bubbleW = msg.cached_user_w;
            float bubbleH = msg.cached_user_h;
            float extraAttachH = msg.attachment_title.empty() ? 0.0f : 44.0f;
            float totalUserH = bubbleH + extraAttachH;
            float rightEdge = colX + colW;

            if (curY + totalUserH >= 40 && curY <= rc.bottom - 60) {
                float curBoxY = (float)curY;

                // Attachment preview pill
                if (!msg.attachment_title.empty()) {
                    float attW = 198.0f;
                    float attH = 38.0f;
                    r.draw_rounded_box(rightEdge - attW, curBoxY, attW, attH, 18.0f,
                                       graphics::D2DColor(224, 206, 250),
                                       graphics::D2DColor(255, 255, 255), 1.5f);

                    r.draw_text(s2ws("🍴 " + msg.attachment_title),
                                rightEdge - attW, curBoxY + 4.0f, attW, 16.0f,
                                L"Segoe UI", 7.5f, true, graphics::D2DColor(49, 46, 129), 1, 1, false);

                    r.draw_text(s2ws(msg.attachment_sub),
                                rightEdge - attW + 8.0f, curBoxY + 20.0f, attW - 16.0f, 14.0f,
                                L"Segoe UI", 5.5f, false, graphics::D2DColor(107, 114, 128), 1, 1, false);

                    curBoxY += 44.0f;
                }

                // Deep wine red user pill bubble
                r.draw_rounded_box(rightEdge - bubbleW, curBoxY, bubbleW, bubbleH, 18.0f,
                                   graphics::D2DColor(92, 29, 36),
                                   graphics::D2DColor(120, 38, 48), 1.0f);

                r.draw_text(wText, rightEdge - bubbleW + 16.0f, curBoxY + 9.0f, bubbleW - 32.0f, bubbleH - 12.0f,
                            L"Segoe UI", 10.0f, false, graphics::D2DColor(248, 249, 250), 0, 0, true);
            }

            curY += (int)totalUserH + 16;
        } else {
            // Muse bubble card
            float bubbleW = colW - 36.0f;
            float innerW = bubbleW - 32.0f;

            bool showCursor = msg.is_streaming && msg.executed_cmds.empty() && ((animFrame % 18) < 12);

            bool cacheDirty = (std::abs(msg.cached_col_w - colW) > 0.5f) ||
                              (msg.cached_text_len != msg.text.size()) ||
                              (msg.cached_reasoning_len != msg.reasoning.size()) ||
                              (msg.cached_cmds_count != msg.executed_cmds.size()) ||
                              (msg.cached_was_streaming != msg.is_streaming) ||
                              (msg.cached_reasoning_collapsed != msg.reasoning_collapsed) ||
                              (msg.cached_card_h <= 0.0f);

            if (cacheDirty) {
                msg.cached_parts = parse_markdown_blocks(msg.text);

                float rBoxH = 0.0f;
                if (!msg.reasoning.empty()) {
                    if (msg.reasoning_collapsed) {
                        rBoxH = 32.0f;
                    } else {
                        std::wstring wR = s2ws(msg.reasoning + (msg.is_streaming && msg.text.empty() ? " ▋" : ""));
                        D2D1_SIZE_F rSz = r.measure_text(wR, innerW - 24.0f, L"Segoe UI", 9.0f, false, true);
                        rBoxH = 30.0f + max(18.0f, rSz.height) + 16.0f;
                    }
                }
                msg.cached_reasoning_h = rBoxH;

                float cH = 28.0f;
                if (rBoxH > 0.0f) {
                    cH += rBoxH + 12.0f;
                }

                if (msg.is_streaming && msg.text.empty() && msg.reasoning.empty()) {
                    cH = 52.0f;
                } else {
                    for (const auto& p : msg.cached_parts) {
                        if (p.is_code) {
                            int codeLines = (int)std::count(p.text.begin(), p.text.end(), '\n') + 1;
                            cH += 34.0f + (codeLines * 19.0f) + 16.0f;
                        } else {
                            std::stringstream lss(p.text);
                            std::string ln;
                            while (std::getline(lss, ln)) {
                                if (!ln.empty() && ln.back() == '\r') ln.pop_back();
                                if (ln.empty()) {
                                    cH += 10.0f;
                                } else {
                                    cH += graphics::SyntaxHighlighter::measure_rich_text_line(r, ln, innerW) + 2.0f;
                                }
                            }
                            cH += 6.0f;
                        }
                    }
                }

                for (const auto& cmd : msg.executed_cmds) {
                    std::wstring wCmd = s2ws("⚡ Sandbox Executed: " + cmd);
                    D2D1_SIZE_F cmdSz = r.measure_text(wCmd, innerW - 20.0f, L"Consolas", 8.5f, false, true);
                    float badgeH = max(26.0f, cmdSz.height + 12.0f);
                    cH += badgeH + 8.0f;
                }

                msg.cached_card_h = cH;
                msg.cached_col_w = colW;
                msg.cached_text_len = msg.text.size();
                msg.cached_reasoning_len = msg.reasoning.size();
                msg.cached_cmds_count = msg.executed_cmds.size();
                msg.cached_was_streaming = msg.is_streaming;
                msg.cached_reasoning_collapsed = msg.reasoning_collapsed;
            }

            float cardH = msg.cached_card_h;
            float reasoningBoxH = msg.cached_reasoning_h;

            if (curY + cardH >= 40 && curY <= rc.bottom - 60) {
                // Card border color with gentle pulse if streaming
                graphics::D2DColor cardBorderCol(38, 40, 47);
                float cardBorderW = 1.0f;
                if (msg.is_streaming) {
                    float pulse = (std::sin(pulsePhase) + 1.0f) * 0.5f;
                    int rCol = (int)(70 + pulse * 69);
                    int gCol = (int)(45 + pulse * 47);
                    int bCol = (int)(110 + pulse * 136);
                    cardBorderCol = graphics::D2DColor(rCol, gCol, bCol);
                    cardBorderW = 1.4f;
                }

                r.draw_rounded_box(colX, (float)curY, bubbleW, cardH, 14.0f,
                                   graphics::D2DColor(28, 29, 34), cardBorderCol, cardBorderW);

                float partY = (float)curY + 14.0f;
                float partX = colX + 16.0f;

                // 1. Thinking wave
                if (msg.is_streaming && msg.text.empty() && msg.reasoning.empty()) {
                    for (int d = 0; d < 3; ++d) {
                        float wave = std::sin(pulsePhase * 1.4f - d * 0.85f);
                        float dy = partY + 7.0f - (wave * 3.5f);
                        float alpha = (150.0f + 105.0f * ((wave + 1.0f) * 0.5f)) / 255.0f;
                        r.fill_ellipse(partX + (d * 13.0f) + 3.75f, dy + 3.75f, 3.75f, 3.75f,
                                       graphics::D2DColor(168, 85, 247, alpha));
                    }
                    r.draw_text(L"Muse đang suy nghĩ...", partX + 46.0f, partY + 1.0f, 220.0f, 22.0f,
                                L"Segoe UI", 9.5f, false, graphics::D2DColor(168, 172, 186), 0, 1, false);
                    partY += 28.0f;
                }

                // 2. Reasoning thought stream
                if (reasoningBoxH > 0.0f) {
                    r.draw_rounded_box(partX, partY, innerW, reasoningBoxH, 8.0f,
                                       graphics::D2DColor(23, 24, 30), graphics::D2DColor(68, 52, 108), 1.0f);

                    reasoning_hitboxes_.push_back({ Gdiplus::RectF(partX, partY, innerW, 30.0f), i });

                    std::wstring headerTitle = (msg.is_streaming && msg.text.empty()) ?
                        L"💭 Đang suy luận trực tiếp..." :
                        (msg.reasoning_collapsed ? L"💭 Đã hoàn thành suy luận" : L"💭 Luồng suy luận");

                    r.draw_text(headerTitle, partX + 12.0f, partY + 6.0f, innerW - 100.0f, 18.0f,
                                L"Segoe UI", 8.5f, true, graphics::D2DColor(192, 132, 252), 0, 0, false);

                    r.draw_text(msg.reasoning_collapsed ? L"Mở rộng ▼" : L"Thu gọn ▲",
                                partX + innerW - 80.0f, partY + 6.0f, 68.0f, 18.0f,
                                L"Segoe UI", 8.0f, false, graphics::D2DColor(168, 85, 247), 2, 0, false);

                    if (!msg.reasoning_collapsed) {
                        std::string rDisp = msg.reasoning;
                        if (msg.is_streaming && msg.text.empty() && showCursor) rDisp += " ▋";
                        std::wstring wReasoning = s2ws(rDisp);

                        r.draw_text(wReasoning, partX + 12.0f, partY + 28.0f, innerW - 24.0f, reasoningBoxH - 36.0f,
                                    L"Segoe UI", 9.0f, false, graphics::D2DColor(156, 163, 175), 0, 0, true);
                    }
                    partY += reasoningBoxH + 12.0f;
                }

                // 3. Parts (Markdown & Code)
                for (size_t pIdx = 0; pIdx < msg.cached_parts.size(); ++pIdx) {
                    const auto& p = msg.cached_parts[pIdx];
                    if (p.is_code) {
                        int thisBlockId = blockCounter++;
                        int codeLines = (int)std::count(p.text.begin(), p.text.end(), '\n') + 1;
                        float blockH = 32.0f + (codeLines * 19.0f) + 8.0f;

                        if (partY + blockH >= 40.0f && partY <= rc.bottom - 55.0f) {
                            r.draw_rounded_box(partX, partY, innerW, blockH, 8.0f,
                                               graphics::D2DColor(21, 22, 26), graphics::D2DColor(42, 44, 52), 1.0f);

                            // Header bar
                            r.draw_rounded_box(partX, partY, innerW, 28.0f, 8.0f,
                                               graphics::D2DColor(36, 37, 44), graphics::D2DColor(0, 0, 0, 0.0f), 0.0f);

                            r.draw_text(s2ws(p.lang), partX + 12.0f, partY, 120.0f, 28.0f,
                                        L"Consolas", 8.5f, false, graphics::D2DColor(156, 163, 175), 0, 1, false);

                            float dlX = partX + innerW - 52.0f;
                            float cpX = partX + innerW - 26.0f;
                            float icY = partY + 7.0f;
                            graphics::D2DColor icCol(156, 163, 175);

                            // Download icon
                            r.draw_line(dlX + 6.0f, icY + 1.0f, dlX + 6.0f, icY + 9.0f, icCol, 1.3f);
                            r.draw_line(dlX + 3.0f, icY + 6.0f, dlX + 6.0f, icY + 9.0f, icCol, 1.3f);
                            r.draw_line(dlX + 9.0f, icY + 6.0f, dlX + 6.0f, icY + 9.0f, icCol, 1.3f);
                            r.draw_line(dlX + 2.0f, icY + 12.0f, dlX + 10.0f, icY + 12.0f, icCol, 1.3f);

                            // Copy icon
                            r.draw_rounded_box(cpX + 3.0f, icY + 3.0f, 8.0f, 9.0f, 1.5f,
                                               graphics::D2DColor(0, 0, 0, 0.0f), icCol, 1.3f);
                            r.draw_line(cpX + 1.0f, icY + 1.0f, cpX + 8.0f, icY + 1.0f, icCol, 1.3f);
                            r.draw_line(cpX + 1.0f, icY + 1.0f, cpX + 1.0f, icY + 9.0f, icCol, 1.3f);

                            if (copiedBlockId == thisBlockId) {
                                r.draw_text(L"✓ Đã chép", dlX - 72.0f, partY, 68.0f, 28.0f,
                                            L"Segoe UI", 8.5f, false, graphics::D2DColor(74, 222, 128), 0, 1, false);
                            } else if (downloadedBlockId == thisBlockId) {
                                r.draw_text(L"✓ Đã lưu", dlX - 66.0f, partY, 62.0f, 28.0f,
                                            L"Segoe UI", 8.5f, false, graphics::D2DColor(74, 222, 128), 0, 1, false);
                            }

                            code_hitboxes_.push_back({
                                Gdiplus::RectF(cpX - 4.0f, partY + 2.0f, 22.0f, 24.0f),
                                Gdiplus::RectF(dlX - 4.0f, partY + 2.0f, 22.0f, 24.0f),
                                p.lang, p.text, thisBlockId
                            });

                            graphics::SyntaxHighlighter::draw_code_highlighted(
                                r, p.lang, p.text,
                                partX + 14.0f, partY + 36.0f, innerW - 28.0f, blockH - 40.0f,
                                (float)(rc.bottom - 55)
                            );
                        }

                        partY += blockH + 12.0f;
                    } else {
                        std::vector<std::string> lines;
                        {
                            std::stringstream lss(p.text);
                            std::string ln;
                            while (std::getline(lss, ln)) {
                                if (!ln.empty() && ln.back() == '\r') ln.pop_back();
                                lines.push_back(ln);
                            }
                        }

                        int lastNonEmptyLine = -1;
                        for (int li = (int)lines.size() - 1; li >= 0; --li) {
                            if (!lines[li].empty()) {
                                lastNonEmptyLine = li;
                                break;
                            }
                        }

                        bool isLastPart = (pIdx + 1 == msg.cached_parts.size());
                        for (size_t li = 0; li < lines.size(); ++li) {
                            const auto& ln = lines[li];
                            if (ln.empty()) {
                                partY += 10.0f;
                                continue;
                            }
                            std::string drawLn = ln;
                            if (isLastPart && (int)li == lastNonEmptyLine && msg.is_streaming && showCursor) {
                                drawLn += " ▋";
                            }
                            float lineH = graphics::SyntaxHighlighter::measure_rich_text_line(r, drawLn, innerW) + 2.0f;
                            if (partY + lineH >= 40.0f && partY <= rc.bottom - 55.0f) {
                                graphics::SyntaxHighlighter::draw_rich_text_line(r, drawLn, partX, partY, innerW);
                            }
                            partY += lineH;
                        }
                        partY += 4.0f;
                    }
                }

                // 4. Executed sandbox commands pills
                for (const auto& cmd : msg.executed_cmds) {
                    std::wstring wCmd = s2ws("⚡ Sandbox Executed: " + cmd);
                    D2D1_SIZE_F cmdSz = r.measure_text(wCmd, innerW - 20.0f, L"Consolas", 8.5f, false, true);
                    float badgeH = max(26.0f, cmdSz.height + 12.0f);

                    if (partY + badgeH >= 40.0f && partY <= rc.bottom - 55.0f) {
                        r.draw_rounded_box(partX, partY, innerW, badgeH, 6.0f,
                                           graphics::D2DColor(6, 44, 34), graphics::D2DColor(16, 185, 129), 1.0f);
                        r.draw_text(wCmd, partX + 10.0f, partY + 6.0f, innerW - 20.0f, badgeH - 12.0f,
                                    L"Consolas", 8.5f, false, graphics::D2DColor(52, 211, 153), 0, 0, true);
                    }
                    partY += badgeH + 8.0f;
                }
            } else {
                for (const auto& p : msg.cached_parts) {
                    if (p.is_code) blockCounter++;
                }
            }

            curY += (int)cardH + 16;
        }
    }

    outContentHeight = curY - startY;

    r.clear_clip();

    // Floating Muse Avatar at top-center when right panel is collapsed
    if (right_panel_w < 30) {
        float flX = (float)chat_left + ((float)chat_w - 48.0f) / 2.0f;
        float flY = 8.0f;
        floating_muse_rect_ = Gdiplus::RectF(flX - 2.0f, flY, 52.0f, 62.0f);

        if (isGenerating) {
            float pulse = (std::sin(pulsePhase) + 1.0f) * 0.5f;
            float rPad = 2.0f + pulse * 4.0f;
            float cx = flX + 24.0f;
            float cy = flY + 24.0f;
            r.draw_ellipse(cx, cy, 24.0f + rPad, 24.0f + rPad,
                           graphics::D2DColor(168, 85, 247, (180.0f - pulse * 120.0f) / 255.0f), 2.0f);
        }

        // Draw avatar circle with 'M'
        r.fill_ellipse(flX + 24.0f, flY + 24.0f, 24.0f, 24.0f, graphics::D2DColor(139, 92, 246));
        r.draw_text(L"M", flX, flY, 48.0f, 48.0f, L"Segoe UI", 16.0f, true, graphics::D2DColor(255, 255, 255), 1, 1, false);

        r.draw_rounded_box(flX - 2.0f, flY + 42.0f, 52.0f, 20.0f, 10.0f,
                           hover_floating_muse_ ? graphics::D2DColor(52, 54, 64) : graphics::D2DColor(36, 38, 45),
                           graphics::D2DColor(58, 60, 70), 1.0f);
        r.draw_text(L"Muse", flX - 2.0f, flY + 42.0f, 52.0f, 20.0f,
                    L"Segoe UI", 8.5f, true, graphics::D2DColor(240, 242, 248), 1, 1, false);
    } else {
        floating_muse_rect_ = Gdiplus::RectF(0, 0, 0, 0);
    }

    // Bottom Input Dock Capsule
    float capsuleW = (float)min(chat_w - 80, 620);
    float capsuleX = (float)chat_left + ((float)chat_w - capsuleW) / 2.0f;
    float capsuleY = (float)(rc.bottom - 62);
    r.draw_rounded_box(capsuleX, capsuleY, capsuleW, 44.0f, 22.0f,
                       graphics::D2DColor(37, 38, 43), graphics::D2DColor(52, 53, 60), 1.0f);

    // Vector '+' inside left of capsule
    btn_plus_rect_ = Gdiplus::RectF(capsuleX + 6.0f, capsuleY + 6.0f, 32.0f, 32.0f);
    float plusCx = capsuleX + 22.0f;
    float plusCy = capsuleY + 22.0f;
    graphics::D2DColor plusCol = hover_btn_plus_ ? graphics::D2DColor(240, 242, 248) : graphics::D2DColor(160, 164, 176);
    r.draw_line(plusCx - 5.5f, plusCy, plusCx + 5.5f, plusCy, plusCol, 1.8f);
    r.draw_line(plusCx, plusCy - 5.5f, plusCx, plusCy + 5.5f, plusCol, 1.8f);

    // Right side: Stop button or Send button
    btn_action_rect_ = Gdiplus::RectF(capsuleX + capsuleW - 36.0f, capsuleY + 8.0f, 28.0f, 28.0f);
    if (isGenerating) {
        float pulse = (std::sin(pulsePhase) + 1.0f) * 0.5f;
        int rRed = (int)(185 + pulse * 45);
        r.draw_rounded_box(btn_action_rect_.X, btn_action_rect_.Y, 28.0f, 28.0f, 14.0f,
                           graphics::D2DColor(rRed, 38, 56), graphics::D2DColor(248, 113, 113), 1.2f);
        r.fill_rect(btn_action_rect_.X + 9.5f, btn_action_rect_.Y + 9.5f, 9.0f, 9.0f,
                    graphics::D2DColor(255, 255, 255));
    } else {
        if (hasInputText || hover_btn_action_) {
            r.draw_rounded_box(btn_action_rect_.X, btn_action_rect_.Y, 28.0f, 28.0f, 14.0f,
                               graphics::D2DColor(124, 58, 237), graphics::D2DColor(0, 0, 0, 0.0f), 0.0f);
            graphics::D2DColor arrCol(255, 255, 255);
            float scx = btn_action_rect_.X + 14.0f;
            float scy = btn_action_rect_.Y + 14.0f;
            r.draw_line(scx, scy + 5.0f, scx, scy - 5.0f, arrCol, 1.8f);
            r.draw_line(scx - 4.0f, scy - 1.0f, scx, scy - 5.0f, arrCol, 1.8f);
            r.draw_line(scx + 4.0f, scy - 1.0f, scx, scy - 5.0f, arrCol, 1.8f);
        }
    }
}

bool ChatView::handle_mouse_move(int x, int y) {
    bool oldPlus = hover_btn_plus_;
    bool oldAct = hover_btn_action_;
    bool oldMuse = hover_floating_muse_;

    hover_btn_plus_ = (x >= btn_plus_rect_.X && x <= btn_plus_rect_.X + btn_plus_rect_.Width &&
                       y >= btn_plus_rect_.Y && y <= btn_plus_rect_.Y + btn_plus_rect_.Height);

    hover_btn_action_ = (x >= btn_action_rect_.X && x <= btn_action_rect_.X + btn_action_rect_.Width &&
                         y >= btn_action_rect_.Y && y <= btn_action_rect_.Y + btn_action_rect_.Height);

    hover_floating_muse_ = (floating_muse_rect_.Width > 0 &&
                            x >= floating_muse_rect_.X && x <= floating_muse_rect_.X + floating_muse_rect_.Width &&
                            y >= floating_muse_rect_.Y && y <= floating_muse_rect_.Y + floating_muse_rect_.Height);

    return (oldPlus != hover_btn_plus_ || oldAct != hover_btn_action_ || oldMuse != hover_floating_muse_);
}

ChatClickResult ChatView::handle_lbutton_down(int x, int y, bool isGenerating) {
    ChatClickResult res;

    // Check code hitboxes
    for (const auto& hb : code_hitboxes_) {
        if (x >= hb.copy_rect.X && x <= hb.copy_rect.X + hb.copy_rect.Width &&
            y >= hb.copy_rect.Y && y <= hb.copy_rect.Y + hb.copy_rect.Height) {
            res.action = ChatClickAction::COPY_CODE;
            res.block_id = hb.block_id;
            res.code = hb.code;
            res.lang = hb.lang;
            return res;
        }
        if (x >= hb.download_rect.X && x <= hb.download_rect.X + hb.download_rect.Width &&
            y >= hb.download_rect.Y && y <= hb.download_rect.Y + hb.download_rect.Height) {
            res.action = ChatClickAction::DOWNLOAD_CODE;
            res.block_id = hb.block_id;
            res.code = hb.code;
            res.lang = hb.lang;
            return res;
        }
    }

    // Check reasoning hitboxes
    for (const auto& rh : reasoning_hitboxes_) {
        if (x >= rh.rect.X && x <= rh.rect.X + rh.rect.Width &&
            y >= rh.rect.Y && y <= rh.rect.Y + rh.rect.Height) {
            res.action = ChatClickAction::TOGGLE_REASONING;
            res.msg_idx = rh.msg_idx;
            return res;
        }
    }

    // Plus button
    if (x >= btn_plus_rect_.X && x <= btn_plus_rect_.X + btn_plus_rect_.Width &&
        y >= btn_plus_rect_.Y && y <= btn_plus_rect_.Y + btn_plus_rect_.Height) {
        res.action = ChatClickAction::PLUS_CLICKED;
        return res;
    }

    // Action button (Stop or Send)
    if (x >= btn_action_rect_.X && x <= btn_action_rect_.X + btn_action_rect_.Width &&
        y >= btn_action_rect_.Y && y <= btn_action_rect_.Y + btn_action_rect_.Height) {
        res.action = isGenerating ? ChatClickAction::STOP_CLICKED : ChatClickAction::SEND_CLICKED;
        return res;
    }

    // Floating Muse avatar
    if (floating_muse_rect_.Width > 0 &&
        x >= floating_muse_rect_.X && x <= floating_muse_rect_.X + floating_muse_rect_.Width &&
        y >= floating_muse_rect_.Y && y <= floating_muse_rect_.Y + floating_muse_rect_.Height) {
        res.action = ChatClickAction::FLOATING_MUSE_CLICKED;
        return res;
    }

    return res;
}

} // namespace views

/*
 * style.cpp - the studio looks like the game's own menu (src/ui/ui_style.cpp):
 * the game's 8x8 pixel font drawn pixel for pixel, square frames, one game
 * pixel borders, space-blue windows, grey panel buttons, red checkmarks.
 */
#include "style.h"

#include <cstring>

#include "imgui.h"
#include "screen_font.h" /* font_bmp: the game's glyphs from 0x20, 10 bytes each */

namespace studio
{

ImVec4 rgb444(int c, float a)
{
	return ImVec4(((c >> 8) & 15) / 15.0f, ((c >> 4) & 15) / 15.0f, (c & 15) / 15.0f, a);
}

/* The game puts its own symbols on some ASCII codes ($ = credits sign,
 * & = o-umlaut, @ = copyright...); the studio needs the plain ones. */
struct Fix
{
	char ch;
	unsigned char rows[8];
	unsigned char adv;
};
static const Fix FIXES[] = {
	{'\'', {0x80, 0x80, 0, 0, 0, 0, 0, 0}, 2},
	{'&', {0x40, 0xA0, 0x40, 0xA8, 0x90, 0x68, 0, 0}, 6},
	{'<', {0, 0x20, 0x40, 0x80, 0x40, 0x20, 0, 0}, 4},
	{'>', {0, 0x80, 0x40, 0x20, 0x40, 0x80, 0, 0}, 4},
	{'@', {0x70, 0x88, 0xB8, 0xB8, 0x80, 0x78, 0, 0}, 6},
	{'[', {0xC0, 0x80, 0x80, 0x80, 0x80, 0x80, 0xC0, 0}, 3},
	{']', {0xC0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xC0, 0}, 3},
	{'^', {0x40, 0xA0, 0, 0, 0, 0, 0, 0}, 4},
	{'`', {0x80, 0x40, 0, 0, 0, 0, 0, 0}, 3},
	{'{', {0x60, 0x40, 0x40, 0x80, 0x40, 0x40, 0x60, 0}, 4},
	{'|', {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0}, 2},
	{'}', {0xC0, 0x40, 0x40, 0x20, 0x40, 0x40, 0xC0, 0}, 4},
	{'~', {0, 0, 0x68, 0xB0, 0, 0, 0, 0}, 6},
	{'$', {0x20, 0x78, 0xA0, 0x70, 0x28, 0xF0, 0x20, 0}, 6},
	{'_', {0, 0, 0, 0, 0, 0, 0xF8, 0}, 6},
};

static void glyph(int ch, const unsigned char *&rows, int &adv)
{
	for (auto &f : FIXES)
		if (f.ch == ch)
		{
			rows = f.rows;
			adv = f.adv;
			return;
		}
	int n = (int)(sizeof(font_bmp) / 10);
	int i = ch - 0x20;
	if (i < 0 || i >= n)
	{
		rows = nullptr;
		adv = 0;
		return;
	}
	rows = font_bmp + i * 10;
	adv = font_bmp[i * 10 + 9];
}

void build_font(int px)
{
	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Clear();
	ImFontConfig cfg;
	cfg.SizePixels = 10.0f * px; /* the game's line is 10 pixels */
	ImFont *font = io.Fonts->AddFontDefault(&cfg);
	int ids[128];
	for (int ch = 0; ch < 128; ch++)
	{
		ids[ch] = -1;
		const unsigned char *rows;
		int adv;
		if (ch < 0x20 || ch > 0x7e)
			continue;
		glyph(ch, rows, adv);
		if (!rows)
			continue;
		ids[ch] = io.Fonts->AddCustomRectFontGlyph(font, (ImWchar)ch, 8 * px, 8 * px, (float)(adv * px),
												   ImVec2(0, (float)px));
	}
	io.Fonts->Build();
	unsigned char *pixels;
	int tw, th;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &tw, &th);
	unsigned int *tex = (unsigned int *)pixels;
	for (int ch = 0; ch < 128; ch++)
	{
		if (ids[ch] < 0)
			continue;
		const ImFontAtlasCustomRect *r = io.Fonts->GetCustomRectByIndex(ids[ch]);
		const unsigned char *rows;
		int adv;
		glyph(ch, rows, adv);
		for (int y = 0; y < r->Height; y++)
		{
			unsigned int *row = tex + (r->Y + y) * tw + r->X;
			unsigned char bits = rows[y / px];
			for (int x = 0; x < r->Width; x++)
				row[x] = (bits & (0x80 >> (x / px))) ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 0);
		}
	}
}

void build_style(int px)
{
	ImGuiStyle &s = ImGui::GetStyle();
	s = ImGuiStyle();
	const float p = (float)px;
	s.WindowRounding = s.ChildRounding = s.FrameRounding = s.PopupRounding = 0;
	s.ScrollbarRounding = s.GrabRounding = s.TabRounding = 0;
	s.WindowBorderSize = p;
	s.FrameBorderSize = p;
	s.PopupBorderSize = p;
	s.ChildBorderSize = p;
	s.WindowPadding = ImVec2(4 * p, 4 * p);
	s.FramePadding = ImVec2(3 * p, 2 * p);
	s.ItemSpacing = ImVec2(2 * p, 2 * p);
	s.ItemInnerSpacing = ImVec2(2 * p, 2 * p);
	s.ScrollbarSize = 5 * p;
	s.GrabMinSize = 5 * p;
	s.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	s.AntiAliasedLines = s.AntiAliasedFill = false; /* crisp pixels */

	ImVec4 *c = s.Colors;
	c[ImGuiCol_Text] = rgb444(COL_TEXT);
	c[ImGuiCol_TextDisabled] = rgb444(COL_PANEL_LIGHT);
	c[ImGuiCol_WindowBg] = rgb444(COL_SPACE, 0.94f);
	c[ImGuiCol_ChildBg] = rgb444(COL_SPACE, 0.0f);
	c[ImGuiCol_PopupBg] = rgb444(COL_SPACE, 0.98f);
	c[ImGuiCol_MenuBarBg] = rgb444(COL_PANEL_DARK);
	c[ImGuiCol_Border] = rgb444(COL_PANEL_LIGHT);
	c[ImGuiCol_BorderShadow] = rgb444(0x000, 0.0f);
	c[ImGuiCol_FrameBg] = rgb444(0x113);
	c[ImGuiCol_FrameBgHovered] = rgb444(0x225);
	c[ImGuiCol_FrameBgActive] = rgb444(0x336);
	c[ImGuiCol_TitleBg] = rgb444(COL_PANEL_DARK);
	c[ImGuiCol_TitleBgActive] = rgb444(COL_PANEL_DARK);
	c[ImGuiCol_TitleBgCollapsed] = rgb444(COL_PANEL_DARK);
	c[ImGuiCol_ScrollbarBg] = rgb444(0x113);
	c[ImGuiCol_ScrollbarGrab] = rgb444(COL_PANEL);
	c[ImGuiCol_ScrollbarGrabHovered] = rgb444(COL_PANEL_LIGHT);
	c[ImGuiCol_ScrollbarGrabActive] = rgb444(COL_HIGHLIGHT);
	c[ImGuiCol_CheckMark] = rgb444(COL_RED);
	c[ImGuiCol_SliderGrab] = rgb444(COL_RED);
	c[ImGuiCol_SliderGrabActive] = rgb444(0xf44);
	c[ImGuiCol_Button] = rgb444(COL_PANEL);
	c[ImGuiCol_ButtonHovered] = rgb444(COL_PANEL_LIGHT);
	c[ImGuiCol_ButtonActive] = rgb444(COL_PANEL_DARK);
	c[ImGuiCol_Header] = rgb444(COL_PANEL);
	c[ImGuiCol_HeaderHovered] = rgb444(COL_PANEL_LIGHT);
	c[ImGuiCol_HeaderActive] = rgb444(COL_PANEL_DARK);
	c[ImGuiCol_Separator] = rgb444(COL_ORANGE);
	c[ImGuiCol_SeparatorHovered] = rgb444(COL_ORANGE);
	c[ImGuiCol_SeparatorActive] = rgb444(COL_ORANGE);
	c[ImGuiCol_Tab] = rgb444(COL_PANEL_DARK);
	c[ImGuiCol_TabHovered] = rgb444(COL_PANEL_LIGHT);
	c[ImGuiCol_TabSelected] = rgb444(COL_PANEL);
	c[ImGuiCol_TabSelectedOverline] = rgb444(COL_ORANGE);
	c[ImGuiCol_ResizeGrip] = rgb444(COL_PANEL);
	c[ImGuiCol_ResizeGripHovered] = rgb444(COL_PANEL_LIGHT);
	c[ImGuiCol_ResizeGripActive] = rgb444(COL_HIGHLIGHT);
	c[ImGuiCol_TextSelectedBg] = rgb444(0x448);
	c[ImGuiCol_NavCursor] = rgb444(COL_ORANGE);
	c[ImGuiCol_ModalWindowDimBg] = rgb444(0x000, 0.6f);
}

void heading(const char *text)
{
	ImGui::PushStyleColor(ImGuiCol_Text, rgb444(COL_ORANGE));
	ImGui::TextWrapped("%s", text);
	ImGui::PopStyleColor();
}

void note(const char *text)
{
	ImGui::PushStyleColor(ImGuiCol_Text, rgb444(COL_PANEL_LIGHT));
	ImGui::TextWrapped("%s", text);
	ImGui::PopStyleColor();
}

bool button(const char *label, float width)
{
	/* grey face with a one pixel bevel, like the game's panel buttons */
	const float p = (float)PX;
	ImVec2 size(width > 0 ? width : ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
	ImVec2 a = ImGui::GetCursorScreenPos();
	ImVec2 b(a.x + size.x, a.y + size.y);
	bool pressed = ImGui::InvisibleButton(label, size);
	bool held = ImGui::IsItemActive(), hover = ImGui::IsItemHovered();
	ImDrawList *dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(a, b, ImGui::GetColorU32(rgb444(hover && !held ? COL_PANEL_LIGHT : COL_PANEL)));
	ImU32 light = ImGui::GetColorU32(rgb444(COL_HIGHLIGHT)), dark = ImGui::GetColorU32(rgb444(0x222));
	ImU32 tl = held ? dark : light, br = held ? light : dark;
	dl->AddRectFilled(a, ImVec2(b.x, a.y + p), tl);
	dl->AddRectFilled(a, ImVec2(a.x + p, b.y), tl);
	dl->AddRectFilled(ImVec2(a.x, b.y - p), b, br);
	dl->AddRectFilled(ImVec2(b.x - p, a.y), b, br);
	const char *end = strstr(label, "##");
	ImVec2 ts = ImGui::CalcTextSize(label, end);
	float off = held ? p : 0;
	ImVec2 tp((float)(int)(a.x + (size.x - ts.x) * 0.5f + off), (float)(int)(a.y + (size.y - ts.y) * 0.5f + off));
	dl->AddText(ImVec2(tp.x + p, tp.y + p), IM_COL32(0, 0, 0, 255), label, end);
	dl->AddText(tp, IM_COL32_WHITE, label, end);
	return pressed;
}

} // namespace studio

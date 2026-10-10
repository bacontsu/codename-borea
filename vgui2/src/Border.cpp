#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "Border.h"
#include "KeyValues.h"
#include "VPanel.h"

namespace vgui2
{
Border::Border()
{
	_inset[0] = 0;
	_inset[1] = 0;
	_inset[2] = 0;
	_inset[3] = 0;
	_name = NULL;

	memset(_sides, 0, sizeof(_sides)); // memset(this + 6, 0, 0x20u) in Win32 pseudo-code - ScriptedSnark
}

Border::~Border()
{
	delete[] _name;

	for (int i = 0; i < 4; i++)
	{
		delete[] _sides[i].lines;
	}
}

void Border::Paint(VPANEL panel)
{
	int wide, tall;
	VHandleToPanel(panel)->GetSize(wide, tall);
	Paint(0, 0, wide, tall, -1, 0, 0);
}

void Border::Paint(int x0, int y0, int x1, int y1)
{
	Paint(x0, y0, x1, y1, -1, 0, 0);
}

void Border::Paint(int x0, int y0, int x1, int y1, int breakSide, int breakStart, int breakStop)
{
	{
		auto& side = _sides[0];

		int left = x0;

		for (int i = 0; i < side.count; ++i, ++left)
		{
			auto& line = side.lines[i];

			vgui2::surface()->DrawSetColor(line.col);

			if (breakSide)
			{
				vgui2::surface()->DrawFilledRect(
					left,
					line.startOffset + y0,
					left + 1,
					y1 - line.endOffset);
			}
			else
			{
				if (breakStart > 0)
				{
					vgui2::surface()->DrawFilledRect(
						left,
						line.startOffset + y0,
						left + 1,
						breakStart + y0);
				}

				if ((y1 - line.endOffset) > breakStop)
				{
					vgui2::surface()->DrawFilledRect(
						left,
						y0 + breakStop + 1,
						left + 1,
						y1 - line.endOffset);
				}
			}
		}
	}

	{
		auto& side = _sides[1];

		int top = y0;

		for (int i = 0; i < side.count; ++i, ++top)
		{
			auto& line = side.lines[i];

			vgui2::surface()->DrawSetColor(line.col);

			if (breakSide == 1)
			{
				if (breakStart > 0)
				{
					vgui2::surface()->DrawFilledRect(
						line.startOffset + x0,
						top,
						breakStart + 1,
						top + 1);
				}

				if ((x1 - line.endOffset) > breakStop)
				{
					vgui2::surface()->DrawFilledRect(
						x0 + breakStop + 1,
						top,
						x1 - line.endOffset,
						top + 1);
				}
			}
			else
			{
				vgui2::surface()->DrawFilledRect(
					line.startOffset + x0,
					top,
					x1 - line.endOffset,
					top + 1);
			}
		}
	}

	{
		auto& side = _sides[2];

		int right = x1 - 1;

		for (int i = 0; i < side.count; ++i, --right)
		{
			auto& line = side.lines[i];

			vgui2::surface()->DrawSetColor(line.col);

			vgui2::surface()->DrawFilledRect(
				right,
				line.startOffset + y0,
				right + 1,
				y1 - line.endOffset);
		}
	}

	{
		auto& side = _sides[3];

		int bottom = y1 - 1;

		for (int i = 0; i < side.count; ++i, --bottom)
		{
			auto& line = side.lines[i];

			vgui2::surface()->DrawSetColor(line.col);

			vgui2::surface()->DrawFilledRect(
				line.startOffset + x0,
				bottom,
				x1 - line.endOffset,
				bottom + 1);
		}
	}
}

void Border::SetInset(int left, int top, int right, int bottom)
{
	_inset[SIDE_LEFT] = left;
	_inset[SIDE_TOP] = top;
	_inset[SIDE_RIGHT] = right;
	_inset[SIDE_BOTTOM] = bottom;
}

void Border::GetInset(int& left, int& top, int& right, int& bottom)
{
	left = _inset[SIDE_LEFT];
	top = _inset[SIDE_TOP];
	right = _inset[SIDE_RIGHT];
	bottom = _inset[SIDE_BOTTOM];
}

void Border::ApplySchemeSettings(IScheme* pScheme, KeyValues* inResourceData)
{
	const char *insetString = inResourceData->GetString("inset", "0 0 0 0");

	int left, top, right, bottom;
	GetInset(left, top, right, bottom);

	sscanf(insetString, "%d %d %d %d", &left, &top, &right, &bottom);

	SetInset(left, top, right, bottom);

	ParseSideSettings(SIDE_LEFT, inResourceData->FindKey("Left"), pScheme);
	ParseSideSettings(SIDE_TOP, inResourceData->FindKey("Top"), pScheme);
	ParseSideSettings(SIDE_RIGHT, inResourceData->FindKey("Right"), pScheme);
	ParseSideSettings(SIDE_BOTTOM, inResourceData->FindKey("Bottom"), pScheme);
}

const char* Border::GetName()
{
	return _name;
}

void Border::SetName(const char* name)
{
	if (_name)
		delete[] _name;

	_name = new char[strlen(name) + 1];
	strcpy(_name, name);
}

void Border::ParseSideSettings(int side_index, KeyValues* inResourceData, IScheme* pScheme)
{
	if (!inResourceData)
		return;

	auto& side = _sides[side_index];

	int iCount = 0;

	for (auto pKV = inResourceData->GetFirstSubKey(); pKV; pKV = pKV->GetNextKey())
	{
		++iCount;
	}

	side.count = iCount;
	side.lines = new line_t[iCount];

	for (int i = 0; i < iCount; ++i)
	{
		side.lines[i].col.SetColor(0, 0, 0, 0);
	}

	int index = 0;

	for (auto pKV = inResourceData->GetFirstSubKey(); pKV; pKV = pKV->GetNextKey(), ++index)
	{
		auto color = pScheme->GetColor(pKV->GetString("color", nullptr), Color(0, 0, 0, 0));

		side.lines[index].col = color;

		auto pszOffset = pKV->GetString("offset", nullptr);

		int Start, End;

		if (pszOffset)
		{
			sscanf(pszOffset, "%d %d", &Start, &End);
		}
		else
		{
			Start = 0;
			End = 0;
		}

		side.lines[index].startOffset = Start;
		side.lines[index].endOffset = End;
	}
}
}

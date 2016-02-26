#include "box_viewer.hpp"

#include "lang.h"
#include "text.h"
#include "utils.h"
#include "pokemon.hpp"

#include "ultra_box_viewer.hpp"
#include "savexit_viewer.hpp"

#include <stdio.h>
#include <string.h>

#define BOX_HEADER_SELECTED (-1)
#define SLOT_NO_SELECTION (-1)

#define SCREEN_WIDTH (320)
#define SCREEN_HEIGHT (240)

#define BOX_HEADER_WIDTH (220)
#define BOX_HEADER_HEIGHT (32)
#define BACKGROUND_WIDTH (220)
#define BACKGROUND_HEIGHT (210)
#define BACKGROUND_SHIFT (40)

#define PC_BOX_SHIFT_USED (0)
#define BK_BOX_SHIFT_USED (SCREEN_WIDTH - BACKGROUND_WIDTH)
#define BK_BOX_SHIFT_UNUSED (PC_BOX_SHIFT_USED + BACKGROUND_WIDTH + BACKGROUND_SHIFT)
#define PC_BOX_SHIFT_UNUSED (BK_BOX_SHIFT_USED - BACKGROUND_WIDTH - BACKGROUND_SHIFT)

#define EGG_ID (PKM_COUNT)

#define BALL_ROW_COUNT (5)
#define BALL_SIZE (32)

// --------------------------------------------------
/// Compute the current box pointer of the cursor
int16_t* currentBox(CursorBox_s* cursorBox)
// --------------------------------------------------
{
	cursorBox->box = &(cursorBox->inBank ? cursorBox->boxBK : cursorBox->boxPC);
	return cursorBox->box;
}


// --------------------------------------------------
/// Compute the slot and the inslot of the cursor
void computeSlot(CursorBox_s* cursorBox)
// --------------------------------------------------
{
	currentBox(cursorBox);
	cursorBox->inslot = (cursorBox->row == BOX_HEADER_SELECTED ? SLOT_NO_SELECTION : cursorBox->row * BOX_COL_PKM_COUNT + cursorBox->col);
	cursorBox->slot   = (cursorBox->row == BOX_HEADER_SELECTED ? SLOT_NO_SELECTION : *cursorBox->box * BOX_PKM_COUNT + cursorBox->inslot);
}


// --------------------------------------------------
void computeBoxSlot(BoxSlot_s* boxSlot, CursorBox_s* cursorBox)
// --------------------------------------------------
{
	// extractBoxSlot shall already be called before.

	boxSlot->rowCount = cursorBox->row - boxSlot->row;
	boxSlot->colCount = cursorBox->col - boxSlot->col;

	if (boxSlot->rowCount < 0)
	{
		boxSlot->row = boxSlot->row + boxSlot->rowCount;
		boxSlot->row *= -1;
	}
	if (boxSlot->colCount < 0)
	{
		boxSlot->col = boxSlot->col + boxSlot->colCount;
		boxSlot->col *= -1;
	}

	boxSlot->rowCount++;
	boxSlot->colCount++;
}


// --------------------------------------------------
/// Convert CursorBox position to BoxSlot position
void extractBoxSlot(BoxSlot_s* boxSlot, CursorBox_s* cursorBox)
// --------------------------------------------------
{
	boxSlot->inBank = cursorBox->inBank;
	boxSlot->slot = cursorBox->slot;
	boxSlot->inslot = cursorBox->inslot;
	boxSlot->box = *(cursorBox->box);
	boxSlot->row = cursorBox->row;
	boxSlot->col = cursorBox->col;
}


// --------------------------------------------------
/// Convert BoxSlot position to CursorBox position
void injectBoxSlot(BoxSlot_s* boxSlot, CursorBox_s* cursorBox)
// --------------------------------------------------
{
	cursorBox->inBank = boxSlot->inBank;
	cursorBox->slot = boxSlot->slot;
	cursorBox->inslot = boxSlot->inslot;
	(boxSlot->inBank ? cursorBox->boxBK : cursorBox->boxPC) = boxSlot->box;
	cursorBox->row = boxSlot->row;
	cursorBox->col = boxSlot->col;
}


/*----------------------------------------------------------*\
 |                       Viewer Class                       |
\*----------------------------------------------------------*/


	/*--------------------------------------------------*\
	 |             Constructor / Destructor             |
	\*--------------------------------------------------*/


// --------------------------------------------------
BoxViewer::BoxViewer(Viewer* parent) : Viewer(parent) { }
// --------------------------------------------------


// --------------------------------------------------
BoxViewer::BoxViewer(ViewType vType, Viewer* parent) : Viewer(vType, parent) { }
// --------------------------------------------------


// --------------------------------------------------
BoxViewer::~BoxViewer()
// --------------------------------------------------
{

}


	/*--------------------------------------------------*\
	 |                  Viewer Methods                  |
	\*--------------------------------------------------*/


		/*------------------------------------------*\
		 |              Public Methods              |
		\*------------------------------------------*/


// --------------------------------------------------
Result BoxViewer::initialize()
// --------------------------------------------------
{
	if (hasChild()) { if (this->child->initialize() == PARENT_STEP); else return CHILD_STEP; }

	// Use the Save Manager of PHBanku
	save = PHBanku::save;

	// Initialize the 2 boxes content
	cursorBox.inBank = true; selectViewBox();
	cursorBox.inBank = false; selectViewBox();

	// Initialize the current Pokémon
	selectViewPokemon();

	sf2d_set_clear_color(RGBA8(0x40,0x40,0x40,0xFF));

	return PARENT_STEP;
}


// --------------------------------------------------
Result BoxViewer::drawTopScreen()
// --------------------------------------------------
{
	if (hasRegularChild()) { if (this->child->drawTopScreen() == PARENT_STEP); else return CHILD_STEP; }

	// Draw the resume background
	sf2d_draw_texture(PHBanku::texture->resumeBackground, 0, 0);

	sftd_draw_text_white(11, 40, "Game's OT");
	sftd_draw_text_white(91, 40, "%s (%lx-%lx-%lx)", save->savedata.OTName, save->savedata.TID, save->savedata.SID, save->savedata.TSV);

	// If there is a current Pokémon
	if (vPkm.pkm && !vPkm.emptySlot)
	{
		uint32_t x, y;

		sf2d_draw_texture_part(PHBanku::texture->ballIcons, 5, 5, (vPkm.ball % BALL_ROW_COUNT) * BALL_SIZE, (vPkm.ball / BALL_ROW_COUNT) * BALL_SIZE, BALL_SIZE, BALL_SIZE);

		x = 32;
		y = 16 - 2;
		// Is the Pokémon an egg?
		if (vPkm.pkm->isEggy)
		{
			sftd_draw_wtext_white(x, y, PHBanku::data->species(0));
		}
		else
		{
			// Is the Pokémon nicknamed?
			if (Pokemon::isNicknamed(vPkm.pkm))
				sftd_draw_wtext_white(x, y, vPkm.NKName);
			else
				sftd_draw_wtext_white(x, y, vPkm.species);
		}

		sftd_draw_text_white(x + 168, y, "Lv.%u", vPkm.level);

		x = 11;
		y = 42 - 2;
		sftd_draw_text_white(x, (y += 15), "Dex No.");
		sftd_draw_text_white(x+50, y, "%03u", vPkm.pkm->speciesID);
		sftd_draw_wtext_white(x+80, y, vPkm.species);
		sftd_draw_text_white(x, (y += 15), "O.T.");
		sftd_draw_wtext_white(x+50, y, vPkm.OTName);
		sftd_draw_text_white(x, (y += 15), "Stat");
		sftd_draw_text_white(x+90, y, "Value");
		sftd_draw_text_white(x+128, y, "IVs");
		sftd_draw_text_white(x+158, y, "EVs");
		sftd_draw_text_white(x, (y+=15), "HP");
		sftd_draw_text_white(x+100, y, "% 3u", vPkm.stats[Stat::HP]);
		sftd_draw_text_white(x+130, y, "% 2u", vPkm.ivs[Stat::HP]);
		sftd_draw_text_white(x+160, y, "% 3u", vPkm.evs[Stat::HP]);
		sftd_draw_text_white(x, (y+=15), "Attack");
		sftd_draw_text_white(x+100, y, "% 3u", vPkm.stats[Stat::ATK]);
		sftd_draw_text_white(x+130, y, "% 2u", vPkm.ivs[Stat::ATK]);
		sftd_draw_text_white(x+160, y, "% 3u", vPkm.evs[Stat::ATK]);
		sftd_draw_text_white(x, (y+=15), "Defense");
		sftd_draw_text_white(x+100, y, "% 3u", vPkm.stats[Stat::DEF]);
		sftd_draw_text_white(x+130, y, "% 2u", vPkm.ivs[Stat::DEF]);
		sftd_draw_text_white(x+160, y, "% 3u", vPkm.evs[Stat::DEF]);
		sftd_draw_text_white(x, (y+=15), "Sp.Attack");
		sftd_draw_text_white(x+100, y, "% 3u", vPkm.stats[Stat::SPA]);
		sftd_draw_text_white(x+130, y, "% 2u", vPkm.ivs[Stat::SPA]);
		sftd_draw_text_white(x+160, y, "% 3u", vPkm.evs[Stat::SPA]);
		sftd_draw_text_white(x, (y+=15), "Sp.Defense");
		sftd_draw_text_white(x+100, y, "% 3u", vPkm.stats[Stat::SPD]);
		sftd_draw_text_white(x+130, y, "% 2u", vPkm.ivs[Stat::SPD]);
		sftd_draw_text_white(x+160, y, "% 3u", vPkm.evs[Stat::SPD]);
		sftd_draw_text_white(x, (y+=15), "Speed");
		sftd_draw_text_white(x+100, y, "% 3u", vPkm.stats[Stat::SPE]);
		sftd_draw_text_white(x+130, y, "% 2u", vPkm.ivs[Stat::SPE]);
		sftd_draw_text_white(x+160, y, "% 3u", vPkm.evs[Stat::SPE]);
		sftd_draw_text_white(x, (y += 15), "Nature");
		sftd_draw_wtext_white(x+50, y, vPkm.nature);
		sftd_draw_text_white(x, (y += 15), "Ability");
		sftd_draw_wtext_white(x+50, y, vPkm.ability);
		sftd_draw_text_white(x, (y += 15), "Item");
		sftd_draw_wtext_white(x+50, y, vPkm.item);

		x = 246;
		y = 147 - 15 - 2;
		sftd_draw_text_white(x, (y += 15), "Hidden Power");
		sftd_draw_wtext_white(x+90, y, vPkm.hpType);
		sftd_draw_text_white(x, (y += 15), "Moves");
		sftd_draw_wtext_white(x+10, (y += 15), vPkm.moves[0]);
		sftd_draw_wtext_white(x+10, (y += 15), vPkm.moves[1]);
		sftd_draw_wtext_white(x+10, (y += 15), vPkm.moves[2]);
		sftd_draw_wtext_white(x+10, (y += 15), vPkm.moves[3]);

		drawPokemonScale(vPkm.pkm, 256, 48, 3.0f);

		if (vPkm.pkm->isShiny)
		{
			sf2d_draw_texture_part(PHBanku::texture->boxTiles, 240, 135, 54, 64, 9, 9);
		}

		if (vPkm.isCured)
		{
			sf2d_draw_texture_part(PHBanku::texture->boxTiles, 260, 135, 72, 64, 9, 9);
		}
		else if (vPkm.isInfected)
		{
			sf2d_draw_texture_part(PHBanku::texture->boxTiles, 260, 135, 81, 64, 27, 9);
		}

		if (vPkm.gen > 0)
		{
			if (vPkm.gen == 6) // Kalos Born (Gen VI)
				sf2d_draw_texture_part(PHBanku::texture->boxTiles, 250, 135, 63, 64, 9, 9);
			else if (vPkm.gen == 5) // Gen V
				sf2d_draw_texture_part(PHBanku::texture->boxTiles, 250, 135, 72, 73, 9, 9);
			else if (vPkm.gen == 4) // Gen IV
				sf2d_draw_texture_part(PHBanku::texture->boxTiles, 250, 135, 63, 73, 9, 9);
			else if (vPkm.gen == 3) // Gen III
				sf2d_draw_texture_part(PHBanku::texture->boxTiles, 250, 135, 54, 73, 9, 9);
		}

		sf2d_draw_texture_part(PHBanku::texture->boxTiles, 290 ,135, 9*0, 64 + 9*vPkm.circle, 9, 9);
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, 300 ,135, 9*1, 64 + 9*vPkm.triangle, 9, 9);
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, 310 ,135, 9*2, 64 + 9*vPkm.square, 9, 9);
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, 320 ,135, 9*3, 64 + 9*vPkm.heart, 9, 9);
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, 330 ,135, 9*4, 64 + 9*vPkm.star, 9, 9);
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, 340 ,135, 9*5, 64 + 9*vPkm.diamond, 9, 9);
	}

	if (hasOverlayChild()) { this->child->drawTopScreen(); }
	return SUCCESS_STEP;
}


// --------------------------------------------------
Result BoxViewer::drawBotScreen()
// --------------------------------------------------
{
	if (hasRegularChild()) { if (this->child->drawBotScreen() == PARENT_STEP); else return CHILD_STEP; }
	{
		// Retrieve the current box, and the drawing offset.
		s16 boxShift = (cursorBox.inBank ? BK_BOX_SHIFT_USED : PC_BOX_SHIFT_USED);
		s16 middleBoxShift = (cursorBox.inBank ? PC_BOX_SHIFT_UNUSED : PC_BOX_SHIFT_USED) + BACKGROUND_WIDTH;

		// Draw the current box: the background and the icons.
		drawBox((cursorBox.inBank ? vBKBox : vPCBox), boxShift, 20);

		// Draw the other box: the background and the icons.
		drawBox((cursorBox.inBank ? vPCBox : vBKBox), (cursorBox.inBank ? PC_BOX_SHIFT_UNUSED : BK_BOX_SHIFT_UNUSED), 20);

		// Draw CursorType buttons (Red|Blue|Green)
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, boxShift + 21 +   0, 0,   0, 0, 64, 32);
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, boxShift + 21 +  64, 0,  64, 0, 64, 32);
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, boxShift + 21 + 128, 0, 128, 0, 64, 32);

		// Draw SwapBox button
		sf2d_draw_texture_part(PHBanku::texture->boxTiles, middleBoxShift + 10, 210, 128, 32, 20, 20);

		// If a Pokémon is currently selected
		if (sPkm)
		{
			u16 pkm_x, pkm_y;

			// If the selected Pokémon is dragged
			if (isPkmDragged)
			{
				// Draw dragged Pokémon icon under the stylus
				pkm_x = touch.px - 16;
				pkm_y = touch.py - 16;
			}
			// If the selected Pokémon is held
			else // if (isPkmHeld)
			{
				if (cursorBox.inslot == SLOT_NO_SELECTION)
				{
					// Draw the Pokémon icon on the box title
					pkm_x = boxShift + 105;
					pkm_y = 8;
				}
				else
				{
					// Draw the Pokémon icon on the current slot a bit shifted
					pkm_x = boxShift + 12 + (cursorBox.inslot % 6) * 35;
					pkm_y = 20 + 13 + (cursorBox.inslot / 6) * 35;
				}
			}

			drawPokemon(sPkm, pkm_x, pkm_y);
		}
		else
		{
			// Draw the cursor
			if (cursorBox.inslot == SLOT_NO_SELECTION)
			{
				// Draw the cursor icon on the box title
				sf2d_draw_texture_part(PHBanku::texture->boxTiles, boxShift + 105, 8 - cursorPositionOffY, 32 * (u8) cursorType, 32, 32, 32);
			}
			else
			{
				// Draw the cursor icon on the current slot a bit shifted
				sf2d_draw_texture_part(PHBanku::texture->boxTiles, boxShift + 17 + (cursorBox.inslot % 6) * 35 + cursorPositionOffY / 2, 20 + 13 + (cursorBox.inslot / 6) * 35 - cursorPositionOffY, 32 * (u8) cursorType, 32, 32, 32);
			}
		}
	}

	if (hasOverlayChild()) { this->child->drawBotScreen(); }
	return SUCCESS_STEP;
}


// --------------------------------------------------
Result BoxViewer::updateControls(const u32& kDown, const u32& kHeld, const u32& kUp, const touchPosition* touch)
// --------------------------------------------------
{
	if (hasRegularChild() || hasOverlayChild()) { if (this->child->updateControls(kDown, kHeld, kUp, touch) == PARENT_STEP); else return CHILD_STEP; }

	if (kDown & KEY_START)
	{
		// Open the Savexit popup
		new SavexitViewer(ViewType::Overlay, this);
		child->initialize();
		return CHILD_STEP;
	}

	{
		bool boolMod = false;
		s16 boxMod = 0;
		s16 rowMod = 0;
		s16 colMod = 0;

		if (kDown & KEY_L) boxMod--;
		else if (kDown & KEY_R) boxMod++;

		if (kDown & KEY_UP) rowMod--;
		else if (kDown & KEY_DOWN) rowMod++;

		if (kDown & KEY_LEFT) { if (cursorBox.row == BOX_HEADER_SELECTED) boxMod--; else colMod--; }
		else if (kDown & KEY_RIGHT) { if (cursorBox.row == BOX_HEADER_SELECTED) boxMod++; else colMod++; }

		if (kDown & KEY_ZL) { cursorBox.inBank = false; boolMod = true; }
		else if (kDown & KEY_ZR) { cursorBox.inBank = true; boolMod = true; }

		if (kDown & KEY_TOUCH)
		{
			s16 boxShift = (cursorBox.inBank ? BK_BOX_SHIFT_USED : PC_BOX_SHIFT_USED);

			// If the SwapBox buttons are touched down
			if (touchWithin(touch->px, touch->py, boxShift + 10, 20, 16, 24)) boxMod--;
			else if (touchWithin(touch->px, touch->py, boxShift + 196, 20, 16, 24)) boxMod++;

			boxShift = (cursorBox.inBank ? PC_BOX_SHIFT_UNUSED : BK_BOX_SHIFT_UNUSED);

			// If the other box (PC|BK) is touched down
			if (touchWithin(touch->px, touch->py, boxShift, 20, BACKGROUND_WIDTH, BACKGROUND_HEIGHT)) { cursorBox.inBank = !cursorBox.inBank; boolMod = true; }
		}
		else if (kHeld & KEY_TOUCH)
		{
			// If there is a Pokémon currently dragged
			if (isPkmDragged)
			{
				s16 boxShift = (cursorBox.inBank ? BK_BOX_SHIFT_USED : PC_BOX_SHIFT_USED);

				// If the SwapBox buttons are touched held once
				if (touchWithin(touch->px, touch->py, boxShift + 10, 20, 16, 24) && !touchWithin(this->touch.px, this->touch.py, boxShift + 10, 20, 16, 24)) boxMod--;
				else if (touchWithin(touch->px, touch->py, boxShift + 196, 20, 16, 24) && !touchWithin(this->touch.px, this->touch.py, boxShift + 196, 20, 16, 24)) boxMod++;

				boxShift = (cursorBox.inBank ? PC_BOX_SHIFT_UNUSED : BK_BOX_SHIFT_UNUSED);

				// If the other box (PC|BK) is touched held
				if (touchWithin(touch->px, touch->py, boxShift, 20, BACKGROUND_WIDTH, BACKGROUND_HEIGHT)) { cursorBox.inBank = !cursorBox.inBank; boolMod = true; }
			}
		}

		if (boxMod || rowMod || colMod)
		{
			currentBox(&cursorBox);
			*cursorBox.box += boxMod;
			cursorBox.row += rowMod;
			cursorBox.col += colMod;

			if (*cursorBox.box < 0) *cursorBox.box = (cursorBox.inBank ? save->bankdata.bk.boxUnlocked : save->savedata.pc.boxUnlocked)-1;
			else if (*cursorBox.box > (cursorBox.inBank ? save->bankdata.bk.boxUnlocked : save->savedata.pc.boxUnlocked)-1) *cursorBox.box = 0;

			if (cursorBox.row < BOX_HEADER_SELECTED) cursorBox.row = BOX_ROW_PKM_COUNT-1;
			else if (cursorBox.row > BOX_ROW_PKM_COUNT-1) cursorBox.row = BOX_HEADER_SELECTED;

			if (cursorBox.col < 0) { cursorBox.col = BOX_COL_PKM_COUNT-1; cursorBox.inBank = !cursorBox.inBank; }
			else if (cursorBox.col > BOX_COL_PKM_COUNT-1) { cursorBox.col = 0; cursorBox.inBank = !cursorBox.inBank; }

			boolMod = true;
		}

		if (boolMod)
		{
			// Update the current Box/Pokémon displayed
			currentBox(&cursorBox);
			selectViewBox();
			selectViewPokemon();
		}
	}

	if (kDown & KEY_SELECT)
	{
		switchCursorType();
	}

	if (cursorType == CursorType::SingleSelect)
	{
		if (kDown & KEY_A && !isPkmDragged)
		{
			// Move a Pokémon (grab and drop)
			selectMovePokemon();
		}

		if (kDown & KEY_B && !isPkmDragged)
		{
			// Drop a Pokémon
			cancelMovePokemon();
		}
	}
	else if (cursorType == CursorType::QuickSelect)
	{
		if (kDown & KEY_A && !isPkmDragged)
		{
			// Move a Pokémon (grab and drop)
			selectMovePokemon();
		}

		if (kDown & KEY_B && !isPkmDragged)
		{
			// Drop a Pokémon
			cancelMovePokemon();
		}

		if (kDown & KEY_Y && !sPkm)
		{
			// Swap the two boxes (PC|BK)
			save->moveBox(cursorBox.boxPC, false, cursorBox.boxBK, true);
		}
	}
	else if (cursorType == CursorType::MultipleSelect)
	{
		if (kDown & KEY_A)
		{
			// Move a Pokémon (grab and drop)
			selectMovePokemon();

			// TODO: Feature to Move Pokémon (multiple-grab and multiple-drop)
			// selectMultiMovePokemon();
		}

		if (kDown & KEY_Y)
		{
			// Open the UltraBox viewer
			UltraBoxViewer* ultraBoxViewer = new UltraBoxViewer(ViewType::Regular, this);
			ultraBoxViewer->selectViewBox(*cursorBox.box, cursorBox.inBank);
			child->initialize();
			return CHILD_STEP;
		}
	}


	{
		if (kDown & KEY_TOUCH)
		{
			s16 boxShift = (cursorBox.inBank ? BK_BOX_SHIFT_USED : PC_BOX_SHIFT_USED);
			s16 middleBoxShift = (cursorBox.inBank ? PC_BOX_SHIFT_UNUSED : PC_BOX_SHIFT_USED) + BACKGROUND_WIDTH;
			u16 px = touch->px;
			u16 py = touch->py;

			// If the TouchArea is within the SingleSelect CursorType button area
			if (touchWithin(px, py, boxShift + 22, 0, 59, 16))
			{
				selectCursorType(CursorType::SingleSelect);
			}
			// If the TouchArea is within the QuickSelect CursorType button area
			else if (touchWithin(px, py, boxShift + 86, 0, 59, 16))
			{
				selectCursorType(CursorType::QuickSelect);
			}
			// If the TouchArea is within the MultipleSelect CursorType button area
			else if (touchWithin(px, py, boxShift + 150, 0, 59, 16))
			{
				selectCursorType(CursorType::MultipleSelect);
			}
			// If the TouchArea is within the Pokémon icons area of the box
			else if (touchWithin(px, py, boxShift, 50, BACKGROUND_WIDTH, BACKGROUND_HEIGHT - 30))
			{
				// If no Pokémon is currently selected or dragged
				if (!sPkm && !isPkmDragged)
				{
					// Move the cursor to the new slot
					cursorBox.row = ((py - 50) / 35);
					cursorBox.col = ((px - boxShift) / 35);

					// Update the current Pokémon
					selectViewPokemon();

					// Move the current Pokémon (grab)
					selectMovePokemon();

					if (!vPkm.emptySlot)
						isPkmDragged = true;
				}
				// If a Pokémon is currently selected and dragged
				else
				{
					u16 oldRow = cursorBox.row;
					u16 oldCol = cursorBox.col;

					// Move the cursor to the new slot
					cursorBox.row = ((py - 50) / 35);
					cursorBox.col = ((px - boxShift) / 35);

					// Update the current Pokémon
					selectViewPokemon();

					// And drop the Pokémon if one is held and it is the same current slot (mean double tap to move a held Pokémon)
					if (isPkmHeld && oldRow == cursorBox.row && oldCol == cursorBox.col)
						selectMovePokemon();
				}
			}
			// If the TouchArea is within the SwapBox button area
			else if (touchWithin(px, py, middleBoxShift + 10, 210, 20, 20))
			{
				if (!isPkmHeld && !isPkmDragged)
				{
					// Swap the two boxes (PC|BK)
					save->moveBox(cursorBox.boxPC, false, cursorBox.boxBK, true);
				}
			}
		}
		else if (kHeld & KEY_TOUCH)
		{
			// Keep the last touched held position
			this->touch.px = touch->px;
			this->touch.py = touch->py;
		}
		else if (kUp & KEY_TOUCH)
		{
			touch = &(this->touch);

			s16 boxShift = (cursorBox.inBank ? BK_BOX_SHIFT_USED : PC_BOX_SHIFT_USED);
			u16 px = touch->px;
			u16 py = touch->py;

			// If a Pokémon is currently dragged
			if (sPkm && isPkmDragged)
			{
				// If the TouchArea is within the Pokémon icons area of the box
				if (touchWithin(px, py, boxShift, 50, BACKGROUND_WIDTH, BACKGROUND_HEIGHT))
				{
					// Move the cursor to the new slot
					cursorBox.row = ((py - 50) / 35);
					cursorBox.col = ((px - boxShift) / 35);

					// Update the current Pokémon
					selectViewPokemon();
					// Move the dragged Pokémon (drop)
					selectMovePokemon();
				}
				else
				{
					// Cancel the Pokémon dragging
					cancelMovePokemon();

					// Return to the old dragged Pokémon slot?
					// injectBoxSlot(&sSlot, &cursorBox);
					// selectViewBox();

					// Update the current Pokémon
					selectViewPokemon();
				}

				isPkmDragged = false;
			}
		}
	}

	cursorPositionOffY += cursorPositionShiftY;
	if (cursorPositionOffY < 0 || cursorPositionOffY > cursorPositionMaxY)
		cursorPositionShiftY *= -1;

	return SUCCESS_STEP;
}


// --------------------------------------------------
void BoxViewer::selectViewBox(uint16_t boxID, bool inBank)
// --------------------------------------------------
{
	// Change a box ID and update both displayed boxes (PC|BK)

	bool BK = cursorBox.inBank;
	cursorBox.inBank = inBank;

	if (inBank)
		cursorBox.boxBK = boxID;
	else
		cursorBox.boxPC = boxID;

	selectViewBox();

	cursorBox.inBank = BK;
	selectViewBox();
}


		/*------------------------------------------*\
		 |             Protected Methods            |
		\*------------------------------------------*/


		/*------------------------------------------*\
		 |              Private Methods             |
		\*------------------------------------------*/


// --------------------------------------------------
void BoxViewer::drawBox(box_s* box, int16_t x, int16_t y)
// --------------------------------------------------
{
	// Draw the box background
	sf2d_draw_texture_part(PHBanku::texture->boxBackgrounds, x, y, ((box->background % 16) % 4) * BACKGROUND_WIDTH, ((box->background % 16) / 4) * BACKGROUND_HEIGHT, BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
	sf2d_draw_texture_part(PHBanku::texture->boxBackgrounds, x, y, ((box->background % 16) % 4) * BOX_HEADER_WIDTH, 840 + ((box->background % 16) / 4) * BOX_HEADER_HEIGHT, BOX_HEADER_WIDTH, BOX_HEADER_HEIGHT);

	// Draw the box arrows
	sf2d_draw_texture_part(PHBanku::texture->boxTiles, x + 4, y + 3, 96, 32, 16, 24);
	sf2d_draw_texture_part(PHBanku::texture->boxTiles, x + BACKGROUND_WIDTH - 20, y + 3, 112, 32, 16, 24);

	// Draw the box title
	char boxTitle[0x1a];
	/// TODO: Remove that if statement when complete box->title.
	if (box->title[0] != '\0') snprintf(boxTitle, 0x1a, box->title);
	else snprintf(boxTitle, 0x1a, "Box %i", box->number+1);
	int boxTitleWidth = sftd_get_text_width(font, 12, boxTitle);
	sftd_draw_text(font, x + (BACKGROUND_WIDTH - boxTitleWidth) / 2, y + 7, RGBA8(0x00,0x00,0x00,0xFF), 12, boxTitle);

	// if (isPkmMSelecting || isPkmMDragged)
	// {
	// 	sf2d_draw_rectangle(x + sSlot.col * 35, y + 30 + sSlot.row * 35, sSlot.colCount * 35, sSlot.colCount * 35, RGBA8(0x44, 0xCC, 0x66, 0xAA));
	// }

	// TODO DRY Merge that! v

	// If there is a Pokémon currently selected
	if (sPkm)
	{
		// Draw Pokémon icons
		for (u8 i = 0; i < 30; i++)
		{
			// If the Pokémon isn't the selected Pokémon
			if (sPkm != &(box->slot[i]))
			{
				drawPokemon(&box->slot[i], x + (i % BOX_COL_PKM_COUNT) * 35, y + 30 + (i / BOX_COL_PKM_COUNT) * 35);
			}
		}
	}
	// If there is no Pokémon currently selected
	else
	{
		// Draw Pokémon icons
		for (u8 i = 0; i < 30; i++)
		{
			drawPokemon(&box->slot[i], x + (i % BOX_COL_PKM_COUNT) * 35, y + 30 + (i / BOX_COL_PKM_COUNT) * 35);
		}
	}

	// TODO DRY Merge that! ^
}


// --------------------------------------------------
void BoxViewer::drawPokemon(pkm_s* pkm, int16_t x, int16_t y)
// --------------------------------------------------
{
	if (pkm->speciesID == 0) return;

	sf2d_texture* pkmIcons = PHBanku::texture->pkmIcons;

	if (pkm->isEggy)
	{
		if (pkm->isShiny) pkmIcons = PHBanku::texture->pkmShinyIcons;

		// Draw the egg icon
		sf2d_draw_texture_part_blend(pkmIcons, x, y, ((pkm->speciesID-1) % 25) * 40, ((pkm->speciesID-1) / 25) * 30, 40, 30, RGBA8(0xFF,0xFF,0xFF,0xAA));
		sf2d_draw_texture_part(pkmIcons, x, y, ((EGG_ID) % 25) * 40, ((EGG_ID) / 25) * 30, 40, 30);
	}
	else
	{
		if (pkm->isShiny) pkmIcons = PHBanku::texture->pkmShinyIcons;

		// Draw the Pokémon icon
		sf2d_draw_texture_part(pkmIcons, x, y, ((pkm->speciesID-1) % 25) * 40, ((pkm->speciesID-1) / 25) * 30, 40, 30);
	}
}


// --------------------------------------------------
void BoxViewer::drawPokemonScale(pkm_s* pkm, int16_t x, int16_t y, float scale)
// --------------------------------------------------
{
	if (pkm->speciesID == 0) return;

	sf2d_texture* pkmIcons = PHBanku::texture->pkmIcons;

	if (pkm->isEggy)
	{
		if (pkm->isShiny) pkmIcons = PHBanku::texture->pkmShinyIcons;

		// Draw the egg icon
		sf2d_draw_texture_part_scale(pkmIcons, x, y, ((pkm->speciesID-1) % 25) * 40, ((pkm->speciesID-1) / 25) * 30, 40, 30, scale, scale);
		sf2d_draw_texture_part(pkmIcons, x, y + 30 * (scale - 1), ((EGG_ID) % 25) * 40, ((EGG_ID) / 25) * 30, 40, 30);
	}
	else
	{
		if (pkm->isShiny) pkmIcons = PHBanku::texture->pkmShinyIcons;

		// Draw the Pokémon icon
		sf2d_draw_texture_part_scale(pkmIcons, x, y, ((pkm->speciesID-1) % 25) * 40, ((pkm->speciesID-1) / 25) * 30, 40, 30, scale, scale);
	}
}


// --------------------------------------------------
void BoxViewer::selectCursorType(CursorType cursorType)
// --------------------------------------------------
{
	this->cursorType = cursorType;
}


// --------------------------------------------------
void BoxViewer::switchCursorType()
// --------------------------------------------------
{
	// ... -> Single -> Quick -> Multiple -> ... //
	if (cursorType == CursorType::SingleSelect)
		selectCursorType(CursorType::QuickSelect);
	else if (cursorType == CursorType::QuickSelect)
		selectCursorType(CursorType::MultipleSelect);
	else if (cursorType == CursorType::MultipleSelect)
		selectCursorType(CursorType::SingleSelect);
}


// --------------------------------------------------
void BoxViewer::selectViewBox()
// --------------------------------------------------
{
	// Compute the current cursor slot
	computeSlot(&cursorBox);

	box_s** vBox = NULL;
	if (cursorBox.inBank)
		vBox = &vBKBox;
	else
		vBox = &vPCBox;

	save->getBox(*cursorBox.box, vBox, cursorBox.inBank);
}


// --------------------------------------------------
void BoxViewer::selectViewPokemon()
// --------------------------------------------------
{
	// Compute the current cursor slot
	computeSlot(&cursorBox);

	// If the cursor isn't in a box slot
	if (cursorBox.slot == SLOT_NO_SELECTION)
	{
		vPkm.pkm = NULL;
	}
	// If the cursor is in a box slot
	else
	{
		save->getPkm(*cursorBox.box, cursorBox.inslot, &vPkm.pkm, cursorBox.inBank);
		populateVPkmData(&vPkm);
	}
}


// --------------------------------------------------
void BoxViewer::selectMovePokemon()
// --------------------------------------------------
{
	computeSlot(&cursorBox);
	// selectViewPokemon();

	// If no Pokémon is currently selected
	if (!sPkm)
	{
		// If the current Pokémon slot isn't empty (to avoid empty slot move)
		if (!vPkm.emptySlot)
		{
			// Select the current Pokémon
			sPkm = vPkm.pkm;
			extractBoxSlot(&sSlot, &cursorBox);
			if (!isPkmDragged) isPkmHeld = true;
		}
	}
	// Else if there is a current Pokémon
	else if (vPkm.pkm)
	{
		// If the selected Pokémon isn't the current Pokémon
		if (sPkm != vPkm.pkm)
		{
			// Swap the Pokémon currenlty selected and the current Pokémon, and keep the return value (true: had moved, false: hadn't)
			bool moved = save->movePkm(sPkm, vPkm.pkm, sSlot.inBank, cursorBox.inBank);

			// If the Pokémon had moved
			if (moved)
			{
				// Cancel the selection
				cancelMovePokemon();
			}
			// And populate the Pokémon data
			populateVPkmData(&vPkm);
		}
		else
		{
			// Cancel the selection, since it's moved to the same slot
			cancelMovePokemon();
		}
	}
}


// --------------------------------------------------
void BoxViewer::selectMultiMovePokemon()
// --------------------------------------------------
{
	computeSlot(&cursorBox);

	// If no Pokémon is currently selected
	if (!sPkm)
	{
		sPkm = vPkm.pkm;
		extractBoxSlot(&sSlot, &cursorBox);
		isPkmMSelecting = true;
	}
	// If a Pokémon is currently selected for the multi selection
	else if (isPkmMSelecting)
	{
		// TODO Stuff here!
		computeBoxSlot(&sSlot, &cursorBox);
		injectBoxSlot(&sSlot, &cursorBox);
		isPkmMSelecting = false;
		isPkmMDragged = true;
	}
	else if (isPkmMDragged)
	{
		// TODO Stuff here!
	}
}


// --------------------------------------------------
void BoxViewer::cancelMovePokemon()
// --------------------------------------------------
{
	// Reset the selection

	sPkm = NULL;
	isPkmHeld = false;
	isPkmDragged = false;
	isPkmMDragged = false;
	isPkmMSelecting = false;
}


// --------------------------------------------------
void BoxViewer::populateVPkmData(vPkm_s* vPkm)
// --------------------------------------------------
{
	memset(vPkm->NKName, 0, 0xD * sizeof(uint32_t));
	memset(vPkm->OTName, 0, 0xD * sizeof(uint32_t));
	memset(vPkm->HTName, 0, 0xD * sizeof(uint32_t));
	utf16_to_utf32(vPkm->NKName, Pokemon::NK_name(vPkm->pkm), 0xD);
	utf16_to_utf32(vPkm->OTName, Pokemon::OT_name(vPkm->pkm), 0xD);
	utf16_to_utf32(vPkm->HTName, Pokemon::HT_name(vPkm->pkm), 0xD);

	vPkm->emptySlot = save->isPkmEmpty(vPkm->pkm);

	vPkm->isKalosBorn = Pokemon::isKalosBorn(vPkm->pkm);
	vPkm->isInfected = Pokemon::isInfected(vPkm->pkm);
	vPkm->isCured = Pokemon::isCured(vPkm->pkm);

	vPkm->circle = Pokemon::circle(vPkm->pkm);
	vPkm->triangle = Pokemon::triangle(vPkm->pkm);
	vPkm->square = Pokemon::square(vPkm->pkm);
	vPkm->heart = Pokemon::heart(vPkm->pkm);
	vPkm->star = Pokemon::star(vPkm->pkm);
	vPkm->diamond = Pokemon::diamond(vPkm->pkm);

	vPkm->species = PHBanku::data->species(vPkm->pkm->speciesID);
	vPkm->item = PHBanku::data->items(vPkm->pkm->itemID);
	vPkm->nature = PHBanku::data->natures(Pokemon::nature(vPkm->pkm));
	vPkm->ability = PHBanku::data->abilities(Pokemon::ability(vPkm->pkm));
	vPkm->hpType = PHBanku::data->types(Pokemon::HPType(vPkm->pkm)+1);

	vPkm->moves[0] = PHBanku::data->moves(Pokemon::move1(vPkm->pkm));
	vPkm->moves[1] = PHBanku::data->moves(Pokemon::move2(vPkm->pkm));
	vPkm->moves[2] = PHBanku::data->moves(Pokemon::move3(vPkm->pkm));
	vPkm->moves[3] = PHBanku::data->moves(Pokemon::move4(vPkm->pkm));

	vPkm->level = Pokemon::level(vPkm->pkm);
	vPkm->stats[Stat::HP] = Pokemon::HP(vPkm->pkm);
	vPkm->stats[Stat::ATK] = Pokemon::ATK(vPkm->pkm);
	vPkm->stats[Stat::DEF] = Pokemon::DEF(vPkm->pkm);
	vPkm->stats[Stat::SPA] = Pokemon::SPA(vPkm->pkm);
	vPkm->stats[Stat::SPD] = Pokemon::SPD(vPkm->pkm);
	vPkm->stats[Stat::SPE] = Pokemon::SPE(vPkm->pkm);
	vPkm->ivs[Stat::HP] = Pokemon::IV_HP(vPkm->pkm);
	vPkm->ivs[Stat::ATK] = Pokemon::IV_ATK(vPkm->pkm);
	vPkm->ivs[Stat::DEF] = Pokemon::IV_DEF(vPkm->pkm);
	vPkm->ivs[Stat::SPA] = Pokemon::IV_SPA(vPkm->pkm);
	vPkm->ivs[Stat::SPD] = Pokemon::IV_SPD(vPkm->pkm);
	vPkm->ivs[Stat::SPE] = Pokemon::IV_SPE(vPkm->pkm);
	vPkm->evs[Stat::HP] = Pokemon::EV_HP(vPkm->pkm);
	vPkm->evs[Stat::ATK] = Pokemon::EV_ATK(vPkm->pkm);
	vPkm->evs[Stat::DEF] = Pokemon::EV_DEF(vPkm->pkm);
	vPkm->evs[Stat::SPA] = Pokemon::EV_SPA(vPkm->pkm);
	vPkm->evs[Stat::SPD] = Pokemon::EV_SPD(vPkm->pkm);
	vPkm->evs[Stat::SPE] = Pokemon::EV_SPE(vPkm->pkm);

	vPkm->ball = Pokemon::ball(vPkm->pkm) - 1;

	if (Pokemon::isGen6Born(vPkm->pkm))
		vPkm->gen = 6;
	else if (Pokemon::isGen5Born(vPkm->pkm))
		vPkm->gen = 5;
	else if (Pokemon::isGen4Born(vPkm->pkm))
		vPkm->gen = 4;
	else if (Pokemon::isGen3Born(vPkm->pkm))
		vPkm->gen = 3;
	else
		vPkm->gen = 0;
}

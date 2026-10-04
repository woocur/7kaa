/*
 * Seven Kingdoms: Ancient Adversaries
 *
 * Copyright 2026 7kaa hires fork contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Filename    : OTRAINER.CPP
// Description : in-game trainer window (single player only)

#include <stdio.h>
#include <OSYS.h>
#include <OVGA.h>
#include <vga_util.h>
#include <OMOUSE.h>
#include <OMOUSECR.h>
#include <KEY.h>
#include <OPOWER.h>
#include <OREMOTE.h>
#include <OFONT.h>
#include <OMUSIC.h>
#include <OCONFIG.h>
#include <OWORLD.h>
#include <OWORLDMT.h>
#include <OINFO.h>
#include <ONATIONA.h>
#include <OTOWN.h>
#include <OUNIT.h>
#include <OFIRM.h>
#include <OFIRMA.h>
#include <OF_BASE.h>
#include <OSPY.h>
#include <OTECHRES.h>
#include <OGODRES.h>
#include <OTRAINER.h>
#include "gettext.h"

//------------- Define constants ------------//

enum { TRAINER_WIDTH = 440,
       ROW_HEIGHT    = 24,
       TITLE_HEIGHT  = 34,
       FOOTER_HEIGHT = 30 };

#define TRAINER_HEIGHT (TITLE_HEIGHT + Trainer::OPTION_COUNT*ROW_HEIGHT + FOOTER_HEIGHT)
#define TRAINER_X1 (ZOOM_X1 + ( (ZOOM_X2-ZOOM_X1+1) - TRAINER_WIDTH ) / 2)
#define TRAINER_Y1 (ZOOM_Y1 + ( (ZOOM_Y2-ZOOM_Y1+1) - TRAINER_HEIGHT ) / 2)
#define TRAINER_X2 (TRAINER_X1 + TRAINER_WIDTH - 1)
#define TRAINER_Y2 (TRAINER_Y1 + TRAINER_HEIGHT - 1)
#define ROW_X1     (TRAINER_X1 + 12)
#define ROW_X2     (TRAINER_X2 - 12)
#define ROW_Y1     (TRAINER_Y1 + TITLE_HEIGHT)

#define CASH_STEP    10000
#define FOOD_STEP    10000
#define LOCK_LEVEL   50000

enum { OPT_CASH, OPT_FOOD, OPT_LOCK, OPT_TECH, OPT_MAP, OPT_REPUTATION,
       OPT_TOWN_LOYALTY, OPT_UNITS, OPT_KING_UNDIE, OPT_FAST_BUILD,
       OPT_FIRM_FINISH, OPT_TOWN_POP, OPT_SPY, OPT_CLOSE };

Trainer trainer;

//--------- Begin of function Trainer::Trainer ---------//

Trainer::Trainer()
{
	active_flag = 0;
	lock_resource_flag = 0;
	status_msg = NULL;
}
//--------- End of function Trainer::Trainer ---------//


//--------- Begin of function Trainer::reset ---------//
//
// Called when a game starts or is loaded.
//
void Trainer::reset()
{
	active_flag = 0;
	lock_resource_flag = 0;
	status_msg = NULL;
}
//--------- End of function Trainer::reset ---------//


//--------- Begin of function Trainer::is_available ---------//
//
int Trainer::is_available()
{
	return nation_array.player_recno && !remote.is_enable() && !remote.is_replay();
}
//--------- End of function Trainer::is_available ---------//


//--------- Begin of function Trainer::enter ---------//
//
void Trainer::enter()
{
	if( active_flag || !is_available() )
		return;

	active_flag = 1;
	status_msg = _("The game is paused while this window is open.");
	mouse_cursor.set_icon(CURSOR_NORMAL);
	power.win_opened = 1;

	while( active_flag )
	{
		sys.yield();
		vga.flip();
		mouse.get_event();

		char useBackBuf = vga.use_back_buf;
		vga.use_front();
		disp();
		if( useBackBuf )
			vga.use_back();

		sys.blt_virtual_buf();
		music.yield();
		detect();

		if( sys.signal_exit_flag )
			break;
	}

	active_flag = 0;
	power.win_opened = 0;
	sys.need_redraw_flag = 1;
}
//--------- End of function Trainer::enter ---------//


//--------- Begin of function Trainer::process ---------//
//
void Trainer::process()
{
	if( !lock_resource_flag || !is_available() )
		return;

	Nation* nationPtr = ~nation_array;

	if( nationPtr->cash < LOCK_LEVEL )
		nationPtr->add_cheat( (float)(LOCK_LEVEL - nationPtr->cash) );
	if( nationPtr->food < LOCK_LEVEL )
		nationPtr->add_food( (float)(LOCK_LEVEL - nationPtr->food) );
}
//--------- End of function Trainer::process ---------//


//--------- Begin of function Trainer::option_text ---------//
//
const char* Trainer::option_text(int optionId, char* buf, int bufSize)
{
	const char* onOff;

	switch( optionId )
	{
		case OPT_CASH:
			snprintf(buf, bufSize, _("Add %d treasure"), CASH_STEP);
			return buf;
		case OPT_FOOD:
			snprintf(buf, bufSize, _("Add %d food"), FOOD_STEP);
			return buf;
		case OPT_LOCK:
			onOff = lock_resource_flag ? _("[On]") : _("[Off]");
			snprintf(buf, bufSize, _("Keep treasure and food at %d %s"), LOCK_LEVEL, onOff);
			return buf;
		case OPT_TECH:
			return _("Research all technology and Greater Beings");
		case OPT_MAP:
			return _("Reveal the whole map");
		case OPT_REPUTATION:
			return _("Maximum reputation");
		case OPT_TOWN_LOYALTY:
			return _("Maximum loyalty in all your towns");
		case OPT_UNITS:
			return _("Maximum combat, skill and loyalty for your units");
		case OPT_KING_UNDIE:
			onOff = config.king_undie_flag ? _("[On]") : _("[Off]");
			snprintf(buf, bufSize, _("Immortal king %s"), onOff);
			return buf;
		case OPT_FAST_BUILD:
			onOff = config.fast_build ? _("[On]") : _("[Off]");
			snprintf(buf, bufSize, _("Fast build %s"), onOff);
			return buf;
		case OPT_FIRM_FINISH:
			return _("Finish or repair the selected building");
		case OPT_TOWN_POP:
			return _("Add 10 people to the selected town");
		case OPT_SPY:
			return _("Maximum skill for all your spies");
		case OPT_CLOSE:
			return _("Close (Esc)");
	}
	return "";
}
//--------- End of function Trainer::option_text ---------//


//--------- Begin of function Trainer::disp ---------//
//
void Trainer::disp()
{
	char buf[200];

	vga_util.d3_panel_up( TRAINER_X1, TRAINER_Y1, TRAINER_X2, TRAINER_Y2 );

	font_bible.center_put( TRAINER_X1, TRAINER_Y1+4, TRAINER_X2, TRAINER_Y1+TITLE_HEIGHT-4,
		_("Trainer (F12)") );

	int y = ROW_Y1;

	for( int i=0 ; i<OPTION_COUNT ; i++, y+=ROW_HEIGHT )
	{
		int hover = mouse.in_area( ROW_X1, y, ROW_X2, y+ROW_HEIGHT-3 );

		if( hover )
			vga_util.d3_panel_down( ROW_X1, y, ROW_X2, y+ROW_HEIGHT-3 );
		else
			vga_util.d3_panel_up( ROW_X1, y, ROW_X2, y+ROW_HEIGHT-3 );

		font_san.put( ROW_X1+10, y+(ROW_HEIGHT-2-font_san.height())/2, option_text(i, buf, sizeof(buf)), 0, ROW_X2-4 );
	}

	if( status_msg )
		font_san.center_put( TRAINER_X1, y+2, TRAINER_X2, TRAINER_Y2-4, status_msg );
}
//--------- End of function Trainer::disp ---------//


//--------- Begin of function Trainer::detect ---------//
//
int Trainer::detect()
{
	if( mouse.key_code == KEY_ESC || mouse.key_code == KEY_F12 )
	{
		active_flag = 0;
		return 1;
	}

	int y = ROW_Y1;

	for( int i=0 ; i<OPTION_COUNT ; i++, y+=ROW_HEIGHT )
	{
		if( mouse.single_click( ROW_X1, y, ROW_X2, y+ROW_HEIGHT-3 ) )
		{
			run_option(i);
			return 1;
		}
	}

	// right click or a click outside the window closes it
	if( mouse.any_click(1) ||
		 (mouse.any_click(0) && !mouse.in_area(TRAINER_X1, TRAINER_Y1, TRAINER_X2, TRAINER_Y2)) )
	{
		active_flag = 0;
		return 1;
	}

	return 0;
}
//--------- End of function Trainer::detect ---------//


//--------- Begin of function Trainer::run_option ---------//
//
void Trainer::run_option(int optionId)
{
	int playerRecno = nation_array.player_recno;
	Nation* nationPtr = ~nation_array;
	int i, count=0;

	if( optionId == OPT_CLOSE )
	{
		active_flag = 0;
		return;
	}

	nationPtr->cheat_enabled_flag = 1;

	switch( optionId )
	{
		case OPT_CASH:
			nationPtr->add_cheat( (float)CASH_STEP );
			status_msg = _("Treasure added.");
			break;

		case OPT_FOOD:
			nationPtr->add_food( (float)FOOD_STEP );
			status_msg = _("Food added.");
			break;

		case OPT_LOCK:
			lock_resource_flag = !lock_resource_flag;
			process();
			status_msg = lock_resource_flag ? _("Treasure and food will stay topped up.") : _("Treasure and food are no longer topped up.");
			break;

		case OPT_TECH:
			tech_res.inc_all_tech_level(playerRecno);
			god_res.enable_know_all(playerRecno);
			status_msg = _("All technology researched.");
			break;

		case OPT_MAP:
			world.unveil(0, 0, MAX_WORLD_X_LOC-1, MAX_WORLD_Y_LOC-1);
			world.visit(0, 0, MAX_WORLD_X_LOC-1, MAX_WORLD_Y_LOC-1, 0, 0);
			status_msg = _("The whole map is revealed.");
			break;

		case OPT_REPUTATION:
			nationPtr->reputation = (float) 100;
			status_msg = _("Reputation is at its maximum.");
			break;

		case OPT_TOWN_LOYALTY:
			for( i=town_array.size() ; i>0 ; i-- )
			{
				if( town_array.is_deleted(i) )
					continue;

				Town* townPtr = town_array[i];

				if( townPtr->nation_recno != playerRecno )
					continue;

				for( int r=0 ; r<MAX_RACE ; r++ )
				{
					if( townPtr->race_pop_array[r] )
						townPtr->race_loyalty_array[r] = (float) 100;
				}
				count++;
			}
			status_msg = _("Loyalty raised in your towns.");
			break;

		case OPT_UNITS:
			for( i=unit_array.size() ; i>0 ; i-- )
			{
				if( unit_array.is_deleted(i) )
					continue;

				Unit* unitPtr = unit_array[i];

				if( unitPtr->nation_recno != playerRecno || unitPtr->is_unit_dead() )
					continue;

				if( unitPtr->race_id )
				{
					unitPtr->set_combat_level(100);
					if( unitPtr->skill.skill_id )
						unitPtr->skill.skill_level = 100;
					unitPtr->loyalty = 100;
				}
				unitPtr->hit_points = (float) unitPtr->max_hit_points;
			}
			for( i=firm_array.size() ; i>0 ; i-- )
			{
				if( firm_array.is_deleted(i) )
					continue;

				Firm* firmPtr = firm_array[i];

				if( firmPtr->nation_recno != playerRecno || !firmPtr->worker_array )
					continue;

				for( int w=0 ; w<firmPtr->worker_count ; w++ )
				{
					Worker* workerPtr = firmPtr->worker_array+w;

					if( !workerPtr->race_id )
						continue;

					workerPtr->combat_level = 100;
					if( workerPtr->skill_id )
						workerPtr->skill_level = 100;
					workerPtr->worker_loyalty = 100;
					workerPtr->hit_points = workerPtr->max_hit_points();
				}
			}
			status_msg = _("Your units are at their best.");
			break;

		case OPT_KING_UNDIE:
			config.king_undie_flag = !config.king_undie_flag;
			status_msg = config.king_undie_flag ? _("Your king is now immortal.") : _("King immortal mode is now disabled.");
			break;

		case OPT_FAST_BUILD:
			config.fast_build = !config.fast_build;
			status_msg = config.fast_build ? _("Fast build is now enabled.") : _("Fast build is now disabled.");
			break;

		case OPT_FIRM_FINISH:
			if( firm_array.selected_recno && !firm_array.is_deleted(firm_array.selected_recno) )
			{
				Firm* firmPtr = firm_array[firm_array.selected_recno];

				if( firmPtr->nation_recno == playerRecno )
				{
					firmPtr->hit_points = firmPtr->max_hit_points;
					if( firmPtr->firm_id == FIRM_BASE )
						((FirmBase*)firmPtr)->pray_points = (float) MAX_PRAY_POINTS;
					status_msg = _("The building is finished.");
					break;
				}
			}
			status_msg = _("Select one of your buildings first.");
			break;

		case OPT_TOWN_POP:
			if( town_array.selected_recno && !town_array.is_deleted(town_array.selected_recno) )
			{
				Town* townPtr = town_array[town_array.selected_recno];

				if( townPtr->nation_recno == playerRecno && townPtr->population < MAX_TOWN_POPULATION )
				{
					int raceId = townPtr->majority_race();
					int addCount = MIN(10, MAX_TOWN_POPULATION - townPtr->population);

					townPtr->init_pop( raceId, addCount, 100 );
					townPtr->auto_set_layout();
					status_msg = _("People added to the town.");
					break;
				}
			}
			status_msg = _("Select one of your towns first.");
			break;

		case OPT_SPY:
			for( i=spy_array.size() ; i>0 ; i-- )
			{
				if( spy_array.is_deleted(i) )
					continue;

				Spy* spyPtr = spy_array[i];

				if( spyPtr->true_nation_recno == playerRecno )
				{
					spyPtr->spy_skill = 100;
					spyPtr->spy_loyalty = 100;
				}
			}
			status_msg = _("Your spies are at their best.");
			break;
	}

	info.disp();
}
//--------- End of function Trainer::run_option ---------//

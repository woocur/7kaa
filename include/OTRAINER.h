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

// Filename    : OTRAINER.H
// Description : in-game trainer window (single player only)

#ifndef __OTRAINER_H
#define __OTRAINER_H

class Trainer
{
public:
	enum { OPTION_COUNT = 14 };

	int	active_flag;
	char	lock_resource_flag;	// keep cash and food topped up
	const char* status_msg;

public:
	Trainer();

	int	is_active()		{ return active_flag; }
	int	is_available();
	void	enter();
	void	process();		// called once per game frame
	void	reset();

private:
	void	disp();
	int	detect();
	void	run_option(int optionId);
	const char* option_text(int optionId, char* buf, int bufSize);
};

extern Trainer trainer;

#endif

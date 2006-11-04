/************************************************************************ 
 * This file is part of PDCurses. PDCurses is public domain software;	*
 * you may use it for any purpose. This software is provided AS IS with	*
 * NO WARRANTY whatsoever.						*
 *									*
 * If you use PDCurses in an application, an acknowledgement would be	*
 * appreciated, but is not mandatory. If you make corrections or	*
 * enhancements to PDCurses, please forward them to the current		*
 * maintainer for the benefit of other users.				*
 *									*
 * See the file maintain.er for details of the current maintainer.	*
 ************************************************************************/

#include <curspriv.h>
#include <stdlib.h>
#include <string.h>

RCSID("$Id: refresh.c,v 1.42 2006/11/04 12:59:03 wmcbrine Exp $");

/*man-start**************************************************************

  Name:                                                       refresh

  Synopsis:
	int refresh(void);
	int wrefresh(WINDOW *win);
	int wnoutrefresh(WINDOW *win);
	int doupdate(void);
	int redrawwin(WINDOW *win);
	int wredrawln(WINDOW *win, int beg_line, int num_lines);

  X/Open Description:
	The routine wrefresh() copies the named window to the physical 
	terminal screen, taking into account what is already there in 
	order to optimize cursor movement. The routine refresh() does 
	the same, using stdscr as a default screen. These routines must 
	be called to get any output on the terminal, as other routines 
	only manipulate data structures. Unless leaveok has been 
	enabled, the physical cursor of the terminal is left at the 
	location of the window's cursor.

	The wnoutrefresh() and doupdate() routines allow multiple 
	updates with more efficiency than wrefresh() alone.  In addition 
	to all of the window structures representing the terminal 
	screen: a physical screen, describing what is actually on the 
	screen and a virtual screen, describing what the programmer 
	wants to have on the screen.

	The wrefresh() function works by first calling wnoutrefresh(), 
	which copies the named window to the virtual screen.  It then 
	calls doupdate(), which compares the virtual screen to the 
	physical screen and does the actual update.  If the programmer 
	wishes to output several windows at once, a series of cals to 
	wrefresh() will result in alternating calls to wnoutrefresh() 
	and doupdate(), causing several bursts of output to the screen.  
	By first calling wnoutrefresh() for each window, it is then 
	possible to call doupdate() once.  This results in only one 
	burst of output, with probably fewer total characters 
	transmitted and certainly less CPU time used.

  X/Open Return Value:
	All functions return OK on success and ERR on error.

  Portability				     X/Open    BSD    SYS V
	refresh					Y	Y	Y
	wrefresh				Y	Y	Y
	wnoutrefresh				Y	Y	Y
	doupdate				Y	Y	Y
	redrawwin				Y	-      4.0
	wredrawln				Y	-      4.0

**man-end****************************************************************/

int wnoutrefresh(WINDOW *win)
{
	int first;		/* first changed char on line */
	int last;		/* last changed char on line  */
	int begy, begx;		/* window's place on screen   */
	int i, j;

	PDC_LOG(("wnoutrefresh() - called: win=%p\n", win));

	if ( (win == (WINDOW *)NULL) || (win->_flags & (_PAD|_SUBPAD)) )
		return ERR;

	begy = win->_begy;
	begx = win->_begx;

	for (i = 0, j = begy; i < win->_maxy; i++, j++)
	{
		if (win->_firstch[i] != _NO_CHANGE)
		{
			first = win->_firstch[i];
			last = win->_lastch[i];

			memcpy(&(curscr->_y[j][begx + first]),
				&(win->_y[i][first]),
				(last - first + 1) * sizeof(chtype));

			first += begx; 
			last += begx;

			if (curscr->_firstch[j] != _NO_CHANGE)
				curscr->_firstch[j] =
					min(curscr->_firstch[j], first);
			else
				curscr->_firstch[j] = first;

			curscr->_lastch[j] = max(curscr->_lastch[j], last);

			win->_firstch[i] = _NO_CHANGE;	/* updated now */
		}
		win->_lastch[i] = _NO_CHANGE;		/* updated now */
	}

	if (win->_clear)
		win->_clear = FALSE;

	if (!win->_leaveit)
	{
		curscr->_cury = win->_cury + begy;
		curscr->_curx = win->_curx + begx;
	}

	return OK;
}

int doupdate(void)
{
	int y;

	PDC_LOG(("doupdate() - called\n"));

	if (isendwin())			/* coming back after endwin() called */
	{
		reset_prog_mode();
		curscr->_clear = TRUE;
		SP->alive = TRUE;	/* so isendwin() result is correct */
	}

	if (curscr == (WINDOW *)NULL)
		return ERR;

	for (y = 0; y < SP->lines; y++)
	{
		PDC_LOG(("doupdate() - Transforming line %d of %d: %s\n",
			y, SP->lines, (curscr->_firstch[y] != _NO_CHANGE) ?
			"Yes" : "No"));

		if (curscr->_clear)
		{
			curscr->_firstch[y] = 0;
			curscr->_lastch[y] = COLS - 1;
		}

		if (curscr->_firstch[y] != _NO_CHANGE)
		{
			int x = curscr->_firstch[y];

			/* coordinates, length, starting pointer */

			PDC_transform_line(y, x,
				curscr->_lastch[y] - x + 1,
				curscr->_y[y] + x);

			curscr->_firstch[y] = _NO_CHANGE;
			curscr->_lastch[y] = _NO_CHANGE;
		}
	}

	curscr->_clear = FALSE;

	if (SP->visibility)
		PDC_gotoyx(curscr->_cury, curscr->_curx);

	SP->cursrow = curscr->_cury;
	SP->curscol = curscr->_curx;

	return OK;
}

int wrefresh(WINDOW *win)
{
	bool save_clear;

	PDC_LOG(("wrefresh() - called\n"));

	if ( (win == (WINDOW *)NULL) || (win->_flags & (_PAD|_SUBPAD)) )
		return ERR;

	save_clear = win->_clear;

	if (win == curscr)
		curscr->_clear = TRUE;
	else
		wnoutrefresh(win);

	if (save_clear && win->_maxy == SP->lines && win->_maxx == SP->cols)
		curscr->_clear = TRUE;

	return doupdate();
}

int refresh(void)
{
	PDC_LOG(("refresh() - called\n"));

	return wrefresh(stdscr);
}

int wredrawln(WINDOW *win, int start, int num)
{
	int i;

	PDC_LOG(("wredrawln() - called: win=%p start=%d num=%d\n",
		win, start, num));

	if ((win == (WINDOW *)NULL) ||
	    (start > win->_maxy || start + num > win->_maxy))
		return ERR;

	for (i = start; i < start + num; i++)
	{
		win->_firstch[i] = 0;
		win->_lastch[i] = win->_maxx - 1;
	}

	return OK;
}

int redrawwin(WINDOW *win)
{
	PDC_LOG(("redrawwin() - called: win=%p\n", win));

	if (win == (WINDOW *)NULL)
		return ERR;

	return wredrawln(win, 0, win->_maxy);
}

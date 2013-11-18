/* workspace.c- Workspace management
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef USE_XSHAPE
#include <X11/extensions/shape.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "WindowMaker.h"
#include "framewin.h"
#include "window.h"
#include "icon.h"
#include "misc.h"
#include "menu.h"
#include "application.h"
#include "dock.h"
#include "actions.h"
#include "workspace.h"
#include "appicon.h"
#include "wmspec.h"
#include "xinerama.h"
#include "event.h"

#define MC_NEW          0
#define MC_DESTROY_LAST 1
#define MC_LAST_USED    2
/* index of the first workspace menu entry */
#define MC_WORKSPACE1   3

#define MAX_SHORTCUT_LENGTH 32
#define WORKSPACE_NAME_DISPLAY_PADDING 32


static WMPropList *dWorkspaces = NULL;
static WMPropList *dClip, *dName;

static void make_keys(void)
{
	if (dWorkspaces != NULL)
		return;

	dWorkspaces = WMCreatePLString("Workspaces");
	dName = WMCreatePLString("Name");
	dClip = WMCreatePLString("Clip");
}

void wWorkspaceMake(WScreen * scr, int count)
{
	while (count > 0) {
		wWorkspaceNew(scr);
		count--;
	}
}

int wWorkspaceNew(WScreen *scr)
{
	WWorkspace *wspace, **list;
	int i;

	if (w_global.workspace.count < MAX_WORKSPACES) {
		w_global.workspace.count++;

		wspace = wmalloc(sizeof(WWorkspace));
		wspace->name = NULL;
		wspace->clip = NULL;

		if (!wspace->name) {
			static const char *new_name = NULL;
			static size_t name_length;

			if (new_name == NULL) {
				new_name = _("Workspace %i");
				name_length = strlen(new_name) + 8;
			}
			wspace->name = wmalloc(name_length);
			snprintf(wspace->name, name_length, new_name, w_global.workspace.count);
		}

		if (!wPreferences.flags.noclip)
			wspace->clip = wDockCreate(scr, WM_CLIP, NULL);

		list = wmalloc(sizeof(WWorkspace *) * w_global.workspace.count);

		for (i = 0; i < w_global.workspace.count - 1; i++)
			list[i] = w_global.workspace.array[i];

		list[i] = wspace;
		if (w_global.workspace.array)
			wfree(w_global.workspace.array);

		w_global.workspace.array = list;

		wWorkspaceMenuUpdate(w_global.workspace.menu);
		wWorkspaceMenuUpdate(w_global.clip.ws_menu);
		wNETWMUpdateDesktop(scr);
		WMPostNotificationName(WMNWorkspaceCreated, scr, (void *)(uintptr_t) (w_global.workspace.count - 1));
		XFlush(dpy);

		return w_global.workspace.count - 1;
	}

	return -1;
}

Bool wWorkspaceDelete(WScreen * scr, int workspace)
{
	WWindow *tmp;
	WWorkspace **list;
	int i, j;

	if (workspace <= 0)
		return False;

	/* verify if workspace is in use by some window */
	tmp = scr->focused_window;
	while (tmp) {
		if (!IS_OMNIPRESENT(tmp) && tmp->frame->workspace == workspace)
			return False;
		tmp = tmp->prev;
	}

	if (!wPreferences.flags.noclip) {
		wDockDestroy(w_global.workspace.array[workspace]->clip);
		w_global.workspace.array[workspace]->clip = NULL;
	}

	list = wmalloc(sizeof(WWorkspace *) * (w_global.workspace.count - 1));
	j = 0;
	for (i = 0; i < w_global.workspace.count; i++) {
		if (i != workspace) {
			list[j++] = w_global.workspace.array[i];
		} else {
			if (w_global.workspace.array[i]->name)
				wfree(w_global.workspace.array[i]->name);
			wfree(w_global.workspace.array[i]);
		}
	}
	wfree(w_global.workspace.array);
	w_global.workspace.array = list;

	w_global.workspace.count--;

	/* update menu */
	wWorkspaceMenuUpdate(w_global.workspace.menu);
	/* clip workspace menu */
	wWorkspaceMenuUpdate(w_global.clip.ws_menu);

	/* update also window menu */
	if (w_global.workspace.submenu) {
		WMenu *menu = w_global.workspace.submenu;

		i = menu->entry_no;
		while (i > w_global.workspace.count)
			wMenuRemoveItem(menu, --i);
		wMenuRealize(menu);
	}
	/* and clip menu */
	if (w_global.clip.submenu) {
		WMenu *menu = w_global.clip.submenu;

		i = menu->entry_no;
		while (i > w_global.workspace.count)
			wMenuRemoveItem(menu, --i);
		wMenuRealize(menu);
	}
	wNETWMUpdateDesktop(scr);
	WMPostNotificationName(WMNWorkspaceDestroyed, scr, (void *)(uintptr_t) (w_global.workspace.count - 1));

	if (w_global.workspace.current >= w_global.workspace.count)
		wWorkspaceChange(scr, w_global.workspace.count - 1);
	if (w_global.workspace.last_used >= w_global.workspace.count)
		w_global.workspace.last_used = 0;

	return True;
}

typedef struct WorkspaceNameData {
	int count;
	RImage *back;
	RImage *text;
	time_t timeout;
} WorkspaceNameData;

static void hideWorkspaceName(void *data)
{
	WScreen *scr = (WScreen *) data;

	if (!scr->workspace_name_data || scr->workspace_name_data->count == 0
	    || time(NULL) > scr->workspace_name_data->timeout) {
		XUnmapWindow(dpy, scr->workspace_name);

		if (scr->workspace_name_data) {
			RReleaseImage(scr->workspace_name_data->back);
			RReleaseImage(scr->workspace_name_data->text);
			wfree(scr->workspace_name_data);

			scr->workspace_name_data = NULL;
		}
		scr->workspace_name_timer = NULL;
	} else {
		RImage *img = RCloneImage(scr->workspace_name_data->back);
		Pixmap pix;

		scr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_FADE_DELAY, hideWorkspaceName, scr);

		RCombineImagesWithOpaqueness(img, scr->workspace_name_data->text,
					     scr->workspace_name_data->count * 255 / 10);

		RConvertImage(scr->rcontext, img, &pix);

		RReleaseImage(img);

		XSetWindowBackgroundPixmap(dpy, scr->workspace_name, pix);
		XClearWindow(dpy, scr->workspace_name);
		XFreePixmap(dpy, pix);
		XFlush(dpy);

		scr->workspace_name_data->count--;
	}
}

static void showWorkspaceName(WScreen * scr, int workspace)
{
	WorkspaceNameData *data;
	RXImage *ximg;
	Pixmap text, mask;
	int w, h;
	int px, py;
	char *name = w_global.workspace.array[workspace]->name;
	int len = strlen(name);
	int x, y;
#ifdef USE_XINERAMA
	int head;
	WMRect rect;
	int xx, yy;
#endif

	if (wPreferences.workspace_name_display_position == WD_NONE || w_global.workspace.count < 2)
		return;

	if (scr->workspace_name_timer) {
		WMDeleteTimerHandler(scr->workspace_name_timer);
		XUnmapWindow(dpy, scr->workspace_name);
		XFlush(dpy);
	}
	scr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_DELAY, hideWorkspaceName, scr);

	if (scr->workspace_name_data) {
		RReleaseImage(scr->workspace_name_data->back);
		RReleaseImage(scr->workspace_name_data->text);
		wfree(scr->workspace_name_data);
	}

	data = wmalloc(sizeof(WorkspaceNameData));
	data->back = NULL;

	w = WMWidthOfString(w_global.workspace.font_for_name, name, len);
	h = WMFontHeight(w_global.workspace.font_for_name);

#ifdef USE_XINERAMA
	head = wGetHeadForPointerLocation(scr);
	rect = wGetRectForHead(scr, head);
	if (scr->xine_info.count) {
		xx = rect.pos.x + (scr->xine_info.screens[head].size.width - (w + 4)) / 2;
		yy = rect.pos.y + (scr->xine_info.screens[head].size.height - (h + 4)) / 2;
	}
	else {
		xx = (scr->scr_width - (w + 4)) / 2;
		yy = (scr->scr_height - (h + 4)) / 2;
	}
#endif

	switch (wPreferences.workspace_name_display_position) {
	case WD_TOP:
#ifdef USE_XINERAMA
		px = xx;
#else
		px = (scr->scr_width - (w + 4)) / 2;
#endif
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_BOTTOM:
#ifdef USE_XINERAMA
		px = xx;
#else
		px = (scr->scr_width - (w + 4)) / 2;
#endif
		py = scr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_TOPLEFT:
		px = WORKSPACE_NAME_DISPLAY_PADDING;
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_TOPRIGHT:
		px = scr->scr_width - (w + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_BOTTOMLEFT:
		px = WORKSPACE_NAME_DISPLAY_PADDING;
		py = scr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_BOTTOMRIGHT:
		px = scr->scr_width - (w + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		py = scr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_CENTER:
	default:
#ifdef USE_XINERAMA
		px = xx;
		py = yy;
#else
		px = (scr->scr_width - (w + 4)) / 2;
		py = (scr->scr_height - (h + 4)) / 2;
#endif
		break;
	}
	XResizeWindow(dpy, scr->workspace_name, w + 4, h + 4);
	XMoveWindow(dpy, scr->workspace_name, px, py);

	text = XCreatePixmap(dpy, scr->w_win, w + 4, h + 4, scr->w_depth);
	mask = XCreatePixmap(dpy, scr->w_win, w + 4, h + 4, 1);

	/*XSetForeground(dpy, scr->mono_gc, 0);
	   XFillRectangle(dpy, mask, scr->mono_gc, 0, 0, w+4, h+4); */

	XFillRectangle(dpy, text, WMColorGC(scr->black), 0, 0, w + 4, h + 4);

	for (x = 0; x <= 4; x++)
		for (y = 0; y <= 4; y++)
			WMDrawString(scr->wmscreen, text, scr->white, w_global.workspace.font_for_name, x, y, name, len);

	XSetForeground(dpy, scr->mono_gc, 1);
	XSetBackground(dpy, scr->mono_gc, 0);

	XCopyPlane(dpy, text, mask, scr->mono_gc, 0, 0, w + 4, h + 4, 0, 0, 1 << (scr->w_depth - 1));

	/*XSetForeground(dpy, scr->mono_gc, 1); */
	XSetBackground(dpy, scr->mono_gc, 1);

	XFillRectangle(dpy, text, WMColorGC(scr->black), 0, 0, w + 4, h + 4);

	WMDrawString(scr->wmscreen, text, scr->white, w_global.workspace.font_for_name, 2, 2, name, len);

#ifdef USE_XSHAPE
	if (w_global.xext.shape.supported)
		XShapeCombineMask(dpy, scr->workspace_name, ShapeBounding, 0, 0, mask, ShapeSet);
#endif
	XSetWindowBackgroundPixmap(dpy, scr->workspace_name, text);
	XClearWindow(dpy, scr->workspace_name);

	data->text = RCreateImageFromDrawable(scr->rcontext, text, None);

	XFreePixmap(dpy, text);
	XFreePixmap(dpy, mask);

	if (!data->text) {
		XMapRaised(dpy, scr->workspace_name);
		XFlush(dpy);

		goto erro;
	}

	ximg = RGetXImage(scr->rcontext, scr->root_win, px, py, data->text->width, data->text->height);

	if (!ximg || !ximg->image) {
		goto erro;
	}

	XMapRaised(dpy, scr->workspace_name);
	XFlush(dpy);

	data->back = RCreateImageFromXImage(scr->rcontext, ximg->image, NULL);
	RDestroyXImage(scr->rcontext, ximg);

	if (!data->back) {
		goto erro;
	}

	data->count = 10;

	/* set a timeout for the effect */
	data->timeout = time(NULL) + 2 + (WORKSPACE_NAME_DELAY + WORKSPACE_NAME_FADE_DELAY * data->count) / 1000;

	scr->workspace_name_data = data;

	return;

 erro:
	if (scr->workspace_name_timer)
		WMDeleteTimerHandler(scr->workspace_name_timer);

	if (data->text)
		RReleaseImage(data->text);
	if (data->back)
		RReleaseImage(data->back);
	wfree(data);

	scr->workspace_name_data = NULL;

	scr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_DELAY +
						      10 * WORKSPACE_NAME_FADE_DELAY, hideWorkspaceName, scr);
}

void wWorkspaceChange(WScreen *scr, int workspace)
{
	if (scr->flags.startup || scr->flags.startup2 || scr->flags.ignore_focus_events)
		return;

	if (workspace != w_global.workspace.current)
		wWorkspaceForceChange(scr, workspace);
}

void wWorkspaceRelativeChange(WScreen * scr, int amount)
{
	int w;

	/* While the deiconify animation is going on the window is
	 * still "flying" to its final position and we don't want to
	 * change workspace before the animation finishes, otherwise
	 * the window will land in the new workspace */
	if (w_global.workspace.ignore_change)
		return;

	w = w_global.workspace.current + amount;

	if (amount < 0) {
		if (w >= 0) {
			wWorkspaceChange(scr, w);
		} else if (wPreferences.ws_cycle) {
			wWorkspaceChange(scr, w_global.workspace.count + w);
		}
	} else if (amount > 0) {
		if (w < w_global.workspace.count) {
			wWorkspaceChange(scr, w);
		} else if (wPreferences.ws_advance) {
			wWorkspaceChange(scr, WMIN(w, MAX_WORKSPACES - 1));
		} else if (wPreferences.ws_cycle) {
			wWorkspaceChange(scr, w % w_global.workspace.count);
		}
	}
}

void wWorkspaceForceChange(WScreen * scr, int workspace)
{
	WWindow *tmp, *foc = NULL, *foc2 = NULL;
	WWindow **toUnmap;
	int toUnmapSize, toUnmapCount;

	if (workspace >= MAX_WORKSPACES || workspace < 0)
		return;

	SendHelperMessage(scr, 'C', workspace + 1, NULL);

	if (workspace > w_global.workspace.count - 1)
		wWorkspaceMake(scr, workspace - w_global.workspace.count + 1);

	wClipUpdateForWorkspaceChange(scr, workspace);

	w_global.workspace.last_used = w_global.workspace.current;
	w_global.workspace.current = workspace;

	wWorkspaceMenuUpdate(w_global.workspace.menu);

	wWorkspaceMenuUpdate(w_global.clip.ws_menu);

	toUnmapSize = 16;
	toUnmapCount = 0;
	toUnmap = wmalloc(toUnmapSize * sizeof(WWindow *));

	if ((tmp = scr->focused_window) != NULL) {
		if ((IS_OMNIPRESENT(tmp) && (tmp->flags.mapped || tmp->flags.shaded) &&
		     !WFLAGP(tmp, no_focusable)) || tmp->flags.changing_workspace) {
			foc = tmp;
		}

		/* foc2 = tmp; will fix annoyance with gnome panel
		 * but will create annoyance for every other application
		 */
		while (tmp) {
			if (tmp->frame->workspace != workspace && !tmp->flags.selected) {
				/* unmap windows not on this workspace */
				if ((tmp->flags.mapped || tmp->flags.shaded) &&
				    !IS_OMNIPRESENT(tmp) && !tmp->flags.changing_workspace) {
					if (toUnmapCount == toUnmapSize)
					{
						toUnmapSize *= 2;
						toUnmap = wrealloc(toUnmap, toUnmapSize * sizeof(WWindow *));
					}
					toUnmap[toUnmapCount++] = tmp;
				}
				/* also unmap miniwindows not on this workspace */
				if (!wPreferences.sticky_icons && tmp->flags.miniaturized &&
				    tmp->icon && !IS_OMNIPRESENT(tmp)) {
					XUnmapWindow(dpy, tmp->icon->core->window);
					tmp->icon->mapped = 0;
				}
				/* update current workspace of omnipresent windows */
				if (IS_OMNIPRESENT(tmp)) {
					WApplication *wapp = wApplicationOf(tmp->main_window);

					tmp->frame->workspace = workspace;

					if (wapp) {
						wapp->last_workspace = workspace;
					}
					if (!foc2 && (tmp->flags.mapped || tmp->flags.shaded)) {
						foc2 = tmp;
					}
				}
			} else {
				/* change selected windows' workspace */
				if (tmp->flags.selected) {
					wWindowChangeWorkspace(tmp, workspace);
					if (!tmp->flags.miniaturized && !foc) {
						foc = tmp;
					}
				} else {
					if (!tmp->flags.hidden) {
						if (!(tmp->flags.mapped || tmp->flags.miniaturized)) {
							/* remap windows that are on this workspace */
							wWindowMap(tmp);
							if (!foc && !WFLAGP(tmp, no_focusable)) {
								foc = tmp;
							}
						}
						/* Also map miniwindow if not omnipresent */
						if (!wPreferences.sticky_icons &&
						    tmp->flags.miniaturized && !IS_OMNIPRESENT(tmp) && tmp->icon) {
							tmp->icon->mapped = 1;
							XMapWindow(dpy, tmp->icon->core->window);
						}
					}
				}
			}
			tmp = tmp->prev;
		}

		while (toUnmapCount > 0)
		{
			wWindowUnmap(toUnmap[--toUnmapCount]);
		}
		wfree(toUnmap);

		/* Gobble up events unleashed by our mapping & unmapping.
		 * These may trigger various grab-initiated focus &
		 * crossing events. However, we don't care about them,
		 * and ignore their focus implications altogether to avoid
		 * flicker.
		 */
		scr->flags.ignore_focus_events = 1;
		ProcessPendingEvents();
		scr->flags.ignore_focus_events = 0;

		if (!foc)
			foc = foc2;

		if (scr->focused_window->flags.mapped && !foc) {
			foc = scr->focused_window;
		}
		if (wPreferences.focus_mode == WKF_CLICK) {
			wSetFocusTo(scr, foc);
		} else {
			unsigned int mask;
			int foo;
			Window bar, win;
			WWindow *tmp;

			tmp = NULL;
			if (XQueryPointer(dpy, scr->root_win, &bar, &win, &foo, &foo, &foo, &foo, &mask)) {
				tmp = wWindowFor(win);
			}

			/* If there's a window under the pointer, focus it.
			 * (we ate all other focus events above, so it's
			 * certainly not focused). Otherwise focus last
			 * focused, or the root (depending on sloppiness)
			 */
			if (!tmp && wPreferences.focus_mode == WKF_SLOPPY) {
				wSetFocusTo(scr, foc);
			} else {
				wSetFocusTo(scr, tmp);
			}
		}
	}

	/* We need to always arrange icons when changing workspace, even if
	 * no autoarrange icons, because else the icons in different workspaces
	 * can be superposed.
	 * This can be avoided if appicons are also workspace specific.
	 */
	if (!wPreferences.sticky_icons)
		wArrangeIcons(scr, False);

	if (scr->dock)
		wAppIconPaint(scr->dock->icon_array[0]);

	if (!wPreferences.flags.noclip && (w_global.workspace.array[workspace]->clip->auto_collapse ||
					   w_global.workspace.array[workspace]->clip->auto_raise_lower)) {
		/* to handle enter notify. This will also */
		XUnmapWindow(dpy, w_global.clip.icon->icon->core->window);
		XMapWindow(dpy, w_global.clip.icon->icon->core->window);
	}
	else if (w_global.clip.icon != NULL) {
		wClipIconPaint();
	}
	wScreenUpdateUsableArea(scr);
	wNETWMUpdateDesktop(scr);
	showWorkspaceName(scr, workspace);

	WMPostNotificationName(WMNWorkspaceChanged, scr, (void *)(uintptr_t) workspace);

	/*   XSync(dpy, False); */
}

static void switchWSCommand(WMenu * menu, WMenuEntry * entry)
{
	wWorkspaceChange(menu->frame->screen_ptr, (long)entry->clientdata);
}

static void lastWSCommand(WMenu *menu, WMenuEntry *entry)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;

	wWorkspaceChange(menu->frame->screen_ptr, w_global.workspace.last_used);
}

static void deleteWSCommand(WMenu *menu, WMenuEntry *entry)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;

	wWorkspaceDelete(menu->frame->screen_ptr, w_global.workspace.count - 1);
}

static void newWSCommand(WMenu *menu, WMenuEntry *foo)
{
	int ws;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) foo;

	ws = wWorkspaceNew(menu->frame->screen_ptr);

	/* autochange workspace */
	if (ws >= 0)
		wWorkspaceChange(menu->frame->screen_ptr, ws);
}

void wWorkspaceRename(WScreen *scr, int workspace, const char *name)
{
	char buf[MAX_WORKSPACENAME_WIDTH + 1];
	char *tmp;

	if (workspace >= w_global.workspace.count)
		return;

	/* trim white spaces */
	tmp = wtrimspace(name);

	if (strlen(tmp) == 0) {
		snprintf(buf, sizeof(buf), _("Workspace %i"), workspace + 1);
	} else {
		strncpy(buf, tmp, MAX_WORKSPACENAME_WIDTH);
	}
	buf[MAX_WORKSPACENAME_WIDTH] = 0;
	wfree(tmp);

	/* update workspace */
	wfree(w_global.workspace.array[workspace]->name);
	w_global.workspace.array[workspace]->name = wstrdup(buf);

	if (w_global.clip.ws_menu) {
		if (strcmp(w_global.clip.ws_menu->entries[workspace + MC_WORKSPACE1]->text, buf) != 0) {
			wfree(w_global.clip.ws_menu->entries[workspace + MC_WORKSPACE1]->text);
			w_global.clip.ws_menu->entries[workspace + MC_WORKSPACE1]->text = wstrdup(buf);
			wMenuRealize(w_global.clip.ws_menu);
		}
	}
	if (w_global.workspace.menu) {
		if (strcmp(w_global.workspace.menu->entries[workspace + MC_WORKSPACE1]->text, buf) != 0) {
			wfree(w_global.workspace.menu->entries[workspace + MC_WORKSPACE1]->text);
			w_global.workspace.menu->entries[workspace + MC_WORKSPACE1]->text = wstrdup(buf);
			wMenuRealize(w_global.workspace.menu);
		}
	}

	if (w_global.clip.icon)
		wClipIconPaint();

	WMPostNotificationName(WMNWorkspaceNameChanged, scr, (void *)(uintptr_t) workspace);
}

/* callback for when menu entry is edited */
static void onMenuEntryEdited(WMenu * menu, WMenuEntry * entry)
{
	char *tmp;

	tmp = entry->text;
	wWorkspaceRename(menu->frame->screen_ptr, (long)entry->clientdata, tmp);
}

WMenu *wWorkspaceMenuMake(WScreen * scr, Bool titled)
{
	WMenu *wsmenu;
	WMenuEntry *entry;

	wsmenu = wMenuCreate(scr, titled ? _("Workspaces") : NULL, False);
	if (!wsmenu) {
		wwarning(_("could not create Workspace menu"));
		return NULL;
	}

	/* callback to be called when an entry is edited */
	wsmenu->on_edit = onMenuEntryEdited;

	wMenuAddCallback(wsmenu, _("New"), newWSCommand, NULL);
	wMenuAddCallback(wsmenu, _("Destroy Last"), deleteWSCommand, NULL);

	entry = wMenuAddCallback(wsmenu, _("Last Used"), lastWSCommand, NULL);
	entry->rtext = GetShortcutKey(wKeyBindings[WKBD_LASTWORKSPACE]);

	return wsmenu;
}

void wWorkspaceMenuUpdate(WMenu *menu)
{
	int i;
	long ws;
	char title[MAX_WORKSPACENAME_WIDTH + 1];
	WMenuEntry *entry;
	int tmp;

	if (!menu)
		return;

	if (menu->entry_no < w_global.workspace.count + MC_WORKSPACE1) {
		/* new workspace(s) added */
		i = w_global.workspace.count - (menu->entry_no - MC_WORKSPACE1);
		ws = menu->entry_no - MC_WORKSPACE1;
		while (i > 0) {
			wstrlcpy(title, w_global.workspace.array[ws]->name, MAX_WORKSPACENAME_WIDTH);

			entry = wMenuAddCallback(menu, title, switchWSCommand, (void *)ws);
			entry->flags.indicator = 1;
			entry->flags.editable = 1;

			i--;
			ws++;
		}
	} else if (menu->entry_no > w_global.workspace.count + MC_WORKSPACE1) {
		/* removed workspace(s) */
		for (i = menu->entry_no - 1; i >= w_global.workspace.count + MC_WORKSPACE1; i--)
			wMenuRemoveItem(menu, i);
	}

	for (i = 0; i < w_global.workspace.count; i++) {
		/* workspace shortcut labels */
		if (i / 10 == w_global.workspace.current / 10)
			menu->entries[i + MC_WORKSPACE1]->rtext = GetShortcutKey(wKeyBindings[WKBD_WORKSPACE1 + (i % 10)]);
		else
			menu->entries[i + MC_WORKSPACE1]->rtext = NULL;

		menu->entries[i + MC_WORKSPACE1]->flags.indicator_on = 0;
	}
	menu->entries[w_global.workspace.current + MC_WORKSPACE1]->flags.indicator_on = 1;
	wMenuRealize(menu);

	/* don't let user destroy current workspace */
	if (w_global.workspace.current == w_global.workspace.count - 1)
		wMenuSetEnabled(menu, MC_DESTROY_LAST, False);
	else
		wMenuSetEnabled(menu, MC_DESTROY_LAST, True);

	/* back to last workspace */
	if (w_global.workspace.count && w_global.workspace.last_used != w_global.workspace.current)
		wMenuSetEnabled(menu, MC_LAST_USED, True);
	else
		wMenuSetEnabled(menu, MC_LAST_USED, False);

	tmp = menu->frame->top_width + 5;
	/* if menu got unreachable, bring it to a visible place */
	if (menu->frame_x < tmp - (int)menu->frame->core->width)
		wMenuMove(menu, tmp - (int)menu->frame->core->width, menu->frame_y, False);

	wMenuPaint(menu);
}

void wWorkspaceSaveState(WMPropList * old_state)
{
	WMPropList *parr, *pstr, *wks_state, *old_wks_state, *foo, *bar;
	int i;

	make_keys();

	old_wks_state = WMGetFromPLDictionary(old_state, dWorkspaces);
	parr = WMCreatePLArray(NULL);
	for (i = 0; i < w_global.workspace.count; i++) {
		pstr = WMCreatePLString(w_global.workspace.array[i]->name);
		wks_state = WMCreatePLDictionary(dName, pstr, NULL);
		WMReleasePropList(pstr);
		if (!wPreferences.flags.noclip) {
			pstr = wClipSaveWorkspaceState(i);
			WMPutInPLDictionary(wks_state, dClip, pstr);
			WMReleasePropList(pstr);
		} else if (old_wks_state != NULL) {
			if ((foo = WMGetFromPLArray(old_wks_state, i)) != NULL) {
				if ((bar = WMGetFromPLDictionary(foo, dClip)) != NULL) {
					WMPutInPLDictionary(wks_state, dClip, bar);
				}
			}
		}
		WMAddToPLArray(parr, wks_state);
		WMReleasePropList(wks_state);
	}
	WMPutInPLDictionary(w_global.session_state, dWorkspaces, parr);
	WMReleasePropList(parr);
}

void wWorkspaceRestoreState(WScreen *scr)
{
	WMPropList *parr, *pstr, *wks_state, *clip_state;
	int i, j;

	make_keys();

	if (w_global.session_state == NULL)
		return;

	parr = WMGetFromPLDictionary(w_global.session_state, dWorkspaces);

	if (!parr)
		return;

	for (i = 0; i < WMIN(WMGetPropListItemCount(parr), MAX_WORKSPACES); i++) {
		wks_state = WMGetFromPLArray(parr, i);
		if (WMIsPLDictionary(wks_state))
			pstr = WMGetFromPLDictionary(wks_state, dName);
		else
			pstr = wks_state;

		if (i >= w_global.workspace.count)
			wWorkspaceNew(scr);

		if (w_global.workspace.menu) {
			wfree(w_global.workspace.menu->entries[i + MC_WORKSPACE1]->text);
			w_global.workspace.menu->entries[i + MC_WORKSPACE1]->text = wstrdup(WMGetFromPLString(pstr));
			w_global.workspace.menu->flags.realized = 0;
		}

		wfree(w_global.workspace.array[i]->name);
		w_global.workspace.array[i]->name = wstrdup(WMGetFromPLString(pstr));
		if (!wPreferences.flags.noclip) {
			int added_omnipresent_icons = 0;

			clip_state = WMGetFromPLDictionary(wks_state, dClip);
			if (w_global.workspace.array[i]->clip)
				wDockDestroy(w_global.workspace.array[i]->clip);

			w_global.workspace.array[i]->clip = wDockRestoreState(scr, clip_state, WM_CLIP);
			if (i > 0)
				wDockHideIcons(w_global.workspace.array[i]->clip);

			/* We set the global icons here, because scr->workspaces[i]->clip
			 * was not valid in wDockRestoreState().
			 * There we only set icon->omnipresent to know which icons we
			 * need to set here.
			 */
			for (j = 0; j < w_global.workspace.array[i]->clip->max_icons; j++) {
				WAppIcon *aicon = w_global.workspace.array[i]->clip->icon_array[j];
				int k;

				if (!aicon || !aicon->omnipresent)
					continue;
				aicon->omnipresent = 0;
				if (wClipMakeIconOmnipresent(aicon, True) != WO_SUCCESS)
					continue;
				if (i == 0)
					continue;

				/* Move this appicon from workspace i to workspace 0 */
				w_global.workspace.array[i]->clip->icon_array[j] = NULL;
				w_global.workspace.array[i]->clip->icon_count--;

				added_omnipresent_icons++;
				/* If there are too many omnipresent appicons, we are in trouble */
				assert(w_global.workspace.array[0]->clip->icon_count + added_omnipresent_icons
				       <= w_global.workspace.array[0]->clip->max_icons);
				/* Find first free spot on workspace 0 */
				for (k = 0; k < w_global.workspace.array[0]->clip->max_icons; k++)
					if (w_global.workspace.array[0]->clip->icon_array[k] == NULL)
						break;
				w_global.workspace.array[0]->clip->icon_array[k] = aicon;
				aicon->dock = w_global.workspace.array[0]->clip;
			}
			w_global.workspace.array[0]->clip->icon_count += added_omnipresent_icons;
		}

		WMPostNotificationName(WMNWorkspaceNameChanged, scr, (void *)(uintptr_t) i);
	}
}

/* Returns the workspace number for a given workspace name */
int wGetWorkspaceNumber(const char *value)
{
        int w, i;

	if (sscanf(value, "%i", &w) != 1) {
		w = -1;
		for (i = 0; i < w_global.workspace.count; i++) {
			if (strcmp(w_global.workspace.array[i]->name, value) == 0) {
				w = i;
				break;
			}
		}
	} else {
		w--;
	}

	return w;
}

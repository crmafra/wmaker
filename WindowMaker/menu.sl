/*
 * Root Menu definition for WindowMaker in Slovenian language
 *
 * Syntax is:
 *
 * <Title> [SHORTCUT <Shortcut>] <Command> <Parameters>
 *
 * <Title> is any string to be used as title. Must be enclosed with " if it
 * 	has spaces
 * 
 * SHORTCUT specifies a shortcut for that item. <Shortcut> has the
 * same syntax of the shortcuts key options in the 
 * ~/GNUstep/Defaults/WindowMaker file, such as RootMenuKey or MiniaturizeKey.
 *
 * You can't specify a shortcut for a MENU or OPEN_MENU entry.
 * 
 * <Command> one of the valid commands: 
 *	MENU - starts (sub)menu definition
 *	END  - end (sub)menu definition
 *	OPEN_MENU - opens a menu from a file, pipe or directory(ies) contents
 *		    and eventually precede each with a command.
 *	WORKSPACE_MENU - adds a submenu for workspace operations. Only one
 *		    workspace_menu is allowed. 		
 *	EXEC <program> - executes an external program
 *	EXIT - exits the window manager
 *	RESTART [<window manager>] - restarts WindowMaker or start another
 *			window manager
 *	REFRESH - refreshes the desktop
 *	ARRANGE_ICONS - rearranges the icons on the workspace
 *	SHUTDOWN - kills all clients (and close the X window session)
 *	SHOW_ALL - unhides all windows on workspace
 *	HIDE_OTHERS - hides all windows on the workspace, except the
 *		focused one (or the last one that received focus)
 *	SAVE_SESSION - saves the current state of the desktop, which include
 *		       all running applications, all their hints (geometry,
 *		       position on screen, workspace they live on, the dock
 *		       or clip from where they were launched, and
 *		       if minimized, shaded or hidden. Also saves the current
 *		       workspace the user is on. All will be restored on every
 *		       start of windowmaker until another SAVE_SESSION or
 *		       CLEAR_SESSION is used. If SaveSessionOnExit = Yes; in
 *		       WindowMaker domain file, then saving is automatically
 *		       done on every windowmaker exit, overwriting any
 *		       SAVE_SESSION or CLEAR_SESSION (see below).
 *	CLEAR_SESSION - clears any previous saved session. This will not have
 *		       any effect if SaveSessionOnExit is True.
 *	INFO - shows the Info Panel
 *
 * OPEN_MENU syntax:
 *   1. File menu handling.
 *	// opens file.menu which must contain a valid menu file and inserts
 *	// it in current position
 *	OPEN_MENU file.menu
 *   2. Pipe menu handling.
 *	// opens command and uses it's stdout to construct menu.
 *	// Command's output must be a valid menu description.
 *	// The space between '|' and command itself is optional.
 *	OPEN_MENU | command
 *   3. Directory handling.
 *	// Opens one or more directories and construct a menu with all
 *	// the subdirectories and executable files in them sorted
 *	// alphabetically.
 *	OPEN_MENU /some/dir [/some/other/dir ...]
 *   4. Directory handling with command.
 *	// Opens one or more directories and construct menu with all
 *	// subdirectories and readable files in them sorted alphabetically,
 *	// preceding each of them with command.
 *	OPEN_MENU /some/dir [/some/other/dir ...] WITH command -options
 *
 *
 * <Parameters> is the program to execute.
 *
 * ** Options for command line in EXEC:
 * %s - substitute with current selection
 * %a(message) - opens a input box with the message and do substitution with
 *		what you typed
 * %w - substitute with XID for the current focused window
 *
 * You can override special characters (as % and ") with the \ character:
 * ex: xterm -T "\"Hello World\""
 *
 * You can also use character escapes, like \n
 *
 * Each MENU statement must have one mathching END statement at the end.
 *
 * Example:
 *
 * "Test" MENU
 *	"XTerm" EXEC xterm
 *		// creates a submenu with the contents of /usr/openwin/bin
 *	"XView apps" OPEN_MENU "/usr/openwin/bin"
 *		// some X11 apps in different directories
 *	"X11 apps" OPEN_MENU /usr/X11/bin ~/bin/X11
 *		// set some background images
 *	"Background" OPEN_MENU ~/images /usr/share/images WITH wmsetbg -u -t
 *		// inserts the style.menu in this entry
 *	"Style" OPEN_MENU style.menu
 * "Test" END
 */

#include "wmmacros"

"Aplikacije" MENU
	"Informacije" MENU
		"Informacije o programu..." INFO_PANEL
		"Licenca..." LEGAL_PANEL
		"Sistemska konzola" EXEC xconsole
		"Obremenitev sistema" EXEC xosview || xload
		"Seznam procesov" EXEC xterm -e top
		"Pomo� (Iskalec man strani)" EXEC xman
	"Informacije" END
	"XTerm" EXEC xterm -sb 
	"Rxvt" EXEC rxvt -bg black -fg white -fn fixed
	"Delovni prostori" WORKSPACE_MENU
	"Aplikacije" MENU
		"Grafika" MENU
			"Gimp" EXEC gimp >/dev/null
			"XV" EXEC xv
			"XPaint" EXEC xpaint
			"XFig" EXEC xfig
		"Grafika" END
		"X File Manager" EXEC xfm
		"OffiX Files" EXEC files
		"LyX" EXEC lyx
		"Netscape" EXEC netscape 
  		"Ghostview" EXEC ghostview %a(Enter file to view)
		"Acrobat" EXEC /usr/local/Acrobat3/bin/acroread %a(Enter PDF to view)
  		"TkDesk" EXEC tkdesk
	"Aplikacije" END
	"Urejevalniki besedil" MENU
		"XFte" EXEC xfte
		"XEmacs" EXEC xemacs || emacs
		"XJed" EXEC xjed 
		"NEdit" EXEC nedit
		"Xedit" EXEC xedit
		"VI" EXEC xterm -e vi
	"Urejevalniki besedil" END
	"Drugo" MENU
		"Xmcd" EXEC xmcd 2> /dev/null
		"Xplaycd" EXEC xplaycd
		"Xmixer" EXEC xmixer
	"Drugo" END
	"Uporabi programi" MENU
		"Kalkulator" EXEC xcalc
		"Lastnosti oken" EXEC xprop | xmessage -center -title 'xprop' -file -
		"Pregled pisav" EXEC xfontsel
		"Terminal emulator" EXEC xminicom
		"Povecevalno steklo" EXEC xmag
		"Barvna lestvica" EXEC xcmap
		"XKill" EXEC xkill
		"ASClock" EXEC asclock -shape
		"Odlagali��e (clipboard)" EXEC xclipboard
	"Uporabni programi" END

	"Izbira" MENU
		"Kopiraj" EXEC echo '%s' | wxcopy
		"Po�lji po po�ti" EXEC xterm -name mail -T "Pine" -e pine %s
		"Odpri WEB stran" EXEC netscape %s
		"Poi��i v navodilih" EXEC MANUAL_SEARCH(%s)
	"Izbira" END

	"Delovni prostor" MENU
		"Skrij druge" HIDE_OTHERS
		"Poka�i vse" SHOW_ALL
		"Poravnaj ikone" ARRANGE_ICONS
		"Obnovi" REFRESH
		"Zakleni" EXEC xlock -allowroot -usefirst
		"Shrani session" SAVE_SESSION
		"Izbri�i shranjen session" CLEAR_SESSION
	"Delovni prostor" END

	"Izgled" MENU
		"Teme" OPEN_MENU THEMES_DIR ~/GNUstep/Library/WindowMaker/Themes WITH setstyle
		"Stili" OPEN_MENU STYLES_DIR ~/GNUstep/Library/WindowMaker/Styles WITH setstyle
		"Zbirke ikon" OPEN_MENU ICON_SETS_DIR ~/GNUstep/Library/WindowMaker/IconSets WITH seticons
		"Odzadje" MENU
			"Barva" MENU
                        	"�rna" WS_BACK '(solid, black)'
                        	"Modra"  WS_BACK '(solid, "#505075")'
				"Indigo" WS_BACK '(solid, "#243e6c")'
				"Temno modra" WS_BACK '(solid, "#180090")'
                        	"Violi�na" WS_BACK '(solid, "#554466")'
                        	"P�eni�na"  WS_BACK '(solid, "wheat4")'
                        	"Temno siva"  WS_BACK '(solid, "#333340")'
                        	"Vinsko rde�a" WS_BACK '(solid, "#400020")'
			"Barva" END
			"Preliv" MENU
				"Zastava" WS_BACK '(mdgradient, green, red, white, green)'
				"Nebo" WS_BACK '(vgradient, blue4, white)'
			"Preliv" END
			"Slike" OPEN_MENU BACKGROUNDS_DIR ~/GNUstep/Library/WindowMaker/Backgrounds WITH wmsetbg -u -t
		"Odzadje" END
		"Shrani temo" EXEC getstyle -t ~/GNUstep/Library/WindowMaker/Themes/"%a(Theme name)"
		"Shrani zbirko ikon" EXEC geticonset ~/GNUstep/Library/WindowMaker/IconSets/"%a(IconSet name)"
	"Izgled" END

	"Izhod"	MENU
		"Ponovno za�eni" RESTART
		"Po�eni AfterStep" RESTART afterstep
		"Izhod..."  EXIT
		"Izhod iz session..." SHUTDOWN
	"Izhod" END
"Aplikacije" END

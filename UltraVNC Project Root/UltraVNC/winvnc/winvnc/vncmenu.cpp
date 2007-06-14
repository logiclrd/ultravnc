//  Copyright (C) 2002 Ultr@VNC Team Members. All Rights Reserved.
//  Copyright (C) 2002 RealVNC Ltd. All Rights Reserved.
//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//
//  This file is part of the VNC system.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//
// If the source code for the VNC system is not available from the place 
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.


// vncMenu

// Implementation of a system tray icon & menu for WinVNC

#include "stdhdrs.h"
#include "WinVNC.h"
#include "vncService.h"
#include "vncConnDialog.h"
#include <lmcons.h>
#include <wininet.h>
#include <shlobj.h>

// Header

#include "vncMenu.h"
#include "HideDesktop.h"

// Constants
const UINT MENU_PROPERTIES_SHOW = RegisterWindowMessage("WinVNC.Properties.User.Show");
const UINT MENU_DEFAULT_PROPERTIES_SHOW = RegisterWindowMessage("WinVNC.Properties.Default.Show");
const UINT MENU_ABOUTBOX_SHOW = RegisterWindowMessage("WinVNC.AboutBox.Show");
const UINT MENU_SERVICEHELPER_MSG = RegisterWindowMessage("WinVNC.ServiceHelper.Message");
const UINT MENU_ADD_CLIENT_MSG = RegisterWindowMessage("WinVNC.AddClient.Message");
const UINT MENU_REMOVE_CLIENTS_MSG = RegisterWindowMessage("WinVNC.RemoveClients.Message"); // REalVNc 336
const UINT MENU_AUTO_RECONNECT_MSG = RegisterWindowMessage("WinVNC.AddAutoClient.Message");

const UINT FileTransferSendPacketMessage = RegisterWindowMessage("UltraVNC.Viewer.FileTransferSendPacketMessage");

const char *MENU_CLASS_NAME = "WinVNC Tray Icon";

BOOL g_restore_ActiveDesktop = FALSE;
bool RunningAsAdministrator ();

// sf@2007 - WTS notifications stuff
#define NOTIFY_FOR_THIS_SESSION 0
typedef BOOL (WINAPI *WTSREGISTERSESSIONNOTIFICATION)(HWND, DWORD);
typedef BOOL (WINAPI *WTSUNREGISTERSESSIONNOTIFICATION)(HWND);


static void KillWallpaper()
{
	HideDesktop();
}

static void RestoreWallpaper()
{
  RestoreDesktop();
}

// Implementation

vncMenu::vncMenu(vncServer *server)
{
	hWTSDll = NULL;
	ports_set=false;
    CoInitialize(0);
	

	// Save the server pointer
	m_server = server;

	// Set the initial user name to something sensible...
	vncService::CurrentUser((char *)&m_username, sizeof(m_username));

	// Create a dummy window to handle tray icon messages
	WNDCLASSEX wndclass;

	wndclass.cbSize			= sizeof(wndclass);
	wndclass.style			= 0;
	wndclass.lpfnWndProc	= vncMenu::WndProc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;
	wndclass.hInstance		= hAppInstance;
	wndclass.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName	= (const char *) NULL;
	wndclass.lpszClassName	= MENU_CLASS_NAME;
	wndclass.hIconSm		= LoadIcon(NULL, IDI_APPLICATION);

	RegisterClassEx(&wndclass);

	m_hwnd = CreateWindow(MENU_CLASS_NAME,
				MENU_CLASS_NAME,
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				200, 200,
				NULL,
				NULL,
				hAppInstance,
				NULL);
	if (m_hwnd == NULL)
	{
		PostQuitMessage(0);
		return;
	}

	if (!hWTSDll) hWTSDll = LoadLibrary( ("wtsapi32.dll") );
	if (hWTSDll)
	{
		WTSREGISTERSESSIONNOTIFICATION FunctionWTSRegisterSessionNotification;    
		FunctionWTSRegisterSessionNotification = (WTSREGISTERSESSIONNOTIFICATION)GetProcAddress((HINSTANCE)hWTSDll, "WTSRegisterSessionNotification" );
		if (FunctionWTSRegisterSessionNotification)
			FunctionWTSRegisterSessionNotification( m_hwnd, NOTIFY_FOR_THIS_SESSION );
	}

	// record which client created this window
	SetWindowLong(m_hwnd, GWL_USERDATA, (LONG) this);

	// Ask the server object to notify us of stuff
	server->AddNotify(m_hwnd);

	// Initialise the properties dialog object
	if (!m_properties.Init(m_server))
	{
		PostQuitMessage(0);
		return;
	}
	if (!m_propertiesPoll.Init(m_server))
	{
		PostQuitMessage(0);
		return;
	}

	SetTimer(m_hwnd, 1, 100, NULL);


	// sf@2002
	if (!m_ListDlg.Init(m_server))
	{
		PostQuitMessage(0);
		return;
	}

	// Load the icons for the tray
//	m_winvnc_icon = LoadIcon(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC));
//	m_flash_icon = LoadIcon(hAppInstance, MAKEINTRESOURCE(IDI_FLASH));
	{
		OSVERSIONINFO	osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	GetVersionEx(&osvi);
/*if (osvi.dwPlatformId==VER_PLATFORM_WIN32_NT)
		{
		  if(osvi.dwMajorVersion==5 && osvi.dwMinorVersion>=1)
		  {
			m_winvnc_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
			m_flash_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_FLASH), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
		  }
		  else
		 {
			  m_winvnc_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_VGACOLOR);
			m_flash_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_FLASH), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_VGACOLOR);
		  }
		 }
	else
		 {
			  m_winvnc_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_VGACOLOR);
			m_flash_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_FLASH), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_VGACOLOR);
		  }
	}*/

	if (osvi.dwPlatformId==VER_PLATFORM_WIN32_NT)
		{
		  if(osvi.dwMajorVersion==5 && osvi.dwMinorVersion>=1)
		  {
			m_winvnc_icon=(HICON)LoadImage(NULL, "icon1.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE|LR_DEFAULTCOLOR);
			m_flash_icon=(HICON)LoadImage(NULL, "icon2.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE|LR_DEFAULTCOLOR);

			if (!m_winvnc_icon) m_winvnc_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
			if (!m_flash_icon) m_flash_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_FLASH), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
		  }
		  else
		 {
			  m_winvnc_icon=(HICON)LoadImage(NULL, "icon1.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE|LR_VGACOLOR);
				m_flash_icon=(HICON)LoadImage(NULL, "icon2.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE|LR_VGACOLOR);
			  if (!m_winvnc_icon)m_winvnc_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_VGACOLOR);
			 if (!m_flash_icon)m_flash_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_FLASH), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_VGACOLOR);
		  }
		 }
	else
		 {
				m_winvnc_icon=(HICON)LoadImage(NULL, "icon1.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE|LR_VGACOLOR);
				m_flash_icon=(HICON)LoadImage(NULL, "icon2.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE|LR_VGACOLOR);
				if (!m_winvnc_icon)m_winvnc_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_VGACOLOR);
				if (!m_flash_icon)m_flash_icon=(HICON)LoadImage(hAppInstance, MAKEINTRESOURCE(IDI_FLASH), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_VGACOLOR);
		  }
	}

	// Load the popup menu
	m_hmenu = LoadMenu(hAppInstance, MAKEINTRESOURCE(IDR_TRAYMENU));

	// Install the tray icon!
	AddTrayIcon();
}

vncMenu::~vncMenu()
{
	if (hWTSDll)
	{
		WTSUNREGISTERSESSIONNOTIFICATION FunctionWTSUnRegisterSessionNotification;
		FunctionWTSUnRegisterSessionNotification = (WTSUNREGISTERSESSIONNOTIFICATION)GetProcAddress((HINSTANCE)hWTSDll,"WTSUnRegisterSessionNotification" );
		if (FunctionWTSUnRegisterSessionNotification)
			FunctionWTSUnRegisterSessionNotification( m_hwnd );
		FreeLibrary( hWTSDll );
		hWTSDll = NULL;
	}

	// Remove the tray icon
	DelTrayIcon();
	
	// Destroy the loaded menu
	if (m_hmenu != NULL)
		DestroyMenu(m_hmenu);

	// Tell the server to stop notifying us!
	if (m_server != NULL)
		m_server->RemNotify(m_hwnd);

	if (m_server->RemoveWallpaperEnabled())
		RestoreWallpaper();
}

void
vncMenu::AddTrayIcon()
{
	// If the user name is non-null then we have a user!
	if (strcmp(m_username, "") != 0)
	{
		// Make sure the server has not been configured to
		// suppress the tray icon.
		if ( ! m_server->GetDisableTrayIcon())
		{
			SendTrayMsg(NIM_ADD, FALSE);
		}
	}
}

void
vncMenu::DelTrayIcon()
{
	SendTrayMsg(NIM_DELETE, FALSE);
}

void
vncMenu::FlashTrayIcon(BOOL flash)
{
	SendTrayMsg(NIM_MODIFY, flash);
}

// Get the local ip addresses as a human-readable string.
// If more than one, then with \n between them.
// If not available, then gets a message to that effect.
void GetIPAddrString(char *buffer, int buflen) {
    char namebuf[256];

    if (gethostname(namebuf, 256) != 0) {
		strncpy(buffer, "Host name unavailable", buflen);
		return;
    };

    HOSTENT *ph = gethostbyname(namebuf);
    if (!ph) {
		strncpy(buffer, "IP address unavailable", buflen);
		return;
    };

    *buffer = '\0';
    char digtxt[5];
    for (int i = 0; ph->h_addr_list[i]; i++) {
    	for (int j = 0; j < ph->h_length; j++) {
			sprintf(digtxt, "%d.", (unsigned char) ph->h_addr_list[i][j]);
			strncat(buffer, digtxt, (buflen-1)-strlen(buffer));
		}	
		buffer[strlen(buffer)-1] = '\0';
		if (ph->h_addr_list[i+1] != 0)
			strncat(buffer, ", ", (buflen-1)-strlen(buffer));
    }
}

void
vncMenu::SendTrayMsg(DWORD msg, BOOL flash)
{
	// Create the tray icon message
	m_nid.hWnd = m_hwnd;
	m_nid.cbSize = sizeof(m_nid);
	m_nid.uID = IDI_WINVNC;			// never changes after construction
	m_nid.hIcon = flash ? m_flash_icon : m_winvnc_icon;
	m_nid.uFlags = NIF_ICON | NIF_MESSAGE;
	m_nid.uCallbackMessage = WM_TRAYNOTIFY;
	//vnclog.Print(LL_INTINFO, VNCLOG("SendTRaymesg\n"));

	// Use resource string as tip if there is one
	if (LoadString(hAppInstance, IDI_WINVNC, m_nid.szTip, sizeof(m_nid.szTip)))
	{
	    m_nid.uFlags |= NIF_TIP;
	}
	
	// Try to add the server's IP addresses to the tip string, if possible
	if (m_nid.uFlags & NIF_TIP)
	{
	    strncat(m_nid.szTip, " - ", (sizeof(m_nid.szTip)-1)-strlen(m_nid.szTip));

	    if (m_server->SockConnected())
	    {
		unsigned long tiplen = strlen(m_nid.szTip);
		char *tipptr = ((char *)&m_nid.szTip) + tiplen;

		GetIPAddrString(tipptr, sizeof(m_nid.szTip) - tiplen);
	    }
	    else
	    {
		strncat(m_nid.szTip, "Not listening", (sizeof(m_nid.szTip)-1)-strlen(m_nid.szTip));
	    }
	}

	// Send the message
	if (Shell_NotifyIcon(msg, &m_nid))
	{
		// Set the enabled/disabled state of the menu items
//		vnclog.Print(LL_INTINFO, VNCLOG("tray icon added ok\n"));
			EnableMenuItem(m_hmenu, ID_ADMIN_PROPERTIES,
			m_properties.AllowProperties() && RunningAsAdministrator() ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(m_hmenu, ID_PROPERTIES,
			m_properties.AllowProperties() && RunningAsAdministrator() ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(m_hmenu, ID_CLOSE,
			m_properties.AllowShutdown() ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(m_hmenu, ID_KILLCLIENTS,
			m_properties.AllowEditClients() ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(m_hmenu, ID_OUTGOING_CONN,
			m_properties.AllowEditClients() ? MF_ENABLED : MF_GRAYED);
	} else {
		if (!vncService::RunningAsService())
		{
			if (msg == NIM_ADD)
			{
				// The tray icon couldn't be created, so use the Properties dialog
				// as the main program window
				if (!m_server->RunningAsApplication0()
					&&
					!m_server->RunningAsApplication0System()
					&& 
					!m_server->RunningAsApplication0User()
					) // sf@2007 - Do not display Properties pages when running in Application0 mode
				{
					vnclog.Print(LL_INTINFO, VNCLOG("opening dialog box\n"));
					m_properties.ShowAdmin(TRUE, TRUE);
					PostQuitMessage(0);
				}
			}
		}
	}
}

int counter=0;
// Process window messages
LRESULT CALLBACK vncMenu::WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	// This is a static method, so we don't know which instantiation we're 
	// dealing with. We use Allen Hadden's (ahadden@taratec.com) suggestion 
	// from a newsgroup to get the pseudo-this.
	vncMenu *_this = (vncMenu *) GetWindowLong(hwnd, GWL_USERDATA);
	//	Beep(100,10);
	//	vnclog.Print(LL_INTINFO, VNCLOG("iMsg 0x%x \n"),iMsg);

	switch (iMsg)
	{

	case WM_WTSSESSION_CHANGE:
	{
		switch( wParam )
		{		
			case WTS_SESSION_LOGON:
				vnclog.Print(LL_INTERR, VNCLOG("++++++++++++++++++++++++++++++++++++WTS_CONSOLE_LOGON\n"));
				//_this->m_server->AutoRestartFlag(TRUE);
				//_this->m_server->KillAuthClients();
				//_this->m_server->KillSockConnect();
				//PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			case WTS_SESSION_LOGOFF:
				vnclog.Print(LL_INTERR, VNCLOG("++++++++++++++++++++++++++++++++++++WTS_CONSOLE_LOGOFF\n"));
				//_this->m_server->AutoRestartFlag(TRUE);
				//_this->m_server->KillAuthClients();
				//_this->m_server->KillSockConnect();
				//PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			case WTS_CONSOLE_CONNECT:
				vnclog.Print(LL_INTERR, VNCLOG("++++++++++++++++++++++++++++++++++++WTS_CONSOLE_CONNECT\n"));
				//_this->m_server->AutoRestartFlag(TRUE);
				//_this->m_server->KillAuthClients();
				//_this->m_server->KillSockConnect();
				//PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			case WTS_CONSOLE_DISCONNECT:
				vnclog.Print(LL_INTERR, VNCLOG("WTS_CONSOLE_DISCONNECT\n"));
				//_this->m_server->AutoRestartFlag(TRUE);
				//_this->m_server->KillAuthClients();
				//_this->m_server->KillSockConnect();
				PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			case WTS_SESSION_LOCK:
				vnclog.Print(LL_INTERR, VNCLOG("WTS_SESSION_LOCK\n"));
				//zz_this->m_server->AutoRestartFlag(TRUE);
				//_this->m_server->KillAuthClients();
				//_this->m_server->KillSockConnect();
				//PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			case WTS_SESSION_UNLOCK:
				vnclog.Print(LL_INTERR, VNCLOG("WTS_SESSION_UNLOCK\n"));
				//_this->m_server->AutoRestartFlag(TRUE);
				//_this->m_server->KillAuthClients();
				//_this->m_server->KillSockConnect();
				//PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			default:
				break;
		}
		break;
	}

		// Every five seconds, a timer message causes the icon to update
	case WM_TIMER:
		// sf@2007 - Can't get the WTS_CONSOLE_CONNECT message work properly for now..
		// So use a hack instead
		if (_this->m_server->RunningAsApplication0()
			||
			_this->m_server->RunningAsApplication0System()
			|| 
			_this->m_server->RunningAsApplication0User()
			)
		{
			if (!vncService::InputDesktopSelected())
			{
				_this->m_server->AutoRestartFlag(TRUE);
				_this->m_server->KillAuthClients();
				_this->m_server->KillSockConnect();
				_this->m_server->ShutdownServer();
				PostMessage(hwnd, WM_CLOSE, 0, 0);
			}
		}
		counter++;

		if ( ! _this->m_server->GetDisableTrayIcon() && counter>50)
			{
				counter=0;
				// *** HACK for running servicified
				if (vncService::RunningAsService()) {
					// Attempt to add the icon if it's not already there
					_this->AddTrayIcon();
					// Trigger a check of the current user
					PostMessage(hwnd, WM_USERCHANGED, 0, 0);
				}

				// Update the icon
				_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
			}
		break;

		// DEAL WITH NOTIFICATIONS FROM THE SERVER:
	case WM_SRV_CLIENT_AUTHENTICATED:
	case WM_SRV_CLIENT_DISCONNECT:
		// Adjust the icon accordingly
		_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);

		if (_this->m_server->AuthClientCount() != 0) {
			if (_this->m_server->RemoveWallpaperEnabled())
				KillWallpaper();
		} else {
			if (_this->m_server->RemoveWallpaperEnabled())
				RestoreWallpaper();
		}
		return 0;

		// STANDARD MESSAGE HANDLING
	case WM_CREATE:
		return 0;

	case WM_COMMAND:
		// User has clicked an item on the tray menu
		switch (LOWORD(wParam))
		{
		case ID_ONLINEHELP:
			MessageBox(NULL,"http://ultravnc.sourceforge.net/help.htm","Info",0);
			//ShellExecute(GetDesktopWindow(), "open", "http://ultravnc.sourceforge.net/help.htm", "", 0, SW_SHOWNORMAL);
			break;
		case ID_HOME:
			MessageBox(NULL,"http://ultravnc.sourceforge.net","Info",0);
			//ShellExecute(GetDesktopWindow(), "open", "http://ultravnc.sourceforge.net/index.html", "", 0, SW_SHOWNORMAL);
			break;

		case ID_DEFAULT_PROPERTIES:
			// Show the default properties dialog, unless it is already displayed
			vnclog.Print(LL_INTINFO, VNCLOG("show default properties requested\n"));
			_this->m_properties.ShowAdmin(TRUE, FALSE);
			_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
			break;
		
		case ID_PROPERTIES:
			// Show the properties dialog, unless it is already displayed
			vnclog.Print(LL_INTINFO, VNCLOG("show user properties requested\n"));
			_this->m_propertiesPoll.Show(TRUE, TRUE);
			_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
			break;

        case ID_ADMIN_PROPERTIES:
			// Show the properties dialog, unless it is already displayed
			vnclog.Print(LL_INTINFO, VNCLOG("show user properties requested\n"));
			_this->m_properties.ShowAdmin(TRUE, TRUE);
			_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
			break;
		
		case ID_OUTGOING_CONN:
			// Connect out to a listening VNC viewer
			{
				vncConnDialog *newconn = new vncConnDialog(_this->m_server);
				if (newconn)
				{
					newconn->DoDialog();
					// delete newconn; // NO ! Already done in vncConnDialog.
				}
			}
			break;

		case ID_KILLCLIENTS:
			// Disconnect all currently connected clients
			_this->m_server->KillAuthClients();
			break;

		// sf@2002
		case ID_LISTCLIENTS:
			_this->m_ListDlg.Display();
			break;

		case ID_ABOUT:
			// Show the About box
			_this->m_about.Show(TRUE);
			break;

		case ID_CLOSE:
			// User selected Close from the tray menu
			_this->m_server->KillAuthClients();
			PostMessage(hwnd, WM_CLOSE, 0, 0);
			break;

		}
		return 0;

	case WM_TRAYNOTIFY:
		// User has clicked on the tray icon or the menu
		{
			// Get the submenu to use as a pop-up menu
			HMENU submenu = GetSubMenu(_this->m_hmenu, 0);

			// What event are we responding to, RMB click?
			if (lParam==WM_RBUTTONUP)
			{
				if (submenu == NULL)
				{ 
					vnclog.Print(LL_INTERR, VNCLOG("no submenu available\n"));
					return 0;
				}

				// Make the first menu item the default (bold font)
				SetMenuDefaultItem(submenu, 0, TRUE);
				
				// Get the current cursor position, to display the menu at
				POINT mouse;
				GetCursorPos(&mouse);

				// There's a "bug"
				// (Microsoft calls it a feature) in Windows 95 that requires calling
				// SetForegroundWindow. To find out more, search for Q135788 in MSDN.
				//
				SetForegroundWindow(_this->m_nid.hWnd);

				// Display the menu at the desired position
				TrackPopupMenu(submenu,
						0, mouse.x, mouse.y, 0,
						_this->m_nid.hWnd, NULL);

				return 0;
			}
			
			// Or was there a LMB double click?
			if (lParam==WM_LBUTTONDBLCLK)
			{
				// double click: execute first menu item
				SendMessage(_this->m_nid.hWnd,
							WM_COMMAND, 
							GetMenuItemID(submenu, 0),
							0);
			}

			return 0;
		}
		
	case WM_CLOSE:
		
		// Only accept WM_CLOSE if the logged on user has AllowShutdown set
		if (!_this->m_properties.AllowShutdown())
		{
			return 0;
		}
		// tnatsni Wallpaper fix
		if (_this->m_server->RemoveWallpaperEnabled())
			RestoreWallpaper();
		DestroyWindow(hwnd);
		break;
		
	case WM_DESTROY:
		// The user wants WinVNC to quit cleanly...
		vnclog.Print(LL_INTINFO, VNCLOG("quitting from WM_DESTROY\n"));
		PostQuitMessage(0);
		return 0;
		
	case WM_QUERYENDSESSION:
		_this->m_server->KillAuthClients();
		_this->m_server->KillSockConnect();
		_this->m_server->ShutdownServer();
		DestroyWindow(hwnd);
		vnclog.Print(LL_INTERR, VNCLOG("WM_QUERYENDSESSION\n"));
		break;
		
	case WM_ENDSESSION:
		vnclog.Print(LL_INTERR, VNCLOG("WM_ENDSESSION\n"));
		break;

	case WM_USERCHANGED:
		// The current user may have changed.
		{
			char newuser[UNLEN+1];

			if (vncService::CurrentUser((char *) &newuser, sizeof(newuser)))
			{
				vnclog.Print(LL_INTINFO,
//					VNCLOG("usernames : old=\"%s\", new=\"%s\"\n"),
					_this->m_username, newuser);

				// Check whether the user name has changed!
				if (strcmp(newuser, _this->m_username) != 0)
				{
					vnclog.Print(LL_INTINFO,
						VNCLOG("user name has changed\n"));

					// User has changed!
					strcpy(_this->m_username, newuser);

					// Redraw the tray icon and set it's state
					_this->DelTrayIcon();
					_this->AddTrayIcon();
					_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
					// We should load in the prefs for the new user
					if (_this->m_properties.m_fUseRegistry)
					{
						_this->m_properties.Load(TRUE);
						_this->m_propertiesPoll.Load(TRUE);
					}
					else
					{
						_this->m_properties.LoadFromIniFile();
						_this->m_propertiesPoll.LoadFromIniFile();
					}

				}
			}
		}
		return 0;
	
	default:
		// Deal with any of our custom message types
		if (iMsg == MENU_PROPERTIES_SHOW)
		{
			// External request to show our Properties dialog
			PostMessage(hwnd, WM_COMMAND, MAKELONG(ID_PROPERTIES, 0), 0);
			return 0;
		}
		if (iMsg == MENU_DEFAULT_PROPERTIES_SHOW)
		{
			// External request to show our Properties dialog
			PostMessage(hwnd, WM_COMMAND, MAKELONG(ID_DEFAULT_PROPERTIES, 0), 0);
			return 0;
		}
		if (iMsg == MENU_ABOUTBOX_SHOW)
		{
			// External request to show our About dialog
			PostMessage(hwnd, WM_COMMAND, MAKELONG(ID_ABOUT, 0), 0);
			return 0;
		}
		if (iMsg == MENU_SERVICEHELPER_MSG)
		{
			// External ServiceHelper message.
			// This message holds a process id which we can use to
			// impersonate a specific user.  In doing so, we can load their
			// preferences correctly

			// vncService::ProcessUserHelperMessage(wParam, lParam);
			// Modif Jeremy C.
			if (!_this->m_properties.Lock_service_helper)
			{
			vncService::ProcessUserHelperMessage((WPARAM)&_this->m_server->m_impersonationtoken, lParam); 

			// - Trigger a check of the current user
			PostMessage(hwnd, WM_USERCHANGED, 0, 0);
			}
			return 0;
		}
		
		// wa@2005 -- added support for the AutoReconnectId
		// removed the previous code that used 999,999
		if ( iMsg == MENU_AUTO_RECONNECT_MSG )
		{
			char szId[MAX_PATH] = {0};
			UINT ret = 0;
			if ( lParam != NULL )
			{
				ret = GlobalGetAtomName( (ATOM)lParam, szId, sizeof( szId ) );
				GlobalDeleteAtom( (ATOM)lParam );
			}
			
			_this->m_server->AutoReconnect(true);
			
			if ( ret > 0 )
				_this->m_server->AutoReconnectId(szId);
			
			return 0;
		}


		if (iMsg == MENU_ADD_CLIENT_MSG)
		{
			/*
			// sf@2005 - FTNoUserImpersonation
			// Dirty trick to avoid to add a new MSG... no time
			if (lParam == 998)
			{
				_this->m_server->FTUserImpersonation(false);
				return 0;
			}
			*/

			// Add Client message.  This message includes an IP address
			// of a listening client, to which we should connect.

			// If there is no IP address then show the connection dialog
			if (!lParam) {
				vncConnDialog *newconn = new vncConnDialog(_this->m_server);
				if (newconn)
				{
					newconn->DoDialog();
					// winvnc -connect fixed
					//CHECH memeory leak
					//			delete newconn;
				}
				return 0;
			}

			unsigned short nport = 0;
			char *nameDup = 0;
			char szAdrName[64];
			char szId[MAX_PATH] = {0};
			// sf@2003 - Values are already converted
			if (_this->m_server->AutoReconnect() && strlen(_this->m_server->AutoReconnectAdr()) > 0)
			{
				nport = _this->m_server->AutoReconnectPort();
				strcpy(szAdrName, _this->m_server->AutoReconnectAdr());
			}
			else
			{
				// Get the IP address stringified
				struct in_addr address;
				address.S_un.S_addr = lParam;
				char *name = inet_ntoa(address);
				if (name == 0)
					return 0;
				nameDup = _strdup(name);
				if (nameDup == 0)
					return 0;
				strcpy(szAdrName, nameDup);
				// Free the duplicate name
				if (nameDup != 0) free(nameDup);

				// Get the port number
				nport = (unsigned short)wParam;
				if (nport == 0)
					nport = INCOMING_PORT_OFFSET;
				
			}

			// wa@2005 -- added support for the AutoReconnectId
			// (but it's not required)
			bool bId = ( strlen(_this->m_server->AutoReconnectId() ) > 0);
			if ( bId )
				strcpy( szId, _this->m_server->AutoReconnectId() );
			
			// sf@2003
			// Stores the client adr/ports the first time we try to connect
			// This way we can call this message again later to reconnect with the same values
			if (_this->m_server->AutoReconnect() && strlen(_this->m_server->AutoReconnectAdr()) == 0)
			{
				_this->m_server->AutoReconnectAdr(szAdrName);
				_this->m_server->AutoReconnectPort(nport);
			}

			// Attempt to create a new socket
			VSocket *tmpsock;
			tmpsock = new VSocket;
			if (tmpsock) {

				// Connect out to the specified host on the VNCviewer listen port
				tmpsock->Create();
				if (tmpsock->Connect(szAdrName, nport)) {
					if ( bId )
					{
						// wa@2005 -- added support for the AutoReconnectId
						// Set the ID for this client -- code taken from vncconndialog.cpp (ln:142)
						tmpsock->Send(szId,250);
						tmpsock->SetTimeout(0);
					}
					// Add the new client to this server
					_this->m_server->AddClient(tmpsock, TRUE, TRUE);
				} else {
					delete tmpsock;
					_this->m_server->AutoConnectRetry();
				}
			}
		
			return 0;
		}

		// Process FileTransfer asynchronous Send Packet Message
		if (iMsg == FileTransferSendPacketMessage) 
		{
		  vncClient* pClient = (vncClient*) wParam;
		  if (_this->m_server->IsClient(pClient)) pClient->SendFileChunk();
		}

	}

	// Message not recognised
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}
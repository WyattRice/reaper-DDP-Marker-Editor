#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#else
#include "../WDL/swell/swell.h"
#endif

#include <stdio.h>
#include <math.h>
#include "resource.h"
#include "cdtext.h"

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"
#include "../WDL/wdltypes.h"
#include "../WDL/wdlstring.h"
#include "../WDL/win32_utf8.h"

reaper_plugin_info_t* g_plugin_info = NULL;
REAPER_PLUGIN_HINSTANCE g_hInst; // handle to the dll instance. could be useful for making win32 API calls
HWND g_parent; // global variable that holds the handle to the Reaper main window, useful for various win32 API calls


#ifndef _WIN32
#define MB_ICONWARNING 0
#define MoveWindow(hwnd,x,y,w,h,activate) SetWindowPos(hwnd,NULL,x,y,w,h,SWP_NOZORDER)
#endif

#define SNM_MAX_PATH					2048

#define MARKERS_INI_SECTION			"DDP marker editor"
#define SEEK_PLAY_INI_SECTION		"Seek play"
#define SEEK_PLAY_KEY				"Seek Play"
#define MARKER_COL_TIME				0
#define MARKER_COL_TYPE				1
#define MARKER_COL_ISRC				2
#define MARKER_COL_EAN				3
#define MARKER_COL_TITLE			4
#define MARKER_COL_PERFORMER		5
#define MARKER_COL_SONGWRITER		6
#define MARKER_COL_COMPOSER			7
#define MARKER_COL_ARRANGER			8
#define MARKER_COL_MESSAGE			9
#define MARKER_COL_IDENTIFICATION	10
#define MARKER_COL_GENRE			11
#define MARKER_COL_LANGUAGE			12
#define NUM_MARKER_COLUMNS			13


typedef struct {
	int ID;
	double position;
	char *name;
	char type; // !, # or @
} MarkerData;

bool m_bPlayOnSel;

void SetWndIcon(HWND hwnd)
{
#ifdef _WIN32
	static HICON s_icon = NULL;
	if (!s_icon)
	{
		wchar_t path[SNM_MAX_PATH];
		if (GetModuleFileNameW(NULL, path, sizeof(path) / sizeof(wchar_t)))
			s_icon = ExtractIconW(g_hInst, path, 0); // WM_GETICON isn't working so use this instead
	}

	if (s_icon)
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)s_icon);
#endif
}

void prepareTimeString(char *buffer, double time, char *separator) {
	if (separator == NULL) separator = "";
	time = (ceil(time * 75.0 - 1e-9) + 0.5) / 75.0;
	int frames = ((int)floor(time * 75)) % 75;
	int sec = ((int)floor(time)) % 60;
	int min = ((int)floor(time / 60));
	if (min > 99) min = 99;
	sprintf(buffer, "%02d%s%02d%s%02d", min, separator, sec, separator, frames);
}


void prepareTimeBinary(char *buffer, double time) {
	time = (ceil(time * 75.0 - 1e-9) + 0.5) / 75.0;
	buffer[2] = (char)(((int)floor(time * 75)) % 75);
	buffer[1] = (char)(((int)floor(time)) % 60);
	buffer[0] = (char)(((int)floor(time / 60)) & 255);
}


void padString(char *str, int size) {
	int len = strlen(str);
	for (int i = len; i < size; i++) str[i] = ' ';
	str[size] = '\0';
}
char *getNextToken(char **str, char separator) {
	if ((str == NULL) || (*str == NULL) || (**str == '\0')) return NULL;
	bool isQuote = false;
	char *start = *str;
	while (**str != '\0') {
		char ch = **str;
		if (ch == '\"') isQuote = !isQuote;
		if (!isQuote && (ch == separator)) {
			**str = '\0';
			if ((*str > start + 1) && (*start == '\"') && (*(*str - 1) == '\"')) { // remove if all quoted
				start++;
				*(*str - 1) = '\0';
			}
			(*str)++;
			return start;
		}
		(*str)++;
	}
	return start;
}


// break token into key and value, by splitting at equal sign
bool getKeyValuePair(char *token, char **key, char **value) {
	if ((token == NULL) || (key == NULL) || (value == NULL)) return false;
	char *eqPos = strchr(token, '=');
	if (eqPos == NULL) return false;
	*key = token;
	*value = eqPos + 1;
	*eqPos = '\0';

	// trim spaces
	while (**key == ' ') (*key)++;
	while (**value == ' ') (*value)++;
	int len = strlen(*key);
	while ((len > 0) && ((*key)[len - 1] == ' ')) len--;
	(*key)[len] = '\0';
	len = strlen(*value);
	while ((len > 0) && ((*value)[len - 1] == ' ')) len--;
	(*value)[len] = '\0';
	return true;
}



HWND hParentWnd, hEditMarkersDlg = NULL, hMarkerListWnd = NULL;
HMENU hEditMenu = NULL;
MarkerData *pMarkerList = NULL;
int numMarkers = 0;
int editMarkersRegisteredCommand = 0;
int editMarkersMenuPos = 0;
const char *editMarkersAction = "DDP marker editor";
gaccel_register_t editMarkersAccelerator = {
	{FCONTROL | FALT | FVIRTKEY, 'D', 0},
	editMarkersAction,
};

char *getNextToken(char **str, char separator);
bool getKeyValuePair(char *token, char **key, char **value);
void prepareTimeString(char *buffer, double time, char *separator);




char *markerColumnNameList[NUM_MARKER_COLUMNS] = {
	"Time", // MARKER_COL_TIME
	"Type", // MARKER_COL_TYPE
	"ISRC", // MARKER_COL_ISRC
	"UPC/EAN", // MARKER_COL_EAN
	"Title", // MARKER_COL_TITLE
	"Performer", // MARKER_COL_PERFORMER
	"Songwriter", // MARKER_COL_SONGWRITER
	"Composer", // MARKER_COL_COMPOSER
	"Arranger", // MARKER_COL_ARRANGER
	"Message", // MARKER_COL_MESSAGE
	"Identification", // MARKER_COL_IDENTIFICATION
	"Genre", // MARKER_COL_GENRE
	"Language", // MARKER_COL_LANGUAGE
};


int markerColumnWidthList[NUM_MARKER_COLUMNS] = {
	60, // MARKER_COL_TIME
	60, // MARKER_COL_TYPE
	120, // MARKER_COL_ISRC
	120, // MARKER_COL_EAN
	200, // MARKER_COL_TITLE
	200, // MARKER_COL_PERFORMER
	200, // MARKER_COL_SONGWRITER
	200, // MARKER_COL_COMPOSER
	200, // MARKER_COL_ARRANGER
	200, // MARKER_COL_MESSAGE
	200, // MARKER_COL_IDENTIFICATION
	120, // MARKER_COL_GENRE
	120, // MARKER_COL_LANGUAGE
};




void writePrivateProfileInt(const char *section, const char *key, int value, const char *configFileName) {
	char valueStr[32];
	snprintf(valueStr, sizeof(valueStr), "%d", value);
	WritePrivateProfileString(section, key, valueStr, configFileName);
}


void loadConfiguration(void) {
	if (GetResourcePath() == NULL) return;
	char configFileName[1024];
	const char* inifilepath = GetResourcePath();
	sprintf(configFileName, "%s//ddpEdit.ini", inifilepath);
	int winX = GetPrivateProfileInt(MARKERS_INI_SECTION, "x", -1000, configFileName);
	int winY = GetPrivateProfileInt(MARKERS_INI_SECTION, "y", -1000, configFileName);
	int winW = GetPrivateProfileInt(MARKERS_INI_SECTION, "w", 640, configFileName);
	int winH = GetPrivateProfileInt(MARKERS_INI_SECTION, "h", 480, configFileName);
	if (winW < 0) winW = 640;
	if (winH < 0) winH = 480;
	if (winX < 0) winX = (GetSystemMetrics(SM_CXSCREEN) - winW) >> 1;
	if (winY < 0) winY = (GetSystemMetrics(SM_CYSCREEN) - winH) >> 1;
	MoveWindow(hEditMarkersDlg, winX, winY, winW, winH, 1);

	int order[NUM_MARKER_COLUMNS];
	for (int colIndex = 0; colIndex < NUM_MARKER_COLUMNS; ++colIndex) {
		char key[32];

		sprintf(key, "col%d", colIndex);
		const int width = GetPrivateProfileInt(MARKERS_INI_SECTION, key,
			markerColumnWidthList[colIndex], configFileName);
		ListView_SetColumnWidth(hMarkerListWnd, colIndex, width);

		sprintf(key, "order%d", colIndex);
		order[colIndex] = GetPrivateProfileInt(MARKERS_INI_SECTION, key, colIndex, configFileName);
	}

	ListView_SetColumnOrderArray(hMarkerListWnd, NUM_MARKER_COLUMNS, order);
}


void saveConfiguration(void) {
	if (GetResourcePath() == NULL) return;
	char configFileName[1024];
	const char* inifilepath = GetResourcePath();
	sprintf(configFileName, "%s//ddpEdit.ini", inifilepath);
	RECT R;
	GetWindowRect(hEditMarkersDlg, &R);
	writePrivateProfileInt(MARKERS_INI_SECTION, "x", R.left, configFileName);
	writePrivateProfileInt(MARKERS_INI_SECTION, "y", R.top, configFileName);
	writePrivateProfileInt(MARKERS_INI_SECTION, "w", R.right - R.left, configFileName);
	writePrivateProfileInt(MARKERS_INI_SECTION, "h", R.bottom - R.top, configFileName);

	for (int colIndex = 0; colIndex < NUM_MARKER_COLUMNS; colIndex++) {
		int cx = ListView_GetColumnWidth(hMarkerListWnd, colIndex);
		char key[32];
		sprintf(key, "col%d", colIndex);
		writePrivateProfileInt(MARKERS_INI_SECTION, key, cx, configFileName);
	}
	for (int colIndex = 0; colIndex < NUM_MARKER_COLUMNS; colIndex++) {
		int order[NUM_MARKER_COLUMNS];
		ListView_GetColumnOrderArray(hMarkerListWnd, NUM_MARKER_COLUMNS, order);
		char key[32];
		sprintf(key, "order%d", colIndex);
		writePrivateProfileInt(MARKERS_INI_SECTION, key, order[colIndex], configFileName);
	}
}


void freeMarkerList(void) {
	if (pMarkerList == NULL) return;
	for (int i = 0; i < numMarkers; i++) {
		if (pMarkerList[i].name) free(pMarkerList[i].name);
	}
	free(pMarkerList);
	pMarkerList = NULL;
	numMarkers = 0;
}


bool insertMarkerToList(int index, int ID, double position, const char *name, char type) {
	if ((index < 0) || (index > numMarkers)) return false;
	MarkerData *pTempList = (MarkerData *)realloc(pMarkerList, (numMarkers + 1) * sizeof(MarkerData));
	if (pTempList == NULL) return false;
	pMarkerList = pTempList;
	for (int i = index; i < numMarkers; i++) {
		memcpy(pMarkerList + index + 1, pMarkerList + index, sizeof(MarkerData));
	}
	pMarkerList[index].ID = ID;
	pMarkerList[index].position = position;
	pMarkerList[index].name = strdup(name);
	pMarkerList[index].type = type;
	numMarkers++;
	return true;
}


bool updateMarkerInList(int index, int ID, double position, const char *name, char type) {
	if ((index < 0) || (index >= numMarkers)) return false;
	if (pMarkerList[index].name) free(pMarkerList[index].name);
	pMarkerList[index].ID = ID;
	pMarkerList[index].position = position;
	pMarkerList[index].name = strdup(name);
	pMarkerList[index].type = type;
	return true;
}


bool deleteMarkerFromList(int index) {
	if ((index < 0) || (index >= numMarkers) || (pMarkerList == NULL)) return false;
	if (pMarkerList[index].name) free(pMarkerList[index].name);
	for (int i = index + 1; i < numMarkers; i++) {
		memcpy(pMarkerList + i - 1, pMarkerList + i, sizeof(MarkerData));
	}
	pMarkerList = (MarkerData *)realloc(pMarkerList, (numMarkers - 1) * sizeof(MarkerData));
	numMarkers--;
	return true;
}


bool checkMarkerIfEqual(int index, int ID, double position, const char *name, char type) {
	if ((index < 0) || (index >= numMarkers) || (pMarkerList == NULL)) return false;
	if ((pMarkerList[index].name == NULL) || (name == NULL)) return false;
	if ((strcmp(pMarkerList[index].name, name) != 0) || (pMarkerList[index].position != position)) return false;
	if ((pMarkerList[index].ID != ID) || (pMarkerList[index].type != type)) return false;
	return true;
}


void updateMarkerList(HWND hListWnd) {
	int i;
	int markerID = 0;
	int markerSerial = 0;
	int markerIndex = 0;
	bool markerIsRegion;
	double markerPosition, markerRegionEnd;
	const char *markerName;

	while ((markerSerial = EnumProjectMarkers(markerSerial, &markerIsRegion, &markerPosition, &markerRegionEnd, &markerName, &markerID)) != 0) {
		char markerType = markerName[0];
		if (!markerIsRegion && ((markerType == '!') || (markerType == '@') || (markerType == '#'))) {
			if (!checkMarkerIfEqual(markerIndex, markerID, markerPosition, markerName, markerType)) {
				char valueText[1024];
				LVITEM lvI;
				lvI.mask = LVIF_TEXT;
				lvI.pszText = valueText;
				lvI.iItem = markerIndex;

				prepareTimeString(valueText, markerPosition, ":");
				lvI.iSubItem = 0;

				if (markerIndex >= numMarkers) {
					ListView_InsertItem(hListWnd, &lvI);
					insertMarkerToList(markerIndex, markerID, markerPosition, markerName, markerType);
				}
				else {
					ListView_SetItem(hListWnd, &lvI);
					updateMarkerInList(markerIndex, markerID, markerPosition, markerName, markerType);

					for (i = 1; i < NUM_MARKER_COLUMNS; i++) {
						lvI.iSubItem = i;
						valueText[0] = '\0';
						ListView_SetItem(hListWnd, &lvI);
					}
				}

				switch (markerType) {
				case '!': strcpy(valueText, "INDEX0"); break;
				case '@': strcpy(valueText, "ALBUM"); break;
				case '#': strcpy(valueText, "INDEX1"); break;
				}
				lvI.iSubItem = MARKER_COL_TYPE;
				ListView_SetItem(hListWnd, &lvI);

				if ((markerType == '@') || (markerType == '#')) { // @ = global album data, # = track data
					char nameCopy[4096];
					lstrcpyn(nameCopy, markerName + 1, sizeof(nameCopy));
					char *token, *key, *value, *pName = nameCopy;
					while ((token = getNextToken(&pName, '|')) != NULL) {
						if (getKeyValuePair(token, &key, &value)) {
							lstrcpyn(valueText, value, sizeof(valueText));
							if (markerType == '@') { // album data
								if ((stricmp(key, "CATALOG") == 0) || (stricmp(key, "EAN") == 0) || (stricmp(key, "UPC") == 0)) {
									lvI.iSubItem = MARKER_COL_EAN;
									ListView_SetItem(hListWnd, &lvI);
								}
								if (stricmp(key, "ALBUM") == 0) {
									lvI.iSubItem = MARKER_COL_TITLE;
									ListView_SetItem(hListWnd, &lvI);
								}

								if (stricmp(key, "LANGUAGE") == 0) {
									for (i = 0; i < NUM_CDTEXT_LANGUAGES; i++) {
										if (stricmp(value, CDTEXTLanguageList[i]) == 0) {
											lvI.iSubItem = MARKER_COL_LANGUAGE;
											ListView_SetItem(hListWnd, &lvI);
											break;
										}
									}
								}

								for (i = 0; i < MAX_ALBUM_CDTEXT; i++) {
									if (stricmp(key, CDTEXTKeyList[i]) == 0) {
										lvI.iSubItem = MARKER_COL_TITLE + i;
										ListView_SetItem(hListWnd, &lvI);
										break;
									}
								}
							}
							if (markerType == '#') { // track data
								if (stricmp(key, "ISRC") == 0) {
									lvI.iSubItem = MARKER_COL_ISRC;
									ListView_SetItem(hListWnd, &lvI);
								}

								for (i = 0; i < MAX_TRACK_CDTEXT; i++) {
									if (stricmp(key, CDTEXTKeyList[i]) == 0) {
										lvI.iSubItem = MARKER_COL_TITLE + i;
										ListView_SetItem(hListWnd, &lvI);
										break;
									}
								}
							}
						}
						else { // plain token without equal sign, interpreted as (album/track) title 
							lstrcpyn(valueText, token, sizeof(valueText));
							lvI.iSubItem = MARKER_COL_TITLE;
							ListView_SetItem(hListWnd, &lvI);
						}
					}
				}
			}
			markerIndex++;
		}
	}

	while (markerIndex < numMarkers) {
		ListView_DeleteItem(hListWnd, numMarkers - 1);
		deleteMarkerFromList(numMarkers - 1);
	}
}


void appendMarkerName(char *name, int nameSize, int *nameOfs, char *key, char *value) {
	if (value[0]) {
		if (*nameOfs > 1) strcat(name, "|");
		if ((*nameOfs += strlen(value) + (key ? strlen(key) + 1 : 0)) < nameSize - 4) {
			if (key) {
				strcat(name, key);
				strcat(name, "=");
			}
			strcat(name, value);
		}
	}
}


WDL_DLGRET editSingleMarkerDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	char valueText[1024];

	switch (uMsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		int itemIndex = (int)(lParam & 0x00FFFFFF);
		char markerType = (char)((lParam >> 24) & 0xff);
		if (itemIndex == 0xFFFFFF)
		{
			if (markerType == '@') SetWindowText(hwndDlg, "Insert album marker");
			else if (markerType == '#') SetWindowText(hwndDlg, "Insert INDEX1 marker");
			else if (markerType == '!') SetWindowText(hwndDlg, "Insert INDEX0 marker");
		}
		MarkerData *pMarkerData = (pMarkerList && (itemIndex >= 0) && (itemIndex < numMarkers)) ? pMarkerList + itemIndex : NULL;

		for (int genreIndex = 1; genreIndex <= NUM_CDTEXT_GENRES; genreIndex++) {
			SendDlgItemMessage(hwndDlg, 100 + CDTEXT_GENRE, CB_ADDSTRING, 0, (LPARAM)CDTEXTGenreList[genreIndex]);
		}

		SendDlgItemMessage(hwndDlg, 120, CB_ADDSTRING, 0, (LPARAM) "");
		for (int langIndex = 0; langIndex < NUM_CDTEXT_LANGUAGES; langIndex++) {
			if (CDTEXTLanguageList[langIndex][0]) {
				SendDlgItemMessage(hwndDlg, 120, CB_ADDSTRING, 0, (LPARAM)CDTEXTLanguageList[langIndex]);
			}
		}

		char timeValue[3];
		prepareTimeBinary(timeValue, pMarkerData ? pMarkerData->position : GetCursorPosition());
		SetDlgItemInt(hwndDlg, 200, timeValue[0], 0);
		SetDlgItemInt(hwndDlg, 201, timeValue[1], 0);
		SetDlgItemInt(hwndDlg, 202, timeValue[2], 0);

		if (pMarkerData) {
			for (int colIndex = MARKER_COL_TITLE; colIndex <= MARKER_COL_GENRE; colIndex++) {
				ListView_GetItemText(hMarkerListWnd, itemIndex, colIndex, valueText, sizeof(valueText) - 1);
#ifdef _WIN32
				SendDlgItemMessage(hwndDlg, 100 + colIndex - MARKER_COL_TITLE, EM_LIMITTEXT, 255, 0);
#endif
				SetDlgItemText(hwndDlg, 100 + colIndex - MARKER_COL_TITLE, valueText);
			}

			if (markerType == '@') {
#ifdef _WIN32
				SendDlgItemMessage(hwndDlg, 121, EM_LIMITTEXT, 13, 0);
#endif
				ListView_GetItemText(hMarkerListWnd, itemIndex, MARKER_COL_EAN, valueText, sizeof(valueText) - 1);
				SetDlgItemText(hwndDlg, 121, valueText);
				ListView_GetItemText(hMarkerListWnd, itemIndex, MARKER_COL_LANGUAGE, valueText, sizeof(valueText) - 1);
				int index = SendDlgItemMessage(hwndDlg, 120, CB_FINDSTRING, 0, (LPARAM)valueText);
				SendDlgItemMessage(hwndDlg, 120, CB_SETCURSEL, index, 0);
			}
			if (markerType == '#') {
#ifdef _WIN32
				SendDlgItemMessage(hwndDlg, 122, EM_LIMITTEXT, 12, 0);
#endif
				ListView_GetItemText(hMarkerListWnd, itemIndex, MARKER_COL_ISRC, valueText, sizeof(valueText) - 1);
				SetDlgItemText(hwndDlg, 122, valueText);
			}
		}
	}; return 0;

	case WM_COMMAND: {
		if (LOWORD(wParam) == IDCANCEL) EndDialog(hwndDlg, IDCANCEL);
		else if (LOWORD(wParam) == IDOK) {
			LPARAM lParam = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
			int itemIndex = (int)(lParam & 0x00FFFFFF);
			char markerType = lParam >> 24;
			char name[4096];
			name[0] = markerType;
			name[1] = '\0';

			int min = GetDlgItemInt(hwndDlg, 200, NULL, 0);
			int sec = GetDlgItemInt(hwndDlg, 201, NULL, 0);
			int frame = GetDlgItemInt(hwndDlg, 202, NULL, 0);
			if (min > 99) min = 99;
			if (sec > 59) sec = 59;
			if (frame > 74) frame = 74;
			double pos = min * 60 + sec + frame / 75.0;

			int nameOfs = 1;
			if ((markerType == '@') || (markerType == '#')) {
				for (int colIndex = MARKER_COL_TITLE; colIndex <= MARKER_COL_GENRE; colIndex++) {
					GetDlgItemText(hwndDlg, 100 + colIndex - MARKER_COL_TITLE, valueText, sizeof(valueText) - 1);
					char *key = (colIndex == MARKER_COL_TITLE) ? NULL : CDTEXTKeyList[colIndex - MARKER_COL_TITLE];
					appendMarkerName(name, sizeof(name), &nameOfs, key, valueText);
				}
			}
			if (markerType == '@') {
				int langIndex = SendDlgItemMessage(hwndDlg, 120, CB_GETCURSEL, 0, 0);
				SendDlgItemMessage(hwndDlg, 120, CB_GETLBTEXT, langIndex, (LPARAM)valueText);
				appendMarkerName(name, sizeof(name), &nameOfs, "LANGUAGE", valueText);

				GetDlgItemText(hwndDlg, 121, valueText, sizeof(valueText) - 1);
				appendMarkerName(name, sizeof(name), &nameOfs, "EAN", valueText);
			}
			else if (markerType == '#') {
				GetDlgItemText(hwndDlg, 122, valueText, sizeof(valueText) - 1);
				appendMarkerName(name, sizeof(name), &nameOfs, "ISRC", valueText);
			}

			Undo_BeginBlock();
			const char *msg;
			if (itemIndex >= numMarkers)
			{
				AddProjectMarker(NULL, false, pos, 0, name, -1);
				msg = "Add marker";
			}
			else
			{
				int markerID = pMarkerList[itemIndex].ID;
				SetProjectMarker(markerID, false, pos, 0, name);
				msg = "Edit marker";
			}

			Undo_EndBlock(msg, UNDO_STATE_MISCCFG);
			updateMarkerList(hMarkerListWnd);

			EndDialog(hwndDlg, IDOK);
		}
	}; return 0;
	}
	return 0;
}


WDL_DLGRET editMarkersDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static int s_ignore_listview_messages;
	switch (uMsg) {
	case WM_INITDIALOG: {
		LVCOLUMN lvc;

		hEditMarkersDlg = hwndDlg;
		SetWndIcon(hwndDlg);
		CheckMenuItem(hEditMenu, editMarkersRegisteredCommand, MF_CHECKED);
		char cOptions[10];
		char configFileName[1024];
		const char* inifilepath = GetResourcePath();
		sprintf(configFileName, "%s//ddpEdit.ini", inifilepath);
		GetPrivateProfileString(SEEK_PLAY_INI_SECTION, SEEK_PLAY_KEY, "1 1", cOptions, 10, configFileName);
		m_bPlayOnSel = (cOptions[0] == '1');

		CheckDlgButton(hwndDlg, IDC_PLAY, m_bPlayOnSel ? BST_CHECKED : BST_UNCHECKED);

		hMarkerListWnd = GetDlgItem(hwndDlg, IDC_MARKERS_LIST);
		//From SWS Extensions Thanks!
		int lvstyle = LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
#ifdef _WIN32
		lvstyle |= LVS_EX_DOUBLEBUFFER;
#else
		// note for osx: style can be overrided in SWS_DockWnd::WndProc() for optional grid line theming
#endif
		ListView_SetExtendedListViewStyleEx(hMarkerListWnd, lvstyle, lvstyle);

#ifdef _WIN32
		// Setup UTF-8 (see http://forum.cockos.com/showthread.php?t=101547)
		WDL_UTF8_HookListView(hMarkerListWnd);
#if !defined(WDL_NO_SUPPORT_UTF8)
#ifdef WDL_SUPPORT_WIN9X
		if (GetVersion() < 0x80000000)
#endif
			SendMessage(hMarkerListWnd, LVM_SETUNICODEFORMAT, 1, 0);
#endif
#endif

		lvc.mask = LVCF_WIDTH | LVCF_TEXT;
		for (int colIndex = 0; colIndex < NUM_MARKER_COLUMNS; colIndex++) {
			lvc.fmt = LVCFMT_LEFT;
			lvc.pszText = markerColumnNameList[colIndex];
			lvc.cx = markerColumnWidthList[colIndex];
			ListView_InsertColumn(hMarkerListWnd, colIndex, &lvc);
		}

		updateMarkerList(hMarkerListWnd);
		SetTimer(hwndDlg, 0, 500, NULL);

		loadConfiguration();
	}; return 0;

	case WM_TIMER: {
		updateMarkerList(hMarkerListWnd);
		double editPosition = GetCursorPosition();
		static double lastEditPosition = -1;
		if (editPosition != lastEditPosition) {
			for (int i = 0; i < numMarkers; i++) {
				if (pMarkerList[i].position == editPosition) {
					s_ignore_listview_messages++;
					ListView_SetItemState(hMarkerListWnd, i, LVIS_SELECTED, LVIS_SELECTED);
					s_ignore_listview_messages--;
					break;
				}
			}
			lastEditPosition = editPosition;
		}
	}; return 0;

	case WM_SIZE: {
		RECT r;
		GetClientRect(hwndDlg, &r);
		int w = r.right;
		int h = r.bottom;
		MoveWindow(hMarkerListWnd, 3, 3, w - 6, h - 25 - 6 - 6, true);
		int w2 = 200;
		if (w2 > w - 100 - 6) w2 = w - 100 - 6;
		MoveWindow(GetDlgItem(hwndDlg, IDC_PLAY_BUTTON), 3, h - 25 - 3, 80, 25, true);
		MoveWindow(GetDlgItem(hwndDlg, IDC_PLAY), 90, h - 25 - 3, 70, 25, true);
		MoveWindow(GetDlgItem(hwndDlg, IDCANCEL), w - 80 - 3, h - 25 - 3, 80, 25, true);

		//InvalidateRect(hwndDlg, &r, false);
		//UpdateWindow(hwndDlg);

	}; return 0;

	case WM_GETMINMAXINFO: {
		MINMAXINFO* mmi = (MINMAXINFO*)lParam;
		mmi->ptMinTrackSize.x = 260; //width
		mmi->ptMinTrackSize.y = 130; //height

		return 0;
	}

	case WM_NOTIFY: {
		NMHDR *pHdr = (NMHDR *)lParam;
		if (pHdr->code == LVN_ITEMCHANGED) {
			NMLISTVIEW *pListData = (NMLISTVIEW *)pHdr;
#ifdef _WIN32
			if ((pListData->uNewState & LVIS_SELECTED) && (pListData->iItem >= 0) && (pListData->iItem < numMarkers))
#else
			if ((pListData->iItem >= 0) && (pListData->iItem < numMarkers))
#endif
			{

				if (!s_ignore_listview_messages)
					SetEditCurPos(pMarkerList[pListData->iItem].position, true, false);
			}
		}

#ifdef _WIN32
		else if (pHdr->code == NM_KILLFOCUS) {
			int selIndex = -1;
			do {
				selIndex = ListView_GetNextItem(hMarkerListWnd, selIndex, LVNI_SELECTED);
				if (selIndex >= 0) {
					ListView_SetItemState(hMarkerListWnd, selIndex, 0, LVIS_SELECTED);
				}
			} while (selIndex >= 0);
		}
#endif

		else if (pHdr->code == NM_DBLCLK) {
			SendMessage(hwndDlg, WM_COMMAND, IDC_EDIT_MARKER, 0);
		}
		if (pHdr->code == NM_CLICK)
		{
			NMLISTVIEW *pListData = (NMLISTVIEW *)pHdr;
			if ((pListData->iItem >= 0) && (pListData->iItem < numMarkers))
			{
				if (IsDlgButtonChecked(hwndDlg, IDC_PLAY) == BST_UNCHECKED)
				{
					SetEditCurPos(pMarkerList[pListData->iItem].position, true, false);
				}
				else
					SetEditCurPos(pMarkerList[pListData->iItem].position, true, true);
			}

		}
		else if (pHdr->code == NM_RCLICK) {
			NMLISTVIEW *pListData = (NMLISTVIEW *)pHdr;
			//NMLISTVIEW *pListData = (NMLISTVIEW *)pHdr;
			int index = pListData->iItem;
			if (index < 0) index = numMarkers;
			if (index > numMarkers) index = numMarkers;

			HMENU hCtxMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDM_EDITMARKERSMENU));
			if (hCtxMenu) {

				HMENU hPopup = GetSubMenu(hCtxMenu, 0);
				bool hasSingleSel = (ListView_GetSelectedCount(hMarkerListWnd) == 1);
				EnableMenuItem(hPopup, IDC_EDIT_MARKER, MF_BYCOMMAND | (hasSingleSel ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
				EnableMenuItem(hPopup, IDC_DELETE_MARKER, MF_BYCOMMAND | (hasSingleSel ? MF_ENABLED : MF_DISABLED | MF_GRAYED));

				POINT pt;
				GetCursorPos(&pt);
				TrackPopupMenu(hPopup, TPM_LEFTALIGN, pt.x, pt.y, 0, hwndDlg, NULL);
				DestroyMenu(hCtxMenu);
			}
		}

#ifdef _WIN32
		else if ((pHdr->code == NM_RETURN) || (pHdr->code == LVN_KEYDOWN)) {
			if (pHdr->code == NM_RETURN) {
				SendMessage(hwndDlg, WM_COMMAND, IDC_EDIT_MARKER, 0);
				return 1;
			}
			if ((pHdr->code == LVN_KEYDOWN) && (((NMLVKEYDOWN *)pHdr)->wVKey == VK_DELETE)) SendMessage(hwndDlg, WM_COMMAND, IDC_DELETE_MARKER, 0);
			if ((pHdr->code == LVN_KEYDOWN) && (((NMLVKEYDOWN *)pHdr)->wVKey == VK_RETURN)) SendMessage(hwndDlg, WM_COMMAND, IDC_EDIT_MARKER, 0);
		}
#endif
	}; break;

	case WM_COMMAND: {

		if (LOWORD(wParam) == IDC_DELETE_MARKER) {
			if (ListView_GetSelectedCount(hMarkerListWnd) == 1) {
				int selIndex = ListView_GetNextItem(hMarkerListWnd, -1, LVNI_SELECTED);
				if ((selIndex >= 0) && (selIndex < numMarkers)) {
					if (IDYES == MessageBox(hwndDlg, "Are you sure to delete the marker?", "Prompt", MB_YESNO | MB_ICONWARNING)) {
						Undo_BeginBlock();
						DeleteProjectMarker(NULL, pMarkerList[selIndex].ID, false);
						Undo_EndBlock("Delete marker", UNDO_STATE_MISCCFG);
						updateMarkerList(hMarkerListWnd);
					}
				}
			}
		}

		else if (LOWORD(wParam) == IDC_EDIT_MARKER) {
			if (ListView_GetSelectedCount(hMarkerListWnd) == 1) {
				int selIndex = ListView_GetNextItem(hMarkerListWnd, -1, LVNI_SELECTED);
				if ((selIndex >= 0) && (selIndex < numMarkers)) {
					int lParam = pMarkerList[selIndex].type << 24;
					if (pMarkerList[selIndex].type == '!') {
						DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_EDITINDEX0MARKER), hwndDlg, editSingleMarkerDlgProc, lParam + (selIndex & 0x00FFFFFF));
					}
					else if (pMarkerList[selIndex].type == '#') {
						DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_EDITINDEX1MARKER), hwndDlg, editSingleMarkerDlgProc, lParam + (selIndex & 0x00FFFFFF));
					}
					else if (pMarkerList[selIndex].type == '@') {
						DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_EDITALBUMMARKER), hwndDlg, editSingleMarkerDlgProc, lParam + (selIndex & 0x00FFFFFF));
					}
				}
			}
		}

		else if (LOWORD(wParam) == IDC_INSERT_INDEX0_MARKER) {
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_EDITINDEX0MARKER), hwndDlg, editSingleMarkerDlgProc, ('!' << 24) + 0x00FFFFFF);
		}
		else if (LOWORD(wParam) == IDC_INSERT_INDEX1_MARKER) {
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_EDITINDEX1MARKER), hwndDlg, editSingleMarkerDlgProc, ('#' << 24) + 0x00FFFFFF);
		}
		else if (LOWORD(wParam) == IDC_INSERT_ALBUM_MARKER) {
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_EDITALBUMMARKER), hwndDlg, editSingleMarkerDlgProc, ('@' << 24) + 0x00FFFFFF);
		}

		if (LOWORD(wParam) == IDC_PLAY_BUTTON)
			Main_OnCommand(40044, 0); //Transport play/stop


		if (LOWORD(wParam) != IDCANCEL) return 0;
		EndDialog(hwndDlg, IDCANCEL);
	};

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return 0;
	case WM_DESTROY: {
		if (hEditMarkersDlg) saveConfiguration();
		KillTimer(hwndDlg, 1);
		freeMarkerList();
		m_bPlayOnSel = IsDlgButtonChecked(hwndDlg, IDC_PLAY) == BST_CHECKED;
		CheckMenuItem(hEditMenu, editMarkersRegisteredCommand, MF_UNCHECKED);
		hEditMarkersDlg = NULL;
		char configFileName[1024];
		const char* inifilepath = GetResourcePath();
		sprintf(configFileName, "%s//ddpEdit.ini", inifilepath);
		char cOptions[4];
		sprintf(cOptions, "%c", m_bPlayOnSel ? '1' : '0');
		WritePrivateProfileString(SEEK_PLAY_INI_SECTION, SEEK_PLAY_KEY, cOptions, configFileName);
	}; return 0;
	}
	return 0;
}


bool editMarkersHookCommandProc(int command, int flag) {
	if (editMarkersRegisteredCommand && (command == editMarkersRegisteredCommand)) {
		if (hEditMarkersDlg == NULL) {
			HWND h = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_EDITMARKERS), hParentWnd, editMarkersDlgProc);
			if (h) ShowWindow(h, SW_SHOW);
		}
		else DestroyWindow(hEditMarkersDlg);

		return true;
	}
	return false;
}
extern "C" {
	REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec) {
		g_hInst = hInstance;
		if (rec) {
			if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc)
				return 0; /*todo: proper error*/
			g_plugin_info = rec;
			g_parent = rec->hwnd_main;

			// load all Reaper API functions in one go, byebye ugly IMPAPI macro!
			int error_count = REAPERAPI_LoadAPI(rec->GetFunc);
			if (error_count > 0)
			{
				char errbuf[256];
				sprintf(errbuf, "Failed to load %d expected API function(s)", error_count);
				MessageBox(g_parent, errbuf, "extension error", MB_OK);
				return 0;
			}
		}



		editMarkersRegisteredCommand = g_plugin_info->Register("command_id", (void *) "_DDP_MARKER_EDITOR");
		editMarkersAccelerator.accel.cmd = editMarkersRegisteredCommand;
		g_plugin_info->Register("gaccel", &editMarkersAccelerator);
		g_plugin_info->Register("hookcommand", (void *)editMarkersHookCommandProc);
		if (GetMainHwnd) hParentWnd = GetMainHwnd();


		//This section below commented out for now because it puts the action in the main edit menu,
		//and would mess up a custom menu.

		//hEditMenu = GetSubMenu(GetMenu(hParentWnd),
			//#ifdef _WIN32
			//			1
			//#else // OS X has one extra menu
			//			2
			//#endif
			//			);
			//		editMarkersMenuPos = GetMenuItemCount(hEditMenu);
			//		MENUITEMINFO mi = { sizeof(MENUITEMINFO) };
			//		mi.fMask = MIIM_TYPE | MIIM_ID;
			//		mi.fType = MFT_STRING;
			//		mi.dwTypeData = (char *)editMarkersAction;
			//		mi.wID = editMarkersRegisteredCommand;
			//		InsertMenuItem(hEditMenu, editMarkersMenuPos, 1, &mi);

		return 1;
	}
};


#ifndef _WIN32 // MAC resources
#include "../WDL/swell/swell-dlggen.h"
#include "res.rc_mac_dlg"
#undef BEGIN
#undef END
#include "../WDL/swell/swell-menugen.h"
#include "res.rc_mac_menu"
#endif

#include <fstream>
#include <Windows.h>

#include "framework.h"
#include "QrCodeGen.h"



#define MAX_LOADSTRING 100
#define GENERATE 10
#define SAVE 100
#define TEXTBOX 1000
#define COMBOBOX 10000

// √лобальные переменные:
HINSTANCE hInst;                                // текущий экземпл€р
WCHAR szTitle[MAX_LOADSTRING];                  // “екст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];			// им€ класса главного окна
HBITMAP hBmpQr = NULL;
HWND hWindow;
HWND hGtBtn;
HWND hSaveBtn;
HWND hEdit;
HWND hComboBox;
std::string svg;

// ќтправить объ€влени€ функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void				CreateBMPFile(HWND hwnd, LPTSTR pszFile, PBITMAPINFO pbi,
									HBITMAP hBMP, HDC hDC);
PBITMAPINFO			CreateBitmapInfoStruct(HWND hwnd, HBITMAP hBmp);
HBITMAP				CreateQRBitmap(HWND hWnd, QrCode qr);
void				ShowQRCode(HWND hWnd, HBITMAP hBmp);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// »нициализаци€ глобальных строк
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_QRCODEGEN, szWindowClass, MAX_LOADSTRING);

	MyRegisterClass(hInstance);

	// ¬ыполнить инициализацию приложени€:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_QRCODEGEN));

	MSG msg;

	// ÷икл основного сообщени€:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

//
//  ‘”Ќ ÷»я: MyRegisterClass()
//
//  ÷≈Ћ№: –егистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_QRCODEGEN));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_QRCODEGEN);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   ‘”Ќ ÷»я: InitInstance(HINSTANCE, int)
//
//   ÷≈Ћ№: —охран€ет маркер экземпл€ра и создает главное окно
//
//    ќћћ≈Ќ“ј–»»:
//
//        ¬ этой функции маркер экземпл€ра сохран€етс€ в глобальной переменной, а также
//        создаетс€ и выводитс€ главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // —охранить маркер экземпл€ра в глобальной переменной

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 400, nullptr, nullptr, hInstance, nullptr);

	hWindow = hWnd;

	if (!hWindow)
	{
		return FALSE;
	}	

	hGtBtn = CreateWindowW(
		L"BUTTON",  // Predefined class; Unicode assumed 
		L"Generate",// Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
		10,         // x position 
		280,         // y position 
		100,        // Button width
		50,         // Button height
		hWnd,     // Parent window
		(HMENU)GENERATE,  // No menu.
		hInstance,
		NULL);      // Pointer not needed.

	hSaveBtn = CreateWindowW(
		L"BUTTON",  // Predefined class; Unicode assumed 
		L"Save",// Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
		130,         // x position 
		280,         // y position 
		100,        // Button width
		50,         // Button height
		hWnd,     // Parent window
		(HMENU)SAVE,       // No menu.
		hInstance,
		NULL);      // Pointer not needed.

	hEdit = CreateWindowW(
		L"EDIT",   // predefined class 
		NULL,         // no window title 
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER| 
		ES_LEFT | ES_MULTILINE,
		10, 10, 400, 250,   // set size in WM_SIZE message 
		hWnd,         // parent window 
		(HMENU)TEXTBOX,   // edit control ID 
		hInstance,
		NULL);        // pointer not needed 

	hComboBox = CreateWindowW(
		L"COMBOBOX",
		NULL,
		WS_VISIBLE | WS_CHILD | CBS_DROPDOWN,
		260,
		280,
		100,
		100,
		hWnd,
		(HMENU)COMBOBOX,
		hInstance,
		NULL);

	if (!hComboBox)
		MessageBox(NULL, TEXT("ComboBox Failed."), TEXT("Error"), MB_OK | MB_ICONERROR);

	SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)"L");
	SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)"M");
	SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)"Q");
	SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)"H");

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  ‘”Ќ ÷»я: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ÷≈Ћ№: ќбрабатывает сообщени€ в главном окне.
//
//  WM_COMMAND  - обработать меню приложени€
//  WM_PAINT    - ќтрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернутьс€
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char text[256];

	switch (message)
	{
	case WM_COMMAND:
	{
		if (LOWORD(wParam) == GENERATE) {

			int ECL = SendMessage(hComboBox, (UINT)CB_GETCURSEL,
				(WPARAM)0, (LPARAM)0);

			HWND hEdit = GetDlgItem(hWnd, TEXTBOX);
			int len = GetWindowTextA(hEdit, text, 256);

			if (len != 0 && ECL != CB_ERR) {

				try {
					QrCode qr = QrCode::Generate(text, ECL);
					
					hBmpQr = CreateQRBitmap(hWnd, qr);
					ShowQRCode(hWnd, hBmpQr);
				}
				catch (int a){
					MessageBox(hWnd, TEXT("Invalid input"), TEXT("Error"), MB_OK | MB_ICONERROR);
				}
				
			}
			else {
				MessageBox(hWnd, TEXT("Error ocurred"), TEXT("Error"), MB_OK | MB_ICONERROR);
			}
		}

		if (LOWORD(wParam) == SAVE) {

			if (hBmpQr != NULL) {
				OPENFILENAME ofn;
				char szFileName[MAX_PATH] = "";

				ZeroMemory(&ofn, sizeof(ofn));

				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFilter = (LPCWSTR)L"Text Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
				ofn.lpstrFile = (LPWSTR)szFileName;
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
				ofn.lpstrDefExt = (LPCWSTR)L"bmp";

				if (GetSaveFileName(&ofn))
				{
					LPWSTR fName = ofn.lpstrFile;

					PBITMAPINFO pInfo = CreateBitmapInfoStruct(hWnd, hBmpQr);
					HDC hDC = GetDC(hWnd);
					SelectObject(hDC, hBmpQr);
					CreateBMPFile(hWnd, fName, pInfo, hBmpQr, hDC);
				
				}
			}
		}
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: ƒобавьте сюда любой код прорисовки, использующий HDC...
		EndPaint(hWnd, &ps);
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

HBITMAP CreateQRBitmap(HWND hWnd, QrCode qr) {

	HDC winDC = GetDC(hWnd);
	HDC bmpDC = CreateCompatibleDC(winDC);
	HBITMAP hBmp = CreateCompatibleBitmap(winDC, 350, 350);

	HBITMAP hBmpOld = (HBITMAP)SelectObject(bmpDC, hBmp);

	int moduleSize = 350 / (qr.GetSize() + 8);
	
	HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
	HBRUSH blackBrush = CreateSolidBrush(RGB(  0,   0,   0));
	
	HPEN hWPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
	HPEN hBPen = CreatePen(PS_SOLID, 1, RGB(  0,   0,   0));

	// Drawing white boarders
	SelectObject(bmpDC, hWPen);
	SelectObject(bmpDC, whiteBrush);	
	Rectangle(bmpDC, 0, 0, 350, 350);

	//// Drawing modules
	std::vector<std::vector<bool>> modules = qr.GetModules();
	
	int y = moduleSize * 4;
	for (int i = 0; i < qr.GetSize(); i++) {
		
		int x = moduleSize * 4;
		for (int j = 0; j < qr.GetSize(); j++) {
			
			if (modules[i][j]) {
				SelectObject(bmpDC, blackBrush);
				SelectObject(bmpDC, hBPen);
			}
			else {
				SelectObject(bmpDC, whiteBrush);
				SelectObject(bmpDC, hWPen);
			}

			int res = Rectangle(bmpDC, x, y, x + moduleSize, y + moduleSize);
			x += moduleSize;
		}
		y += moduleSize;
	}

	hBmp = (HBITMAP)SelectObject(bmpDC, hBmpOld);

	DeleteDC(bmpDC);
	ReleaseDC(hWnd, winDC);
	return hBmp;
}

void ShowQRCode(HWND hWnd, HBITMAP hBmp) {

	BITMAP bitmap;
	HGDIOBJ oldBitmap;

	if (hBmp == NULL)
		return;

	HDC winDC = GetDC(hWnd);
	HDC memDC = CreateCompatibleDC(winDC);

	oldBitmap = SelectObject(memDC, hBmp);

	GetObject(hBmp, sizeof(bitmap), &bitmap);

	BitBlt(winDC, 420, 0, bitmap.bmWidth, bitmap.bmHeight, memDC, 0, 0, SRCCOPY);
	
	SelectObject(memDC, oldBitmap);

	DeleteDC(memDC);
	ReleaseDC(hWnd, winDC);
}

void CreateBMPFile(HWND hwnd, LPTSTR pszFile, PBITMAPINFO pbi,
	HBITMAP hBMP, HDC hDC)
{
	HANDLE hf;                 // file handle  
	BITMAPFILEHEADER hdr;       // bitmap file-header  
	PBITMAPINFOHEADER pbih;     // bitmap info-header  
	LPBYTE lpBits;              // memory pointer  
	DWORD dwTotal;              // total count of bytes  
	DWORD cb;                   // incremental count of bytes  
	BYTE* hp;                   // byte pointer  
	DWORD dwTmp;

	pbih = (PBITMAPINFOHEADER)pbi;
	lpBits = (LPBYTE)GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);

	if (!lpBits)
		MessageBox(hwnd, TEXT("GlobalAlloc"), TEXT("Error"), MB_OK | MB_ICONERROR);

	// Retrieve the color table (RGBQUAD array) and the bits  
	// (array of palette indices) from the DIB.  
	if (!GetDIBits(hDC, hBMP, 0, (WORD)pbih->biHeight, lpBits, pbi,
		DIB_RGB_COLORS))
	{
		MessageBox(hwnd, TEXT("GetDIBits"), TEXT("Error"), MB_OK | MB_ICONERROR);
	}

	// Create the .BMP file.  
	hf = CreateFile(pszFile,
		GENERIC_READ | GENERIC_WRITE,
		(DWORD)0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		(HANDLE)NULL);

	if (hf == INVALID_HANDLE_VALUE)
		MessageBox(hwnd, TEXT("CreateFile"), TEXT("Error"), MB_OK | MB_ICONERROR);

	hdr.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"  
	// Compute the size of the entire file.  
	hdr.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) +
		pbih->biSize + pbih->biClrUsed
		* sizeof(RGBQUAD) + pbih->biSizeImage);
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;

	// Compute the offset to the array of color indices.  
	hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) +
		pbih->biSize + pbih->biClrUsed
		* sizeof(RGBQUAD);

	// Copy the BITMAPFILEHEADER into the .BMP file.  
	if (!WriteFile(hf, (LPVOID)& hdr, sizeof(BITMAPFILEHEADER),
		(LPDWORD)& dwTmp, NULL))
	{
		MessageBox(hwnd, TEXT("WriteFile"), TEXT("Error"), MB_OK | MB_ICONERROR);
	}

	// Copy the BITMAPINFOHEADER and RGBQUAD array into the file.  
	if (!WriteFile(hf, (LPVOID)pbih, sizeof(BITMAPINFOHEADER)
		+ pbih->biClrUsed * sizeof(RGBQUAD),
		(LPDWORD)& dwTmp, (NULL)))
		MessageBox(hwnd, TEXT("WriteFile"), TEXT("Error"), MB_OK | MB_ICONERROR);

	// Copy the array of color indices into the .BMP file.  
	dwTotal = cb = pbih->biSizeImage;
	hp = lpBits;
	if (!WriteFile(hf, (LPSTR)hp, (int)cb, (LPDWORD)& dwTmp, NULL))
		MessageBox(hwnd, TEXT("WriteFile"), TEXT("Error"), MB_OK | MB_ICONERROR);

	// Close the .BMP file.  
	if (!CloseHandle(hf))
		MessageBox(hwnd, TEXT("CloseHandle"), TEXT("Error"), MB_OK | MB_ICONERROR);
	// Free memory.  
	GlobalFree((HGLOBAL)lpBits);
}

PBITMAPINFO CreateBitmapInfoStruct(HWND hwnd, HBITMAP hBmp)
{
	BITMAP bmp;
	PBITMAPINFO pbmi;
	WORD    cClrBits;

	// Retrieve the bitmap color format, width, and height.  
	if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)& bmp))
		MessageBox(hwnd, TEXT("GetObject"), TEXT("Error"), MB_OK | MB_ICONERROR);

	// Convert the color format to a count of bits.  
	cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
	if (cClrBits == 1)
		cClrBits = 1;
	else if (cClrBits <= 4)
		cClrBits = 4;
	else if (cClrBits <= 8)
		cClrBits = 8;
	else if (cClrBits <= 16)
		cClrBits = 16;
	else if (cClrBits <= 24)
		cClrBits = 24;
	else cClrBits = 32;

	// Allocate memory for the BITMAPINFO structure. (This structure  
	// contains a BITMAPINFOHEADER structure and an array of RGBQUAD  
	// data structures.)  

	if (cClrBits < 24)
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
			sizeof(BITMAPINFOHEADER) +
			sizeof(RGBQUAD) * (1 << cClrBits));

	// There is no RGBQUAD array for these formats: 24-bit-per-pixel or 32-bit-per-pixel 

	else
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
			sizeof(BITMAPINFOHEADER));

	// Initialize the fields in the BITMAPINFO structure.  

	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bmp.bmWidth;
	pbmi->bmiHeader.biHeight = bmp.bmHeight;
	pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
	pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
	if (cClrBits < 24)
		pbmi->bmiHeader.biClrUsed = (1 << cClrBits);

	// If the bitmap is not compressed, set the BI_RGB flag.  
	pbmi->bmiHeader.biCompression = BI_RGB;

	// Compute the number of bytes in the array of color  
	// indices and store the result in biSizeImage.  
	// The width must be DWORD aligned unless the bitmap is RLE 
	// compressed. 
	pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits + 31) & ~31) / 8
		* pbmi->bmiHeader.biHeight;
	// Set biClrImportant to 0, indicating that all of the  
	// device colors are important.  
	pbmi->bmiHeader.biClrImportant = 0;
	return pbmi;
}
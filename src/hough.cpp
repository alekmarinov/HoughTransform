// testwin.cpp : Defines the entry point for the application.
//

#include "resource.h"
#include <windows.h>
#include <stdio.h>
#include <windowsx.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <malloc.h>

#define MAX_LOADSTRING 100
#define MIN(a,b) ((a)>(b)?(b):(a))

// Global Variables:
TCHAR szTitle[MAX_LOADSTRING];										// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];								// Window class name

typedef enum {
		SIZE_SMALL=0,
		SIZE_MEDIUM,
		SIZE_LARGE,
} SIZE_TYPE;

// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
void				TransparentRectangle(HDC dc, int x, int y, int w, int h);
void				DrawBuffers(HWND hWnd, HDC dc);
void				RedrawState(HWND hWnd, HDC dc);
void				ChangeSize(SIZE_TYPE size);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
char				szStatusLine[255];

void ReallocBuffers();
void ClearBuffers();
void DrawDot();
void DrawHLine();
void DrawVLine();
void DrawBox();
void DrawCircle();
void get_sizes(int *i_w, int *i_h, int *h_w, int *h_h);
void MakeHough();

struct { int w; int h; } size_def[3] = { {80, 40}, {160, 80}, {320, 240} };

int brightness=8;
int freepen=0;
int do_draw=0;
int oldFreePenX=0, oldFreePenY=0;

struct
{
	int i_width, i_height;
	BYTE *src_buffer;
	int h_width, h_height;
	double step;
	WORD *dst_buffer;
} hData;

int main(int argc, char** argv)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	MSG msg;
	HACCEL hAccelTable;
	szStatusLine[0]=0;

	memset(&hData, 0, sizeof(hData));
	ChangeSize(SIZE_LARGE);
	DrawDot();

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_CLASS_NAME, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, SW_NORMAL)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}


void ReallocBuffers()
{
	printf("ReallocBuffers\n");
	printf("free 0x%X:0x%X\n", hData.src_buffer, hData.dst_buffer);
	if (hData.src_buffer) free(hData.src_buffer);
	if (hData.dst_buffer) free(hData.dst_buffer);

	hData.src_buffer=(BYTE *)calloc(hData.i_width * hData.i_height, 1);
	hData.dst_buffer=(WORD *)calloc(hData.h_width * hData.h_height * 2, 1);
	printf("alloc 0x%X:0x%X\n", hData.src_buffer, hData.dst_buffer);
	hData.step = (double)(2 * M_PI)/ hData.h_width;
	ClearBuffers();
}

void ClearBuffers()
{
	printf("ClearBuffers\n");
	memset(hData.src_buffer, 0, hData.i_width * hData.i_height);
	memset(hData.dst_buffer, 0, hData.h_width * hData.h_height * 2);
}

void DrawDot()
{
	ClearBuffers();
	hData.src_buffer[(hData.i_height)/2*hData.i_width + hData.i_width/2]=255;
	MakeHough();
}

void DrawHLine()
{
	int i;
	ClearBuffers();
	for (i=0; i<hData.i_width; i++)
		hData.src_buffer[(hData.i_height/2)*hData.i_width + i]=128;
	MakeHough();
}

void DrawVLine()
{
	int i;
	ClearBuffers();
	for (i=0; i<hData.i_height; i++)
		hData.src_buffer[i*hData.i_width + hData.i_width/2]=128;
	MakeHough();
}

void DrawBox()
{
	printf("DrawBox\n");
	int i, j;
	ClearBuffers();
	for (j=0; j<hData.i_height; j++)
		for (i=0; i<hData.i_width; i++)
		hData.src_buffer[j*hData.i_width + i]=255;
	MakeHough();
}

void DrawCircle()
{
	ClearBuffers();
	int min=MIN(hData.i_width, hData.i_height);
	int cx=hData.i_width>>1, cy=hData.i_height>>1;
	int i, y, r;
	min>>=1;
	//r=min;
	for (r=1; r<min; r++)
		for (i=-min; i<min; i++)
		{
			y=(int)(floor(0.5+sqrt(r*r-i*i)));
			if (hData.i_width>hData.i_height)
			{
				hData.src_buffer[cx + y + (cy + i) * hData.i_width] = 255;
				hData.src_buffer[cx - y + (cy + i) * hData.i_width] = 255;
			}
			else
			{
				hData.src_buffer[cx + i + (cy + y) * hData.i_width] = 255;
				hData.src_buffer[cx + i + (cy - y) * hData.i_width] = 255;
			}
		}
	MakeHough();
}

struct 
{
	int size;
	int show_rects;
} state = { SIZE_MEDIUM, 0 };

void get_sizes(int *i_w, int *i_h, int *h_w, int *h_h)
{
	*i_w=size_def[state.size].w;
	*i_h=size_def[state.size].h;
	*h_w=*i_w;
	*h_h=(int)floor(0.5+sqrt( *i_w * *i_w + *i_h * *i_h ));

}

void MakeHough()
{
	printf("MakeHough\n");
	freepen=0;
	oldFreePenX=oldFreePenY=0;
	int x, y, i;
	double Ro, Fi, f, r;
	int center_x, center_y;
	int hcenter_y = hData.h_height / 2;
	BYTE color;

	center_x=hData.i_width / 2;
	center_y=hData.i_height / 2;

	for (y=0; y<hData.i_height; y++)
		for (x=0; x<hData.i_width; x++)
		{
			color=hData.src_buffer[x+y*hData.i_width];
			if (color>0)color=1;
			if (color)
			{
				int x0, y0;

				/* according the center of coordinate system */
				x0 = x-center_x; 
				y0 = y-center_y; 

				Ro = sqrt(x0 * x0 + y0 * y0);
				Fi = atan2(y0, x0);

				/* draw cosinusoide */
				f=-M_PI;
				for (i=0; i<hData.h_width; i++)
				{
					int address;
					r = Ro * cos(f - Fi - M_PI/2);
					address=(int)floor(0.5 + hcenter_y + r) * hData.h_width + i;
					if (address < hData.h_width * hData.h_height)
						hData.dst_buffer[address] += color;
					f += hData.step;
				}
			}
		}
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;
	memset(&wcex, 0, sizeof(WNDCLASSEX));

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDR_MENU1);
	wcex.lpszClassName	= szWindowClass;

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;
   RECT rect;

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   GetWindowRect(hWnd, &rect);
   SetWindowPos(hWnd, 0, rect.left, rect.top, (int)(size_def[SIZE_LARGE].w * 2 + 100), (int)(size_def[SIZE_LARGE].h * 2), 0);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}


void TransparentRectangle(HDC dc, int x, int y, int w, int h)
{
	POINT p;
	w--;
	h--;
	MoveToEx(dc, x, y, &p);
	LineTo(dc, x+w, y);
	LineTo(dc, x+w, y+h);
	LineTo(dc, x, y+h);
	LineTo(dc, x, y);
}

void DrawBuffers(HWND hWnd, HDC dc)
{
	HPEN hPen;
	BYTE color;
	WORD rColor;
	WORD gColor;
	WORD bColor;
	int center_x, center_y;
	RECT rt;

	if (!hData.src_buffer) return ;
	GetClientRect(hWnd, &rt);
	center_x = rt.right / 2;
	center_y = rt.bottom / 2;

	int i_x, i_y, h_x, h_y, i_w, i_h, h_w, h_h, i, j;
	get_sizes(&i_w, &i_h, &h_w, &h_h);

	i_x=(center_x - i_w)/2;
	i_y=(rt.bottom - i_h)/2;
	h_x=1+center_x + (center_x - h_w)/2;
	h_y=(rt.bottom-h_h)/2;


	hPen=CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
	HGDIOBJ oldObj=SelectObject(dc, hPen);

	for (j=0; j<i_h; j++)
		for (i=0; i<i_w; i++)
		{
			color=hData.src_buffer[j*i_w+i];
			if (color)
				SetPixel(dc, i_x+i, i_y+j, RGB(255, 255, 255));
		}

	for (j=0; j<h_h; j++)
		for (i=0; i<h_w; i++)
		{
			WORD w=hData.dst_buffer[j*h_w+i] * brightness;
			if (w>255) {
				SetPixel(dc, h_x+i, h_y+j, RGB((w>>8) << 2,(w>>8) << 2,  255));
			} else
			{
				SetPixel(dc, h_x+i, h_y+j, RGB(0, 0, w));
			}
		}

	DeleteObject(hPen);
	SelectObject(dc,oldObj);
}

void DrawDiagrames(HWND hWnd, HDC dc)
{
	int i, j;
	HPEN hPen=CreatePen(PS_SOLID, 1, RGB(0, 128, 64));
	POINT p;
	LONG *sums;

	int center_x, center_y;
	RECT rt;
	int h_x, h_y, i_w, i_h, h_w, h_h;

	GetClientRect(hWnd, &rt);
	center_x = rt.right / 2;
	center_y = rt.bottom / 2;
	get_sizes(&i_w, &i_h, &h_w, &h_h);
	h_x=1+center_x + (center_x - h_w)/2;
	h_y=(rt.bottom-h_h)/2;

	HGDIOBJ oldObj=SelectObject(dc, hPen);
	/* horizontal diagram */
	sums=(LONG *)malloc(hData.h_width * sizeof(LONG));
	int sumMaxI=0;
	LONG sumMax=0;
	LONG sumMin=9999999999;
	for (i=0; i<hData.h_width; i++)
	{
		int max=0;
		for (j=0; j<hData.h_height; j++)
		{
			if (hData.dst_buffer[j*hData.h_width + i]>max) max=hData.dst_buffer[j*hData.h_width + i];
		}
		sums[i]=max;
		if (max>sumMax) { sumMax=max; sumMaxI=i; }
		if (max<sumMin) { sumMin=max; }
	}
	printf("Maximum %d at Phi=%2.4f\n", sumMax, (float)360/(2*M_PI)*hData.step*sumMaxI);

	/* scale */ 
	double scale_factor=(double)50/sumMax;

	for (i=0; i<hData.h_width; i++)
	{
		MoveToEx(dc, h_x+i, rt.bottom-10, &p);
		LineTo  (dc, h_x+i, rt.bottom -10- (int)ceil(0.5f + sums[i] * scale_factor));
	}
	DeleteObject(hPen);
	hPen=CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	SelectObject(dc, hPen);
	MoveToEx(dc, h_x+sumMaxI, rt.bottom-10-(int)ceil(0.5f + sumMax * scale_factor), &p);
	LineTo  (dc, h_x+sumMaxI, rt.bottom-10-(int)ceil(0.5f + 2*sumMax * scale_factor));

	int rad=40;
	double phi=hData.step*sumMaxI;
	int x=rad*cos(phi);
	int y=rad*sin(phi);
	MoveToEx(dc, rad, rt.bottom-rad, &p);
	LineTo(dc, rad+x, rt.bottom-rad+y);

	DeleteObject(hPen);
	SelectObject(dc,oldObj);
	free(sums);
}

void RedrawState(HWND hWnd, HDC dc)
{
	RECT rt;
	HPEN hPen;
	POINT p;
	int i_w, i_h, h_w, h_h;

	GetClientRect(hWnd, &rt);
	DrawText(dc, szStatusLine, (int)strlen(szStatusLine), &rt, DT_LEFT);

	hPen=CreatePen(PS_DASH, 1, RGB(128, 128, 128));
	HGDIOBJ oldObj=SelectObject(dc, hPen);
	if (state.show_rects)
	{
		int center_x = rt.right>>1, center_y = rt.bottom>>1;
		get_sizes(&i_w, &i_h, &h_w, &h_h);
		TransparentRectangle(dc, (center_x - i_w)/2, (rt.bottom - i_h)/2, i_w, i_h);
		TransparentRectangle(dc, 1+center_x + (center_x - h_w)/2, 1+((rt.bottom-h_h)<0?0:rt.bottom-h_h)/2, h_w, h_h);
	}

	MoveToEx(dc, rt.right>>1, 0, &p);
	LineTo(dc, rt.right>>1, rt.bottom);
	DeleteObject(hPen);
	SelectObject(dc, oldObj);
}

void ChangeSize(SIZE_TYPE size)
{
	state.size=size;
	get_sizes(&hData.i_width, &hData.i_height, &hData.h_width, &hData.h_height);

	ReallocBuffers();
}

int mouseOver=0, LastX=0, LastY=0;
void onMouseMove(HWND hWnd, HDC hdc, int xPos, int yPos)
{
	RECT rt;
	POINT p;
	HPEN hPen;
	int center_x, center_y, h_x, h_y, i_w, i_h, h_w, h_h;
	GetClientRect(hWnd, &rt);
	center_x = rt.right>>1;
	center_y = rt.bottom>>1;
	get_sizes(&i_w, &i_h, &h_w, &h_h);
	h_x=1+center_x + (center_x - h_w)/2;
	h_y=(rt.bottom-h_h)/2;

    if (xPos>=h_x && xPos<h_x+h_w && yPos>=h_y && yPos <h_y+h_h)
	{
		HGDIOBJ oldObj;
		SetCursor(LoadCursor(0, IDC_CROSS));
		mouseOver=1;
		if (LastX)
		{
			// hide old
			hPen=(HPEN)GetStockObject(BLACK_PEN);
			oldObj=SelectObject(hdc, hPen);
			MoveToEx(hdc, LastX, h_y-10, &p);
			LineTo(hdc, LastX, h_y);
			MoveToEx(hdc, LastX, rt.bottom, &p);
			LineTo(hdc, LastX, rt.bottom-10);

			MoveToEx(hdc, h_x-10, LastY, &p);
			LineTo(hdc, h_x, LastY);
			MoveToEx(hdc, h_x+h_w, LastY, &p);
			LineTo(hdc, h_x+h_w+10, LastY);
			SelectObject(hdc, oldObj);
		}
		hPen=CreatePen(PS_DASH, 1, RGB(128, 128, 128));
		oldObj=SelectObject(hdc, hPen);

		MoveToEx(hdc, xPos, h_y-10, &p);
		LineTo(hdc, xPos, h_y);
		MoveToEx(hdc, xPos, rt.bottom, &p);
		LineTo(hdc, xPos, rt.bottom-10);

		MoveToEx(hdc, h_x-10, yPos, &p);
		LineTo(hdc, h_x, yPos);
		MoveToEx(hdc, h_x+h_w, yPos, &p);
		LineTo(hdc, h_x+h_w+10, yPos);

		DeleteObject(hPen);
		SelectObject(hdc, oldObj);
		LastX=xPos;
		LastY=yPos;

		double step=(double)M_PI*2/h_w;

		sprintf(szStatusLine, "              Ro:%2.4f, Phi:%2.4f               ", (float)abs(yPos-h_y - h_h/2), (float)360/(2*M_PI)*step*(xPos - h_x - h_w/2));
		RedrawState(hWnd, hdc);
	} else
		mouseOver=0;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent, xPos, yPos;
	PAINTSTRUCT ps;
	HDC hdc;
	RECT rt;
	POINT pt;

	switch (message) 
	{
		case WM_LBUTTONDOWN:
			if (freepen)
			{
					int x=GET_X_LPARAM(lParam), y=GET_Y_LPARAM(lParam);
					int i_x, i_y, i_w, i_h, h_w, h_h, center_x, center_y;
					GetClientRect(hWnd, &rt);
					center_x = rt.right / 2;
					center_y = rt.bottom / 2;

					get_sizes(&i_w, &i_h, &h_w, &h_h);
					i_x=(center_x - i_w)/2;
					i_y=(rt.bottom - i_h)/2;
					if (x>=i_x && y>=i_y && x < i_x+i_w && y < i_y+i_h) 
					{
						hdc = GetDC(hWnd);
						SelectObject(hdc, GetStockObject(WHITE_PEN));
						SetPixel(hdc, x, y, RGB(255, 255, 255));
						ReleaseDC(hWnd, hdc);
						hData.src_buffer[ (y-i_y)*hData.i_width + (x-i_x) ] = 1;
					}
				do_draw=1;
			}
			break;
		case WM_LBUTTONUP:
				do_draw=0;
			break;
		case WM_MOUSEMOVE:
			xPos = GET_X_LPARAM(lParam); 
			yPos = GET_Y_LPARAM(lParam);
			hdc = GetDC(hWnd);
			onMouseMove(hWnd, hdc, xPos, yPos);
			if (freepen && do_draw)
			{
				int i_x, i_y, i_w, i_h, h_w, h_h, center_x, center_y;
				GetClientRect(hWnd, &rt);
				center_x = rt.right / 2;
				center_y = rt.bottom / 2;

				get_sizes(&i_w, &i_h, &h_w, &h_h);
				i_x=(center_x - i_w)/2;
				i_y=(rt.bottom - i_h)/2;
				if (xPos>=i_x && yPos>=i_y && xPos < i_x+i_w && yPos < i_y+i_h) 
				{
					SetPixel(hdc, xPos, yPos, RGB(255, 255, 255));
					hData.src_buffer[ (yPos-i_y)*hData.i_width + (xPos-i_x) ] = 1;
				}
					
			}
			ReleaseDC(hWnd, hdc);
			break;

		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
				case ID_SIZE_SMALL:
						szStatusLine[0]=0;
						ChangeSize(SIZE_SMALL);
						DrawDot();
					break;
				case ID_SIZE_MEDIUM:
						szStatusLine[0]=0;
						ChangeSize(SIZE_MEDIUM);
						DrawDot();
					break;
				case ID_SIZE_LARGE:
						szStatusLine[0]=0;
						ChangeSize(SIZE_LARGE);
						DrawDot();
					break;

				case ID_IMAGE_DOT:
					DrawDot();
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
				break;
				case ID_IMAGE_HLINE:
					DrawHLine();
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
				break;
				case ID_IMAGE_VLINE:
					DrawVLine();
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
				break;
				case ID_IMAGE_BOX:
					DrawBox();
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
				break;
				case ID_IMAGE_CIRCLE:
					DrawCircle();
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
				break;
				case ID_FILE_HOUGH:
					MakeHough();
					freepen=0;
					break;
				case ID_IMAGE_FREEPEN:
					freepen=1;
					ClearBuffers();
					//MakeHough();
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
				break;
				case IDM_SMALL:
					GetClientRect(hWnd, &rt);
					ChangeSize(SIZE_SMALL);
					InvalidateRect(hWnd, &rt, 1);
					break;
				case IDM_MEDIUM:
					GetClientRect(hWnd, &rt);
					ChangeSize(SIZE_MEDIUM);
					InvalidateRect(hWnd, &rt, 1);
					break;
				case IDM_LARGE:
					GetClientRect(hWnd, &rt);
					ChangeSize(SIZE_LARGE);
					InvalidateRect(hWnd, &rt, 1);
					break;
				case ID_SHOW_RECT:
					GetClientRect(hWnd, &rt);
					state.show_rects^=1;
					InvalidateRect(hWnd, &rt, 1);
					break;
				case IDM_ABOUT:
				   DialogBox(GetModuleHandle(NULL), (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				   break;
				case IDM_EXIT:
				   DestroyWindow(hWnd);
				   break;
				case ID_BRIGHTNESS_X3:
					brightness=2;
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
					break;
				case ID_BRIGHTNESS_X5:
					brightness=4;
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
					break;
				case ID_BRIGHTNESS_X9:
					brightness=8;
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
					break;
				case ID_BRIGHTNESS_X6:
					brightness=1;
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
					break;
				case ID_BRIGHTNESS_X16:
					brightness=64;
					GetClientRect(hWnd, &rt);
					InvalidateRect(hWnd, &rt, 1);
					break;
				default:
				   return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			DrawBuffers(hWnd, hdc);
			DrawDiagrames(hWnd, hdc);
			RedrawState(hWnd, hdc);
			EndPaint(hWnd, &ps);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

// Mesage handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
				return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}

//指定注释类型
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "gdiplus")
#pragma comment(lib, "comctl32.lib")


#include <windows.h>
#include <gdiplus.h>//加载并显示图片
#include <list>
#include <commctrl.h>
#include <tchar.h>
#include "resource.h"

//关于bottom的宏定义：
#define IDC_TOOLBAR   1001
#define IDC_STATUSBAR 1002
//#define PIC_RESOURCE_USED

#ifdef PIC_RESOURCE_USED
#define IDB_NEW   110
#define IDB_OPEN  111
#define IDB_SAVE  112
#endif
#define ID_FOPEN  1111
#define ID_FCLOSE 1112
#define ID_FSAVE  1113

TCHAR szClassName[] = TEXT("Window");//char
using namespace Gdiplus;

class ImageListPanel
{
	
	BOOL m_bDrag;//是否存在图片
	int m_nDragIndex;//拖图的数量
	int m_nSplitPrevIndex;
	int m_nSplitPrevPosY;
	int m_nMargin;
	int m_nImageMaxCount;//存储的最大图片数量
	std::list<Bitmap*> m_listBitmap;//Bitmap:图片处理 链表。无随机读取功能
	WNDPROC fnWndProc;//作为一个窗口类的指针
	bool image_dropfile;
	HDC hdc_buffer;
public:
	HWND m_hWnd;//句柄 窗口信息
	ImageListPanel(int nImageMaxCount, DWORD dwStyle, int x, int y, int width, int height, HWND hParent)//hParent:父窗口
		: m_nImageMaxCount(nImageMaxCount)//构造函数
		, m_hWnd(0)
		, fnWndProc(0)
		, m_nMargin(4)
		, m_bDrag(0)
		, m_nSplitPrevIndex(-1)
		, m_nSplitPrevPosY(0)
		, image_dropfile(0)
	{
		//上次描边区域初始化为0
		WNDCLASSW wndclass = { 0, WndProc, 0, 0, GetModuleHandle(0), 0, LoadCursor(0, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), 0, __FUNCTIONW__ };
		wndclass.hbrBackground = NULL;//用透明画刷刷背景
		//注册窗口类
		RegisterClassW(&wndclass);
		//x,y:与父窗口左上角坐标的相对位置 
		m_hWnd = CreateWindowW(__FUNCTIONW__, 0, dwStyle, x, y, width, height, hParent, 0, GetModuleHandle(0), this);//GetModuleHandle：转化成handle
		
	}
	~ImageListPanel()//清除链表
	{
		for (auto &bitmap : m_listBitmap) {
			//清除链表
			delete bitmap;
			bitmap = 0;
			DeleteObject(hdc_buffer);
		}
	}
	//链表元素的位置转移
	BOOL MoveImage(int nIndexFrom, int nIndexTo)
	{
		if (nIndexFrom < 0) nIndexFrom = 0;
		if (nIndexTo < 0) nIndexTo = 0;
		if (nIndexFrom == nIndexTo) return FALSE;//不移动
		std::list<Bitmap*>::iterator itFrom = m_listBitmap.begin();//开始的迭代器
		std::list<Bitmap*>::iterator itTo = m_listBitmap.begin();
		std::advance(itFrom, nIndexFrom);//给迭代器添加指定偏移量
		std::advance(itTo, nIndexTo);
		m_listBitmap.splice(itTo, m_listBitmap, itFrom);//转移元素itFrom->itTo
		return TRUE;
	}
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) 
	{
		/*******************/
		/*******************/
		static int image_height//图片的统一宽度（采用第一张的宽度）
				,image_client_y//客户区窗口高度，来确定对应的尺寸
				,image_client_x;
		static int iDeltaPerLine, iAccumDelta;
		int image_ivertpos, image_ipaint_begin;//保存滚动条位置
		SCROLLINFO image_si; //滚动条结构
		PAINTSTRUCT image_ps;
		image_ivertpos = image_ipaint_begin = 0;
		HDC hdc_new, hdc_buffer;
		ULONG ulScroLLlines;

		//HBITMAP bmp;//位图
		/*******************/
		/*******************/
		//WM_NCCREATE：CREATE信息之前被发出的信息
		if (msg == WM_NCCREATE) {
			//GWLP_USERDATA:设置与该窗口相关的用户数据 第三个参数为指定替换值
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
			image_height = 115;
			return TRUE;
		}
		ImageListPanel	* _this = (ImageListPanel*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (_this) {
			switch (msg) 
			{
				//创建窗口信息
			case WM_CREATE:
				DragAcceptFiles(hWnd, TRUE);//接收shell拖过来的文件
				//CreateWindow(TEXT("BUTTON"), TEXT("Click Me"), WS_CHILD | WS_VISIBLE, 30, 10, 80, 20, hWnd, (HMENU)1002, NULL, NULL);
				//没有return0
				
			case WM_SETTINGCHANGE:
				//调用SystemParametersInfo函数对系统参数进行获取
				//SPI_GETWHEELSCROLLLINES：设置滚动球
				SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ulScroLLlines, 0);
				if (ulScroLLlines)
					iDeltaPerLine = WHEEL_DELTA / ulScroLLlines;
				else
					iDeltaPerLine = 0;
				return 0;
				
			case WM_DROPFILES://文件拖放产生的信息
				{
					HDROP hDrop = (HDROP)wParam;
					TCHAR szFileName[MAX_PATH];//name
					UINT iFile, nFiles;
					//return files numbers
					nFiles = DragQueryFile((HDROP)hDrop, 0xFFFFFFFF, NULL, 0);//返回的文件数目
					BOOL bUpdate = FALSE;

					//根据文件拖放数量获取其路径使用Bitmap保存图片，然后压入链表
					for (iFile = 0; iFile < nFiles; ++iFile) 
					{
						//szFileName:文件名(完整路径)
						DragQueryFile(hDrop, iFile, szFileName, sizeof(szFileName));//保存路径名
						
						//FromFile：创建基于图像文件的位图对象	pBitmap：指针指向该对象
						Bitmap* pBitmap = Gdiplus::Bitmap::FromFile(szFileName);
						//pBitmp成功创建
						if (pBitmap && pBitmap->GetLastStatus() == Ok) 
						{
							//m_nImageMaxCount容纳的最大限制
							if (_this->m_listBitmap.size() < _this->m_nImageMaxCount) {//最多八张图片
								_this->m_listBitmap.push_back(pBitmap);//添加到链表中
								//_this->m_vectorBitmap.push_back(pBitmap);
								 bUpdate = TRUE;
							}
						}
					}
					_this->image_dropfile = 0;
					image_si.cbSize = sizeof(image_si);
					image_si.fMask = SIF_RANGE |SIF_PAGE ;//nMin,nMax 成员有效	nPage成员有效
					image_si.nMin = 0;
					//产生拖放文件 size()肯定大于等于1
					image_si.nMax = _this->m_listBitmap.size()*image_height-20;
					image_si.nPage =image_height*image_client_y / (image_height+_this->m_nMargin);//客户区高度
					SetScrollInfo(hWnd, SB_VERT, &image_si, TRUE);//设置垂直滚动条的数据
					_this->image_dropfile = 0;
					//SetScrolRange(hwnd,iBar,iMin,iMax,bREdraw);
					DragFinish(hDrop);//释放内存空间
					if (bUpdate)//重绘
						InvalidateRect(hWnd, 0, 1);//发送重绘信息 
					break;
				}
			/*******************/
			/*******************/
			case WM_SIZE://只会初始时设置一次
			{	image_client_y = HIWORD(lParam);
				image_client_x = LOWORD(lParam);
				break;
			}
			case WM_VSCROLL:
			{
				 image_si.cbSize = sizeof(image_si);
				 image_si.fMask = SIF_ALL;//整个结构都有效
				 GetScrollInfo(_this->m_hWnd, SB_VERT, &image_si);//已经开始获取
				 image_ivertpos = image_si.nPos;//保存移动之前的位置
				 switch (LOWORD(wParam))//鼠标对滚动条的操作
				{
				 case SB_TOP://顶端
					 image_si.nPos = image_si.nMin;
					 break;
				 case SB_BOTTOM://底部
					 image_si.nPos = image_si.nMax;
					 break;
				 case SB_LINEUP://向上滚动一行
					 image_si.nPos -= 20;
					 break;
				 case SB_LINEDOWN://向下滚动一行
					 image_si.nPos += 20;
					 break;
				 case SB_PAGEUP://向上滚动一页
					 image_si.nPos -= image_si.nPage;
					 break;
				 case SB_PAGEDOWN://向下滚动一页
					 image_si.nPos += image_si.nPage;
					 break;
				 case SB_THUMBTRACK://用户正在拖动，直至释放 要处理的
					 image_si.nPos = image_si.nTrackPos;
					 break;
				 default:
					 break;
				}
				 image_si.fMask = SIF_POS;//pos有效
				 SetScrollInfo(hWnd, SB_VERT, &image_si, TRUE);//第四个参数：是否重画
				 GetScrollInfo(hWnd, SB_VERT, &image_si);
				 if (image_si.nPos != image_ivertpos)
					{
					 ScrollWindow(hWnd, 0, image_ivertpos-image_si.nPos, NULL, NULL);
					}
					return 0;
			}
			case WM_MOUSEWHEEL:
				if (iDeltaPerLine == 0)
					break;
				iAccumDelta += (short)HIWORD(wParam);
				while (iAccumDelta >= iDeltaPerLine)
				{
					SendMessage(hWnd, WM_VSCROLL, SB_LINEUP, 0);//发送上一行信息
					iAccumDelta -= iDeltaPerLine;
				}
				while (iAccumDelta <= -iDeltaPerLine)
				{
					SendMessage(hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
					iAccumDelta += iDeltaPerLine;
				}
				return 0;

			case WM_PAINT:
				{	//绘制的是子窗口 
					PAINTSTRUCT ps;
					HDC hdc = BeginPaint(hWnd, &ps);
					hdc_buffer = CreateCompatibleDC(hdc);
					HBITMAP bmp = CreateCompatibleBitmap(hdc, image_client_x,_this->m_listBitmap.size()*image_height);
					HBITMAP hbmp = (HBITMAP)SelectObject(hdc_buffer, bmp);
					
					image_si.cbSize = sizeof(image_si);//填充字节
					image_si.fMask = SIF_POS;
					GetScrollInfo(hWnd, SB_VERT, &image_si);
					image_ivertpos = image_si.nPos;
					RECT rect;//创建一个矩形结构
					GetClientRect(hWnd, &rect);
					INT nTop = 0;
					Graphics g(hdc_buffer);
					int nWidth = rect.right - 2 * _this->m_nMargin;
					if (!_this->image_dropfile)
					{
						int i = 0;
						for (auto image_:_this->m_listBitmap)
						{
							SelectObject(hdc_buffer, image_);
							g.DrawImage(image_
								, _this->m_nMargin
								, i*(image_height + _this->m_nMargin)
								, nWidth
								, image_height);
							_this->image_dropfile = 1;
							i++;
						}
						_this->hdc_buffer = hdc_buffer;							
						BitBlt(hdc, 0, 0,image_client_x , image_client_y, hdc_buffer, 0, 0, SRCCOPY);
						DeleteObject(bmp);
						//DeleteObject(hdc_buffer);
					}
					else{
						BitBlt(hdc, 0, 0, image_client_x, image_client_y, _this->hdc_buffer, 0, image_ivertpos, SRCCOPY);
					}
					EndPaint(hWnd, &ps);
					break;
				}
					//点击鼠标左键的信息 
				case WM_LBUTTONDOWN://可以获得其坐标来更新描边信息 点击关闭按钮移除功能。可以修改为点击描边功能
				{
					//image_ivertpos 滚动条位置,image_paint_begin当前要绘制的最后一张图
					GetScrollInfo(_this->m_hWnd, SB_VERT, &image_si);//获取
					int offset = image_si.nPos;//new_image_ivertpos - ((new_image_ivertpos /(image_height + 4))*image_height + 4);
					RECT rect;
					HDC hdc = GetDC(hWnd);
					GetClientRect(hWnd, &rect);//客户区矩形
					POINT point = { LOWORD(lParam), HIWORD(lParam) };//当前点击的坐标
					INT nTop = _this->m_nMargin;//首副图的高度
					int line_width=2, line_height=2;
					//rect :left ： 指定矩形框左上角的x坐标		top:指定矩形框左上角的y坐标		right定矩形框右下角的x坐标		bottom：指定矩形框右下角的y坐标
					int nWidth = rect.right - 2 * _this->m_nMargin;
					HBRUSH image_brush_red = CreateSolidBrush(RGB(255, 0, 0));
					HBRUSH image_brush_white = CreateSolidBrush(RGB(0, 0, 0));
					for (int i = 1; i <= _this->m_listBitmap.size(); i++)
						//循环判断 可以根据滚动条位置进行判断？点击位置是客户区位置，与滚动条位置无关
					{
						int outline_top=nTop*(i-1) + image_height*(i - 1) - offset;
						RECT rect_outline = { _this->m_nMargin, outline_top, nWidth + _this->m_nMargin, outline_top + image_height };
						HRGN image_frame = CreateRectRgn(_this->m_nMargin,  outline_top,nWidth + _this->m_nMargin, outline_top + image_height);
						if (PtInRect(&rect_outline, point))//判断是否在图内
						{
							FrameRgn(hdc, image_frame, image_brush_red, line_width, line_height);
						}
						else
							FrameRgn(hdc, image_frame, image_brush_white, line_width, line_height);
					}
				}
						//nTop改为初始值
					//鼠标移动的信息
				case WM_MOUSEMOVE:
					if (_this->m_bDrag)
					{
						RECT rect;
						GetClientRect(hWnd, &rect);
						INT nCursorY = HIWORD(lParam);//子窗口句柄
						INT nTop = 0;
						int nWidth = rect.right - 2 * _this->m_nMargin;
						int nIndex = 0;
						for (auto it = _this->m_listBitmap.begin(); it != _this->m_listBitmap.end(); ++it) {
							//int nHeight = (*it)->GetHeight() * nWidth / (*it)->GetWidth();
							RECT rectImage = { 0, nTop, rect.right, nTop + image_height + _this->m_nMargin };
							if (nCursorY >= nTop && (nIndex + 1 == _this->m_listBitmap.size() || nCursorY < nTop + image_height + _this->m_nMargin)) {
								int nCurrentIndex;
								int nCurrentPosY;
								if (nCursorY < nTop + image_height / 2 + _this->m_nMargin) {
									nCurrentIndex = nIndex;
									nCurrentPosY = nTop;
								} else {
									nCurrentIndex = nIndex + 1;
									nCurrentPosY = nTop + image_height + _this->m_nMargin;
								}
								if (nCurrentIndex != _this->m_nSplitPrevIndex) {
									HDC hdc = GetDC(hWnd);
									if(_this->m_nSplitPrevIndex != -1)//涂画 PATINVERT：使用布尔XOR（异或）操作符将指定模式的颜色与目标矩形的颜色进行组合
										PatBlt(hdc, 0, _this->m_nSplitPrevPosY, rect.right, _this->m_nMargin, PATINVERT);
									PatBlt(hdc, 0, nCurrentPosY, rect.right, _this->m_nMargin, PATINVERT);
									//释放hdc
									ReleaseDC(hWnd, hdc);
									_this->m_nSplitPrevIndex = nCurrentIndex;
									_this->m_nSplitPrevPosY = nCurrentPosY;
								}
								return 0;
							}
							//换下一个图
							nTop += image_height + _this->m_nMargin;
							++nIndex;
						}
						break;
					}
				case WM_LBUTTONUP://松开鼠标事件：
					if (_this->m_bDrag)
					{//不需要再获得窗口信息，释放 SetCapture()对应。
						ReleaseCapture();//释放鼠标捕获，恢复通常鼠标处理
						_this->m_bDrag = FALSE;
						if (_this->m_nSplitPrevIndex != -1) {
							RECT rect;
							GetClientRect(hWnd, &rect);
							HDC hdc = GetDC(hWnd);
							PatBlt(hdc, 0, _this->m_nSplitPrevPosY, rect.right, _this->m_nMargin, PATINVERT);
							ReleaseDC(hWnd, hdc);
							if (_this->MoveImage(_this->m_nDragIndex, _this->m_nSplitPrevIndex)) {
								InvalidateRect(hWnd, 0, 1);
							}
							_this->m_nSplitPrevIndex = -1;
						}

					}
					break;
				case WM_DESTROY:
					//DeleteObject(hdc_buffer);
					break;
			}
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
};

HWND CreateToolbar(HWND hParentWnd)
{
	HWND hWndTB;
	TBBUTTON tbb[3];
	HIMAGELIST hImageList, hHotImageList, hDisableImageList;
	HBITMAP hBitmap;

	HINSTANCE hInst = GetModuleHandle(NULL);
	//创建Toolbar控件
	hWndTB = CreateWindowEx(0, TOOLBARCLASSNAME, TEXT(""),
		WS_CHILD | WS_VISIBLE | WS_BORDER | TBSTYLE_LIST | TBSTYLE_AUTOSIZE | TBSTYLE_TOOLTIPS,
		0, 0, 0,0,
		hParentWnd,
		(HMENU)IDC_TOOLBAR,
		hInst,
		NULL);
	if (!hWndTB)
	{
		return NULL;
	}
	SendMessage(hWndTB, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	//下面创建三组24x24像素大小的位图图像列表，用于工具栏图标
	//位图列表
	hImageList = ImageList_Create(24, 24, ILC_COLOR24 | ILC_MASK, 3, 1);
	
	hBitmap = (HBITMAP)LoadImage(NULL, TEXT("bitmap1.BMP"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION); //加载一组图片
	
		//MessageBox(hParentWnd, L"fail", L"Error", MB_OK | MB_ICONERROR );
	ImageList_AddMasked(hImageList, hBitmap, RGB(255, 255, 255));
	DeleteObject(hBitmap);
	SendMessage(hWndTB, TB_SETIMAGELIST, 0, (LPARAM)hImageList); //正常显示时的图像列表

	hHotImageList = ImageList_Create(24, 24, ILC_COLOR24 | ILC_MASK, 3, 1);
	hBitmap = (HBITMAP)LoadImage(NULL, TEXT("green24x3.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	ImageList_AddMasked(hHotImageList, hBitmap, RGB(255, 255, 255));
	DeleteObject(hBitmap);
	SendMessage(hWndTB, TB_SETHOTIMAGELIST, 0, (LPARAM)hHotImageList); //鼠标悬浮时的图像列表

	hDisableImageList = ImageList_Create(24, 24, ILC_COLOR24 | ILC_MASK, 3, 1);
	hBitmap = (HBITMAP)LoadImage(NULL, TEXT("gray24x3.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	ImageList_AddMasked(hDisableImageList, hBitmap, RGB(255, 255, 255));
	DeleteObject(hBitmap);
	SendMessage(hWndTB, TB_SETDISABLEDIMAGELIST, 0, (LPARAM)hDisableImageList); //当工具栏button失能是的图像列表

	ZeroMemory(tbb, sizeof(tbb));
	tbb[0].iBitmap = STD_FILENEW;
	tbb[0].fsState = TBSTATE_ENABLED;
	tbb[0].fsStyle = TBSTYLE_BUTTON | BTNS_AUTOSIZE;
	tbb[0].idCommand = ID_FOPEN;
	tbb[0].iString = (INT_PTR)TEXT("打开");
	tbb[1].iBitmap = MAKELONG(1, 0);
	tbb[1].fsState = TBSTATE_ENABLED;
	tbb[1].fsStyle = TBSTYLE_BUTTON | BTNS_AUTOSIZE;
	tbb[1].idCommand = ID_FCLOSE;
	tbb[1].iString = (INT_PTR)TEXT("关闭");
	tbb[2].iBitmap = MAKELONG(2, 0);
	tbb[2].fsState = TBSTATE_ENABLED;
	tbb[2].fsStyle = TBSTYLE_BUTTON | BTNS_AUTOSIZE;
	tbb[2].idCommand = ID_FSAVE;
	tbb[2].iString = (INT_PTR)TEXT("保存");
	SendMessage(hWndTB, TB_ADDBUTTONS, sizeof(tbb) / sizeof(TBBUTTON), (LPARAM)&tbb); //配置工具栏按钮信息
	SendMessage(hWndTB, WM_SIZE, 0, 0);
	return hWndTB;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//主窗口 回调函数
	static ImageListPanel *pImagePanel1;
	static HWND toolbar_hwnd;
	PAINTSTRUCT ps;
	switch (msg)
	{
	case WM_CREATE:
		//WS_CHILD:子窗口	WS_VISILE：可见形式	WS_BORDER:单边框		WS_EX_LEFTSCROLLBAR：垂直滚动条在左方
		pImagePanel1 = new ImageListPanel(10, WS_CHILD | WS_VISIBLE | WM_HSCROLL| WS_BORDER | WS_VSCROLL, 0, 0, 100,500, hWnd);//创建一个子窗口
		return 0;
	case WM_SIZE:
		//改变窗口指定大小
		//移动m_hWnd所指窗口
		MoveWindow(pImagePanel1->m_hWnd, 0,0,200, 500, TRUE);
		break;
	
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		int wmEvent = HIWORD(wParam);
		// 分析菜单选择:
		switch (wmId)
		{
			case ID_FOPEN:
			SendMessage(toolbar_hwnd, TB_ENABLEBUTTON, (WPARAM)ID_FOPEN, (LPARAM)MAKELONG(FALSE, 0));
			SendMessage(toolbar_hwnd, TB_ENABLEBUTTON, (WPARAM)ID_FSAVE, (LPARAM)MAKELONG(TRUE, 0));
			break;
			case ID_FSAVE:
			SendMessage(toolbar_hwnd, TB_ENABLEBUTTON, (WPARAM)ID_FSAVE, (LPARAM)MAKELONG(FALSE, 0));
			SendMessage(toolbar_hwnd, TB_ENABLEBUTTON, (WPARAM)ID_FOPEN, (LPARAM)MAKELONG(TRUE, 0));
			break;
			case ID_FCLOSE:
			MessageBox(hWnd, TEXT("click!"), TEXT("hint"), MB_OK);
			break;
		}
	}
		return 0;
	case WM_NOTIFY:
		{
			LPNMHDR lpnmhdr = (LPNMHDR)lParam;
			LPTOOLTIPTEXT lpttext;
			if (lpnmhdr->code == TTN_GETDISPINFO)//悬浮
			{
				//处理鼠标在工具栏上悬浮移动时的文本提示
				lpttext = (LPTOOLTIPTEXT)lParam;
				switch (lpttext->hdr.idFrom)
				{
					case ID_FOPEN:
					lpttext->lpszText = TEXT("打开文件");
					break;
					case ID_FCLOSE:
					lpttext->lpszText = TEXT("关闭文件");
					break;
					case ID_FSAVE:
					lpttext->lpszText = TEXT("保存为文件");
					break;
			 }
		}
	 break;
	}
		
	case WM_DESTROY:
		delete pImagePanel1;
		PostQuitMessage(0);
		break;
	default://不关心的消息由DefWindwProc()进行处理
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}
//主函数


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	//Gdi+处理
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	//函数用来初始化windows gdi+ 
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, 0);
	MSG msg;					//windows 中的消息结构体
	//窗口类的定义
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,//形式 宽度高度调整时重绘
		WndProc,				//对应回调函数
		0,
		0,
		hInstance,				//传入的句柄
		0,
		LoadCursor(0, IDC_CROSS),//指定光标
		(HBRUSH)(COLOR_WINDOW + 1),
		0,
		szClassName
	};
	//上面确定窗口的内容

	//下面相当于new 窗口 注册
	RegisterClass(&wndclass);
	//创建窗口 
	HWND hWnd = CreateWindow(	
		szClassName,			//TEXT（"windows"）窗口类名称 
		TEXT("图片显示"),	//窗口标题
		//若在此添加滚动条风格WS_VSCROLL，对应的是整个窗口的滚动条
		WS_OVERLAPPEDWINDOW ,	// 窗口风格，或称窗口格式 
		CW_USEDEFAULT,			//初始窗口x坐标
		0,						//y
		CW_USEDEFAULT,			// 初始 x 方向尺寸
		0,						//y
		0,
		0,
		hInstance,				//句柄
		0
	);
	//toolbar是相对于主窗口而言的
	//控件注册的必要步骤：
	INITCOMMONCONTROLSEX icc;//注册控件
	icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icc.dwICC = ICC_BAR_CLASSES;//注册工具栏、状态栏、Trackbar和Tooltip类。
	InitCommonControlsEx(&icc);//写入

	ShowWindow(hWnd, SW_SHOWDEFAULT);//显示窗口属性。SW_SHOWEFAULT
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);//消息循环
		DispatchMessage(&msg);
	}
	//gdi使用完关闭
	GdiplusShutdown(gdiplusToken);
	return (int)msg.wParam;//返回鼠标键盘信息
}

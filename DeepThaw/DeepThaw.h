
// DeepThaw.h: PROJECT_NAME 应用程序的主头文件
//

#pragma once


#include "resource.h"		// 主符号


// CDeepThawApp:
// 有关此类的实现，请参阅 DeepThaw.cpp
//

class CDeepThawApp : public CWinApp
{
public:
	CDeepThawApp();

// 重写
public:
	virtual BOOL InitInstance();

// 实现

	DECLARE_MESSAGE_MAP()
};

extern CDeepThawApp theApp;

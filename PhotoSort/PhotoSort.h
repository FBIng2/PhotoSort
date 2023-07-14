
// PhotoSort.h : fichier d'en-tête principal de l'application PROJECT_NAME
//

#pragma once

#ifndef __AFXWIN_H__
	#error "incluez 'pch.h' avant d'inclure ce fichier pour PCH"
#endif

#include "resource.h"		// symboles principaux
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <mutex>
//#include <functional>

#define PROG_VERSION 2.0

#define IMAGEFILE		0
#define VIDEOFILE		1

#define HTMLNORMALFILE		0
#define HTMLSLIDESHOWFILE	1

namespace fs = std::filesystem;
using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

// CPhotoSortApp :
// Consultez PhotoSort.cpp pour l'implémentation de cette classe
//

class CPhotoSortApp : public CWinApp
{
public:
	CPhotoSortApp();

// Substitutions
public:
	virtual BOOL InitInstance();

// Implémentation

	DECLARE_MESSAGE_MAP()
};

extern CPhotoSortApp theApp;

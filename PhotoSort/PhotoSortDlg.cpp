
// PhotoSortDlg.cpp : fichier d'implémentation
//

#include "pch.h"
#include "framework.h"
#include "PhotoSort.h"
#include "PhotoSortDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

std::atomic_int cptfiles;
std::atomic_bool bContinue;
std::atomic_bool StopThread;
std::atomic_int NbThreadLightFiles;
std::atomic_int NbThreadHeavyFiles;
CString FileProcessed;
std::mutex mLock;

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lParam*/, LPARAM lpData)
{

	if (uMsg == BFFM_INITIALIZED)
	{
		//std::string tmp = (const char*)lpData;
		//std::cout << "path: " << tmp << std::endl;
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}

	return 0;
}

CString BrowseFolder()
{
	TCHAR path[MAX_PATH];

	BROWSEINFO bi = { 0 };
	bi.lpszTitle = _T("Browse for folder...");
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = BrowseCallbackProc;
	//bi.lParam = (LPARAM)path_param;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

	if (pidl != 0)
	{
		//get the name of the folder and put it in path
		SHGetPathFromIDList(pidl, path);

		//free memory used
		IMalloc* imalloc = 0;
		if (SUCCEEDED(SHGetMalloc(&imalloc)))
		{
			imalloc->Free(pidl);
			imalloc->Release();
		}

		return path;
	}

	return L"";
}


 void RetrieveFileCreationDate(CString FileName, std::vector<CString> *mDate)
{
	CString strTmp;
	WIN32_FILE_ATTRIBUTE_DATA fInfo;

	GetFileAttributesEx(FileName, GetFileExInfoStandard, &fInfo);

	SYSTEMTIME sys = { 0 };
	FileTimeToSystemTime(&fInfo.ftLastWriteTime, &sys);

	strTmp.Format(_T("%d"), sys.wYear);
	mDate->push_back(strTmp);
	strTmp.Format(_T("%d"), sys.wMonth);
	mDate->push_back(strTmp);
	strTmp.Format(_T("%d"), sys.wDay);
	mDate->push_back(strTmp);
}

int Refresh()
{
	MSG       msg;
	if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		if ((msg.message == WM_QUIT)
			|| (msg.message == WM_CLOSE)
			|| (msg.message == WM_DESTROY)
			|| (msg.message == WM_NCDESTROY)
			|| (msg.message == WM_HSCROLL)
			|| (msg.message == WM_VSCROLL)
			)
			return(1);
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return(0);
}


char* GetUTF8Data(CString InputData)
{
	size_t converted, msize;
	char* result;
	msize = InputData.GetLength() + 1;
	if (bContinue == TRUE)
	{
		result = new char[msize];
		memset(result, 0, msize);
		wcstombs_s(&converted, result, (size_t)msize, InputData.GetBuffer(), (size_t)msize - 1);
		result[msize - 1] = 0;
	}
	else
	{
		result = new char[1];
		result[0] = 0;
		StopThread = TRUE;
	}

	return result;
}

FileResultClass ExtractDataFromList(std::vector<FileResultClass>* InputList, CString YearToFind)
{
	FileResultClass tmphtmlresult;

	for (int j = 0; j < (int)InputList->size(); j++)
	{
		if (InputList->at(j).Year == YearToFind)
		{
			tmphtmlresult = InputList->at(j);
			InputList->erase(InputList->begin() + j);
			break;
			//
		}
	}

	return tmphtmlresult;
}

CString ToUppercaseString(CString strInput)
{
	CString result;
	std::string DataUpperCase;
	char* tmp;
	tmp = GetUTF8Data(strInput);
	DataUpperCase.append(tmp);
	delete[] tmp;

	std::transform(DataUpperCase.begin(), DataUpperCase.end(), DataUpperCase.begin(), [](char c) {return static_cast<char>(std::toupper(c)); });

	result = DataUpperCase.c_str();
	return result;
}

char* GetFileData(CString FileName, long* sizereturn)
{
	std::vector<CString> Out;
	CString result, tmpyear, tmpmonth;
	long size;
	char* buffer = NULL, * UTF8Data;

	UTF8Data = GetUTF8Data(FileName);
	std::ifstream file(UTF8Data, std::ios::in | std::ios::binary | std::ios::ate);
	file.seekg(0, std::ifstream::end);
	size = (long)file.tellg();
	file.seekg(0);
	*sizereturn = (long)size;
	if (*sizereturn > 0)
	{
		file.seekg(0, std::ios::beg);
		buffer = new char[(long)size];
		memset(buffer, 0, (long)size);
		file.read(buffer, (long)size);
	}
	file.close();

	delete[] UTF8Data;

	return buffer;
}


long long GetFileSize(CString FileName)
{
	std::streampos size;
	char* UTF8Data;

	UTF8Data = GetUTF8Data(FileName);

	std::ifstream file(UTF8Data, std::ios::in | std::ios::binary | std::ios::ate);
	size = file.tellg();
	file.close();
	return size;
}


void WriteFileDataInfos(CString InputFileName, CString FileData)
{
	FILE* fp;
	char* tmpfilename, * bufferdata;

	tmpfilename = GetUTF8Data(InputFileName);
	bufferdata = GetUTF8Data(FileData);

	fopen_s(&fp, tmpfilename, "w");
	if (fp != NULL)
	{
		fwrite(bufferdata, sizeof(char), strlen(bufferdata), fp);
		fclose(fp);
	}

	delete[] bufferdata;
	delete[] tmpfilename;

}




bool IsOctetIsBetweenZeroAndNine(char Octet)
{

	if ((Octet >= 0x30) && (Octet <= 0x39))
		return true;
	else
		return false;

}


CString GetMonthStringForDate(CString InputMonth)
{
	CString result;

	if (_wtoi(InputMonth) == 1)
		result = "January";
	else if (_wtoi(InputMonth) == 2)
		result = "February";
	else if (_wtoi(InputMonth) == 3)
		result = "March";
	else if (_wtoi(InputMonth) == 4)
		result = "April";
	else if (_wtoi(InputMonth) == 5)
		result = "May";
	else if (_wtoi(InputMonth) == 6)
		result = "June";
	else if (_wtoi(InputMonth) == 7)
		result = "July";
	else if (_wtoi(InputMonth) == 8)
		result = "August";
	else if (_wtoi(InputMonth) == 9)
		result = "September";
	else if (_wtoi(InputMonth) == 10)
		result = "October";
	else if (_wtoi(InputMonth) == 11)
		result = "November";
	else if (_wtoi(InputMonth) == 12)
		result = "December";
	else
		result = "";

	return result;
}

CString GetMonthString(CString InputMonth)
{
	CString result;

	if (_wtoi(InputMonth) == 1)
		result = "01-January";
	else if (_wtoi(InputMonth) == 2)
		result = "02-February";
	else if (_wtoi(InputMonth) == 3)
		result = "03-March";
	else if (_wtoi(InputMonth) == 4)
		result = "04-April";
	else if (_wtoi(InputMonth) == 5)
		result = "05-May";
	else if (_wtoi(InputMonth) == 6)
		result = "06-June";
	else if (_wtoi(InputMonth) == 7)
		result = "07-July";
	else if (_wtoi(InputMonth) == 8)
		result = "08-August";
	else if (_wtoi(InputMonth) == 9)
		result = "09-September";
	else if (_wtoi(InputMonth) == 10)
		result = "10-October";
	else if (_wtoi(InputMonth) == 11)
		result = "11-November";
	else if (_wtoi(InputMonth) == 12)
		result = "12-December";
	else
		result = "";

	return result;
}



void SortFinalResults(std::vector <FileResultClass>* InputVector)
{
	int i, j;
	FileResultClass tmpdata;

	for (i = 0; i < (int)InputVector->size() - 1; ++i)
	{
		for (j = 0; j < (int)InputVector->size() - i - 1; ++j)
		{
			if (_wtoi(InputVector->at(j).Year) > _wtoi(InputVector->at(j + 1).Year))
			{
				tmpdata = InputVector->at(j);
				InputVector->at(j) = InputVector->at(j + 1);
				InputVector->at(j + 1) = tmpdata;
			}
		}
	}
}


void SortFilesByTags(CPhotoSortDlg* MainDlg)
{
	unsigned int i, j;
	FileDataClass tmpdata;
	//*******************************************************************************
	//SORT FILES
	//*******************************************************************************

	CWnd* pStaticCtrl = MainDlg->GetDlgItem(IDC_PercentStatic);
	MainDlg->m_ProgressCtrl.SetPos(0);
	MainDlg->pInformationCtrl->SetWindowTextW(_T("SORTING IMAGES PER YEAR"));
	//SORT DATAS PER YEARS

	if (MainDlg->ListOfFiles->size() == 0)
		return;

	for (i = 0; i < MainDlg->ListOfFiles->size() - 1; ++i)
	{
		for (j = 0; j < MainDlg->ListOfFiles->size() - i - 1; ++j)
		{
			if (_wtoi(MainDlg->ListOfFiles->at(j).YearTag) > _wtoi(MainDlg->ListOfFiles->at(j + 1).YearTag))
			{
				tmpdata = MainDlg->ListOfFiles->at(j);
				MainDlg->ListOfFiles->at(j) = MainDlg->ListOfFiles->at(j + 1);
				MainDlg->ListOfFiles->at(j + 1) = tmpdata;
			}
		}
		double percent = (i / (double(MainDlg->ListOfFiles->size()))) * 100;
		CString mpercent;
		mpercent.Format(_T("%0.2f%%"), percent);
		pStaticCtrl->SetWindowTextW(mpercent);
		MainDlg->m_ProgressCtrl.SetPos(i);
	}

	MainDlg->m_ProgressCtrl.SetPos(0);
	MainDlg->pInformationCtrl->SetWindowTextW(_T("SORTING IMAGES PER MONTH"));
	//SORT DATAS PER MONTH
	for (i = 0; i < MainDlg->ListOfFiles->size() - 1; ++i)
	{
		for (j = 0; j < MainDlg->ListOfFiles->size() - i - 1; ++j)
		{
			if ((_wtoi(MainDlg->ListOfFiles->at(j).MonthTag) > _wtoi(MainDlg->ListOfFiles->at(j + 1).MonthTag) && (_wtoi(MainDlg->ListOfFiles->at(j).YearTag)) == _wtoi(MainDlg->ListOfFiles->at(j + 1).YearTag)))
			{
				tmpdata = MainDlg->ListOfFiles->at(j);
				MainDlg->ListOfFiles->at(j) = MainDlg->ListOfFiles->at(j + 1);
				MainDlg->ListOfFiles->at(j + 1) = tmpdata;
			}
		}
		double percent = (i / (double(MainDlg->ListOfFiles->size()))) * 100;
		CString mpercent;
		mpercent.Format(_T("%0.2f%%"), percent);
		pStaticCtrl->SetWindowTextW(mpercent);
		MainDlg->m_ProgressCtrl.SetPos(i);
	}

	CString mpercent;
	mpercent = "100%";
	pStaticCtrl->SetWindowTextW(mpercent);
	MainDlg->m_ProgressCtrl.SetPos(MainDlg->ListOfFiles->size());

	MainDlg->pInformationCtrl->SetWindowTextW(_T("CREATE HTML FILE RESULT"));

}


void AddHTMLVideoInfoToHTMLPage(FileDataClass* InputFileElement, CString* HTMLOut, CString* OldMonth)
{
	CString strtmp, infodate, strrelativepath;
	CString CurrentMonthTag;

	CurrentMonthTag = InputFileElement->MonthTag;

	if (CurrentMonthTag != *OldMonth)
	{
		*HTMLOut += "</div>\n";
		strtmp.Format(_T("<div class = \"separator\"><b><p style = \"color:blue; font-size:18px;\">%s</p></b></div>\n"), CurrentMonthTag.GetString());
		*HTMLOut += strtmp;
		*HTMLOut += "<div class=\"video-container\">\n";
	}

	strrelativepath.Format(_T("%s\\%s\\%s\\%s"), InputFileElement->YearTag.GetString(), InputFileElement->MonthTag.GetString(), InputFileElement->SupFolder.GetString(), InputFileElement->FileName.GetString());
	*HTMLOut += "<div class=\"video-item\">\n";
	*HTMLOut += "<video controls>";
	strtmp.Format(_T("<source src=\"%s\" type = \"video/mp4\">\n"), strrelativepath.GetString());
	*HTMLOut += strtmp;
	*HTMLOut += "</video>\n";
	*HTMLOut += "<div class=\"video-caption\">";
	infodate.Format(_T("%s %s %s"), InputFileElement->DayTag.GetString(), InputFileElement->MonthTagForDate.GetString(), InputFileElement->YearTag.GetString());
	//strtmp.Format(_T("<p style = \"font-size:16px;\">%s</p>\n"), InputFileElement->FileName.GetString());
	strtmp.Format(_T("<p style = \"font-size:16px;\"><a href=\"%s\">%s<br>%s</a></p>\n"), strrelativepath.GetString(), InputFileElement->FileName.GetString(), infodate.GetString());
	*HTMLOut += strtmp;
	*HTMLOut += "</div>\n</div>\n";
}


void AddHTMLImageInfoToHTMLPage(FileDataClass* InputFileElement, CString* HTMLOut, CString* OldMonth)
{
	CString strtmp, infodate, strrelativepath;
	CString CurrentMonthTag;


	CurrentMonthTag = InputFileElement->MonthTag;
	
		if (CurrentMonthTag != *OldMonth)
		{
			*HTMLOut += "</div>\n";
			*HTMLOut += "<div class=\"row\">\n";
			*HTMLOut += "<hr>";
			strtmp.Format(_T("<b><p style = \"color:blue; font-size:20px;\">%s</p></b>\n"), CurrentMonthTag.GetString());
			*HTMLOut += strtmp;
		}

		strrelativepath.Format(_T("%s\\%s\\%s\\%s"), InputFileElement->YearTag.GetString(), InputFileElement->MonthTag.GetString(), InputFileElement->SupFolder.GetString(), InputFileElement->FileName.GetString());
		*HTMLOut += "<div class = \"column\">\n";
		strtmp.Format(_T("<img src=\"%s\" style = \"width:100%%\">"), strrelativepath.GetString());
		*HTMLOut += strtmp;

		infodate.Format(_T("%s %s %s"), InputFileElement->DayTag.GetString(), InputFileElement->MonthTagForDate.GetString(), InputFileElement->YearTag.GetString());
		strtmp.Format(_T("<div class = \"centered\"><a href=\"%s\">%s<br>%s</a></div></div>\n"), strrelativepath.GetString(), InputFileElement->FileName.GetString(), infodate.GetString());
		*HTMLOut += strtmp;
	

}


void CreateHTMLSildeShowOtherJPEGFiles(CPhotoSortDlg* MainDlg, std::vector <FileResultClass>* ResultHTMLYear)
{
	int IndexStart = 0, IndexStop = 0;;
	int i, j;
	int CptImagesFiles = 0;
	CString CurrentYear, HTMLOut, strrelativepath, strtmp, HTMLHeader, HTMLEnding, CurrentMonth, OldCurrentMonth, infodate;
	FileResultClass tmphtmlresult;

	HTMLHeader = "<!DOCTYPE html>\n";
	HTMLHeader += "<html>\n";
	HTMLHeader += "<head>\n";
	HTMLHeader += "<meta name = \"viewport\" content = \"width = device - width, initial - scale = 1\">\n";
	HTMLHeader += "<style>\n";
	
	HTMLHeader += "body {\n";
	HTMLHeader += "font-family: Arial;\n";
	HTMLHeader += "margin: 0;\n";
	HTMLHeader += "}\n";
	HTMLHeader += "* {\n";
	HTMLHeader += "box-sizing: border-box;\n";
	HTMLHeader += "}\n";
	HTMLHeader += "img {\n";
	HTMLHeader += "object-fit:contain;\n";
	HTMLHeader += "width:100%;\n";
	HTMLHeader += "border: solid 2px #000000;\n";
	HTMLHeader += "align-items: center;\n";
	HTMLHeader += "height: 800px;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".container {\n";
	HTMLHeader += "position: absolute;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".mySlides {\n";
	HTMLHeader += "display: none;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".cursor {\n";
	HTMLHeader += "  cursor: pointer;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".prev,\n";
	HTMLHeader += ".next {\n";
	HTMLHeader += "cursor: pointer;\n";
	HTMLHeader += "position: absolute;\n";
	HTMLHeader += "top: 400px;\n";
	HTMLHeader += "width: auto;\n";
	HTMLHeader += "padding: 40px;\n";
	HTMLHeader += "margin-top: -100px;\n";
	HTMLHeader += "margin-left: 0px;\n";
	HTMLHeader += "margin-right: 50px\n";
	HTMLHeader += "color: black;\n";
	HTMLHeader += "font-weight: bold;\n";
	HTMLHeader += "font-size: 20px;\n";
	HTMLHeader += "border-radius: 0 3px 3px 0;\n";
	HTMLHeader += "background-color: #F2F2F2;\n";
	HTMLHeader += "-webkit-user-select: none;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".next {\n";
	HTMLHeader += "right: 0;\n";
	HTMLHeader += "border-radius: 3px 0 0 3px;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".prev:hover,\n";
	HTMLHeader += ".next:hover {\n";
	HTMLHeader += "background-color: rgba(0, 255, 0, 0.8);\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".numbertext {\n";
	HTMLHeader += "color: #000000;\n";
	HTMLHeader += "font-size: 24px;\n";
	HTMLHeader += "padding: 8px 12px;\n";
	HTMLHeader += "position: absolute;\n";
	HTMLHeader += "top: 0;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".caption-container {\n";
	HTMLHeader += "text-align: center;\n";
	HTMLHeader += "background-color: #808B96;\n";
	HTMLHeader += "padding: 2px 16px;\n";
	HTMLHeader += "color: blue;\n";
	HTMLHeader += "font-size: 20px;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".row:after {\n";
	HTMLHeader += "content: \"\";\n";
	HTMLHeader += "display: table;\n";
	HTMLHeader += "clear: both;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".column {\n";
	HTMLHeader += "float: left;\n";
	HTMLHeader += "width: 16.66%;\n";
	HTMLHeader += "}\n";
	HTMLHeader += "video.center{\n";
	HTMLHeader += "display: block;\n";
	HTMLHeader += "margin-left: auto;\n";
	HTMLHeader += "margin-right: auto;\n";
	HTMLHeader += "}\n";
  
	HTMLHeader += ".demo {\n";
	HTMLHeader += "opacity: 0.4;\n";
	HTMLHeader += "object-fit:contain;\n";
	HTMLHeader += "width:100%;\n";
	HTMLHeader += "border: solid 1px #CCC;\n";
	HTMLHeader += "align-items: center;\n";
	HTMLHeader += "height: 500px;\n";
	HTMLHeader += "border-radius: 0 3px 3px 0;\n";
	HTMLHeader += "background-color: #000000;\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".active,\n";
	HTMLHeader += ".demo:hover {\n";
	HTMLHeader += "opacity: 1;\n";
	HTMLHeader += "}\n";
	HTMLHeader += "</style>\n";
	HTMLHeader += "<body>\n";

	HTMLEnding = "<script>\n";
	HTMLEnding += "let slideIndex = 1;\n";
	HTMLEnding += "let RefreshInput = 5;\n";
	HTMLEnding += "let OldRefreshInput = 1;\n";
	HTMLEnding += "showSlides(slideIndex);\n";
	HTMLEnding += "let intervalleTimer = window.setInterval(Timer, 5000);\n";
	HTMLEnding += "function plusSlides(n) {\n";
	HTMLEnding += "showSlides(slideIndex += n);\n";
	HTMLEnding += "}\n";
	HTMLEnding += "function currentSlide(n) {\n";
	HTMLEnding += "showSlides(slideIndex = n);\n";
	HTMLEnding += "}\n";
	HTMLEnding += "function showSlides(n) {\n";
	HTMLEnding += "let i;\n";
	HTMLEnding += "let slides = document.getElementsByClassName(\"mySlides\");\n";
	HTMLEnding += "let dots = document.getElementsByClassName(\"demo\");\n";
	HTMLEnding += "let captionText = document.getElementById(\"caption\");\n";
	HTMLEnding += "if (n > slides.length) { slideIndex = 1 };\n";
	HTMLEnding += "if (n < 1) { slideIndex = slides.length };\n";
	HTMLEnding += "for (i = 0; i < slides.length; i++) {\n";
	HTMLEnding += "slides[i].style.display = \"none\";\n";
	HTMLEnding += "}\n";
	HTMLEnding += "for (i = 0; i < dots.length; i++) {\n";
	HTMLEnding += "dots[i].className = dots[i].className.replace(\" active\", \"\");\n";
	HTMLEnding += "}\n";
	HTMLEnding += "slides[slideIndex - 1].style.display = \"block\";\n";
	HTMLEnding += "dots[slideIndex - 1].className += \" active\";\n";
	HTMLEnding += "let test = document.getElementById(dots[slideIndex - 1].id);\n";
	HTMLEnding += "captionText.innerHTML = test.id;\n";
	HTMLEnding += "}\n";
	HTMLEnding += "function Timer()\n";
	HTMLEnding += "{\n";
	HTMLEnding += "slideIndex++;\n";
	HTMLEnding += "showSlides(slideIndex);\n";
	HTMLEnding += "RefreshInput = document.getElementById(\"TimeIntervalId\").value;\n";
	HTMLEnding += "if (RefreshInput >= 1)\n";
	HTMLEnding += "{\n";
	HTMLEnding += "if (RefreshInput != OldRefreshInput)\n";
	HTMLEnding += "{\n";
	HTMLEnding += "clearInterval(intervalleTimer);\n";
	HTMLEnding += "intervalleTimer = window.setInterval(Timer, RefreshInput*1000);\n";
	HTMLEnding += "}\n";
	HTMLEnding += "}\n";
	HTMLEnding += "OldRefreshInput=RefreshInput;\n";
	HTMLEnding += "}\n";
	HTMLEnding += "function ManageTimer()\n";
	HTMLEnding += "{\n";
	HTMLEnding += "var elem = document.getElementById(\"BtnTimer\");\n";
	HTMLEnding += "if (elem.innerHTML == \"Stop\")\n";
	HTMLEnding += "{\n";
	HTMLEnding += "elem.innerHTML = \"Start\";\n";
	HTMLEnding += "clearInterval(intervalleTimer);\n";
	HTMLEnding += "}\n";
	HTMLEnding += "else\n";
	HTMLEnding += "{\n";
	HTMLEnding += "elem.innerHTML = \"Stop\";\n";
	HTMLEnding += "RefreshInput = document.getElementById(\"TimeIntervalId\").value;\n";
	HTMLEnding += "if (RefreshInput >= 1)\n";
	HTMLEnding += "intervalleTimer = window.setInterval(Timer, RefreshInput*1000);\n";
	HTMLEnding += "else\n";
	HTMLEnding += "intervalleTimer = window.setInterval(Timer, 5000);\n";
	HTMLEnding += "}\n";
	HTMLEnding += "}\n";
	HTMLEnding += "</script>\n";
	HTMLEnding += "</body>\n";
	HTMLEnding += "</html>\n";

	for (i = 0; i < (int)MainDlg->ListOfFiles->size(); i++)
	{
		if (MainDlg->ListOfFiles->at(i).YearTag.GetLength() > 1)
		{
			IndexStart = i;
			CurrentYear = MainDlg->ListOfFiles->at(i).YearTag;
			break;
		}
	}

	for (i = IndexStart; i < (int)MainDlg->ListOfFiles->size(); i++)
	{

		CurrentYear = MainDlg->ListOfFiles->at(i).YearTag;
		CptImagesFiles = 0;
		HTMLOut.Format(_T("<h2 style=\"text-align:center\">Slideshow Gallery of Year %s</h2>\n"), CurrentYear.GetString());
		HTMLOut += "<label for = \"TimeInterValLabel\" style=\"color:blue; font-size:24px;\">Change Time Intervall between Pictures(in s) :</label>\n";
		HTMLOut += "<input type = \"number\" id = \"TimeIntervalId\" name = \"TimeInterval\" min = \"1\" max = \"100\">\n";
		HTMLOut += "<button id = \"BtnTimer\" onclick = \"ManageTimer()\" style=\"color:blue; font-size:24px;\">Stop</button>\n";
		HTMLOut += "<br><br>\n";
		HTMLOut += "<div class=\"container\">\n";
		for (j = i; j < (int)MainDlg->ListOfFiles->size(); j++)
		{
			if (MainDlg->ListOfFiles->at(j).YearTag == CurrentYear)
				CptImagesFiles++;

			if ((MainDlg->ListOfFiles->at(j).YearTag != CurrentYear) || (j == ((int)MainDlg->ListOfFiles->size() - 1)))
			{
				IndexStop = j;
				break;
			}
		}
		tmphtmlresult = ExtractDataFromList(ResultHTMLYear, CurrentYear);
		int cpt = 1;
		for (j = i; j < (int)IndexStop; j++)
		{
			if (MainDlg->ListOfFiles->at(j).FileType == IMAGEFILE)
			{
				strrelativepath.Format(_T("%s\\%s\\%s\\%s"), MainDlg->ListOfFiles->at(j).YearTag.GetString(), MainDlg->ListOfFiles->at(j).MonthTag.GetString(), MainDlg->ListOfFiles->at(j).SupFolder.GetString(), MainDlg->ListOfFiles->at(j).FileName.GetString());
				HTMLOut += "<div class=\"mySlides\">\n";
				strtmp.Format(_T("<div class=\"numbertext\">%d / %d</div>\n"), cpt, CptImagesFiles);
				HTMLOut += strtmp;
				strtmp.Format(_T("<img src=\"%s\">\n"), strrelativepath.GetString());
				HTMLOut += strtmp;
				HTMLOut += "</div>\n";
				cpt++;
			}

			if (MainDlg->ListOfFiles->at(j).FileType == VIDEOFILE)
			{
				strrelativepath.Format(_T("%s\\%s\\%s\\%s"), MainDlg->ListOfFiles->at(j).YearTag.GetString(), MainDlg->ListOfFiles->at(j).MonthTag.GetString(), MainDlg->ListOfFiles->at(j).SupFolder.GetString(), MainDlg->ListOfFiles->at(j).FileName.GetString());
				HTMLOut += "<div class=\"mySlides\">\n";
				strtmp.Format(_T("<div class=\"numbertext\">%d / %d</div>\n"), cpt, CptImagesFiles);
				HTMLOut += strtmp;
				HTMLOut += "<video class=\"center\" controls>";
				strtmp.Format(_T("<source src=\"%s\" type = \"video/mp4\">\n"), strrelativepath.GetString());
				HTMLOut += strtmp;
				HTMLOut += "</video>\n";
				HTMLOut += "</div>\n";
				cpt++;
			}
		}

		HTMLOut += "<a class=\"prev\" onclick=\"plusSlides(-1)\"><b>PRV</a>\n";
		HTMLOut += "<a class=\"next\" onclick=\"plusSlides(1)\"><b>NXT</a>\n";
		HTMLOut += "<div class=\"caption-container\">\n";
		HTMLOut += "<p id = \"caption\"></p>\n";
		HTMLOut += "<div class=\"row\">\n";
		
		cpt = 1;
		OldCurrentMonth = "";
		for (j = i; j < (int)IndexStop; j++)
		{
			if (MainDlg->ListOfFiles->at(j).FileType == IMAGEFILE)
			{
				CurrentMonth = MainDlg->ListOfFiles->at(j).MonthTag;
				if (CurrentMonth != OldCurrentMonth)
				{
					HTMLOut += "</div>\n";
					HTMLOut += "<div class=\"row\">\n";
					strtmp.Format(_T("<hr><p>%s<br></p>\n"), CurrentMonth.GetString());
					HTMLOut += strtmp;
				}

				strrelativepath.Format(_T("%s\\%s\\%s\\%s"), MainDlg->ListOfFiles->at(j).YearTag.GetString(), MainDlg->ListOfFiles->at(j).MonthTag.GetString(), MainDlg->ListOfFiles->at(j).SupFolder.GetString(), MainDlg->ListOfFiles->at(j).FileName.GetString());
				HTMLOut += "<div class=\"column\">\n";
				infodate.Format(_T("Date:%s %s %s"), MainDlg->ListOfFiles->at(j).DayTag.GetString(), MainDlg->ListOfFiles->at(j).MonthTagForDate.GetString(), MainDlg->ListOfFiles->at(j).YearTag.GetString());
				strtmp.Format(_T("<img class=\"demo cursor\" id=\"%s %s\" src=\"%s\" style=\"width:100%%\" onclick=\"currentSlide(%d)\" alt=\"%s %s\">\n"), MainDlg->ListOfFiles->at(j).FileName.GetString(),infodate.GetString(),strrelativepath.GetString(), cpt, MainDlg->ListOfFiles->at(j).FileName.GetString(), infodate.GetString());
				HTMLOut += strtmp;
				HTMLOut += "</div>\n";
				OldCurrentMonth = CurrentMonth;
				cpt++;
			}

			if (MainDlg->ListOfFiles->at(j).FileType == VIDEOFILE)
			{
				CurrentMonth = MainDlg->ListOfFiles->at(j).MonthTag;
				if (CurrentMonth != OldCurrentMonth)
				{
					HTMLOut += "</div>\n";
					HTMLOut += "<div class=\"row\">\n";
					strtmp.Format(_T("<hr><p>%s<br></p>\n"), CurrentMonth.GetString());
					HTMLOut += strtmp;
				}

				strrelativepath.Format(_T("%s\\%s\\%s\\%s"), MainDlg->ListOfFiles->at(j).YearTag.GetString(), MainDlg->ListOfFiles->at(j).MonthTag.GetString(), MainDlg->ListOfFiles->at(j).SupFolder.GetString(), MainDlg->ListOfFiles->at(j).FileName.GetString());
				HTMLOut += "<div class=\"column\">\n";
				infodate.Format(_T("Date:%s %s %s"), MainDlg->ListOfFiles->at(j).DayTag.GetString(), MainDlg->ListOfFiles->at(j).MonthTagForDate.GetString(), MainDlg->ListOfFiles->at(j).YearTag.GetString());
				strtmp.Format(_T("<video class=\"demo cursor\" id=\"%s %s\" controls>"), MainDlg->ListOfFiles->at(j).FileName.GetString(),infodate.GetString());
				HTMLOut += strtmp;
				strtmp.Format(_T("<source src=\"%s\" type = \"video/mp4\" onclick=\"currentSlide(%d)\" title=\"%s %s\">\n"), strrelativepath.GetString(), cpt, MainDlg->ListOfFiles->at(j).FileName.GetString(), infodate.GetString());
				HTMLOut += strtmp;
				HTMLOut += "</video>\n";
				HTMLOut += "</div>\n";
				OldCurrentMonth = CurrentMonth;
				cpt++;
			}
			
		}

		HTMLOut += "</div>\n</div>\n";
		tmphtmlresult.FileDataSlideShow = HTMLHeader + HTMLOut + HTMLEnding;
		tmphtmlresult.FileNameSlideShow.Format(_T("%s\\ResultSlideShow%s.html"), MainDlg->OutputFolderPath.GetString(), tmphtmlresult.Year.GetString());
		ResultHTMLYear->push_back(tmphtmlresult);
		i = IndexStop - 1;
		if (IndexStop == ((int)MainDlg->ListOfFiles->size() - 1))
			break;
	}

}


void CreateHTMLInfoForJPEGFiles(CPhotoSortDlg* MainDlg, std::vector <FileResultClass>* ResultHTMLYear)
{
	CString CurrentYear, CurrentMonth, strtmp;
	unsigned int i, j;
	FileResultClass tmphtmlresult;
	CString OutHTMLResult;
	int IndexStart = 0;
	int CptFile = 0;


	for (i = 0; i < MainDlg->ListOfFiles->size(); i++)
	{
		if (MainDlg->ListOfFiles->at(i).YearTag.GetLength() > 1)
		{
			IndexStart = i;
			break;
		}
	}

	for (i = IndexStart; i < MainDlg->ListOfFiles->size(); i++)
	{

		CurrentYear = MainDlg->ListOfFiles->at(i).YearTag;
		OutHTMLResult.Format(_T("<h2> PICTURES OF YEAR %s</h2>\n"), CurrentYear.GetString());
		tmphtmlresult.Month = "";
		for (j = i; j < MainDlg->ListOfFiles->size(); j++)
		{
			if ((MainDlg->ListOfFiles->at(j).YearTag == CurrentYear) && (MainDlg->ListOfFiles->at(j).FileType == IMAGEFILE))
			{
				AddHTMLImageInfoToHTMLPage(&MainDlg->ListOfFiles->at(j), &OutHTMLResult, &tmphtmlresult.Month);
				CptFile++;
			}
			else if ((MainDlg->ListOfFiles->at(j).YearTag != CurrentYear) && (MainDlg->ListOfFiles->at(j).FileType == IMAGEFILE))
			{
				OutHTMLResult += "</div>\n";
				tmphtmlresult.Year = CurrentYear;
				tmphtmlresult.NbFilesImages.Format(_T("%d"), CptFile);
				tmphtmlresult.FileData = OutHTMLResult;//creation fichier html de l'année
				tmphtmlresult.FileName.Format(_T("%s\\Result%s.html"), MainDlg->OutputFolderPath.GetString(), tmphtmlresult.Year.GetString());
				ResultHTMLYear->push_back(tmphtmlresult);
				i = j - 1;
				CptFile = 0;
				break;
			}

			if (j == MainDlg->ListOfFiles->size() - 1)
			{
				if (MainDlg->ListOfFiles->at(j).FileType == IMAGEFILE)
					AddHTMLImageInfoToHTMLPage(&MainDlg->ListOfFiles->at(j), &OutHTMLResult, &tmphtmlresult.Month);
				OutHTMLResult += "</div>\n";
				tmphtmlresult.NbFilesImages.Format(_T("%d"), CptFile);
				tmphtmlresult.FileData = OutHTMLResult;//creation fichier html de l'année
				tmphtmlresult.FileName.Format(_T("%s\\Result%s.html"), MainDlg->OutputFolderPath.GetString(), tmphtmlresult.Year.GetString());
				ResultHTMLYear->push_back(tmphtmlresult);
				i = j;
				CptFile = 0;
				break;
			}

			tmphtmlresult.Month = MainDlg->ListOfFiles->at(j).MonthTag;
			tmphtmlresult.Year = MainDlg->ListOfFiles->at(j).YearTag;
		}

	}

}


void CreateHTMLInfoForMovieFiles(CPhotoSortDlg* MainDlg, std::vector <FileResultClass>* ResultHTMLYear)
{
	CString CurrentYear, CurrentMonth, strtmp, OldYear, OldMonth;
	unsigned int i, j;
	FileResultClass tmphtmlresult;
	CString OutHTMLResult, HTMLHeaderFile;
	FileDataClass PreviousData;
	int IndexStart = 0;
	int CptFile = 0;

	for (i = 0; i < MainDlg->ListOfFiles->size(); i++)
	{
		if (MainDlg->ListOfFiles->at(i).YearTag.GetLength() > 1)
		{
			IndexStart = i;
			break;
		}
	}

	CurrentYear = MainDlg->ListOfFiles->at(IndexStart).YearTag;


	for (i = IndexStart; i < MainDlg->ListOfFiles->size(); i++)
	{
		CurrentYear = MainDlg->ListOfFiles->at(i).YearTag;
		OutHTMLResult = "";
		tmphtmlresult = ExtractDataFromList(ResultHTMLYear, CurrentYear);
		tmphtmlresult.Month = "";
		HTMLHeaderFile.Format(_T("<h2> VIDEOS OF YEAR %s</h2>\n"), CurrentYear.GetString());
		for (j = i; j < MainDlg->ListOfFiles->size(); j++)
		{
			if ((MainDlg->ListOfFiles->at(j).YearTag == CurrentYear) && (MainDlg->ListOfFiles->at(j).FileType == VIDEOFILE))
			{

				AddHTMLVideoInfoToHTMLPage(&MainDlg->ListOfFiles->at(j), &OutHTMLResult, &tmphtmlresult.Month);
				CptFile++;
			}
			else if ((MainDlg->ListOfFiles->at(j).YearTag != CurrentYear) && (MainDlg->ListOfFiles->at(j).FileType == VIDEOFILE))
			{
				OutHTMLResult += "</div>\n";
				tmphtmlresult.Year = CurrentYear;
				tmphtmlresult.NbFilesVideo.Format(_T("%d"), CptFile);
				if (CptFile > 0)
					tmphtmlresult.FileData = tmphtmlresult.FileData + HTMLHeaderFile + OutHTMLResult;//creation fichier html de l'année
				tmphtmlresult.FileName.Format(_T("%s\\Result%s.html"), MainDlg->OutputFolderPath.GetString(), CurrentYear.GetString());
				ResultHTMLYear->push_back(tmphtmlresult);
				OutHTMLResult = "";
				i = j - 1;
				CptFile = 0;
				break;
			}

			if (j == MainDlg->ListOfFiles->size() - 1)
			{
				if (MainDlg->ListOfFiles->at(j).FileType == VIDEOFILE)
					AddHTMLVideoInfoToHTMLPage(&MainDlg->ListOfFiles->at(j), &OutHTMLResult, &tmphtmlresult.Month);
				if (CptFile > 0)
				{
					OutHTMLResult = HTMLHeaderFile + OutHTMLResult;
					OutHTMLResult += "</div>\n";
				}
				else
					OutHTMLResult = "";
				tmphtmlresult.NbFilesVideo.Format(_T("%d"), CptFile);
				tmphtmlresult.FileData = tmphtmlresult.FileData + OutHTMLResult;//concatenate
				tmphtmlresult.FileName.Format(_T("%s\\Result%s.html"), MainDlg->OutputFolderPath.GetString(), tmphtmlresult.Year.GetString());
				ResultHTMLYear->push_back(tmphtmlresult);
				i = j;
				CptFile = 0;
				break;
			}

			if (MainDlg->ListOfFiles->at(j).FileType == VIDEOFILE)
			{
				tmphtmlresult.Month = MainDlg->ListOfFiles->at(j).MonthTag;
				tmphtmlresult.Year = MainDlg->ListOfFiles->at(j).YearTag;
			}
		}

	}

}


void CreateHTMLInfoForEmptyTagFiles(CPhotoSortDlg* MainDlg, std::vector <FileResultClass>* ResultHTMLYear)
{
	CString OutHTMLResult, strtmp;
	FileResultClass tmphtmlresult;
	unsigned int i, CptFileJPEG, CptFileVideo;

	OutHTMLResult += "<h2> LISTING OF UNKNOWN DATE FILES </h2>";
	OutHTMLResult += "<div class=\"row\">\n";
	CptFileJPEG = 0;
	CptFileVideo = 0;

	for (i = 0; i < MainDlg->ListOfFiles->size(); i++) //WRITE PICTURE FILES ONLY
	{
		if ((MainDlg->ListOfFiles->at(i).YearTag.GetLength() == 1) && (MainDlg->ListOfFiles->at(i).FileType == IMAGEFILE))
		{
			OutHTMLResult += "<div class = \"column\">\n";
			strtmp.Format(_T("<img src=\"%s\" style = \"width:100%%\"><a href=\"%s\">%s</a>"), MainDlg->ListOfFiles->at(i).CompletePath.GetString(), MainDlg->ListOfFiles->at(i).CompletePath.GetString(), MainDlg->ListOfFiles->at(i).FileName.GetString());
			OutHTMLResult += strtmp;
			OutHTMLResult += "</div>\n";
			CptFileJPEG++;
		}
	}

	OutHTMLResult += "</div>\n";

	for (i = 0; i < MainDlg->ListOfFiles->size(); i++)
	{
		if ((MainDlg->ListOfFiles->at(i).YearTag.GetLength() == 1) && (MainDlg->ListOfFiles->at(i).FileType == VIDEOFILE))
		{
			OutHTMLResult += "<div class=\"video-container\">\n";
			OutHTMLResult += "<div class=\"video-item\">\n";
			OutHTMLResult += "<video controls>";
			strtmp.Format(_T("<source src=\"%s\" type = \"video/mp4\">\n"), MainDlg->ListOfFiles->at(i).CompletePath.GetString());
			OutHTMLResult += strtmp;
			OutHTMLResult += "</video>\n";
			OutHTMLResult += "<div class=\"video-caption\">";
			strtmp.Format(_T("<p style = \"font-size:16px;\">%s</p>\n"), MainDlg->ListOfFiles->at(i).FileName.GetString());
			OutHTMLResult += strtmp;
			OutHTMLResult += "</div>\n</div>\n";
			CptFileVideo++;
		}
	}

	if ((CptFileJPEG > 0) || (CptFileVideo > 0))
	{
		OutHTMLResult += "</div>\n";
		tmphtmlresult.Year = "UNKNOWN";
		tmphtmlresult.FileData = OutHTMLResult; //on concatene
		tmphtmlresult.NbFilesImages.Format(_T("%d"), CptFileJPEG);
		tmphtmlresult.NbFilesVideo.Format(_T("%d"), CptFileVideo);
		tmphtmlresult.FileName.Format(_T("%s\\ResultUNKNOWNDATES.html"), MainDlg->OutputFolderPath.GetString());
		ResultHTMLYear->push_back(tmphtmlresult);
	}

}



void GenerateHTMLSummary(CPhotoSortDlg* MainDlg)
{
	unsigned long i;
	CString OutputHTMLFileName, HTMLHeader, HTMLEnding, HTMLFileToWrite, strtmp;
	std::vector <FileResultClass> ResultHTMLYear;
	FileResultClass tmphtmlresult;

	HTMLHeader = "<!DOCTYPE html>\n";
	HTMLHeader += "<html>\n";
	HTMLHeader += "<head>\n";
	HTMLHeader += "<style>\n";

	HTMLHeader += "* {\r\n  ";
	HTMLHeader += "box-sizing: border-box;\r\n";
	HTMLHeader += "	}\r\n\r\n";

	HTMLHeader += ".column {\r\n";
	HTMLHeader += "  float: left;\r\n";
	HTMLHeader += "  width: 20%;\r\n";
	HTMLHeader += "  padding: 5px;\r\n";
	HTMLHeader += "}\r\n\r\n";

	HTMLHeader += ".row::after {\r\n";
	HTMLHeader += "  content: \"\";\r\n";
	HTMLHeader += "  clear: both;\r\n";
	HTMLHeader += "  display: table;\r\n";
	HTMLHeader += "}\n";
	HTMLHeader += ".centered{\r\n";
	HTMLHeader += " position: relative;\r\n";
	HTMLHeader += "  top: 50%;\r\n";
	HTMLHeader += "  left: 50%;\r\n";
	HTMLHeader += "  font-size: 16px;\r\n";
	HTMLHeader += "  padding-top: 30px;\r\n";
	HTMLHeader += "  transform: translate(-50%,-50%);\r\n";
	HTMLHeader += "}\r\n";

	HTMLHeader += ".video-container {\r\n";
	HTMLHeader += "display: flex;\r\n";
	HTMLHeader += "flex-wrap: wrap;\r\n";
	HTMLHeader += "}\r\n";

	HTMLHeader += ".video-item {\r\n";
	HTMLHeader += "flex: 0 0 20%;\r\n";
	HTMLHeader += "padding: 10px;\r\n";
	HTMLHeader += "box-sizing: border-box;\r\n";
	HTMLHeader += "}\r\n";

	HTMLHeader += " .video-item video {\r\n";
	HTMLHeader += " width: 100%;\r\n";
	HTMLHeader += "height: auto;\r\n";
	HTMLHeader += "}\r\n";

	HTMLHeader += ".video-caption {\r\n";
	HTMLHeader += "margin-top: 5px;\r\n";
	HTMLHeader += "}\r\n";

	HTMLHeader += ".separator{\r\n";
	HTMLHeader += "border-top: 5px solid #000;\r\n";
	HTMLHeader += "margin: 20px 0;\r\n";
	HTMLHeader += "}\r\n";

	HTMLHeader += "</style>\n";
	HTMLHeader += "</head>\n";
	HTMLHeader += "<body>\n\n";

	HTMLEnding = "</body>\n";
	HTMLEnding += "</html>\n";

	SortFilesByTags(MainDlg);

	for (i = 0; i < (int)MainDlg->ListOfFiles->size(); i++)
	{
		MainDlg->ListOfFiles->at(i).MonthTag = GetMonthString(MainDlg->ListOfFiles->at(i).MonthTag);
		MainDlg->ListOfFiles->at(i).MonthTagForDate = GetMonthStringForDate(MainDlg->ListOfFiles->at(i).MonthTagForDate);
	}

	//*******************************************************************************
	//LIST IMAGES
	//*******************************************************************************
	CreateHTMLInfoForJPEGFiles(MainDlg, &ResultHTMLYear);

	//*******************************************************************************
	//LIST VIDEOS
	//*******************************************************************************
	CreateHTMLInfoForMovieFiles(MainDlg, &ResultHTMLYear);

	//*******************************************************************************
	//LIST EMPTY TAG FILES
	//*******************************************************************************
	CreateHTMLInfoForEmptyTagFiles(MainDlg, &ResultHTMLYear);

	CreateHTMLSildeShowOtherJPEGFiles(MainDlg, &ResultHTMLYear);

	//*******************************************************************************
	//WRITE SEPARATED HTML FILES
	//*******************************************************************************

	//RAJOUTER FONCTION DE TRI FINALE SUR ResultHTMLYear
	SortFinalResults(&ResultHTMLYear);

	for (i = 0; i < (int)ResultHTMLYear.size(); i++)
	{
		ResultHTMLYear[i].FileData = HTMLHeader + ResultHTMLYear[i].FileData + HTMLEnding;
		WriteFileDataInfos(ResultHTMLYear[i].FileName, ResultHTMLYear[i].FileData);
		WriteFileDataInfos(ResultHTMLYear[i].FileNameSlideShow, ResultHTMLYear[i].FileDataSlideShow);
	}

	OutputHTMLFileName = MainDlg->OutputFolderPath.GetString();
	OutputHTMLFileName += "\\Results.html";

	HTMLFileToWrite = "<!DOCTYPE html>\n";
	HTMLFileToWrite += "<html>\n";

	HTMLFileToWrite += "<head>\n";
	HTMLFileToWrite += "<style>\n";
	HTMLFileToWrite += "table, th, td{\n";
	HTMLFileToWrite += "border: 1px solid black;\n";
	HTMLFileToWrite += "border - collapse: collapse;\n";
	HTMLFileToWrite += "}\n";
	HTMLFileToWrite += "th{\n";
	HTMLFileToWrite += "text-align: left;\n";
	HTMLFileToWrite += "}\n";
	HTMLFileToWrite += "</style>\n";
	HTMLFileToWrite += "</head>\n";

	HTMLFileToWrite += "<body>\n";
	HTMLFileToWrite += "<h2> RESULTS OF PROCESSING \n";
	strtmp.Format(_T("<br>TOTAL NUMBER OF FILES PROCESSED:%d\n"), MainDlg->ListOfFiles->size());
	HTMLFileToWrite += strtmp;
	strtmp.Format(_T("<br>FILE GENERATED BY:%s</h2>\n"), MainDlg->MainAppWindowTittle.GetString());
	HTMLFileToWrite += strtmp;


	HTMLFileToWrite += "<table style=\"width:100%\">\n";
	HTMLFileToWrite += "<tr>";
	HTMLFileToWrite += "<th> YEAR </th>\n";
	HTMLFileToWrite += "<th> FILE RESULT </th>\n";
	HTMLFileToWrite += "<th> Number of Picture Files </th>\n";
	HTMLFileToWrite += "<th> Number of Video Files </th>\n";
	HTMLFileToWrite += "</tr>";

	for (i = 0; i < (int)ResultHTMLYear.size(); i++)
	{
		HTMLFileToWrite += "<tr>";
		strtmp.Format(_T("<td style=\"color:black; font-size:24px;\">%s</td>\n<td style=\"color:blue; font-size:24px;\"><a href=\"%s\">%s</a></td>\n"), ResultHTMLYear[i].Year.GetString(), ResultHTMLYear[i].FileName.GetString(), ResultHTMLYear[i].FileName.GetString());
		HTMLFileToWrite += strtmp;
		strtmp.Format(_T("<td style=\"color:black; font-size:24px;\">%d</td>\n<td style=\"color:blue; font-size:24px;\">%d</td>\n"), _wtoi(ResultHTMLYear[i].NbFilesImages.GetString()), _wtoi(ResultHTMLYear[i].NbFilesVideo.GetString()));
		HTMLFileToWrite += strtmp;
		HTMLFileToWrite += "</tr>";
	}

	HTMLFileToWrite += "</table>\n";
	HTMLFileToWrite += "<br>\n";
	HTMLFileToWrite += "<p style = \"font-size:24px;\">SLIDE SHOW PAGES LIST</p>\n";

	HTMLFileToWrite += "<table style=\"width:100%\">\n";
	HTMLFileToWrite += "<tr>";
	HTMLFileToWrite += "<th> YEAR </th>\n";
	HTMLFileToWrite += "<th> SLIDE SHOW FILE </th>\n";
	HTMLFileToWrite += "</tr>";

	for (i = 0; i < (int)ResultHTMLYear.size(); i++)
	{
		HTMLFileToWrite += "<tr>";
		strtmp.Format(_T("<td style=\"color:black; font-size:24px;\">%s</td>\n<td style=\"color:blue; font-size:24px;\"><a href=\"%s\">%s</a></td>\n"), ResultHTMLYear[i].Year.GetString(), ResultHTMLYear[i].FileNameSlideShow.GetString(), ResultHTMLYear[i].FileNameSlideShow.GetString());
		HTMLFileToWrite += strtmp;
		HTMLFileToWrite += "</tr>";
	}

	HTMLFileToWrite += "</table>\n";

	HTMLFileToWrite += HTMLEnding;

	WriteFileDataInfos(OutputHTMLFileName, HTMLFileToWrite);
	ShellExecute(MainDlg->m_hWnd, _T("open"), OutputHTMLFileName.GetString(), NULL, NULL, SW_SHOWDEFAULT);
}




long GetTotalNumberOfFilesToTreat(CPhotoSortDlg* MainDlg, CString InputFolderStr)
{
	std::wstring FileName, strFiles;
	long NbCount = 0;
	wchar_t buffer[255];

	for (auto const& dir_entry : fs::recursive_directory_iterator(InputFolderStr.GetString(), fs::directory_options::skip_permission_denied))
	{

		//FileName.SetString(dir_entry.path().c_str());
		FileName = dir_entry.path();
		if (FileName.find('.') > 0)
			NbCount++;

		swprintf_s(buffer, _T("%d"), NbCount);
		strFiles = _T("STEP 1/5:COUNTING TOTAL NUMBER OF FILES ----> ");
		strFiles.append(buffer);
		MainDlg->pInformationCtrl->SetWindowTextW(strFiles.c_str());
		Refresh();
	}


	return (long)NbCount;
	/*char buf[1024];
	
	Command.Format(_T("C:\\Windows\\System32\\cmd.exe start /min /c dir / a:-d / s / b \"%s\" | find /c \":\\\""), InputFolderStr.GetString());
	*/
}

std::vector<CString> GetListOfFilesInFolder(CString InputFolder)
{
	std::vector<CString> Result;
	CString FileName;
	long Index = 0;

	for (auto const& dir_entry : fs::recursive_directory_iterator(InputFolder.GetString(), fs::directory_options::skip_permission_denied))
	{
		FileName.SetString(dir_entry.path().c_str());

		if ((FileName.Find('.') > 0) && (FileName.Find(_T("htm")) < 0) && (FileName.Find(_T("txt")) < 0))
		{
			Index = FileName.ReverseFind('\\');
			FileName = FileName.Mid(Index + 1);
			Result.push_back(FileName);
		}
	}

	return Result;
}


int VerifyCopyOfFiles(CPhotoSortDlg* MainDlg)
{
	int result = 1;
	std::vector<CString> Result;
	Result = GetListOfFilesInFolder(MainDlg->OutputFolderPath);

	if (Result.size() != (int)MainDlg->ListOfFiles->size())
		result = -1;
	else
		result = 1;

	return result;
}

//TRY TO GET DATETIME FORMAT of type xxxx(:or-)yy(:or-)zz
void GetStandardDateFormat(std::vector<CString>* OutResult, char* buffer, long* size)
{
	long IndexStartYear = -1, IndexStopMonth = -1;
	CString tmpyear, tmpmonth;
	char Separator = 0;
	long i;
	DataString result;
	std::vector<CString> out;


	for (i = 12; i < *size; i++)
	{
		if (((buffer[i - 12] >= 0) && (buffer[i - 12] <= 0x20)) || ((buffer[i - 12] == 't') && (buffer[i - 11] == 'e')))
			if (IsOctetIsBetweenZeroAndNine(buffer[i - 10]))
				if (IsOctetIsBetweenZeroAndNine(buffer[i - 9]))
					if (IsOctetIsBetweenZeroAndNine(buffer[i - 8]))
						if (IsOctetIsBetweenZeroAndNine(buffer[i - 7]))
							if ((buffer[i - 6] == ':') || (buffer[i - 6] == '-'))
								if ((IsOctetIsBetweenZeroAndNine(buffer[i - 5]) && (IsOctetIsBetweenZeroAndNine(buffer[i - 4]))))
									if ((buffer[i - 3] == ':') || (buffer[i - 3] == '-'))
										if ((IsOctetIsBetweenZeroAndNine(buffer[i - 2]) && (IsOctetIsBetweenZeroAndNine(buffer[i - 1]))))
										{
											IndexStartYear = i - 10;
											IndexStopMonth = i;
											Separator = buffer[i - 3];
											break;
										}

	}

	if (IndexStartYear > 0)
	{
		for (long j = IndexStartYear; j < IndexStopMonth; j++)
			result.AppendChar((unsigned char)buffer[j]);

		out = result.Split(Separator);
		for (int j = 0; j < (int)out.size(); j++)
			OutResult->push_back(out[j]);

	}

}


void GetCanonDateFormat(std::vector<CString>* OutResult, char* buffer, long* size)
{
	long IndexStart = -1, IndexStop = 0;
	CString tmpyear, tmpmonth;
	DataString Result;
	std::vector<CString> Fields;

	for (long i = 8; i < *size; i++)
	{
		if (IsOctetIsBetweenZeroAndNine(buffer[i - 8]))
			if (IsOctetIsBetweenZeroAndNine(buffer[i - 7]))
				if (buffer[i - 6] == ':')
					if ((IsOctetIsBetweenZeroAndNine(buffer[i - 5])) && (IsOctetIsBetweenZeroAndNine(buffer[i - 4])))
						if (buffer[i - 3] == ':')
							if ((IsOctetIsBetweenZeroAndNine(buffer[i - 2])) && (IsOctetIsBetweenZeroAndNine(buffer[i - 1])))
							{
								IndexStart = i;
								break;
							}

	}

	if (IndexStart > 0)
	{
		for (long j = IndexStart; j > IndexStart - 50; j--)
		{
			if (buffer[j] == 0)
			{
				IndexStart = j + 1;
				break;
			}
		}

		for (long j = IndexStart; j < IndexStart + 50; j++)
		{
			if (buffer[j] == 0)
			{
				IndexStop = j;
				break;
			}
		}

		for (long j = IndexStart; j < IndexStop; j++)
			Result.AppendChar((unsigned char)buffer[j]);

		Result.Remove(13);
		Result.Remove(10);

		if (Result.Find(' ') < 0)
			return;

		Fields = Result.Split(' ');
		if (Fields.size() < 4)
			return;
		else
			tmpyear = Fields[4];


		if (Fields[1] == "JAN")
			tmpmonth = "01";
		else if (Fields[1] == "FEB")
			tmpmonth = "02";
		else if (Fields[1] == "MAR")
			tmpmonth = "03";
		else if (Fields[1] == "APR")
			tmpmonth = "04";
		else if (Fields[1] == "MAY")
			tmpmonth = "05";
		else if (Fields[1] == "JUN")
			tmpmonth = "06";
		else if (Fields[1] == "JUL")
			tmpmonth = "07";
		else if (Fields[1] == "AUG")
			tmpmonth = "08";
		else if (Fields[1] == "SEP")
			tmpmonth = "09";
		else if (Fields[1] == "OCT")
			tmpmonth = "10";
		else if (Fields[1] == "NOV")
			tmpmonth = "11";
		else if (Fields[1] == "DEC")
			tmpmonth = "12";
		else
			tmpmonth = "";

		OutResult->push_back(tmpyear);
		OutResult->push_back(tmpmonth);
		OutResult->push_back(Fields[2]);
	}

}

// A DEBUGGER SUR UN FICHIER AVI FUJI
void GetFujiDateFormat(std::vector<CString>* OutResult, char* buffer, long* size)
{
	long IndexStart = -1, IndexStop = -1;
	CString tmpyear, tmpmonth;
	DataString Result;
	std::vector<CString> Fields;

	for (long i = 8; i < *size; i++)
	{
		if (IsOctetIsBetweenZeroAndNine(buffer[i - 8]))
			if (IsOctetIsBetweenZeroAndNine(buffer[i - 7]))
				if (buffer[i - 6] == ':')
					if ((IsOctetIsBetweenZeroAndNine(buffer[i - 5])) && (IsOctetIsBetweenZeroAndNine(buffer[i - 4])))
						if (buffer[i - 3] == ':')
							if ((IsOctetIsBetweenZeroAndNine(buffer[i - 2])) && (IsOctetIsBetweenZeroAndNine(buffer[i - 1])))
							{
								IndexStart = i;
								break;
							}

	}

	if (IndexStart > 0)
	{
		for (long j = IndexStart; j > IndexStart - 50; j--)
		{
			if (buffer[j] == 0)
			{
				IndexStart = j + 1;
				break;
			}
		}

		for (long j = IndexStart; j < IndexStart + 50; j++)
		{
			if (buffer[j] == 0)
			{
				IndexStop = j;
				break;
			}
		}

		for (long j = IndexStart; j < IndexStop; j++)
			Result.AppendChar((unsigned char)buffer[j]);

		Result.Remove(13);
		Result.Remove(10);

		if (Result.Find(' ') < 0)
			return;

		Fields = Result.Split(' ');
		if (Fields.size() < 4)
			return;
		else
			tmpyear = Fields[4];


		if (Fields[1] == "JAN")
			tmpmonth = "01";
		else if (Fields[1] == "FEB")
			tmpmonth = "02";
		else if (Fields[1] == "MAR")
			tmpmonth = "03";
		else if (Fields[1] == "APR")
			tmpmonth = "04";
		else if (Fields[1] == "MAY")
			tmpmonth = "05";
		else if (Fields[1] == "JUN")
			tmpmonth = "06";
		else if (Fields[1] == "JUL")
			tmpmonth = "07";
		else if (Fields[1] == "AUG")
			tmpmonth = "08";
		else if (Fields[1] == "SEP")
			tmpmonth = "09";
		else if (Fields[1] == "OCT")
			tmpmonth = "10";
		else if (Fields[1] == "NOV")
			tmpmonth = "11";
		else if (Fields[1] == "DEC")
			tmpmonth = "12";
		else
			tmpmonth = "";

		OutResult->push_back(tmpyear);
		OutResult->push_back(tmpmonth);
	}

}



int windows_system(const char* cmd)
{
	PROCESS_INFORMATION p_info;
	STARTUPINFO s_info;
	LPWSTR cmdline;

	memset(&s_info, 0, sizeof(s_info));
	memset(&p_info, 0, sizeof(p_info));
	s_info.cb = sizeof(s_info);

	CString tmp(cmd);
	cmdline = _wcsdup(tmp);

	if (CreateProcess(NULL, cmdline, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &s_info, &p_info))
	{
		WaitForSingleObject(p_info.hProcess, INFINITE);
		CloseHandle(p_info.hProcess);
		CloseHandle(p_info.hThread);
	}

	delete[] cmdline;
	return 1;
}

int CopyLightFileBinaryStream(FileDataClass  InputFileName)
{
	CString tmpsrc;
	tmpsrc.Format(_T("%s\\%s"), InputFileName.SourceFolder.GetString(), InputFileName.FileName.GetString());
	CopyFile(tmpsrc.GetString(), InputFileName.CompletePath.GetString(), FALSE);
	
	NbThreadLightFiles--;
	return 1;
}


int CopyHeavyFileBinaryStream(FileDataClass  InputFileName)
{
	CString tmpsrc;
	tmpsrc.Format(_T("%s\\%s"), InputFileName.SourceFolder.GetString(), InputFileName.FileName.GetString());
	CopyFile(tmpsrc.GetString(), InputFileName.CompletePath.GetString(), FALSE);

	NbThreadHeavyFiles--;
	return 1;
}


/*int CopyFileBinary(FileDataClass  InputFileName)
{
	CString command;
	char* tmpdata;
	
	command.Format(_T("C:\\windows\\system32\\robocopy \"%s\" \"%s\" \"%s\" /MT:8"), InputFileName.SourceFolder.GetString(), InputFileName.Path.GetString(), InputFileName.FileName.GetString());
	tmpdata = GetUTF8Data(command);
	windows_system(tmpdata);
	delete[] tmpdata;
	NbThread--;

	return 1;
}*/

void ThreadManageLightFileCopy(std::vector<FileDataClass> *InputVector, CPhotoSortDlg* MainDlg)
{
	int deltaThread;
	CString mpercent, InfosProcess, TotalSizeStr;
	double sizetotal;

	for (int i = 0; i < (int)InputVector->size(); i++)
	{
		if (bContinue == FALSE)
			return ;
		deltaThread = InputVector->size() - i;
		if (deltaThread >= 8)
			deltaThread = 8;

		NbThreadLightFiles = deltaThread;
		for (int j = (int)i; j < (int)i + deltaThread; j++)
		{
			std::thread{CopyLightFileBinaryStream,InputVector->at(j)}.detach();
			mLock.lock();
			MainDlg->TotalSize = MainDlg->TotalSize - InputVector->at(j).FileSize;
			double percent = (cptfiles / (double(MainDlg->ListOfFiles->size()))) * 100;
			mpercent.Format(_T("%0.2f%%"), percent);
			MainDlg->pStaticCtrl->SetWindowTextW(mpercent);
			InfosProcess.Format(_T("PERFORM THE COPY --> FILE HAS BEEN COPIED TO %s"), MainDlg->ListOfFiles->at(i).CompletePath.GetString());
			MainDlg->pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
			MainDlg->m_ProgressCtrl.SetPos(cptfiles);
			sizetotal = double(MainDlg->TotalSize) / 1e9;
			TotalSizeStr.Format(_T("%0.02f Gb"), sizetotal);
			MainDlg->pStaticSizeCtrl->SetWindowTextW(TotalSizeStr.GetString());
			cptfiles++;
			mLock.unlock();
		}

		while (NbThreadLightFiles > 0)
		{
			Sleep(100);
		}

		i = i + deltaThread - 1;
	}

	return;
}


void ThreadManageHeavyFileCopy(std::vector<FileDataClass>* InputVector, CPhotoSortDlg* MainDlg)
{
	int deltaThread;
	CString mpercent, InfosProcess, TotalSizeStr;
	double sizetotal;

	for (int i = 0; i < (int)InputVector->size(); i++)
	{
		if (bContinue == FALSE)
			return;
		deltaThread = InputVector->size() - i;
		if (deltaThread >= 4)
			deltaThread = 4;

		NbThreadHeavyFiles = deltaThread;
		for (int j = i; j < (int)i + deltaThread; j++)
		{
			std::thread{CopyHeavyFileBinaryStream,InputVector->at(j) }.detach();
			mLock.lock();
			MainDlg->TotalSize = MainDlg->TotalSize - InputVector->at(j).FileSize;
			double percent = (cptfiles / (double(MainDlg->ListOfFiles->size()))) * 100;
			mpercent.Format(_T("%0.2f%%"), percent);
			MainDlg->pStaticCtrl->SetWindowTextW(mpercent);
			InfosProcess.Format(_T("PERFORM THE COPY --> FILE HAS BEEN COPIED TO %s"), MainDlg->ListOfFiles->at(i).CompletePath.GetString());
			MainDlg->pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
			MainDlg->m_ProgressCtrl.SetPos(cptfiles);
			sizetotal = double(MainDlg->TotalSize) / 1e9;
			TotalSizeStr.Format(_T("%0.02f Gb"), sizetotal);
			MainDlg->pStaticSizeCtrl->SetWindowTextW(TotalSizeStr.GetString());
			cptfiles++;
			mLock.unlock();
		}

		while (NbThreadHeavyFiles > 0)
		{
			Sleep(100);
		}

		i = i + deltaThread - 1;
		//cptfiles += deltaThread;
	}

	return;
}

__int64 GetAvailableFreeSpace(CString InputFolder)
{
	__int64 lpFreeBytesAvailable, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes;

	//DWORD dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwTotalClusters;
	BOOL test;

	// If the function succeeds, the return value is nonzero. If the function fails, the return value is 0 (zero).

	test = GetDiskFreeSpaceEx(

		InputFolder,

		(PULARGE_INTEGER)&lpFreeBytesAvailable,

		(PULARGE_INTEGER)&lpTotalNumberOfBytes,

		(PULARGE_INTEGER)&lpTotalNumberOfFreeBytes

	);

	if (test > 0)
		return lpFreeBytesAvailable;
	else
		return 0;
}



static DWORD WINAPI ThreadProcessInformations(CPhotoSortDlg* MainDlg, CProgressCtrl* m_ProgressCtrl)
{
	unsigned long i;
	CString InfosProcess, strFileName, TotalSizeStr;
	int Cond = 0;

	StopThread = true;
	for (i = 0; i < MainDlg->ListOfFiles->size(); i++)
	{
		if (bContinue == TRUE)
		{
			if ((MainDlg->ListOfFiles->at(i).FileExtension == "JPG") || (MainDlg->ListOfFiles->at(i).FileExtension == "JPEG"))
				MainDlg->ManageJPEGPhotos(&MainDlg->ListOfFiles->at(i));
			else if (MainDlg->ListOfFiles->at(i).FileExtension == "MP4")
				MainDlg->ManageMP4File(&MainDlg->ListOfFiles->at(i));
			else if (MainDlg->ListOfFiles->at(i).FileExtension == "MOV")
				MainDlg->ManageMOVFile(&MainDlg->ListOfFiles->at(i));
			else if (MainDlg->ListOfFiles->at(i).FileExtension == "PNG")
				MainDlg->ManagePNGFile(&MainDlg->ListOfFiles->at(i));
			else if (MainDlg->ListOfFiles->at(i).FileExtension == "AVI")
				MainDlg->ManageAVIFile(&MainDlg->ListOfFiles->at(i));
			else
			{
				MainDlg->CopyToUnsortedFolder(&MainDlg->ListOfFiles->at(i));
				InfosProcess.Format(_T("Step 4/5:Processing File %s --> UNSORTED FOLDER"), MainDlg->ListOfFiles->at(i).FileName.GetString());
				MainDlg->pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
			}

			double percent = (i / (double(MainDlg->ListOfFiles->size()))) * 100;
			CString mpercent;
			mpercent.Format(_T("%0.2f%%"), percent);
			MainDlg->pStaticCtrl->SetWindowTextW(mpercent);
			m_ProgressCtrl->SetPos(i);
		}
		else
			return 1;
	}

	std::vector<FileDataClass> LightFileList;
	std::vector<FileDataClass> HeavyFileList;
	//Split the vector on 2 vectors depending on the file size before the copy process
	for (i = 0; i < MainDlg->ListOfFiles->size(); i++)
	{
		if (MainDlg->ListOfFiles->at(i).FileSize < 5e6)
			LightFileList.push_back(MainDlg->ListOfFiles->at(i));
		else
			HeavyFileList.push_back(MainDlg->ListOfFiles->at(i));
	}

	//VERIFY THE AVAILABLE SPACE IN THE DESTINATION DRIVE
	__int64 Result;
	Result=GetAvailableFreeSpace(MainDlg->OutputFolderPath);

	if (Result < (MainDlg->TotalSize*0.2))
	{

		MessageBox(NULL, _T("Not enought space available to perform the copy.Please choose another location"), _T("Error"), MB_OK + MB_ICONSTOP);
		return 0;
	}
	

	//PERFORM THE COPY
	cptfiles = 0;
	std::thread th1{ ThreadManageLightFileCopy,&LightFileList,MainDlg};
	std::thread th2{ ThreadManageHeavyFileCopy,&HeavyFileList,MainDlg};
	th1.join();
	th2.join();
	CString mpercent;
	//NbThread = 0;
	/*CString mpercent;
	
	int CptTimeOut = 0;
	int deltaThread;
	for (i = 0; i < MainDlg->ListOfFiles->size(); i++)
	{
		CptTimeOut = 0;
		if (bContinue == FALSE)
			return 1;
		deltaThread = MainDlg->ListOfFiles->size()-i;
		if (deltaThread >= 4)
			deltaThread = 4;
		
		NbThread = deltaThread;
		std::thread th{ ThreadManageCopy,i,MainDlg};
		th.join();
		i = i + deltaThread - 1;
		double percent = (i / (double(MainDlg->ListOfFiles->size()))) * 100;
		mpercent.Format(_T("%0.2f%%"), percent);
		MainDlg->pStaticCtrl->SetWindowTextW(mpercent);
		InfosProcess.Format(_T("PERFORM THE COPY --> FILE HAS BEEN COPIED TO %s"), MainDlg->ListOfFiles->at(i).CompletePath.GetString());
		MainDlg->pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
		m_ProgressCtrl->SetPos(i);
		sizetotal = double(MainDlg->TotalSize) / 1e9;
		TotalSizeStr.Format(_T("%0.02f Gb"), sizetotal);
		MainDlg->pStaticSizeCtrl->SetWindowTextW(TotalSizeStr.GetString());
	}*/


	if (MainDlg->ListOfFiles->size() > 0)
	{
		double percent = (i / (double(MainDlg->ListOfFiles->size())) * 100);
		mpercent.Format(_T("%0.2f%%"), percent);
		MainDlg->pStaticCtrl->SetWindowTextW(mpercent);
		m_ProgressCtrl->SetPos(i);

		MainDlg->pInformationCtrl->SetWindowTextW(_T("VERIFYING COPY OF FILES IN THE OUTPUT FOLDER"));

		if (VerifyCopyOfFiles(MainDlg) < 0)
		{
			m_ProgressCtrl->SetPos(0);
			std::vector<CString> OutputFileList;
			MessageBox(MainDlg->m_hWnd, _T("Difference of the number of files copied has been detected"), _T("Information"), MB_OK);
			OutputFileList = GetListOfFilesInFolder(MainDlg->OutputFolderPath);
			InfosProcess = "LIST OF FILES MISSING IN THE OUTPUT FOLDER:\r\n";
			for (i = 0; i < (int)MainDlg->ListOfFiles->size(); i++)
			{

				strFileName = MainDlg->ListOfFiles->at(i).FileName;
				Cond = 0;
				for (int j = 0; j < (int)OutputFileList.size(); j++)
				{
					if (OutputFileList[j] == strFileName)
					{
						Cond = 1;
						break;
					}

				}

				if (Cond == 0)
				{
					InfosProcess += strFileName;
					InfosProcess += "\r\n";
				}

				m_ProgressCtrl->SetPos(i);
			}

			if (OutputFileList.size() == 0)
			{
				for (int j = 0; j < (int)MainDlg->ListOfFiles->size(); j++)
				{
					strFileName = MainDlg->ListOfFiles->at(j).FileName;
					InfosProcess += strFileName;
					InfosProcess += "\n";
				}
			}

			MainDlg->pInformationCtrl->SetWindowTextW(InfosProcess);
			CString LogFileName;
			LogFileName = MainDlg->OutputFolderPath;
			LogFileName += "\\";
			LogFileName += "ResultLogFile.txt";

			WriteFileDataInfos(LogFileName, InfosProcess);
			ShellExecute(MainDlg->m_hWnd, _T("open"), LogFileName.GetString(), NULL, NULL, SW_SHOWDEFAULT);
		}

		GenerateHTMLSummary(MainDlg);
	}

	StopThread = true;
	return 1;
}


// boîte de dialogue CAboutDlg utilisée pour la boîte de dialogue 'À propos de' pour votre application

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Données de boîte de dialogue
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // Prise en charge de DDX/DDV

// Implémentation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
	
END_MESSAGE_MAP()


// boîte de dialogue de CPhotoSortDlg



CPhotoSortDlg::CPhotoSortDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PHOTOSORT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPhotoSortDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_PROGRESSCTRL, m_ProgressCtrl);
}

BEGIN_MESSAGE_MAP(CPhotoSortDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BrowseSourceBtn, &CPhotoSortDlg::OnClickedBrowsesourcebtn)
	ON_BN_CLICKED(IDC_BrowseDestinationBtn, &CPhotoSortDlg::OnClickedBrowsedestinationbtn)
	ON_BN_CLICKED(IDC_StartBtn, &CPhotoSortDlg::OnClickedStartbtn)
	ON_BN_CLICKED(IDOK, &CPhotoSortDlg::OnBnClickedOk)
	ON_WM_CTLCOLOR()
	ON_COMMAND(ID_ABOUT, &CPhotoSortDlg::OnAboutMenu)
END_MESSAGE_MAP()


// gestionnaires de messages de CPhotoSortDlg

BOOL CPhotoSortDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Ajouter l'élément de menu "À propos de..." au menu Système.

	// IDM_ABOUTBOX doit se trouver dans la plage des commandes système.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	ListOfFiles = new std::vector<FileDataClass>;
	
	MainAppWindowTittle.Format(_T("PhotoSort V%0.2f"), PROG_VERSION);
	SetWindowText(MainAppWindowTittle);

	// Définir l'icône de cette boîte de dialogue.  L'infrastructure effectue cela automatiquement
	//  lorsque la fenêtre principale de l'application n'est pas une boîte de dialogue
	SetIcon(m_hIcon, TRUE);			// Définir une grande icône
	SetIcon(m_hIcon, FALSE);		// Définir une petite icône

	// TODO: ajoutez ici une initialisation supplémentaire

	return TRUE;  // retourne TRUE, sauf si vous avez défini le focus sur un contrôle
}

void CPhotoSortDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// Si vous ajoutez un bouton Réduire à votre boîte de dialogue, vous devez utiliser le code ci-dessous
//  pour dessiner l'icône.  Pour les applications MFC utilisant le modèle Document/Vue,
//  cela est fait automatiquement par l'infrastructure.

void CPhotoSortDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // contexte de périphérique pour la peinture

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Centrer l'icône dans le rectangle client
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Dessiner l'icône
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// Le système appelle cette fonction pour obtenir le curseur à afficher lorsque l'utilisateur fait glisser
//  la fenêtre réduite.
HCURSOR CPhotoSortDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

std::vector<CString>  CPhotoSortDlg::GetAVIDateTime(FileDataClass* FileName)
{
	char* buffer, * UTF8Data;
	std::vector<CString> Out;
	CString result, tmpyear, tmpmonth;

	UTF8Data = GetUTF8Data(FileName->CompletePath);
	std::ifstream file(UTF8Data, std::ios::in | std::ios::binary | std::ios::ate);
	file.seekg(0, std::ios::beg);
	buffer = new char[100000];
	while (file)
	{
		// Try to read next chunk of data
		file.read(buffer, 100000);
		// Get the number of bytes actually read
		long count = (long)file.gcount();
		// If nothing has been read, break
		if (!count)
			break;

		GetStandardDateFormat(&Out, buffer, &count);
		if (Out.size() == 0)
			GetCanonDateFormat(&Out, buffer, &count);
		if (Out.size() == 0)
			GetFujiDateFormat(&Out, buffer, &count);
		else if (Out.size() > 0)
		{
			file.close();
			break;
		}

	}

	delete[] UTF8Data;
	delete[] buffer;

	if (Out.size() == 0)
		RetrieveFileCreationDate(FileName->CompletePath, &Out);


	return Out;
}



std::vector<CString>  CPhotoSortDlg::GetMP4DateTime(FileDataClass* FileName)
{
	char* buffer, * UTF8Data;
	std::vector<CString> Out;
	CString result, tmpyear, tmpmonth;

	UTF8Data = GetUTF8Data(FileName->CompletePath);
	std::ifstream file(UTF8Data, std::ios::in | std::ios::binary | std::ios::ate);
	file.seekg(0, std::ios::beg);
	buffer = new char[100000];
	while (file)
	{
		// Try to read next chunk of data
		file.read(buffer, 100000);
		// Get the number of bytes actually read
		long count = (long)file.gcount();
		// If nothing has been read, break
		if (!count)
			break;

		GetStandardDateFormat(&Out, buffer, &count);
		if (Out.size() > 0)
		{
			file.close();
			break;
		}

	}


	delete[] UTF8Data;
	delete[] buffer;

	if (Out.size() == 0)
		RetrieveFileCreationDate(FileName->CompletePath, &Out);

	return Out;
}

std::vector<CString> CPhotoSortDlg::GetMOVDateTime(FileDataClass* FileName)
{
	std::vector<CString> Out;
	CString result, tmpyear, tmpmonth;
	char* buffer, * UTF8Data;
	
	UTF8Data = GetUTF8Data(FileName->CompletePath);
	std::ifstream file(UTF8Data, std::ios::in | std::ios::binary | std::ios::ate);
	file.seekg(0, std::ios::beg);
	buffer = new char[100000];
	
	while (file)
	{
		// Try to read next chunk of data
		file.read(buffer, 100000);
		// Get the number of bytes actually read
		long count = (long)file.gcount();
		// If nothing has been read, break
		if (!count)
			break;

		GetStandardDateFormat(&Out, buffer, &count);
		if (Out.size() > 0)
		{
			file.close();
			break;
		}

	}

	delete[] UTF8Data;
	delete[] buffer;

	if (Out.size() == 0)
		RetrieveFileCreationDate(FileName->CompletePath, &Out);


	return Out;
}

std::vector<CString>  CPhotoSortDlg::GetJPGDateTime(FileDataClass* FileName)
{
	std::vector<CString> Out;
	CString result, tmpyear, tmpmonth;
	char* buffer;
	long size;

	buffer = GetFileData(FileName->CompletePath, &size);
	FileName->FileSize = size;
	GetStandardDateFormat(&Out, buffer, &size);
	delete[] buffer;

	if (Out.size() == 0)
	 RetrieveFileCreationDate(FileName->CompletePath,&Out);

	return Out;
}


std::vector<CString>  CPhotoSortDlg::GetPNGDateTime(FileDataClass* FileName)
{
	char* buffer;
	long size;
	std::vector<CString> Out;
	CString  tmpyear, tmpmonth;
	DataString result;

	buffer = GetFileData(FileName->CompletePath, &size);
	FileName->FileSize = size;

	for (long i = 0; i < (long)(size)-4; i++)
	{
		if ((buffer[i] == 0x00) && (buffer[i + 1] == 0x00) && (buffer[i + 2] == 0x00) && (buffer[i + 3] == 0x00) && (buffer[i + 4] == 0x32))
		{
			for (int j = i + 4; j < i + 14; j++)
			{
				if (buffer[j] > 0)
					result.AppendChar((unsigned char)buffer[j]);
			}

			Out = result.Split(':');
			break;
		}

	}

	delete[] buffer;

	if (Out.size() == 0)
		RetrieveFileCreationDate(FileName->CompletePath, &Out);

	return Out;
}

void CPhotoSortDlg::ManageJPEGPhotos(FileDataClass* InputFileName)
{
	std::vector<CString> DateFields;
	CString command, outfilename, InfosProcess;
	//char* tmpdata;

	DateFields = GetJPGDateTime(InputFileName);
	if ((DateFields.size() > 0) && (DateFields.size() < 4))
	{
		if (_wtoi(DateFields[0]) > 0)
		{
			InputFileName->Path = OutputFolderPath + "\\" + DateFields[0];
			InputFileName->Path = InputFileName->Path + "\\" + GetMonthString(DateFields[1]) + "\\" + InputFileName->SupFolder;
			std::filesystem::create_directories(InputFileName->Path.GetString());
			InputFileName->YearTag = DateFields[0];
			InputFileName->MonthTag = DateFields[1];  //on laisse la valeur décimale car on effectuera le tri aprés
			InputFileName->MonthTagForDate = DateFields[1];
			InputFileName->DayTag = DateFields[2];

			//command.Format(_T("C:\\windows\\system32\\robocopy \"%s\" \"%s\" \"%s\""), InputFileName->SourceFolder.GetString(), tmppath.GetString(), InputFileName->FileName.GetString());
			InputFileName->CompletePath.Format(_T("%s\\%s"), InputFileName->Path.GetString(), InputFileName->FileName.GetString());
			
			InfosProcess.Format(_T("Step 4/5:Processing File %s --> The Time Tag has been found: %s %s"), InputFileName->FileName.GetString(), InputFileName->YearTag.GetString(), InputFileName->MonthTag.GetString());
			if (pInformationCtrl->GetSafeHwnd() != NULL)
			pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
		}
		else
			CopyToUnsortedFolder(InputFileName);
	}
	else
	{
		CopyToUnsortedFolder(InputFileName);
	}
}

void CPhotoSortDlg::ManageMOVFile(FileDataClass* InputFileName)
{

	std::vector<CString> DateFields;
	CString command, outfilename, InfosProcess;
	//char* tmpdata;

	DateFields = GetMOVDateTime(InputFileName);
	if ((DateFields.size() > 0) && (DateFields.size() < 4))
	{
		if (_wtoi(DateFields[0]) > 0)
		{
			InputFileName->Path = OutputFolderPath + "\\" + DateFields[0];
			InputFileName->Path = InputFileName->Path + "\\" + GetMonthString(DateFields[1]) + "\\" + InputFileName->SupFolder;
			std::filesystem::create_directories(InputFileName->Path.GetString());

			InputFileName->YearTag = DateFields[0];
			InputFileName->MonthTag = DateFields[1];
			InputFileName->MonthTagForDate = DateFields[1];
			InputFileName->DayTag = DateFields[2];

			//command.Format(_T("C:\\windows\\system32\\robocopy \"%s\" \"%s\" \"%s\""), InputFileName->SourceFolder.GetString(), tmppath.GetString(), InputFileName->FileName.GetString());
			InputFileName->CompletePath.Format(_T("%s\\%s"), InputFileName->Path.GetString(), InputFileName->FileName.GetString());
			
			InfosProcess.Format(_T("Step 4/5:Processing File %s --> The Time Tag has been found: %s %s"), InputFileName->FileName.GetString(), InputFileName->YearTag.GetString(), InputFileName->MonthTag.GetString());
			if (pInformationCtrl->GetSafeHwnd() != NULL)
			pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
		}
		else
			CopyToUnsortedFolder(InputFileName);
	}
	else
	{
		CopyToUnsortedFolder(InputFileName);
	}

}

void CPhotoSortDlg::ManageMP4File(FileDataClass* InputFileName)
{
	std::vector<CString> DateFields;
	CString command, outfilename, InfosProcess;
	//char* tmpdata;

	DateFields = GetMP4DateTime(InputFileName);
	if ((DateFields.size() > 0) && (DateFields.size() < 4))
	{
		if (_wtoi(DateFields[0]) > 0)
		{
			InputFileName->Path = OutputFolderPath + "\\" + DateFields[0];
			InputFileName->Path = InputFileName->Path + "\\" + GetMonthString(DateFields[1]) + "\\" + InputFileName->SupFolder;
			std::filesystem::create_directories(InputFileName->Path.GetString());

			InputFileName->YearTag = DateFields[0];
			InputFileName->MonthTag = DateFields[1];
			InputFileName->MonthTagForDate = DateFields[1];
			InputFileName->DayTag = DateFields[2];
			//command.Format(_T("C:\\windows\\system32\\robocopy \"%s\" \"%s\" \"%s\""), InputFileName->SourceFolder.GetString(), tmppath.GetString(), InputFileName->FileName.GetString());
			InputFileName->CompletePath.Format(_T("%s\\%s"), InputFileName->Path.GetString(), InputFileName->FileName.GetString());
			
			InfosProcess.Format(_T("Step 4/5:Processing File %s --> The Time Tag has been found: %s %s"), InputFileName->FileName.GetString(), InputFileName->YearTag.GetString(), InputFileName->MonthTag.GetString());
			if (pInformationCtrl->GetSafeHwnd() != NULL)
			pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
		}
		else
			CopyToUnsortedFolder(InputFileName);
	}
	else
	{
		CopyToUnsortedFolder(InputFileName);
	}

}

void CPhotoSortDlg::ManageAVIFile(FileDataClass* InputFileName)
{
	std::vector<CString> DateFields;
	CString command, outfilename, InfosProcess;
	//char* tmpdata;

	DateFields = GetAVIDateTime(InputFileName);
	if ((DateFields.size() > 0) && (DateFields.size() < 4))
	{
		if (_wtoi(DateFields[0]) > 0)
		{
			InputFileName->Path = OutputFolderPath + "\\" + DateFields[0];
			InputFileName->Path = InputFileName->Path + "\\" + GetMonthString(DateFields[1]) + "\\" + InputFileName->SupFolder;
			std::filesystem::create_directories(InputFileName->Path.GetString());

			InputFileName->YearTag = DateFields[0];
			InputFileName->MonthTag = DateFields[1];
			InputFileName->MonthTagForDate = DateFields[1];
			InputFileName->DayTag = DateFields[2];
			InputFileName->CompletePath.Format(_T("%s\\%s"), InputFileName->Path.GetString(), InputFileName->FileName.GetString());
			InfosProcess.Format(_T("Step 4/5:Processing File %s --> The Time Tag has been found: %s %s"), InputFileName->FileName.GetString(), InputFileName->YearTag.GetString(), InputFileName->MonthTag.GetString());
			if (pInformationCtrl->GetSafeHwnd() != NULL)
			pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
		}
		else
			CopyToUnsortedFolder(InputFileName);
	}
	else
	{
		CopyToUnsortedFolder(InputFileName);
	}

}

void CPhotoSortDlg::ManagePNGFile(FileDataClass* InputFileName)
{
	std::vector<CString> DateFields;
	CString command, outfilename, InfosProcess;
	FileDataClass tmpcopy;
	//char* tmpdata;

	DateFields = GetPNGDateTime(InputFileName);
	if ((DateFields.size() > 0) && (DateFields.size() < 4))
	{
		if (_wtoi(DateFields[0]) > 0)
		{
			InputFileName->Path = OutputFolderPath + "\\" + DateFields[0];
			InputFileName->Path = InputFileName->Path + "\\" + GetMonthString(DateFields[1]) + "\\" + InputFileName->SupFolder;
			std::filesystem::create_directories(InputFileName->Path.GetString());

			InputFileName->YearTag = DateFields[0];
			InputFileName->MonthTag = DateFields[1];
			InputFileName->MonthTagForDate = DateFields[1];
			InputFileName->DayTag = DateFields[2];
			InputFileName->CompletePath.Format(_T("%s\\%s"), InputFileName->Path.GetString(), InputFileName->FileName.GetString());

			InfosProcess.Format(_T("Step 4/5:Processing File %s --> The Time Tag has been found: %s %s"), InputFileName->FileName.GetString(), InputFileName->YearTag.GetString(), InputFileName->MonthTag.GetString());
			if (pInformationCtrl->GetSafeHwnd() != NULL)
			pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
		}
		else
			CopyToUnsortedFolder(InputFileName);
	}
	else
	{
		CopyToUnsortedFolder(InputFileName);
	}

}


void CPhotoSortDlg::CopyToUnsortedFolder(FileDataClass* InputFileName)
{
	CString tmppath, InfosProcess;
	char* tmpdata;
	CString command;

	tmppath.Format(_T("%s\\UNSORTED\\%s"), OutputFolderPath.GetString(), InputFileName->SupFolder.GetString());

	command.Format(_T("C:\\windows\\system32\\robocopy \"%s\" \"%s\" \"%s\""), InputFileName->SourceFolder.GetString(), tmppath.GetString(), InputFileName->FileName.GetString());

	InputFileName->YearTag = "0";
	InputFileName->MonthTag = "0";
	tmpdata = GetUTF8Data(command);
	windows_system(tmpdata);
	delete[] tmpdata;
	InputFileName->CompletePath.Format(_T("%s\\%s"), tmppath.GetString(), InputFileName->FileName.GetString());
	InfosProcess.Format(_T("Step 4/5:Processing File %s --> UNSORTED FOLDER"), InputFileName->FileName.GetString());
	if (pInformationCtrl->GetSafeHwnd() != NULL)
	pInformationCtrl->SetWindowTextW(InfosProcess.GetString());
}



void CPhotoSortDlg::GetListOfFilesInInputFolder(CString InputPath, std::vector<CString>* FileExtensionList, std::vector<CString>* MovieFileExtensionList)
{
	FileDataClass tmp;

	int Index = 0, IndexSupFolder = 0;
	unsigned int i, Cond = 0;
	CString FileExtension, strtmp, tmpsourcefolder;
	long CptFile = 0, CptFileFound = 0;
	double percent;
	CString mpercent;
	CString tmpSupFolder;
	DataString FileName;

	strtmp = "CREATING FILE LIST OF IMAGE AND MOVIE FILES FROM THE LIST OF FILES AND FOLDERS DETECTED";
	pInformationCtrl->SetWindowTextW(strtmp.GetString());

	
	// Iterate over the `std::filesystem::directory_entry` elements using `auto`
	for (auto const& dir_entry : fs::recursive_directory_iterator(InputPath.GetString(), fs::directory_options::skip_permission_denied))
	{
		if (bContinue == FALSE)
		{
			delete ListOfFiles;
			break;
		}
		tmp.FileName = "";
		tmp.CompletePath = "";
		FileName.SetString(dir_entry.path().c_str());
		tmp.CompletePath = FileName;
		Index = tmp.CompletePath.ReverseFind('\\');
		tmp.FileName = tmp.CompletePath.Mid(Index + 1);
		tmpSupFolder = tmp.CompletePath.Left(Index);
		IndexSupFolder = tmpSupFolder.ReverseFind('\\');
		tmpSupFolder = tmpSupFolder.Mid(IndexSupFolder + 1);
		tmpsourcefolder = tmp.CompletePath.Left(Index);
		tmp.SourceFolder = tmpsourcefolder;
		tmp.SupFolder = tmpSupFolder;
		strtmp.Format(_T("Step 2/5:Search Images and Video Files --> %s\r\n"), tmp.CompletePath.GetString());
		pInformationCtrl->SetWindowTextW(strtmp.GetString());
		if (tmp.FileName.Find('.') > 0)
		{
			Index = tmp.FileName.ReverseFind('.');
			FileExtension = tmp.FileName.Mid(Index + 1);
			FileExtension = ToUppercaseString(FileExtension);
			tmp.FileExtension = FileExtension;
			tmp.FileSize = GetFileSize(tmp.CompletePath);
			Cond = 0;

			if (tmp.FileSize > 100000) //on ne s'interesse qu'aux fichiers dont la taille minimum dépasse 100ko car en dessous de cette valeur les photos sont de toute facon de mauvaise qualité
			{

				for (i = 0; i < FileExtensionList->size(); i++)
				{
					if (FileExtension == FileExtensionList->at(i))
					{
						tmp.FileType = IMAGEFILE;
						ListOfFiles->push_back(tmp);
						TotalSize += tmp.FileSize;
						Cond = 1;
						CptFileFound++;
						break;
					}
				}


				if (Cond == 0)
				{
					for (i = 0; i < MovieFileExtensionList->size(); i++)
					{
						if (FileExtension == MovieFileExtensionList->at(i))
						{
							tmp.FileType = VIDEOFILE;
							ListOfFiles->push_back(tmp);
							CptFileFound++;
							TotalSize += tmp.FileSize;
							break;
						}
					}
				}

				strtmp.Format(_T("%d"), CptFileFound);
				pStaticNumberOfFiles->SetWindowTextW(strtmp.GetString());
				
			}

			CptFile++;
		}


		percent = (CptFile / (double(TotalNbFiles))) * 100;
		mpercent.Format(_T("%0.2f%%"), percent);
		pStaticCtrl->SetWindowTextW(mpercent);
		m_ProgressCtrl.SetPos(CptFile);
		Refresh();
	}

	Refresh();
}


void CPhotoSortDlg::RemoveDuplicateFilesInList()
{
	CString FileName, strtmp;
	long long tmpsize;
	TotalSize = 0;
	double sizetotal;
	CString TotalSizeStr;

	strtmp = "Step 3/5:REMOVE DUPLICATE FILES FROM LIST";
	pInformationCtrl->SetWindowTextW(strtmp.GetString());
	m_ProgressCtrl.SetRange32(0, (int)ListOfFiles->size());
	for (int i = 0; i < (int)ListOfFiles->size(); i++)
	{
		tmpsize = ListOfFiles->at(i).FileSize;
		FileName = ListOfFiles->at(i).FileName;

		for (int j = i+1; j < (int)ListOfFiles->size(); j++)
		{
			if ((ListOfFiles->at(j).FileSize == tmpsize) && (ListOfFiles->at(j).FileName == FileName))
			{
				ListOfFiles->erase(ListOfFiles->begin() + j);
			}
		}

		m_ProgressCtrl.SetPos(i);
	}

	strtmp = "CALCULATE TOTAL SIZE TO PROCESS";
	pInformationCtrl->SetWindowTextW(strtmp.GetString());
	for (int i = 0; i < (int)ListOfFiles->size(); i++)
	{
		TotalSize += ListOfFiles->at(i).FileSize;
		sizetotal = double(TotalSize) / 1e9;
		TotalSizeStr.Format(_T("%0.02f Gb"), sizetotal);
		pStaticSizeCtrl->SetWindowTextW(TotalSizeStr.GetString());
	}
}

void CPhotoSortDlg::ProcessPhotos()
{
	CString strtmp;

	pStaticCtrl = GetDlgItem(IDC_PercentStatic);
	pInformationCtrl = GetDlgItem(IDC_ProcessInformationsStatic);
	pStaticNumberOfFiles = GetDlgItem(IDC_NumerOfFilesStatic);
	pStaticSizeCtrl = GetDlgItem(IDC_TotalSizeStatic);

	if (InputFolderPath.GetLength() == 0)
	{
		MessageBox(_T("Please select a source folder"), _T("Information"), MB_OK + MB_ICONSTOP);
		return;
	}

	if (OutputFolderPath.GetLength() == 0)
	{
		MessageBox(_T("Please select a destination folder"), _T("Information"), MB_OK + MB_ICONSTOP);
		return;
	}

	if (InputFolderPath == OutputFolderPath)
	{
		MessageBox(_T("The source and destination folders shall be differents.Please choose another location for destination folder"), _T("Information"), MB_OK + MB_ICONSTOP);
		return;
	}

	ImageFileExtensionList.push_back(_T("JPEG"));
	ImageFileExtensionList.push_back(_T("JPG"));
	ImageFileExtensionList.push_back(_T("PNG"));
	ImageFileExtensionList.push_back(_T("BMP"));
	ImageFileExtensionList.push_back(_T("TIF"));

	VideoFileExtensionList.push_back(_T("MP4"));
	VideoFileExtensionList.push_back(_T("MOV"));
	VideoFileExtensionList.push_back(_T("AVI"));

	bContinue = TRUE;
	strtmp = "RETRIEVING TOTAL NUMBER OF FILES AND FOLDER OF THE INPUT LOCATION\r\n";
	strtmp += "WARNING:THE PROCESS CAN TAKE SOME TIME IF YOU HAVE SPECIFIED AN ENTIRE DRIVE\r\n";
	pInformationCtrl->SetWindowTextW(strtmp.GetString());
	TotalNbFiles = GetTotalNumberOfFilesToTreat(this, InputFolderPath);

	m_ProgressCtrl.SetRange32(0, (int)TotalNbFiles);

	ListOfFiles->erase(ListOfFiles->begin(), ListOfFiles->end());
	GetListOfFilesInInputFolder(InputFolderPath, &ImageFileExtensionList, &VideoFileExtensionList);
	RemoveDuplicateFilesInList();
	m_ProgressCtrl.SetRange32(0, ListOfFiles->size());
	
	StopThread = 0;
	Refresh();
	std::thread{ThreadProcessInformations,this,&m_ProgressCtrl}.detach();
}

void CPhotoSortDlg::OnClickedBrowsesourcebtn()
{
	InputFolderPath = BrowseFolder();

	CWnd* pControl = GetDlgItem(IDC_InputEditCtrl);
	pControl->SetWindowTextW(InputFolderPath);
}


void CPhotoSortDlg::OnClickedBrowsedestinationbtn()
{
	OutputFolderPath = BrowseFolder();

	CWnd* pControl = GetDlgItem(IDC_OutputEditCtrl);
	pControl->SetWindowTextW(OutputFolderPath);
}


void CPhotoSortDlg::OnClickedStartbtn()
{
	ProcessPhotos();
}


void CPhotoSortDlg::OnBnClickedOk()
{
	// TODO: ajoutez ici le code de votre gestionnaire de notification de contrôle
	bContinue = FALSE;
	CDialogEx::OnOK();
}


HBRUSH CPhotoSortDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	if ((nCtlColor == CTLCOLOR_STATIC) && (pWnd->GetDlgCtrlID() == IDC_ProcessInformationsStatic))
	{
		pDC->SetBkColor(RGB(255, 255, 255));
		return (HBRUSH)CreateSolidBrush(RGB(255, 255, 255));
	}

	if ((nCtlColor == CTLCOLOR_STATIC) && (pWnd->GetDlgCtrlID() == IDC_NumerOfFilesStatic))
	{
		pDC->SetBkColor(RGB(255, 255, 255));
		return (HBRUSH)CreateSolidBrush(RGB(255, 255, 255));
	}

	return hbr;
}


void CPhotoSortDlg::OnAboutMenu()
{
	CAboutDlg Dlg;
	Dlg.DoModal();

}

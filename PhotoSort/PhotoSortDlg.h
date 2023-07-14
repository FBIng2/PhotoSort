
// PhotoSortDlg.h : fichier d'en-tête
//

#pragma once


struct FileDataClass
{
	CString FileName{};
	CString CompletePath{};
	CString Path{};
	CString SourceFolder{};
	CString SupFolder{};
	CString YearTag{};
	CString MonthTag{};
	CString MonthTagForDate{};
	CString DayTag{};
	CString FileExtension{};
	bool FileType{};
	long long FileSize{};
};


struct FileResultClass
{
	CString FileData{};
	CString FileName{};
	CString FileDataSlideShow{};
	CString FileNameSlideShow{};
	CString Year{};
	CString Month{};
	CString NbFilesImages{};
	CString NbFilesVideo{};
};



class DataString :public CString
{
public:
	DataString& DataString::operator=(const DataString& c) //DEBUG OK
	{
		if (this != &c)
		{
			this->SetString(c);
		}
		return *this;
	}

	DataString& DataString::operator=(const CString& c) //DEBUG OK
	{
		if (this != &c)
		{
			this->SetString(c);
		}
		return *this;
	}


	std::vector <CString> Split(char Separator)
	{
		std::vector <CString> OutputArray;
		CString tmp, tmp1;

		if (this->Find(Separator) < 0)
			return OutputArray;

		tmp = *this;
		const wchar_t* str = tmp;

		do
		{
			tmp1.Empty();
			while (*str != Separator && *str)
			{
				tmp1.AppendChar(*str);
				str++;
			}

			OutputArray.push_back(tmp1);
		} while (0 != *str++);

		return OutputArray;
	};

};

// boîte de dialogue de CPhotoSortDlg
class CPhotoSortDlg : public CDialogEx
{
// Construction
public:
	CPhotoSortDlg(CWnd* pParent = nullptr);	// constructeur standard
	CString InputFolderPath, OutputFolderPath;
	std::vector<FileDataClass>* ListOfFiles;
	std::vector<CString> ImageFileExtensionList;
	std::vector<CString> VideoFileExtensionList;
	CProgressCtrl m_ProgressCtrl;
	CWnd* pStaticCtrl;
	CWnd* pStaticNumberOfFiles;
	CWnd* pStaticSizeCtrl;
	CString MainAppWindowTittle;
	CWnd* pInformationCtrl;
	long TotalNbFiles;
	long long TotalSize;

	void OnAboutMenu();
	void CallCopyDialog();


	//FUNCTIONS
	void ManageJPEGPhotos(FileDataClass* InputFileName);
	void ManageMP4File(FileDataClass* InputFileName);
	void ManagePNGFile(FileDataClass* InputFileName);
	void ManageMOVFile(FileDataClass* InputFileName);
	void ManageAVIFile(FileDataClass* InputFileName);
	void CopyToUnsortedFolder(FileDataClass* InputFileName);

	std::vector<CString> GetJPGDateTime(FileDataClass* FileName);
	std::vector<CString> GetMOVDateTime(FileDataClass* FileName);
	std::vector<CString> GetPNGDateTime(FileDataClass* FileName);
	std::vector<CString>  GetMP4DateTime(FileDataClass* FileName);
	std::vector<CString>  GetAVIDateTime(FileDataClass* FileName);

	void ProcessPhotos();
	void GetListOfFilesInInputFolder(CString InputPath, std::vector<CString>* FileExtensionList, std::vector<CString>* MovieFileExtensionList);
	void RemoveDuplicateFilesInList();
// Données de boîte de dialogue
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PHOTOSORT_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// Prise en charge de DDX/DDV


// Implémentation
protected:
	HICON m_hIcon;

	// Fonctions générées de la table des messages
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnClickedBrowsesourcebtn();
	afx_msg void OnClickedBrowsedestinationbtn();
	afx_msg void OnClickedStartbtn();
	afx_msg void OnBnClickedOk();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};


static DWORD WINAPI ThreadProcessInformations(CPhotoSortDlg* MainDlg, CProgressCtrl* m_ProgressCtrl);
//static DWORD WINAPI ThreadCopyFiles(FileDataClass* InputFileName, CString* OutputFolderPath, std::vector<CString> *DateFields);
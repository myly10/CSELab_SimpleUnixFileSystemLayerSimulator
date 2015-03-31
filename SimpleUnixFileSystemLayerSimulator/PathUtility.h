#include <Windows.h>
#define MAX_FILENAME_LENGTH 50
#define MAX_PATH_FILE_LENGTH (MAX_PATH + MAX_FILENAME_LENGTH)
#pragma once
class PathUtility
{
public:
	PathUtility(void);
	~PathUtility(void);
	static int getCurrentDirectory(){
	}
	
	//调用者传入一个字符串指针，由该函数申请空间，并填入所需内容
	//调用者需要释放这个空间，否则会造成泄露
	static int ConstructFullFilePath(TCHAR **szPath, const TCHAR *filename){
		
		//确保文件名长度不超过限制
		if(_tcslen(filename)>MAX_FILENAME_LENGTH)
			return  -1;

		//为路径获取存储空间
		*szPath = (TCHAR*)malloc(MAX_PATH_FILE_LENGTH * sizeof(TCHAR));

		if(!GetModuleFileName(NULL,*szPath,MAX_PATH_FILE_LENGTH))
		{
			return -1;
		}
		(_tcsrchr(*szPath, _T('\\')))[1] = 0;
		_tcscat(*szPath,filename);
		return 0;
	}
	static int ConstructFullFilePathA(char **szPath, const char *filename){
		if(strlen(filename)>MAX_FILENAME_LENGTH)
			return -1;
		
		*szPath = (char*)malloc(MAX_PATH_FILE_LENGTH * sizeof(char));

		if(!GetModuleFileNameA(NULL,*szPath,MAX_PATH_FILE_LENGTH))
		{
			return -1;
		}
		(strrchr(*szPath, _T('\\')))[1] = 0;
		strcat(*szPath,filename);
		return 0;
	}
};


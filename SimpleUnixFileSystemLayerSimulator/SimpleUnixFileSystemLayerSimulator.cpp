#include "stdafx.h"
#include "PathUtility.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <string>
#include <sstream>

#include <time.h>
#include <stdio.h>
#include <vector>
using namespace std;

#define FAILURE -1
#define DISKFILENAME TEXT("diskblocks.data")

/////////////////////////
//block信息
////////////////////////
//每个block的大小
#define BLOCK_SIZE 1024
//block的数量
#define BLOCK_NUM 16384

/////////////////////////
//inode信息信息
////////////////////////

//每个inode所能映射的最大BLOCK数量
#define N 14
//inode table中的inode数量
#define INODE_NUM 128

//inode的类型
enum ITYPE{
	FS_UNUSEDINODE=0,	//空inode
	FS_FILE=1,		//文件inode
	FS_DIRECTORY=2	//目录inode
};

//inode结构体
typedef struct inode_
{
	int type;				//inode类型,取值为ITYPE中的枚举值
	int size;				//inode所映射的block的总大小,单位是byte
	int block_numbers[N];	//inode对应的block number的列表
} inode_t;

//目录类型的inode对应的block结构
//每个block中有8个entry,每个entry大小为128个byte
//其中每个entry的第0~126个byte为目录/文件名称字符串,第127个byte为该entry对应的inodenumber
#define BLOCK_DIRECTORY_ENTRY_SIZE 128
#define BLOCK_DIRECTORY_ENTRY_NUM 8
#define BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET 127

//inode table
inode_t inode_table[INODE_NUM];

/////////////////////////
//	磁盘block分布信息
//  说明:以下所有以OFFSET为后缀的值,均表示对应区域起始block的block_number
//           所有以SIZE为后缀的值,均表示对应区域所占用的block数量
////////////////////////

//boot block的启起始位置
#define BOOT_BLOCK_OFFSET 0
//super block的起始位置
#define SUPER_BLOCK_OFFSET 1

//bit map for free inode区域的起始位置和大小
#define BITMAP_FOR_FREE_BLOCK_OFFSET 2
#define BITMAP_FOR_FREE_BLOCK_SIZE ((BLOCK_NUM/8)/BLOCK_SIZE)

//inode table区域的起始位置和大小
#define INODE_TABLE_OFFSET (BITMAP_FOR_FREE_BLOCK_OFFSET + BITMAP_FOR_FREE_BLOCK_SIZE)
//inodetable实际所需空间byte为单位，
#define INODE_TABLE_SIZE_REAL (INODE_NUM *sizeof(inode_t))
//inodetable所需的block数量
#define INODE_TABLE_SIZE ((INODE_TABLE_SIZE_REAL / BLOCK_SIZE)+1)

//文件block区域的起始位置和大小
#define FILE_BLOCK_OFFSET (INODE_TABLE_OFFSET + INODE_TABLE_SIZE)
#define FILE_BLOCK_SIZE (BLOCK_NUM - FILE_BLOCK_OFFSET)

//bitmap for free block
byte freeblockbitmap[BITMAP_FOR_FREE_BLOCK_SIZE*BLOCK_SIZE];

//文件最大值——无效值
//#define MAX_FILE_SIZE (BLOCK_SIZE * N - 1)
//#define MAX_NAME_LENGT 0

//你可以通过如下两个函数一次性载入和写回inodetable与bitmap

//从diskblocks.data文件中的offset位置开始，读入大小为size的数据，写入dst所指向的内存空间
//调用前请申请好大小为size的空间
byte *loaddiskcontent(int offset, int size){
	byte* dst=new byte[size];
	memset(dst, 0, size);
	TCHAR *szPath;
	std::ifstream dbfile;

	PathUtility::ConstructFullFilePath(&szPath, DISKFILENAME);
	dbfile.open(szPath, ios::in|ios::binary);
	dbfile.seekg(offset, ios::beg);
	dbfile.read((char*)dst, size);
	dbfile.close();

	free(szPath);
	return dst;
}
//从diskblocks.data文件中的offset位置开始，写入大小为size由src所指向的数据
//该操作并不影响文件的其他部分
int savediskcontent(byte* src, int offset, int size){
	TCHAR *szPath;
	std::ofstream dbfile;

	PathUtility::ConstructFullFilePath(&szPath, DISKFILENAME);

	//修改二进制文件的一部分请使用ios::out|ios::binary|ios::in
	dbfile.open(szPath, ios::out|ios::binary|ios::in);
	if (!dbfile.is_open()){
		cout<<"file open failed.\n";
		return FAILURE;
	}
	dbfile.seekp(
		//0,
		offset,
		ios::beg);
	dbfile.write((char*)src, size);
	dbfile.flush();
	dbfile.close();

	free(szPath);
	return 0;
}

//构建空的diskblocks.data
int createEmptyBlockFile(int blocknum);

//load bitmap and inode table
void load_disk_structure(){
	//load bitmap
	byte *bitmap=loaddiskcontent(BITMAP_FOR_FREE_BLOCK_OFFSET*BLOCK_SIZE, BITMAP_FOR_FREE_BLOCK_SIZE*BLOCK_SIZE);
	memcpy(freeblockbitmap, bitmap, BITMAP_FOR_FREE_BLOCK_SIZE*BLOCK_SIZE);
	delete[] bitmap;

	//load inode table
	byte *inodeTableRaw=loaddiskcontent(INODE_TABLE_OFFSET*BLOCK_SIZE, INODE_TABLE_SIZE_REAL);
	memcpy(inode_table, inodeTableRaw, INODE_TABLE_SIZE_REAL);
	delete[] inodeTableRaw;
}

//block layer
byte *BLOCK_NUMBER_TO_BLOCK(int b);
int WRITE_BLOCK_BY_BLOCK_NUMBER(int b, byte* block);

//file layer
int INDEX_TO_BLOCK_NUMBER(inode_t &i, int index);
byte * INODE_TO_BLOCK(int offset, inode_t i, byte* block);

//inode number
inode_t &INODE_NUMBER_TO_INODE(int inode_number, inode_t* inode);
byte * INODE_NUMBER_TO_BLOCK(int offset, int inode_number);

//file name
int NAME_TO_INODE_NUMBER(const char* filename, int dir_inode_num);
int LOOKUP(const char* target_name, int dir_inode_num);

//path name layer
int PATH_TO_INODE_NUMBER(const char* path, int dir);

//打印diskblock结构
int printDiskLayout(){
	printf("block size:%d, block num:%d\n", BLOCK_SIZE, BLOCK_NUM);
	printf("inode num:%d, inode size:%d\n", INODE_NUM, sizeof(inode_t));
	printf("bit map block: %d - %d\tbyte(0x%x - 0x%x)\n", BITMAP_FOR_FREE_BLOCK_OFFSET, BITMAP_FOR_FREE_BLOCK_OFFSET+BITMAP_FOR_FREE_BLOCK_SIZE-1
		, BITMAP_FOR_FREE_BLOCK_OFFSET * BLOCK_SIZE, (BITMAP_FOR_FREE_BLOCK_OFFSET+BITMAP_FOR_FREE_BLOCK_SIZE-1)*BLOCK_SIZE);
	printf("  inode block: %d - %d\tbyte(0x%x - 0x%x)\n", INODE_TABLE_OFFSET, INODE_TABLE_OFFSET+INODE_TABLE_SIZE-1
		, INODE_TABLE_OFFSET * BLOCK_SIZE, (INODE_TABLE_OFFSET+INODE_TABLE_SIZE-1) * BLOCK_SIZE);
	printf("   file block: %d - %d\tbyte(0x%x - 0x%x)\n\n\n", FILE_BLOCK_OFFSET, FILE_BLOCK_OFFSET+FILE_BLOCK_SIZE-1
		, FILE_BLOCK_OFFSET * BLOCK_SIZE, (FILE_BLOCK_OFFSET+FILE_BLOCK_SIZE-1) * BLOCK_SIZE);
	return 0;
}

int LINK(){
	return 0;
}

//path name layer
int PATH_TO_INODE_NUMBER(const char* path, int dir){
	if (strcmp(path, "/")==0) return 0; //if only root dir, return root inode number
	else{
		string s(path);
		int currentPos=0, nextPos;
		if (s.find('/')==0) { //split dir names with '/'
			dir=0;
			currentPos=1;
		}
		while (currentPos<s.size()){ //level by level go into subdir
			nextPos=s.find('/', currentPos+1);
			if (nextPos==s.npos) nextPos=s.size();
			dir=LOOKUP(s.substr(currentPos, nextPos-currentPos).c_str(), dir);
			currentPos=nextPos+1;
			if (dir==FAILURE) break;
		}
		return dir;
	}
}

//file name layer
int LOOKUP(const char* target_name, int dir_inode_num){
	inode_t &i=INODE_NUMBER_TO_INODE(dir_inode_num, inode_table);
	if (i.type!=FS_DIRECTORY) return FAILURE;
	byte* b;
	for (int offset=0; offset<i.size; offset+=BLOCK_SIZE){
		b=INODE_NUMBER_TO_BLOCK(offset, dir_inode_num); //read dir block from offset
		for (int be=0; be!=BLOCK_DIRECTORY_ENTRY_NUM; ++be) // compare each filename with target_name
			if (strncmp(target_name, ((char*)b)+be*BLOCK_DIRECTORY_ENTRY_SIZE, BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET)==0){
				int blockNum=b[BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET+be*BLOCK_DIRECTORY_ENTRY_SIZE]; //if found, return block number
				delete[] b;
				return blockNum;
			}
		delete[] b;
	}
	return FAILURE;
}

int NAME_TO_INODE_NUMBER(const char* filename, int dir_inode_num){
	return LOOKUP(filename, dir_inode_num);
}

//inode number layer
inode_t &INODE_NUMBER_TO_INODE(int inode_number, inode_t* inode){
	if (inode_number<0) return *(inode_t*)nullptr;
	return inode[inode_number];
}

byte * INODE_NUMBER_TO_BLOCK(int offset, int inode_number){
	if (inode_number<0) return nullptr;
	inode_t &i=INODE_NUMBER_TO_INODE(inode_number, inode_table);
	auto o=offset/BLOCK_SIZE;
	auto b=INDEX_TO_BLOCK_NUMBER(i, o);
	return BLOCK_NUMBER_TO_BLOCK(b);
}
//file
int INDEX_TO_BLOCK_NUMBER(inode_t &i, int index){
	if (index<0) return FAILURE;
	return i.block_numbers[index];
}

byte * INODE_TO_BLOCK(int offset, inode_t &i){
	return BLOCK_NUMBER_TO_BLOCK(INDEX_TO_BLOCK_NUMBER(i, offset/BLOCK_SIZE));
}

//block
//读取磁盘中block number为b的block中的数据 and return it，大小为一个block_size
//调用前请使用new或malloc申请足够的空间用于存放结果
byte *BLOCK_NUMBER_TO_BLOCK(int b){
	byte* block=new byte[BLOCK_SIZE];
	if (b<0||b>=BLOCK_NUM){
		delete[] block;
		return nullptr;
	}

	TCHAR *szPath;
	std::ifstream dbfile;

	memset(block, 0, BLOCK_SIZE*sizeof(byte));
	PathUtility::ConstructFullFilePath(&szPath, DISKFILENAME);
	dbfile.open(szPath, ios::in|ios::binary);
	dbfile.seekg(b * BLOCK_SIZE, ios::beg);
	dbfile.read((char*)block, BLOCK_SIZE);
	dbfile.close();

	free(szPath);
	return block;
}

//将block所指的数据写入到磁盘中block number为b的block中，大小为一个block_size
int WRITE_BLOCK_BY_BLOCK_NUMBER(int b, byte* srcBlock){
	if (b<0||b>=BLOCK_NUM)
		return FAILURE;

	TCHAR *szPath;
	std::ofstream dbfile;

	PathUtility::ConstructFullFilePath(&szPath, DISKFILENAME);
	dbfile.open(szPath, ios::out|ios::binary|ios::in);
	dbfile.seekp(b * BLOCK_SIZE, ios::beg);
	dbfile.write((char*)srcBlock, BLOCK_SIZE);
	dbfile.flush();
	dbfile.close();

	free(szPath);
	return 0;
}

class Commands{
public:
	//user command parser
	static int execute(vector<string> &cmd, int cwdInodeNum){
		if (cmd[0]=="ls") return ls_(cmd, cwdInodeNum);
		if (cmd[0]=="info") return printDiskLayout();
		if (cmd[0]=="cat") return cat_(cmd, cwdInodeNum);
		if (cmd[0]=="export") return export_(cmd, cwdInodeNum);
		if (cmd[0]=="tree") return tree_(cmd, cwdInodeNum);
		if (cmd[0]=="find") return find_(cmd);
		if (cmd[0]=="createFile" || cmd[0]=="touch") return createEmptyFile(cmd, cwdInodeNum);
		if (cmd[0]=="insertFile"||cmd[0]=="import") return insertFile(cmd, cwdInodeNum);
		if (cmd[0]=="createDirecotory" || cmd[0]=="mkdir") return createDirecotory(cmd,cwdInodeNum);
		if (cmd[0]=="fsck") return fsck_();
		else{
			cerr<<"Error: command \""<<cmd[0]<<"\" was not found."<<endl;
			return FAILURE;
		}
	}

	//find an available block and mark it as used in bitmap
	static int findAndMarkNextAvailableBlock(int blockNumCurrent){
		if (blockNumCurrent<FILE_BLOCK_OFFSET && blockNumCurrent>=FILE_BLOCK_OFFSET+FILE_BLOCK_SIZE)
			return FAILURE;
		while (blockNumCurrent!=FILE_BLOCK_OFFSET+FILE_BLOCK_SIZE){
			if (((freeblockbitmap[blockNumCurrent/8]>>(blockNumCurrent%8))&1)==0){ //determine whether blockNumCurrent is used
				freeblockbitmap[blockNumCurrent/8]|=1<<(blockNumCurrent%8);
				return blockNumCurrent;
			}
			++blockNumCurrent;
		}
		return FAILURE;
	}
	
	//unmark mismarked block in bitmap
	static int unMarkBlockBitmap(int blockNumCurrent){
		if (blockNumCurrent<FILE_BLOCK_OFFSET && blockNumCurrent>=FILE_BLOCK_OFFSET+FILE_BLOCK_SIZE)
			return FAILURE;
		if ((freeblockbitmap[blockNumCurrent/8]>>(blockNumCurrent%8))&1==0) return FAILURE;
		freeblockbitmap[blockNumCurrent/8]&=~(1<<(blockNumCurrent%8));
		return 0;
	}

	//command ls
	static int ls_(vector<string> &cmd, int cwdInodeNum){
	//error checking start
		inode_t &wdInode=cmd.size()>1?
			INODE_NUMBER_TO_INODE(PATH_TO_INODE_NUMBER(cmd[1].c_str(), cwdInodeNum), inode_table)
			:INODE_NUMBER_TO_INODE(cwdInodeNum, inode_table);
		if (&wdInode==nullptr){
			cerr<<"Error: path/file not found"<<endl;
			return FAILURE;
		}
		if (wdInode.type!=FS_DIRECTORY){
			cerr<<"Error: not a directory"<<endl;
			return FAILURE;
		}
	//error checking finished

		for (int i=0; i!=N; ++i){
			if (wdInode.block_numbers[i]<=0) continue;
			byte *tb=INODE_TO_BLOCK(i*BLOCK_SIZE, wdInode); //read directory content
			for (int j=0; j!=BLOCK_DIRECTORY_ENTRY_NUM; ++j){ //enter each subentry
				int inodeNumCurrent=tb[j*BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET];
				//if file name == '\0', regard as no file here
				if (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_UNUSEDINODE||tb[j*BLOCK_DIRECTORY_ENTRY_SIZE]=='\0') continue;
				switch (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type){ //display file name and info
				case FS_DIRECTORY: cout<<"d-- "; break;
				case FS_FILE: cout<<"-f- "; break;
				case FS_UNUSEDINODE: cout<<"--u "; break;
				}
				cout<<inodeNumCurrent<<"\t "<<inode_table[inodeNumCurrent].size<<"\t";
				for (int p=0; p!=BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET; ++p){
					if (*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p)=='\0') break;
					cout<<*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p);
				}
				cout<<endl;
			}
			delete[] tb;
		}
		return 0;
	}

	//command cat
	static int cat_(vector<string> & cmd, int cwdInodeNum) {
		//error checking start
		if (cmd.size()<2){
			cerr<<"Error: no file specified."<<endl;
			return FAILURE;
		}
		inode_t &inode=INODE_NUMBER_TO_INODE(PATH_TO_INODE_NUMBER(cmd[1].c_str(), cwdInodeNum),inode_table);
		if (&inode==nullptr){
			cerr<<"Error: path/file not found"<<endl;
			return FAILURE;
		}
		if (inode.type==FS_DIRECTORY){
			cerr<<"Error: is a directory."<<endl;
			return FAILURE;
		}
		//error checking finished
		int offset=0;
		char *data=new char[inode.size];
		while (offset<inode.size){
			byte *tb=INODE_TO_BLOCK(offset, inode); //read blocks
			memcpy(data+offset, tb, min(BLOCK_SIZE, inode.size-offset));
			offset+=BLOCK_SIZE;
			delete[] tb;
		}
		cout<<string(data,inode.size)<<endl; //and print them out
		delete[] data;
		return 0;
	}

	//command export
	static int export_(vector<string> & cmd, int cwdInodeNum){
		if (cmd.size()<2){
			cerr<<"Error: no file specified."<<endl;
			return FAILURE;
		}
		inode_t &inode=INODE_NUMBER_TO_INODE(PATH_TO_INODE_NUMBER(cmd[1].c_str(), cwdInodeNum), inode_table);
		if (&inode==nullptr){
			cerr<<"Error: path/file not found"<<endl;
			return FAILURE;
		}
		if (inode.type==FS_DIRECTORY){
			cerr<<"Error: is a directory."<<endl;
			return FAILURE;
		}
		
		//generally same process as cat, then write to file rather than cout

		int offset=0;
		char *data=new char[inode.size];
		while (offset<inode.size){
			byte *tb=INODE_TO_BLOCK(offset, inode);
			memcpy(data+offset, tb, min(BLOCK_SIZE, inode.size-offset));
			offset+=BLOCK_SIZE;
			delete[] tb;
		}
		int fileNamePos=cmd[1].find_last_of('/');
		if (fileNamePos==cmd[1].npos) fileNamePos=0;
		else ++fileNamePos;

		TCHAR *szPath;
		std::ofstream outFile;

		PathUtility::ConstructFullFilePath(&szPath, cmd[1].substr(fileNamePos).c_str());
		outFile.open(szPath,ios::binary);
		outFile.write(data, inode.size);
		outFile.flush();
		outFile.close();

		delete[] data;
		return 0;
	}

	//command tree
	static int tree_(vector<string> & cmd, int cwdInodeNum){
		int indentFactor=1;
		int wdInodeNum=cmd.size()>1?PATH_TO_INODE_NUMBER(cmd[1].c_str(), cwdInodeNum):cwdInodeNum;
		inode_t &wdInode=cmd.size()>1?
			INODE_NUMBER_TO_INODE(wdInodeNum, inode_table)
			:INODE_NUMBER_TO_INODE(cwdInodeNum, inode_table);
		if (&wdInode==nullptr){
			cerr<<"Error: path/file not found"<<endl;
			return FAILURE;
		}
		if (wdInode.type!=FS_DIRECTORY){
			cerr<<"Error: not a directory"<<endl;
			return FAILURE;
		}

		//start generating files in current dir
		string result;
		if (cmd.size()>1){
			int fileNamePos=cmd[1].find_last_of('/');
			if (fileNamePos==cmd[1].npos) fileNamePos=0;
			else ++fileNamePos;
			if (fileNamePos>=cmd[1].size()) result+="+ /";
			else result+="+ "+cmd[1].substr(fileNamePos);
		}
		else result+="+ /";
		ostringstream oss;
		oss<<wdInodeNum;
		result+=string(" (")+oss.str()+")\n";
		oss.str("");
		oss.clear();

		//recursively ls
		for (int i=0; i!=N; ++i){
			if (wdInode.block_numbers[i]<=0) continue;
			byte *tb=INODE_TO_BLOCK(i*BLOCK_SIZE, wdInode);
			for (int j=0; j!=BLOCK_DIRECTORY_ENTRY_NUM; ++j){
				string s="";
				int fileSize=-1;
				int inodeNumCurrent=tb[j*BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET];
				if (INODE_NUMBER_TO_INODE(inodeNumCurrent,inode_table).type==FS_UNUSEDINODE || tb[j*BLOCK_DIRECTORY_ENTRY_SIZE]=='\0') continue;
				result+=string(indentFactor*2, ' ');

				switch (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type){
				case FS_DIRECTORY: result+="+ "; break;
				case FS_FILE: result+="- "; fileSize=INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).size;  break;
				case FS_UNUSEDINODE: result+="x "; break;
				}

				for (int p=0; p!=BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET; ++p){
					if (*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p)=='\0') break;
					s+=*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p);
				}
				result+=s;

				//output file info
				oss<<inodeNumCurrent;
				result+=string(" (")+oss.str()+")";
				oss.str("");
				oss.clear();
				if (fileSize!=-1){
					oss<<fileSize;
					result+=string(" size: ")+oss.str()+"\n";
					oss.str("");
					oss.clear();
				}
				else result+="\n";
				if (s!="." && s!=".." && INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_DIRECTORY)
					if (tree_Recursive(inodeNumCurrent, indentFactor+1, result)==FAILURE){
						delete[] tb;
						return FAILURE;
					}
			}
			delete[] tb;
		}
		cout<<result<<endl;
		return 0;
	}

	//recursive processing part of tree command
	static int tree_Recursive(int cwdInodeNum, int indentFactor, string &result){
		inode_t &wdInode=INODE_NUMBER_TO_INODE(cwdInodeNum, inode_table);
		if (&wdInode==nullptr){
			cerr<<"Error: path/file not found"<<endl;
			return FAILURE;
		}
		if (wdInode.type!=FS_DIRECTORY){
			cerr<<"Error: not a directory"<<endl;
			return FAILURE;
		}

		//start generating files in current dir
		for (int i=0; i!=N; ++i){
			if (wdInode.block_numbers[i]<=0) continue;
			byte *tb=INODE_TO_BLOCK(i*BLOCK_SIZE, wdInode);
			for (int j=0; j!=BLOCK_DIRECTORY_ENTRY_NUM; ++j){
				string s="";
				int fileSize=-1;
				int inodeNumCurrent=tb[j*BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET];
				if (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_UNUSEDINODE||tb[j*BLOCK_DIRECTORY_ENTRY_SIZE]=='\0') continue;
				result+=string(indentFactor*2, ' ');

				switch (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type){
				case FS_DIRECTORY: result+="+ "; break;
				case FS_FILE: result+="- "; fileSize=INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).size;  break;
				case FS_UNUSEDINODE: result+="x "; break;
				}

				for (int p=0; p!=BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET; ++p){
					if (*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p)=='\0') break;
					s+=*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p);
				}
				result+=s;
				
				//output file info
				ostringstream oss;
				oss<<inodeNumCurrent;
				result+=string(" (")+oss.str()+")";
				oss.clear();
				if (fileSize!=-1){
					oss<<fileSize;
					result+=string(" size: ")+oss.str()+"\n";
					oss.clear();
				}
				else result+="\n";
				if (s!="." && s!=".." && INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_DIRECTORY)
					if (tree_Recursive(inodeNumCurrent, indentFactor+1, result)==FAILURE){
						delete[] tb;
						return FAILURE;
					}
			}
			delete[] tb;
		}
		return 0;
	}

	//command find
	static int find_(vector<string> & cmd){
		if (cmd.size()<3){
			cerr<<"Error: missing parameter."<<endl;
			return FAILURE;
		}
		if (cmd[1][0]!='/'){
			cerr<<"Error: relative path is not accpeted."<<endl;
			return FAILURE;
		}
		int cwdInodeNum=0;
		inode_t &wdInode=cmd.size()>1?
			INODE_NUMBER_TO_INODE(PATH_TO_INODE_NUMBER(cmd[1].c_str(), cwdInodeNum), inode_table)
			:INODE_NUMBER_TO_INODE(cwdInodeNum, inode_table);
		if (&wdInode==nullptr){
			cerr<<"Error: path/file not found"<<endl;
			return FAILURE;
		}
		if (wdInode.type!=FS_DIRECTORY){
			cerr<<"Error: not a directory"<<endl;
			return FAILURE;
		}

		//start generating files in current dir
		bool isFound=false;
		string path=cmd[1];
		if (path=="/") path="";
		
		//recursively ls and look for target
		for (int i=0; i!=N; ++i){
			if (wdInode.block_numbers[i]<=0) continue;
			byte *tb=INODE_TO_BLOCK(i*BLOCK_SIZE, wdInode);
			for (int j=0; j!=BLOCK_DIRECTORY_ENTRY_NUM; ++j){
				string s="";
				int inodeNumCurrent=tb[j*BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET];
				if (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_UNUSEDINODE||tb[j*BLOCK_DIRECTORY_ENTRY_SIZE]=='\0') continue;

				for (int p=0; p!=BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET; ++p){
					if (*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p)=='\0') break;
					s+=*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p);
				}

				if (s==cmd[2]){
					isFound=true;
					cout<<path+"/"+s<<endl;
				}
				if (s!="." && s!=".." && INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_DIRECTORY)
					switch (find_Recursive(cmd[2], inodeNumCurrent, path+"/"+s)){
					case FAILURE: delete[] tb; return FAILURE;
					case 0: isFound=true;
				}
			}
			delete[] tb;
		}
		if (!isFound){
			cout<<"Error: nothing found."<<endl;
			return 1;
		}
		else return 0;
	}

	//recursive processing part of find command
	static int find_Recursive(const string &target, int cwdInodeNum, const string &path){
		//start generating files in current dir
		bool isFound=false;
		inode_t &wdInode=INODE_NUMBER_TO_INODE(cwdInodeNum,inode_table);
		for (int i=0; i!=N; ++i){
			if (wdInode.block_numbers[i]<=0) continue;
			byte *tb=INODE_TO_BLOCK(i*BLOCK_SIZE, wdInode);
			for (int j=0; j!=BLOCK_DIRECTORY_ENTRY_NUM; ++j){
				string s="";
				int inodeNumCurrent=tb[j*BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET];
				if (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_UNUSEDINODE||tb[j*BLOCK_DIRECTORY_ENTRY_SIZE]=='\0') continue;

				for (int p=0; p!=BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET; ++p){
					if (*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p)=='\0') break;
					s+=*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p);
				}

				if (s==target){
					isFound=true;
					cout<<path+"/"+s<<endl;
				}
				if (s!="." && s!=".." && INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_DIRECTORY)
					switch (find_Recursive(target, inodeNumCurrent, path+"/"+s)){
					case FAILURE: delete[] tb; return FAILURE;
					case 0: isFound=true;
				}
			}
			delete[] tb;
		}
		return !isFound;
	}

	//command createEmptyFile or touch
	static int createEmptyFile(vector<string> & cmd, int cwdInodeNum){
		if (cmd.size()<3){
			cerr<<"Error: missing parameter"<<endl;
			return FAILURE;
		}
		inode_t &wdInode=INODE_NUMBER_TO_INODE(PATH_TO_INODE_NUMBER(cmd[1].c_str(), cwdInodeNum), inode_table);
		if (&wdInode==nullptr){
			cerr<<"Error: path/file not found"<<endl;
			return FAILURE;
		}
		if (wdInode.type!=FS_DIRECTORY){
			cerr<<"Error: \""<<cmd[1]<<"\" is not a directory."<<endl;
			return FAILURE;
		}
		if (cmd[2].size()>BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET){
			cerr<<"Error: file name too long (>127 bytes)."<<endl;
			return FAILURE;
		}

		//find name conflicts
		for (int i=0; i!=N; ++i){
			if (wdInode.block_numbers[i]<=0) continue;
			byte *tb=INODE_TO_BLOCK(i*BLOCK_SIZE, wdInode);
			for (int j=0; j!=BLOCK_DIRECTORY_ENTRY_NUM; ++j){
				int inodeNumCurrent=tb[j*BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET];
				if (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_UNUSEDINODE||tb[j*BLOCK_DIRECTORY_ENTRY_SIZE]=='\0') continue;
				string s="";
				for (int p=0; p!=BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET; ++p){
					if (*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p)=='\0') break;
					s+=*((char*)tb+j*BLOCK_DIRECTORY_ENTRY_SIZE+p);
				}
				if (s==cmd[2]){
					cerr<<"Error: file with the same name already exists."<<endl;
					return FAILURE;
				}
			}
			delete[] tb;
		}

		//search for available inode and block
		bool inodeAvailable=false;
		for (int i=0; i!=N; ++i){
			if (wdInode.block_numbers[i]==0){ //this procedure can make sure the process later will have available block
				int blockNum=findAndMarkNextAvailableBlock(FILE_BLOCK_OFFSET);
				if (blockNum==FAILURE){
					cerr<<"Error: no block available"<<endl;
					return FAILURE;
				}
				wdInode.block_numbers[i]=blockNum;
				wdInode.size+=BLOCK_SIZE;
				savediskcontent(freeblockbitmap, BITMAP_FOR_FREE_BLOCK_OFFSET*BLOCK_SIZE, BITMAP_FOR_FREE_BLOCK_SIZE*BLOCK_SIZE);
			}
			byte *tb=INODE_TO_BLOCK(i*BLOCK_SIZE, wdInode);
			for (int j=0; j!=BLOCK_DIRECTORY_ENTRY_NUM; ++j){
				int inodeNumCurrent=tb[j*BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET];
				if (tb[j*BLOCK_DIRECTORY_ENTRY_SIZE]=='\0'){
					for (inodeNumCurrent=0; inodeNumCurrent!=INODE_NUM; ++inodeNumCurrent){
						if (INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table).type==FS_UNUSEDINODE){
							inodeAvailable=true;
							memcpy(tb+j*BLOCK_DIRECTORY_ENTRY_SIZE, cmd[2].c_str(), sizeof(char)*cmd[2].size()+1);
							tb[j*BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET]=(unsigned char)inodeNumCurrent;
							inode_t &inodeCurrent=INODE_NUMBER_TO_INODE(inodeNumCurrent, inode_table);
							inodeCurrent.type=FS_FILE;
							inodeCurrent.size=0;
							memset(inodeCurrent.block_numbers, 0, sizeof(int)*N);

							//write file data
							savediskcontent((byte*)inode_table, INODE_TABLE_OFFSET*BLOCK_SIZE, INODE_TABLE_SIZE_REAL);
							savediskcontent(tb, wdInode.block_numbers[i]*BLOCK_SIZE, BLOCK_SIZE);
							delete[] tb;
							return 0;
						}
					}
				}
			}
			delete[] tb;;
		}
		if (!inodeAvailable){
			cerr<<"Error: cannot write inode"<<endl;
			return FAILURE;
		}
		return 0;
	}

	//command insertFile or import
	static int insertFile(vector<string> & cmd, int cwdInodeNum){
		if (cmd.size()<3){
			cerr<<"Error: missing parameter"<<endl;
			return FAILURE;
		}

		TCHAR *szPath;
		std::ifstream srcFile;
		PathUtility::ConstructFullFilePath(&szPath, cmd[2].c_str());
		srcFile.open(szPath, ios::binary);
		if (!srcFile.is_open()){
			cerr<<"Error: cannot open file"<<endl;
			return FAILURE;
		}
		if (!srcFile){
			cerr<<"Error: cannot open source file"<<endl;
			return FAILURE;
		}
		srcFile.seekg(0, srcFile.end);
		int fileSize=srcFile.tellg();
		if (fileSize>N*BLOCK_SIZE){
			cerr<<"Error: file too big"<<endl;
			return FAILURE;
		}

		//allocate blocks
		vector<int> blocks;
		for (int i=0; i<fileSize; i+=BLOCK_SIZE){
			int t=findAndMarkNextAvailableBlock(FILE_BLOCK_OFFSET); //allocate blocks here...
			if (t==FAILURE){
				for (--i; i!=-1; --i)
					unMarkBlockBitmap(blocks[i]); //...and if fail, free all pre-allocated blocks
				cerr<<"Error: no enough blocks available"<<endl;
				return FAILURE;
			}
			blocks.push_back(t);
		}
		if (FAILURE==createEmptyFile(cmd, cwdInodeNum)){
			for (int i=0; i!=blocks.size(); ++i)
				unMarkBlockBitmap(blocks[i]); //if cannot create file, free all pre-allocated blocks
			return FAILURE;
		}

		//write bitmap
		savediskcontent(freeblockbitmap, BITMAP_FOR_FREE_BLOCK_OFFSET*BLOCK_SIZE, BITMAP_FOR_FREE_BLOCK_SIZE*BLOCK_SIZE);

		//change an empty file into a file with size
		string target=cmd[1];
		if (target[target.size()-1]!='/') target.push_back('/');
		target+=cmd[2];
		int wdInodeNum=PATH_TO_INODE_NUMBER(target.c_str(), cwdInodeNum);
		inode_t &inode=INODE_NUMBER_TO_INODE(wdInodeNum, inode_table);
		inode.size=fileSize;
		for (int i=0; i!=blocks.size(); ++i)
			inode.block_numbers[i]=blocks[i];

		//write inode table
		savediskcontent((byte*)inode_table, INODE_TABLE_OFFSET*BLOCK_SIZE, INODE_TABLE_SIZE_REAL);
		
		srcFile.seekg(0, srcFile.beg);
		byte tb[BLOCK_SIZE];
		for (int i=0; i!=blocks.size(); ++i){
			memset(tb, 0, BLOCK_SIZE*sizeof(byte));
			srcFile.read((char*)tb, (fileSize-i)<BLOCK_SIZE?(fileSize-i):BLOCK_SIZE);
			WRITE_BLOCK_BY_BLOCK_NUMBER(inode.block_numbers[i], tb); //write file block by block
		}
		srcFile.close();
		return 0;
	}

	//command createDirectory or mkdir
	static int createDirecotory(vector<string> & cmd, int cwdInodeNum){
		int blockNum=findAndMarkNextAvailableBlock(FILE_BLOCK_OFFSET);
		if (blockNum==FAILURE){
			cerr<<"Error: no available block"<<endl;
			return FAILURE;
		}
		if (createEmptyFile(cmd, cwdInodeNum)==FAILURE){
			unMarkBlockBitmap(blockNum); //same process as insertFile
			return FAILURE;
		}
		string target=cmd[1];
		int pwdInodeNum=PATH_TO_INODE_NUMBER(target.c_str(), cwdInodeNum);
		if (target[target.size()-1]!='/') target.push_back('/');
		target+=cmd[2];
		int wdInodeNum=PATH_TO_INODE_NUMBER(target.c_str(), cwdInodeNum);
		inode_t &inode=INODE_NUMBER_TO_INODE(wdInodeNum,inode_table);
		//change empty file into a dir
		inode.type=FS_DIRECTORY;
		inode.size=BLOCK_SIZE;
		inode.block_numbers[0]=blockNum;
		savediskcontent((byte*)inode_table, INODE_TABLE_OFFSET*BLOCK_SIZE, INODE_TABLE_SIZE_REAL);

		//generate dir content, . and ..
		byte tb[BLOCK_SIZE]={0};
		tb[0]='.';
		tb[BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET]=wdInodeNum;
		tb[BLOCK_DIRECTORY_ENTRY_SIZE]=tb[BLOCK_DIRECTORY_ENTRY_SIZE+1]='.';
		tb[BLOCK_DIRECTORY_ENTRY_SIZE+BLOCK_DIRECTORY_ENTRY_INODENUM_OFFSET]=pwdInodeNum;
		savediskcontent(tb, blockNum*BLOCK_SIZE, BLOCK_SIZE);

		return 0;
	}

	//command fsck
	static int fsck_() {
		byte bitmap[BITMAP_FOR_FREE_BLOCK_SIZE*BLOCK_SIZE]={0};
		for (int i=0; i!=INODE_NUM; ++i) {
			inode_t &it=inode_table[i];
			if (it.type!=FS_UNUSEDINODE) {
				for (int j=0; j*BLOCK_SIZE<it.size; ++j) {
					int blockNum=it.block_numbers[j];
					if (((bitmap[blockNum/8]>>(blockNum%8))&1)==1) {
						cerr<<"ERROR: block bitmap conflict at inode number "<<i<<" and block number "<<blockNum<<endl;
						return FAILURE;
					}
					bitmap[blockNum/8]|=1<<(blockNum%8);
				}
			}
		}
		if (memcmp(bitmap, freeblockbitmap, BITMAP_FOR_FREE_BLOCK_SIZE*BLOCK_SIZE)!=0)
			for (int i=0; i!=BLOCK_NUM; ++i)
				if (((bitmap[i/8]>>(i%8))&1)!=((bitmap[i/8]>>(i%8))&1)) {
					cerr<<"ERROR: block bitmap mismarked, block number "<<i<<endl;
					return FAILURE;
				}
		cout<<"fsck: check finished with no error found"<<endl;
		return 0;
	}
};

int _tmain(int argc, char* argv[]){
	int cwdInodeNum=0;
	load_disk_structure();
	if (argc==1||(argc>=2&&strcmp(argv[1], "shell"))==0){ //if no command or shell parameter, enter interactive shell
		while (true){
			cout<<"simple_sh 0.1  / > ";//TODO display CWD
			vector<string> command;
			string s;
			getline(cin, s);
			istringstream iss(s);
			while (iss>>s)
				command.push_back(s);
			if (command.size()==0 || command[0]=="") continue;
			if (command[0]=="exit"){
				cout<<"Exited."<<endl;
				return 0;
			}
			cout<<"\n>>>> Exited with return code "<<(Commands::execute(command, cwdInodeNum))<<" <<<<"<<endl;
		}
	}
	else{ //otherwise, execute single command and exit
		vector<string> command;
		for (int i=1; i<argc; ++i)
			command.push_back(string(argv[i]));
		cout<<"\n>>>> Exited with return code "<<(Commands::execute(command, cwdInodeNum))<<" <<<<"<<endl;
		return 0;
	}
	return 0;
}
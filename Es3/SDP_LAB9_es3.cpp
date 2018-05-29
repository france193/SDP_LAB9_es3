/**
* Name:    Francesco
* Surname: Longo
* ID:      223428
* Lab:     9
* Ex:      3
*
* Visit N directory trees concurrently with N threads.
*
* A C program receives N parameters on the command line.
* Each parameter indicates a relative or an absolute path to
* a file system directory tree.
*
* The program has to run N threads (one for each argument).
* Each thread recursively visits one of the directories, and,
* for each directory entry, it prints-out its thread identifier
* and the directory entry name.
* The main thread awaits for the termination of all threads.
* When all threads have visited their directory also the program
* ends.
*
* Version A
* As all output lines are generated independently by each single
* thread, printing messages from different threads are interleaved
* on standard output.
*
* Version B
* To avoid the problem of Version A, Version B uses files.
* In other words, each thread stores its output on a file whose name
* is generated (to avoid conflicts) based on the thread identifier.
* When all working threads end, the main thread copies all files on
* standard output with the desired strategy (and no interruption).
*
* Version C - Optional / Elective
* (need synchronization among threads)
* This version generalizes version B by using a centralized thread
* (the "output thread") dedicated to generate the entire application
* output.
* All threads resort to the centralized and dedicated output thread to
* perform all operations on standard output.
* The output thread is executed within a critical section, and it just
* awaits for more output to come in, and then it takes care of it with
* the strategy at choice (i.e.,, it may store messages on files, like
* Version B, it may use a dedicated data structure to store messages, it
* may decide which verbosity level to use, it may compute output
* statistics, etc.).
*
**/

// !UNICODE
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

// !_CRT_SECURE_NO_WARNINGS
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif 

// Include
#include <Windows.h>
#include <tchar.h>
#include <assert.h>
#include <stdio.h>

// The version can be set to 'A', 'B' or 'C'
#define VERSION 'C'

// VERSION == 'A'
#if VERSION == 'A'
#define visitDirectory	visitDirectoryA
#define whatToDo		whatToDoA
#define collectData		collectDataA
#endif

// VERSION == 'B'
#if VERSION == 'B'
#define visitDirectory	visitDirectoryB
#define whatToDo		whatToDoB
#define collectData		collectDataB
#endif 

// VERSION == 'C'
#if VERSION == 'C'
#define visitDirectory visitDirectoryC
#define whatToDo whatToDoC
#define collectData collectDataC
#endif

// Global varialbes
CRITICAL_SECTION cs;

// Define
#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_DOT 3

#define SECONDS 10

// Prototypes
DWORD WINAPI visitDirectoryA(LPVOID param);
DWORD WINAPI visitDirectoryB(LPVOID param);
DWORD WINAPI visitDirectoryC(LPVOID param);
VOID whatToDoA(LPTSTR path, LPTSTR entryName, HANDLE fHandle);
VOID whatToDoB(LPTSTR path, LPTSTR entryName, HANDLE fHandle);
VOID whatToDoC(LPTSTR path, LPTSTR entryName, HANDLE fHandle);
VOID collectDataA(DWORD index);
VOID collectDataB(DWORD index);
VOID collectDataC(DWORD index);

VOID visitDirectoryRecursiveAndDo(LPTSTR path1, DWORD level, VOID(*toDo)(LPTSTR path, LPTSTR entryName, HANDLE fHandle), HANDLE fHandle);
static DWORD FileType(LPWIN32_FIND_DATA pFileData);
LPTSTR getFileNameByThreadId(DWORD tId);

int Return(int seconds, int value);
LPWSTR getErrorMessageAsString(DWORD errorCode);

// main
INT _tmain(INT argc, LPTSTR argv[]) {
	LPHANDLE tHandles;
	LPDWORD threadIds;
	DWORD nThreads;
	DWORD i;

	// check the input parameters
	if (argc < 2) {
		_ftprintf(stderr, _T("Usage: %s list_of_folders_to_be_visited\n"), argv[0]);
		return 1;
	}

	// get number of desired thread
	nThreads = argc - 1;

	// allocate handle for thread and array for thread ID
	tHandles = (LPHANDLE)malloc(nThreads * sizeof(HANDLE));
	threadIds = (LPDWORD)malloc(nThreads * sizeof(DWORD));

	// check correctness of allocation
	if (tHandles == NULL || threadIds == NULL) {
		_ftprintf(stderr, _T("Error allocating space for threads\n"));
		return 2;
	}

	// initialize critical section
	InitializeCriticalSection(&cs);

	// create threads with visitDirectory code and check if they are correctly created
	for (i = 0; i < nThreads; i++) {
		tHandles[i] = CreateThread(0, 0, visitDirectory, argv[i + 1], 0, &threadIds[i]);
		if (tHandles[i] == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, _T("Error creating thread\n"));
			return 3;
		}
	}

	// wait all threads
	WaitForMultipleObjects(nThreads, tHandles, TRUE, INFINITE);

	// check if all are correctly ended and close handle
	for (i = 0; i < nThreads; i++) {
		// want to be sure that it is not NULL
		assert(tHandles[i]);
		CloseHandle(tHandles[i]);
		collectData(threadIds[i]);
	}

	// free dynamic arrays
	free(threadIds);
	free(tHandles);

	Return(SECONDS, 0);
}

/************************* A ****************************/
DWORD WINAPI visitDirectoryA(LPVOID param) {
	// take his parameter
	LPTSTR path = (LPTSTR)param;
	// start visit
	visitDirectoryRecursiveAndDo(path, 0, whatToDo, NULL);

	return 0;
}

VOID whatToDoA(LPTSTR path, LPTSTR entryName, HANDLE fHandle) {
	// normal print on standard output
	_tprintf(_T("%u - %s\n"), GetCurrentThreadId(), entryName);

	return;
}

VOID collectDataA(DWORD thId) {
	// nothing
	return;
}

/************************* B ****************************/

DWORD WINAPI visitDirectoryB(LPVOID param) {
	LPTSTR path = (LPTSTR)param;
	HANDLE hOut;
	LPTSTR filename;

	// create file name from thread id
	filename = getFileNameByThreadId(GetCurrentThreadId());

	// create file handle and error check
	hOut = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Error creating file %s\n"), filename);
		free(filename);
		return 1;
	}

	// free filename
	free(filename);

	visitDirectoryRecursiveAndDo(path, 0, whatToDo, hOut);

	CloseHandle(hOut);

	return 0;
}

VOID whatToDoB(LPTSTR path, LPTSTR entryName, HANDLE fHandle) {
	DWORD strLen;
	DWORD nWrote;
	TCHAR line[MAX_PATH + 20];

	// write info on a string and then on a file
	_stprintf(line, _T("%u - %s\n"), GetCurrentThreadId(), entryName);

	strLen = (DWORD)_tcscnlen(line, MAX_PATH + 20);

	assert(strLen < MAX_PATH + 20);

	// write on a file and check
	if (!WriteFile(fHandle, line, strLen * sizeof(TCHAR), &nWrote, NULL) || nWrote != strLen * sizeof(TCHAR)) {
		_ftprintf(stderr, _T("%u - Error writing file"), GetCurrentThreadId());
	}

	return;
}

VOID collectDataB(DWORD thId) {
	LPTSTR filename;
	HANDLE fIn;
	LARGE_INTEGER fileSize;
	LPTSTR content;
	DWORD nRead;

	// print the content of the file

	filename = getFileNameByThreadId(thId);

	fIn = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Impossible to open file %s\n"), filename);
		free(filename);
		return;
	}

	if (!GetFileSizeEx(fIn, &fileSize)) {
		_ftprintf(stderr, _T("Impossible to get file size for %s\n"), filename);
		free(filename);
		CloseHandle(fIn);
		return;
	}

	assert(fileSize.HighPart == 0);

	// allocate an extra position for the '\0' terminator
	content = (LPTSTR)malloc(fileSize.LowPart + sizeof(TCHAR));

	if (content == NULL) {
		_ftprintf(stderr, _T("Impossible to allocate content for file %s\n"), filename);
		free(filename);
		CloseHandle(fIn);
		return;
	}

	if (!ReadFile(fIn, content, fileSize.LowPart, &nRead, NULL) || nRead != fileSize.LowPart) {
		_ftprintf(stderr, _T("Error in reading file %s\n"), filename);
		free(filename);
		free(content);
		CloseHandle(fIn);
		return;
	}

	CloseHandle(fIn);

	content[fileSize.LowPart / sizeof(TCHAR)] = _T('\0');

	_tprintf(_T("%s"), content);

	free(filename);
	free(content);

	return;
}

/************************* C ****************************/

DWORD WINAPI visitDirectoryC(LPVOID param) {
	LPTSTR path = (LPTSTR)param;

	EnterCriticalSection(&cs);

	// visit is done on mutual exclusion
	visitDirectoryRecursiveAndDo(path, 0, whatToDo, NULL);

	LeaveCriticalSection(&cs);

	return 0;
}

VOID whatToDoC(LPTSTR path, LPTSTR entryName, HANDLE fHandle) {
	// print on output in mutual exclusion
	_tprintf(_T("%u - %s\n"), GetCurrentThreadId(), entryName);

	return;
}

VOID collectDataC(DWORD thId) {
	// nothing
	return;
}
/********************************************************/

VOID visitDirectoryRecursiveAndDo(LPTSTR path1, DWORD level, VOID(*toDo)(LPTSTR path, LPTSTR entryName, HANDLE fHandle), HANDLE fHandle) {
	WIN32_FIND_DATA findFileData;
	HANDLE hFind;
	TCHAR searchPath[MAX_PATH];
	TCHAR newPath1[MAX_PATH];

	// build the searchPath string, to be able to search inside path1: searchPath = path1\*
	_sntprintf(searchPath, MAX_PATH - 1, _T("%s\\*"), path1);
	searchPath[MAX_PATH - 1] = 0;

	// search inside path1
	hFind = FindFirstFile(searchPath, &findFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("FindFirstFile failed. Error: %x\n"), GetLastError());
		return;
	}

	do {
		// generate a new path by appending to path1 the name of the found entry
		_sntprintf(newPath1, MAX_PATH, _T("%s\\%s"), path1, findFileData.cFileName);

		toDo(findFileData.cFileName, newPath1, fHandle);

		// check the type of file

		DWORD fType = FileType(&findFileData);

		if (fType == TYPE_FILE) {
			// this is a file
			//_tprintf(_T("FILE %s "), path1);
			// call the function to copy with modifications
		}

		if (fType == TYPE_DIR) {
			// this is a directory
			//_tprintf(_T("DIR %s\n"), path1);
			// recursive call to the new paths
			visitDirectoryRecursiveAndDo(newPath1, level + 1, toDo, fHandle);
		}
	} while (FindNextFile(hFind, &findFileData));

	FindClose(hFind);
	return;
}

static DWORD FileType(LPWIN32_FIND_DATA pFileData) {
	BOOL IsDir;
	DWORD FType;

	FType = TYPE_FILE;
	IsDir = (pFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	if (IsDir) {
		if (lstrcmp(pFileData->cFileName, _T(".")) == 0 || lstrcmp(pFileData->cFileName, _T("..")) == 0) {
			FType = TYPE_DOT;
		}
		else {
			FType = TYPE_DIR;
		}
	}
		
	return FType;
}

LPTSTR getFileNameByThreadId(DWORD tId) {
	LPTSTR result = (LPTSTR)malloc(MAX_PATH * sizeof(TCHAR));

	if (result == NULL) {
		return NULL;
	}

	_stprintf(result, _T("temp%u.txt"), tId);

	return result;
}

int Return(int seconds, int value) {
	Sleep(seconds * 1000);
	return value;
}

LPWSTR getErrorMessageAsString(DWORD errorCode) {
	LPWSTR errString = NULL;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		0,
		errorCode,
		0,
		(LPWSTR)&errString,
		0,
		0);

	return errString;
}

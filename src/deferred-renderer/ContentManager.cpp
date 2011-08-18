#include "PCH.h"
#include "ContentManager.h"

ContentManager::ContentManager()
{
}

ContentManager::~ContentManager()
{
	for (UINT i = 0; i < _searchPaths.size(); i++)
	{
		SAFE_DELETE(_searchPaths[i]);
	}
}

HRESULT ContentManager::getPath(const WCHAR* inPathSegment, WCHAR* outputPath, UINT outputLen)
{
	for (UINT i = 0; i < _searchPaths.size(); i++)
	{
		wcsncpy_s(outputPath, outputLen, _searchPaths[i], MAX_PATH);
		wcsncat_s(outputPath, outputLen, L"\\", MAX_PATH);
		wcsncat_s(outputPath, outputLen, inPathSegment, MAX_PATH);

		if (GetFileAttributes(outputPath) != INVALID_FILE_ATTRIBUTES)
		{
			return S_OK;
		}
	}

	return E_FAIL;
}

void ContentManager::AddSearchPath(const WCHAR* path)
{
	WCHAR* newPath = new WCHAR[MAX_PATH];

	_wgetcwd(newPath, MAX_PATH);
	wcsncat_s(newPath, MAX_PATH, L"\\", MAX_PATH);
	wcsncat_s(newPath, MAX_PATH, path, MAX_PATH);

	_searchPaths.push_back(newPath);
}

void ContentManager::ReleaseContent()
{
	std::map<long, ContentType*>::iterator i;
	for (i = _loadedContent.begin(); i != _loadedContent.end(); i++)
	{
		SAFE_RELEASE(i->second);
	}

	_loadedContent.clear();
}
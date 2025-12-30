#include "vdjYoutube.h"
#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "json.hpp"
#include "resource.h"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")

namespace fs = std::filesystem;

using json = nlohmann::json;

HINSTANCE g_hInst = NULL;

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		g_hInst = hInst;
	}
	return TRUE;
}

HRESULT VDJ_API DllGetClassObject(const GUID &rclsid, const GUID &riid, void **ppObject)
{
	if (memcmp(&riid, &IID_IVdjPluginOnlineSource, sizeof(GUID)) == 0)
	{
		*ppObject = new VdjYoutube();
		return S_OK;
	}

	return CLASS_E_CLASSNOTAVAILABLE;
}

VdjYoutube::VdjYoutube() : m_workerRunning(true)
{
	char path[MAX_PATH];
	if (GetModuleFileNameA(g_hInst, path, MAX_PATH))
	{
		PathRemoveFileSpecA(path);
		m_pluginPath = path;
		m_cachePath = m_pluginPath + "\\Cache";
		CreateDirectoryA(m_cachePath.c_str(), NULL);
	}
	m_workerThread = std::thread(&VdjYoutube::DownloadWorker, this);
}

VdjYoutube::~VdjYoutube()
{
	m_workerRunning = false;
	m_queueCv.notify_all();
	if (m_workerThread.joinable())
		m_workerThread.join();
}

HRESULT VDJ_API VdjYoutube::OnLoad()
{
	return S_OK;
}

HRESULT VDJ_API VdjYoutube::OnGetPluginInfo(TVdjPluginInfo8 *infos)
{
	infos->PluginName = "Youtube";
	infos->Author = "g0razd";
	infos->Description = "YouTube Source with Caching";
	infos->Version = "1.1";
	infos->Flags = 0;

	// Load icon from resource
	infos->Bitmap = (HBITMAP)LoadImageA(g_hInst, MAKEINTRESOURCEA(IDB_ICON1), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);

	return S_OK;
}

ULONG VDJ_API VdjYoutube::Release()
{
	delete this;
	return 0;
}

HRESULT VDJ_API VdjYoutube::OnSearch(const char *searchString, IVdjTracksList *tracksList)
{
	m_stopSearch = false;
	std::string cmd = "\"" + GetYtDlpPath() + "\" --flat-playlist -j --no-warnings \"ytsearch20:" + searchString + "\"";

	std::string output = ExecCmd(cmd.c_str());
	std::istringstream iss(output);
	std::string line;

	while (std::getline(iss, line) && !m_stopSearch)
	{
		try
		{
			auto j = json::parse(line);
			std::string id = j.value("id", "");
			std::string title = j.value("title", "Unknown");
			std::string artist = j.value("uploader", "YouTube");
			std::string thumbnail = j.value("thumbnail", "");
			int duration = j.value("duration", 0);

			if (!id.empty())
			{
				const char *streamUrl = nullptr;
				std::string cachePath;
				if (IsCached(id))
				{
					cachePath = GetCacheFilePath(id);
					streamUrl = cachePath.c_str();
				}

				// uniqueId, title, artist, remix, genre, label, comment, coverUrl, streamUrl, length
				tracksList->add(id.c_str(), title.c_str(), artist.c_str(), nullptr, nullptr, nullptr, nullptr, thumbnail.empty() ? nullptr : thumbnail.c_str(), streamUrl, (float)duration);

				// Queue metadata for potential download (source="search")
				SaveMetadata(id, title, artist, duration, "search");
			}
		}
		catch (...)
		{
		}
	}

	return S_OK;
}

HRESULT VDJ_API VdjYoutube::OnSearchCancel()
{
	m_stopSearch = true;
	return S_OK;
}

HRESULT VDJ_API VdjYoutube::GetStreamUrl(const char *uniqueId, IVdjString &url, IVdjString &errorMessage)
{
	if (IsCached(uniqueId))
	{
		std::string path = GetCacheFilePath(uniqueId);
		url = path.c_str();
		return S_OK;
	}

	// Not cached, stream it and queue download
	QueueDownload(uniqueId);

	// For streaming, we need a single URL with both audio and video.
	// "bestvideo+bestaudio" returns two URLs, which VDJ might not handle correctly (black screen/no audio).
	// So we prefer pre-merged formats like 22 (720p) or 18 (360p).
	std::string cmd = "\"" + GetYtDlpPath() + "\" -f \"22/18/best[ext=mp4]\" -g \"" + uniqueId + "\"";
	std::string output = ExecCmd(cmd.c_str());

	std::string cleanUrl;
	std::istringstream iss(output);
	std::string line;
	while (std::getline(iss, line))
	{
		// Trim
		line.erase(0, line.find_first_not_of(" \t\r\n"));
		line.erase(line.find_last_not_of(" \t\r\n") + 1);
		if (line.find("http") == 0)
		{
			cleanUrl = line;
			break;
		}
	}

	if (cleanUrl.empty())
	{
		errorMessage = "Failed to get stream URL";
		return E_FAIL;
	}

	url = cleanUrl.c_str();
	return S_OK;
}

HRESULT VDJ_API VdjYoutube::GetFolderList(IVdjSubfoldersList *subfoldersList)
{
	subfoldersList->add("cached_songs", "History");

	std::string playlistFile = m_pluginPath + "\\playlists.txt";
	std::ifstream file(playlistFile);
	std::string line;

	while (std::getline(file, line))
	{
		size_t delimiter = line.find('|');
		if (delimiter != std::string::npos)
		{
			std::string name = line.substr(0, delimiter);
			std::string url = line.substr(delimiter + 1);
			subfoldersList->add(url.c_str(), name.c_str());
		}
	}
	return S_OK;
}

HRESULT VDJ_API VdjYoutube::GetFolder(const char *folderUniqueId, IVdjTracksList *tracksList)
{
	if (std::string(folderUniqueId) == "cached_songs")
	{
		if (!fs::exists(m_cachePath))
			return S_OK;

		for (const auto &entry : fs::directory_iterator(m_cachePath))
		{
			if (entry.path().extension() == ".json")
			{
				try
				{
					std::ifstream f(entry.path());
					json j = json::parse(f);

					std::string id = j.value("id", "");
					std::string title = j.value("title", "Unknown");
					std::string artist = j.value("artist", "YouTube");
					int duration = j.value("duration", 0);
					std::string source = j.value("source", "search");

					// Only show if it's not from a playlist (unless user wants all)
					// User request: "songs that are in playlists dont need to show up in the cached section"
					if (source == "playlist")
						continue;

					if (!id.empty() && IsCached(id.c_str()))
					{
						std::string cachePath = GetCacheFilePath(id);
						tracksList->add(id.c_str(), title.c_str(), artist.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, cachePath.c_str(), (float)duration);
					}
				}
				catch (...)
				{
				}
			}
		}
		return S_OK;
	}

	// folderUniqueId is the URL
	std::string cmd = "\"" + GetYtDlpPath() + "\" --flat-playlist -j --no-warnings \"" + folderUniqueId + "\"";
	std::string output = ExecCmd(cmd.c_str());
	std::istringstream iss(output);
	std::string line;

	while (std::getline(iss, line))
	{
		try
		{
			auto j = json::parse(line);
			std::string id = j.value("id", "");
			std::string title = j.value("title", "Unknown");
			std::string artist = j.value("uploader", "YouTube");
			std::string thumbnail = j.value("thumbnail", "");
			int duration = j.value("duration", 0);

			if (!id.empty())
			{
				const char *streamUrl = nullptr;
				std::string cachePath;
				if (IsCached(id))
				{
					cachePath = GetCacheFilePath(id);
					streamUrl = cachePath.c_str();
				}
				tracksList->add(id.c_str(), title.c_str(), artist.c_str(), nullptr, nullptr, nullptr, nullptr, thumbnail.empty() ? nullptr : thumbnail.c_str(), streamUrl, (float)duration);
			}
		}
		catch (...)
		{
		}
	}
	return S_OK;
}

HRESULT VDJ_API VdjYoutube::GetContextMenu(const char *uniqueId, IVdjContextMenu *contextMenu)
{
	contextMenu->add("Download Now");
	return S_OK;
}

HRESULT VDJ_API VdjYoutube::OnContextMenu(const char *uniqueId, size_t menuIndex)
{
	if (menuIndex == 0)
	{
		QueueDownload(uniqueId);
	}
	return S_OK;
}

HRESULT VDJ_API VdjYoutube::GetFolderContextMenu(const char *folderUniqueId, IVdjContextMenu *contextMenu)
{
	contextMenu->add("Cache Playlist");
	return S_OK;
}

HRESULT VDJ_API VdjYoutube::OnFolderContextMenu(const char *folderUniqueId, size_t menuIndex)
{
	if (menuIndex == 0)
	{
		std::string url = folderUniqueId;
		// Spawn a thread to parse playlist and queue items
		std::thread([this, url]()
					{
            std::string cmd = "\"" + GetYtDlpPath() + "\" --flat-playlist -j --no-warnings \"" + url + "\"";
            std::string output = ExecCmd(cmd.c_str());
            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line)) {
                try {
                    auto j = json::parse(line);
                    std::string id = j.value("id", "");
                    if (!id.empty()) {
                        // Pass source="playlist"
                        QueueDownload(id, j.value("title", ""), j.value("uploader", ""), j.value("duration", 0), "playlist");
                    }
                } catch (...) {}
            } })
			.detach();
	}
	return S_OK;
}

// Private Methods

void VdjYoutube::DownloadWorker()
{
	while (m_workerRunning)
	{
		DownloadTask task;
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_queueCv.wait(lock, [this]
						   { return !m_downloadQueue.empty() || !m_workerRunning; });

			if (!m_workerRunning)
				break;

			task = m_downloadQueue.front();
			m_downloadQueue.pop();
		}

		if (IsCached(task.id))
			continue;

		// Save metadata if we have it
		if (!task.title.empty())
		{
			SaveMetadata(task.id, task.title, task.artist, task.duration, task.source);
		}

		std::string outputFile = GetCacheFilePath(task.id);
		std::string format;
		std::string cmd;

		if (HasFfmpeg())
		{
			// If ffmpeg is available, we can merge best video and audio (up to 1080p)
			// Force merge to mp4 container to ensure VDJ compatibility
			format = "bestvideo[height<=1080]+bestaudio/best[height<=1080]/best";
			cmd = "\"" + GetYtDlpPath() + "\" -f \"" + format + "\" --merge-output-format mp4 -o \"" + outputFile + "\" \"" + task.id + "\"";
		}
		else
		{
			// Fallback to single file formats if no ffmpeg
			format = "22/18/best[ext=mp4]/best";
			cmd = "\"" + GetYtDlpPath() + "\" -f \"" + format + "\" -o \"" + outputFile + "\" \"" + task.id + "\"";
		}

		// Run download (blocking in this thread)
		std::string output = ExecCmd(cmd.c_str());

		// Verify download success
		HANDLE hFile = CreateFileA(outputFile.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER size;
			if (GetFileSizeEx(hFile, &size) && size.QuadPart > 1024)
			{
				// Download successful
				CloseHandle(hFile);
			}
			else
			{
				// File too small or empty, delete it
				CloseHandle(hFile);
				DeleteFileA(outputFile.c_str());
				std::string metaPath = m_cachePath + "\\" + task.id + ".json";
				DeleteFileA(metaPath.c_str());
			}
		}
		else
		{
			// File not found, delete metadata
			std::string metaPath = m_cachePath + "\\" + task.id + ".json";
			DeleteFileA(metaPath.c_str());
		}
	}
}

void VdjYoutube::QueueDownload(const std::string &id, const std::string &title, const std::string &artist, int duration, const std::string &source)
{
	std::lock_guard<std::mutex> lock(m_queueMutex);
	if (m_queuedIds.find(id) == m_queuedIds.end() && !IsCached(id))
	{
		m_queuedIds.insert(id);
		m_downloadQueue.push({id, title, artist, duration, source});
		m_queueCv.notify_one();
	}
}

bool VdjYoutube::IsCached(const std::string &id)
{
	std::string path = GetCacheFilePath(id);
	return PathFileExistsA(path.c_str());
}

std::string VdjYoutube::GetCacheFilePath(const std::string &id)
{
	return m_cachePath + "\\" + id + ".mp4";
}

std::string VdjYoutube::GetYtDlpPath()
{
	std::string bundled = m_pluginPath + "\\yt-dlp.exe";
	if (fs::exists(bundled))
		return bundled;
	return "yt-dlp";
}

std::string VdjYoutube::ExecCmd(const char *cmd)
{
	std::string result;
	HANDLE hPipeRead, hPipeWrite;
	SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

	if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0))
		return "";

	STARTUPINFOA si = {sizeof(STARTUPINFOA)};
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdOutput = hPipeWrite;
	si.hStdError = hPipeWrite; // Capture stderr too
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi = {0};
	char cmdLine[2048];
	strcpy_s(cmdLine, cmd);

	if (CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(hPipeWrite); // Close write end in parent

		char buffer[128];
		DWORD bytesRead;
		while (ReadFile(hPipeRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			result += buffer;
		}

		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else
	{
		CloseHandle(hPipeWrite);
	}
	CloseHandle(hPipeRead);
	return result;
}

void VdjYoutube::SaveMetadata(const std::string &id, const std::string &title, const std::string &artist, int duration, const std::string &source)
{
	if (id.empty())
		return;

	std::string path = m_cachePath + "\\" + id + ".json";
	// We overwrite if it exists to update the source if needed, or just check if we want to preserve "search" over "playlist"
	// For now, let's just write it.

	json j;
	if (fs::exists(path))
	{
		try
		{
			std::ifstream f(path);
			j = json::parse(f);
		}
		catch (...)
		{
		}
	}

	j["id"] = id;
	if (!title.empty())
		j["title"] = title;
	if (!artist.empty())
		j["artist"] = artist;
	if (duration > 0)
		j["duration"] = duration;
	if (!source.empty())
		j["source"] = source;

	std::ofstream file(path);
	file << j.dump(4);
}

bool VdjYoutube::HasFfmpeg()
{
	return fs::exists(m_pluginPath + "\\ffmpeg.exe");
}

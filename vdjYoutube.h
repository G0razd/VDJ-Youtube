#pragma once

#include "vdjOnlineSource.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <set>

class VdjYoutube : public IVdjPluginOnlineSource
{
public:
	VdjYoutube();
	~VdjYoutube();

	// IVdjPlugin
	HRESULT VDJ_API OnLoad() override;
	HRESULT VDJ_API OnGetPluginInfo(TVdjPluginInfo8 *infos) override;
	ULONG VDJ_API Release() override;

	// IVdjPluginOnlineSource
	HRESULT VDJ_API OnSearch(const char *search, IVdjTracksList *tracksList) override;
	HRESULT VDJ_API OnSearchCancel() override;
	HRESULT VDJ_API GetStreamUrl(const char *uniqueId, IVdjString &url, IVdjString &errorMessage) override;

	// Folders / Playlists
	HRESULT VDJ_API GetFolderList(IVdjSubfoldersList *subfoldersList) override;
	HRESULT VDJ_API GetFolder(const char *folderUniqueId, IVdjTracksList *tracksList) override;

	// Context menu
	HRESULT VDJ_API GetContextMenu(const char *uniqueId, IVdjContextMenu *contextMenu) override;
	HRESULT VDJ_API OnContextMenu(const char *uniqueId, size_t menuIndex) override;
	HRESULT VDJ_API GetFolderContextMenu(const char *folderUniqueId, IVdjContextMenu *contextMenu) override;
	HRESULT VDJ_API OnFolderContextMenu(const char *folderUniqueId, size_t menuIndex) override;

private:
	std::string m_pluginPath;
	std::string m_cachePath;
	bool m_stopSearch = false;

	// Download Queue
	struct DownloadTask
	{
		std::string id;
		std::string title;
		std::string artist;
		int duration;
		std::string source; // "search" or "playlist"
	};

	std::queue<DownloadTask> m_downloadQueue;
	std::mutex m_queueMutex;
	std::condition_variable m_queueCv;
	std::atomic<bool> m_workerRunning;
	std::thread m_workerThread;
	std::set<std::string> m_queuedIds;

	void DownloadWorker();
	void QueueDownload(const std::string &id, const std::string &title = "", const std::string &artist = "", int duration = 0, const std::string &source = "search");
	bool IsCached(const std::string &id);
	std::string GetCacheFilePath(const std::string &id);
	std::string GetYtDlpPath();
	bool HasFfmpeg();

	// Helpers
	std::string ExecCmd(const char *cmd);
	void Log(const std::string &message);

	void SaveMetadata(const std::string &id, const std::string &title, const std::string &artist, int duration, const std::string &source);
};

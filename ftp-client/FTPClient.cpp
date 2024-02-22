#include "FTPClient.h"

#include <future>
#include <ctime>
#include <iterator>
#include <experimental/filesystem>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#endif

FTPClient::FTPClient(const std::string& host, const std::string& username, const std::string& password)
    : host_(host),
      username_(username),
      password_(password)
{
    curl_global_init(CURL_GLOBAL_ALL);


}

FTPClient::~FTPClient() {
    curl_global_cleanup();
}

size_t FTPClient::writeCallback(void* contents, size_t size, size_t nmemb, std::ofstream* file) {
    size_t dataSize = size * nmemb;
    file->write((char*)contents, dataSize);
    return dataSize;
}

size_t FTPClient::readCallback(void* buffer, size_t size, size_t nmemb, std::ifstream* file) {
    file->read((char*)buffer, size * nmemb);
    return file->gcount();
}

bool FTPClient::resumeEnabled(CURL* curl, const std::string& remoteFilePath) {
    std::stringstream command;
    command << "SIZE " << remoteFilePath;

    curl_easy_setopt(curl, CURLOPT_URL, ("ftp://" + host_).c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, command.str().c_str());

    curl_easy_perform(curl);


    // 清理设置的选项
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);

    long responseCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    if (responseCode == 213) {
        return true;
    }

    return false;
}

off_t FTPClient::getLocalFileSize(const std::string& localFilePath) {
    std::ifstream file(localFilePath, std::ios::in | std::ios::binary);
    if (!file) {
        return 0;
    }

    file.seekg(0, std::ios::end);
    off_t position = file.tellg();

    file.close();

    return position;
}

off_t FTPClient::getRemoteFileSize(CURL* curl, const std::string& remoteFilePath) {
    // 设置远程路径和URL
    std::stringstream str;
    curl_easy_setopt(curl, CURLOPT_URL, ("ftp://" + host_ + remoteFilePath).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, copyDataSizeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    CURLcode result = curl_easy_perform(curl);

    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

    if (result == CURLE_OK) {
        double fileSize;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &fileSize);
        return static_cast<off_t>(fileSize);
    } else {
        std::cerr << "Failed to get remote file size for: " << remoteFilePath << ". Error: " << curl_easy_strerror(result) << std::endl;
        return 0;
    }
}


void FTPClient::deleteRemoteFile(CURL* curl, const std::string& remoteFilePath) {

    std::stringstream command;
    command << "DELE " << remoteFilePath;

    curl_easy_setopt(curl, CURLOPT_URL, ("ftp://" + host_).c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, command.str().c_str());

    CURLcode result = curl_easy_perform(curl);

    // 清理设置的选项
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);

    if (result != CURLE_OK) {
        std::cerr << "Failed to delete remote file: " << remoteFilePath << std::endl;
    }
}

bool FTPClient::createLocalFolder(const std::string& localFolderPath) {
#if defined(_WIN32)
    std::wstring wFolderPath = std::wstring(localFolderPath.begin(), localFolderPath.end());
    if (CreateDirectoryW(wFolderPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    } else {
        std::cerr << "Failed to create local folder: " << localFolderPath << std::endl;
        return false;
    }
#elif defined(__linux__) || defined(__APPLE__)
    if (mkdir(localFolderPath.c_str(), 0777) == 0 || errno == EEXIST) {
        return true;
    } else if (errno == ENOENT) {
        std::string command = "mkdir -p " + localFolderPath;
        if (system(command.c_str()) == 0) {
            return true;
        }
    }
    std::cerr << "Failed to create local folder: " << localFolderPath << std::endl;
    return false;
#else
    // Unsupported platform
    std::cerr << "Platform not supported." << std::endl;
    return false;
#endif
}

bool FTPClient::createRemoteDirectory(const std::string &remoteDirectoryPath)
{
    CURL* curlCreateDir = curl_easy_init();
    if (!curlCreateDir) {
        return false;
    }

    curl_easy_setopt(curlCreateDir, CURLOPT_URL, ("ftp://" + host_).c_str());
    curl_easy_setopt(curlCreateDir, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curlCreateDir, CURLOPT_PASSWORD, password_.c_str());

    std::string directory;
    std::string mkdir;
    std::istringstream iss(remoteDirectoryPath);
    while (std::getline(iss, directory, '/')) {
        if (directory.empty()) continue;

        mkdir += "/" + directory;
        std::stringstream command;
        command << "MKD " << mkdir;

        curl_easy_setopt(curlCreateDir, CURLOPT_CUSTOMREQUEST, command.str().c_str());
        curl_easy_perform(curlCreateDir);

        long responseCode;
        curl_easy_getinfo(curlCreateDir, CURLINFO_RESPONSE_CODE, &responseCode);

        if (responseCode != 257) {
            curl_easy_cleanup(curlCreateDir);
            return false;
        }
    }

    curl_easy_cleanup(curlCreateDir);

    return true;
}

std::vector<std::string> FTPClient::listRemoteFiles(const std::string& remoteFolderPath) {
    std::string strcomm = replaceSpacesWithPercent20(remoteFolderPath);
    strcomm = "/" + strcomm;

    std::vector<std::string> fileList;
    CURL* curl = curl_easy_init();
    if (!curl) {
        return fileList;
    }

    curl_easy_setopt(curl, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
//    curl_easy_setopt(curl, CURLOPT_URL, ("ftp://" + host_).c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "LIST");

    curl_easy_setopt(curl, CURLOPT_URL, ("ftp://" + host_ + strcomm).c_str());
//    curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);

    std::stringstream responseStream;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToStringStreamCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStream);

    CURLcode result = curl_easy_perform(curl);

    // 清理设置的选项
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_cleanup(curl);

    if (result == CURLE_OK) {
        std::string file;
        while (std::getline(responseStream, file)) {
            std::istringstream iss(file);
            std::vector<std::string> tokens{
                std::istream_iterator<std::string>{iss},
                std::istream_iterator<std::string>{}
            };

            std::string fileOrDirectoryName;
            // 从第8项开始遍历并连接字符串
            for (size_t i = 8; i < tokens.size(); ++i) {
                fileOrDirectoryName += (fileOrDirectoryName.empty() ? "" : " ") + tokens[i];
            }

            if (tokens.size() >= 9 && tokens[0][0] == 'd') {
                // 是目录
                if(tokens[8] != "." && tokens[8] != "..")
                {
                    const auto& subFileList = listRemoteFiles(remoteFolderPath + fileOrDirectoryName + "/");
                    fileList.insert(fileList.end(), subFileList.begin(), subFileList.end());
                }
            }else if(tokens.size() >= 9 && tokens[0][0] == '-'){
                std::string modTime = tokens[5] + " " + tokens[6];
                fileList.push_back(remoteFolderPath + fileOrDirectoryName);
            }
        }
    } else {
        std::cerr << "Failed to list remote files: " << remoteFolderPath << std::endl;
    }

    return fileList;
}

std::vector<std::string> FTPClient::listLocalFiles(const std::string &localFolderPath)
{
    std::vector<std::string> fileList;
    // 遍历文件夹内的文件和子文件夹
    for (const auto& entry : std::experimental::filesystem::directory_iterator(localFolderPath)) {
        std::experimental::filesystem::file_status status = entry.status();
        if (std::experimental::filesystem::is_directory(status)) {
            const auto& subFileList = listLocalFiles(entry.path().string());
            fileList.insert(fileList.end(), subFileList.begin(), subFileList.end());
        } else if (std::experimental::filesystem::is_regular_file(status)) {
            fileList.push_back(entry.path().string());
        }
    }
    return fileList;
}

size_t FTPClient::writeToStringStreamCallback(void* contents, size_t size, size_t nmemb, std::stringstream* stream) {
    size_t dataSize = size * nmemb;
    stream->write((char*)contents, dataSize);
    return dataSize;
}

size_t FTPClient::copyDataSizeCallback(void *contents, size_t size, size_t nmemb, void *data)
{
    return size * nmemb;
}

int FTPClient::progressCallback(void *p, double dltotal, double dlnow, double ultotal, double ulnow)
{
    if (p == NULL) {
        std::cerr << "Error: p is null." << std::endl;
        return -1;
    }

    FileTransferInfo* fileInfo = static_cast<FileTransferInfo*>(p);
    switch (fileInfo->transferType) {
    case Download:{
        fileInfo->totalSize = (long)dltotal;
        fileInfo->transferredSize = (long)dlnow;
        fileInfo->remainingSize = (long)(dltotal - dlnow);
        if(dltotal == 0){
            dlnow = 0;
            dltotal = 1;
        }
        fileInfo->transferProgress = (dlnow / dltotal) * 100;
        break;
    }
    case Upload:{
//        fileInfo->totalSize = (long)ultotal;
        fileInfo->transferredSize = (long)ulnow;
        fileInfo->remainingSize = (long)(fileInfo->totalSize - ulnow);

        fileInfo->transferProgress = (ulnow / (double)fileInfo->totalSize) * 100;
        break;
    }
    default:
        break;
    }

    std::cout << "process: " << fileInfo->transferProgress << "% " << fileInfo->filename << std::endl;

    return 0;
}

void FTPClient::recordDownloadedFile(const std::string& localFilePath, const std::string& keyword) {

    std::ofstream recordFile("downloaded_files.txt", std::ios::app);
    recordFile << keyword << " : " << localFilePath << std::endl;
    recordFile.close();
}

bool FTPClient::isDownloaded(const std::string& keyword) {

    return isKeywordMatched("downloaded_files.txt", keyword);
}

bool FTPClient::isKeywordMatched(const std::string& filePath, const std::string& keyword) {
    if (keyword.empty()) {
        return false;
    }

    std::ifstream file(filePath);
    if (!file) {
        std::cerr << "Failed to open local file: " << filePath << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find(keyword) != std::string::npos) {
            file.close();
            return true;
        }
    }

    file.close();
    return false;
}

bool FTPClient::fileExists(const std::string &filePath){
    std::ifstream file(filePath);
    return file.good();
}

void FTPClient::sanitizePath(std::string &path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    // 删除路径结尾的 /
    if (!path.empty() && path.back() == '/') {
        path.erase(path.length() - 1);
    }
}

// 将URL中空格替换为%20，解决包含空格无法传输问题
std::string FTPClient::replaceSpacesWithPercent20(const std::string &str)
{
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(' ', pos)) != std::string::npos) {
        result.replace(pos, 1, "%20");
        pos += 3; // 移过替换的字符
    }

    return result;
}

// 实现下载文件的函数
bool FTPClient::downloadFile(const std::string &remoteFilePath, const std::string &localFilePath, const std::string& keyword) {

    std::string sanitizedRemotePath = remoteFilePath;
    std::string sanitizedLocalPath = localFilePath;

    sanitizePath(sanitizedRemotePath);
    sanitizePath(sanitizedLocalPath);

    //  在目标文件开头插入 /
    if (!sanitizedRemotePath.empty() && sanitizedRemotePath[0] != '/') {
        sanitizedRemotePath.insert(0, "/");
    }

    // 判断是否已经下载文件
    if (isDownloaded(sanitizedLocalPath)) {
        return true;
    }


    CURL* curl_download = curl_easy_init();
    if (!curl_download) {
        return false;
    }

    curl_easy_setopt(curl_download, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curl_download, CURLOPT_PASSWORD, password_.c_str());

    // 将本地路径拆分为目录和文件名
    size_t separatorIndex = sanitizedLocalPath.find_last_of('/');
    std::string loacalDirectoryPath = sanitizedLocalPath.substr(0, separatorIndex);
    std::string loacalFileName = sanitizedLocalPath.substr(separatorIndex + 1);

    if (!createLocalFolder(loacalDirectoryPath)) {
        return false;
    }

    std::ofstream file;
    bool isResumeEnabled = resumeEnabled(curl_download, sanitizedRemotePath);
    if (fileExists(sanitizedLocalPath) && isResumeEnabled) {
        file.open(sanitizedLocalPath, std::ios::out | std::ios::in  | std::ios::binary); // 断点续传时以追加模式打开文件
    } else {
        file.open(sanitizedLocalPath, std::ios::out | std::ios::trunc | std::ios::binary); // 直接下载时重新创建文件
    }

    if (!file.is_open()) {
        std::cerr << "Failed to open local file: " << sanitizedLocalPath << std::endl;
        return false;
    }

    // 定位到文件末尾，准备追加数据
    file.seekp(0, std::ios::end);

    curl_easy_setopt(curl_download, CURLOPT_URL, ("ftp://" + host_ + replaceSpacesWithPercent20(sanitizedRemotePath)).c_str());
    curl_easy_setopt(curl_download, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);
    curl_easy_setopt(curl_download, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_download, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl_download, CURLOPT_RESUME_FROM, getLocalFileSize(sanitizedLocalPath));

    // 设置CURLOPT_NOPROGRESS为0，以启用进度回调函数
    // 设置CURLOPT_PROGRESSFUNCTION为progressCallback函数指针，用于获取上传进度
    FileTransferInfo fileInfo;
    fileInfo.filename = sanitizedRemotePath;
    fileInfo.transferType = Download;

    curl_easy_setopt(curl_download, CURLOPT_PROGRESSDATA, &fileInfo);
    curl_easy_setopt(curl_download,CURLOPT_PROGRESSFUNCTION , progressCallback);
    curl_easy_setopt(curl_download,CURLOPT_NOPROGRESS , 0L);

    CURLcode result = curl_easy_perform(curl_download);

    file.close();

    bool res = false;
    if (result == CURLE_OK) {
        std::cout << "File downloaded successfully!" << std::endl;
        if (enableDeleteAfterDownload_) {
            deleteRemoteFile(curl_download, sanitizedRemotePath);
        }
        recordDownloadedFile(sanitizedLocalPath, keyword);
        res = true;
    } else {
        std::cerr << "Failed to download file: " << sanitizedRemotePath << std::endl;
    }

    curl_easy_cleanup(curl_download);
    return res;
}

// 实现下载整个文件夹的函数-单线程
bool FTPClient::downloadFolder(const std::string& remoteFolderPath, const std::string& localFolderPath, const std::string& keyword) {

    std::string sanitizedRemotePath = remoteFolderPath;
    std::string sanitizedLocalPath = localFolderPath;

    sanitizePath(sanitizedRemotePath);
    sanitizePath(sanitizedLocalPath);

    if (!createLocalFolder(localFolderPath)) {
        return false;
    }

    std::vector<std::string> fileNames = listRemoteFiles(sanitizedRemotePath);
    for (const std::string& fileName : fileNames) {
        std::string remoteFilePath = sanitizedRemotePath + "/" + fileName;
        std::string localFilePath = sanitizedLocalPath + "/" + fileName;

        downloadFile(remoteFilePath, localFilePath, keyword);
    }

    return true;
}

// 实现并发下载文件夹的函数
bool FTPClient::concurrentDownloadFolder(const std::string& remoteFolderPath, const std::string& localFolderPath, const std::string& keyword) {

    std::string sanitizedRemotePath = remoteFolderPath;
    std::string sanitizedLocalPath = localFolderPath;

    sanitizePath(sanitizedRemotePath);
    sanitizePath(sanitizedLocalPath);

    std::vector<std::string> fileNames = listRemoteFiles(sanitizedRemotePath);
    std::vector<std::future<bool>> futures;
    for (const std::string& fileName : fileNames) {
        std::string remoteFilePath = sanitizedRemotePath + "/" + fileName;
        std::string localFilePath = sanitizedLocalPath + "/" + fileName;

        futures.emplace_back(std::async(std::launch::async, [=](){
            return downloadFile(remoteFilePath, localFilePath, keyword);
        }));
    }

    bool allSucceeded = true;
    for (auto& future : futures) {
        if(!future.get())
            allSucceeded = false;
    }

    return allSucceeded;
}

// 实现上传文件的函数
bool FTPClient::uploadFile(const std::string& localFilePath, const std::string& remoteFilePath) {

    std::string sanitizedRemotePath = remoteFilePath;
    std::string sanitizedLocalPath = localFilePath;

    sanitizePath(sanitizedRemotePath);
    sanitizePath(sanitizedLocalPath);

    // 在开头添加 /
    if (sanitizedRemotePath.empty() || sanitizedRemotePath[0] != '/') {
        sanitizedRemotePath.insert(0, "/");
    }

    std::ifstream file(sanitizedLocalPath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open local file: " << sanitizedLocalPath << std::endl;
        return false;
    }

    CURL* curlUpload = curl_easy_init();
    if (!curlUpload) {
        return false;
    }

    curl_easy_setopt(curlUpload, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curlUpload, CURLOPT_PASSWORD, password_.c_str());

    long remoteFileSize = getRemoteFileSize(curlUpload, sanitizedRemotePath);
    long localFileSize = getLocalFileSize(sanitizedLocalPath);

    if (localFileSize <= remoteFileSize) {
        std::cout << "Local file size is the same as remote file size. No need to upload." << std::endl;
        curl_easy_cleanup(curlUpload);
        return true;
    }

    // 设置偏移量，断点续传
    curl_easy_setopt(curlUpload, CURLOPT_RESUME_FROM, remoteFileSize);

    curl_easy_setopt(curlUpload, CURLOPT_URL, ("ftp://" + host_ + "/" + replaceSpacesWithPercent20(sanitizedRemotePath)).c_str());
    curl_easy_setopt(curlUpload, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curlUpload, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);      // 如果不存在则自动创建该目录
    curl_easy_setopt(curlUpload, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(curlUpload, CURLOPT_READDATA, &file);

    // 设置CURLOPT_NOPROGRESS为0，以启用进度回调函数
    // 设置CURLOPT_PROGRESSFUNCTION为progressCallback函数指针，用于获取上传进度
    FileTransferInfo fileInfo;
    fileInfo.filename = sanitizedLocalPath;
    fileInfo.totalSize = localFileSize;
    fileInfo.transferType = Upload;

    curl_easy_setopt(curlUpload, CURLOPT_PROGRESSDATA, &fileInfo);
    curl_easy_setopt(curlUpload,CURLOPT_PROGRESSFUNCTION , progressCallback);
    curl_easy_setopt(curlUpload,CURLOPT_NOPROGRESS , 0L);

    CURLcode result = curl_easy_perform(curlUpload);

    file.close();

    bool res = false;
    if (result == CURLE_OK) {
        std::cout << "File uploaded successfully!" << std::endl;
        res = true;
    } else {
        std::cerr << "Failed to upload file: " << sanitizedLocalPath << std::endl;
    }

    curl_easy_cleanup(curlUpload);
    return res;
}

bool FTPClient::uploadFolder(const std::string &localFolderPath, const std::string &remoteFolderPath)
{
    std::string sanitizedRemotePath = remoteFolderPath;
    std::string sanitizedLocalPath = localFolderPath;

    // 标准化文件路径
    sanitizePath(sanitizedRemotePath);
    sanitizePath(sanitizedLocalPath);

    std::vector<std::string> fileNames = listLocalFiles(sanitizedLocalPath);
    for (std::string fileName : fileNames) {
        sanitizePath(fileName);
        std::string localFilePath = fileName;
        std::string remoteFilePath = fileName;
        remoteFilePath = sanitizedRemotePath + remoteFilePath.replace(0, sanitizedLocalPath.length(), "");

        uploadFile(localFilePath, remoteFilePath);
    }

    return true;
}

bool FTPClient::concurrentUploadFolder(const std::string &localFolderPath, const std::string &remoteFolderPath)
{
    std::string sanitizedRemotePath = remoteFolderPath;
    std::string sanitizedLocalPath = localFolderPath;

    // 标准化文件路径
    sanitizePath(sanitizedRemotePath);
    sanitizePath(sanitizedLocalPath);

    std::vector<std::future<bool>> futures;
    std::vector<std::string> fileNames = listLocalFiles(sanitizedLocalPath);
    for (std::string fileName : fileNames) {
        sanitizePath(fileName);
        std::string localFilePath = fileName;
        std::string remoteFilePath = fileName;
        remoteFilePath = sanitizedRemotePath + remoteFilePath.replace(0, sanitizedLocalPath.length(), "");

        futures.emplace_back(std::async(std::launch::async, [=](){
            return uploadFile(localFilePath, remoteFilePath);
        }));
    }

    bool allSucceeded = true;
    for (auto& future : futures) {
        if(!future.get())
            allSucceeded = false;
    }

    return allSucceeded;
}

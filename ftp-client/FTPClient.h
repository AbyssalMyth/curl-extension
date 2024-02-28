#ifndef FTPCLIENT_H
#define FTPCLIENT_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <vector>
#include <mutex>
#include <map>
#include <condition_variable>

#include <curl/curl.h>

/**
 * @brief FTP客户端类
 */
class FTPClient
{
public:

    enum FTP_Code {
        FTP_FAILED = 0,
        FTP_OK = 1,
        INITIALIZATION_FAILED,              /* 1 - 初始化网络失败 */
        LOCAL_FILE_OPEN_FAILED,             /*  - 本地文件打开失败 */
        CREATE_FOLDER_FAILED,               /*  - 创建文件夹失败 */
        FILENAME_CONTAINS_KEYWORD,          /*  - 文件名中包含关键词，忽略下载此文件 */
        REMOTE_AND_LOCAL_FILE_IDENTICAL,    /* - 远程与本地文件大小一致，不传输。仅上传 */
        REMOTE_FILE_DELE_FAILED             /* - 删除远端文件失败,文件已下载成功 */
    };

    struct FTPFileInfo {
        std::string permissions;    // 文件权限
        std::string userGroup;      // 用户组
        std::string userName;       // 用户名
        int fileSize;               // 文件大小
        std::string date;           // 日期
        std::string fileName;       // 文件名
        std::string path;           // 路径
    };

    enum TransferType {
        Upload,
        Download
    };

    struct FileTransferInfo {
        std::string filename;      // 文件名
        long totalSize;            // 文件总大小
        long transferredSize;      // 已传输大小
        long remainingSize;        // 剩余大小
        double transferProgress;   // 传输进度
        TransferType transferType; // 传输类型（上传或下载）
    };

public:
    /**
     * @brief 构造函数
     * @param host FTP服务器主机名:端口。例如：127.0.0.1:21
     * @param username FTP登录用户名
     * @param password FTP登录密码
     */
    FTPClient(const std::string& host, const std::string& username, const std::string& password);

    /**
     * @brief 析构函数
     */
    ~FTPClient();

    /**
     * @brief 判断FTP服务器是否支持断点续传
     * @param curl CURL对象
     * @param remoteFilePath 远程文件路径
     * @return 如果支持断点续传，则返回true，否则返回false
     */
    bool resumeEnabled(CURL *curl, const std::string& remoteFilePath);

    /**
     * @brief 获取本地文件的大小，用于断点续传
     * @param localFilePath 本地文件路径
     * @return 返回文件大小
     */
    off_t getLocalFileSize(const std::string& localFilePath);

    /**
     * @brief 获取远程文件的大小，用于断点续传
     * @param curl CURL对象
     * @param remoteFilePath 远程文件路径
     * @return 返回文件大小
     */
    off_t getRemoteFileSize(CURL* curl, const std::string& remoteFilePath);

    /**
     * @brief 删除远程FTP服务器上的文件
     * @param curl CURL对象
     * @param remoteFilePath 远程文件路径
     */
    bool deleteRemoteFile(CURL* curl, const std::string& remoteFilePath);

    /**
     * @brief 创建本地文件夹
     * @param localFolderPath 本地文件夹路径
     * @return 如果创建成功，则返回true，否则返回false
     */
    bool createLocalFolder(const std::string& localFolderPath);

    /**
     * @brief 创建远程文件夹
     * @param remoteDirectoryPath 远程文件夹路径
     * @return 如果创建成功，则返回true，否则返回false
     */
    bool createRemoteDirectory(const std::string& remoteDirectoryPath);

    /**
     * @brief 列出远程FTP服务器上指定文件夹下的文件列表
     * @param remoteFolderPath 远程文件夹路径
     * @return 返回文件列表
     */
    std::vector<FTPFileInfo> listRemoteFiles(const std::string& remoteFolderPath);

    /**
     * @brief 列出本地指定文件夹下的文件列表
     * @param localFolderPath 远程文件夹路径
     * @return 返回文件列表
     */
    std::vector<std::string> listLocalFiles(const std::string& localFolderPath);

    /**
     * @brief 获取传输正在传输文件信息
     * @return 传输文件信息
     */
    std::map<int, FileTransferInfo>* getFileTransferInfoAddr();

    /**
     * @brief 下载文件到本地
     * @param remoteFilePath 远程文件路径
     * @param localFilePath 本地文件路径
     * @param filterKeywords 过滤条件列表，当下载文件名包含关键词时不下载
     * @return 返回状态号
     */
    FTP_Code downloadFile(const std::string &remoteFilePath, const std::string &localFilePath, const std::vector<std::string>& filterKeywords);

    /**
     * @brief 下载整个FTP服务器文件夹到本地
     * @param remoteFolderPath 远程文件夹路径
     * @param localFolderPath 本地文件夹路径
     * @param filterKeywords 过滤条件列表，当下载文件名包含关键词时不下载
     * @return 下载成功则返回true，否则返回false
     */
    bool downloadFolder(const std::string& remoteFolderPath, const std::string& localFolderPath, std::vector<std::string>& filterKeywords);

    /**
     * @brief 并发下载整个FTP服务器文件夹到本地
     * @param remoteFolderPath 远程文件夹路径
     * @param localFolderPath 本地文件夹路径
     * @param filterKeywords 过滤条件列表，当下载文件名包含关键词时不下载
     * @return 下载成功则返回true，否则返回false
     */
    bool concurrentDownloadFolder(const std::string& remoteFolderPath, const std::string& localFolderPath, const std::vector<std::string>& filterKeywords);

    /**
     * @brief 上传文件到FTP服务器
     * @param localFilePath 本地文件路径
     * @param remoteFilePath 远程文件路径
     * @return 返回状态号
     */
    FTP_Code uploadFile(const std::string& localFilePath, const std::string& remoteFilePath);

    /**
     * @brief 下载整个FTP服务器文件夹到本地
     * @param localFolderPath 本地文件夹路径
     * @param remoteFolderPath 远程文件夹路径
     * @return 上传成功则返回true，否则返回false
     */
    bool uploadFolder(const std::string& localFolderPath, const std::string& remoteFolderPath);

    /**
     * @brief 并发下载整个FTP服务器文件夹到本地
     * @param localFolderPath 本地文件夹路径
     * @param remoteFolderPath 远程文件夹路径
     * @return 上传成功则返回true，否则返回false
     */
    bool concurrentUploadFolder(const std::string& localFolderPath, const std::string& remoteFolderPath);


    bool enableDeleteAfterDownload_;

private:
    /**
     * @brief 写回调函数，用于将文件内容写入本地文件
     * @param contents 文件内容
     * @param size 文件块大小
     * @param nmemb 文件块数量
     * @param file 本地文件流
     * @return 返回写入的字节数
     */
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::ofstream* file=NULL);

    /**
     * @brief 读回调函数，用于从本地文件读取内容
     * @param buffer 缓冲区
     * @param size 文件块大小
     * @param nmemb 文件块数量
     * @param file 本地文件流
     * @return 返回读取的字节数
     */
    static size_t readCallback(void* buffer, size_t size, size_t nmemb, std::ifstream* file);

    /**
     * @brief 将返回内容写入字符串流的回调函数
     * @param contents 文件内容
     * @param size 文件块大小
     * @param nmemb 文件块数量
     * @param stream 字符串流
     * @return 返回写入的字节数
     */
    static size_t writeToStringStreamCallback(void* contents, size_t size, size_t nmemb, std::stringstream* stream);

    /**
     * @brief 返回数据大小回调函数
     * @param contents  文件内容
     * @param size      文件块大小
     * @param nmemb     文件块数量
     * @param data      指向要复制数据的目标内存区域的指针
     * @return          返回成功复制的总字节数
     */
    static size_t copyDataSizeCallback(void* contents, size_t size, size_t nmemb, void *data);

    /**
     * @brief 进度回调函数
     * @param clientp  用户定义的指针，可以在回调函数中访问
     * @param dltotal  下载的总字节数
     * @param dlnow    已经下载的字节数
     * @param ultotal  上传的总字节数,经测试无法直接传入该参数
     * @param ulnow    已经上传的字节数
     * @return         返回零以继续传输，非零值将中止传输
     */
    static int progressCallback(void *p, double dltotal, double dlnow, double ultotal, double ulnow);


    /**
     * @brief 记录已下载文件的路径
     * @param localFilePath 本地文件路径
     * @param keyword 匹配关键字，如果文件内容中包含该关键字，则记录为已下载
     */
    void recordDownloadedFile(const std::string& localFilePath);

    /**
     * @brief 判断文件是否已下载
     * @param keyword 记录为关键字
     * @return 如果已下载，则返回true，否则返回false
     */
    bool isDownloaded(const std::string& keyword);

    /**
     * @brief 判断文件内容是否匹配关键字
     * @param filePath 文件路径
     * @param keyword 匹配关键字
     * @return 如果文件内容中包含该关键字，则返回true，否则返回false
     */
    bool isKeywordMatched(const std::string& filePath, const std::string& keyword);

    /**
     * @brief 判断文件是否存在
     * @param filePath 文件路径
     * @return 如果文件存在，则返回true，否则返回false
     */
    bool fileExists(const std::string& filePath);

    /**
     * @brief 归一化，将路径中\\替换为/
     * @param path 路径
     */
    void sanitizePath(std::string& path);

    /**
     * @brief 将路径中的空格替换为 %20,解决空格无法传输问题
     * @param str 文件路径
     * @return
     */
    std::string replaceSpacesWithPercent20(const std::string& str);

private:
    std::string host_;  ///< FTP服务器主机名
    std::string username_;  ///< FTP登录用户名
    std::string password_;  ///< FTP登录密码


    std::mutex mutex;
    std::map<int, FileTransferInfo> taskProgress;
};

#endif  // FTPCLIENT_H

# FTP Client with libcurl

This FTP client is implemented using libcurl and provides functionality for downloading, uploading files, as well as uploading or downloading entire directories. The client supports concurrent operations for directory transfers, allowing multiple transfers to be performed simultaneously. Additionally, the client can automatically create directories on the server if they do not already exist, as well as on the local machine.

## Features

- Download files from the server
- Upload files to the server
- Download entire directories from the server
- Upload entire directories to the server
- Concurrent operations support for directory transfers
- Automatic creation of directories on the server and the local machine
- Option to delete files on the server after successful download

## Getting Started

1. Ensure that you have libcurl installed on your system. You can download the latest version of libcurl from the [official website](https://curl.se/download.html).

2. Add the provided `.h` and `.cpp` files to your project directory.

3. Include the FTP client header file in your code:
```cpp
#include "ftp_client.h"
```
4. Create an instance of the FTP client and start using its functions to interact with the FTP server:
```cpp

std::vector<std::string> filterKeywords;
filterKeywords << "temp";
// Create an FTP client object
FTPClient ftpClient("ftp.example.com", "username", "password");

// Set the option to enable deleting files after download
ftpClient.enableDeleteAfterDownload_=true;

// Download a file from the server with keyword matching
ftpClient.downloadFile("remote_file.txt", "local_file.txt", filterKeywords);

// Download an entire directory from the server with keyword matching
ftpClient.downloadFolder("remote_directory", "local_directory", filterKeywords);

// Concurrently download an entire directory from the server with keyword matching
ftpClient.concurrentDownloadFolder("remote_directory", "local_directory", filterKeywords);

// Upload a file to the server
ftpClient.uploadFile("local_file.txt", "remote_file.txt");

// Upload an entire directory to the server
ftpClient.uploadFolder("local_directory", "remote_directory");

// Concurrently upload an entire directory to the server
ftpClient.concurrentUploadFolder("local_directory", "remote_directory");
```
6. Customize and expand the usage of the FTP client functions based on your project requirements.

## Note
- Before using the FTP client functions, make sure to configure the FTP server address, username, and password accordingly.
- You may need to modify the FTP client header and implementation files to suit your specific project needs.

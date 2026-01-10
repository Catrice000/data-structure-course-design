#include "byow.h"
#include <limits.h>
#include <time.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#define PORT 8082
#define BUFFER_SIZE 8192
#define RESPONSE_BUFFER_SIZE 131072  // 128KB for large JSON responses
#define SAVE_FILE "save-file.txt"

// 全局世界实例
static World* currentWorld = NULL;

// ==================== HTTP响应函数 ====================

void sendHttpResponse(int clientSocket, int statusCode, const char* contentType, 
                      const char* body) {
    size_t bodyLen = strlen(body);
    size_t headerLen = 200;  // 足够大的头部空间
    size_t totalLen = headerLen + bodyLen;
    
    // 如果响应太大，使用动态分配
    char* response = NULL;
    if (totalLen > RESPONSE_BUFFER_SIZE) {
        response = (char*)malloc(totalLen + 1);
        if (!response) {
            // 如果分配失败，尝试发送错误响应
            const char* error = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            send(clientSocket, error, strlen(error), 0);
            return;
        }
    } else {
        response = (char*)malloc(RESPONSE_BUFFER_SIZE);
        if (!response) {
            const char* error = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            send(clientSocket, error, strlen(error), 0);
            return;
        }
    }
    
    // 构建HTTP头部
    int headerSize = snprintf(response, totalLen,
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n",
        statusCode, contentType, bodyLen);
    
    // 复制body
    memcpy(response + headerSize, body, bodyLen);
    
    // 发送响应
    size_t sent = 0;
    size_t toSend = headerSize + bodyLen;
    
    while (sent < toSend) {
        ssize_t n = send(clientSocket, response + sent, toSend - sent, 0);
        if (n < 0) {
            break;  // 发送失败
        }
        sent += n;
    }
    
    free(response);
}

void sendJsonResponse(int clientSocket, const char* json) {
    sendHttpResponse(clientSocket, 200, "application/json", json);
}

void sendErrorResponse(int clientSocket, const char* message) {
    char errorJson[512];
    snprintf(errorJson, sizeof(errorJson), "{\"error\":\"%s\"}", message);
    sendJsonResponse(clientSocket, errorJson);
}

// ==================== API处理函数 ====================

void handleGenerateWorld(int clientSocket, const char* queryString) {
    long seed = 0;
    int width = 80;
    int height = 50;
    
    // 解析查询参数
    if (queryString) {
        char* seedStr = strstr(queryString, "seed=");
        char* widthStr = strstr(queryString, "width=");
        char* heightStr = strstr(queryString, "height=");
        
        if (seedStr) {
            seed = strtol(seedStr + 5, NULL, 10);
        } else {
            seed = time(NULL);  // 使用当前时间作为种子
        }
        
        if (widthStr) {
            width = atoi(widthStr + 6);
            if (width < 20) width = 20;
            if (width > MAX_WORLD_WIDTH) width = MAX_WORLD_WIDTH;
        }
        
        if (heightStr) {
            height = atoi(heightStr + 7);
            if (height < 20) height = 20;
            if (height > MAX_WORLD_HEIGHT) height = MAX_WORLD_HEIGHT;
        }
    } else {
        seed = time(NULL);
    }
    
    // 销毁旧世界
    if (currentWorld) {
        destroyWorld(currentWorld);
    }
    
    // 生成新世界
    currentWorld = generateWorldFromSeed(seed, width, height);
    
    if (!currentWorld) {
        sendErrorResponse(clientSocket, "Failed to generate world");
        return;
    }
    
    // 返回世界JSON（使用更大的缓冲区）
    char jsonBuffer[131072];  // 128KB
    int result = getWorldJSON(currentWorld, jsonBuffer, sizeof(jsonBuffer));
    if (result != 0) {
        sendErrorResponse(clientSocket, "Failed to generate JSON");
        return;
    }
    sendJsonResponse(clientSocket, jsonBuffer);
}

void handleGetWorld(int clientSocket) {
    if (!currentWorld) {
        sendErrorResponse(clientSocket, "No world generated yet");
        return;
    }
    
    char jsonBuffer[131072];  // 128KB
    int result = getWorldJSON(currentWorld, jsonBuffer, sizeof(jsonBuffer));
    if (result != 0) {
        sendErrorResponse(clientSocket, "Failed to generate JSON");
        return;
    }
    sendJsonResponse(clientSocket, jsonBuffer);
}

void handleGetRooms(int clientSocket) {
    if (!currentWorld) {
        sendErrorResponse(clientSocket, "No world generated yet");
        return;
    }
    
    char jsonBuffer[8192];
    getRoomsJSON(currentWorld, jsonBuffer, sizeof(jsonBuffer));
    sendJsonResponse(clientSocket, jsonBuffer);
}

void handleGetCorridors(int clientSocket) {
    if (!currentWorld) {
        sendErrorResponse(clientSocket, "No world generated yet");
        return;
    }
    
    char jsonBuffer[8192];
    getCorridorsJSON(currentWorld, jsonBuffer, sizeof(jsonBuffer));
    sendJsonResponse(clientSocket, jsonBuffer);
}

void handleGetMap(int clientSocket) {
    if (!currentWorld) {
        sendErrorResponse(clientSocket, "No world generated yet");
        return;
    }
    
    char jsonBuffer[16384];
    getWorldMapJSON(currentWorld, jsonBuffer, sizeof(jsonBuffer));
    sendJsonResponse(clientSocket, jsonBuffer);
}

void handleFindPath(int clientSocket, const char* queryString) {
    if (!currentWorld) {
        sendErrorResponse(clientSocket, "No world generated yet");
        return;
    }
    
    int startRoomId = -1;
    int endRoomId = -1;
    
    // 解析查询参数
    if (queryString) {
        char* startStr = strstr(queryString, "start=");
        char* endStr = strstr(queryString, "end=");
        
        if (startStr) startRoomId = atoi(startStr + 6);
        if (endStr) endRoomId = atoi(endStr + 4);
    }
    
    if (startRoomId < 0 || endRoomId < 0 || 
        startRoomId >= currentWorld->roomCount || 
        endRoomId >= currentWorld->roomCount) {
        sendErrorResponse(clientSocket, "Invalid room IDs");
        return;
    }
    
    int path[MAX_ROOMS];
    int pathLength = findShortestPath(currentWorld, startRoomId, endRoomId, path, MAX_ROOMS);
    
    if (pathLength < 0) {
        sendErrorResponse(clientSocket, "Path not found");
        return;
    }
    
    // 构建JSON响应
    char jsonBuffer[2048];
    int pos = snprintf(jsonBuffer, sizeof(jsonBuffer), "{\"path\":[");
    for (int i = 0; i < pathLength; i++) {
        if (i > 0) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
        pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "%d", path[i]);
    }
    pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "],\"length\":%d}", pathLength);
    sendJsonResponse(clientSocket, jsonBuffer);
}

// 解析POST请求体
void parsePostBody(const char* request, char* body, size_t bodySize) {
    const char* bodyStart = strstr(request, "\r\n\r\n");
    if (bodyStart) {
        bodyStart += 4;  // 跳过 "\r\n\r\n"
        strncpy(body, bodyStart, bodySize - 1);
        body[bodySize - 1] = '\0';
    } else {
        body[0] = '\0';
    }
}

// 保存游戏
void handleSaveGame(int clientSocket, const char* requestBody) {
    FILE* file = fopen(SAVE_FILE, "w");
    if (!file) {
        sendErrorResponse(clientSocket, "Failed to open save file");
        return;
    }
    
    fprintf(file, "%s", requestBody);
    fclose(file);
    
    sendJsonResponse(clientSocket, "{\"status\":\"saved\"}");
}

// 加载游戏
void handleLoadGame(int clientSocket) {
    FILE* file = fopen(SAVE_FILE, "r");
    if (!file) {
        sendErrorResponse(clientSocket, "No save file found");
        return;
    }
    
    char buffer[4096];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[len] = '\0';
    fclose(file);
    
    sendJsonResponse(clientSocket, buffer);
}

// ==================== HTTP请求解析 ====================

void handleHttpRequest(int clientSocket, const char* request) {
    char method[16];
    char path[256];
    char queryString[256] = {0};
    
    // 解析请求行
    sscanf(request, "%s %s", method, path);
    
    // 提取查询字符串
    char* queryStart = strchr(path, '?');
    if (queryStart) {
        *queryStart = '\0';
        strncpy(queryString, queryStart + 1, sizeof(queryString) - 1);
    }
    
    // 处理OPTIONS请求（CORS预检）
    if (strcmp(method, "OPTIONS") == 0) {
        sendHttpResponse(clientSocket, 200, "text/plain", "");
        return;
    }
    
    // 路由处理
    if (strcmp(path, "/api/generate") == 0 || strcmp(path, "/api/generateWorld") == 0) {
        if (strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0) {
            handleGenerateWorld(clientSocket, queryString);
        } else {
            sendErrorResponse(clientSocket, "Method not allowed");
        }
    } else if (strcmp(path, "/api/world") == 0 || strcmp(path, "/api/getWorld") == 0) {
        if (strcmp(method, "GET") == 0) {
            handleGetWorld(clientSocket);
        } else {
            sendErrorResponse(clientSocket, "Method not allowed");
        }
    } else if (strcmp(path, "/api/rooms") == 0) {
        if (strcmp(method, "GET") == 0) {
            handleGetRooms(clientSocket);
        } else {
            sendErrorResponse(clientSocket, "Method not allowed");
        }
    } else if (strcmp(path, "/api/corridors") == 0) {
        if (strcmp(method, "GET") == 0) {
            handleGetCorridors(clientSocket);
        } else {
            sendErrorResponse(clientSocket, "Method not allowed");
        }
    } else if (strcmp(path, "/api/map") == 0) {
        if (strcmp(method, "GET") == 0) {
            handleGetMap(clientSocket);
        } else {
            sendErrorResponse(clientSocket, "Method not allowed");
        }
    } else if (strcmp(path, "/api/path") == 0 || strcmp(path, "/api/findPath") == 0) {
        if (strcmp(method, "GET") == 0) {
            handleFindPath(clientSocket, queryString);
        } else {
            sendErrorResponse(clientSocket, "Method not allowed");
        }
    } else if (strcmp(path, "/api/save") == 0) {
        if (strcmp(method, "POST") == 0) {
            char body[4096];
            parsePostBody(request, body, sizeof(body));
            handleSaveGame(clientSocket, body);
        } else {
            sendErrorResponse(clientSocket, "Method not allowed");
        }
    } else if (strcmp(path, "/api/load") == 0) {
        if (strcmp(method, "GET") == 0) {
            handleLoadGame(clientSocket);
        } else {
            sendErrorResponse(clientSocket, "Method not allowed");
        }
    } else {
        sendErrorResponse(clientSocket, "Not found");
    }
}

// ==================== 主函数 ====================

int main(void) {
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("WSAStartup failed\n");
            return 1;
        }
    #endif
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // 设置socket选项（允许地址重用）
    int opt = 1;
    #ifdef _WIN32
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    #else
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    #endif
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(serverSocket);
        return 1;
    }
    
    if (listen(serverSocket, 5) < 0) {
        perror("Listen failed");
        close(serverSocket);
        return 1;
    }
    
    printf("BYOW Server running on port %d\n", PORT);
    printf("Open http://localhost:%d in your browser\n", PORT);
    
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }
        
        char buffer[BUFFER_SIZE];
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            handleHttpRequest(clientSocket, buffer);
        }
        
        close(clientSocket);
    }
    
    close(serverSocket);
    
    #ifdef _WIN32
        WSACleanup();
    #endif
    
    // 清理
    if (currentWorld) {
        destroyWorld(currentWorld);
    }
    
    return 0;
}


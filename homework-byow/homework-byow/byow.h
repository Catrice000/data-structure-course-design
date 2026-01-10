#ifndef BYOW_H
#define BYOW_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// ==================== 常量定义 ====================

#define MAX_WORLD_WIDTH 100
#define MAX_WORLD_HEIGHT 100
#define MAX_ROOMS 50
#define MAX_CORRIDORS 100
#define MAX_PATH_LEN 256

// 瓦片类型
#define TILE_FLOOR 0
#define TILE_WALL 1
#define TILE_ROOM 2
#define TILE_CORRIDOR 3

// ==================== 数据结构定义 ====================

// 坐标点
typedef struct Point {
    int x;
    int y;
} Point;

// 房间结构
typedef struct Room {
    int id;              // 房间ID
    int x, y;           // 房间左上角坐标
    int width, height;  // 房间宽度和高度
    bool exists;        // 房间是否存在
} Room;

// 走廊结构
typedef struct Corridor {
    int id;             // 走廊ID
    Point start;        // 起点
    Point end;          // 终点
    bool isTurning;     // 是否是转弯走廊
} Corridor;

// 图的邻接表节点（房间连接）
typedef struct RoomConnection {
    int roomId;                    // 连接的房间ID
    struct RoomConnection* next;   // 下一个连接
} RoomConnection;

// 并查集结构
typedef struct DisjointSet {
    int parent[MAX_ROOMS];  // 父节点数组
    int rank[MAX_ROOMS];    // 秩数组（用于路径压缩）
    int count;              // 集合数量
} DisjointSet;

// 队列节点（用于BFS）
typedef struct QueueNode {
    int roomId;
    struct QueueNode* next;
} QueueNode;

// 队列结构
typedef struct Queue {
    QueueNode* front;
    QueueNode* rear;
    int size;
} Queue;

// 世界结构（核心数据结构）
typedef struct World {
    // 地图数据
    int tiles[MAX_WORLD_HEIGHT][MAX_WORLD_WIDTH];  // 瓦片地图
    
    // 房间和走廊
    Room rooms[MAX_ROOMS];           // 房间数组
    Corridor corridors[MAX_CORRIDORS]; // 走廊数组
    int roomCount;                    // 房间数量
    int corridorCount;                // 走廊数量
    
    // 图结构（邻接表）
    RoomConnection* connections[MAX_ROOMS];  // 每个房间的连接列表
    
    // 并查集（用于连通性检查）
    DisjointSet* disjointSet;
    
    // 世界属性
    int width, height;   // 世界尺寸
    long seed;           // 随机种子
    bool initialized;    // 是否已初始化
} World;

// ==================== 并查集操作接口 ====================

/**
 * 初始化并查集
 * @param ds 并查集指针
 * @param count 元素数量
 */
void initDisjointSet(DisjointSet* ds, int count);

/**
 * 查找元素所在的集合根节点
 * @param ds 并查集指针
 * @param x 元素
 * @return 根节点
 */
int findSet(DisjointSet* ds, int x);

/**
 * 合并两个集合
 * @param ds 并查集指针
 * @param x 元素1
 * @param y 元素2
 */
void unionSets(DisjointSet* ds, int x, int y);

/**
 * 检查两个元素是否在同一个集合
 * @param ds 并查集指针
 * @param x 元素1
 * @param y 元素2
 * @return true表示在同一个集合
 */
bool isConnected(DisjointSet* ds, int x, int y);

// ==================== 队列操作接口 ====================

/**
 * 创建队列
 * @return 队列指针
 */
Queue* createQueue(void);

/**
 * 销毁队列
 * @param queue 队列指针
 */
void destroyQueue(Queue* queue);

/**
 * 入队
 * @param queue 队列指针
 * @param roomId 房间ID
 */
void enqueue(Queue* queue, int roomId);

/**
 * 出队
 * @param queue 队列指针
 * @return 房间ID，队列为空返回-1
 */
int dequeue(Queue* queue);

/**
 * 检查队列是否为空
 * @param queue 队列指针
 * @return true表示为空
 */
bool isEmptyQueue(Queue* queue);

// ==================== 世界生成接口 ====================

/**
 * 创建新世界
 * @param seed 随机种子
 * @param width 世界宽度
 * @param height 世界高度
 * @return 世界指针，失败返回NULL
 */
World* createWorld(long seed, int width, int height);

/**
 * 销毁世界
 * @param world 世界指针
 */
void destroyWorld(World* world);

/**
 * 生成随机房间
 * @param world 世界指针
 * @param minSize 最小房间尺寸
 * @param maxSize 最大房间尺寸
 * @param maxRooms 最大房间数量
 * @return 成功返回0，失败返回-1
 */
int generateRooms(World* world, int minSize, int maxSize, int maxRooms);

/**
 * 生成走廊连接房间
 * @param world 世界指针
 * @return 成功返回0，失败返回-1
 */
int generateCorridors(World* world);

/**
 * 使用最小生成树连接所有房间
 * @param world 世界指针
 * @return 成功返回0，失败返回-1
 */
int connectRoomsWithMST(World* world);

/**
 * 检查世界是否连通
 * @param world 世界指针
 * @return true表示连通
 */
bool isWorldConnected(World* world);

/**
 * 根据种子生成完整世界
 * @param seed 随机种子
 * @param width 世界宽度
 * @param height 世界高度
 * @return 世界指针，失败返回NULL
 */
World* generateWorldFromSeed(long seed, int width, int height);

// ==================== 世界查询接口 ====================

/**
 * 获取指定位置的瓦片类型
 * @param world 世界指针
 * @param x X坐标
 * @param y Y坐标
 * @return 瓦片类型
 */
int getTile(World* world, int x, int y);

/**
 * 设置指定位置的瓦片类型
 * @param world 世界指针
 * @param x X坐标
 * @param y Y坐标
 * @param tileType 瓦片类型
 */
void setTile(World* world, int x, int y, int tileType);

/**
 * 获取房间列表（JSON格式）
 * @param world 世界指针
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 * @return 成功返回0，失败返回-1
 */
int getRoomsJSON(World* world, char* buffer, size_t bufferSize);

/**
 * 获取走廊列表（JSON格式）
 * @param world 世界指针
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 * @return 成功返回0，失败返回-1
 */
int getCorridorsJSON(World* world, char* buffer, size_t bufferSize);

/**
 * 获取世界地图数据（JSON格式）
 * @param world 世界指针
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 * @return 成功返回0，失败返回-1
 */
int getWorldMapJSON(World* world, char* buffer, size_t bufferSize);

/**
 * 获取世界完整信息（JSON格式）
 * @param world 世界指针
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 * @return 成功返回0，失败返回-1
 */
int getWorldJSON(World* world, char* buffer, size_t bufferSize);

// ==================== 路径查找接口 ====================

/**
 * 使用BFS查找两个房间之间的最短路径
 * @param world 世界指针
 * @param startRoomId 起始房间ID
 * @param endRoomId 目标房间ID
 * @param path 输出路径数组
 * @param maxPathLength 最大路径长度
 * @return 路径长度，失败返回-1
 */
int findShortestPath(World* world, int startRoomId, int endRoomId, 
                     int* path, int maxPathLength);

// ==================== 工具函数接口 ====================

/**
 * 检查两个房间是否重叠
 * @param room1 房间1
 * @param room2 房间2
 * @return true表示重叠
 */
bool roomsOverlap(Room* room1, Room* room2);

/**
 * 计算两个房间之间的距离
 * @param room1 房间1
 * @param room2 房间2
 * @return 距离
 */
int roomDistance(Room* room1, Room* room2);

/**
 * 检查坐标是否在世界范围内
 * @param world 世界指针
 * @param x X坐标
 * @param y Y坐标
 * @return true表示在范围内
 */
bool isValidPosition(World* world, int x, int y);

/**
 * 获取随机数（基于世界种子）
 * @param world 世界指针
 * @param min 最小值
 * @param max 最大值
 * @return 随机数
 */
int getRandom(World* world, int min, int max);

#endif // BYOW_H



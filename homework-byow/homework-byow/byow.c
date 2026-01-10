#include "byow.h"
#include <time.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

// ==================== 随机数生成器（基于种子）====================

static long currentSeed = 0;

// 线性同余生成器（LCG）
static long lcgNext(void) {
    currentSeed = (currentSeed * 1103515245 + 12345) & 0x7fffffff;
    return currentSeed;
}

static void setSeed(long seed) {
    currentSeed = seed;
}

// ==================== 内部工具：安全缓冲区追加 ====================
static int bufAppend(char* buffer, size_t bufferSize, int* pos, const char* fmt, ...) {
    if (!buffer || bufferSize == 0 || !pos || *pos < 0) return -1;
    if ((size_t)*pos >= bufferSize) return -1;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buffer + *pos, bufferSize - (size_t)*pos, fmt, ap);
    va_end(ap);

    if (n < 0) return -1;
    if ((size_t)n >= bufferSize - (size_t)*pos) return -1; // 溢出（被截断）
    *pos += n;
    return 0;
}

static void clearAllConnections(World* world) {
    if (!world) return;
    for (int i = 0; i < MAX_ROOMS; i++) {
        RoomConnection* conn = world->connections[i];
        while (conn) {
            RoomConnection* next = conn->next;
            free(conn);
            conn = next;
        }
        world->connections[i] = NULL;
    }
}

// ==================== 并查集操作实现 ====================
// 并查集：用于检查房间之间的连通性，支持路径压缩和按秩合并优化

void initDisjointSet(DisjointSet* ds, int count) {
    if (!ds) return;

    if (count < 0) count = 0;
    if (count > MAX_ROOMS) count = MAX_ROOMS;

    // 始终初始化整个数组，避免count < MAX_ROOMS时findSet访问未初始化槽位
    for (int i = 0; i < MAX_ROOMS; i++) {
        ds->parent[i] = i;
        ds->rank[i] = 0;
    }
    ds->count = count;
}

int findSet(DisjointSet* ds, int x) {
    if (!ds) return -1;
    if (x < 0 || x >= MAX_ROOMS) return -1;

    // 路径压缩
    if (ds->parent[x] != x) {
        ds->parent[x] = findSet(ds, ds->parent[x]);
    }
    return ds->parent[x];
}

void unionSets(DisjointSet* ds, int x, int y) {
    if (!ds) return;

    int rootX = findSet(ds, x);
    int rootY = findSet(ds, y);

    if (rootX < 0 || rootY < 0) return;
    if (rootX == rootY) return;

    // 按秩合并
    if (ds->rank[rootX] < ds->rank[rootY]) {
        ds->parent[rootX] = rootY;
    } else if (ds->rank[rootX] > ds->rank[rootY]) {
        ds->parent[rootY] = rootX;
    } else {
        ds->parent[rootY] = rootX;
        ds->rank[rootX]++;
    }

    ds->count--;
}

bool isConnected(DisjointSet* ds, int x, int y) {
    if (!ds) return false;
    int rx = findSet(ds, x);
    int ry = findSet(ds, y);
    if (rx < 0 || ry < 0) return false;
    return rx == ry;
}

// ==================== 队列操作实现 ====================
// 队列：用于BFS路径查找算法，使用链表实现

Queue* createQueue(void) {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (!q) return NULL;

    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
    return q;
}

void destroyQueue(Queue* queue) {
    if (!queue) return;

    while (!isEmptyQueue(queue)) {
        (void)dequeue(queue);
    }
    free(queue);
}

void enqueue(Queue* queue, int roomId) {
    if (!queue) return;

    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!node) return;

    node->roomId = roomId;
    node->next = NULL;

    if (queue->rear == NULL) {
        // 空队列
        queue->front = node;
        queue->rear = node;
    } else {
        queue->rear->next = node;
        queue->rear = node;
    }

    queue->size++;
}

int dequeue(Queue* queue) {
    if (!queue || queue->front == NULL) return -1;

    QueueNode* oldFront = queue->front;
    int roomId = oldFront->roomId;

    queue->front = oldFront->next;
    if (queue->front == NULL) {
        // 出队后变空
        queue->rear = NULL;
    }

    free(oldFront);
    queue->size--;
    return roomId;
}

bool isEmptyQueue(Queue* queue) {
    return (queue == NULL || queue->front == NULL);
}

// ==================== 工具函数实现 ====================

bool roomsOverlap(Room* room1, Room* room2) {
    return !(room1->x + room1->width < room2->x ||
             room2->x + room2->width < room1->x ||
             room1->y + room1->height < room2->y ||
             room2->y + room2->height < room1->y);
}

int roomDistance(Room* room1, Room* room2) {
    int centerX1 = room1->x + room1->width / 2;
    int centerY1 = room1->y + room1->height / 2;
    int centerX2 = room2->x + room2->width / 2;
    int centerY2 = room2->y + room2->height / 2;

    int dx = centerX1 - centerX2;
    int dy = centerY1 - centerY2;
    return (int)sqrt(dx * dx + dy * dy);
}

bool isValidPosition(World* world, int x, int y) {
    return x >= 0 && x < world->width && y >= 0 && y < world->height;
}

int getRandom(World* world, int min, int max) {
    (void)world;  // 参数未使用，但保留以保持接口一致性
    if (min >= max) return min;
    long r = lcgNext();
    return min + (int)(r % (max - min + 1));
}

// ==================== 世界创建和销毁 ====================

static void ensureAtLeastOneRoom(World* world) {
    if (!world) return;
    if (world->roomCount > 0) return;

    // 最小房间 3x3（与generateWorldFromSeed的minSize一致）
    const int w = 3, h = 3;

    // 若地图过小，则直接不处理（让上层保持失败语义）
    if (world->width < w + 2 || world->height < h + 2) return;

    int x = (world->width - w) / 2;
    int y = (world->height - h) / 2;
    if (x < 1) x = 1;
    if (y < 1) y = 1;
    if (x + w >= world->width - 1) x = world->width - w - 2;
    if (y + h >= world->height - 1) y = world->height - h - 2;

    Room r;
    r.id = 0;
    r.x = x;
    r.y = y;
    r.width = w;
    r.height = h;
    r.exists = true;

    world->rooms[0] = r;
    world->roomCount = 1;

    for (int ry = y; ry < y + h; ry++) {
        for (int rx = x; rx < x + w; rx++) {
            if (isValidPosition(world, rx, ry)) {
                world->tiles[ry][rx] = TILE_ROOM;
            }
        }
    }
}

World* createWorld(long seed, int width, int height) {
    // 过小尺寸：连最小房间+边界都放不下，直接失败更合理
    // （min room 3x3，四周留1格墙 => 至少 5x5）
    if (width < 5 || height < 5) return NULL;

    // 超限：不直接失败，夹紧到最大值，避免前端输入导致“生成世界失败”
    if (width > MAX_WORLD_WIDTH) width = MAX_WORLD_WIDTH;
    if (height > MAX_WORLD_HEIGHT) height = MAX_WORLD_HEIGHT;

    World* world = (World*)malloc(sizeof(World));
    if (!world) return NULL;

    // 设置种子
    setSeed(seed);
    world->seed = seed;
    world->width = width;
    world->height = height;
    world->roomCount = 0;
    world->corridorCount = 0;
    world->initialized = false;

    // 初始化地图为墙壁
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            world->tiles[y][x] = TILE_WALL;
        }
    }

    // 初始化房间数组
    for (int i = 0; i < MAX_ROOMS; i++) {
        world->rooms[i].exists = false;
        world->connections[i] = NULL;
    }

    // 初始化并查集
    world->disjointSet = (DisjointSet*)malloc(sizeof(DisjointSet));
    if (!world->disjointSet) {
        free(world);
        return NULL;
    }
    initDisjointSet(world->disjointSet, MAX_ROOMS);

    return world;
}

void destroyWorld(World* world) {
    if (!world) return;

    // 释放所有连接
    for (int i = 0; i < MAX_ROOMS; i++) {
        RoomConnection* conn = world->connections[i];
        while (conn) {
            RoomConnection* next = conn->next;
            free(conn);
            conn = next;
        }
    }

    // 释放并查集
    if (world->disjointSet) {
        free(world->disjointSet);
    }

    free(world);
}

// ==================== 房间生成实现 ====================

int generateRooms(World* world, int minSize, int maxSize, int maxRooms) {
    int attempts = 0;
    int maxAttempts = maxRooms * 5;  // 尝试次数

    while (world->roomCount < maxRooms && attempts < maxAttempts) {
        attempts++;

        // 生成随机房间尺寸和位置
        int w = getRandom(world, minSize, maxSize);
        int h = getRandom(world, minSize, maxSize);
        int x = getRandom(world, 1, world->width - w - 1);
        int y = getRandom(world, 1, world->height - h - 1);

        Room newRoom;
        newRoom.id = world->roomCount;
        newRoom.x = x;
        newRoom.y = y;
        newRoom.width = w;
        newRoom.height = h;
        newRoom.exists = true;

        // 检查是否与现有房间重叠
        bool overlap = false;
        for (int i = 0; i < world->roomCount; i++) {
            if (world->rooms[i].exists && roomsOverlap(&newRoom, &world->rooms[i])) {
                overlap = true;
                break;
            }
        }

        if (!overlap) {
            // 添加房间
            world->rooms[world->roomCount] = newRoom;

            // 在地图上绘制房间
            for (int ry = y; ry < y + h; ry++) {
                for (int rx = x; rx < x + w; rx++) {
                    if (isValidPosition(world, rx, ry)) {
                        world->tiles[ry][rx] = TILE_ROOM;
                    }
                }
            }

            world->roomCount++;
        }
    }

    return 0;
}

// ==================== 走廊生成实现 ====================

void drawCorridor(World* world, Point start, Point end) {
    // 绘制L型走廊
    Point current = start;

    // 先水平移动
    int stepX = (end.x > current.x) ? 1 : -1;
    while (current.x != end.x) {
        if (isValidPosition(world, current.x, current.y)) {
            if (world->tiles[current.y][current.x] == TILE_WALL) {
                world->tiles[current.y][current.x] = TILE_CORRIDOR;
            }
        }
        current.x += stepX;
    }

    // 再垂直移动
    int stepY = (end.y > current.y) ? 1 : -1;
    while (current.y != end.y) {
        if (isValidPosition(world, current.x, current.y)) {
            if (world->tiles[current.y][current.x] == TILE_WALL) {
                world->tiles[current.y][current.x] = TILE_CORRIDOR;
            }
        }
        current.y += stepY;
    }

    // 确保终点也是走廊
    if (isValidPosition(world, end.x, end.y)) {
        if (world->tiles[end.y][end.x] == TILE_WALL) {
            world->tiles[end.y][end.x] = TILE_CORRIDOR;
        }
    }
}

int generateCorridors(World* world) {
    if (world->roomCount < 2) return -1;

    world->corridorCount = 0;

    // 为每对相邻房间生成走廊
    for (int i = 0; i < world->roomCount - 1; i++) {
        if (!world->rooms[i].exists) continue;

        Room* room1 = &world->rooms[i];
        int centerX1 = room1->x + room1->width / 2;
        int centerY1 = room1->y + room1->height / 2;

        // 找到最近的房间
        int minDist = INT_MAX;
        int nearestRoomId = -1;

        for (int j = i + 1; j < world->roomCount; j++) {
            if (!world->rooms[j].exists) continue;

            int dist = roomDistance(room1, &world->rooms[j]);
            if (dist < minDist) {
                minDist = dist;
                nearestRoomId = j;
            }
        }

        if (nearestRoomId != -1) {
            Room* room2 = &world->rooms[nearestRoomId];
            int centerX2 = room2->x + room2->width / 2;
            int centerY2 = room2->y + room2->height / 2;

            Point start = {centerX1, centerY1};
            Point end = {centerX2, centerY2};

            // 创建走廊
            Corridor corridor;
            corridor.id = world->corridorCount;
            corridor.start = start;
            corridor.end = end;
            corridor.isTurning = (start.x != end.x && start.y != end.y);

            world->corridors[world->corridorCount] = corridor;
            world->corridorCount++;

            // 绘制走廊
            drawCorridor(world, start, end);

            // 添加到图的连接中
            RoomConnection* conn1 = (RoomConnection*)malloc(sizeof(RoomConnection));
            conn1->roomId = nearestRoomId;
            conn1->next = world->connections[i];
            world->connections[i] = conn1;

            RoomConnection* conn2 = (RoomConnection*)malloc(sizeof(RoomConnection));
            conn2->roomId = i;
            conn2->next = world->connections[nearestRoomId];
            world->connections[nearestRoomId] = conn2;

            // 合并并查集
            unionSets(world->disjointSet, i, nearestRoomId);
        }
    }

    return 0;
}

// ==================== 最小生成树连接 ====================

int connectRoomsWithMST(World* world) {
    if (!world) return -1;
    if (world->roomCount < 2) return -1;

    // 如果被重复调用，先清理旧邻接表，避免内存泄漏/重复边
    clearAllConnections(world);

    // 重新初始化并查集
    initDisjointSet(world->disjointSet, world->roomCount);

    // 计算所有房间对的距离
    typedef struct {
        int room1, room2;
        int distance;
    } Edge;

    Edge edges[MAX_ROOMS * MAX_ROOMS];
    int edgeCount = 0;

    for (int i = 0; i < world->roomCount; i++) {
        for (int j = i + 1; j < world->roomCount; j++) {
            if (world->rooms[i].exists && world->rooms[j].exists) {
                edges[edgeCount].room1 = i;
                edges[edgeCount].room2 = j;
                edges[edgeCount].distance = roomDistance(&world->rooms[i], &world->rooms[j]);
                edgeCount++;
            }
        }
    }

    // 简单排序（冒泡排序）
    for (int i = 0; i < edgeCount - 1; i++) {
        for (int j = 0; j < edgeCount - i - 1; j++) {
            if (edges[j].distance > edges[j + 1].distance) {
                Edge temp = edges[j];
                edges[j] = edges[j + 1];
                edges[j + 1] = temp;
            }
        }
    }

    // Kruskal算法：按距离从小到大连接
    int corridorIndex = 0;
    for (int i = 0; i < edgeCount; i++) {
        int room1 = edges[i].room1;
        int room2 = edges[i].room2;

        if (!isConnected(world->disjointSet, room1, room2)) {
            // 连接这两个房间
            Room* r1 = &world->rooms[room1];
            Room* r2 = &world->rooms[room2];

            int centerX1 = r1->x + r1->width / 2;
            int centerY1 = r1->y + r1->height / 2;
            int centerX2 = r2->x + r2->width / 2;
            int centerY2 = r2->y + r2->height / 2;

            Point start = {centerX1, centerY1};
            Point end = {centerX2, centerY2};

            // 创建走廊
            if (corridorIndex < MAX_CORRIDORS) {
                Corridor corridor;
                corridor.id = corridorIndex;
                corridor.start = start;
                corridor.end = end;
                corridor.isTurning = (start.x != end.x && start.y != end.y);

                world->corridors[corridorIndex] = corridor;
                corridorIndex++;

                // 绘制走廊
                drawCorridor(world, start, end);

                // 添加到图的连接中
                RoomConnection* conn1 = (RoomConnection*)malloc(sizeof(RoomConnection));
                conn1->roomId = room2;
                conn1->next = world->connections[room1];
                world->connections[room1] = conn1;

                RoomConnection* conn2 = (RoomConnection*)malloc(sizeof(RoomConnection));
                conn2->roomId = room1;
                conn2->next = world->connections[room2];
                world->connections[room2] = conn2;

                // 合并并查集
                unionSets(world->disjointSet, room1, room2);
            }
        }
    }

    world->corridorCount = corridorIndex;
    return 0;
}

// ==================== 世界生成主函数 ====================

World* generateWorldFromSeed(long seed, int width, int height) {
    World* world = createWorld(seed, width, height);
    if (!world) return NULL;

    // 生成房间（更小的房间，更多数量，更像地牢风格）
    // 房间尺寸：最小3x3，最大6x6，生成25个房间
    generateRooms(world, 3, 6, 25);

    // 若随机生成一个都没有，放置保底房间
    ensureAtLeastOneRoom(world);

    // 使用MST连接所有房间（房间数<2时函数会返回-1，这里保持原逻辑）
    connectRoomsWithMST(world);

    world->initialized = true;
    return world;
}

// ==================== 连通性检查 ====================

bool isWorldConnected(World* world) {
    if (world->roomCount == 0) return true;
    if (world->roomCount == 1) return true;

    // 检查所有房间是否在同一个集合
    int root = findSet(world->disjointSet, 0);
    for (int i = 1; i < world->roomCount; i++) {
        if (world->rooms[i].exists && findSet(world->disjointSet, i) != root) {
            return false;
        }
    }
    return true;
}

// ==================== 瓦片操作 ====================

int getTile(World* world, int x, int y) {
    if (!isValidPosition(world, x, y)) {
        return TILE_WALL;
    }
    return world->tiles[y][x];
}

void setTile(World* world, int x, int y, int tileType) {
    if (isValidPosition(world, x, y)) {
        world->tiles[y][x] = tileType;
    }
}

// ==================== JSON输出函数 ====================

int getRoomsJSON(World* world, char* buffer, size_t bufferSize) {
    if (!world || !buffer || bufferSize == 0) return -1;

    int pos = 0;
    if (bufAppend(buffer, bufferSize, &pos, "[") != 0) return -1;

    bool first = true;
    for (int i = 0; i < world->roomCount; i++) {
        if (!world->rooms[i].exists) continue;
        if (!first) {
            if (bufAppend(buffer, bufferSize, &pos, ",") != 0) return -1;
        }
        first = false;

        if (bufAppend(buffer, bufferSize, &pos,
            "{\"id\":%d,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
            world->rooms[i].id,
            world->rooms[i].x,
            world->rooms[i].y,
            world->rooms[i].width,
            world->rooms[i].height) != 0) return -1;
    }

    if (bufAppend(buffer, bufferSize, &pos, "]") != 0) return -1;
    return 0;
}

int getCorridorsJSON(World* world, char* buffer, size_t bufferSize) {
    if (!world || !buffer || bufferSize == 0) return -1;

    int pos = 0;
    if (bufAppend(buffer, bufferSize, &pos, "[") != 0) return -1;

    for (int i = 0; i < world->corridorCount; i++) {
        if (i > 0) {
            if (bufAppend(buffer, bufferSize, &pos, ",") != 0) return -1;
        }
        if (bufAppend(buffer, bufferSize, &pos,
            "{\"id\":%d,\"start\":{\"x\":%d,\"y\":%d},\"end\":{\"x\":%d,\"y\":%d},\"isTurning\":%s}",
            world->corridors[i].id,
            world->corridors[i].start.x,
            world->corridors[i].start.y,
            world->corridors[i].end.x,
            world->corridors[i].end.y,
            world->corridors[i].isTurning ? "true" : "false") != 0) return -1;
    }

    if (bufAppend(buffer, bufferSize, &pos, "]") != 0) return -1;
    return 0;
}

int getWorldMapJSON(World* world, char* buffer, size_t bufferSize) {
    if (!world || !buffer || bufferSize == 0) return -1;

    int pos = 0;
    if (bufAppend(buffer, bufferSize, &pos, "[") != 0) return -1;

    for (int y = 0; y < world->height; y++) {
        if (y > 0) {
            if (bufAppend(buffer, bufferSize, &pos, ",") != 0) return -1;
        }
        if (bufAppend(buffer, bufferSize, &pos, "[") != 0) return -1;

        for (int x = 0; x < world->width; x++) {
            if (x > 0) {
                if (bufAppend(buffer, bufferSize, &pos, ",") != 0) return -1;
            }
            if (bufAppend(buffer, bufferSize, &pos, "%d", world->tiles[y][x]) != 0) return -1;
        }

        if (bufAppend(buffer, bufferSize, &pos, "]") != 0) return -1;
    }

    if (bufAppend(buffer, bufferSize, &pos, "]") != 0) return -1;
    return 0;
}

int getWorldJSON(World* world, char* buffer, size_t bufferSize) {
    if (!world || !buffer || bufferSize == 0) return -1;

    int pos = 0;
    if (bufAppend(buffer, bufferSize, &pos,
        "{\"seed\":%ld,\"width\":%d,\"height\":%d,\"roomCount\":%d,\"corridorCount\":%d,",
        world->seed, world->width, world->height, world->roomCount, world->corridorCount) != 0) return -1;

    if (bufAppend(buffer, bufferSize, &pos, "\"rooms\":") != 0) return -1;
    char roomsBuffer[4096];
    if (getRoomsJSON(world, roomsBuffer, sizeof(roomsBuffer)) != 0) return -1;
    if (bufAppend(buffer, bufferSize, &pos, "%s,", roomsBuffer) != 0) return -1;

    if (bufAppend(buffer, bufferSize, &pos, "\"corridors\":") != 0) return -1;
    char corridorsBuffer[16384]; // 走廊上限更大，避免溢出
    if (getCorridorsJSON(world, corridorsBuffer, sizeof(corridorsBuffer)) != 0) return -1;
    if (bufAppend(buffer, bufferSize, &pos, "%s,", corridorsBuffer) != 0) return -1;

    if (bufAppend(buffer, bufferSize, &pos, "\"map\":") != 0) return -1;
    char mapBuffer[65536];
    if (getWorldMapJSON(world, mapBuffer, sizeof(mapBuffer)) != 0) return -1;
    if (bufAppend(buffer, bufferSize, &pos, "%s", mapBuffer) != 0) return -1;

    if (bufAppend(buffer, bufferSize, &pos, "}") != 0) return -1;
    return 0;
}

// ==================== 路径查找实现 ====================

int findShortestPath(World* world, int startRoomId, int endRoomId,
                     int* path, int maxPathLength) {
    if (!world || !path || maxPathLength <= 0) return -1;
    if (startRoomId < 0 || endRoomId < 0) return -1;
    if (startRoomId >= MAX_ROOMS || endRoomId >= MAX_ROOMS) return -1;
    if (!world->rooms[startRoomId].exists || !world->rooms[endRoomId].exists) return -1;

    if (startRoomId == endRoomId) {
        path[0] = startRoomId;
        return 1;
    }

    Queue* queue = createQueue();
    if (!queue) return -1;

    bool visited[MAX_ROOMS] = {false};
    int parent[MAX_ROOMS];
    for (int i = 0; i < MAX_ROOMS; i++) {
        parent[i] = -1;
    }

    visited[startRoomId] = true;
    enqueue(queue, startRoomId);

    while (!isEmptyQueue(queue)) {
        int current = dequeue(queue);

        // 遍历所有连接的房间
        RoomConnection* conn = world->connections[current];
        while (conn) {
            int nextRoom = conn->roomId;

            if (!visited[nextRoom]) {
                visited[nextRoom] = true;
                parent[nextRoom] = current;
                enqueue(queue, nextRoom);

                if (nextRoom == endRoomId) {
                    // 找到路径，回溯
                    destroyQueue(queue);

                    int pathLength = 0;
                    int node = endRoomId;
                    int tempPath[MAX_ROOMS];

                    while (node != -1 && pathLength < maxPathLength) {
                        tempPath[pathLength++] = node;
                        node = parent[node];
                    }

                    // 反转路径
                    for (int i = 0; i < pathLength; i++) {
                        path[i] = tempPath[pathLength - 1 - i];
                    }

                    return pathLength;
                }
            }

            conn = conn->next;
        }
    }

    destroyQueue(queue);
    return -1;  // 未找到路径
}

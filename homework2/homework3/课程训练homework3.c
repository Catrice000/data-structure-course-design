#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_USERS 1000
#define MAX_NAME_LEN 64

/* ================= 数据结构 ================= */

typedef struct User {
    int id;
    char name[MAX_NAME_LEN];
    bool exists;
} User;

typedef struct AdjListNode {
    int userId;
    struct AdjListNode* next;
} AdjListNode;

typedef struct AdjList {
    AdjListNode* head;
} AdjList;

typedef struct Graph {
    User users[MAX_USERS + 1];        // 1..MAX_USERS
    AdjList adjLists[MAX_USERS + 1];  // 1..MAX_USERS
    int userCount;
    int nextId;
} Graph;

/* ================= 工具函数 ================= */

static void trimNewline(char* s) {
    if (!s) return;
    s[strcspn(s, "\n")] = '\0';
}

static int readInt(const char* prompt) {
    char buf[128];
    while (1) {
        if (prompt) printf("%s", prompt);
        if (!fgets(buf, sizeof(buf), stdin)) return -1;
        trimNewline(buf);
        if (buf[0] == '\0') continue;

        char* end = NULL;
        long v = strtol(buf, &end, 10);
        if (*end == '\0') return (int)v;

        printf("输入不是整数，再来一遍。\n");
    }
}

static void readString(const char* prompt, char* out, size_t outSize) {
    if (prompt) printf("%s", prompt);
    if (!fgets(out, outSize, stdin)) {
        out[0] = '\0';
        return;
    }
    trimNewline(out);
}

/* ================= 图基础操作 ================= */

static Graph* initGraph(void) {
    Graph* g = (Graph*)malloc(sizeof(Graph));
    if (!g) {
        printf("内存分配失败！\n");
        return NULL;
    }
    g->userCount = 0;
    g->nextId = 1;

    for (int i = 0; i <= MAX_USERS; i++) {
        g->users[i].exists = false;
        g->users[i].id = i;
        g->users[i].name[0] = '\0';
        g->adjLists[i].head = NULL;
    }
    return g;
}

static void freeGraph(Graph* g) {
    if (!g) return;
    for (int i = 1; i <= MAX_USERS; i++) {
        AdjListNode* cur = g->adjLists[i].head;
        while (cur) {
            AdjListNode* tmp = cur;
            cur = cur->next;
            free(tmp);
        }
        g->adjLists[i].head = NULL;
    }
    free(g);
}

static bool isValidUserId(int id) {
    return id >= 1 && id <= MAX_USERS;
}

static bool userExists(Graph* g, int id) {
    return g && isValidUserId(id) && g->users[id].exists;
}

static AdjListNode* createAdjListNode(int userId) {
    AdjListNode* node = (AdjListNode*)malloc(sizeof(AdjListNode));
    if (!node) return NULL;
    node->userId = userId;
    node->next = NULL;
    return node;
}

static bool areFriends(Graph* g, int a, int b) {
    if (!g) return false;
    AdjListNode* cur = g->adjLists[a].head;
    while (cur) {
        if (cur->userId == b) return true;
        cur = cur->next;
    }
    return false;
}

static int addUser(Graph* g, const char* name) {
    if (!g) return -1;
    if (g->userCount >= MAX_USERS) {
        printf("用户数量已达上限（%d）。\n", MAX_USERS);
        return -1;
    }

    int id = g->nextId;
    while (id <= MAX_USERS && g->users[id].exists) id++;
    if (id > MAX_USERS) {
        printf("没有可用的用户ID了。\n");
        return -1;
    }

    g->users[id].exists = true;
    g->users[id].id = id;
    strncpy(g->users[id].name, name, MAX_NAME_LEN - 1);
    g->users[id].name[MAX_NAME_LEN - 1] = '\0';

    g->userCount++;
    g->nextId = id + 1;

    printf("添加用户成功：ID=%d，名字=%s\n", id, g->users[id].name);
    return id;
}

static bool addFriend(Graph* g, int a, int b) {
    if (!userExists(g, a) || !userExists(g, b)) {
        printf("用户不存在。\n");
        return false;
    }
    if (a == b) {
        printf("不能把自己加为好友。\n");
        return false;
    }
    if (areFriends(g, a, b)) {
        printf("他们已经是好友了。\n");
        return false;
    }

    AdjListNode* n1 = createAdjListNode(b);
    AdjListNode* n2 = createAdjListNode(a);
    if (!n1 || !n2) {
        free(n1);
        free(n2);
        printf("内存分配失败，添加好友失败。\n");
        return false;
    }

    n1->next = g->adjLists[a].head;
    g->adjLists[a].head = n1;

    n2->next = g->adjLists[b].head;
    g->adjLists[b].head = n2;

    printf("添加好友成功：%d(%s) <-> %d(%s)\n",
           a, g->users[a].name, b, g->users[b].name);
    return true;
}

static bool removeFriendOneSide(Graph* g, int from, int target) {
    AdjListNode* prev = NULL;
    AdjListNode* cur = g->adjLists[from].head;
    while (cur && cur->userId != target) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur) return false;

    if (!prev) g->adjLists[from].head = cur->next;
    else prev->next = cur->next;

    free(cur);
    return true;
}

static bool removeFriend(Graph* g, int a, int b) {
    if (!userExists(g, a) || !userExists(g, b)) {
        printf("用户不存在。\n");
        return false;
    }
    if (a == b) {
        printf("自己和自己没法“解除好友”，别闹。\n");
        return false;
    }
    if (!areFriends(g, a, b)) {
        printf("他们本来就不是好友。\n");
        return false;
    }

    bool ok1 = removeFriendOneSide(g, a, b);
    bool ok2 = removeFriendOneSide(g, b, a);

    if (ok1 && ok2) {
        printf("删除好友关系成功：%d(%s) x %d(%s)\n",
               a, g->users[a].name, b, g->users[b].name);
        return true;
    }

    printf("删除过程中出现异常（图结构不一致）。\n");
    return false;
}

/* ================= 显示与统计 ================= */

static int degreeOf(Graph* g, int id) {
    int d = 0;
    AdjListNode* cur = g->adjLists[id].head;
    while (cur) { d++; cur = cur->next; }
    return d;
}

static void displayAllUsers(Graph* g) {
    if (!g || g->userCount == 0) {
        printf("暂无用户。\n");
        return;
    }
    printf("\n========== 用户列表 ==========\n");
    printf("%-6s %-30s %-8s\n", "ID", "用户名", "好友数");
    printf("------------------------------------------------\n");
    for (int i = 1; i <= MAX_USERS; i++) {
        if (g->users[i].exists) {
            printf("%-6d %-30s %-8d\n", i, g->users[i].name, degreeOf(g, i));
        }
    }
    printf("================================\n\n");
}

static void displayFriends(Graph* g, int id) {
    if (!userExists(g, id)) {
        printf("用户不存在。\n");
        return;
    }
    printf("\n用户 %d (%s) 的好友列表：\n", id, g->users[id].name);
    AdjListNode* cur = g->adjLists[id].head;
    if (!cur) {
        printf("  暂无好友\n\n");
        return;
    }
    while (cur) {
        int fid = cur->userId;
        printf("  - %d: %s\n", fid, g->users[fid].name);
        cur = cur->next;
    }
    printf("\n");
}

static void displayStatistics(Graph* g) {
    if (!g) return;
    if (g->userCount == 0) {
        printf("暂无用户，统计为空。\n");
        return;
    }

    long long totalAdj = 0;
    int maxDeg = -1, minDeg = 1e9;
    int maxUser = -1, minUser = -1;

    for (int i = 1; i <= MAX_USERS; i++) {
        if (!g->users[i].exists) continue;
        int d = degreeOf(g, i);
        totalAdj += d;
        if (d > maxDeg) { maxDeg = d; maxUser = i; }
        if (d < minDeg) { minDeg = d; minUser = i; }
    }

    long long edges = totalAdj / 2; // 无向图每条边算两次
    double avgFriends = (double)totalAdj / (double)g->userCount;

    printf("\n========== 统计信息 ==========\n");
    printf("用户总数：%d\n", g->userCount);
    printf("好友关系总数（边数）：%lld\n", edges);
    printf("平均好友数：%.2f\n", avgFriends);
    if (maxUser != -1)
        printf("最多好友：%d（用户 %d: %s）\n", maxDeg, maxUser, g->users[maxUser].name);
    if (minUser != -1)
        printf("最少好友：%d（用户 %d: %s）\n", minDeg, minUser, g->users[minUser].name);
    printf("==============================\n\n");
}

/* ================= 图算法：最短路径 BFS ================= */

static int findShortestPath(Graph* g, int start, int goal, int* outPath, int* outLen) {
    if (!userExists(g, start) || !userExists(g, goal)) return -2;
    if (start == goal) {
        outPath[0] = start;
        *outLen = 1;
        return 0;
    }

    bool visited[MAX_USERS + 1] = {false};
    int parent[MAX_USERS + 1];
    int dist[MAX_USERS + 1];
    int queue[MAX_USERS + 1];
    int front = 0, rear = 0;

    for (int i = 0; i <= MAX_USERS; i++) {
        parent[i] = -1;
        dist[i] = -1;
    }

    visited[start] = true;
    dist[start] = 0;
    queue[rear++] = start;

    while (front < rear) {
        int cur = queue[front++];

        AdjListNode* n = g->adjLists[cur].head;
        while (n) {
            int nb = n->userId;
            if (!visited[nb]) {
                visited[nb] = true;
                parent[nb] = cur;
                dist[nb] = dist[cur] + 1;
                queue[rear++] = nb;

                if (nb == goal) break;
            }
            n = n->next;
        }
        if (visited[goal]) break;
    }

    if (!visited[goal]) return -1;

    // 回溯路径
    int tmp[MAX_USERS + 1];
    int len = 0;
    int x = goal;
    while (x != -1) {
        tmp[len++] = x;
        x = parent[x];
    }
    // 反转
    for (int i = 0; i < len; i++) {
        outPath[i] = tmp[len - 1 - i];
    }
    *outLen = len;
    return dist[goal];
}

static void printPath(Graph* g, int* path, int len, int degree) {
    if (degree < 0) {
        printf("不可达：他们不在同一个“朋友圈”。\n\n");
        return;
    }
    printf("\n最短路径（%d 度好友）：\n", degree);
    for (int i = 0; i < len; i++) {
        int id = path[i];
        printf("%d(%s)", id, g->users[id].name);
        if (i != len - 1) printf(" -> ");
    }
    printf("\n\n");
}

/* ================= 共同好友 ================= */

static void findCommonFriends(Graph* g, int a, int b) {
    if (!userExists(g, a) || !userExists(g, b)) {
        printf("用户不存在。\n");
        return;
    }

    bool mark[MAX_USERS + 1] = {false};
    AdjListNode* cur = g->adjLists[a].head;
    while (cur) {
        mark[cur->userId] = true;
        cur = cur->next;
    }

    int count = 0;
    printf("\n共同好友：\n");
    AdjListNode* cur2 = g->adjLists[b].head;
    while (cur2) {
        int x = cur2->userId;
        if (mark[x]) {
            printf("  - %d: %s\n", x, g->users[x].name);
            count++;
        }
        cur2 = cur2->next;
    }

    if (count == 0) printf("  （没有共同好友）\n");
    printf("\n");
}

/* ================= 推荐好友：好友的好友 ================= */

static void recommendFriends(Graph* g, int id) {
    if (!userExists(g, id)) {
        printf("用户不存在。\n");
        return;
    }

    bool isFriend[MAX_USERS + 1] = {false};
    bool added[MAX_USERS + 1] = {false};

    // 标记自己
    isFriend[id] = true;

    // 标记直接好友
    AdjListNode* f = g->adjLists[id].head;
    while (f) {
        isFriend[f->userId] = true;
        f = f->next;
    }

    int recCount = 0;
    printf("\n推荐好友（好友的好友）：\n");

    // 遍历每个好友的好友
    AdjListNode* friendNode = g->adjLists[id].head;
    while (friendNode) {
        int fid = friendNode->userId;
        AdjListNode* fof = g->adjLists[fid].head;
        while (fof) {
            int candidate = fof->userId;
            if (!isFriend[candidate] && !added[candidate] && userExists(g, candidate)) {
                printf("  - %d: %s（通过 %d: %s）\n",
                       candidate, g->users[candidate].name, fid, g->users[fid].name);
                added[candidate] = true;
                recCount++;
            }
            fof = fof->next;
        }
        friendNode = friendNode->next;
    }

    if (recCount == 0) printf("  （暂无推荐）\n");
    printf("\n");
}

/* ================= 朋友圈：连通分量 DFS ================= */

static void dfsComponent(Graph* g, int u, bool* visited, int* comp, int* cnt) {
    visited[u] = true;
    comp[(*cnt)++] = u;

    AdjListNode* cur = g->adjLists[u].head;
    while (cur) {
        int v = cur->userId;
        if (!visited[v]) dfsComponent(g, v, visited, comp, cnt);
        cur = cur->next;
    }
}

static void showConnectedComponent(Graph* g, int id) {
    if (!userExists(g, id)) {
        printf("用户不存在。\n");
        return;
    }

    bool visited[MAX_USERS + 1] = {false};
    int comp[MAX_USERS + 1];
    int cnt = 0;

    dfsComponent(g, id, visited, comp, &cnt);

    printf("\n用户 %d(%s) 的朋友圈（连通分量，共 %d 人）：\n", id, g->users[id].name, cnt);
    for (int i = 0; i < cnt; i++) {
        int x = comp[i];
        printf("  - %d: %s\n", x, g->users[x].name);
    }
    printf("\n");
}

/* ================= 菜单 ================= */

static void showMenu(void) {
    printf("========== 社交网络系统（图/邻接表） ==========\n");
    printf("1. 添加用户\n");
    printf("2. 添加好友关系\n");
    printf("3. 删除好友关系\n");
    printf("4. 显示所有用户\n");
    printf("5. 查看好友列表\n");
    printf("6. 查找最短路径（BFS）\n");
    printf("7. 查找共同好友\n");
    printf("8. 推荐好友（好友的好友）\n");
    printf("9. 查找朋友圈（连通分量 DFS）\n");
    printf("10. 统计信息\n");
    printf("0. 退出程序\n");
    printf("=============================================\n");
}

/* ================= 主函数 ================= */

int main(void) {
    Graph* g = initGraph();
    if (!g) return 1;

    printf("欢迎使用社交网络好友关系系统。\n");

    while (1) {
        showMenu();
        int choice = readInt("请选择操作：");

        if (choice == 0) {
            printf("退出程序，记得释放内存这种人类基本礼仪。\n");
            freeGraph(g);
            return 0;
        }

        if (choice == 1) {
            char name[MAX_NAME_LEN];
            readString("请输入用户名称：", name, sizeof(name));
            if (name[0] == '\0') {
                printf("名字不能为空。\n");
                continue;
            }
            addUser(g, name);
        } 
        else if (choice == 2) {
            int a = readInt("请输入用户1 ID：");
            int b = readInt("请输入用户2 ID：");
            addFriend(g, a, b);
        } 
        else if (choice == 3) {
            int a = readInt("请输入用户1 ID：");
            int b = readInt("请输入用户2 ID：");
            removeFriend(g, a, b);
        } 
        else if (choice == 4) {
            displayAllUsers(g);
        } 
        else if (choice == 5) {
            int id = readInt("请输入用户ID：");
            displayFriends(g, id);
        } 
        else if (choice == 6) {
            int a = readInt("请输入起始用户ID：");
            int b = readInt("请输入目标用户ID：");
            int path[MAX_USERS + 1];
            int len = 0;
            int deg = findShortestPath(g, a, b, path, &len);
            if (deg == -2) printf("用户不存在。\n\n");
            else printPath(g, path, len, deg);
        } 
        else if (choice == 7) {
            int a = readInt("请输入用户1 ID：");
            int b = readInt("请输入用户2 ID：");
            findCommonFriends(g, a, b);
        } 
        else if (choice == 8) {
            int id = readInt("请输入用户ID：");
            recommendFriends(g, id);
        } 
        else if (choice == 9) {
            int id = readInt("请输入用户ID：");
            showConnectedComponent(g, id);
        } 
        else if (choice == 10) {
            displayStatistics(g);
        } 
        else {
            printf("无效选择。\n\n");
        }
    }
}

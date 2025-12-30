#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NAME_LEN 100
#define MAX_ARTIST_LEN 100
#define MAX_ALBUM_LEN 100

// ========== 数据结构定义 ==========

typedef struct {
    char name[MAX_NAME_LEN];
    char artist[MAX_ARTIST_LEN];
    char album[MAX_ALBUM_LEN];
} Song;

typedef struct ListNode {
    Song song;
    struct ListNode* next;
} ListNode;

typedef struct QueueNode {
    Song song;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode* front;
    QueueNode* rear;
    int size;
} Queue;

typedef struct StackNode {
    Song song;
    struct StackNode* next;
} StackNode;

typedef struct {
    StackNode* top;
    int size;
} Stack;


// ========== 通用输入工具（解决 scanf/fgets 混用的麻烦） ==========

static void trimNewline(char* s) {
    if (!s) return;
    s[strcspn(s, "\n")] = 0;
}

static void readLine(const char* prompt, char* buf, int cap) {
    if (prompt) printf("%s", prompt);
    if (fgets(buf, cap, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    trimNewline(buf);
}

static int readInt(const char* prompt) {
    char line[64];
    while (1) {
        readLine(prompt, line, sizeof(line));
        if (line[0] == '\0') {
            printf("输入为空，请重新输入。\n");
            continue;
        }
        char* end = NULL;
        long v = strtol(line, &end, 10);
        if (*end != '\0') {
            printf("请输入整数。\n");
            continue;
        }
        return (int)v;
    }
}


// ========== 链表：歌曲列表 ==========

ListNode* createListNode(Song song) {
    ListNode* node = (ListNode*)malloc(sizeof(ListNode));
    if (node == NULL) {
        printf("内存分配失败！\n");
        return NULL;
    }
    node->song = song;
    node->next = NULL;
    return node;
}

void addSongToList(ListNode** head, Song song) {
    if (head == NULL) return;

    ListNode* newNode = createListNode(song);
    if (newNode == NULL) return;

    if (*head == NULL) {
        *head = newNode;
    } else {
        ListNode* cur = *head;
        while (cur->next != NULL) cur = cur->next;
        cur->next = newNode;
    }
    printf("歌曲 \"%s\" 添加成功！\n", song.name);
}

ListNode* findSongInList(ListNode* head, const char* name) {
    for (ListNode* cur = head; cur != NULL; cur = cur->next) {
        if (strcmp(cur->song.name, name) == 0) return cur;
    }
    return NULL;
}

int deleteSongFromList(ListNode** head, const char* name) {
    if (head == NULL || *head == NULL) {
        printf("歌曲列表为空！\n");
        return 0;
    }

    ListNode* cur = *head;
    ListNode* prev = NULL;

    if (strcmp(cur->song.name, name) == 0) {
        *head = cur->next;
        free(cur);
        printf("歌曲 \"%s\" 删除成功！\n", name);
        return 1;
    }

    while (cur != NULL && strcmp(cur->song.name, name) != 0) {
        prev = cur;
        cur = cur->next;
    }

    if (cur == NULL) {
        printf("未找到歌曲 \"%s\"！\n", name);
        return 0;
    }

    prev->next = cur->next;
    free(cur);
    printf("歌曲 \"%s\" 删除成功！\n", name);
    return 1;
}

void displayAllSongs(ListNode* head) {
    if (head == NULL) {
        printf("歌曲列表为空！\n");
        return;
    }

    printf("\n========== 歌曲列表 ==========\n");
    printf("%-4s %-30s %-20s %-30s\n", "序号", "歌曲名称", "歌手", "专辑");
    printf("------------------------------------------------------------\n");

    int idx = 1;
    for (ListNode* cur = head; cur != NULL; cur = cur->next) {
        printf("%-4d %-30s %-20s %-30s\n",
               idx++, cur->song.name, cur->song.artist, cur->song.album);
    }
    printf("================================\n\n");
}

int modifySongInList(ListNode* head, const char* name) {
    ListNode* node = findSongInList(head, name);
    if (node == NULL) {
        printf("未找到歌曲 \"%s\"！\n", name);
        return 0;
    }

    printf("\n当前歌曲信息：\n");
    printf("名称：%s\n歌手：%s\n专辑：%s\n", node->song.name, node->song.artist, node->song.album);

    char buf[MAX_NAME_LEN];

    readLine("新名称（直接回车保持不变）：", buf, sizeof(buf));
    if (strlen(buf) > 0) strcpy(node->song.name, buf);

    readLine("新歌手（直接回车保持不变）：", buf, sizeof(buf));
    if (strlen(buf) > 0) strcpy(node->song.artist, buf);

    readLine("新专辑（直接回车保持不变）：", buf, sizeof(buf));
    if (strlen(buf) > 0) strcpy(node->song.album, buf);

    printf("修改成功！\n");
    return 1;
}

void sortSongsByName(ListNode** head) {
    if (head == NULL || *head == NULL || (*head)->next == NULL) {
        printf("歌曲不足两首，无需排序。\n");
        return;
    }

    int swapped;
    ListNode* lptr = NULL;

    do {
        swapped = 0;
        ListNode* cur = *head;

        while (cur->next != lptr) {
            if (strcmp(cur->song.name, cur->next->song.name) > 0) {
                Song tmp = cur->song;
                cur->song = cur->next->song;
                cur->next->song = tmp;
                swapped = 1;
            }
            cur = cur->next;
        }
        lptr = cur;
    } while (swapped);

    printf("歌曲列表已按名称排序！\n");
}

int getListLength(ListNode* head) {
    int count = 0;
    for (ListNode* cur = head; cur != NULL; cur = cur->next) count++;
    return count;
}

// 从链表随机取一首歌（成功返回1，并写入 out）
int getRandomSong(ListNode* head, Song* out) {
    int len = getListLength(head);
    if (len <= 0) return 0;

    int k = rand() % len; // 0..len-1
    ListNode* cur = head;
    while (k-- > 0 && cur != NULL) cur = cur->next;

    if (cur == NULL) return 0;
    *out = cur->song;
    return 1;
}


// ========== 队列：播放队列 ==========

void initQueue(Queue* q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

void enqueue(Queue* q, Song song) {
    if (q == NULL) return;

    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    if (node == NULL) {
        printf("内存分配失败！\n");
        return;
    }
    node->song = song;
    node->next = NULL;

    if (q->rear == NULL) {
        q->front = q->rear = node;
    } else {
        q->rear->next = node;
        q->rear = node;
    }
    q->size++;
    printf("歌曲 \"%s\" 已添加到播放队列！\n", song.name);
}

int dequeue(Queue* q, Song* out) {
    if (q == NULL || q->front == NULL) {
        printf("播放队列为空！\n");
        return 0;
    }

    QueueNode* temp = q->front;
    if (out) *out = temp->song;

    q->front = q->front->next;
    if (q->front == NULL) q->rear = NULL;

    free(temp);
    q->size--;
    return 1;
}

void displayQueue(Queue* q) {
    if (q == NULL || q->front == NULL) {
        printf("播放队列为空！\n");
        return;
    }

    printf("\n========== 播放队列 ==========\n");
    printf("%-4s %-30s %-20s %-30s\n", "序号", "歌曲名称", "歌手", "专辑");
    printf("------------------------------------------------------------\n");

    int idx = 1;
    for (QueueNode* cur = q->front; cur != NULL; cur = cur->next) {
        printf("%-4d %-30s %-20s %-30s\n",
               idx++, cur->song.name, cur->song.artist, cur->song.album);
    }
    printf("================================\n");
    printf("队列中共有 %d 首歌曲\n\n", q->size);
}

void clearQueue(Queue* q) {
    if (q == NULL) return;
    while (q->front != NULL) {
        QueueNode* temp = q->front;
        q->front = q->front->next;
        free(temp);
    }
    q->rear = NULL;
    q->size = 0;
}

// Fisher-Yates 洗牌：打乱播放队列
void shuffleQueue(Queue* q) {
    if (q == NULL || q->size <= 1) {
        printf("播放队列为空或只有一首歌曲，无需打乱！\n");
        return;
    }

    int n = q->size;
    Song* arr = (Song*)malloc(sizeof(Song) * n);
    if (arr == NULL) {
        printf("内存分配失败！\n");
        return;
    }

    QueueNode* cur = q->front;
    for (int i = 0; i < n; i++) {
        arr[i] = cur->song;
        cur = cur->next;
    }

    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Song tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }

    clearQueue(q);

    for (int i = 0; i < n; i++) {
        // 静默入队：不打印那句“已添加…”
        QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
        if (!node) {
            printf("内存分配失败！\n");
            free(arr);
            return;
        }
        node->song = arr[i];
        node->next = NULL;

        if (q->rear == NULL) q->front = q->rear = node;
        else { q->rear->next = node; q->rear = node; }
        q->size++;
    }

    free(arr);
    printf("播放队列已随机打乱！\n");
}


// ========== 栈：播放历史 ==========

void initStack(Stack* s) {
    s->top = NULL;
    s->size = 0;
}

void push(Stack* s, Song song) {
    if (s == NULL) return;

    StackNode* node = (StackNode*)malloc(sizeof(StackNode));
    if (node == NULL) {
        printf("内存分配失败！\n");
        return;
    }
    node->song = song;
    node->next = s->top;
    s->top = node;
    s->size++;
}

int pop(Stack* s, Song* out) {
    if (s == NULL || s->top == NULL) {
        printf("播放历史为空！\n");
        return 0;
    }

    StackNode* temp = s->top;
    if (out) *out = temp->song;

    s->top = s->top->next;
    free(temp);
    s->size--;
    return 1;
}

void displayStack(Stack* s) {
    if (s == NULL || s->top == NULL) {
        printf("播放历史为空！\n");
        return;
    }

    printf("\n========== 播放历史 ==========\n");
    printf("%-4s %-30s %-20s %-30s\n", "序号", "歌曲名称", "歌手", "专辑");
    printf("------------------------------------------------------------\n");

    int idx = 1;
    for (StackNode* cur = s->top; cur != NULL; cur = cur->next) {
        printf("%-4d %-30s %-20s %-30s\n",
               idx++, cur->song.name, cur->song.artist, cur->song.album);
    }
    printf("================================\n");
    printf("历史记录中共有 %d 首歌曲\n\n", s->size);
}

void clearStack(Stack* s) {
    if (s == NULL) return;
    while (s->top != NULL) {
        StackNode* temp = s->top;
        s->top = s->top->next;
        free(temp);
    }
    s->size = 0;
}


// ========== 业务辅助 ==========

Song inputSong() {
    Song song;
    readLine("请输入歌曲名称：", song.name, MAX_NAME_LEN);
    readLine("请输入歌手：", song.artist, MAX_ARTIST_LEN);
    readLine("请输入专辑：", song.album, MAX_ALBUM_LEN);
    return song;
}

void printSong(const Song* s) {
    printf("名称：%s\n歌手：%s\n专辑：%s\n", s->name, s->artist, s->album);
}

void playOneSong(const Song* s, Stack* history) {
    printf("\n正在播放：\n");
    printSong(s);
    printf("播放完成！\n");
    if (history) push(history, *s);
}


// ========== 菜单 ==========

void showMainMenu() {
    printf("\n========== 音乐播放器 ==========\n");
    printf("1. 歌曲管理\n");
    printf("2. 播放队列管理\n");
    printf("3. 播放历史查看\n");
    printf("4. 播放下一首（从队列中播放）\n");
    printf("5. 随机播放（从歌曲列表随机选一首）\n");
    printf("0. 退出程序\n");
    printf("================================\n");
}

void showSongMenu() {
    printf("\n========== 歌曲管理 ==========\n");
    printf("1. 添加歌曲\n");
    printf("2. 删除歌曲\n");
    printf("3. 修改歌曲\n");
    printf("4. 查询歌曲\n");
    printf("5. 显示所有歌曲\n");
    printf("6. 按名称排序\n");
    printf("0. 返回主菜单\n");
    printf("================================\n");
}

void showQueueMenu() {
    printf("\n========== 播放队列管理 ==========\n");
    printf("1. 添加歌曲到播放队列（按歌名从列表添加）\n");
    printf("2. 显示播放队列\n");
    printf("3. 清空播放队列\n");
    printf("4. 随机打乱播放队列\n");
    printf("0. 返回主菜单\n");
    printf("================================\n");
}


// ========== 主程序 ==========

int main() {
    ListNode* songList = NULL;
    Queue playQueue;
    Stack playHistory;

    initQueue(&playQueue);
    initStack(&playHistory);
    srand((unsigned int)time(NULL));

    printf("欢迎使用音乐播放器！\n");

    while (1) {
        showMainMenu();
        int choice = readInt("请选择操作：");

        if (choice == 1) {
            while (1) {
                showSongMenu();
                int sub = readInt("请选择操作：");

                if (sub == 1) {
                    Song song = inputSong();
                    addSongToList(&songList, song);
                } else if (sub == 2) {
                    char name[MAX_NAME_LEN];
                    readLine("请输入要删除的歌曲名称：", name, MAX_NAME_LEN);
                    deleteSongFromList(&songList, name);
                } else if (sub == 3) {
                    char name[MAX_NAME_LEN];
                    readLine("请输入要修改的歌曲名称：", name, MAX_NAME_LEN);
                    modifySongInList(songList, name);
                } else if (sub == 4) {
                    char name[MAX_NAME_LEN];
                    readLine("请输入要查询的歌曲名称：", name, MAX_NAME_LEN);
                    ListNode* found = findSongInList(songList, name);
                    if (found) {
                        printf("\n找到歌曲：\n");
                        printSong(&found->song);
                    } else {
                        printf("未找到歌曲 \"%s\"！\n", name);
                    }
                } else if (sub == 5) {
                    displayAllSongs(songList);
                } else if (sub == 6) {
                    sortSongsByName(&songList);
                } else if (sub == 0) {
                    break;
                } else {
                    printf("无效的选择，请重新输入！\n");
                }
            }
        } else if (choice == 2) {
            while (1) {
                showQueueMenu();
                int sub = readInt("请选择操作：");

                if (sub == 1) {
                    char name[MAX_NAME_LEN];
                    readLine("请输入要添加到播放队列的歌曲名称：", name, MAX_NAME_LEN);
                    ListNode* found = findSongInList(songList, name);
                    if (found) enqueue(&playQueue, found->song);
                    else printf("未找到歌曲 \"%s\"！请先在歌曲管理中添加。\n", name);
                } else if (sub == 2) {
                    displayQueue(&playQueue);
                } else if (sub == 3) {
                    clearQueue(&playQueue);
                    printf("播放队列已清空！\n");
                } else if (sub == 4) {
                    shuffleQueue(&playQueue);
                } else if (sub == 0) {
                    break;
                } else {
                    printf("无效的选择，请重新输入！\n");
                }
            }
        } else if (choice == 3) {
            displayStack(&playHistory);
        } else if (choice == 4) {
            Song song;
            if (dequeue(&playQueue, &song)) {
                playOneSong(&song, &playHistory);
            }
        } else if (choice == 5) {
            Song song;
            if (!getRandomSong(songList, &song)) {
                printf("歌曲列表为空，无法随机播放！\n");
            } else {
                playOneSong(&song, &playHistory);
            }
        } else if (choice == 0) {
            printf("感谢使用，再见！\n");

            // 释放链表
            while (songList != NULL) {
                ListNode* temp = songList;
                songList = songList->next;
                free(temp);
            }

            clearQueue(&playQueue);
            clearStack(&playHistory);
            return 0;
        } else {
            printf("无效的选择，请重新输入！\n");
        }
    }
}

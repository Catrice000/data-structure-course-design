#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CONTENT_LEN 512
#define MAX_AUTHOR_LEN 64
#define MAX_DEPTH 3
#define INIT_CHILDREN_CAPACITY 4

/* ================= 数据结构定义 ================= */

typedef struct CommentNode {
    int id;
    char content[MAX_CONTENT_LEN];
    char author[MAX_AUTHOR_LEN];
    time_t timestamp;
    int likeCount;

    struct CommentNode* parent;
    struct CommentNode** children;
    int childCount;
    int childCapacity;
    int depth;
} CommentNode;

typedef struct {
    CommentNode** rootComments;
    int rootCount;
    int rootCapacity;
    int nextId;
} CommentSystem;

/* ================= 工具函数 ================= */

void trimNewline(char* s) {
    s[strcspn(s, "\n")] = 0;
}

void inputCommentInfo(char* content, char* author) {
    printf("请输入评论内容：");
    fgets(content, MAX_CONTENT_LEN, stdin);
    trimNewline(content);

    printf("请输入作者名称：");
    fgets(author, MAX_AUTHOR_LEN, stdin);
    trimNewline(author);
}

/* ================= 创建与初始化 ================= */

CommentNode* createCommentNode(int id, char* content, char* author) {
    CommentNode* node = (CommentNode*)malloc(sizeof(CommentNode));
    if (node == NULL) {
        printf("内存分配失败！\n");
        return NULL;
    }

    node->id = id;
    strcpy(node->content, content);
    strcpy(node->author, author);
    node->timestamp = time(NULL);
    node->likeCount = 0;

    node->parent = NULL;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    node->depth = 0;

    return node;
}

void initCommentSystem(CommentSystem* system) {
    system->rootComments = NULL;
    system->rootCount = 0;
    system->rootCapacity = 0;
    system->nextId = 1;
}

/* ================= 添加评论 ================= */

int addRootComment(CommentSystem* system, CommentNode* comment) {
    if (system->rootCount >= system->rootCapacity) {
        int newCap = (system->rootCapacity == 0) ?
                     INIT_CHILDREN_CAPACITY :
                     system->rootCapacity * 2;

        CommentNode** newArr =
            (CommentNode**)realloc(system->rootComments,
                                   newCap * sizeof(CommentNode*));
        if (newArr == NULL) {
            printf("内存扩展失败！\n");
            return 0;
        }
        system->rootComments = newArr;
        system->rootCapacity = newCap;
    }

    system->rootComments[system->rootCount++] = comment;
    printf("主评论添加成功！ID=%d\n", comment->id);
    return 1;
}

int addReply(CommentNode* parent, CommentNode* reply) {
    if (parent->depth >= MAX_DEPTH) {
        printf("超过最大嵌套深度（%d层），无法回复。\n", MAX_DEPTH);
        return 0;
    }

    if (parent->childCount >= parent->childCapacity) {
        int newCap = (parent->childCapacity == 0) ?
                     INIT_CHILDREN_CAPACITY :
                     parent->childCapacity * 2;

        CommentNode** newChildren =
            (CommentNode**)realloc(parent->children,
                                   newCap * sizeof(CommentNode*));
        if (newChildren == NULL) {
            printf("子节点扩展失败！\n");
            return 0;
        }
        parent->children = newChildren;
        parent->childCapacity = newCap;
    }

    reply->parent = parent;
    reply->depth = parent->depth + 1;
    parent->children[parent->childCount++] = reply;

    printf("回复添加成功！ID=%d\n", reply->id);
    return 1;
}

/* ================= 遍历与显示 ================= */

void displayComment(CommentNode* node, int indent) {
    if (node == NULL) return;

    for (int i = 0; i < indent; i++) printf("  ");
    printf("[%d] %s：%s (赞 %d)\n",
           node->id, node->author, node->content, node->likeCount);

    for (int i = 0; i < node->childCount; i++) {
        displayComment(node->children[i], indent + 1);
    }
}

void displayAllComments(CommentSystem* system) {
    if (system->rootCount == 0) {
        printf("暂无评论。\n");
        return;
    }

    printf("\n========== 评论列表 ==========\n");
    for (int i = 0; i < system->rootCount; i++) {
        displayComment(system->rootComments[i], 0);
        printf("\n");
    }
}

/* ================= 查找 ================= */

CommentNode* findCommentById(CommentNode* root, int id) {
    if (root == NULL) return NULL;
    if (root->id == id) return root;

    for (int i = 0; i < root->childCount; i++) {
        CommentNode* found =
            findCommentById(root->children[i], id);
        if (found != NULL) return found;
    }
    return NULL;
}

CommentNode* findCommentInSystem(CommentSystem* system, int id) {
    for (int i = 0; i < system->rootCount; i++) {
        CommentNode* found =
            findCommentById(system->rootComments[i], id);
        if (found != NULL) return found;
    }
    return NULL;
}

/* ================= 删除 ================= */

void removeFromParent(CommentNode* parent, CommentNode* child) {
    if (parent == NULL) return;

    int index = -1;
    for (int i = 0; i < parent->childCount; i++) {
        if (parent->children[i] == child) {
            index = i;
            break;
        }
    }
    if (index == -1) return;

    for (int i = index; i < parent->childCount - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->childCount--;
}

void deleteComment(CommentNode* node) {
    if (node == NULL) return;

    for (int i = node->childCount - 1; i >= 0; i--) {
        deleteComment(node->children[i]);
    }

    if (node->parent != NULL) {
        removeFromParent(node->parent, node);
    }

    free(node->children);
    free(node);
}

int deleteCommentFromSystem(CommentSystem* system, int id) {
    CommentNode* target = findCommentInSystem(system, id);
    if (target == NULL) {
        printf("未找到评论 %d\n", id);
        return 0;
    }

    if (target->parent == NULL) {
        for (int i = 0; i < system->rootCount; i++) {
            if (system->rootComments[i] == target) {
                for (int j = i; j < system->rootCount - 1; j++) {
                    system->rootComments[j] = system->rootComments[j + 1];
                }
                system->rootCount--;
                break;
            }
        }
    }

    deleteComment(target);
    printf("评论 %d 已删除。\n", id);
    return 1;
}

/* ================= 统计与点赞 ================= */

int countAllComments(CommentNode* node) {
    if (node == NULL) return 0;
    int count = 1;
    for (int i = 0; i < node->childCount; i++) {
        count += countAllComments(node->children[i]);
    }
    return count;
}

int countTotalComments(CommentSystem* system) {
    int total = 0;
    for (int i = 0; i < system->rootCount; i++) {
        total += countAllComments(system->rootComments[i]);
    }
    return total;
}

void likeComment(CommentNode* node) {
    if (node == NULL) return;
    node->likeCount++;
    printf("评论 %d 点赞成功，当前点赞数：%d\n",
           node->id, node->likeCount);
}

/* ================= 菜单 ================= */

void showMainMenu() {
    printf("\n========== 评论系统 ==========\n");
    printf("1. 添加主评论\n");
    printf("2. 添加回复\n");
    printf("3. 显示所有评论\n");
    printf("4. 查找评论\n");
    printf("5. 删除评论\n");
    printf("6. 点赞评论\n");
    printf("7. 统计信息\n");
    printf("0. 退出程序\n");
    printf("==============================\n");
    printf("请选择操作：");
}

/* ================= 主程序 ================= */

int main() {
    CommentSystem system;
    initCommentSystem(&system);

    int choice, id;
    char content[MAX_CONTENT_LEN];
    char author[MAX_AUTHOR_LEN];

    printf("欢迎使用评论系统！\n");

    while (1) {
        showMainMenu();
        scanf("%d", &choice);
        getchar();

        switch (choice) {
        case 1: {
            inputCommentInfo(content, author);
            CommentNode* node =
                createCommentNode(system.nextId++, content, author);
            if (node) addRootComment(&system, node);
            break;
        }
        case 2: {
            printf("请输入父评论ID：");
            scanf("%d", &id);
            getchar();

            CommentNode* parent = findCommentInSystem(&system, id);
            if (!parent) {
                printf("未找到父评论。\n");
                break;
            }

            inputCommentInfo(content, author);
            CommentNode* reply =
                createCommentNode(system.nextId++, content, author);
            if (reply) addReply(parent, reply);
            break;
        }
        case 3:
            displayAllComments(&system);
            break;

        case 4: {
            printf("请输入评论ID：");
            scanf("%d", &id);
            getchar();
            CommentNode* found = findCommentInSystem(&system, id);
            if (found) displayComment(found, 0);
            else printf("未找到评论。\n");
            break;
        }
        case 5:
            printf("请输入要删除的评论ID：");
            scanf("%d", &id);
            getchar();
            deleteCommentFromSystem(&system, id);
            break;

        case 6:
            printf("请输入要点赞的评论ID：");
            scanf("%d", &id);
            getchar();
            likeComment(findCommentInSystem(&system, id));
            break;

        case 7:
            printf("主评论数：%d\n", system.rootCount);
            printf("总评论数：%d\n", countTotalComments(&system));
            break;

        case 0:
            printf("退出程序。\n");
            for (int i = 0; i < system.rootCount; i++) {
                deleteComment(system.rootComments[i]);
            }
            free(system.rootComments);
            return 0;

        default:
            printf("无效选择。\n");
        }
    }
}

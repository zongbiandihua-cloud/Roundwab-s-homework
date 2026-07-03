#define _CRT_SECURE_NO_WARNINGS
/* 包含：选课记录基础工具、两种存储结构（双向链表 /哈希表）、
 * 筛选排序、统计、CSV 持久化、数据生成、性能对比。只用 C 标准库。
 * 结构各自定义自己的结构体（ListStore /HashStore），
 * 对上层都伪装成通用的 Store*，每个函数内部再转回自己的类型。
 */
#include "oms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* ================================================================
 *  一、选课记录基础工具
 * ================================================================ */

int date_to_int(int year, int month, int day) {
    return year * 10000 + month * 100 + day;
}

/* 记录合法性校验：基本的输入检查 */
int record_is_valid(const CourseRecord *rec) {
    int i;
    if (rec == NULL) return 0;

    /* 学号必须是 12 位数字 */
    if (strlen(rec->studentId) != 12) return 0;
    for (i = 0; i < 12; i++)
        if (rec->studentId[i] < '0' || rec->studentId[i] > '9') return 0;

    /* 课程编号长度为 8 */
    if (strlen(rec->courseId) != 8) return 0;

    /* 姓名、学院、课程名不能为空 */
    if (rec->name[0] == '\0' || rec->college[0] == '\0' || rec->courseName[0] == '\0')
        return 0;

    /* 成绩范围 0-100 */
    if (rec->score < 0 || rec->score > 100) return 0;

    /* 学分范围 0.5-6.0 */
    if (rec->credit <= 0.0f || rec->credit > 6.0f) return 0;

    /* 日期基本合法性 */
    if (rec->year < 2000 || rec->year > 2100) return 0;
    if (rec->month < 1 || rec->month > 12) return 0;
    if (rec->day < 1 || rec->day > 31) return 0;

    return 1;
}

/* 判断一条记录是否就是给定的 学号 + 课程编号 */
static int same_key(const CourseRecord *r, const char *sid, const char *cid) {
    return strcmp(r->studentId, sid) == 0 && strcmp(r->courseId, cid) == 0;
}


/* 按指定字段比较两条记录：a<b 返回负，相等返回 0，a>b 返回正（排序用） */
static int compare_field(const CourseRecord *a, const CourseRecord *b, FieldId field) {
    switch (field) {
        case FIELD_STUDENT_ID:  return strcmp(a->studentId, b->studentId);
        case FIELD_NAME:        return strcmp(a->name, b->name);
        case FIELD_COLLEGE:     return strcmp(a->college, b->college);
        case FIELD_COURSE_ID:   return strcmp(a->courseId, b->courseId);
        case FIELD_COURSE_NAME: return strcmp(a->courseName, b->courseName);
        case FIELD_TERM:        return strcmp(a->term, b->term);
        case FIELD_CREDIT:
            if (a->credit < b->credit) return -1;
            if (a->credit > b->credit) return 1;
            return 0;
        case FIELD_DATE:
            return date_to_int(a->year, a->month, a->day) -
                   date_to_int(b->year, b->month, b->day);
        case FIELD_SCORE:       return a->score - b->score;
        default:                return 0;
    }
}

void record_print_header(void) {
    printf("%-13s %-8s %-20s %-10s %-18s %-5s %-8s %-11s %-5s\n",
           "学号", "姓名", "学院", "课程编号", "课程名称",
           "学分", "学期", "选课日期", "成绩");
}

void record_print_one(const CourseRecord *rec) {
    printf("%-13s %-8s %-20s %-10s %-18s %-5.1f %-8s %04d-%02d-%02d %-5d\n",
           rec->studentId, rec->name, rec->college, rec->courseId,
           rec->courseName, rec->credit, rec->term,
           rec->year, rec->month, rec->day, rec->score);
}


/* ================================================================
 *  二、双向链表实现
 *  尾插 O(1)；查找 / 删除 O(n)。
 * ================================================================ */

typedef struct ListNode {
    CourseRecord rec;
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

typedef struct {
    ListNode *head;
    ListNode *tail;
    int count;
} ListStore;

static Store *list_create(void) {
    ListStore *s = (ListStore *)malloc(sizeof(ListStore));
    s->head = s->tail = NULL;
    s->count = 0;
    return (Store *)s;
}

static void list_destroy(Store *store) {
    ListStore *s = (ListStore *)store;
    ListNode *cur = s->head;
    while (cur != NULL) {
        ListNode *next = cur->next;
        free(cur);
        cur = next;
    }
    free(s);
}

/* 在链表里找到键对应的节点，找不到返回 NULL */
static ListNode *list_find_node(ListStore *s, const char *sid, const char *cid) {
    ListNode *cur = s->head;
    while (cur != NULL) {
        if (same_key(&cur->rec, sid, cid)) return cur;
        cur = cur->next;
    }
    return NULL;
}

/* 尾插。若键已存在则覆盖成绩等字段，不重复插入 */
static int list_insert(Store *store, const CourseRecord *r) {
    ListStore *s = (ListStore *)store;
    ListNode *exist = list_find_node(s, r->studentId, r->courseId);
    ListNode *node;
    if (exist != NULL) { exist->rec = *r; return 1; }

    node = (ListNode *)malloc(sizeof(ListNode));
    node->rec = *r;
    node->next = NULL;
    node->prev = s->tail;
    if (s->tail != NULL) s->tail->next = node;
    else                 s->head = node;
    s->tail = node;
    s->count++;
    return 1;
}

/* 按键删除，并把前后指针接好 */
static int list_remove(Store *store, const char *sid, const char *cid) {
    ListStore *s = (ListStore *)store;
    ListNode *node = list_find_node(s, sid, cid);
    if (node == NULL) return 0;
    if (node->prev != NULL) node->prev->next = node->next;
    else                    s->head = node->next;
    if (node->next != NULL) node->next->prev = node->prev;
    else                    s->tail = node->prev;
    free(node);
    s->count--;
    return 1;
}

static CourseRecord *list_find(Store *store, const char *sid, const char *cid) {
    ListNode *node = list_find_node((ListStore *)store, sid, cid);
    return node ? &node->rec : NULL;
}

/* 把全部记录按顺序拷进 out，返回条数 */
static int list_get_all(Store *store, CourseRecord *out) {
    ListStore *s = (ListStore *)store;
    ListNode *cur = s->head;
    int n = 0;
    while (cur != NULL) {
        out[n++] = cur->rec;
        cur = cur->next;
    }
    return n;
}

static int list_size(Store *store) { return ((ListStore *)store)->count; }

const StoreOps list_ops = {
    "双向链表",
    list_create, list_destroy, list_insert, list_remove,
    list_find, list_get_all, list_size
};


/* ================================================================
 *  三、哈希表实现
 *  链地址法解决冲突；元素太多时扩容（再哈希）。
 *  平均插入 / 查找 / 删除 O(1)。
 * ================================================================ */

typedef struct HashNode {
    CourseRecord rec;
    struct HashNode *next;
} HashNode;

typedef struct {
    HashNode **buckets;
    int bucketCount;
    int count;
} HashStore;

#define HASH_INIT_BUCKETS 17

/* djb2 字符串哈希，对 学号+课程编号 求值，再取模得到桶号 */
static unsigned long hash_key(const char *sid, const char *cid, int bucketCount) {
    unsigned long h = 5381;
    const char *p;
    for (p = sid; *p; p++) h = h * 33 + (unsigned char)(*p);
    for (p = cid; *p; p++) h = h * 33 + (unsigned char)(*p);
    return h % (unsigned long)bucketCount;
}

static Store *hash_create(void) {
    HashStore *s = (HashStore *)malloc(sizeof(HashStore));
    s->bucketCount = HASH_INIT_BUCKETS;
    s->count = 0;
    s->buckets = (HashNode **)calloc(s->bucketCount, sizeof(HashNode *));
    return (Store *)s;
}

static void hash_destroy(Store *store) {
    HashStore *s = (HashStore *)store;
    int i;
    for (i = 0; i < s->bucketCount; i++) {
        HashNode *cur = s->buckets[i];
        while (cur != NULL) {
            HashNode *next = cur->next;
            free(cur);
            cur = next;
        }
    }
    free(s->buckets);
    free(s);
}

/* 扩容：桶数翻倍多一点，把老节点重新散列到新桶 */
static void hash_resize(HashStore *s) {
    int newCount = s->bucketCount * 2 + 1;
    HashNode **newBuckets = (HashNode **)calloc(newCount, sizeof(HashNode *));
    int i;
    for (i = 0; i < s->bucketCount; i++) {
        HashNode *cur = s->buckets[i];
        while (cur != NULL) {
            HashNode *next = cur->next;
            unsigned long idx = hash_key(cur->rec.studentId, cur->rec.courseId, newCount);
            cur->next = newBuckets[idx];
            newBuckets[idx] = cur;
            cur = next;
        }
    }
    free(s->buckets);
    s->buckets = newBuckets;
    s->bucketCount = newCount;
}

/* 插入；键已存在则覆盖。元素数超过桶数的 3/4 时先扩容 */
static int hash_insert(Store *store, const CourseRecord *r) {
    HashStore *s = (HashStore *)store;
    unsigned long idx;
    HashNode *cur, *node;

    if (s->count + 1 > s->bucketCount * 3 / 4)
        hash_resize(s);

    idx = hash_key(r->studentId, r->courseId, s->bucketCount);
    for (cur = s->buckets[idx]; cur != NULL; cur = cur->next) {
        if (same_key(&cur->rec, r->studentId, r->courseId)) {
            cur->rec = *r;          /* 键已存在：覆盖 */
            return 1;
        }
    }
    node = (HashNode *)malloc(sizeof(HashNode));
    node->rec = *r;
    node->next = s->buckets[idx];   /* 头插到链表 */
    s->buckets[idx] = node;
    s->count++;
    return 1;
}

static int hash_remove(Store *store, const char *sid, const char *cid) {
    HashStore *s = (HashStore *)store;
    unsigned long idx = hash_key(sid, cid, s->bucketCount);
    HashNode *cur = s->buckets[idx];
    HashNode *prev = NULL;
    while (cur != NULL) {
        if (same_key(&cur->rec, sid, cid)) {
            if (prev) prev->next = cur->next;
            else      s->buckets[idx] = cur->next;
            free(cur);
            s->count--;
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

static CourseRecord *hash_find(Store *store, const char *sid, const char *cid) {
    HashStore *s = (HashStore *)store;
    unsigned long idx = hash_key(sid, cid, s->bucketCount);
    HashNode *cur = s->buckets[idx];
    while (cur != NULL) {
        if (same_key(&cur->rec, sid, cid)) return &cur->rec;
        cur = cur->next;
    }
    return NULL;
}

/* 逐个桶、逐条链表地把记录拷进 out */
static int hash_get_all(Store *store, CourseRecord *out) {
    HashStore *s = (HashStore *)store;
    int i, n = 0;
    for (i = 0; i < s->bucketCount; i++) {
        HashNode *cur = s->buckets[i];
        while (cur != NULL) {
            out[n++] = cur->rec;
            cur = cur->next;
        }
    }
    return n;
}

static int hash_size(Store *store) { return ((HashStore *)store)->count; }

const StoreOps hash_ops = {
    "哈希表",
    hash_create, hash_destroy, hash_insert, hash_remove,
    hash_find, hash_get_all, hash_size
};


/* ================================================================
 *  四、多条件筛选与多关键字排序
 * ================================================================ */

/* 判断一条记录是否满足全部生效的筛选条件 */
static int match_cond(const CourseRecord *r, const FilterCond *c) {
    if (c->useCourseName) {
        if (c->courseNameFuzzy) {
            if (strstr(r->courseName, c->courseName) == NULL) return 0;  /* 模糊：子串包含 */
        } else {
            if (strcmp(r->courseName, c->courseName) != 0) return 0;      /* 精确 */
        }
    }
    if (c->useTerm && strcmp(r->term, c->term) != 0) return 0;
    if (c->useCollege && strcmp(r->college, c->college) != 0) return 0;
    if (c->useScoreRange && (r->score < c->scoreMin || r->score > c->scoreMax)) return 0;
    return 1;
}

/* 筛选：先取出全部记录，再把满足条件的挑进 *out */
int query_filter(Store *s, const StoreOps *ops, const FilterCond *cond, CourseRecord **out) {
    int total = ops->size(s);
    CourseRecord *all, *res;
    int got, i, n = 0;

    if (total == 0) { *out = NULL; return 0; }

    all = (CourseRecord *)malloc(total * sizeof(CourseRecord));
    res = (CourseRecord *)malloc(total * sizeof(CourseRecord));  /* 最多命中 total 条 */
    got = ops->get_all(s, all);
    for (i = 0; i < got; i++)
        if (match_cond(&all[i], cond))
            res[n++] = all[i];

    free(all);
    *out = res;
    return n;
}

/* qsort 的比较函数没法直接带参数，这里用文件内静态变量存当前排序规则。
 * 单线程命令行程序这样用没问题。 */
static const SortKey *g_keys;
static int g_keyCount;

static int sort_cmp(const void *pa, const void *pb) {
    const CourseRecord *a = (const CourseRecord *)pa;
    const CourseRecord *b = (const CourseRecord *)pb;
    int i;
    for (i = 0; i < g_keyCount; i++) {
        int c = compare_field(a, b, g_keys[i].field);
        if (c != 0)
            return g_keys[i].descending ? -c : c;   /* 降序就把结果取反 */
    }
    return 0;
}

void query_sort(CourseRecord *arr, int n, const SortKey *keys, int keyCount) {
    if (n <= 1 || keyCount <= 0) return;
    g_keys = keys;
    g_keyCount = keyCount;
    qsort(arr, n, sizeof(CourseRecord), sort_cmp);
}


/* ================================================================
 *  五、数据统计分析
 * ================================================================ */

#define MAX_GROUPS 64   /* 分组上限：学院 / 课程都不会超过这个数 */

/* 按字符串字段分组计数。byCollege=1 按学院分组，否则按课程名分组 */
static void group_count(Store *s, const StoreOps *ops, int byCollege,
                        const char *title, const char *keyLabel) {
    int total = ops->size(s);
    CourseRecord *all;
    char keys[MAX_GROUPS][64];
    int counts[MAX_GROUPS];
    int groupNum = 0, got, i, j;

    printf("\n===== %s =====\n", title);
    if (total == 0) { printf("（暂无记录）\n"); return; }

    all = (CourseRecord *)malloc(total * sizeof(CourseRecord));
    got = ops->get_all(s, all);

    for (i = 0; i < got; i++) {
        const char *key = byCollege ? all[i].college : all[i].courseName;
        /* 在已有分组里线性查找 */
        for (j = 0; j < groupNum; j++)
            if (strcmp(keys[j], key) == 0) break;
        if (j < groupNum) {
            counts[j]++;                       /* 找到：计数 +1 */
        } else if (groupNum < MAX_GROUPS) {
            strcpy(keys[groupNum], key);       /* 没找到：新建一个分组 */
            counts[groupNum] = 1;
            groupNum++;
        }
    }

    printf("%-24s %s\n", keyLabel, "选课人数");
    for (j = 0; j < groupNum; j++)
        printf("%-24s %d\n", keys[j], counts[j]);
    printf("（共 %d 个分组）\n", groupNum);

    free(all);
}

void stats_by_course(Store *s, const StoreOps *ops) {
    group_count(s, ops, 0, "每门课程选课人数", "课程名称");
}

void stats_by_college(Store *s, const StoreOps *ops) {
    group_count(s, ops, 1, "各学院选课人数分布", "学院");
}

/* 成绩分布：把全部记录取出来，按分数段累加 */
void stats_by_score(Store *s, const StoreOps *ops) {
    int total = ops->size(s);
    CourseRecord *all;
    int excellent = 0, good = 0, medium = 0, pass = 0, fail = 0;
    int got, i;

    printf("\n===== 成绩分布统计 =====\n");
    if (total == 0) { printf("（暂无记录）\n"); return; }

    all = (CourseRecord *)malloc(total * sizeof(CourseRecord));
    got = ops->get_all(s, all);
    for (i = 0; i < got; i++) {
        int sc = all[i].score;
        if (sc >= 90)      excellent++;
        else if (sc >= 80) good++;
        else if (sc >= 70) medium++;
        else if (sc >= 60) pass++;
        else               fail++;
    }

    printf("优秀 (90-100): %d\n", excellent);
    printf("良好 (80-89) : %d\n", good);
    printf("中等 (70-79) : %d\n", medium);
    printf("及格 (60-69) : %d\n", pass);
    printf("不及格 (<60) : %d\n", fail);
    printf("合计         : %d\n", got);

    free(all);
}


/* ================================================================
 *  六、CSV 持久化
 * ================================================================ */

#define CSV_HEADER "studentId,name,college,courseId,courseName,credit,term,year,month,day,score\n"

/* 把全部记录写成 CSV 文件，第一行是表头 */
int persist_save(Store *s, const StoreOps *ops, const char *path) {
    FILE *fp = fopen(path, "w");
    int total = ops->size(s);
    CourseRecord *all;
    int got, i;

    if (fp == NULL) return -1;
    fputs(CSV_HEADER, fp);

    if (total > 0) {
        all = (CourseRecord *)malloc(total * sizeof(CourseRecord));
        got = ops->get_all(s, all);
        for (i = 0; i < got; i++)
            fprintf(fp, "%s,%s,%s,%s,%s,%.1f,%s,%d,%d,%d,%d\n",
                    all[i].studentId, all[i].name, all[i].college, all[i].courseId,
                    all[i].courseName, all[i].credit, all[i].term,
                    all[i].year, all[i].month, all[i].day, all[i].score);
        free(all);
    } else {
        got = 0;
    }
    fclose(fp);
    return got;
}

/* 从 CSV 文件逐行读入记录。用 sscanf 按逗号拆字段（姓名/学院里没有逗号） */
int persist_load(Store *s, const StoreOps *ops, const char *path) {
    FILE *fp = fopen(path, "r");
    char line[512];
    int n = 0;

    if (fp == NULL) return 0;   /* 文件不存在：首次运行，当作 0 条 */

    while (fgets(line, sizeof(line), fp) != NULL) {
        CourseRecord r;
        int got;
        if (strncmp(line, "studentId", 9) == 0) continue;   /* 跳过表头 */

        got = sscanf(line, "%12[^,],%31[^,],%63[^,],%8[^,],%63[^,],%f,%7[^,],%d,%d,%d,%d",
                     r.studentId, r.name, r.college, r.courseId, r.courseName,
                     &r.credit, r.term, &r.year, &r.month, &r.day, &r.score);
        if (got == 11 && record_is_valid(&r)) {
            ops->insert(s, &r);
            n++;
        }
    }
    fclose(fp);
    return n;
}


/* ================================================================
 *  七、测试数据生成
 *  按任务书附录的规则生成合理的随机记录。
 * ================================================================ */

static const char *k_surnames[] = {
    "赵","钱","孙","李","周","吴","郑","王","冯","陈","褚","卫","蒋","沈","韩","杨"
};
static const char *k_given_names[] = {
    "伟","芳","娜","敏","静","丽","强","磊","军","洋","勇","艳","杰","娟","涛","明","超","霞"
};
#define SURNAME_NUM (int)(sizeof(k_surnames)/sizeof(k_surnames[0]))
#define GIVEN_NUM   (int)(sizeof(k_given_names)/sizeof(k_given_names[0]))

/* 学院名 + 两位学院代码 */
static const char *k_colleges[] = {
    "计算机科学与工程学院", "数学与统计学院", "电子信息工程学院",
    "机械工程学院", "外国语学院", "经济管理学院"
};
static const char *k_college_code[] = { "01", "02", "03", "04", "05", "06" };
#define COLLEGE_NUM 6

/* 课程名 + 对应的 8 位课程编号 */
static const char *k_course_names[] = {
    "数据结构与算法", "操作系统原理", "数据库系统原理", "计算机网络",
    "高等数学", "线性代数", "大学英语", "编译原理", "概率论与数理统计", "软件工程"
};
static const char *k_course_ids[] = {
    "CS300102", "CS300203", "CS300305", "CS300407",
    "MA100101", "MA100202", "FL200101", "CS400108", "MA100303", "CS400209"
};
#define COURSE_NUM 10

/* 每月天数（2 月简化按 28 算） */
static int days_in_month(int month) {
    static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return d[month - 1];
}

/* 生成大致呈正态、集中在中高分段的成绩：几个随机数取平均更靠近中间 */
static int rand_score(void) {
    int sum = 0, i, s;
    for (i = 0; i < 4; i++) sum += 50 + rand() % 51;   /* 4 个 50..100 的数 */
    s = sum / 4;
    if (s > 100) s = 100;
    return s;
}

/* 生成一条合法记录，seq 用于学号里的序号 */
static void gen_one(CourseRecord *r, int seq) {
    int collegeIdx = rand() % COLLEGE_NUM;
    int courseIdx  = rand() % COURSE_NUM;
    int enrollYear = 2020 + rand() % 5;      /* 入学年份 2020-2024 */
    int termYear   = 2020 + rand() % 7;      /* 选课年份 2020-2026 */
    int termSeason = (rand() % 2) ? 2 : 1;   /* 01 春 / 02 秋 */
    int month;

    /* 学号：入学年份(4) + 学院代码(2) + 序号(6) */
    sprintf(r->studentId, "%04d%s%06d", enrollYear, k_college_code[collegeIdx], seq % 1000000);
    /* 姓名：随机姓 + 随机名 */
    sprintf(r->name, "%s%s", k_surnames[rand() % SURNAME_NUM], k_given_names[rand() % GIVEN_NUM]);

    strcpy(r->college, k_colleges[collegeIdx]);
    strcpy(r->courseId, k_course_ids[courseIdx]);
    strcpy(r->courseName, k_course_names[courseIdx]);

    r->credit = 1.0f + 0.5f * (rand() % 7);   /* 1.0 - 4.0，步长 0.5 */
    if (r->credit > 4.0f) r->credit = 4.0f;

    sprintf(r->term, "%04d-%02d", termYear, termSeason);

    /* 选课日期与学期对应：春季在 2-3 月，秋季在 8-9 月 */
    month = (termSeason == 1) ? (2 + rand() % 2) : (8 + rand() % 2);
    r->year  = termYear;
    r->month = month;
    r->day   = 1 + rand() % days_in_month(month);

    r->score = rand_score();
}

void datagen_fill(CourseRecord *arr, int n, unsigned int seed) {
    int i;
    srand(seed);
    for (i = 0; i < n; i++)
        gen_one(&arr[i], i + 1);
}


/* ================================================================
 *  八、多结构性能对比
 *  对同一份数据，用两种结构分别测插入 / 查找 / 删除耗时。
 * ================================================================ */

/* 对一种结构跑一轮测试，打印一行结果 */
static void bench_one(const StoreOps *ops, CourseRecord *data, int n, int testCount) {
    Store *s = ops->create();
    clock_t t0, t1;
    double tInsert, tFind, tDelete, memMb;
    int i;

    /* 插入：全部 n 条 */
    t0 = clock();
    for (i = 0; i < n; i++) ops->insert(s, &data[i]);
    t1 = clock();
    tInsert = (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC;

    /* 查找：取前 testCount 条来查 */
    t0 = clock();
    for (i = 0; i < testCount; i++)
        ops->find_rec(s, data[i].studentId, data[i].courseId);
    t1 = clock();
    tFind = (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC;

    /* 删除：删掉前 testCount 条 */
    t0 = clock();
    for (i = 0; i < testCount; i++)
        ops->remove_rec(s, data[i].studentId, data[i].courseId);
    t1 = clock();
    tDelete = (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC;

    /* 内存粗略估算：每条记录约一个节点大小（含指针开销） */
    memMb = (double)n * (sizeof(CourseRecord) + 24) / (1024.0 * 1024.0);

    printf("%-10s %8d %12.3f %12.3f %12.3f %10.2f\n",
           ops->name, n, tInsert, tFind, tDelete, memMb);

    ops->destroy(s);
}

void benchmark_run(int n, unsigned int seed) {
    CourseRecord *data;
    int testCount;

    data = (CourseRecord *)malloc(n * sizeof(CourseRecord));
    if (data == NULL) { printf("内存不足，无法生成 %d 条数据。\n", n); return; }
    datagen_fill(data, n, seed);

    testCount = n < 1000 ? n : 1000;   /* 查找 / 删除最多测 1000 次，避免链表太慢 */

    printf("\n========== 性能对比测试（n = %d，查找/删除测 %d 次）==========\n", n, testCount);
    printf("%-10s %8s %12s %12s %12s %10s\n",
           "数据结构", "规模", "插入(ms)", "查找(ms)", "删除(ms)", "内存(MB)");
    bench_one(&list_ops, data, n, testCount);
    bench_one(&hash_ops, data, n, testCount);
    printf("说明：链表查找/删除是 O(n)，哈希平均 O(1)；\n");
    printf("      内存为按节点数的粗略估算。\n");

    free(data);
}

#define _CRT_SECURE_NO_WARNINGS
/* 当前存储结构默认用双向链表
 * 把任务 1-6 的功能串起来：增删改查、持久化、筛选排序、统计、批量删除、性能对比。
 */
#include "oms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DATA_FILE "records.csv"

/* 基准日期 2026-09-01，早于 2023-09-01 的记录算过期 */
#define EXPIRE_THRESHOLD 20230901

static Store *g_store;
static const StoreOps *g_ops = &list_ops;   /* 当前活动结构：链表 */


/* 读一行，去掉结尾的换行符 */
static void read_line(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    if (fgets(buf, size, stdin) == NULL) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\r\n")] = '\0';
}

/* 读一个整数 */
static int read_int(const char *prompt) {
    char buf[64];
    read_line(prompt, buf, sizeof(buf));
    return atoi(buf);
}


/* ---------------- 显示全部记录 ---------------- */
static void show_all(void) {
    int total = g_ops->size(g_store);
    CourseRecord *all;
    int got, i, limit;

    if (total == 0) { printf("\n当前没有记录。\n"); return; }

    all = (CourseRecord *)malloc(total * sizeof(CourseRecord));
    got = g_ops->get_all(g_store, all);

    printf("\n当前共 %d 条记录：\n", got);
    record_print_header();
    limit = got < 50 ? got : 50;          /* 最多显示 50 条，避免刷屏 */
    for (i = 0; i < limit; i++) record_print_one(&all[i]);
    if (got > 50) printf("...（仅显示前 50 条）\n");

    free(all);
}


/* ---------------- 任务 1：增删改查 ---------------- */
static void do_insert(void) {
    CourseRecord r;
    char buf[64];
    memset(&r, 0, sizeof(r));
    read_line("学号(12位): ", r.studentId, sizeof(r.studentId));
    read_line("姓名: ", r.name, sizeof(r.name));
    read_line("学院: ", r.college, sizeof(r.college));
    read_line("课程编号(8位): ", r.courseId, sizeof(r.courseId));
    read_line("课程名称: ", r.courseName, sizeof(r.courseName));
    read_line("学分: ", buf, sizeof(buf)); r.credit = (float)atof(buf);
    read_line("学期(如2024-02): ", r.term, sizeof(r.term));
    r.year  = read_int("选课年: ");
    r.month = read_int("选课月: ");
    r.day   = read_int("选课日: ");
    r.score = read_int("成绩(0-100): ");

    if (!record_is_valid(&r)) { printf("× 记录非法，未插入。\n"); return; }
    if (g_ops->insert(g_store, &r)) printf("√ 插入成功。\n");
    else printf("× 插入失败。\n");
}

static void do_remove(void) {
    char sid[13], cid[9];
    read_line("学号: ", sid, sizeof(sid));
    read_line("课程编号: ", cid, sizeof(cid));
    if (g_ops->remove_rec(g_store, sid, cid)) printf("√ 删除成功。\n");
    else printf("× 未找到对应记录。\n");
}

static void do_modify(void) {
    char sid[13], cid[9];
    CourseRecord *r;
    read_line("学号: ", sid, sizeof(sid));
    read_line("课程编号: ", cid, sizeof(cid));
    r = g_ops->find_rec(g_store, sid, cid);
    if (r == NULL) { printf("× 未找到对应记录。\n"); return; }
    record_print_header();
    record_print_one(r);
    r->score = read_int("新成绩(0-100): ");
    printf("√ 修改完成。\n");
}

static void do_find(void) {
    char sid[13], cid[9];
    CourseRecord *r;
    read_line("学号: ", sid, sizeof(sid));
    read_line("课程编号: ", cid, sizeof(cid));
    r = g_ops->find_rec(g_store, sid, cid);
    if (r == NULL) { printf("× 未找到。\n"); return; }
    record_print_header();
    record_print_one(r);
}


/* ---------------- 任务 3（1）：多条件筛选 ---------------- */
static void do_filter(void) {
    FilterCond cond;
    CourseRecord *out = NULL;
    char buf[64];
    int n, i;
    memset(&cond, 0, sizeof(cond));

    read_line("课程名(模糊匹配，回车跳过): ", buf, sizeof(buf));
    if (buf[0]) { cond.useCourseName = 1; cond.courseNameFuzzy = 1; strcpy(cond.courseName, buf); }

    read_line("学期(精确，如2024-02，回车跳过): ", buf, sizeof(buf));
    if (buf[0]) { cond.useTerm = 1; strcpy(cond.term, buf); }

    read_line("学院(精确，回车跳过): ", buf, sizeof(buf));
    if (buf[0]) { cond.useCollege = 1; strcpy(cond.college, buf); }

    read_line("成绩下限(回车跳过): ", buf, sizeof(buf));
    if (buf[0]) {
        cond.useScoreRange = 1;
        cond.scoreMin = atoi(buf);
        read_line("成绩上限: ", buf, sizeof(buf));
        cond.scoreMax = buf[0] ? atoi(buf) : 100;
    }

    n = query_filter(g_store, g_ops, &cond, &out);
    printf("\n命中 %d 条：\n", n);
    if (n > 0) {
        record_print_header();
        for (i = 0; i < n && i < 50; i++) record_print_one(&out[i]);

        /* 筛选结果支持导出为文件 */
        read_line("导出文件名(回车不导出): ", buf, sizeof(buf));
        if (buf[0]) {
            FILE *fp = fopen(buf, "w");
            if (fp) {
                fprintf(fp, "studentId,name,college,courseId,courseName,credit,term,year,month,day,score\n");
                for (i = 0; i < n; i++)
                    fprintf(fp, "%s,%s,%s,%s,%s,%.1f,%s,%d,%d,%d,%d\n",
                            out[i].studentId, out[i].name, out[i].college, out[i].courseId,
                            out[i].courseName, out[i].credit, out[i].term,
                            out[i].year, out[i].month, out[i].day, out[i].score);
                fclose(fp);
                printf("√ 已导出到 %s\n", buf);
            } else {
                printf("× 导出失败。\n");
            }
        }
    }
    free(out);
}


/* ---------------- 任务 3（2）：多关键字排序 ---------------- */

/* 把字段名字符串转成枚举，不认识返回 -1 */
static FieldId parse_field(const char *s) {
    if (strcmp(s, "studentId") == 0)  return FIELD_STUDENT_ID;
    if (strcmp(s, "name") == 0)       return FIELD_NAME;
    if (strcmp(s, "college") == 0)    return FIELD_COLLEGE;
    if (strcmp(s, "courseId") == 0)   return FIELD_COURSE_ID;
    if (strcmp(s, "courseName") == 0) return FIELD_COURSE_NAME;
    if (strcmp(s, "credit") == 0)     return FIELD_CREDIT;
    if (strcmp(s, "term") == 0)       return FIELD_TERM;
    if (strcmp(s, "date") == 0)       return FIELD_DATE;
    if (strcmp(s, "score") == 0)      return FIELD_SCORE;
    return (FieldId)-1;
}

static void do_sort(void) {
    FilterCond empty;
    CourseRecord *out = NULL;
    SortKey keys[MAX_SORT_KEYS];
    char buf[128];
    int n, i, keyCount = 0;
    char *tok;

    memset(&empty, 0, sizeof(empty));
    n = query_filter(g_store, g_ops, &empty, &out);   /* 取出全部记录 */

    printf("可用字段: studentId name college courseId courseName credit term date score\n");
    read_line("排序规则(如 score:desc studentId:asc): ", buf, sizeof(buf));

    /* 按空格切成若干个 "字段:方向" */
    tok = strtok(buf, " ");
    while (tok != NULL && keyCount < MAX_SORT_KEYS) {
        char *colon = strchr(tok, ':');
        FieldId f;
        int desc = 0;
        if (colon) {
            *colon = '\0';
            desc = (strcmp(colon + 1, "desc") == 0);
        }
        f = parse_field(tok);
        if ((int)f >= 0) {
            keys[keyCount].field = f;
            keys[keyCount].descending = desc;
            keyCount++;
        }
        tok = strtok(NULL, " ");
    }

    query_sort(out, n, keys, keyCount);
    printf("\n排序结果（前 50 条 / 共 %d）：\n", n);
    record_print_header();
    for (i = 0; i < n && i < 50; i++) record_print_one(&out[i]);
    free(out);
}


/* ---------------- 任务 4：统计分析 ---------------- */
static void do_stats(void) {
    int c = read_int("1.按课程  2.按学院  3.成绩分布 : ");
    if (c == 1) stats_by_course(g_store, g_ops);
    else if (c == 2) stats_by_college(g_store, g_ops);
    else if (c == 3) stats_by_score(g_store, g_ops);
    else printf("无效选项。\n");
}


/* ---------------- 任务 5：批量删除过期记录 ---------------- */
static void do_expire_delete(void) {
    int total = g_ops->size(g_store);
    CourseRecord *all, *expired;
    int got, i, ecount = 0;
    char buf[8];

    if (total == 0) { printf("\n当前没有记录。\n"); return; }

    all     = (CourseRecord *)malloc(total * sizeof(CourseRecord));
    expired = (CourseRecord *)malloc(total * sizeof(CourseRecord));   /* 最多全部过期 */
    got = g_ops->get_all(g_store, all);

    /* 先挑出所有过期记录（以选课日期为准，不是学期） */
    for (i = 0; i < got; i++)
        if (date_to_int(all[i].year, all[i].month, all[i].day) < EXPIRE_THRESHOLD)
            expired[ecount++] = all[i];

    printf("\n基准日期 2026-09-01，将删除选课日期早于 2023-09-01 的记录。\n");
    printf("共发现 %d 条过期记录。\n", ecount);

    if (ecount == 0) {
        printf("无过期记录，无需删除。\n");   /* 边界情况：没有过期记录 */
    } else {
        read_line("确认删除? (y/n): ", buf, sizeof(buf));
        if (buf[0] == 'y' || buf[0] == 'Y') {
            for (i = 0; i < ecount; i++)
                g_ops->remove_rec(g_store, expired[i].studentId, expired[i].courseId);
            printf("√ 已删除 %d 条过期记录，当前剩余 %d 条。\n", ecount, g_ops->size(g_store));
        } else {
            printf("已取消。\n");
        }
    }

    free(all);
    free(expired);
}


/* ---------------- 任务 6：性能对比 ---------------- */
static void do_benchmark(void) {
    int n = read_int("测试数据规模(如 100 / 1000 / 10000): ");
    if (n <= 0) { printf("规模非法。\n"); return; }
    benchmark_run(n, 12345u);
}


/* ---------------- 生成测试数据 ---------------- */
static void do_generate(void) {
    int n = read_int("生成记录条数: ");
    CourseRecord *data;
    int i, ok = 0;
    if (n <= 0) { printf("条数非法。\n"); return; }
    data = (CourseRecord *)malloc(n * sizeof(CourseRecord));
    if (data == NULL) { printf("内存不足。\n"); return; }
    datagen_fill(data, n, 2024u);
    for (i = 0; i < n; i++)
        if (g_ops->insert(g_store, &data[i])) ok++;
    free(data);
    printf("√ 已生成并载入（去重后当前共 %d 条）。\n", g_ops->size(g_store));
}


/* ---------------- 任务 2：保存 ---------------- */
static void do_save(void) {
    int n = persist_save(g_store, g_ops, DATA_FILE);
    if (n >= 0) printf("√ 已保存 %d 条到 %s\n", n, DATA_FILE);
    else printf("× 保存失败。\n");
}


/* ---------------- 切换存储结构 ----------------
 * 运行时在 双向链表 / 哈希表 之间切换
 *   1. 从旧结构取出全部记录
 *   2. 新建目标结构，把记录逐条搬进去（顺便计时，直观看到重建速度差异）
 *   3. 销毁旧结构，切换全局的 g_store / g_ops
 * 只底层换了实现
 */
static void do_switch(void) {
    const StoreOps *targets[2] = { &list_ops, &hash_ops };
    const StoreOps *newOps;
    Store *newStore;
    CourseRecord *all = NULL;
    clock_t t0, t1;
    double ms;
    int choice, total, got = 0, i, migrated = 0;

    printf("\n可切换的存储结构：\n");
    printf(" 1. 双向链表    2. 哈希表\n");
    printf("（当前：%s）\n", g_ops->name);
    choice = read_int("切换到(1/2): ");
    if (choice < 1 || choice > 2) { printf("无效选项。\n"); return; }
    newOps = targets[choice - 1];

    if (newOps == g_ops) {
        printf("当前已经是【%s】，无需切换。\n", g_ops->name);
        return;
    }

    /* 1. 取出旧结构里的全部记录 */
    total = g_ops->size(g_store);
    if (total > 0) {
        all = (CourseRecord *)malloc(total * sizeof(CourseRecord));
        if (all == NULL) { printf("内存不足，切换取消。\n"); return; }
        got = g_ops->get_all(g_store, all);
    }

    /* 2. 建新结构并搬数据，计时 */
    newStore = newOps->create();
    if (newStore == NULL) { printf("新结构创建失败，切换取消。\n"); free(all); return; }

    t0 = clock();
    for (i = 0; i < got; i++)
        if (newOps->insert(newStore, &all[i])) migrated++;
    t1 = clock();
    ms = (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC;

    /* 3. 销毁旧结构，切换全局指针 */
    g_ops->destroy(g_store);
    g_store = newStore;
    g_ops = newOps;
    free(all);

    printf("√ 已切换到【%s】，迁移 %d 条记录，重建耗时 %.3f ms。\n",
           g_ops->name, migrated, ms);
}


/* ---------------- 菜单 ---------------- */
static void menu(void) {
    printf("\n========= 校园选课记录检索与大数据分析系统 =========\n");
    printf("当前存储结构: %s    记录数: %d\n", g_ops->name, g_ops->size(g_store));
    printf(" 1. 显示全部记录        2. 插入记录\n");
    printf(" 3. 删除记录            4. 修改成绩\n");
    printf(" 5. 精确查找            6. 多条件筛选(可导出)\n");
    printf(" 7. 多关键字排序        8. 统计分析\n");
    printf(" 9. 批量删除过期记录   10. 性能对比测试\n");
    printf("11. 生成测试数据       12. 保存\n");
    printf("13. 切换存储结构\n");
    printf(" 0. 保存并退出\n");
    printf("====================================================\n");
}

int main(void) {
    int loaded, choice;
    char buf[16];

    g_store = g_ops->create();
    if (g_store == NULL) { printf("初始化失败。\n"); return 1; }

    loaded = persist_load(g_store, g_ops, DATA_FILE);
    printf("启动：从 %s 载入 %d 条记录。\n", DATA_FILE, loaded);

    while (1) {
        menu();
        read_line("请选择: ", buf, sizeof(buf));
        choice = atoi(buf);
        switch (choice) {
            case 1:  show_all();         break;
            case 2:  do_insert();        break;
            case 3:  do_remove();        break;
            case 4:  do_modify();        break;
            case 5:  do_find();          break;
            case 6:  do_filter();        break;
            case 7:  do_sort();          break;
            case 8:  do_stats();         break;
            case 9:  do_expire_delete(); break;
            case 10: do_benchmark();     break;
            case 11: do_generate();      break;
            case 12: do_save();          break;
            case 13: do_switch();        break;
            case 0:
                do_save();
                g_ops->destroy(g_store);
                printf("再见！\n");
                return 0;
            default:
                printf("无效选项，请重试。\n");
        }
    }
}

/* 性能对比的全部声明集中到一起。只用到 C 标准库*/
#ifndef OMS_H
#define OMS_H

/* ================================================================
 *  一、选课记录数据类型
 * ================================================================ */

/* 一条选课记录。学号 + 课程编号 共同构成唯一键。 */
typedef struct {
    char  studentId[13];   /* 学号：12 位数字 + '\0' */
    char  name[32];        /* 姓名 */
    char  college[64];     /* 学院 */
    char  courseId[9];     /* 课程编号：8 位 + '\0' */
    char  courseName[64];  /* 课程名称 */
    float credit;          /* 学分 */
    char  term[8];         /* 选课学期，如 "2024-02" */
    int   year;            /* 选课日期：年 */
    int   month;           /* 选课日期：月 */
    int   day;             /* 选课日期：日 */
    int   score;           /* 成绩 0-100 */
} CourseRecord;

/* 可排序 / 筛选的字段编号 */
typedef enum {
    FIELD_STUDENT_ID = 0,
    FIELD_NAME,
    FIELD_COLLEGE,
    FIELD_COURSE_ID,
    FIELD_COURSE_NAME,
    FIELD_CREDIT,
    FIELD_TERM,
    FIELD_DATE,
    FIELD_SCORE
} FieldId;

/* 把日期折算成可比较的整数：year*10000 + month*100 + day */
int  date_to_int(int year, int month, int day);

/* 校验一条记录是否合法。合法返回 1，非法返回 0。 */
int  record_is_valid(const CourseRecord *rec);

/* 打印表头 / 打印一条记录 */
void record_print_header(void);
void record_print_one(const CourseRecord *rec);


/* ================================================================
 *  二、统一存储接口（用函数指针实现解耦）
 *
 *  上层业务只调用下面这张 StoreOps 表，不关心底层到底是链表、
 *  AVL 树还是哈希表。三种结构各自填一张表（list_ops / avl_ops /
 *  hash_ops）。Store 只是一个通用指针，真正的结构体定义在 oms.c 里。
 * ================================================================ */

typedef struct Store Store;   /* 通用句柄，具体类型对上层隐藏 */

typedef struct {
    const char *name;                                                /* 结构名称，用于打印 */
    Store *(*create)(void);                                          /* 创建空结构 */
    void   (*destroy)(Store *s);                                     /* 销毁释放 */
    int    (*insert)(Store *s, const CourseRecord *r);               /* 插入，成功返回 1 */
    int    (*remove_rec)(Store *s, const char *sid, const char *cid);/* 删除单条，删到返回 1 */
    CourseRecord *(*find_rec)(Store *s, const char *sid, const char *cid); /* 查找，返回指针或 NULL */
    int    (*get_all)(Store *s, CourseRecord *out);                  /* 把全部记录拷进 out，返回条数 */
    int    (*size)(Store *s);                                        /* 当前记录数 */
} StoreOps;

/* 三
两种实现的操作表 */
extern const StoreOps list_ops;
extern const StoreOps hash_ops;


/* ================================================================
 *  三、多条件筛选与多关键字排序
 * ================================================================ */

#define MAX_SORT_KEYS 4

/* 单个排序关键字：字段 + 方向 */
typedef struct {
    FieldId field;
    int descending;   /* 1 降序，0 升序 */
} SortKey;

/* 多条件筛选条件，各 useXxx 为 1 时该条件才生效 */
typedef struct {
    int  useCourseName;  char courseName[64]; int courseNameFuzzy; /* 1 模糊匹配 */
    int  useTerm;        char term[8];
    int  useCollege;     char college[64];
    int  useScoreRange;  int scoreMin, scoreMax;
} FilterCond;

/* 筛选：把满足条件的记录拷进 *out（函数内 malloc，调用者负责 free）。
 * 返回命中条数。 */
int  query_filter(Store *s, const StoreOps *ops, const FilterCond *cond, CourseRecord **out);

/* 多关键字排序：按 keys[0..keyCount-1] 的优先级排序 arr。 */
void query_sort(CourseRecord *arr, int n, const SortKey *keys, int keyCount);


/* ================================================================
 *  四、数据统计分析
 * ================================================================ */

/* 每门课程选课人数（按课程名分组计数） */
void stats_by_course(Store *s, const StoreOps *ops);

/* 各学院选课人数分布（按学院分组计数） */
void stats_by_college(Store *s, const StoreOps *ops);

/* 成绩分布：优(90-100)/良(80-89)/中(70-79)/及格(60-69)/不及格(<60) */
void stats_by_score(Store *s, const StoreOps *ops);


/* ================================================================
 *  五、数据持久化（CSV 格式）
 *
 *  选 CSV 的理由：字段固定、结构简单，能直接用记事本/Excel 打开查看，
 *  方便人工构造测试数据，万级规模解析也够快。
 * ================================================================ */

/* 把存储结构里的全部记录写入 CSV 文件。成功返回写出条数，失败返回 -1。 */
int  persist_save(Store *s, const StoreOps *ops, const char *path);

/* 从 CSV 文件读取记录并插入存储结构。成功返回读入条数，文件不存在返回 0。 */
int  persist_load(Store *s, const StoreOps *ops, const char *path);


/* ================================================================
 *  六、测试数据生成
 * ================================================================ */

/* 生成 n 条合法的随机选课记录到 arr（arr 需预先分配 n 个元素）。
 * seed 为随机种子，便于复现。 */
void datagen_fill(CourseRecord *arr, int n, unsigned int seed);


/* ================================================================
 *  七、多结构性能对比
 * ================================================================ */

/* 在规模 n 下，对链表 / 哈希表分别测插入、查找、删除耗时并打印对比表。
 * （AVL 实现仍保留在 oms.c，如需三方对比可自行加回 bench_one 调用。） */
void benchmark_run(int n, unsigned int seed);

#endif /* OMS_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Print the usage information
 *
 */
void usage() {
    printf("Usage: ./csim -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
}

/**
 * @brief Parse the arguments. It will store them into *s, *E, *b, the file.
 *
 * @return int 0 means parsed successfully
 */
int parse_cmd(int args, char **argv, int *s, int *E, int *b, char *filename) {
    int flag[4] = {0};
    int flag_num = 4;

    for (int i = 0; i < args; i++) {
        char *str = argv[i];
        if (str[0] == '-') {
            if (str[1] == 's' && i < args) {
                i++;
                sscanf(argv[i], "%d", s);
                flag[0] = 1;
            } else if (str[1] == 'E' && i < args) {
                i++;
                sscanf(argv[i], "%d", E);
                flag[1] = 1;
            } else if (str[1] == 'b' && i < args) {
                i++;
                sscanf(argv[i], "%d", b);
                flag[2] = 1;
            } else if (str[1] == 't' && i < args) {
                i++;
                sscanf(argv[i], "%s", filename);
                flag[3] = 1;
            }
        }
    }
    for (int i = 0; i < flag_num; i++) {
        if (flag[i] == 0) {
            printf("./csim: Missing required command line argument\n");
            usage();
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Print the final string.
 *
 */
void printSummary(int hits, int misses, int evictions) {
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

/**
 * @brief Read and parse one line from the file-ptr trace.
 *
 * @return int 0 for read and parsed successfully
 */
int readline(FILE *trace, char *op, unsigned long long *address, int *request_length) {
    char str[30];
    if (fgets(str, 30, trace) == NULL) {
        return -1;
    }
    sscanf(str, " %c %llx,%d", op, address, request_length);

    return 0;
}

#pragma region Structures-And-Functions

// [1/4] Your code for definition of structures, global variables or functions
// Cache行结构
typedef struct {
    int valid;              // 有效位
    unsigned long long tag; // 标记位
    int lru_counter;        // LRU计数器
} cache_line_t;

// Cache组结构
typedef struct {
    cache_line_t *lines;    // 指向该组的所有行
} cache_set_t;

// Cache结构
typedef struct {
    int S;                  // 组数 (2^s)
    int E;                  // 每组行数
    int B;                  // 块大小 (2^b)
    cache_set_t *sets;      // 指向所有组
} cache_t;

// 全局变量
cache_t data_cache;     // 数据缓存
cache_t inst_cache;     // 指令缓存
int hits = 0, misses = 0, evictions = 0;
int lru_time = 0;  // 全局时间戳，用于LRU

// 最近访问的指令地址（用于顺序预测）
unsigned long long recent_inst_addrs[3] = {0};
int recent_count = 0;

// 初始化cache
void init_cache(cache_t *cache, int s, int E, int b) {
    cache->S = 1 << s;  // 2^s
    cache->E = E;
    cache->B = 1 << b;  // 2^b
    
    // 分配组数组
    cache->sets = (cache_set_t*)malloc(cache->S * sizeof(cache_set_t));
    
    // 为每组分配行数组
    for (int i = 0; i < cache->S; i++) {
        cache->sets[i].lines = (cache_line_t*)malloc(E * sizeof(cache_line_t));
        // 初始化每行
        for (int j = 0; j < E; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].lru_counter = 0;
        }
    }
}

// 释放cache内存
void free_cache(cache_t *cache) {
    for (int i = 0; i < cache->S; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
}

// 访问cache
void access_cache(cache_t *cache, unsigned long long address, int s, int b, int count_stats) {
    // 提取标记位和组索引
    unsigned long long tag = address >> (s + b);
    unsigned long long set_index = (address >> b) & ((1 << s) - 1);
    
    cache_set_t *set = &cache->sets[set_index];
    
    // 查找是否命中
    for (int i = 0; i < cache->E; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            // 命中
            hits++;
            set->lines[i].lru_counter = lru_time++;
            return;
        }
    }
    
    // 未命中
    if (count_stats) misses++;
    
    // 查找空行
    for (int i = 0; i < cache->E; i++) {
        if (!set->lines[i].valid) {
            // 找到空行，直接放入
            set->lines[i].valid = 1;
            set->lines[i].tag = tag;
            set->lines[i].lru_counter = lru_time++;
            return;
        }
    }
    
    // 没有空行，需要替换（LRU）
    if (count_stats) evictions++;
    
    // 找到LRU行（lru_counter最小的）
    int lru_index = 0;
    for (int i = 1; i < cache->E; i++) {
        if (set->lines[i].lru_counter < set->lines[lru_index].lru_counter) {
            lru_index = i;
        }
    }
    
    // 替换LRU行
    set->lines[lru_index].tag = tag;
    set->lines[lru_index].lru_counter = lru_time++;
}
// 更新最近访问的指令地址
void update_recent_inst_addrs(unsigned long long address) {
    // 左移数组
    for (int i = 2; i > 0; i--) {
        recent_inst_addrs[i] = recent_inst_addrs[i-1];
    }
    recent_inst_addrs[0] = address;
    if (recent_count < 3) recent_count++;
}

// 检查是否需要进行顺序预测
int should_predict(unsigned long long address) {
    // 检查是否在缓存行右边界（每个缓存行32字节，4条指令）
    // 指令地址8字节对齐，所以缓存行内偏移为0,8,16,24
    // 右边界即偏移为24的位置
    unsigned long long block_offset = address & 0x1F; // 低5位
    return (block_offset == 24); // 0x18 = 24
}

// 检查最近3条指令是否连续递增
int is_sequential_pattern() {
    if (recent_count < 3) return 0;
    
    // 检查是否连续且递增（每条指令8字节）
    return (recent_inst_addrs[1] == recent_inst_addrs[0] + 8) &&
           (recent_inst_addrs[2] == recent_inst_addrs[1] + 8);
}

// 进行顺序预测
void sequential_predict(unsigned long long current_addr) {
    if (should_predict(current_addr) && is_sequential_pattern()) {
        unsigned long long next_addr = current_addr + 8;
        // 预测下一条指令，计入统计
        access_cache(&inst_cache, next_addr, 3, 5, 1);
    }
}
#pragma endregion

int main(int args, char **argv) {

#pragma region Parse
    int s, E, b;
    char filename[1024];
    if (parse_cmd(args, argv, &s, &E, &b, filename) != 0) {
        return 0;
    }
#pragma endregion

#pragma region Cache-Init

    // [2/4] Your code for initialzing your cache
    // You can use variables s, E and b directly.
    init_cache(&data_cache, s, E, b);
    // 初始化指令缓存（固定参数：s=3, E=2, b=5）
    init_cache(&inst_cache, 3, 2, 5);
#pragma endregion

#pragma region Handle-Trace
    FILE *trace = fopen(filename, "r");
    char op;
    unsigned long long address;
    int request_length;

    int preload_phase = 1; // 是否在预载阶段

    while (readline(trace, &op, &address, &request_length) != -1) {
        // [3/4] Your code for handling the trace line
        if (op == 'P') {
            // 预载指令，不计入统计
            access_cache(&inst_cache, address, 3, 5, 0);
            update_recent_inst_addrs(address);
        } else {
            // 结束预载阶段
            if (preload_phase) {
                preload_phase = 0;
                // 重置最近访问记录
                recent_count = 0;
            }
            switch (op) {
                case 'L':  // Load
                case 'S':  // Store
                    access_cache(&data_cache, address, s, b, 1);
                    break;
                case 'M':  // Modify (Load + Store)
                    access_cache(&data_cache, address, s, b, 1);  // Load
                    access_cache(&data_cache, address, s, b, 1);  // Store
                    break;
                case 'I':  // Instruction fetch
                    access_cache(&inst_cache, address, 3, 5, 1);
                    update_recent_inst_addrs(address);
                    // 进行顺序预测
                    sequential_predict(address);
                    break;
            }
        }
    }

#pragma endregion

    // [4/4] Your code to output the hits, misses and evictions
    printSummary(hits, misses, evictions);

    // Maybe you can 'free' your cache here
    free_cache(&data_cache);
    free_cache(&inst_cache);
    return 0;
}

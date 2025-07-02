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
cache_t cache;
int hits = 0, misses = 0, evictions = 0;
int lru_time = 0;  // 全局时间戳，用于LRU

// 初始化cache
void init_cache(int s, int E, int b) {
    cache.S = 1 << s;  // 2^s
    cache.E = E;
    cache.B = 1 << b;  // 2^b
    
    // 分配组数组
    cache.sets = (cache_set_t*)malloc(cache.S * sizeof(cache_set_t));
    
    // 为每组分配行数组
    for (int i = 0; i < cache.S; i++) {
        cache.sets[i].lines = (cache_line_t*)malloc(E * sizeof(cache_line_t));
        // 初始化每行
        for (int j = 0; j < E; j++) {
            cache.sets[i].lines[j].valid = 0;
            cache.sets[i].lines[j].tag = 0;
            cache.sets[i].lines[j].lru_counter = 0;
        }
    }
}

// 释放cache内存
void free_cache() {
    for (int i = 0; i < cache.S; i++) {
        free(cache.sets[i].lines);
    }
    free(cache.sets);
}

// 访问cache
void access_cache(unsigned long long address, int s, int b) {
    // 提取标记位和组索引
    unsigned long long tag = address >> (s + b);
    unsigned long long set_index = (address >> b) & ((1 << s) - 1);
    
    cache_set_t *set = &cache.sets[set_index];
    
    // 查找是否命中
    for (int i = 0; i < cache.E; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            // 命中
            hits++;
            set->lines[i].lru_counter = lru_time++;
            return;
        }
    }
    
    // 未命中
    misses++;
    
    // 查找空行
    for (int i = 0; i < cache.E; i++) {
        if (!set->lines[i].valid) {
            // 找到空行，直接放入
            set->lines[i].valid = 1;
            set->lines[i].tag = tag;
            set->lines[i].lru_counter = lru_time++;
            return;
        }
    }
    
    // 没有空行，需要替换（LRU）
    evictions++;
    
    // 找到LRU行（lru_counter最小的）
    int lru_index = 0;
    for (int i = 1; i < cache.E; i++) {
        if (set->lines[i].lru_counter < set->lines[lru_index].lru_counter) {
            lru_index = i;
        }
    }
    
    // 替换LRU行
    set->lines[lru_index].tag = tag;
    set->lines[lru_index].lru_counter = lru_time++;
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
    init_cache(s, E, b);
#pragma endregion

#pragma region Handle-Trace
    FILE *trace = fopen(filename, "r");
    char op;
    unsigned long long address;
    int request_length;

    while (readline(trace, &op, &address, &request_length) != -1) {
        // [3/4] Your code for handling the trace line
        switch (op) {
            case 'L':  // Load
            case 'S':  // Store
                access_cache(address, s, b);
                break;
            case 'M':  // Modify (Load + Store)
                access_cache(address, s, b);  // Load
                access_cache(address, s, b);  // Store
                break;
            case 'I':  // Instruction fetch (ignore)
                break;
        }
    }

#pragma endregion

    // [4/4] Your code to output the hits, misses and evictions
    printSummary(hits, misses, evictions);

    // Maybe you can 'free' your cache here
    free_cache();
    return 0;
}

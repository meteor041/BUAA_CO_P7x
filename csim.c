#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void access_cache(unsigned long long address, int s, int b, int first);
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
void printSummary(int hits, int misses, int evictions, 
        int inMSHRs, int processMSHRs) {
    printf("hits:%d misses:%d evictions:%d inMSHRs:%d processMSHRs:%d\n", 
        hits, misses, evictions, inMSHRs, processMSHRs);

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
    unsigned long long lru_counter;        // LRU计数器
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

typedef struct cache_miss_node {
    int valid;
    unsigned long long block_addr;
    int cnt;
    unsigned long long *block_offset;
    struct cache_miss_node *next;
} cache_miss_t;

typedef struct {
    cache_miss_t *head;
    cache_miss_t *tail;
    int size;
} MSHRs;


// 全局变量
cache_t cache;
MSHRs mshr;
int hits = 0, misses = 0, evictions = 0;
int inMSHRs = 0, processMSHRs = 0;
unsigned long long lru_time = 0;  // 全局时间戳，用于LRU
int mshr_size = 0;

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

void init_MSHRs() {
    mshr.head = NULL;
    mshr.tail = NULL;
    mshr.size = 0;
}

// 释放cache内存
void free_cache() {
    for (int i = 0; i < cache.S; i++) {
        free(cache.sets[i].lines);
    }
    free(cache.sets);
}

void free_MSHRs() {
    cache_miss_t *current = mshr.head;
    while (current != NULL) {
        cache_miss_t *next = current->next;
        free(current->block_offset);
        free(current);
        current = next;
    }
    mshr.head = NULL;
    mshr.tail = NULL;
    mshr.size = 0;
}

void process_miss(cache_set_t *set, unsigned long long tag) {
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

void clear_mshr(int s, int b) {
    if (mshr.head == NULL) {
        return; // MSHRs为空，无需处理
    }
    
    processMSHRs++;
    
    // 按照申请顺序（链表顺序）处理每个Block Miss块
    cache_miss_t *current = mshr.head;
    while (current != NULL) {
        /* 第一次访问：把整个块装入 cache（可能逐出） */
        unsigned long long first_offset = current->block_offset[0];
        unsigned long long address = ((current->block_addr) << b) | first_offset;
        // access_cache(address, s, b, 1);   /* 含 eviction++ */
        // 提取标记位和组索引
        unsigned long long tag = address >> (s + b);
        unsigned long long set_index = (address >> b) & ((1 << s) - 1);
        
        cache_set_t *set = &cache.sets[set_index];
        
        int flag = 0;
        // 查找是否命中
        for (int i = 0; i < cache.E; i++) {
            if (set->lines[i].valid && set->lines[i].tag == tag) {
                // 命中
                hits++;
                set->lines[i].lru_counter = lru_time++;
                flag = 1;
                break;
            }
        }
        if (!flag) {
            misses++;
            process_miss(set, tag);
        }
        // 按照存入顺序处理Block Miss块中的每条指令
        for (int k = 1; k < current->cnt; k++) {
            // 这些一定会hit
            unsigned long long address = ((current->block_addr) << b) | current->block_offset[k];
            unsigned long long tag = address >> (s + b);
            unsigned long long set_index = (address >> b) & ((1 << s) - 1);
            
            cache_set_t *set = &cache.sets[set_index];
            
            int flag = 0;
            // 查找是否命中
            for (int i = 0; i < cache.E; i++) {
                if (set->lines[i].valid && set->lines[i].tag == tag) {
                    // 命中
                    hits++;
                    set->lines[i].lru_counter = lru_time++;
                    flag = 1;
                    break;
                }
            }
        }
        current = current->next;
    }
    
    // 清空MSHRs
    free_MSHRs();
    init_MSHRs();
    mshr_size = 0;
}

void addMSHRs(unsigned long long address, int s, int b, int first) {
    if (mshr_size >= 20) {
        clear_mshr(s, b);
        access_cache(address, s, b, 0);
        return;
    }
    
    // 查找是否已存在对应的Block Miss块 (Secondary Miss)
    cache_miss_t *current = mshr.head;
    unsigned long long block_addr = address >> b;
    unsigned long long block_offset = address & (1 << b - 1);
    while (current != NULL) {
        if (current->block_addr == block_addr) {
            // 找到对应的Block Miss块，添加指令 (Secondary Miss)
            if (mshr_size >= 20) {
                clear_mshr(s, b);
                access_cache(address, s, b, 0);
                return;
            }
            mshr_size++; // Secondary Miss只需要1条信息
            current->block_offset[current->cnt++] = block_offset;
            inMSHRs++;
            return;
        }
        current = current->next;
    }
    
    // 未找到对应MSHR的miss块,申请一个 (Primary Miss)
    if (mshr_size >= 19) { // Primary Miss需要2条信息
        clear_mshr(s, b);
        access_cache(address, s, b, 0);
        return;
    }
    
    inMSHRs++;           /* 指令进 MSHR 计一次 */
    // 创建新的Block Miss块
    cache_miss_t *new_block = (cache_miss_t *)malloc(sizeof(cache_miss_t));
    new_block->valid = 1;
    new_block->block_addr = block_addr;
    new_block->cnt = 0;
    new_block->block_offset = (unsigned long long *)malloc(20 * sizeof(unsigned long long));
    new_block->next = NULL;
    
    // 添加指令到新块
    new_block->block_offset[new_block->cnt++] = block_offset;
    
    // 将新块添加到链表尾部（保持申请顺序）
    if (mshr.head == NULL) {
        mshr.head = new_block;
        mshr.tail = new_block;
    } else {
        mshr.tail->next = new_block;
        mshr.tail = new_block;
    }
    
    mshr_size += 2; // Primary Miss需要2条信息
}

// 访问cache
void access_cache(unsigned long long address, int s, int b, int first) {
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
    addMSHRs(address, s, b, first);
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
    init_MSHRs();
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
                access_cache(address, s, b, 1);
                break;
            case 'M':  // Modify (Load + Store)
                access_cache(address, s, b, 1);  // Load
                access_cache(address, s, b, 1);  // Store
                break;
            case 'I':  // Instruction fetch (ignore)
                break;
        }
    }
    if (mshr.head != NULL) {
        clear_mshr(s, b);
    }
#pragma endregion

    // [4/4] Your code to output the hits, misses and evictions
    printSummary(hits, misses, evictions, inMSHRs, processMSHRs);

    // Maybe you can 'free' your cache here
    free_cache();
    free_MSHRs();
    return 0;
}

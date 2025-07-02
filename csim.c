#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Print the usage information
 *
 */
void usage() {
    printf("Usage: ./csim -t <file>\n");
    printf("Options:\n");
    printf("  -t <file>  Trace file.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  linux>  ./csim -t traces/yi.trace\n");
}

/**
 * @brief Parse the arguments. It will store them into *s, *E, *b, the file.
 *
 * @return int 0 means parsed successfully
 */
int parse_cmd(int args, char **argv, char *filename) {
    int flag_num = 1;
    int flag[1] = {0};

    for (int i = 0; i < args; i++) {
        char *str = argv[i];
        if (str[0] == '-') {
            if (str[1] == 't' && i < args) {
                i++;
                sscanf(argv[i], "%s", filename);
                flag[0] = 1;
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
void printSummary(int layer, int hits, int misses, int evictions) {
    printf("L%d: hits:%d misses:%d evictions:%d\n", layer, hits, misses, evictions);
}

/**
 * @brief Read and parse one line from the file-ptr trace.
 *
 * @return int 0 for read and parsed successfully
 */
int readline(FILE *trace, int *core, char *op, unsigned long long *address, int *request_length) {
    char str[30];
    if (fgets(str, 30, trace) == NULL) {
        return -1;
    }
    sscanf(str, "%d %c %llx,%d", core, op, address, request_length);

    return 0;
}

// [1/4] 你可以在这里定义全局变量、结构、函数等
typedef struct {
    int valid;
    unsigned long long tag;
    int lru_counter;
} cache_line_t;

typedef struct {
    cache_line_t *lines;
} cache_set_t;

typedef struct {
    int S;
    int E;
    int B;
    cache_set_t *sets;
} cache_t;

cache_t cache_group[4];
cache_t l2_cache;
int l1_hits = 0, l1_misses = 0, l1_evictions = 0;
int l2_hits = 0, l2_misses = 0, l2_evictions = 0;
int lru_time = 0;

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

void free_cache(cache_t *cache) {
    for (int i = 0; i < cache->S; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
}

void fill_l1_cache(cache_t *cache, cache_set_t *set, unsigned long long tag) {
    // 查找L1Cache空行
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
    l1_evictions++;
    
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

void fill_l2_cache(cache_set_t *set, unsigned long long tag) {
    for (int i = 0; i < l2_cache.E; i++) {
        if (!set->lines[i].valid) {
            // 找到空行，直接放入
            set->lines[i].valid = 1;
            set->lines[i].tag = tag;
            set->lines[i].lru_counter = lru_time++;
            return;
        }
    }
    
    // 没有空行，需要替换（LRU）
    l2_evictions++;
    
    // 找到LRU行（lru_counter最小的）
    int lru_index = 0;
    for (int i = 1; i < l2_cache.E; i++) {
        if (set->lines[i].lru_counter < set->lines[lru_index].lru_counter) {
            lru_index = i;
        }
    }
    
    // 替换LRU行
    set->lines[lru_index].tag = tag;
    set->lines[lru_index].lru_counter = lru_time++;
}

void access_cache(cache_t *cache, unsigned long long address, int s, int b) {
    // 提取标记位和组索引
    unsigned long long tag = address >> (s + b);
    unsigned long long set_index = (address >> b) & ((1 << s) - 1);
    
    cache_set_t *set = &cache->sets[set_index];
    
    // 查找L1Cache是否命中
    for (int i = 0; i < cache->E; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            // 命中
            l1_hits++;
            set->lines[i].lru_counter = lru_time++;
            return;
        }
    }
    
    // L1Cache未命中
    l1_misses++;
    
    unsigned long long l2_tag = address >> (6 + 6);
    unsigned long long l2_set_index = (address >> 6) & ((1 << 6) - 1);
    cache_set_t *l2_set = &l2_cache.sets[l2_set_index];
    for (int i = 0; i < l2_cache.E; i++) {
        if (l2_set->lines[i].valid && l2_set->lines[i].tag == l2_tag) {
            // l2命中
            l2_hits++;
            l2_set->lines[i].lru_counter = lru_time++;
            fill_l1_cache(cache, set, tag);
            return;
        }
    }
    
    // L2Cache未命中
    l2_misses++;

    fill_l1_cache(cache, set, tag);
    fill_l2_cache(l2_set, l2_tag);

}

int main(int args, char **argv) {

    // 解析命令行参数
    char filename[1024];
    if (parse_cmd(args, argv, filename) != 0) {
        return 0;
    }

    // [2/4] 初始化 Cache
    for (int i = 0; i < 4; i++) {
        init_cache(&cache_group[i], 4, 8, 4);

    }
    init_cache(&l2_cache, 6, 8, 6);

    // 读取 Trace 行
    FILE *trace = fopen(filename, "r");
    int core;
    char op;
    unsigned long long addr;
    int request_length;

    while (readline(trace, &core, &op, &addr, &request_length) != -1) {
        // [3/4] 处理trace行
        switch (op) {
            case 'L':  // Load
            case 'S':  // Store
                access_cache(&cache_group[core], addr, 4, 4);
                break;
            case 'M':  // Modify (Load + Store)
                access_cache(&cache_group[core], addr, 4, 4);  // Load
                access_cache(&cache_group[core], addr, 4, 4);  // Store
                break;
        }
    }

    // [4/4] 分别输出L1和L2的 hits, misses 与 evictions
    printSummary(1, l1_hits, l1_misses, l1_evictions);
    printSummary(2, l2_hits, l2_misses, l2_evictions);

    // 也许你可以在这里进行 'free'
    for (int i = 0; i < 4; i++) {
        free_cache(&cache_group[i]);
    }
    free_cache(&l2_cache);
    return 0;
}

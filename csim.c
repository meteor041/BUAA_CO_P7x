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

int main(int args, char **argv) {

    // 解析命令行参数
    char filename[1024];
    if (parse_cmd(args, argv, filename) != 0) {
        return 0;
    }

    // [2/4] 初始化 Cache


    // 读取 Trace 行
    FILE *trace = fopen(filename, "r");
    int core;
    char op;
    unsigned long long addr;
    int request_length;

    while (readline(trace, &core, &op, &addr, &request_length) != -1) {
        // [3/4] 处理trace行
    }

    // [4/4] 分别输出L1和L2的 hits, misses 与 evictions
    printSummary(1, hits1, misses1, evictions1);
    printSummary(2, hits2, misses2, evictions2);

    // 也许你可以在这里进行 'free'

    return 0;
}

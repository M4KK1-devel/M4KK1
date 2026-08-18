#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 测试基本数据类型
int test_basic_types() {
    char c = 'A';
    short s = 1234;
    int i = 56789;
    long l = 123456789L;
    
    if (c != 'A' || s != 1234 || i != 56789 || l != 123456789L) {
        printf("FAIL: Basic types test\n");
        return 1;
    }
    printf("PASS: Basic types test\n");
    return 0;
}

// 测试指针和数组
int test_pointers_arrays() {
    int arr[5] = {1, 2, 3, 4, 5};
    int *ptr = arr;
    
    if (ptr[0] != 1 || ptr[4] != 5) {
        printf("FAIL: Pointers/arrays test\n");
        return 1;
    }
    
    // 测试指针算术
    ptr++;
    if (*ptr != 2) {
        printf("FAIL: Pointer arithmetic test\n");
        return 1;
    }
    
    printf("PASS: Pointers/arrays test\n");
    return 0;
}

// 测试结构体
struct Point {
    int x;
    int y;
};

int test_structs() {
    struct Point p1 = {10, 20};
    struct Point *p2 = &p1;
    
    if (p2->x != 10 || p2->y != 20) {
        printf("FAIL: Structs test\n");
        return 1;
    }
    
    printf("PASS: Structs test\n");
    return 0;
}

// 测试函数调用和递归
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int test_functions() {
    if (factorial(5) != 120) {
        printf("FAIL: Functions test\n");
        return 1;
    }
    
    printf("PASS: Functions test\n");
    return 0;
}

// 测试字符串操作
int test_strings() {
    char str1[20] = "Hello";
    char str2[20] = "World";
    char result[40];
    
    strcpy(result, str1);
    strcat(result, " ");
    strcat(result, str2);
    
    if (strcmp(result, "Hello World") != 0) {
        printf("FAIL: Strings test\n");
        return 1;
    }
    
    printf("PASS: Strings test\n");
    return 0;
}

// 测试内存分配
int test_memory() {
    int *arr = (int *)malloc(10 * sizeof(int));
    if (arr == NULL) {
        printf("FAIL: Memory allocation test\n");
        return 1;
    }
    
    for (int i = 0; i < 10; i++) {
        arr[i] = i * 10;
    }
    
    if (arr[5] != 50) {
        printf("FAIL: Memory access test\n");
        free(arr);
        return 1;
    }
    
    free(arr);
    printf("PASS: Memory test\n");
    return 0;
}

int main() {
    printf("=== PCC Self-Hosting Test Suite ===\n");
    
    int failures = 0;
    failures += test_basic_types();
    failures += test_pointers_arrays();
    failures += test_structs();
    failures += test_functions();
    failures += test_strings();
    failures += test_memory();
    
    printf("\n=== Test Results ===\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
        printf("PCC self-hosting capability verified.\n");
        return 0;
    } else {
        printf("%d test(s) FAILED\n", failures);
        return 1;
    }
}

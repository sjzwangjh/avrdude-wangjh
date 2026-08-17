/*
 * picpart.c - PIC 器件信息查询实现
 * 使用嵌入式常量数据（来自 pic10-12-16-init.xml 的静态编译），无需运行时 XML 文件。
 *
 * 索引号分配规则：
 *   索引 0 = PIC12F629
 *   索引 1 = PIC12F675
 *   ...
 *   最大索引 = PIC_DEVICE_COUNT - 1
 *
 * 调用流程：
 *   1. 直接调用 pic_devcode("PIC12F629") 获取索引
 *   2. 直接调用 pic_get_name_by_index(0) 获取器件名
 *   3. 无需初始化/释放
 */

#include "picpart.h"
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

 /* 嵌入式器件名称表 - 由 pic10-12-16-init.xml 生成 */
#include "pic_devicenames.inc"

uint16_t pic_find_index_by_name(const char* name) {
    if (!name) return 0xFFFF;

    for (uint16_t i = 0; i < PIC_DEVICE_COUNT; i++) {
        if (strcasecmp(pic_device_names[i], name) == 0)
            return i;
    }
    return 0xFFFF;
}

const char* pic_get_name_by_index(uint16_t index) {
    if (index >= PIC_DEVICE_COUNT)
        return NULL;
    return pic_device_names[index];
}

uint16_t pic_get_device_count(void) {
    return PIC_DEVICE_COUNT;
}

/*
 * pic_devcode() - 给 stk500v2_set_device_id() 调用
 *
 * 根据器件 ID 返回 PIC 索引号。
 * 输入 part_id: 例如 "PIC12F629"、"p12f629"
 * 返回: 16 位索引号 (0 开始)，0xFFFF 表示未知器件
 *
 * 查找规则：
 *   1. 先完整匹配（区分大小写不敏感）
 *   2. 如果以 'p' 开头，去掉 'p' 加 "PIC" 前缀重新匹配
 */
uint16_t pic_devcode(const char* part_id) {
    if (!part_id) return 0xFFFF;

    /* 直接完整匹配 */
    uint16_t idx = pic_find_index_by_name(part_id);
    if (idx != 0xFFFF)
        return idx;

    /* 去前缀匹配：如果以 'p' 开头，去掉 'p' 并加 "PIC" 前缀 */
    if (part_id[0] == 'p' || part_id[0] == 'P') {
        const char* rest = part_id + 1;
        size_t rest_len = strlen(rest);
        /* 构建 "PIC" + rest */
        char pic_name[64];
        if (rest_len + 3 < sizeof(pic_name)) {
            pic_name[0] = 'P';
            pic_name[1] = 'I';
            pic_name[2] = 'C';
            memcpy(pic_name + 3, rest, rest_len + 1);
            return pic_find_index_by_name(pic_name);
        }
    }

    return 0xFFFF;
}

int pic_is_pic_part(const char* part_id) {
    return pic_devcode(part_id) != 0xFFFF;
}


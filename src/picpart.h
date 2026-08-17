/*
 * picpart.h - PIC 器件信息查询接口
 * 从 pic10-12-16-init.xml 提取 PIC 器件名称列表，按出现顺序分配索引号。
 */

#ifndef __PICPART_H__
#define __PICPART_H__

#include <stdint.h>

 /* PIC 器件查询函数（使用嵌入式常量表，无需外部文件） */

 /* 通过器件名查找 PIC 索引号，返回 0xFFFF 表示未找到 */
uint16_t pic_find_index_by_name(const char* name);

/* 通过索引号获取器件名，返回 NULL 表示无效索引 */
const char* pic_get_name_by_index(uint16_t index);

/* 获取 PIC 器件总数 */
uint16_t pic_get_device_count(void);

/* pic_devcode() - 给 stk500v2_set_device_id() 调用
 * 根据器件 ID 返回 PIC 索引号。
 * 输入: "PIC12F629" 或 "p12f629"
 * 返回: 16 位索引号，0xFFFF 表示未知器件
 */
uint16_t pic_devcode(const char* part_id);

/* Check whether a part id is a known PIC device (pic_devcode != 0xFFFF) */
int pic_is_pic_part(const char* part_id);

#endif /* PICPART_H */




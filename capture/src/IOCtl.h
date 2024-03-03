#pragma once

#define CHANGE_QUEUE_SIZE          50000

// op_type
#define  OP_TEXTOUT        1
#define  OP_BITBLT         2
#define  OP_COPYBITS       3
#define  OP_STROKEPATH     4
#define  OP_LINETO         5
#define  OP_FILLPATH       6
#define  OP_STROKEANDFILLPATH  7
#define  OP_STRETCHBLT     8
#define  OP_ALPHABLEND     9
#define  OP_TRANSPARENTBLT 10
#define  OP_GRADIENTFILL   11
#define  OP_PLGBLT         12
#define  OP_STRETCHBLTROP  13
#define  OP_RENDERHINT     14

#define ESCAPE_CODE_MAP_USERBUFFER     0x0DAACC00
#define ESCAPE_CODE_UNMAP_USERBUFFER   0x0DAACC10

struct draw_change_t
{
	unsigned int    op_type; /// 各种GDI操作
	RECT            rect;    /// 发生变化的区域
};

struct draw_queue_t
{
	unsigned int       next_index;                 /// 下一个将要绘制的位置, 从 0到 ( CHANGE_QUEUE_SIZE - 1 ),循环队列指示器
	draw_change_t      draw_queue[CHANGE_QUEUE_SIZE];    
};

struct draw_buffer_t
{
	char                magic[16];   
	unsigned int        cx;          ///屏幕长度
	unsigned int        cy;          ///屏幕高度
	unsigned int        line_bytes;  ///每个扫描行的实际数据长度
	unsigned int        line_stride; ///每个扫描行的4字节对齐的数据长度
	unsigned int        bitcount;    ///8,16,24,32 
	draw_queue_t        changes;     ///发生变化的矩形框集合
	unsigned int        data_length; //data_buffer指向的屏幕RGB数据长度，等于 line_stride*cy
	char                data_buffer[1]; //存放屏幕数据区域
};



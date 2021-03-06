# lab1-3 实验报告

## 汇编代码思想与略解

```
.section .init
.global _start
_start:
ldr r0, =0x3F200000 @ r0, GPIO base
mov r2, #1
lsl r2, #27
str r2, [r0, #8] @ set output mode
lsl r2, #2 @ r2, Open switch value
```
这若干代码与助教给出的教程相同——打开 LED 的输出模式。但尚未点亮灯，只是将用来点亮的数值存入了 r2 以待后续使用。
```
ldr r1, =0x3F003000 @ r1, 1MHZ counter base
ldr r4, =0x000F4240 @ r4, 1 second
```
这部分准备好相关数据：r1 指向时钟计数器基地址，r4 存储一秒有多少 1MHz 周期——即 1,000,000 的十六进制。
```
loop3:
str r2, [r0, #28] @ enable LED
ldr r3, [r1, #4] 
add r3, r3, r4 @ 1 second after
loop1:@ poll until 1 second pass
ldr r5, [r1, #4]
cmp r3, r5
bhs loop1
str r2, [r0, #40] @ disable LED
ldr r3, [r1, #4] 
add r3, r3, r4 @ 1 second after
loop2:@ poll until 1 second pass
ldr r5, [r1, #4]
cmp r3, r5
bhs loop2
b loop3
```
开始循环，立刻点亮LED灯，轮询判断是否经过了一秒（loop1的部分）；然后熄灭LED灯，再轮询一秒，然后重新回到loop3继续。灯闪烁一秒，熄灭一秒，不断循环，永远不会停止。

需要解释的是：
- 根据助教给出的教程，关闭位在 GPIO 40-48 字节，但是没有指明具体是哪个字节控制LED灯。我采用的是对称性猜测：既然 GPIO 20-28 字节控制打开，那么在对称对应的位置，也应该是控制LED关闭的地方。也就是，我仍旧在用 r2 存的值来控制LED的关闭，与控制打开的几乎一样的指令使用格式
- 计数器每一微秒递增1，在这期间，足以执行许多条指令了——就算是时钟频率不高的树莓派（默认700MHz）。这意味着我们轮询的时候，每次判断 cmp 指令的时候，计数器要么与上次相比不变，要么最多多了1——我们的计时十分精确了。
- 我没有考虑时间计数器后四字节 32 位值上溢的情况（从最大值突变回0），虽然这应该会导致程序异常，但是在这种四千多秒出现一次的事件面前，无视它也可以很好完成本实验了。

## 探究问题
- 在本实验中，第一分区还有以下文件是重要的：start.elf, bootcode.bin, config.txt。这些都是启动树莓派所必需的程序，本实验的第一部分已经详解了树莓派启动的各个部分即所需的各个文件了，这里就不再讲了。
- 本实验没有用到第二分区。第二分区里装载的是 linux 内核，但 kernel7.img 被我们的闪灯汇编程序替代了，它并不会加载 cmdline.txt，不会启动 init 程序，不会启动操作系统。
- 分别是：汇编器，链接器，复制兼格式转换。汇编器将检查我们的汇编程序，它有无语法错误，有无诸如行标签异常、立即数不可表示之类的错误，并将我们的程序翻译为 objective file 内含机器码。链接器可以将多个 .o 文件链接起来形成一个最终的汇编结果。objcopy 将一个目标文件的内容拷贝到另外一个目标文件当中，并实现文件格式的转换——比如我们这里把 .o 文件变成了 .img 文件。一般来讲，标准的程序编译过程只要用到汇编器和链接器。
# lab1-2 实验报告

## 首先回答一下问题探究
- 有 bootcode.bin, cmdline.txt, config.txt, start.elf。其中除了 cmdline.txt 的另外三个都是为了树莓派的启动的各个阶段所需要的文件，这已经在 lab1-1 中讲过了，这里就不谈了。而要提 cmdline.txt 的作用，就要看一下他的内容：`dwc_otg.lpm_enable=0 console=serial0,115200 console=tty1 root=PARTUUID=7ee80803-02 rootfstype=ext4 elevator=deadline fsck.repair=yes rootwait quiet init=/init`。其中最后一句话很有思考意义：它设置了操作系统启动的时候打开的第一个进程 init 。综述来讲，cmdline.txt 设置了 linux kernel 启动时的一些选项，包括文件系统 ext4，启动的第一个进程 init，分区 UUID 等等关键信息。
- 第一个分区用到的文件系统是 fat32，这在 lab1-1 的调研中提到了，树莓派会寻找sd卡中的第一个 fat32 分区，获得关键启动文件如bootcode.bin ，config.txt ，start.elf，这都是树莓派设定好的，不能使用其他文件系统。
- 加载第二个分区的 init 程序是由第一个分区里的 cmdline.txt 里最后一句 `init=/init` 设定的，对于标准 linux 启动过程，init 程序是第一个启动的进程，将用来初始化操作系统，用它来调用操作系统启动的其他进程，完成操作系统的启动。
- 至少打开几个编译选项难以回答，本人只是尝试尽可能减小选项数目，第二个大部分将探讨。
- init 程序一行退出，linux kernel panic 并报错：“attempted to kill init! ” init程序即是 linux 第一个启动的进程，也是它关闭时最后一个退出的进程，linux 期望它在linux运行期间一直保持运行，用它完成linux的退出后它才能结束执行。

## 本人对 linux kernel 编译选项精简的尝试

本人关闭编译选项有以下几个判断依据：
- 对于默认为 M 的选项，且没有以它为父选项的子 * 模式编译选项，则 M 编译选项可以关闭—— M 选项得到的是操作系统内核模块，我们的 init 都没有实际上启动操作系统，当然不需要这些模块了。
- 对于为 * 模式选项但确认无用的，可以关闭。
- 对于其他似可有似可无的，尝试关闭稍有理由的并运行在树莓派上实验它。

接下来逐选项解释为何从默认选项里关掉它（或保留它）：
- Virtualization 默认关闭
- Library routines 关闭全部可关闭内容。init 程序中使用的如此基本的函数没有必要这些高级算法。
- Cryptographic API 关闭全部可关闭内容。init 不需要密码学算法。
- Security options 默认全部子选项关闭
- Kernel hacking 里保留了 Compile-time checks and compiler options——>Make section mismatch errors non-fatal，这是为了交叉编译通过；还保留了 Magic SysRq key，因为对于这个不知可以不可以删除的选项，去掉之后无法点亮屏幕（但灯还可以闪烁），所以对于这个难以解释的选项选择了保留。其他诸如debugging，reporting 等等一些支持反馈的、debug的、追踪监视的选项都是可以去掉的。
- File systems，只保留且必须保留ext4主选项，其他自选项都全部关闭（如Ext4 POSIX Access Control Lists，Use ext4 for ext2 file systems，Ext4 Security Labels），我们保留 ext4 主要是为了支持启动 init 。当然，这些选项（包括下面十分神似 fat32 的 MS-DOS，VFAT 之类的选项）都经过了删除实验后确定去掉是可行的，甚至包括本地语言里的 ASCII 和 English（其他语言本就是一定可以去掉的）。
- Firmware Drivers 保持默认，当然也应该是必要的。
- Devices 比较复杂，我保留的有“Generic Driver Options ——> Select only drivers that don't need compile-time external firmware”，“Prevent firmware from being built”，“DMA Contiguous Memory Allocator”；还有关于LED，SD卡，基本的字符图形输出设备选项，BCM2835 DMA Engine，Mailbox等的一些选项。事实上，当我们删掉那些一定不必要的：输入设备（键盘鼠标等），多媒体设备，音箱，PWM，传感器，实时时钟，USB，Hardware Monitoring，多语言等，剩下的选项并不多，可以在排除显然不能动的选项（SD卡，LED）之后一个个实验它们，就可以确认比如 Watchdog timer等这类设备也是可以去掉的，而mailbox这种看似“邮箱”实则十分关键的东西是不能去掉的。
- Networking support 当然是可以全去掉的。
- Power management options 也都去掉了，在这个指导思想下，其他选项卡里的关于节能、控能、监控功率的选项也都去掉了。
- Userspace binary formats 中关于 debugging 的 “Enable core dump support” 和第二个选项直接去掉了；为 M 的倒数第二个选项也去掉了；由于我们没有scripts，“Kernel support for scripts starting with #!” 也去掉了；而第一个选项实测是不能去掉的。
- Floating point emulation 根据编译器要求只保留一个
- CPU Power Management 全部关掉。我们不必节能。
- Boot options 默认没有什么可更改的。
- Kernel Features 里，第一个关于多核心的选项可以去掉，还有 “Use kernel mem{cpy,set}() for {copy_to,clear}user()”，“Enable seccomp to safely compute untrusted bytecode”这种安全性或者修改函数调用的可以去掉。“Memory allocator for compressed pages” 去掉因为我们没有做压缩（虽然此话略缺乏依据，但实测是可行的）。
- Bus support 默认全关。
- System type 里基本都是树莓派核心芯片的支持，基本没有做什么改动。
- Enable the block layer 里本是全部关闭可以使亮灯正常的（这大多数是对磁盘的支持），但是屏幕居然会无法显示。最终只保留了一个选项 “Partition Types——>Advanced partition selection——>PC BIOS (MSDOS partition tables) support” ，而且这个选项的 Helps 也很 “精炼”：“Say Y here.” 关于I/O schedule的内容都关掉了，因为显然不需要 “调度” 。
- Enable loadable module support 里保留第二个选项，并打开其子选项，关闭了其他选项。这主要是为了编译不报错。
- 终于到了 General setup。其中一个最关键的选项是压缩方式 “Kernel compression mode (LZMA)”，这可以使最后的 zImage 缩小好多。“Support for paging of anonymous memory (swap)” 应当确认可以删掉，因为根据 Help ，扩大存储不是我们的目的。高级系统调用一类的 “Enable process_vm_readv/writev syscalls” 可去掉。“Namespaces support” “Automatic process group scheduling” 应该也是可去掉的，毕竟我们只有一个 init，没什么别的多于进程，不涉及重用或调度。下面还有一些设计系统调用的都可以去掉。之后，剩的选项又不多，就可以进行实验了。

完整的设置选项可以参照本实验 files 文件夹里一个额外的 config 文件进行对照。
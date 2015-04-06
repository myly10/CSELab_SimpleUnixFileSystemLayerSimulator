# CSELab_SimpleUnixFileSystemLayerSimulator
Lab 1 of CSE in Fudan University

计算机系统工程 LAB1
—— UNIX FILE SYSTEM LAYERING AND NAMING
发布时间： 2015-3-26
DEADLINE： 2015-4-09 （卓越班为 4-16）
目的：
了解 UNIX 文件系统的基本架构，深入理解文件系统的 NAMING LAYER 和相关 API 的具体实
现
要求：
1. 阅读理解教材第二章第五节的内容（ Case study: unix® file system layering and naming）
2. 阅读理解项目 SimpleUnixFileSystemLayerSimulator 的框架代码。
该框架包含了 UNIX 文件系统的各层接口，并通过读写 diskblocks.data 文件模拟了 BLOCK
层的相关操作。 diskblocks.data 是一个二进制流文件，我们通过对该文件不同位置的读写，
模拟了 BLOCK 层对磁盘的读写。
在 SimpleUnixFileSystemLayerSimulator.cpp 中，我们定义了一系列的宏，这些宏决定了文
件系统对 blocks 的区域划分（详见教材 P94 Figure2.20），其中包含了 block 的数量以及大
小,bootblock， superblock， bitmap for free blocks， inode table 以及 file blocks 等各个区域的起
始位置和大小， inode 的数据结构和数量等信息。
3. 根据教材第二章第五节，以 SimpleUnixFileSystemLayerSimulator 框架为基础，在 block 读
写操作函数（ WRITE_BLOCK_BY_BLOCK_NUMBER 和 BLOCK_NUMBER_TO_BLOCK） 的基础上，实现 unix 文
件系统各个 Layer 中的函数。
4. 添 加 额 外 功 能 函 数 ， 并 修 改 main 函 数 中 的 代 码 ， 使 得 最 终 编 译 出 的
SimpleUnixFileSystemLayerSimulator.exe 能够支持如下命令行参数，简单起见，之后的 fs.exe
均指代 SimpleUnixFileSystemLayerSimulator.exe：
ls 指令，列出某个目录下的文件和子目录
>fs.exe ls <某个目录的绝对路径>
export 指令，从文件系统中提取某个文件及其内容，并以原文件名保存在 fs.exe 所在的同一
目录下
>fs.exe export <某个文件的绝对路径>
tree 指令，生成某个目录的目录树，须包含该目录的全部子目录和文件，以及子目录下的目
录和文件，以及子目录的子目录下的目录和文件，以及子目录的子目录的子目录下的目录和
文件。。。依次类推，学过数据结构的同学会知道这个叫做递归。。。 orz
>fs.exe tree <某个目录的绝对路径>
Sample： 下图第七行开始为“ fs.exe tree /”指令的输出，也就是从根目录开始打印目录树。
+前缀表示目录， -前缀表示文件，中间是文件 /目录名称，名称后的括号为对应的
inode_number，如果是文件则最后还须包括该文件的大小。每进入一层子目录，均增加两个
空格的缩进。
find 指令，在某个目录下查询所有名称为某关键字的目录和文件
>fs.exe find <某个目录的绝对路径> <要查询的关键字>
5. 如果命令行参数中的路径不存在， 或出现其它错误， 需给出合理提示。
6. simpledisk.data 中包含了一个简单的目录结构， 同学们可以以此来调试自己的程序。
diskblocks.data 模拟文件中包含了比较多的文件和目录，且 inode number 和 block 并不连续，
同学们可以以此来验证你们的代码是否正确。如果你觉得各 layer 函数的参数和返回值设置
不够合理，可以自行修改，但尽量保证函数名不变，并写好注释
7. 关键步骤务必添加注释

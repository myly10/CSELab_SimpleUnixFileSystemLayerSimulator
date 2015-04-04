
卓越班同学的第二阶段任务：
你们需要在文档1的基础上完成如下内容：



新指令

createFile 在某个特定目录B下创建新的空文件A

>fs.exe createFile <目录B的路径> <空文件A的文件名>



insertFile 导入某个文件A到特定目录B下，参数1为目标路径，参数2为要添加的文件A的在系统中的绝对路径

>fs.exe insertFile <目录B的路径> <被导入文件A在系统中的路径>



createDirecotory 在某个特定目录B下添加子目录C

>fs.exe createDirecotory <目标B的路径> <要添加的目录C的名称>




提示：
1.因为文件系统是持久化存储设备，所以每次更新操作（添加文件或者目录）都需要将结果写回到diskblock.data文件，以使再次执行fs.exe时能够看到这些修改。

2.为了添加新的目录和文件，你需要编写一些函数查找空闲的inode和block，我们不对查找策略进行要求，你可以从恰当的起始位置开始，选取第一个空闲的inode或者block

3.每次申请了新的block后，请将bitmap for free blocks相应的bit设置为1，表示占用。



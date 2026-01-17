# NanoRedis: 一个极简的类Redis内存数据库

NanoRedis 是一个用于学习的迷你内存数据库. 我主要实现了两个核心模块: 一个基于 Extendible Hashing 的 DashTable 数据结构,  以及一套 Shared-Nothing 的多核并发架构. 

## HashTable

首先编写任何程序,  首先就是 “数据结构”的设计,  redis 这种内存数据库,  最主要的存储结构就是 Hashtable,  Hashtable 就是为了把一个个key 映射到 内存固定的槽位,  能实现 O(1) 查找, 主要有两个挑战:  1. 如何解决 “冲突” 2. 如何解决动态扩展(rehash),  redis 中使用传统的拉链式解决冲突,  使用两表切换来解决扩展问题,  是非常标准的做法.  并且经过优化之后,  性能也非常不错; 但是我的实现为了能探索其他解决方案,  所以选择了完全不同的方案. 

**hash 冲突解决**,  我们没有实现拉链式方式,  这种被证明在负载因子较高的时候,  对 cache line不够优化,   双字典切换的方式**rehash方案**也同样被证明非常消耗资源, 影响面也非常大; 我们这里的数据结构设计,  希望能达到这样一种效果,  首先大部分申请的内存都是整齐的, 匹配 cache line; 尽量使用批量的数组 类型的结构,  而不是 指针/链表类的数据结构,  我们最终选择了 ankerl::unordered_dense 作为 hashtable,  这是一个类似 absl flat_hash 的实现,  不会产生链表,  注意我们只是用来解决hash冲突,  并不使用它的扩容,  它申请之后就是固定大小的,  不会改变大小;  关于动态扩容,  这里是学习的 Dragonfly DashTable 中的设计,  当然我们没有直接使用它的代码,  它的代码非常复杂,  我们借用了它的多 Segemnt 设计,  使用 global_depth/local_depth 来动态扩展增长 Segemnt,  这会让系统 Rehash 过程控制在一个很小的范围内,  大部分 key 不会收到影响

## DashTable

DashTable 的核心思想可以用一句话概括: **通过一个可动态扩展的目录（Directory）来索引多个独立的数据段（Segment）**. 

整体结构是这样的: 最上层是一个 Segment Directory,  它是一个指针数组,  每个指针指向一个 Segment. 每个 Segment 内部就是一个普通的哈希表,  存储实际的键值对. 当你要查找一个 key 时,  先通过 key 的哈希值定位到对应的 Segment,  然后在这个 Segment 内部进行查找. 有一个关键的设计决策: **使用哈希值的高位来计算 Segment 索引**. 比如当 `global_depth` 为 3 时,  我取哈希值的最高 3 位作为索引. 具体实现就是 `hash >> (64 - global_depth_)`. 使用高位而不是低位有几个好处: 高位在扩容时更稳定,  而且 DragonflyDB 也是这么做的,  这让调试变得容易一些. Extendible Hashing 的精髓在于 **Global Depth** 和 **Local Depth** 这两个概念. Global Depth 决定了目录的大小（总共有 2^global_depth 个槽位）,  而 Local Depth 决定了有多少个目录槽位指向同一个 Segment. 比如一个 Segment 的 local_depth 是 2,  而 global_depth 是 3,  那么就有 2^(3-2) = 2 个目录槽位指向这个 Segment. 这种设计的妙处在于: **不同的 Segment 可以有不同的 Local Depth**. 数据分布不均匀的热点 Segment 可以多分裂几次（local_depth 更高）,  而冷门的 Segment 保持原样. 这避免了传统哈希表"一刀切"式扩容的浪费. 当某个 Segment 的负载超过 80% 时,  触发分裂操作. 分裂的过程也很直接: 创建一个新的 Segment,  把原来 Segment 中大约一半的元素移过去,  然后更新目录中的指针. 如果分裂时发现 Local Depth 已经等于 Global Depth,  就先扩展目录（大小翻倍）,  再进行分裂. 

说起来简单,  但实现时有几个坑. 比如扩展目录时,  需要从后往前遍历并复制指针,  否则会覆盖还没处理的数据.  还有,  分裂时要先收集所有需要移动的元素,  再统一移动,  不能边遍历边修改. 

DashTable 的底层哈希表,  而是直接用了 `ankerl::unordered_dense::map`. 这是一个基于开放寻址的高性能哈希表,  比标准库的 `std::unordered_map` 快不少. DashTable 最终提供了标准的 `Insert`、`Find`、`Erase` 接口,  复杂度都是 O(1). 但更重要的是,  它的扩容开销从 O(n) 降到了 O(k),  k 是单个 Segment 的大小. 

## NanoObj

我们需要一个通用的容器, 能够包装所有这些类型, 同时还要尽可能紧凑高效. NanoObj 的核心设计目标只有一个:**16 字节**. 为什么是 16 字节? 因为这是一个很好的平衡点——既足够小(内存友好, 缓存友好), 又足够大(能存下大部分常见数据). 要在 16 字节里塞下字符串、整数、以及各种复杂类型的指针, 需要一些精巧的设计. 

整体思路是用一个 **Tag + Union** 的结构. 第一个字节 `taglen_` 既是类型标签, 又是内联字符串的长度;第二个字节 `flag_` 是位图标志(目前保留);剩下的 14 字节是一个联合体 `union`, 根据 tag 的不同可以解释为不同的数据. 比如当 `taglen_` 在 0-14 之间时, 表示这是一个内联字符串, 长度就是 `taglen_` 的值, 数据直接存在这 14 字节里. 当 `taglen_` 是 15 时, 表示这是一个整数, 14 字节里存的是 `int64_t`. 当 `taglen_` 是 16 时, 表示这是一个小字符串, 14 字节里存的是指针、长度和前缀. 

字符串的存储策略是自适应的. 对于 14 字节以内的短字符串, 直接内联存储, 不需要任何堆分配——这是最快的路径. 对于超过 14 字节的字符串, 使用 `SmallString` 结构:8 字节指针指向堆上的实际字符串, 2 字节存长度(最大支持 64KB, 对 Redis 来说够用了), 还有 4 字节存前缀. 这个前缀是个小优化:在哈希表中比较两个字符串时, 可以先比较前缀, 如果前缀都不一样就能快速失败, 避免访问堆内存. 整数的处理更直接. `int64_t` 只需要 8 字节, 直接塞进联合体里, 虽然会浪费 6 字节, 但这是联合体的代价, 无法避免. 不过这里有个有趣的优化:对于键(key), 我会尝试自动转换为整数. 比如你存一个键 `"123"`, 我会检测到这是个数字字符串, 直接转成整数 123 存储. 这样不仅节省空间(不需要堆分配), 而且比较和哈希都更快. 这个优化在实际场景中很有用, 因为很多应用会用数字 ID 作为键. 复合类型(哈希、集合、列表、有序集合)用 `RobjWrapper` 存储. 这个结构包含一个 `void*` 指针指向实际的数据结构(比如哈希用的是 `DashTable`, 集合用的是 `std::unordered_set`), 加上类型标识、编码标识和大小信息. 这样 NanoObj 本身只是一个轻量级的包装器, 实际的数据存在堆上, 由具体的数据结构管理. 

生命周期管理是个需要小心的地方. NanoObj 支持拷贝和移动语义. 拷贝时, 内联字符串直接 `memcpy`, 整数直接复制值, `SmallString` 需要深度拷贝(分配新内存), Redis 对象只复制元数据(指针和大小), 不拷贝底层对象. 移动时就简单多了, 直接把数据拷过去, 然后把源对象重置为 NULL. 析构时, 如果是 `SmallString` 需要释放堆内存, 如果是 Redis 对象需要释放底层对象. 

## 并发架构

如何处理并发,  以及利用多核 CPU? Redis 中的原始设计,  使用了单线程,  就是在一个线程中操作 *io 解析*,  *内存数据操作*,  使用 epoll event 循环实现多用户操作,  最近的更新已经实现了 io 的多线程操作,  但是内存数据操作还是单线程; 而传统的内存加锁. 所有线程共享同一个哈希表,  用互斥锁保护. 这种方式实现简单,  但随着核心数增加,  锁竞争会成为瓶颈. 极端情况下,  即使你有 64 个核心,  实际利用率可能只有几个核心的水平. 表现有可能还不如redis的单线程架构. 

我选择 **Shared-Nothing 架构**. 这个名字听起来很学术,  但核心思想很朴素: 每个线程有自己独立的数据分片,  线程之间不共享任何可变状态. 既然没有共享,  自然就不需要锁. 

具体来说,  NanoRedis 启动时会创建 N 个 vCPU 线程（通常等于 CPU 核心数）,  每个线程独占一个 EngineShard. EngineShard 就是一个数据分片,  内部包含一个 DashTable 实例. 当请求到达时,  根据 key 的哈希值决定发往哪个分片. 如果目标分片就在当前线程,  直接处理；如果在其他线程,  通过消息队列转发. 每个 vCPU 线程都创建自己的 TCP 服务器,  并用 `SO_REUSEPORT` 选项绑定到同一个端口. 这是 Linux 3. 9 引入的特性,  允许多个 socket 监听同一个端口,  内核会自动做负载均衡. 这样就避免了单一 accept 线程成为瓶颈. Ngnix 等网络库都是使用这种方式.  线程之间的通信通过 TaskQueue 完成. 这是一个 MPSC（多生产者单消费者）队列: 多个线程可以往里投递任务,  但只有目标分片的线程会消费. 当一个请求需要访问其他分片的数据时,  当前协程把任务打包发到目标分片的队列,  然后挂起自己. 注意是协程挂起,  不是线程阻塞——这是关键. 目标线程处理完任务后,  通过 semaphore 通知发起方,  发起方的协程被唤醒继续执行. 这里我用了 Photon 这个协程库. 它提供了用户态的轻量级协程和基于 io_uring/epoll 的异步 IO. 一个线程可以跑成千上万个协程,  协程之间的切换开销比线程切换小得多. 所以即使某个请求需要等待其他分片,  只是当前协程让出 CPU,  其他协程可以继续执行,  线程不会闲着. 

Shared-Nothing 架构的优势很明显: 没有锁竞争,  缓存更友好（数据始终在同一个核心上处理）,  扩展性接近线性. 但也有代价: 跨分片操作有额外的延迟（大约 1-2 微秒）,  多 key 命令需要协调多个分片,  实现起来更复杂. 不过在实际场景中,  大部分请求都是单 key 操作,  可以走快速路径直接在本地处理. 跨分片操作的,  因为我们 io 任务分配是随机的,  如果我们启用了4个线程,  则只能保证 25% 的请求是本地线程处理的,  所以这里任务转发要做的非常高效. 

## DashTable + Shared-Nothing 

DashTable 和 Shared-Nothing 架构是互补的. DashTable 解决的是单个分片内的数据存储效率问题,  让扩容变得平滑无感；Shared-Nothing 架构解决的是多核利用率问题,  让每个 CPU 核心都能充分发挥性能. 从一个请求的视角来看: 请求到达某个 vCPU 的 TCP 服务器,  解析出 key 后计算目标分片. 如果是本地分片,  直接调用 DashTable 的 Find/Insert 方法；如果是远程分片,  通过 TaskQueue 转发,  最终还是会落到某个 DashTable 上.  两层设计各司其职, 组合在一起就是一个能高效利用多核、同时保持低延迟的内存数据库. 

## 简单测试
nano:
```
ubuntu@ubuntu ~/nano_redis (master)> redis-benchmark -p 9527 -t set, get, incr, lpush, lpop, sadd, spop, hset -q -n 100000 -c 100 -r 1000
WARNING: Could not fetch server CONFIG
SET: 213219. 61 requests per second,  p50=0. 231 msec
GET: 320512. 81 requests per second,  p50=0. 159 msec
INCR: 265957. 47 requests per second,  p50=0. 199 msec
LPUSH: 357142. 84 requests per second,  p50=0. 143 msec
LPOP: 276243. 09 requests per second,  p50=0. 183 msec
SADD: 279329. 59 requests per second,  p50=0. 183 msec
HSET: 200803. 22 requests per second,  p50=0. 255 msec
SPOP: 236966. 83 requests per second,  p50=0. 215 msec
```

valkey:
```
ubuntu@ubuntu ~/nano_redis (master)> redis-benchmark -p 9999 -t set, get, incr, lpush, lpop, sadd, spop, hset -q -n 100000 -c 100 -r 1000
SET: 300300. 28 requests per second,  p50=0. 167 msec
GET: 202429. 16 requests per second,  p50=0. 247 msec
INCR: 248138. 95 requests per second,  p50=0. 247 msec
LPUSH: 373134. 31 requests per second,  p50=0. 135 msec
LPOP: 367647. 03 requests per second,  p50=0. 135 msec
SADD: 355871. 91 requests per second,  p50=0. 135 msec
HSET: 375939. 84 requests per second,  p50=0. 135 msec
SPOP: 374531. 84 requests per second,  p50=0. 135 msec
```

!! 我们的这个简单的实现并没有比 redis/valkey 高出多少性能,  他们做了更多的优化,  当然我们只是使用单客户端简单测试系统的整体性能,  我们的代码机器,  特别是写入操作和复杂数据结构处理(这里我们大部分使用C++ std简单实现),  但是目前的系统基本跑通了

~待续~
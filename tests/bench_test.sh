# 编译
cd /home/ubuntu/nano_redis && mkdir -p build && cd build && cmake .. && make -j4

# 运行单测
./unit_tests

# 启动服务器（4 shards）
./nano_redis_server --num_shards=4 --port=9527 &
./valkey-server --port 9999 --io-threads 4 --io-threads-do-reads yes

# 基准测试
redis-benchmark -p 9527 -t set,get,incr,lpush,lpop,sadd,spop,hset -q -n 100000 -c 100 -r 1000
redis-benchmark -p 9999 -t set,get,incr,lpush,lpop,sadd,spop,hset -q -n 100000 -c 100 -r 1000
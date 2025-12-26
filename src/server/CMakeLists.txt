# 定义变量SRC_LIST，存储当前目录下的所有源文件
aux_source_directory(. SRC_LIST)
#database目录下的源文件
aux_source_directory(./database DB_LIST)
#model目录下的源文件
aux_source_directory(./model MODEL_LIST)
#redis目录下的源文件
aux_source_directory(./redis REDIS_LIST)

#生成可执行文件
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST})

#链接库
target_link_libraries(ChatServer muduo_net muduo_base /usr/lib64/mysql/libmysqlclient.so hiredis pthread)

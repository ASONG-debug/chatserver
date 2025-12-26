# 定义变量SRC_LIST，存储当前目录下的所有源文件
#aux_source_directory(. SRC_LIST)
#生成可执行文件
#add_executable(ChatClient ${SRC_LIST})
add_executable(ChatClient ./main1.cpp)
#指定可执行文件链接时需要依赖的库文件
target_link_libraries(ChatClient pthread)
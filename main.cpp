#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <chrono>
#include <thread>
#include <string>

// ==================== 公共定义 ====================
#define SERVER_PORT 8888          // 服务器端口
#define SERVER_IP "127.0.0.1"     // 服务器IP（本地回环）
#define BUFFER_SIZE 1024          // 数据缓冲区大小
#define MAX_CLIENTS 10            // 最大并发客户端数
#define TEST_DATA "Hello from Client!"  // 测试发送数据
#define TEST_SEND_COUNT 5         // 客户端单次连接发送次数

// 错误处理宏
#define ERROR_EXIT(msg) do { \
    std::cerr << "[Error] " << msg << " (errno: " << errno << ")" << std::endl; \
    exit(EXIT_FAILURE); \
} while (0)

// ==================== 服务器相关声明 ====================
void* handle_client(void* client_fd_ptr);  // 客户端处理线程
void run_server();                         // 服务器主逻辑

// ==================== 客户端相关声明 ====================
void run_client();                         // 客户端主逻辑

// ==================== 服务器实现 ====================
// 客户端处理线程函数
void* handle_client(void* client_fd_ptr) {
    int client_fd = *(int*)client_fd_ptr;
    free(client_fd_ptr);  // 释放传递的客户端FD内存

    char buffer[BUFFER_SIZE];
    ssize_t recv_len, send_len;

    std::cout << "[Server] Client connected (FD: " << client_fd << ")" << std::endl;

    // 循环接收客户端数据
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        recv_len = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

        if (recv_len <= 0) {
            if (recv_len == 0) {
                std::cout << "[Server] Client disconnected (FD: " << client_fd << ")" << std::endl;
            } else {
                std::cerr << "[Server] Recv failed from FD " << client_fd << " (errno: " << errno << ")" << std::endl;
            }
            break;
        }

        std::cout << "[Server] Received from FD " << client_fd << ": " << buffer << std::endl;

        // 回复客户端
        std::string reply = "Server received: " + std::string(buffer);
        send_len = send(client_fd, reply.c_str(), reply.length(), 0);
        if (send_len != reply.length()) {
            std::cerr << "[Server] Send failed to FD " << client_fd << " (errno: " << errno << ")" << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 模拟处理延迟
    }

    close(client_fd);
    pthread_exit(nullptr);
}

// 服务器主逻辑
void run_server() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t client_threads[MAX_CLIENTS];
    int thread_count = 0;

    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) ERROR_EXIT("Socket creation failed");
    std::cout << "[Server] Socket created (FD: " << server_fd << ")" << std::endl;

    // 端口复用
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        ERROR_EXIT("Setsockopt failed");
    }

    // 绑定IP和端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ERROR_EXIT("Bind failed");
    }
    std::cout << "[Server] Bound to " << SERVER_IP << ":" << SERVER_PORT << std::endl;

    // 监听
    if (listen(server_fd, MAX_CLIENTS) < 0) ERROR_EXIT("Listen failed");
    std::cout << "[Server] Listening for clients (max " << MAX_CLIENTS << ")" << std::endl;

    // 循环接受连接
    while (true) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) ERROR_EXIT("Accept failed");

        if (thread_count >= MAX_CLIENTS) {
            std::cerr << "[Server] Max clients reached, reject FD " << client_fd << std::endl;
            close(client_fd);
            continue;
        }

        // 创建线程处理客户端
        int* client_fd_ptr = (int*)malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        if (pthread_create(&client_threads[thread_count], nullptr, handle_client, client_fd_ptr) != 0) {
            std::cerr << "[Server] Thread creation failed (errno: " << errno << ")" << std::endl;
            free(client_fd_ptr);
            close(client_fd);
            continue;
        }

        pthread_detach(client_threads[thread_count]);
        thread_count++;
        std::cout << "[Server] Created thread for FD " << client_fd << " (thread count: " << thread_count << ")" << std::endl;
    }

    close(server_fd);  // 实际不会执行
}

// ==================== 客户端实现 ====================
void run_client() {
    int client_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t send_len, recv_len;

    // 创建socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) ERROR_EXIT("Socket creation failed");
    std::cout << "[Client] Socket created (FD: " << client_fd << ")" << std::endl;

    // 配置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    // 连接服务器
    if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ERROR_EXIT("Connect failed (server not running?)");
    }
    std::cout << "[Client] Connected to " << SERVER_IP << ":" << SERVER_PORT << std::endl;

    // 发送测试数据
    for (int i = 0; i < TEST_SEND_COUNT; i++) {
        send_len = send(client_fd, TEST_DATA, strlen(TEST_DATA), 0);
        if (send_len != strlen(TEST_DATA)) ERROR_EXIT("Send failed");
        std::cout << "[Client] Sent (" << i + 1 << "/" << TEST_SEND_COUNT << "): " << TEST_DATA << std::endl;

        // 接收回复
        memset(buffer, 0, BUFFER_SIZE);
        recv_len = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (recv_len <= 0) ERROR_EXIT("Recv failed (server disconnected?)");
        std::cout << "[Client] Received from server: " << buffer << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));  // 间隔1秒
    }

    close(client_fd);
    std::cout << "[Client] Disconnected" << std::endl;
}

// ==================== 主函数（入口） ====================
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <server|client>" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string mode = argv[1];
    if (mode == "server") {
        run_server();
    } else if (mode == "client") {
        run_client();
    } else {
        std::cerr << "Invalid mode! Use 'server' or 'client'" << std::endl;
        exit(EXIT_FAILURE);
    }

    return 0;
}
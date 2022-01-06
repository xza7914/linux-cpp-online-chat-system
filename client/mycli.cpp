#include "client.h"

int Client::pipefd[2] = {0, 0};
Client client;

int main() {
    client.run();
}
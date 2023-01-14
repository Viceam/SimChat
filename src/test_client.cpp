#include "client.h"

int main()
{
    Client clnt(8023,"127.0.0.1");
    clnt.run();
    return 0;
}

#include "server.h"

int main()
{
	Server server(8023, "127.0.0.1");
	server.run();
	return 0;
}

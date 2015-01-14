#include "config.h"

using namespace thinkfan;
using namespace std;

int main(int argc, char **argv) {
	Config *config = Config::parse_config(argv[1]);
}

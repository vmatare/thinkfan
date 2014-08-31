#include "Alternation.h"
#include "Concatenation.h"
#include "Literal.h"

int main(int argc, char **argv) {
	Concatenation grammar((Literal("a") + Literal("bc")));
}

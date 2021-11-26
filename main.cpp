#include <stdio.h>
#include <string.h>

#include "include/pmwcas.h"

int main()
{
    pmwcas::InitLibrary(pmwcas::TlsAllocator::Create, pmwcas::TlsAllocator::Destroy);
    printf("%s\n", "test success!");
	return 0;
}

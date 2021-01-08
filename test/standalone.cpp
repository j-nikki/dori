//
// Tests for syntax errors, missing headers, etc.
//

#include <dori/all.h>

int main()
{
    dori::vector<int, double> v;
    dori::vector_cast<float, int64_t>(v);
}
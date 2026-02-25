int main() {
    int x;
    int total;
    x = 1;
    total = 0;
    {
        int y;
        y = 10;
        total = total + y;
    }
    {
        int y;
        y = 20;
        total = total + y;
    }
    total = total + x;
    return total;
}
